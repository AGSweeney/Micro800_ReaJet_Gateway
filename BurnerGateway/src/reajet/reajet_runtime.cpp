#ifdef BURNERGATEWAY_MAIN_TU
static int ParseTagIndex(const char *url)
{
    if (!url)
    {
        return -1;
    }
    while (*url == '/')
    {
        ++url;
    }
    const char *prefix = "api/plc/tags/remove/";
    const size_t prefixLen = strlen(prefix);
    if (strncmp(url, prefix, prefixLen) != 0)
    {
        return -1;
    }
    const char *p = url + prefixLen;
    if (!*p)
    {
        return -1;
    }
    int idx = 0;
    while (*p >= '0' && *p <= '9')
    {
        idx = (idx * 10) + (*p - '0');
        ++p;
    }
    return idx;
}

static const char *CipTypeNameFromCode(uint16_t typeCode)
{
    switch (typeCode)
    {
    case 0xC1: return "BOOL";
    case 0xC2: return "SINT";
    case 0xC3: return "INT";
    case 0xC4: return "DINT";
    case 0xC5: return "LINT";
    case 0xC6: return "USINT";
    case 0xC7: return "UINT";
    case 0xC8: return "UDINT";
    case 0xC9: return "ULINT";
    case 0xCA: return "REAL";
    case 0xCB: return "LREAL";
    case 0xCC: return "TIME";
    case 0xCD: return "DATE";
    case 0xCE: return "TIME_OF_DAY";
    case 0xCF: return "DATE_AND_TIME";
    case 0xD1: return "BYTE";
    case 0xD2: return "WORD";
    case 0xD3: return "DWORD";
    case 0xD4: return "LWORD";
    case 0xDA: return "STRING";
    default:   return "";
    }
}

static bool ParseIpv4Text(const char *text, IPADDR4 &ipOut)
{
    if (!text || !text[0])
    {
        return false;
    }
    unsigned a = 0, b = 0, c = 0, d = 0;
    if (sscanf(text, "%u.%u.%u.%u", &a, &b, &c, &d) != 4)
    {
        return false;
    }
    if (a > 255 || b > 255 || c > 255 || d > 255)
    {
        return false;
    }
    const uint32_t raw = (static_cast<uint32_t>(a) << 24) |
                         (static_cast<uint32_t>(b) << 16) |
                         (static_cast<uint32_t>(c) << 8) |
                         static_cast<uint32_t>(d);
    ipOut = IPV4FromConst(raw);
    return !ipOut.IsNull();
}

static bool IsLoopbackIpText(const char *text)
{
    if (!text || !text[0])
    {
        return false;
    }
    return strncmp(text, "127.", 4) == 0;
}

static void HexEncode(const uint8_t *data, size_t len, char *out, size_t outSize)
{
    if (!out || outSize < 2)
    {
        return;
    }
    out[0] = '\0';
    if (!data || len == 0)
    {
        return;
    }
    static const char kHex[] = "0123456789ABCDEF";
    size_t w = 0;
    for (size_t i = 0; i < len && (w + 2) < outSize; ++i)
    {
        const uint8_t b = data[i];
        out[w++] = kHex[(b >> 4) & 0x0F];
        out[w++] = kHex[b & 0x0F];
    }
    out[w] = '\0';
}

static bool AppendCipSymbolSegment(const char *seg, uint8_t *path, size_t pathCap, size_t &pathLen)
{
    if (!seg || !seg[0])
    {
        return false;
    }
    const size_t segLen = strlen(seg);
    if (segLen > 255)
    {
        return false;
    }
    const size_t need = 2 + segLen + ((segLen & 1) ? 1 : 0);
    if ((pathLen + need) > pathCap)
    {
        return false;
    }
    path[pathLen++] = 0x91;
    path[pathLen++] = static_cast<uint8_t>(segLen);
    memcpy(path + pathLen, seg, segLen);
    pathLen += segLen;
    if (segLen & 1)
    {
        path[pathLen++] = 0x00;
    }
    return true;
}

static bool AppendCipArrayIndex(uint32_t idx, uint8_t *path, size_t pathCap, size_t &pathLen)
{
    if (idx <= 0xFFu)
    {
        if ((pathLen + 2) > pathCap)
        {
            return false;
        }
        path[pathLen++] = 0x28;
        path[pathLen++] = static_cast<uint8_t>(idx & 0xFFu);
        return true;
    }
    if (idx <= 0xFFFFu)
    {
        if ((pathLen + 4) > pathCap)
        {
            return false;
        }
        path[pathLen++] = 0x29;
        path[pathLen++] = 0x00;
        path[pathLen++] = static_cast<uint8_t>(idx & 0xFFu);
        path[pathLen++] = static_cast<uint8_t>((idx >> 8) & 0xFFu);
        return true;
    }
    return false;
}

static bool EncodeCipTagPath(const char *tagPath, uint8_t *path, size_t pathCap, size_t &pathLenOut)
{
    if (!tagPath || !tagPath[0] || !path || pathCap < 4)
    {
        return false;
    }
    pathLenOut = 0;
    const char *p = tagPath;
    while (*p)
    {
        char seg[80]{0};
        size_t s = 0;
        while (*p && *p != '.')
        {
            if (s + 1 >= sizeof(seg))
            {
                return false;
            }
            seg[s++] = *p++;
        }
        if (*p == '.')
        {
            ++p;
        }
        seg[s] = '\0';
        if (!seg[0])
        {
            return false;
        }

        const char *br = strchr(seg, '[');
        char base[80]{0};
        if (!br)
        {
            CopyString(base, sizeof(base), seg);
            if (!AppendCipSymbolSegment(base, path, pathCap, pathLenOut))
            {
                return false;
            }
            continue;
        }

        const size_t baseLen = static_cast<size_t>(br - seg);
        if (baseLen == 0 || baseLen >= sizeof(base))
        {
            return false;
        }
        memcpy(base, seg, baseLen);
        base[baseLen] = '\0';
        if (!AppendCipSymbolSegment(base, path, pathCap, pathLenOut))
        {
            return false;
        }

        const char *q = br;
        while (*q)
        {
            if (*q != '[')
            {
                return false;
            }
            ++q;
            if (!isdigit(static_cast<unsigned char>(*q)))
            {
                return false;
            }
            uint32_t idx = 0;
            while (*q && isdigit(static_cast<unsigned char>(*q)))
            {
                idx = (idx * 10u) + static_cast<uint32_t>(*q - '0');
                ++q;
            }
            if (*q != ']')
            {
                return false;
            }
            ++q;
            if (!AppendCipArrayIndex(idx, path, pathCap, pathLenOut))
            {
                return false;
            }
        }
    }
    return (pathLenOut > 0 && (pathLenOut % 2) == 0);
}

static bool ReadPlcTagRaw(const char *tagPath,
                          uint16_t &typeCodeOut,
                          uint8_t *dataOut,
                          size_t dataCap,
                          size_t &dataLenOut,
                          char *errorOut,
                          size_t errorOutSize)
{
    auto setErr = [&](const char *msg) {
        if (!errorOut || errorOutSize < 2)
        {
            return;
        }
        if (!msg)
        {
            errorOut[0] = '\0';
            return;
        }
        const size_t len = strlen(msg);
        const size_t copyLen = (len < (errorOutSize - 1)) ? len : (errorOutSize - 1);
        memcpy(errorOut, msg, copyLen);
        errorOut[copyLen] = '\0';
    };

    setErr("");
    dataLenOut = 0;
    typeCodeOut = 0;
    if (!IsNetworkLinkReady())
    {
        setErr("Network not ready");
        return false;
    }
    const IPADDR4 targetIp = static_cast<IPADDR4>(gPlcTargetIp);
    if (targetIp.IsNull())
    {
        setErr("No PLC selected");
        return false;
    }

    uint8_t tagPathCip[256]{0};
    size_t tagPathLen = 0;
    if (!EncodeCipTagPath(tagPath, tagPathCip, sizeof(tagPathCip), tagPathLen))
    {
        setErr("Invalid tag path");
        return false;
    }

    const int fd = connect(targetIp, 44818, TICKS_PER_SECOND * 3);
    if (fd < 0)
    {
        setErr("TCP connect to PLC failed");
        NotifyPlcCommunicationFailure("TCP connect to 44818 failed");
        return false;
    }

    uint32_t session = 0;
    if (!RegisterEnipSessionFd(fd, session))
    {
        close(fd);
        setErr("ENIP RegisterSession failed");
        NotifyPlcCommunicationFailure("ENIP RegisterSession failed");
        return false;
    }

    uint8_t cip[512]{0};
    size_t c = 0;
    cip[c++] = 0x4C; // CIP Read Tag service
    cip[c++] = static_cast<uint8_t>(tagPathLen / 2);
    memcpy(cip + c, tagPathCip, tagPathLen);
    c += tagPathLen;
    WriteLe16(cip + c, 1); // element count
    c += 2;

    uint8_t rrPayload[1024]{0};
    size_t rrLen = 0;
    if (!SendEnipRrData(fd, session, cip, c, rrPayload, sizeof(rrPayload), rrLen))
    {
        UnregisterEnipSessionFd(fd, session);
        close(fd);
        setErr("ENIP SendRRData failed");
        NotifyPlcCommunicationFailure("ENIP SendRRData failed");
        return false;
    }

    const uint8_t *cipResp = nullptr;
    size_t cipRespLen = 0;
    if (!ExtractCipFromRrPayload(rrPayload, rrLen, cipResp, cipRespLen) || cipRespLen < 4)
    {
        UnregisterEnipSessionFd(fd, session);
        close(fd);
        setErr("Invalid CIP payload");
        NotifyPlcCommunicationFailure("Invalid ENIP payload");
        return false;
    }

    const uint8_t service = cipResp[0];
    const uint8_t status = cipResp[2];
    const uint8_t addStatusWords = cipResp[3];
    const size_t payloadOffset = 4 + (static_cast<size_t>(addStatusWords) * 2);
    if (service != 0xCC || payloadOffset > cipRespLen)
    {
        UnregisterEnipSessionFd(fd, session);
        close(fd);
        setErr("Unexpected read-tag response");
        NotifyPlcCommunicationFailure("Unexpected ENIP read response");
        return false;
    }
    if (status != 0)
    {
        UnregisterEnipSessionFd(fd, session);
        close(fd);
        setErr("PLC read-tag returned error");
        return false;
    }

    const uint8_t *payload = cipResp + payloadOffset;
    const size_t payloadLen = cipRespLen - payloadOffset;
    if (payloadLen < 2)
    {
        UnregisterEnipSessionFd(fd, session);
        close(fd);
        setErr("Read-tag payload too short");
        return false;
    }

    typeCodeOut = ReadLe16(payload);
    const size_t rawLen = payloadLen - 2;
    dataLenOut = (rawLen < dataCap) ? rawLen : dataCap;
    if (dataLenOut > 0)
    {
        memcpy(dataOut, payload + 2, dataLenOut);
    }

    UnregisterEnipSessionFd(fd, session);
    close(fd);
    setErr("");
    NotifyPlcCommunicationSuccess();
    return true;
}

static bool WritePlcTagRaw(const char *tagPath,
                           uint16_t typeCode,
                           const uint8_t *data,
                           size_t dataLen,
                           char *errorOut,
                           size_t errorOutSize)
{
    auto setErr = [&](const char *msg) {
        if (!errorOut || errorOutSize < 2)
        {
            return;
        }
        if (!msg)
        {
            errorOut[0] = '\0';
            return;
        }
        const size_t len = strlen(msg);
        const size_t copyLen = (len < (errorOutSize - 1)) ? len : (errorOutSize - 1);
        memcpy(errorOut, msg, copyLen);
        errorOut[copyLen] = '\0';
    };

    setErr("");
    if (!tagPath || !tagPath[0] || !data || dataLen == 0)
    {
        setErr("Invalid write parameters");
        return false;
    }
    if (!IsNetworkLinkReady())
    {
        setErr("Network not ready");
        return false;
    }

    const IPADDR4 targetIp = static_cast<IPADDR4>(gPlcTargetIp);
    if (targetIp.IsNull())
    {
        setErr("No PLC selected");
        return false;
    }

    uint8_t tagPathCip[256]{0};
    size_t tagPathLen = 0;
    if (!EncodeCipTagPath(tagPath, tagPathCip, sizeof(tagPathCip), tagPathLen))
    {
        setErr("Invalid tag path");
        return false;
    }

    const int fd = connect(targetIp, 44818, TICKS_PER_SECOND * 3);
    if (fd < 0)
    {
        setErr("TCP connect to PLC failed");
        NotifyPlcCommunicationFailure("TCP connect to 44818 failed");
        return false;
    }

    uint32_t session = 0;
    if (!RegisterEnipSessionFd(fd, session))
    {
        close(fd);
        setErr("ENIP RegisterSession failed");
        NotifyPlcCommunicationFailure("ENIP RegisterSession failed");
        return false;
    }

    uint8_t cip[512]{0};
    size_t c = 0;
    if (c + 1 + 1 + tagPathLen + 2 + 2 + dataLen > sizeof(cip))
    {
        UnregisterEnipSessionFd(fd, session);
        close(fd);
        setErr("Write payload too large");
        return false;
    }
    cip[c++] = 0x4D; // CIP Write Tag service
    cip[c++] = static_cast<uint8_t>(tagPathLen / 2);
    memcpy(cip + c, tagPathCip, tagPathLen);
    c += tagPathLen;
    WriteLe16(cip + c, typeCode);
    c += 2;
    WriteLe16(cip + c, 1); // element count
    c += 2;
    memcpy(cip + c, data, dataLen);
    c += dataLen;

    uint8_t rrPayload[512]{0};
    size_t rrLen = 0;
    if (!SendEnipRrData(fd, session, cip, c, rrPayload, sizeof(rrPayload), rrLen))
    {
        UnregisterEnipSessionFd(fd, session);
        close(fd);
        setErr("ENIP SendRRData failed");
        NotifyPlcCommunicationFailure("ENIP SendRRData failed");
        return false;
    }

    const uint8_t *cipResp = nullptr;
    size_t cipRespLen = 0;
    if (!ExtractCipFromRrPayload(rrPayload, rrLen, cipResp, cipRespLen) || cipRespLen < 4)
    {
        UnregisterEnipSessionFd(fd, session);
        close(fd);
        setErr("Invalid CIP payload");
        NotifyPlcCommunicationFailure("Invalid ENIP payload");
        return false;
    }

    const uint8_t service = cipResp[0];
    const uint8_t status = cipResp[2];
    if (service != 0xCD || status != 0)
    {
        UnregisterEnipSessionFd(fd, session);
        close(fd);
        setErr("PLC write-tag returned error");
        return false;
    }

    UnregisterEnipSessionFd(fd, session);
    close(fd);
    setErr("");
    NotifyPlcCommunicationSuccess();
    return true;
}

static bool ReaResponseCodeIsAck(const char *errorCode)
{
    if (!errorCode || errorCode[0] == '\0')
    {
        return false;
    }
    if (strcmp(errorCode, "00000000") == 0)
    {
        return true;
    }
    // REA JET DOD 2.0 returns 00000066 on successful REA-PLC commands.
    // The manual's port 22169 example and printer testing both show this.
    if (strcmp(errorCode, "00000066") == 0)
    {
        return true;
    }
    return false;
}

static void ParseReaResponseErrorInfo(const char *raw,
                                      bool *ackOut,
                                      int32_t *errorCodeOut,
                                      char *errorCodeAsciiOut,
                                      size_t errorCodeAsciiOutSize,
                                      char *errorTextOut,
                                      size_t errorTextOutSize)
{
    if (ackOut)
    {
        *ackOut = false;
    }
    if (errorCodeOut)
    {
        *errorCodeOut = -2;
    }
    if (errorCodeAsciiOut && errorCodeAsciiOutSize > 0)
    {
        errorCodeAsciiOut[0] = '\0';
    }
    if (errorTextOut && errorTextOutSize > 0)
    {
        strncpy(errorTextOut, "No response", errorTextOutSize - 1);
        errorTextOut[errorTextOutSize - 1] = '\0';
    }
    if (!raw || !raw[0])
    {
        return;
    }
    if (strcmp(raw, "<send_failed>") == 0)
    {
        if (errorCodeOut)
        {
            *errorCodeOut = -1;
        }
        if (errorTextOut && errorTextOutSize > 0)
        {
            strncpy(errorTextOut, "TCP send/receive failed", errorTextOutSize - 1);
            errorTextOut[errorTextOutSize - 1] = '\0';
        }
        return;
    }
    if (strlen(raw) < 20)
    {
        if (errorCodeOut)
        {
            *errorCodeOut = -2;
        }
        if (errorTextOut && errorTextOutSize > 0)
        {
            strncpy(errorTextOut, "Invalid response length", errorTextOutSize - 1);
            errorTextOut[errorTextOutSize - 1] = '\0';
        }
        return;
    }

    char errorCode[9]{0};
    CopyAsciiField(raw, strlen(raw), 12, 8, errorCode, sizeof(errorCode));
    if (errorCodeAsciiOut && errorCodeAsciiOutSize > 0)
    {
        strncpy(errorCodeAsciiOut, errorCode, errorCodeAsciiOutSize - 1);
        errorCodeAsciiOut[errorCodeAsciiOutSize - 1] = '\0';
    }
    const unsigned long codeVal = strtoul(errorCode, nullptr, 16);
    if (errorCodeOut)
    {
        *errorCodeOut = static_cast<int32_t>(codeVal);
    }
    if (ackOut)
    {
        *ackOut = ReaResponseCodeIsAck(errorCode);
    }
    if (errorTextOut && errorTextOutSize > 0)
    {
        strncpy(errorTextOut, ReaErrorCodeText(errorCode), errorTextOutSize - 1);
        errorTextOut[errorTextOutSize - 1] = '\0';
    }
}

static bool WriteMappingResponseToPlc(const MappingRecord &m, bool ackOk, int32_t errorCode)
{
    bool ok = true;
    if (m.responseTag[0])
    {
        uint8_t ackBool = ackOk ? 1u : 0u;
        char writeErr[96]{0};
        if (!WritePlcTagRaw(m.responseTag, 0xC1, &ackBool, 1, writeErr, sizeof(writeErr)))
        {
            iprintf("Mapping ACK write failed id=%d tag=%s err=%s\r\n", m.id, m.responseTag, writeErr);
            ok = false;
        }
    }
    if (m.errorTag[0])
    {
        uint8_t dintBuf[4]{0};
        WriteLe32(dintBuf, static_cast<uint32_t>(errorCode));
        char writeErr[96]{0};
        if (!WritePlcTagRaw(m.errorTag, 0xC4, dintBuf, sizeof(dintBuf), writeErr, sizeof(writeErr)))
        {
            iprintf("Mapping error write failed id=%d tag=%s err=%s\r\n", m.id, m.errorTag, writeErr);
            ok = false;
        }
    }
    return ok;
}

static bool ResetMappingResponseToPlc(const MappingRecord &m)
{
    if (!m.responseTag[0] && !m.errorTag[0])
    {
        return true;
    }
    return WriteMappingResponseToPlc(m, false, 0);
}

static void FormatTagValueText(uint16_t typeCode, const uint8_t *data, size_t len, char *out, size_t outSize)
{
    if (!out || outSize < 2)
    {
        return;
    }
    out[0] = '\0';
    if (!data || len == 0)
    {
        return;
    }

    switch (typeCode)
    {
    case 0xC1:
        sniprintf(out, outSize, "%u", data[0] ? 1u : 0u);
        return;
    case 0xC2:
        sniprintf(out, outSize, "%d", static_cast<int>(static_cast<int8_t>(data[0])));
        return;
    case 0xC6:
        sniprintf(out, outSize, "%u", static_cast<unsigned>(data[0]));
        return;
    case 0xC3:
    case 0xC7:
        if (len >= 2)
        {
            const uint16_t v = ReadLe16(data);
            if (typeCode == 0xC3)
                sniprintf(out, outSize, "%d", static_cast<int>(static_cast<int16_t>(v)));
            else
                sniprintf(out, outSize, "%u", static_cast<unsigned>(v));
            return;
        }
        break;
    case 0xC4:
    case 0xC8:
        if (len >= 4)
        {
            const uint32_t v = ReadLe32(data);
            if (typeCode == 0xC4)
                sniprintf(out, outSize, "%ld", static_cast<long>(static_cast<int32_t>(v)));
            else
                sniprintf(out, outSize, "%lu", static_cast<unsigned long>(v));
            return;
        }
        break;
    case 0xCA:
        if (len >= 4)
        {
            float f = 0.0f;
            memcpy(&f, data, 4);
            sniprintf(out, outSize, "%.6f", static_cast<double>(f));
            return;
        }
        break;
    case 0xDA:
        {
            const uint8_t slen = data[0];
            size_t n = static_cast<size_t>(slen);
            if ((1 + n) > len)
            {
                n = (len > 1) ? (len - 1) : 0;
            }
            if (n >= outSize)
            {
                n = outSize - 1;
            }
            memcpy(out, data + 1, n);
            out[n] = '\0';
            return;
        }
    default:
        break;
    }

    HexEncode(data, len, out, outSize);
}

static void ToHexAsciiUnsigned(uint32_t value, unsigned width, char *out, size_t outSize)
{
    if (!out || outSize < 2 || width < 1)
    {
        return;
    }
    sniprintf(out, outSize, "%0*lX", static_cast<int>(width), static_cast<unsigned long>(value));
}

static bool CopyAsciiField(const char *src, size_t srcLen, size_t offset, size_t width, char *out, size_t outSize)
{
    if (!src || !out || outSize < 2 || width == 0)
    {
        return false;
    }
    if (offset + width > srcLen)
    {
        return false;
    }
    if (width >= outSize)
    {
        width = outSize - 1;
    }
    memcpy(out, src + offset, width);
    out[width] = '\0';
    return true;
}

static bool IsAllZeroField(const char *text, size_t len)
{
    if (!text)
    {
        return true;
    }
    for (size_t i = 0; i < len; ++i)
    {
        if (text[i] != '0')
        {
            return false;
        }
    }
    return true;
}

static const char *ReaInstructionName(const char *instruction)
{
    if (!instruction)
    {
        return "Unknown";
    }
    if (strcmp(instruction, "0001") == 0) return "Assign Job";
    if (strcmp(instruction, "0002") == 0) return "Start Job";
    if (strcmp(instruction, "0003") == 0) return "Stop Job";
    if (strcmp(instruction, "0004") == 0) return "Set Label Contents";
    if (strcmp(instruction, "0005") == 0) return "Set Label Object";
    if (strcmp(instruction, "0006") == 0) return "Change Character Set";
    if (strcmp(instruction, "0007") == 0) return "Set EOT";
    if (strcmp(instruction, "0008") == 0) return "Get Status";
    if (strcmp(instruction, "C0DE") == 0) return "Change Character Set";
    if (strcmp(instruction, "FFFF") == 0) return "Unknown Instruction";
    return "Unknown";
}

static const char *ReaErrorCodeText(const char *errorCode)
{
    if (!errorCode || errorCode[0] == '\0')
    {
        return "No error code";
    }
    if (strcmp(errorCode, "00000000") == 0) return "OK";
    if (strcmp(errorCode, "00000066") == 0) return "OK (DOD2)";
    if (strcmp(errorCode, "00000002") == 0) return "Failed (unknown reason)";
    if (strcmp(errorCode, "00000003") == 0) return "Unknown error";
    if (strcmp(errorCode, "00000004") == 0) return "Invalid parameters";
    if (strcmp(errorCode, "00000005") == 0) return "Fatal error (device restart)";
    if (strcmp(errorCode, "00000006") == 0) return "Memory exception";
    if (strcmp(errorCode, "0000000A") == 0) return "Job is still running";
    if (strcmp(errorCode, "0000000B") == 0) return "No job is running";
    if (strcmp(errorCode, "0000000C") == 0) return "No job assigned";
    if (strcmp(errorCode, "0000000D") == 0) return "Job has no group";
    if (strcmp(errorCode, "0000000E") == 0) return "Job does not exist";
    if (strcmp(errorCode, "00000012") == 0) return "Print head not ready";
    if (strcmp(errorCode, "00000028") == 0) return "Label object not found";
    if (strcmp(errorCode, "00000029") == 0) return "Object content not found";
    if (strcmp(errorCode, "0000002A") == 0) return "Invalid object content";
    if (strcmp(errorCode, "00000060") == 0) return "Object unchanged";
    if (strcmp(errorCode, "00000061") == 0) return "Object properties changed";
    if (strcmp(errorCode, "00000062") == 0) return "Content unchanged";
    if (strcmp(errorCode, "00000063") == 0) return "Generic error";
    if (strlen(errorCode) == 8)
    {
        char codePart[5]{0};
        char domainPart[5]{0};
        memcpy(codePart, errorCode, 4);
        memcpy(domainPart, errorCode + 4, 4);
        const unsigned long code = strtoul(codePart, nullptr, 16);
        const unsigned long domain = strtoul(domainPart, nullptr, 16);
        if (domain == 0x65u)
        {
            if (code == 0x0Au) return "Invalid label element";
            if (code == 0x14u) return "Label resource missing";
            if (code == 0x1Eu) return "Label font missing";
            if (code == 0x28u) return "Label object not found";
            if (code == 0x29u) return "Object content not found";
            if (code == 0x2Au) return "Invalid object content";
            if (code == 0x5Au) return "Permission denied / cannot access label element";
            if (code == 0x60u) return "Object unchanged";
            if (code == 0x61u) return "Object properties changed";
            if (code == 0x62u) return "Content unchanged";
            if (code == 0x63u) return "Object content would stop printing";
            return "Label domain error";
        }
    }
    return "Error";
}

static const char *ReaJobStatusText(const char *jobStatus)
{
    if (!jobStatus || strlen(jobStatus) < 4)
    {
        return "Unknown";
    }
    char prefix[5]{0};
    memcpy(prefix, jobStatus, 4);
    if (strcmp(prefix, "0000") == 0) return "No job assigned";
    if (strcmp(prefix, "0001") == 0) return "Job assigned, not released";
    if (strcmp(prefix, "0002") == 0) return "Print release without job (error)";
    if (strcmp(prefix, "0003") == 0) return "Job assigned and released";
    return "Unknown job state";
}

static void AppendCartridgeStatusText(const char *status4, char *out, size_t outSize)
{
    if (!status4 || !out || outSize < 2)
    {
        return;
    }
    const unsigned long bits = strtoul(status4, nullptr, 16);
    if (bits == 0)
    {
        strncat(out, "inserted/ink OK", outSize - strlen(out) - 1);
        return;
    }
    bool first = true;
    if (bits & 0x1)
    {
        strncat(out, first ? "inserted" : ", inserted", outSize - strlen(out) - 1);
        first = false;
    }
    if (bits & 0x2)
    {
        strncat(out, first ? "empty" : ", empty", outSize - strlen(out) - 1);
        first = false;
    }
    if (bits & 0x4)
    {
        strncat(out, first ? "temp high" : ", temp high", outSize - strlen(out) - 1);
        first = false;
    }
    if (bits & 0x8)
    {
        strncat(out, first ? "ink low" : ", ink low", outSize - strlen(out) - 1);
        first = false;
    }
    if (bits & 0x10)
    {
        strncat(out, first ? "temp above target" : ", temp above target", outSize - strlen(out) - 1);
    }
    if (out[0] == '\0')
    {
        strncpy(out, "status flags set", outSize - 1);
        out[outSize - 1] = '\0';
    }
}

static void DecodeReaResponseHuman(const char *raw, char *decodedOut, size_t decodedOutSize, bool *ackOut)
{
    if (ackOut)
    {
        *ackOut = false;
    }
    if (!decodedOut || decodedOutSize < 2)
    {
        return;
    }
    decodedOut[0] = '\0';
    if (!raw || raw[0] == '\0')
    {
        strncpy(decodedOut, "No response", decodedOutSize - 1);
        decodedOut[decodedOutSize - 1] = '\0';
        return;
    }
    if (strcmp(raw, "<send_failed>") == 0)
    {
        strncpy(decodedOut, "TCP send/receive failed", decodedOutSize - 1);
        decodedOut[decodedOutSize - 1] = '\0';
        return;
    }

    const size_t rawLen = strlen(raw);
    if (rawLen < 64)
    {
        sniprintf(decodedOut, decodedOutSize, "Invalid response length (%u, expected 64)", static_cast<unsigned>(rawLen));
        return;
    }

    char instruction[5]{0};
    char reqId[9]{0};
    char errorCode[9]{0};
    char deviceStatus[5]{0};
    char jobStatus[9]{0};
    char statusField[33]{0};
    CopyAsciiField(raw, rawLen, 0, 4, instruction, sizeof(instruction));
    CopyAsciiField(raw, rawLen, 4, 8, reqId, sizeof(reqId));
    CopyAsciiField(raw, rawLen, 12, 8, errorCode, sizeof(errorCode));
    CopyAsciiField(raw, rawLen, 20, 4, deviceStatus, sizeof(deviceStatus));
    CopyAsciiField(raw, rawLen, 24, 8, jobStatus, sizeof(jobStatus));
    CopyAsciiField(raw, rawLen, 32, 32, statusField, sizeof(statusField));

    const bool ack = ReaResponseCodeIsAck(errorCode);
    if (ackOut)
    {
        *ackOut = ack;
    }

    char statusSummary[160]{0};
    if (IsAllZeroField(statusField, 32))
    {
        strncpy(statusSummary, "Ready (no warnings)", sizeof(statusSummary) - 1);
    }
    else
    {
        char cart1[5]{0};
        char ink1[5]{0};
        memcpy(cart1, statusField, 4);
        memcpy(ink1, statusField + 4, 4);
        char cartText[96]{0};
        AppendCartridgeStatusText(cart1, cartText, sizeof(cartText));
        sniprintf(statusSummary, sizeof(statusSummary), "Cart1=%s ink1=%s ml", cartText, ink1);
    }

    const char *errText = ReaErrorCodeText(errorCode);
    sniprintf(decodedOut,
              decodedOutSize,
              "%s | %s (%s) | ID %s | Error: %s (%s) | Device: %s | Job: %s | Status: %s",
              ack ? "ACK OK" : "ACK FAIL",
              ReaInstructionName(instruction),
              instruction,
              reqId,
              errText,
              errorCode,
              (strcmp(deviceStatus, "0000") == 0) ? "OK" : deviceStatus,
              ReaJobStatusText(jobStatus),
              statusSummary);
}

static uint32_t ParseReaHexAsciiField32(const char *hexField)
{
    if (!hexField || hexField[0] == '\0')
    {
        return 0;
    }
    return static_cast<uint32_t>(strtoul(hexField, nullptr, 16));
}

static bool ParseReaPlcStatusResponse(const char *raw, ReaPlcLiveStatus &status)
{
    if (!raw || raw[0] == '\0')
    {
        status.pollOk = false;
        status.pollErrors++;
        return false;
    }

    CopyString(status.rawResponse, sizeof(status.rawResponse), raw);
    const size_t rawLen = strlen(raw);
    if (rawLen < 64)
    {
        status.pollOk = false;
        status.pollErrors++;
        return false;
    }

    CopyAsciiField(raw, rawLen, 20, 4, status.deviceStatus, sizeof(status.deviceStatus));
    CopyAsciiField(raw, rawLen, 24, 8, status.jobStatus, sizeof(status.jobStatus));
    CopyAsciiField(raw, rawLen, 32, 32, status.statusField, sizeof(status.statusField));

    char jobPrefix[5]{0};
    CopyAsciiField(status.jobStatus, 8, 0, 4, jobPrefix, sizeof(jobPrefix));
    if (strcmp(jobPrefix, "0000") == 0)
    {
        status.jobAssigned = false;
        status.jobReleased = false;
    }
    else if (strcmp(jobPrefix, "0001") == 0)
    {
        status.jobAssigned = true;
        status.jobReleased = false;
    }
    else if (strcmp(jobPrefix, "0003") == 0)
    {
        status.jobAssigned = true;
        status.jobReleased = true;
    }
    else
    {
        status.jobAssigned = (strcmp(jobPrefix, "0002") != 0);
        status.jobReleased = (strcmp(jobPrefix, "0002") == 0 || strcmp(jobPrefix, "0003") == 0);
    }
    status.printerActive = status.jobReleased;
    status.jobStateRaw = ParseReaHexAsciiField32(status.jobStatus);

    for (int i = 0; i < 8; ++i)
    {
        char word[5]{0};
        CopyAsciiField(status.statusField, 32, static_cast<size_t>(i * 4), 4, word, sizeof(word));
        status.statusWords[i] = static_cast<uint16_t>(ParseReaHexAsciiField32(word) & 0xFFFFu);
    }
    for (int i = 0; i < 4; ++i)
    {
        const uint16_t w = status.statusWords[i * 2];
        status.triggerLevel[i] = (w == 1u);
    }
    status.triggerLevelValid = true;

    status.pollOk = true;
    status.pollCount++;
    status.lastPollSec = static_cast<uint32_t>(Secs);
    return true;
}

static bool StrIStartsWith(const char *text, const char *prefix)
{
    if (!text || !prefix)
    {
        return false;
    }
    while (*prefix)
    {
        char a = *text++;
        char b = *prefix++;
        if (a >= 'A' && a <= 'Z')
        {
            a = static_cast<char>(a - 'A' + 'a');
        }
        if (b >= 'A' && b <= 'Z')
        {
            b = static_cast<char>(b - 'A' + 'a');
        }
        if (a != b)
        {
            return false;
        }
    }
    return true;
}

static const char *FindXmlTagContent(const char *xml, const char *tag, char *out, size_t outSize)
{
    if (out && outSize > 0)
    {
        out[0] = '\0';
    }
    if (!xml || !tag || !out || outSize == 0)
    {
        return nullptr;
    }
    char openTag[64]{0};
    sniprintf(openTag, sizeof(openTag), "<%s", tag);
    const char *start = strstr(xml, openTag);
    if (!start)
    {
        return nullptr;
    }
    const char *gt = strchr(start, '>');
    if (!gt)
    {
        return nullptr;
    }
    const char *valStart = gt + 1;
    char closeTag[32]{0};
    sniprintf(closeTag, sizeof(closeTag), "</%s>", tag);
    const char *valEnd = strstr(valStart, closeTag);
    if (!valEnd)
    {
        return nullptr;
    }
    size_t len = static_cast<size_t>(valEnd - valStart);
    if (len >= outSize)
    {
        len = outSize - 1;
    }
    memcpy(out, valStart, len);
    out[len] = '\0';
    return out;
}

static bool ParseXmlFloatTag(const char *xml, const char *tag, float &valueOut)
{
    char text[64]{0};
    if (!FindXmlTagContent(xml, tag, text, sizeof(text)))
    {
        return false;
    }
    char *endPtr = nullptr;
    const double v = strtod(text, &endPtr);
    if (endPtr == text)
    {
        return false;
    }
    valueOut = static_cast<float>(v);
    return true;
}

static bool CopyXmlElementWithId(const char *xml, const char *tag, int id, char *out, size_t outSize)
{
    if (!xml || !tag || !out || outSize == 0)
    {
        return false;
    }
    out[0] = '\0';
    char openTag[64]{0};
    sniprintf(openTag, sizeof(openTag), "<%s", tag);
    char idNeedle[24]{0};
    sniprintf(idNeedle, sizeof(idNeedle), "id=\"%d\"", id);
    char closeTag[64]{0};
    sniprintf(closeTag, sizeof(closeTag), "</%s>", tag);

    const char *p = xml;
    while ((p = strstr(p, openTag)) != nullptr)
    {
        const char *gt = strchr(p, '>');
        if (!gt)
        {
            return false;
        }
        const size_t headerLen = static_cast<size_t>(gt - p);
        if (headerLen < 96)
        {
            char header[96]{0};
            memcpy(header, p, headerLen);
            header[headerLen] = '\0';
            if (strstr(header, idNeedle))
            {
                const char *end = strstr(gt + 1, closeTag);
                if (!end)
                {
                    return false;
                }
                end += strlen(closeTag);
                size_t len = static_cast<size_t>(end - p);
                if (len >= outSize)
                {
                    len = outSize - 1;
                }
                memcpy(out, p, len);
                out[len] = '\0';
                return true;
            }
        }
        p = gt + 1;
    }
    return false;
}

static bool ParseShaftEncoderSpeed(const char *xml, int id, float &speedOut)
{
    char element[512]{0};
    if (!CopyXmlElementWithId(xml, "ShaftEncoder", id, element, sizeof(element)))
    {
        return false;
    }
    return ParseXmlFloatTag(element, "Speed", speedOut);
}

static bool ParseTriggerLevel(const char *xml, int id, bool &levelOut)
{
    char element[256]{0};
    if (!CopyXmlElementWithId(xml, "Trigger", id, element, sizeof(element)))
    {
        return false;
    }
    char text[32]{0};
    if (!FindXmlTagContent(element, "Level", text, sizeof(text)))
    {
        return false;
    }
    if (StrIStartsWith(text, "High") || strcmp(text, "1") == 0 || StrIStartsWith(text, "true"))
    {
        levelOut = true;
        return true;
    }
    if (StrIStartsWith(text, "Low") || strcmp(text, "0") == 0 || StrIStartsWith(text, "false"))
    {
        levelOut = false;
        return true;
    }
    return false;
}

static bool ParseXmlBoolishTag(const char *xml, const char *tag, bool &valueOut)
{
    char text[32]{0};
    if (!FindXmlTagContent(xml, tag, text, sizeof(text)))
    {
        return false;
    }
    if (strcmp(text, "1") == 0 || StrIStartsWith(text, "true"))
    {
        valueOut = true;
        return true;
    }
    if (strcmp(text, "0") == 0 || StrIStartsWith(text, "false"))
    {
        valueOut = false;
        return true;
    }
    return false;
}

static void ReaPiSetError(const char *msg)
{
    CopyString(gReaPiSession.lastError, sizeof(gReaPiSession.lastError), msg ? msg : "");
}

static void ReaPiCloseSession()
{
    if (gReaPiSession.fd >= 0)
    {
        close(gReaPiSession.fd);
        gReaPiSession.fd = -1;
    }
    gReaPiSession.versionSelected = false;
    gReaPiSession.subscribed = false;
    gReaPiSession.connected = false;
}

static void ClearStoredReajetConfig()
{
    gReajetTargetIpText = "";
    ReaPiCloseSession();
    SetReajetConnectionState(false, false, IPADDR4{}, 0, "REAJet destination cleared");
    gStatusLastPollMs = 0;
    SaveConfigToStorage();
    iprintf("REAJet target cleared from config.\r\n");
}

static bool ReaPiSendFramedXml(int fd, const char *xml)
{
    if (!xml || !xml[0])
    {
        return false;
    }
    // Send header + body in ONE write so the frame goes out as a single TCP
    // segment, matching the working Python client (single sendall). The DOD2
    // firmware does not respond when the header and body arrive split.
    static char frameBuf[kReaPiXmlBufSize + 64];
    const size_t bodyLen = strlen(xml);
    if ((bodyLen + 32) >= sizeof(frameBuf))
    {
        return false;
    }
    const int frameLen = sniprintf(frameBuf,
                                   sizeof(frameBuf),
                                   "Content-Length: %u\n\n%s",
                                   static_cast<unsigned>(bodyLen),
                                   xml);
    if (frameLen <= 0)
    {
        return false;
    }
    return SendAllFd(fd, reinterpret_cast<const uint8_t *>(frameBuf), static_cast<size_t>(frameLen));
}

static char gReaPiLastRxRaw[512]{0};
static bool gManualReaProbeActive = false;
static volatile uint8_t gReaPlcTxBusy = 0;

static OS_CRIT &ReaPlcTxCrit()
{
    static OS_CRIT crit;
    return crit;
}

static void ReaPlcPrepareSocket(int fd)
{
    if (fd < 0)
    {
        return;
    }
    setsockoption(fd, SO_NONAGLE);
}

static bool ReaPlcStopAlreadyStoppedResponse(const char *raw)
{
    if (!raw || strlen(raw) < 20)
    {
        return false;
    }
    char errorCode[9]{0};
    CopyAsciiField(raw, strlen(raw), 12, 8, errorCode, sizeof(errorCode));
    if (strcmp(errorCode, "000b0066") == 0 || strcmp(errorCode, "000B0066") == 0)
    {
        return true;
    }
    if (strcmp(errorCode, "0000000B") == 0 || strcmp(errorCode, "0000000b") == 0)
    {
        return true;
    }
    return false;
}

static bool ReaPiRecvFramedXml(int fd, char *bodyOut, size_t bodyOutSize, uint32_t waitTicks)
{
    if (!bodyOut || bodyOutSize == 0)
    {
        return false;
    }
    bodyOut[0] = '\0';
    gReaPiLastRxRaw[0] = '\0';
    static char rxBuf[kReaPiXmlBufSize + 512];
    memset(rxBuf, 0, sizeof(rxBuf));
    size_t got = 0;

    for (int attempt = 0; attempt < 12 && (got + 1) < sizeof(rxBuf); ++attempt)
    {
        fd_set readSet;
        FD_ZERO(&readSet);
        FD_SET(fd, &readSet);
        const uint32_t thisWait = (got == 0) ? waitTicks : (TICKS_PER_SECOND / 5);
        const int ready = select(fd + 1, &readSet, nullptr, nullptr, thisWait);
        if (ready <= 0)
        {
            return false;
        }

        const size_t room = (sizeof(rxBuf) - 1) - got;
        const int rc = read(fd, rxBuf + got, static_cast<int>(room));
        if (rc <= 0)
        {
            return false;
        }
        got += static_cast<size_t>(rc);
        rxBuf[got] = '\0';
        CopyString(gReaPiLastRxRaw, sizeof(gReaPiLastRxRaw), rxBuf);

        const char *sep = strstr(rxBuf, "\r\n\r\n");
        size_t sepLen = 4;
        if (!sep)
        {
            sep = strstr(rxBuf, "\n\n");
            sepLen = 2;
        }

        if (sep)
        {
            const size_t headerLen = static_cast<size_t>(sep - rxBuf);
            char header[256]{0};
            const size_t copyHeader = (headerLen < sizeof(header) - 1) ? headerLen : (sizeof(header) - 1);
            memcpy(header, rxBuf, copyHeader);
            header[copyHeader] = '\0';

            int contentLen = -1;
            char *line = header;
            while (line && *line)
            {
                char *next = strchr(line, '\n');
                if (next)
                {
                    *next = '\0';
                    if (next > line && next[-1] == '\r')
                    {
                        next[-1] = '\0';
                    }
                }
                if (StrIStartsWith(line, "content-length:"))
                {
                    const char *colon = strchr(line, ':');
                    if (colon)
                    {
                        contentLen = atoi(colon + 1);
                    }
                }
                if (!next)
                {
                    break;
                }
                line = next + 1;
            }

            if (contentLen < 0 || static_cast<size_t>(contentLen) >= bodyOutSize)
            {
                return false;
            }

            const size_t bodyStart = headerLen + sepLen;
            const size_t haveBody = (got > bodyStart) ? (got - bodyStart) : 0;
            if (haveBody >= static_cast<size_t>(contentLen))
            {
                memcpy(bodyOut, rxBuf + bodyStart, static_cast<size_t>(contentLen));
                bodyOut[contentLen] = '\0';
                return true;
            }
        }
        else if (rxBuf[0] == '<' && strstr(rxBuf, "</REA-JET>"))
        {
            size_t copyLen = got;
            if (copyLen >= bodyOutSize)
            {
                copyLen = bodyOutSize - 1;
            }
            memcpy(bodyOut, rxBuf, copyLen);
            bodyOut[copyLen] = '\0';
            return true;
        }
    }

    return false;
}

static bool ReaPiBuildCommandXmlVariant(const char *version,
                                        uint32_t cmdId,
                                        const char *commandName,
                                        int commandStyle,
                                        bool withXmlDecl,
                                        char *out,
                                        size_t outSize)
{
    if (!version || !version[0] || !commandName || !out || outSize == 0)
    {
        return false;
    }
    // Match the working Python REA-PI client exactly: XML declaration without encoding.
    const char *xmlDecl = withXmlDecl ? "<?xml version=\"1.0\"?>\n" : "";
    if (commandStyle == 1)
    {
        return sniprintf(out,
                         outSize,
                         "%s<REA-JET><REA-PI version=\"%s\"><Command name=\"%s\" id=\"%u\"><Data/></Command></REA-PI></REA-JET>",
                         xmlDecl,
                         version,
                         commandName,
                         static_cast<unsigned>(cmdId)) > 0;
    }
    return sniprintf(out,
                     outSize,
                     "%s<REA-JET><REA-PI version=\"%s\"><Command name=\"%s\" id=\"%u\"/></REA-PI></REA-JET>",
                     xmlDecl,
                     version,
                     commandName,
                     static_cast<unsigned>(cmdId)) > 0;
}

static bool ReaPiBuildSubscribeXml(const char *eventName, char *out, size_t outSize, bool withJobArg)
{
    if (!eventName || !out || outSize == 0)
    {
        return false;
    }
    const uint32_t cmdId = gReaPiSession.nextCmdId++;
    if (withJobArg)
    {
        return sniprintf(out,
                         outSize,
                         "<?xml version=\"1.0\"?>\n"
                         "<REA-JET><REA-PI version=\"%s\"><Command name=\"SUBSCRIBE\" id=\"%u\">"
                         "<Data><EVENT>%s</EVENT><EVENT_ARGS name=\"0\">0</EVENT_ARGS></Data>"
                         "</Command></REA-PI></REA-JET>",
                         static_cast<NBString>(gReaPiVersion).c_str(),
                         static_cast<unsigned>(cmdId),
                         eventName) > 0;
    }
    return sniprintf(out,
                     outSize,
                     "<?xml version=\"1.0\"?>\n"
                     "<REA-JET><REA-PI version=\"%s\"><Command name=\"SUBSCRIBE\" id=\"%u\">"
                     "<Data><EVENT>%s</EVENT></Data>"
                     "</Command></REA-PI></REA-JET>",
                     static_cast<NBString>(gReaPiVersion).c_str(),
                     static_cast<unsigned>(cmdId),
                     eventName) > 0;
}

static bool ReaPiSendSubscribeEvent(const char *eventName, bool withJobArg, char *respOut, size_t respOutSize, uint32_t waitTicks)
{
    if (gReaPiSession.fd < 0)
    {
        return false;
    }
    char xml[kReaPiXmlBufSize]{0};
    if (!ReaPiBuildSubscribeXml(eventName, xml, sizeof(xml), withJobArg))
    {
        return false;
    }
    if (!ReaPiSendFramedXml(gReaPiSession.fd, xml))
    {
        gReaPiSession.commandErrors++;
        ReaPiSetError("REA-PI subscribe send failed");
        ReaPiCloseSession();
        return false;
    }
    if (!respOut || respOutSize == 0)
    {
        return true;
    }
    if (!ReaPiRecvFramedXml(gReaPiSession.fd, respOut, respOutSize, waitTicks))
    {
        gReaPiSession.commandErrors++;
        ReaPiSetError("REA-PI subscribe recv failed");
        ReaPiCloseSession();
        return false;
    }
    return true;
}

static bool ReaPiRecvProtocolLine(int fd, char *lineOut, size_t lineOutSize, uint32_t waitTicks)
{
    if (!lineOut || lineOutSize == 0)
    {
        return false;
    }
    lineOut[0] = '\0';
    size_t got = 0;
    while (got + 1 < lineOutSize)
    {
        uint8_t b = 0;
        if (!RecvExactFd(fd, &b, 1, waitTicks))
        {
            return false;
        }
        lineOut[got++] = static_cast<char>(b);
        lineOut[got] = '\0';
        if (b == '\n')
        {
            break;
        }
    }
    return got > 0 && lineOut[got - 1] == '\n' && strstr(lineOut, "REA-PI") != nullptr;
}

static bool ReaPiSendCommand(const char *commandName, char *respOut, size_t respOutSize, uint32_t waitTicks)
{
    if (gReaPiSession.fd < 0)
    {
        return false;
    }
    const char *version = static_cast<NBString>(gReaPiVersion).c_str();
    char xml[kReaPiXmlBufSize]{0};
    const uint32_t cmdId = gReaPiSession.nextCmdId++;
    // Follow REA-PI manual command envelope exactly: Command + Data node.
    if (!ReaPiBuildCommandXmlVariant(version, cmdId, commandName, 1, true, xml, sizeof(xml)))
    {
        return false;
    }
    if (!ReaPiSendFramedXml(gReaPiSession.fd, xml))
    {
        gReaPiSession.commandErrors++;
        ReaPiSetError("REA-PI send failed");
        ReaPiCloseSession();
        return false;
    }
    if (!respOut || respOutSize == 0)
    {
        return true;
    }
    if (!ReaPiRecvFramedXml(gReaPiSession.fd, respOut, respOutSize, waitTicks))
    {
        gReaPiSession.commandErrors++;
        ReaPiSetError("REA-PI recv failed");
        ReaPiCloseSession();
        return false;
    }
    return true;
}

static void ReaPiParseInstallationActivityXml(const char *xml)
{
    if (!xml || !xml[0])
    {
        return;
    }
    float speed = 0.0f;
    if (ParseXmlFloatTag(xml, "ProductSpeed", speed))
    {
        gReaPiSession.productSpeed = speed;
        gReaPiSession.productSpeedValid = true;
    }
    else if (ParseXmlFloatTag(xml, "Speed", speed))
    {
        gReaPiSession.productSpeed = speed;
        gReaPiSession.productSpeedValid = true;
    }
    if (ParseShaftEncoderSpeed(xml, 0, speed))
    {
        gReaPiSession.encoder1Speed = speed;
        gReaPiSession.encoder1SpeedValid = true;
        gReaPiSession.productSpeed = speed;
        gReaPiSession.productSpeedValid = true;
    }
    if (ParseShaftEncoderSpeed(xml, 1, speed))
    {
        gReaPiSession.encoder2Speed = speed;
        gReaPiSession.encoder2SpeedValid = true;
    }
    if (ParseXmlFloatTag(xml, "ShaftEncoderSpeed", speed))
    {
        gReaPiSession.encoder1Speed = speed;
        gReaPiSession.encoder1SpeedValid = true;
    }
    static const char *kTrigTags[4] = {"Trigger1Level", "Trigger2Level", "Trigger3Level", "Trigger4Level"};
    for (int i = 0; i < 4; ++i)
    {
        bool level = false;
        if (ParseXmlBoolishTag(xml, kTrigTags[i], level) || ParseTriggerLevel(xml, i, level))
        {
            gReaPiSession.triggerLevel[i] = level;
            gReaPiSession.triggerLevelValid = true;
        }
    }
}

static void ReaPiHandleEventXml(const char *xml)
{
    if (!xml || !xml[0])
    {
        return;
    }
    CopyString(gReaPiSession.lastEventXml, sizeof(gReaPiSession.lastEventXml), xml);
    gReaPiSession.eventsReceived++;
    {
        const char *nameAttr = strstr(xml, "Command name=\"");
        if (nameAttr)
        {
            nameAttr += 14;
            const char *endQuote = strchr(nameAttr, '"');
            if (endQuote)
            {
                size_t len = static_cast<size_t>(endQuote - nameAttr);
                if (len >= sizeof(gReaPiSession.lastEventName))
                {
                    len = sizeof(gReaPiSession.lastEventName) - 1;
                }
                memcpy(gReaPiSession.lastEventName, nameAttr, len);
                gReaPiSession.lastEventName[len] = '\0';
            }
        }
    }
    if (strstr(xml, "INSTALLATIONACTIVITY") || strstr(xml, "InstallationActivity"))
    {
        ReaPiParseInstallationActivityXml(xml);
    }
    if (strstr(xml, "\"PRINTTRIGGER\""))
    {
        gReaPiSession.printTriggerCount++;
    }
    if (strstr(xml, "\"PRINTSTART\""))
    {
        gReaPiSession.printStartCount++;
    }
    if (strstr(xml, "\"PRINTEND\""))
    {
        gReaPiSession.printEndCount++;
    }
    if (strstr(xml, "\"PRINTABORTED\""))
    {
        gReaPiSession.printAbortedCount++;
    }
    if (strstr(xml, "\"MISSINGCONTENT\""))
    {
        gReaPiSession.missingContentCount++;
    }
    if (strstr(xml, "\"PRINTREJECTED\""))
    {
        gReaPiSession.printRejectedCount++;
    }
    if (strstr(xml, "PRINTSPEEDERROR") || strstr(xml, "PrintSpeedError"))
    {
        char codeText[16]{0};
        if (FindXmlTagContent(xml, "Code", codeText, sizeof(codeText)))
        {
            gReaLiveStatus.printSpeedErrorCode = atoi(codeText);
            gReaLiveStatus.printSpeedErrorValid = true;
        }
    }
    if (strstr(xml, "JOBSET") || strstr(xml, "JobSet"))
    {
        char name[64]{0};
        if (FindXmlTagContent(xml, "Filename", name, sizeof(name)) ||
            FindXmlTagContent(xml, "JobFileName", name, sizeof(name)) ||
            FindXmlTagContent(xml, "FileName", name, sizeof(name)) ||
            FindXmlTagContent(xml, "JobName", name, sizeof(name)))
        {
            CopyString(gReaLiveStatus.jobSetFileName, sizeof(gReaLiveStatus.jobSetFileName), name);
            gReaLiveStatus.jobSetFileNameValid = true;
        }
    }
}

static bool ReaPiSubscribeDefaultEvents()
{
    // Verified live on DOD2 firmware 3.96: all of these subscriptions are accepted.
    static const struct
    {
        const char *eventName;
        bool required;
    } kSubs[] = {
        {"INSTALLATIONACTIVITY", true},   // speed + trigger levels (the critical one)
        {"JOBSET", false},
        {"JOBSTARTED", false},
        {"JOBSTOPPED", false},
        {"PRINTSPEEDERROR", false},
        {"PRINTTRIGGER", false},
        {"PRINTSTART", false},
        {"PRINTEND", false},
        {"PRINTABORTED", false},
        {"MISSINGCONTENT", false},
        {"PRINTREJECTED", false},
    };
    char resp[kReaPiXmlBufSize]{0};
    for (size_t i = 0; i < sizeof(kSubs) / sizeof(kSubs[0]); ++i)
    {
        resp[0] = '\0';
        if (!ReaPiSendSubscribeEvent(kSubs[i].eventName, false, resp, sizeof(resp), TICKS_PER_SECOND * 3))
        {
            if (kSubs[i].required)
            {
                ReaPiSetError("REA-PI subscribe failed");
                return false;
            }
        }
    }
    gReaPiSession.subscribed = true;
    return true;
}

static bool ReaPiConnectSession()
{
    ReaPiCloseSession();
    if (!IsNetworkLinkReady())
    {
        ReaPiSetError("Network not ready");
        return false;
    }
    char ipText[64]{0};
    CopyReajetTargetIpText(ipText, sizeof(ipText));
    IPADDR4 targetIp{};
    if (IsLoopbackIpText(ipText) || !ParseIpv4Text(ipText, targetIp))
    {
        ReaPiSetError("invalid REAJet IP");
        return false;
    }
    const uint16_t port = static_cast<uint16_t>(static_cast<uint32_t>(gReaPiPort));
    const int fd = connect(targetIp, port, TICKS_PER_SECOND * 3);
    if (fd < 0)
    {
        ReaPiSetError("REA-PI TCP connect failed");
        gReaPiSession.connectAttempts++;
        gReaPiSession.lastConnectSec = static_cast<uint32_t>(Secs);
        return false;
    }
    gReaPiSession.fd = fd;
    gReaPiSession.connectAttempts++;
    gReaPiSession.lastConnectSec = static_cast<uint32_t>(Secs);

    char protocolLine[40]{0};
    if (!ReaPiRecvProtocolLine(fd, protocolLine, sizeof(protocolLine), TICKS_PER_SECOND))
    {
        ReaPiSetError("REA-PI protocol banner read failed");
        ReaPiCloseSession();
        return false;
    }
    if (!SendAllFd(fd, kReaPiBanner, kReaPiBannerLen))
    {
        ReaPiSetError("REA-PI banner echo failed");
        ReaPiCloseSession();
        return false;
    }
    char welcomeResp[kReaPiXmlBufSize]{0};
    if (!ReaPiRecvFramedXml(fd, welcomeResp, sizeof(welcomeResp), TICKS_PER_SECOND * 3))
    {
        ReaPiSetError("REA-PI Welcome read failed");
        ReaPiCloseSession();
        return false;
    }

    char versionSelect[256]{0};
    sniprintf(versionSelect,
              sizeof(versionSelect),
              "<REA-JET><REA-PI><VersionSelect><Version>%s</Version></VersionSelect></REA-PI></REA-JET>",
              static_cast<NBString>(gReaPiVersion).c_str());
    if (!ReaPiSendFramedXml(fd, versionSelect))
    {
        ReaPiSetError("REA-PI VersionSelect send failed");
        ReaPiCloseSession();
        return false;
    }
    char versionResp[kReaPiXmlBufSize]{0};
    if (!ReaPiRecvFramedXml(fd, versionResp, sizeof(versionResp), TICKS_PER_SECOND * 2))
    {
        ReaPiSetError("REA-PI VersionSelection missing; continuing");
    }
    gReaPiSession.versionSelected = (versionResp[0] != '\0');
    gReaPiSession.connected = true;

    char deviceResp[kReaPiXmlBufSize]{0};
    if (!ReaPiSendCommand("GETDEVICEINFO", deviceResp, sizeof(deviceResp), TICKS_PER_SECOND * 3))
    {
        ReaPiSetError("REA-PI GETDEVICEINFO failed");
        ReaPiCloseSession();
        return false;
    }
    if (!ReaPiSubscribeDefaultEvents())
    {
        ReaPiCloseSession();
        return false;
    }
    ReaPiSetError("");
    return true;
}

static void ReaPiPumpEvents()
{
    if (gReaPiSession.fd < 0)
    {
        return;
    }
    fd_set readSet;
    FD_ZERO(&readSet);
    FD_SET(gReaPiSession.fd, &readSet);
    const uint32_t wait = TICKS_PER_SECOND / 10;
    const int ready = select(gReaPiSession.fd + 1, &readSet, nullptr, nullptr, wait);
    if (ready <= 0)
    {
        return;
    }
    char eventXml[kReaPiXmlBufSize]{0};
    if (ReaPiRecvFramedXml(gReaPiSession.fd, eventXml, sizeof(eventXml), TICKS_PER_SECOND / 2))
    {
        ReaPiHandleEventXml(eventXml);
    }
    else
    {
        ReaPiSetError("REA-PI event recv failed");
        ReaPiCloseSession();
    }
}

static void MergeReaPiIntoLiveStatus()
{
    if (gReaPiSession.productSpeedValid)
    {
        gReaLiveStatus.clockSpeedMpm = gReaPiSession.productSpeed;
        gReaLiveStatus.clockSpeedValid = true;
    }
    if (gReaPiSession.encoder1SpeedValid)
    {
        gReaLiveStatus.clockSpeedMpm = gReaPiSession.encoder1Speed;
        gReaLiveStatus.clockSpeedValid = true;
    }
    if (gReaPiSession.encoder2SpeedValid)
    {
        gReaLiveStatus.clockSpeed2Mpm = gReaPiSession.encoder2Speed;
        gReaLiveStatus.clockSpeed2Valid = true;
    }
    if (gReaPiSession.triggerLevelValid)
    {
        for (int i = 0; i < 4; ++i)
        {
            gReaLiveStatus.triggerLevel[i] = gReaPiSession.triggerLevel[i];
        }
        gReaLiveStatus.triggerLevelValid = true;
    }
}

// After a job set, read back the current encoder/clock speed (cached from
// REA-PI INSTALLATIONACTIVITY events) and write it to the mapping's PLC REAL tag.
static bool RefreshLiveClockSpeedFromReaPi(uint32_t waitTicks)
{
    if (gReaPiSession.fd < 0)
    {
        return gReaLiveStatus.clockSpeedValid;
    }

    const uint32_t stepTicks = TICKS_PER_SECOND / 10;
    const uint32_t attempts = (waitTicks + stepTicks - 1u) / stepTicks;
    for (uint32_t attempt = 0; attempt < attempts; ++attempt)
    {
        ReaPiPumpEvents();
        MergeReaPiIntoLiveStatus();
        if (gReaLiveStatus.clockSpeedValid)
        {
            return true;
        }
        if (attempt + 1u < attempts)
        {
            OSTimeDly(stepTicks);
        }
    }
    return gReaLiveStatus.clockSpeedValid;
}

static bool WriteMappingSpeedToPlc(const MappingRecord &m)
{
    if (!m.speedTag[0])
    {
        return true;
    }
    (void)RefreshLiveClockSpeedFromReaPi(TICKS_PER_SECOND * 2);
    if (!gReaLiveStatus.clockSpeedValid)
    {
        return false;
    }
    uint8_t realBuf[4]{0};
    memcpy(realBuf, &gReaLiveStatus.clockSpeedMpm, sizeof(realBuf));
    char writeErr[96]{0};
    return WritePlcTagRaw(m.speedTag, 0xCA, realBuf, sizeof(realBuf), writeErr, sizeof(writeErr));
}

static void MaybeRunReaPiService()
{
    if (gManualReaProbeActive)
    {
        return;
    }
    if (!static_cast<bool>(gReaPiEnabled))
    {
        if (gReaPiSession.fd >= 0)
        {
            ReaPiCloseSession();
        }
        return;
    }
    if (!IsNetworkLinkReady())
    {
        if (gReaPiSession.fd >= 0)
        {
            ReaPiCloseSession();
        }
        return;
    }

    const uint32_t nowSec = static_cast<uint32_t>(Secs);
    const uint32_t reconnectSec = static_cast<uint32_t>(gReaPiReconnectSec);
    if (gReaPiSession.fd < 0)
    {
        if (gReaPiSession.lastConnectSec == 0u ||
            (nowSec - gReaPiSession.lastConnectSec) >= reconnectSec)
        {
            (void)ReaPiConnectSession();
            MergeReaPiIntoLiveStatus();
            WriteReaStatusToPlc();
        }
        return;
    }

    ReaPiPumpEvents();
    MergeReaPiIntoLiveStatus();

    // DOD2 reports GETPRINTSTATUS as an unknown command. Keep the REA-PI
    // session open and rely on subscribed INSTALLATIONACTIVITY events for speed.
    gReaPiSession.lastKeepAliveSec = nowSec;
    WriteReaStatusToPlc();
}

static void UpdateReaLiveStatusFromResponse(const char *raw)
{
    if (ParseReaPlcStatusResponse(raw, gReaLiveStatus))
    {
        MergeReaPiIntoLiveStatus();
    }
}

static bool WriteReaStatusToPlc()
{
    if (!IsNetworkLinkReady())
    {
        return false;
    }

    bool ok = true;
    char writeErr[96]{0};

    if (static_cast<NBString>(gStatusJobAssignedTag).c_str()[0])
    {
        uint8_t v = gReaLiveStatus.jobAssigned ? 1u : 0u;
        if (!WritePlcTagRaw(static_cast<NBString>(gStatusJobAssignedTag).c_str(), 0xC1, &v, 1, writeErr, sizeof(writeErr)))
        {
            ok = false;
        }
    }
    if (static_cast<NBString>(gStatusJobReleasedTag).c_str()[0])
    {
        uint8_t v = gReaLiveStatus.jobReleased ? 1u : 0u;
        if (!WritePlcTagRaw(static_cast<NBString>(gStatusJobReleasedTag).c_str(), 0xC1, &v, 1, writeErr, sizeof(writeErr)))
        {
            ok = false;
        }
    }
    if (static_cast<NBString>(gStatusPrinterActiveTag).c_str()[0])
    {
        uint8_t v = gReaLiveStatus.printerActive ? 1u : 0u;
        if (!WritePlcTagRaw(static_cast<NBString>(gStatusPrinterActiveTag).c_str(), 0xC1, &v, 1, writeErr, sizeof(writeErr)))
        {
            ok = false;
        }
    }
    if (static_cast<NBString>(gStatusPollOkTag).c_str()[0])
    {
        uint8_t v = gReaLiveStatus.pollOk ? 1u : 0u;
        if (!WritePlcTagRaw(static_cast<NBString>(gStatusPollOkTag).c_str(), 0xC1, &v, 1, writeErr, sizeof(writeErr)))
        {
            ok = false;
        }
    }
    if (static_cast<NBString>(gStatusJobStateTag).c_str()[0])
    {
        uint8_t dintBuf[4]{0};
        WriteLe32(dintBuf, gReaLiveStatus.jobStateRaw);
        if (!WritePlcTagRaw(static_cast<NBString>(gStatusJobStateTag).c_str(), 0xC4, dintBuf, sizeof(dintBuf), writeErr, sizeof(writeErr)))
        {
            ok = false;
        }
    }
    if (gReaLiveStatus.clockSpeedValid && static_cast<NBString>(gStatusClockSpeedTag).c_str()[0])
    {
        uint8_t realBuf[4]{0};
        memcpy(realBuf, &gReaLiveStatus.clockSpeedMpm, sizeof(realBuf));
        if (!WritePlcTagRaw(static_cast<NBString>(gStatusClockSpeedTag).c_str(), 0xCA, realBuf, sizeof(realBuf), writeErr, sizeof(writeErr)))
        {
            ok = false;
        }
    }
    if (gReaLiveStatus.clockSpeed2Valid && static_cast<NBString>(gStatusClockSpeed2Tag).c_str()[0])
    {
        uint8_t realBuf[4]{0};
        memcpy(realBuf, &gReaLiveStatus.clockSpeed2Mpm, sizeof(realBuf));
        if (!WritePlcTagRaw(static_cast<NBString>(gStatusClockSpeed2Tag).c_str(), 0xCA, realBuf, sizeof(realBuf), writeErr, sizeof(writeErr)))
        {
            ok = false;
        }
    }
    if (gReaLiveStatus.triggerLevelValid)
    {
        struct TrigTagPair { config_string *tag; int index; };
        static TrigTagPair trigTags[] = {
            {&gStatusTrig1Tag, 0}, {&gStatusTrig2Tag, 1}, {&gStatusTrig3Tag, 2}, {&gStatusTrig4Tag, 3},
        };
        for (size_t ti = 0; ti < sizeof(trigTags) / sizeof(trigTags[0]); ++ti)
        {
            const char *tagPath = static_cast<NBString>(*trigTags[ti].tag).c_str();
            if (!tagPath[0])
            {
                continue;
            }
            const uint8_t v = gReaLiveStatus.triggerLevel[trigTags[ti].index] ? 1u : 0u;
            if (!WritePlcTagRaw(tagPath, 0xC1, &v, 1, writeErr, sizeof(writeErr)))
            {
                ok = false;
            }
        }
    }
    if (static_cast<NBString>(gStatusJobStartedStoppedTag).c_str()[0])
    {
        uint8_t v = gReaLiveStatus.jobReleased ? 1u : 0u;
        if (!WritePlcTagRaw(static_cast<NBString>(gStatusJobStartedStoppedTag).c_str(), 0xC1, &v, 1, writeErr, sizeof(writeErr)))
        {
            ok = false;
        }
    }
    if (gReaLiveStatus.printSpeedErrorValid && static_cast<NBString>(gStatusPrintSpeedErrorTag).c_str()[0])
    {
        uint8_t intBuf[4]{0};
        WriteLe32(intBuf, static_cast<uint32_t>(gReaLiveStatus.printSpeedErrorCode));
        if (!WritePlcTagRaw(static_cast<NBString>(gStatusPrintSpeedErrorTag).c_str(), 0xC4, intBuf, sizeof(intBuf), writeErr, sizeof(writeErr)))
        {
            ok = false;
        }
    }
    if (static_cast<NBString>(gStatusReaPiOkTag).c_str()[0])
    {
        const uint8_t v = (gReaPiSession.connected && gReaPiSession.subscribed) ? 1u : 0u;
        if (!WritePlcTagRaw(static_cast<NBString>(gStatusReaPiOkTag).c_str(), 0xC1, &v, 1, writeErr, sizeof(writeErr)))
        {
            ok = false;
        }
    }
    return ok;
}

static void RunReaStatusPollOnce()
{
    if (!IsNetworkLinkReady())
    {
        return;
    }

    char ipText[64]{0};
    CopyReajetTargetIpText(ipText, sizeof(ipText));
    IPADDR4 targetIp{};
    if (IsLoopbackIpText(ipText) || !ParseIpv4Text(ipText, targetIp))
    {
        return;
    }
    const bool useEot = (static_cast<uint32_t>(gReajetTargetPort) == 22169u);

    char frame[32]{0};
    sniprintf(frame, sizeof(frame), "0008%08lX000000", static_cast<unsigned long>(gReaStatusRequestId++));

    char resp[96]{0};
    if (!SendReajetAscii(frame, useEot, resp, sizeof(resp)))
    {
        gReaLiveStatus.pollOk = false;
        gReaLiveStatus.pollErrors++;
        return;
    }
    UpdateReaLiveStatusFromResponse(resp);
    WriteReaStatusToPlc();
}

static void MaybeRunReaStatusPoll()
{
    if (gManualReaProbeActive || gReaPlcTxBusy)
    {
        return;
    }
    const uint32_t intervalMs = static_cast<uint32_t>(gStatusPollMs);
    if (intervalMs == 0u)
    {
        return;
    }
    const uint32_t nowMs = static_cast<uint32_t>(Secs * 1000);
    if (gStatusLastPollMs != 0u && (nowMs - gStatusLastPollMs) < intervalMs)
    {
        return;
    }
    gStatusLastPollMs = nowMs;
    RunReaStatusPollOnce();
}

static bool SendReajetAscii(const char *ascii, bool appendEot, char *respOut, size_t respOutSize);

static void SetRuntimeLastTxPayload(const char *jobValue, const char *textValue)
{
    CopyString(gRuntimeLastTxJob, sizeof(gRuntimeLastTxJob), jobValue ? jobValue : "");
    CopyString(gRuntimeLastTxText, sizeof(gRuntimeLastTxText), textValue ? textValue : "");
    gRuntimeLastTxAsciiStop[0] = '\0';
    gRuntimeLastTxAsciiJob[0] = '\0';
    gRuntimeLastTxAsciiText[0] = '\0';
    gRuntimeLastTxAsciiStart[0] = '\0';
    gRuntimeLastTxAscii[0] = '\0';
    gRuntimeLastTxCmd[0] = '\0';
    gRuntimeLastTxJobStateVerified[0] = '\0';
}

static bool PollReajetJobStatus(bool useEot, char *jobStatusOut, size_t jobStatusOutSize, bool *releasedOut)
{
    if (jobStatusOut && jobStatusOutSize > 0)
    {
        jobStatusOut[0] = '\0';
    }
    if (releasedOut)
    {
        *releasedOut = false;
    }

    char frame[32]{0};
    sniprintf(frame, sizeof(frame), "0008%08lX000000", static_cast<unsigned long>(gReaStatusRequestId++));
    char resp[96]{0};
    if (!SendReajetAscii(frame, useEot, resp, sizeof(resp)))
    {
        return false;
    }

    ReaPlcLiveStatus status{};
    if (!ParseReaPlcStatusResponse(resp, status))
    {
        return false;
    }
    if (jobStatusOut && jobStatusOutSize > 0)
    {
        CopyString(jobStatusOut, jobStatusOutSize, status.jobStatus);
    }
    if (releasedOut)
    {
        *releasedOut = status.jobReleased;
    }
    return true;
}

static void AppendRuntimeJobStateVerified(const char *jobStatus, bool released)
{
    char note[128]{0};
    sniprintf(note,
              sizeof(note),
              "Verified GETSTATUS: %s (%s)",
              ReaJobStatusText(jobStatus),
              jobStatus ? jobStatus : "?");
    CopyString(gRuntimeLastTxJobStateVerified, sizeof(gRuntimeLastTxJobStateVerified), note);
    strncat(gRuntimeLastTxResponseDecoded,
            " | ",
            sizeof(gRuntimeLastTxResponseDecoded) - strlen(gRuntimeLastTxResponseDecoded) - 1);
    strncat(gRuntimeLastTxResponseDecoded,
            note,
            sizeof(gRuntimeLastTxResponseDecoded) - strlen(gRuntimeLastTxResponseDecoded) - 1);
}

static void SetRuntimeLastTxAsciiForCommand(const char *cmd, const char *ascii)
{
    if (!cmd || !ascii)
    {
        return;
    }
    if (strcmp(cmd, "0003") == 0)
    {
        CopyString(gRuntimeLastTxAsciiStop, sizeof(gRuntimeLastTxAsciiStop), ascii);
    }
    else if (strcmp(cmd, "0001") == 0)
    {
        CopyString(gRuntimeLastTxAsciiJob, sizeof(gRuntimeLastTxAsciiJob), ascii);
    }
    else if (strcmp(cmd, "0004") == 0 || strcmp(cmd, "0005") == 0)
    {
        CopyString(gRuntimeLastTxAsciiText, sizeof(gRuntimeLastTxAsciiText), ascii);
    }
    else if (strcmp(cmd, "0002") == 0)
    {
        CopyString(gRuntimeLastTxAsciiStart, sizeof(gRuntimeLastTxAsciiStart), ascii);
    }
    CopyString(gRuntimeLastTxAscii, sizeof(gRuntimeLastTxAscii), ascii);
}

static void SetRuntimeLastResponsePending(const char *cmd)
{
    char pending[128]{0};
    sniprintf(pending,
               sizeof(pending),
               "Sending REA-PLC command %s...",
               (cmd && cmd[0]) ? cmd : "?");
    CopyString(gRuntimeLastTxResponseDecoded, sizeof(gRuntimeLastTxResponseDecoded), pending);
}

static void SetRuntimeLastResponse(const char *raw)
{
    if (!raw)
    {
        gRuntimeLastTxResponse[0] = '\0';
        gRuntimeLastTxResponseDecoded[0] = '\0';
        gRuntimeLastTxResponseAck = false;
        gRuntimeLastTxErrorCode = -2;
        gRuntimeLastTxErrorText[0] = '\0';
        return;
    }
    strncpy(gRuntimeLastTxResponse, raw, sizeof(gRuntimeLastTxResponse) - 1);
    gRuntimeLastTxResponse[sizeof(gRuntimeLastTxResponse) - 1] = '\0';
    ParseReaResponseErrorInfo(gRuntimeLastTxResponse,
                              &gRuntimeLastTxResponseAck,
                              &gRuntimeLastTxErrorCode,
                              nullptr,
                              0,
                              gRuntimeLastTxErrorText,
                              sizeof(gRuntimeLastTxErrorText));
    DecodeReaResponseHuman(gRuntimeLastTxResponse,
                           gRuntimeLastTxResponseDecoded,
                           sizeof(gRuntimeLastTxResponseDecoded),
                           &gRuntimeLastTxResponseAck);
}

static bool IsValidReaPlcAsciiChar(unsigned char ch)
{
    return (ch >= 0x20 && ch <= 0x7E);
}

static bool ValidateReaPlcAsciiText(const char *text,
                                    size_t maxLen,
                                    bool allowEmpty,
                                    const char *fieldLabel,
                                    char *errOut,
                                    size_t errOutSize)
{
    if (!text)
    {
        text = "";
    }
    if (errOut && errOutSize > 0)
    {
        errOut[0] = '\0';
    }
    if (!allowEmpty && !text[0])
    {
        if (errOut && errOutSize > 1)
        {
            sniprintf(errOut, errOutSize, "%s is empty", fieldLabel ? fieldLabel : "value");
        }
        return false;
    }
    const size_t len = strlen(text);
    if (len > maxLen)
    {
        if (errOut && errOutSize > 1)
        {
            sniprintf(errOut,
                      errOutSize,
                      "%s length %u exceeds max %u",
                      fieldLabel ? fieldLabel : "value",
                      static_cast<unsigned>(len),
                      static_cast<unsigned>(maxLen));
        }
        return false;
    }
    for (size_t i = 0; i < len; ++i)
    {
        const unsigned char ch = static_cast<unsigned char>(text[i]);
        if (!IsValidReaPlcAsciiChar(ch))
        {
            if (errOut && errOutSize > 1)
            {
                sniprintf(errOut,
                          errOutSize,
                          "%s has invalid char 0x%02X at pos %u (valid: 0x20-0x7E)",
                          fieldLabel ? fieldLabel : "value",
                          static_cast<unsigned>(ch),
                          static_cast<unsigned>(i));
            }
            return false;
        }
    }
    return true;
}

static const char *ReaPlcDefaultIdBlockForCommand(const char *cmd)
{
    if (cmd && strcmp(cmd, "0005") == 0)
    {
        return "0;1;message1;Text_1;Content@value";
    }
    return "0;1;message1;Text_1";
}

static bool ValidateReajetCommandPayload(const char *cmdIn,
                                         const char *destTarget,
                                         const char *jobValue,
                                         const char *contentValue,
                                         char *errOut,
                                         size_t errOutSize)
{
    const char *cmd = (cmdIn && cmdIn[0]) ? cmdIn : "0004";
    if (errOut && errOutSize > 0)
    {
        errOut[0] = '\0';
    }

    if (strcmp(cmd, "0001") == 0)
    {
        const char *job = (jobValue && jobValue[0]) ? jobValue : nullptr;
        if (!job)
        {
            if (errOut && errOutSize > 1)
            {
                sniprintf(errOut, errOutSize, "job filename required for 0001");
            }
            return false;
        }
        return ValidateReaPlcAsciiText(job,
                                       kReaGatewayMaxJobFilenameLen,
                                       false,
                                       "job filename",
                                       errOut,
                                       errOutSize);
    }

    if (strcmp(cmd, "0002") == 0 || strcmp(cmd, "0003") == 0)
    {
        if (jobValue && jobValue[0])
        {
            return ValidateReaPlcAsciiText(jobValue,
                                           kReaGatewayMaxJobFilenameLen,
                                           false,
                                           "job filename",
                                           errOut,
                                           errOutSize);
        }
        return true;
    }

    if (strcmp(cmd, "0004") == 0 || strcmp(cmd, "0005") == 0)
    {
        if (contentValue &&
            !ValidateReaPlcAsciiText(contentValue,
                                   kReaPlcMaxContentChars,
                                   true,
                                   "content",
                                   errOut,
                                   errOutSize))
        {
            return false;
        }
        if (jobValue && jobValue[0] &&
            !ValidateReaPlcAsciiText(jobValue,
                                     kReaGatewayMaxJobFilenameLen,
                                     false,
                                     "job filename",
                                     errOut,
                                     errOutSize))
        {
            return false;
        }

        const char *idBlock = (destTarget && destTarget[0]) ? destTarget : ReaPlcDefaultIdBlockForCommand(cmd);
        const size_t contentLen = contentValue ? strlen(contentValue) : 0;
        const size_t paramLen = 4 + strlen(idBlock) + 4 + contentLen;
        if (paramLen > kReaGatewayMaxParameterBuild)
        {
            if (errOut && errOutSize > 1)
            {
                sniprintf(errOut,
                          errOutSize,
                          "content frame size %u exceeds gateway limit %u",
                          static_cast<unsigned>(paramLen),
                          static_cast<unsigned>(kReaGatewayMaxParameterBuild));
            }
            return false;
        }
        if (contentLen > kReaPlcMaxContentChars)
        {
            if (errOut && errOutSize > 1)
            {
                sniprintf(errOut,
                          errOutSize,
                          "content length %u exceeds REA-PLC max %u",
                          static_cast<unsigned>(contentLen),
                          static_cast<unsigned>(kReaPlcMaxContentChars));
            }
            return false;
        }
        return true;
    }

    return true;
}

static void BuildReajet0001Parameter(const char *jobValue, const char *fallbackTarget, char *parameter, size_t parameterSize)
{
    if (!parameter || parameterSize < 2)
    {
        return;
    }
    parameter[0] = '\0';
    const char *value = (jobValue && jobValue[0]) ? jobValue : nullptr;
    if (value)
    {
        const char *pipe = strchr(value, '|');
        if (pipe && pipe > value)
        {
            sniprintf(parameter, parameterSize, "%.*s%s", static_cast<int>(pipe - value), value, pipe + 1);
        }
        else
        {
            sniprintf(parameter, parameterSize, "0%s", value);
        }
        return;
    }
    if (fallbackTarget && fallbackTarget[0])
    {
        const char *pipe = strchr(fallbackTarget, '|');
        if (pipe && pipe > fallbackTarget)
        {
            sniprintf(parameter, parameterSize, "%.*s%s", static_cast<int>(pipe - fallbackTarget), fallbackTarget, pipe + 1);
        }
        else
        {
            strncpy(parameter, fallbackTarget, parameterSize - 1);
            parameter[parameterSize - 1] = '\0';
        }
        return;
    }
    strncpy(parameter, "0default.job", parameterSize - 1);
    parameter[parameterSize - 1] = '\0';
}

static bool BuildReajetAsciiFrameCmd(const char *cmdIn,
                                     const char *destTarget,
                                     const char *jobValue,
                                     const char *contentValue,
                                     char *asciiOut,
                                     size_t asciiOutSize)
{
    if (!asciiOut || asciiOutSize < 32)
    {
        return false;
    }
    const char *cmd = (cmdIn && cmdIn[0]) ? cmdIn : "0004";
    const char *target = destTarget ? destTarget : "";
    const char *content = contentValue ? contentValue : "";
    char parameter[kReaGatewayParameterBufSize]{0};
    char validationErr[128]{0};
    if (!ValidateReajetCommandPayload(cmd, target, jobValue, contentValue, validationErr, sizeof(validationErr)))
    {
        iprintf("REA-PLC payload validation failed cmd=%s: %s\r\n", cmd, validationErr);
        return false;
    }

    if (strcmp(cmd, "0001") == 0)
    {
        BuildReajet0001Parameter(jobValue, target, parameter, sizeof(parameter));
    }
    else if (strcmp(cmd, "0002") == 0 || strcmp(cmd, "0003") == 0)
    {
        if (jobValue && jobValue[0])
        {
            strncpy(parameter, jobValue, sizeof(parameter) - 1);
            parameter[sizeof(parameter) - 1] = '\0';
        }
        else
        {
            strncpy(parameter, "0", sizeof(parameter) - 1);
            parameter[sizeof(parameter) - 1] = '\0';
        }
    }
    else if (strcmp(cmd, "0008") == 0)
    {
        parameter[0] = '\0';
    }
    else if (strcmp(cmd, "0004") == 0 || strcmp(cmd, "0005") == 0)
    {
        const char *idBlock = target[0] ? target :
            (strcmp(cmd, "0004") == 0 ? "0;1;message1;Text_1" : "0;1;message1;Text_1;Content@value");
        char lenA[8]{0};
        char lenB[8]{0};
        ToHexAsciiUnsigned(static_cast<uint32_t>(strlen(idBlock)), 4, lenA, sizeof(lenA));
        ToHexAsciiUnsigned(static_cast<uint32_t>(strlen(content)), 4, lenB, sizeof(lenB));
        sniprintf(parameter, sizeof(parameter), "%s%s%s%s", lenA, idBlock, lenB, content);
    }
    else
    {
        parameter[0] = '\0';
    }

    const size_t parameterLen = strlen(parameter);
    if (parameterLen > kReaGatewayMaxParameterBuild || parameterLen > kReaPlcMaxParameterChars)
    {
        iprintf("REA-PLC parameter length %u exceeds limit\r\n", static_cast<unsigned>(parameterLen));
        return false;
    }

    char paramLenHex[8]{0};
    ToHexAsciiUnsigned(static_cast<uint32_t>(parameterLen), 6, paramLenHex, sizeof(paramLenHex));
    const int frameLen = sniprintf(asciiOut, asciiOutSize, "%s00000001%s%s", cmd, paramLenHex, parameter);
    if (frameLen < 0 || static_cast<size_t>(frameLen) >= asciiOutSize)
    {
        iprintf("REA-PLC ASCII frame truncated cmd=%s\r\n", cmd);
        return false;
    }
    return true;
}

static bool ReadMappingPlcValue(const char *tagPath, char *valueOut, size_t valueOutSize)
{
    if (!tagPath || !tagPath[0] || !valueOut || valueOutSize < 2)
    {
        return false;
    }
    uint8_t data[512]{0};
    size_t dataLen = 0;
    uint16_t typeCode = 0;
    char error[96]{0};
    if (!ReadPlcTagRaw(tagPath, typeCode, data, sizeof(data), dataLen, error, sizeof(error)))
    {
        return false;
    }
    FormatTagValueText(typeCode, data, dataLen, valueOut, valueOutSize);
    return true;
}

static bool ReadMappingPlcBool(const char *tagPath, bool &valueOut)
{
    char text[16]{0};
    if (!ReadMappingPlcValue(tagPath, text, sizeof(text)))
    {
        return false;
    }
    valueOut = (strcmp(text, "1") == 0);
    return true;
}

static bool SendMappingReajetCommand(const MappingRecord &m,
                                     const char *cmd,
                                     const char *jobValue,
                                     const char *contentValue,
                                     bool useEot,
                                     bool writeAckToPlc)
{
    char ascii[1000]{0};
    if (!BuildReajetAsciiFrameCmd(cmd, m.destTarget, jobValue, contentValue, ascii, sizeof(ascii)))
    {
        iprintf("Mapping runtime build failed id=%d cmd=%s\r\n", m.id, cmd);
        if (writeAckToPlc)
        {
            WriteMappingResponseToPlc(m, false, -3);
        }
        return false;
    }
    char resp[96]{0};
    CopyString(gRuntimeLastTxCmd, sizeof(gRuntimeLastTxCmd), cmd ? cmd : "");
    SetRuntimeLastTxAsciiForCommand(cmd, ascii);
    gRuntimeLastTxMappingId = m.id;
    SetRuntimeLastResponsePending(cmd);
    if (!SendReajetAscii(ascii, useEot, resp, sizeof(resp)))
    {
        SetRuntimeLastResponse("<send_failed>");
        iprintf("Mapping runtime TX failed id=%d cmd=%s frame=%s eot=%u\r\n",
                m.id,
                cmd,
                ascii,
                useEot ? 1u : 0u);
        if (writeAckToPlc)
        {
            WriteMappingResponseToPlc(m, false, gRuntimeLastTxErrorCode);
        }
        return false;
    }
    SetRuntimeLastResponse(resp);
    iprintf("Mapping runtime TX ok id=%d cmd=%s frame=%s\r\n", m.id, cmd, ascii);
    if (!gRuntimeLastTxResponseAck)
    {
        iprintf("Mapping runtime REA reject id=%d cmd=%s err=%ld decoded=%s\r\n",
                m.id,
                cmd,
                static_cast<long>(gRuntimeLastTxErrorCode),
                gRuntimeLastTxResponseDecoded);
    }
    if (writeAckToPlc)
    {
        WriteMappingResponseToPlc(m, gRuntimeLastTxResponseAck, gRuntimeLastTxErrorCode);
    }
    return true;
}

static bool SendReajetAsciiTo(IPADDR4 targetIp, uint16_t port, const char *ascii, bool appendEot, char *respOut, size_t respOutSize, bool updateConnState)
{
    if (!ascii || !ascii[0])
    {
        return false;
    }
    if (!IsNetworkLinkReady())
    {
        iprintf("REA-PLC fail network-not-ready port=%u\r\n", static_cast<unsigned>(port));
        return false;
    }
    if (targetIp.IsNull() || port < 1)
    {
        iprintf("REA-PLC fail bad-target port=%u\r\n", static_cast<unsigned>(port));
        return false;
    }

    OSCriticalSectionObj crit(ReaPlcTxCrit());
    gReaPlcTxBusy = 1;

    const uint32_t connectTimeout = (TICKS_PER_SECOND >= 4u) ? (TICKS_PER_SECOND / 4u) : 1u;
    const uint32_t recvTimeout = (TICKS_PER_SECOND >= 4u) ? (TICKS_PER_SECOND / 4u) : 1u;

    const int fd = connect(targetIp, port, connectTimeout);
    if (fd < 0)
    {
        iprintf("REA-PLC fail connect %hI:%u\r\n", targetIp, static_cast<unsigned>(port));
        gReaPlcTxBusy = 0;
        if (updateConnState)
        {
            char msg[96]{0};
            sniprintf(msg, sizeof(msg), "TCP connect to %u failed", static_cast<unsigned>(port));
            SetReajetConnectionState(true, false, targetIp, port, msg);
        }
        return false;
    }
    ReaPlcPrepareSocket(fd);
    uint8_t tx[1000]{0};
    size_t txLen = strlen(ascii);
    if (txLen >= (sizeof(tx) - 2))
    {
        txLen = sizeof(tx) - 2;
    }
    memcpy(tx, ascii, txLen);
    if (appendEot)
    {
        tx[txLen++] = 0x04;
    }
    if (!SendAllFd(fd, tx, txLen))
    {
        iprintf("REA-PLC fail send %hI:%u len=%u eot=%u\r\n",
                targetIp,
                static_cast<unsigned>(port),
                static_cast<unsigned>(txLen),
                appendEot ? 1u : 0u);
        close(fd);
        gReaPlcTxBusy = 0;
        if (updateConnState)
        {
            SetReajetConnectionState(true, false, targetIp, port, "TCP send failed");
        }
        return false;
    }
    const size_t rxLen = appendEot ? 65 : 64;
    uint8_t rx[96]{0};
    const bool got = RecvExactFd(fd, rx, rxLen, recvTimeout);
    close(fd);
    gReaPlcTxBusy = 0;
    if (!got)
    {
        iprintf("REA-PLC fail recv %hI:%u expect=%u eot=%u\r\n",
                targetIp,
                static_cast<unsigned>(port),
                static_cast<unsigned>(rxLen),
                appendEot ? 1u : 0u);
    }
    if (updateConnState)
    {
        char msg[96]{0};
        if (got)
        {
            sniprintf(msg, sizeof(msg), "TCP connect to %u ok", static_cast<unsigned>(port));
            SetReajetConnectionState(true, true, targetIp, port, msg);
        }
        else
        {
            sniprintf(msg, sizeof(msg), "TCP response from %u failed", static_cast<unsigned>(port));
            SetReajetConnectionState(true, false, targetIp, port, msg);
        }
    }
    if (respOut && respOutSize > 1)
    {
        respOut[0] = '\0';
        if (got)
        {
            size_t n = appendEot ? 64 : rxLen;
            if (n >= respOutSize)
            {
                n = respOutSize - 1;
            }
            memcpy(respOut, rx, n);
            respOut[n] = '\0';
        }
    }
    return got;
}

static bool SendReajetAscii(const char *ascii, bool appendEot, char *respOut, size_t respOutSize)
{
    char ipText[64]{0};
    CopyReajetTargetIpText(ipText, sizeof(ipText));
    IPADDR4 targetIp{};
    if (IsLoopbackIpText(ipText) || !ParseIpv4Text(ipText, targetIp))
    {
        iprintf("REA-PLC fail bad-ip \"%s\"\r\n", ipText);
        return false;
    }
    const uint16_t port = static_cast<uint16_t>(static_cast<uint32_t>(gReajetTargetPort));
    return SendReajetAsciiTo(targetIp, port, ascii, appendEot, respOut, respOutSize, true);
}

struct ManualReaPiProbe
{
    bool attempted{false};
    bool connected{false};
    bool versionSelected{false};
    bool deviceInfoOk{false};
    bool installationActivityOk{false};
    bool productionDataOk{false};
    bool dateTimeOk{false};
    bool networkConfigOk{false};
    bool sensorLevelsOk{false};
    bool ioInputOk{false};
    bool ioOutputOk{false};
    bool labelContentOk{false};
    bool printCounterValid{false};
    uint32_t printCounter{0};
    char dateTimeText[32]{0};
    char error[96]{0};
    char protocolLine[64]{0};
    char welcomeXml[kReaPiXmlBufSize]{0};
    char versionSelectXml[256]{0};
    char versionSelectFrame[384]{0};
    char versionSelectRxRaw[512]{0};
    char versionXml[kReaPiXmlBufSize]{0};
    char deviceInfoXml[kReaPiXmlBufSize]{0};
    char installationActivityXml[kReaPiXmlBufSize]{0};
    char productionDataXml[kReaPiXmlBufSize]{0};
    char networkConfigXml[kReaPiXmlBufSize]{0};
    char sensorLevelsXml[kReaPiXmlBufSize]{0};
    char ioInputXml[kReaPiXmlBufSize]{0};
    char ioOutputXml[kReaPiXmlBufSize]{0};
    char labelContentXml[kReaPiXmlBufSize]{0};
    char deviceInfoError[64]{0};
    char diagnosticHint[160]{0};
    char lastCommandXml[512]{0};
    float productSpeed{0.0f};
    float encoder1Speed{0.0f};
    float encoder2Speed{0.0f};
    bool productSpeedValid{false};
    bool encoder1SpeedValid{false};
    bool encoder2SpeedValid{false};
    bool triggerLevel[4]{false, false, false, false};
    bool triggerLevelValid{false};
    int printSpeedErrorCode{0};
    bool printSpeedErrorValid{false};
    char jobSetFileName[64]{0};
    bool jobSetFileNameValid{false};
};

static ManualReaPiProbe gManualReaPiProbe{};
static const ManualReaPiProbe kEmptyManualReaPiProbe{};

static void FdPrintJsonString(int sock, const char *text)
{
    fdprintf(sock, "\"");
    if (text)
    {
        for (const char *p = text; *p; ++p)
        {
            const unsigned char ch = static_cast<unsigned char>(*p);
            if (ch == '"' || ch == '\\')
            {
                fdprintf(sock, "\\%c", ch);
            }
            else if (ch == '\n')
            {
                fdprintf(sock, "\\n");
            }
            else if (ch == '\r')
            {
                fdprintf(sock, "\\r");
            }
            else if (ch == '\t')
            {
                fdprintf(sock, "\\t");
            }
            else if (ch < 0x20)
            {
                fdprintf(sock, "\\u%04x", static_cast<unsigned>(ch));
            }
            else
            {
                fdprintf(sock, "%c", ch);
            }
        }
    }
    fdprintf(sock, "\"");
}

static void FdPrintJsonStringLimited(int sock, const char *text, size_t maxChars)
{
    fdprintf(sock, "\"");
    if (text)
    {
        size_t emitted = 0;
        for (const char *p = text; *p && emitted < maxChars; ++p, ++emitted)
        {
            const unsigned char ch = static_cast<unsigned char>(*p);
            if (ch == '"' || ch == '\\')
            {
                fdprintf(sock, "\\%c", ch);
            }
            else if (ch == '\n')
            {
                fdprintf(sock, "\\n");
            }
            else if (ch == '\r')
            {
                fdprintf(sock, "\\r");
            }
            else if (ch == '\t')
            {
                fdprintf(sock, "\\t");
            }
            else if (ch < 0x20)
            {
                fdprintf(sock, "\\u%04x", static_cast<unsigned>(ch));
            }
            else
            {
                fdprintf(sock, "%c", ch);
            }
        }
        if (text[emitted])
        {
            fdprintf(sock, "...[truncated]");
        }
    }
    fdprintf(sock, "\"");
}

static bool ReaPiBuildCommandXmlForVersion(const char *version,
                                           uint32_t cmdId,
                                           const char *commandName,
                                           int commandStyle,
                                           bool withXmlDecl,
                                           char *out,
                                           size_t outSize)
{
    return ReaPiBuildCommandXmlVariant(version, cmdId, commandName, commandStyle, withXmlDecl, out, outSize);
}

static bool ReaPiSendCommandOnFd(int fd, const char *version, uint32_t &nextCmdId, const char *commandName, char *respOut, size_t respOutSize, uint32_t waitTicks)
{
    char xml[512]{0};
    if (!ReaPiBuildCommandXmlForVersion(version,
                                        nextCmdId++,
                                        commandName,
                                        1,   // require <Data/> per manual
                                        true,
                                        xml,
                                        sizeof(xml)))
    {
        return false;
    }
    CopyString(gManualReaPiProbe.lastCommandXml, sizeof(gManualReaPiProbe.lastCommandXml), xml);
    if (!ReaPiSendFramedXml(fd, xml))
    {
        return false;
    }
    if (ReaPiRecvFramedXml(fd, respOut, respOutSize, waitTicks))
    {
        return true;
    }
    return false;
}

static bool ReaPiSubscribeEventOnFd(int fd,
                                    const char *version,
                                    uint32_t &nextCmdId,
                                    const char *eventName,
                                    char *respOut,
                                    size_t respOutSize,
                                    uint32_t waitTicks)
{
    if (fd < 0 || !version || !eventName)
    {
        return false;
    }
    char xml[512]{0};
    const uint32_t cmdId = nextCmdId++;
    if (sniprintf(xml,
                  sizeof(xml),
                  "<?xml version=\"1.0\"?>\n"
                  "<REA-JET><REA-PI version=\"%s\"><Command name=\"SUBSCRIBE\" id=\"%u\">"
                  "<Data><EVENT>%s</EVENT></Data></Command></REA-PI></REA-JET>",
                  version,
                  static_cast<unsigned>(cmdId),
                  eventName) <= 0)
    {
        return false;
    }
    CopyString(gManualReaPiProbe.lastCommandXml, sizeof(gManualReaPiProbe.lastCommandXml), xml);
    if (!ReaPiSendFramedXml(fd, xml))
    {
        return false;
    }
    return ReaPiRecvFramedXml(fd, respOut, respOutSize, waitTicks);
}

static void TryParseJobFileNameFromXml(const char *xml, char *nameOut, size_t nameOutSize, bool &validOut)
{
    if (validOut || !xml || !xml[0] || !nameOut || nameOutSize == 0)
    {
        return;
    }
    char name[64]{0};
    if (FindXmlTagContent(xml, "Filename", name, sizeof(name)) ||
        FindXmlTagContent(xml, "JobFileName", name, sizeof(name)) ||
        FindXmlTagContent(xml, "FileName", name, sizeof(name)) ||
        FindXmlTagContent(xml, "JobName", name, sizeof(name)))
    {
        CopyString(nameOut, nameOutSize, name);
        validOut = true;
    }
}

static void ParseManualReaPiXml(const char *xml, ManualReaPiProbe &probe)
{
    if (!xml || !xml[0])
    {
        return;
    }
    float speed = 0.0f;
    if (ParseXmlFloatTag(xml, "ProductSpeed", speed) || ParseXmlFloatTag(xml, "Speed", speed))
    {
        probe.productSpeed = speed;
        probe.productSpeedValid = true;
    }
    if (ParseShaftEncoderSpeed(xml, 0, speed))
    {
        probe.encoder1Speed = speed;
        probe.encoder1SpeedValid = true;
        probe.productSpeed = speed;
        probe.productSpeedValid = true;
    }
    if (ParseShaftEncoderSpeed(xml, 1, speed))
    {
        probe.encoder2Speed = speed;
        probe.encoder2SpeedValid = true;
    }
    if (ParseXmlFloatTag(xml, "ShaftEncoderSpeed", speed))
    {
        probe.encoder1Speed = speed;
        probe.encoder1SpeedValid = true;
    }
    if (ParseXmlFloatTag(xml, "ShaftEncoder2Speed", speed))
    {
        probe.encoder2Speed = speed;
        probe.encoder2SpeedValid = true;
    }
    static const char *kTrigTags[4] = {"Trigger1Level", "Trigger2Level", "Trigger3Level", "Trigger4Level"};
    for (int i = 0; i < 4; ++i)
    {
        bool level = false;
        if (ParseXmlBoolishTag(xml, kTrigTags[i], level) || ParseTriggerLevel(xml, i, level))
        {
            probe.triggerLevel[i] = level;
            probe.triggerLevelValid = true;
        }
    }
    char codeText[16]{0};
    if (FindXmlTagContent(xml, "Code", codeText, sizeof(codeText)))
    {
        probe.printSpeedErrorCode = atoi(codeText);
        probe.printSpeedErrorValid = true;
    }
    TryParseJobFileNameFromXml(xml, probe.jobSetFileName, sizeof(probe.jobSetFileName), probe.jobSetFileNameValid);
}

static bool ProbeReaPiManualOneVersion(IPADDR4 targetIp, uint16_t port, const char *version, ManualReaPiProbe &probe)
{
    probe.attempted = true;
    if (targetIp.IsNull() || port < 1)
    {
        CopyString(probe.error, sizeof(probe.error), "Invalid REA-PI target");
        return false;
    }
    const int fd = connect(targetIp, port, TICKS_PER_SECOND * 8);
    if (fd < 0)
    {
        CopyString(probe.error, sizeof(probe.error), "REA-PI TCP connect failed");
        return false;
    }
    probe.connected = true;

    char protocolLine[40]{0};
    if (!ReaPiRecvProtocolLine(fd, protocolLine, sizeof(protocolLine), TICKS_PER_SECOND * 8))
    {
        close(fd);
        CopyString(probe.error, sizeof(probe.error), "REA-PI protocol banner read failed");
        return false;
    }
    CopyString(probe.protocolLine, sizeof(probe.protocolLine), protocolLine);
    if (!SendAllFd(fd, kReaPiBanner, kReaPiBannerLen))
    {
        close(fd);
        CopyString(probe.error, sizeof(probe.error), "REA-PI banner echo failed");
        return false;
    }
    if (!ReaPiRecvFramedXml(fd, probe.welcomeXml, sizeof(probe.welcomeXml), TICKS_PER_SECOND * 8))
    {
        close(fd);
        CopyString(probe.error, sizeof(probe.error), "REA-PI Welcome read failed");
        return false;
    }

    {
        char versionSelect[256]{0};
        sniprintf(versionSelect,
                  sizeof(versionSelect),
                  "<REA-JET><REA-PI><VersionSelect><Version>%s</Version></VersionSelect></REA-PI></REA-JET>",
                  version);
        CopyString(probe.versionSelectXml, sizeof(probe.versionSelectXml), versionSelect);
        sniprintf(probe.versionSelectFrame,
                  sizeof(probe.versionSelectFrame),
                  "Content-Length: %u\\n\\n%s",
                  static_cast<unsigned>(strlen(versionSelect)),
                  versionSelect);
        if (!ReaPiSendFramedXml(fd, versionSelect))
        {
            close(fd);
            CopyString(probe.error, sizeof(probe.error), "REA-PI VersionSelect send failed");
            return false;
        }
        if (!ReaPiRecvFramedXml(fd, probe.versionXml, sizeof(probe.versionXml), TICKS_PER_SECOND * 2))
        {
            CopyString(probe.error, sizeof(probe.error), "REA-PI VersionSelection missing; continuing");
        }
        CopyString(probe.versionSelectRxRaw, sizeof(probe.versionSelectRxRaw), gReaPiLastRxRaw);
        probe.versionSelected = (probe.versionXml[0] != '\0');
    }

    uint32_t nextCmdId = 1;
    // Match the working Python client timing; this printer can be slow to answer REA-PI commands.
    probe.deviceInfoOk = ReaPiSendCommandOnFd(fd, version, nextCmdId, "GETDEVICEINFO", probe.deviceInfoXml, sizeof(probe.deviceInfoXml), TICKS_PER_SECOND * 8);
    if (probe.deviceInfoOk)
    {
        ParseManualReaPiXml(probe.deviceInfoXml, probe);
    }
    else
    {
        CopyString(probe.deviceInfoError, sizeof(probe.deviceInfoError), "No framed response");
    }
    probe.installationActivityXml[0] = '\0';
    if (ReaPiSubscribeEventOnFd(fd,
                                version,
                                nextCmdId,
                                "INSTALLATIONACTIVITY",
                                probe.installationActivityXml,
                                sizeof(probe.installationActivityXml),
                                TICKS_PER_SECOND * 8))
    {
        // The first XML is the SUBSCRIBE response. The speed arrives as a following event.
        probe.installationActivityXml[0] = '\0';
        if (ReaPiRecvFramedXml(fd, probe.installationActivityXml, sizeof(probe.installationActivityXml), TICKS_PER_SECOND * 8))
        {
            probe.installationActivityOk = true;
            ParseManualReaPiXml(probe.installationActivityXml, probe);
        }
    }

    if (!probe.jobSetFileNameValid)
    {
        char jobSetXml[kReaPiXmlBufSize]{0};
        if (ReaPiSubscribeEventOnFd(fd,
                                    version,
                                    nextCmdId,
                                    "JOBSET",
                                    jobSetXml,
                                    sizeof(jobSetXml),
                                    TICKS_PER_SECOND * 4))
        {
            jobSetXml[0] = '\0';
            if (ReaPiRecvFramedXml(fd, jobSetXml, sizeof(jobSetXml), TICKS_PER_SECOND * 4))
            {
                ParseManualReaPiXml(jobSetXml, probe);
            }
        }
    }

    // Additional read-only status queries verified supported on DOD2 firmware 3.96.
    probe.productionDataOk = ReaPiSendCommandOnFd(fd, version, nextCmdId, "GETPRODUCTIONDATA", probe.productionDataXml, sizeof(probe.productionDataXml), TICKS_PER_SECOND * 2);
    if (probe.productionDataOk)
    {
        char counterText[16]{0};
        if (FindXmlTagContent(probe.productionDataXml, "SystemPrintCounter", counterText, sizeof(counterText)))
        {
            probe.printCounter = static_cast<uint32_t>(strtoul(counterText, nullptr, 10));
            probe.printCounterValid = true;
        }
    }

    {
        char dateTimeXml[512]{0};
        probe.dateTimeOk = ReaPiSendCommandOnFd(fd, version, nextCmdId, "GETDATETIME", dateTimeXml, sizeof(dateTimeXml), TICKS_PER_SECOND * 2);
        if (probe.dateTimeOk)
        {
            char y[8]{0}, mo[4]{0}, d[4]{0}, h[4]{0}, mi[4]{0}, sec[4]{0};
            if (FindXmlTagContent(dateTimeXml, "Year", y, sizeof(y)) &&
                FindXmlTagContent(dateTimeXml, "Month", mo, sizeof(mo)) &&
                FindXmlTagContent(dateTimeXml, "Day", d, sizeof(d)) &&
                FindXmlTagContent(dateTimeXml, "Hour", h, sizeof(h)) &&
                FindXmlTagContent(dateTimeXml, "Minute", mi, sizeof(mi)) &&
                FindXmlTagContent(dateTimeXml, "Second", sec, sizeof(sec)))
            {
                sniprintf(probe.dateTimeText, sizeof(probe.dateTimeText), "%s-%s-%s %s:%s:%s", y, mo, d, h, mi, sec);
            }
        }
    }

    probe.networkConfigOk = ReaPiSendCommandOnFd(fd, version, nextCmdId, "GETNETWORKCONFIG", probe.networkConfigXml, sizeof(probe.networkConfigXml), TICKS_PER_SECOND * 2);
    probe.sensorLevelsOk = ReaPiSendCommandOnFd(fd, version, nextCmdId, "GETPRODUCTSENSORLEVEL", probe.sensorLevelsXml, sizeof(probe.sensorLevelsXml), TICKS_PER_SECOND * 2);
    probe.ioInputOk = ReaPiSendCommandOnFd(fd, version, nextCmdId, "GETIOINPUTLEVEL", probe.ioInputXml, sizeof(probe.ioInputXml), TICKS_PER_SECOND * 2);
    probe.ioOutputOk = ReaPiSendCommandOnFd(fd, version, nextCmdId, "GETIOOUTPUTLEVEL", probe.ioOutputXml, sizeof(probe.ioOutputXml), TICKS_PER_SECOND * 2);
    probe.labelContentOk = ReaPiSendCommandOnFd(fd, version, nextCmdId, "GETLABELCONTENT", probe.labelContentXml, sizeof(probe.labelContentXml), TICKS_PER_SECOND * 2);
    if (probe.labelContentOk && !probe.jobSetFileNameValid)
    {
        ParseManualReaPiXml(probe.labelContentXml, probe);
    }
    close(fd);

    if (!probe.deviceInfoOk && !probe.installationActivityOk)
    {
        CopyString(probe.error, sizeof(probe.error), "REA-PI commands failed");
        if (probe.connected && probe.welcomeXml[0])
        {
            CopyString(probe.diagnosticHint,
                       sizeof(probe.diagnosticHint),
                       "Welcome received but command replies are empty; likely printer REA-PI mode/session gate (TargetGUI/service setting).");
            CopyString(probe.deviceInfoError, sizeof(probe.deviceInfoError), "No command XML response");
        }
        return false;
    }
    CopyString(probe.error, sizeof(probe.error), "");
    return true;
}

static bool ProbeReaPiManual(IPADDR4 targetIp, uint16_t port, const char *version, ManualReaPiProbe &probe)
{
    if (ProbeReaPiManualOneVersion(targetIp, port, version, probe))
    {
        return true;
    }
    return false;
}

static void PrintReaPlcStatusJson(int sock, const ReaPlcLiveStatus &status, const char *decoded)
{
    fdprintf(sock,
             "\"reaPlc\":{\"ok\":%s,\"rawResponse\":",
             status.pollOk ? "true" : "false");
    FdPrintJsonString(sock, status.rawResponse);
    fdprintf(sock,
             ",\"decoded\":");
    FdPrintJsonString(sock, decoded);
    fdprintf(sock,
             ",\"deviceStatus\":\"%s\",\"jobStatus\":\"%s\",\"jobAssigned\":%s,\"jobReleased\":%s,"
             "\"printerActive\":%s,\"jobStateRaw\":%lu,\"statusField\":\"%s\","
             "\"statusWords\":[%u,%u,%u,%u,%u,%u,%u,%u],"
             "\"triggerLevels\":[%s,%s,%s,%s],\"triggerLevelValid\":%s}",
             status.deviceStatus,
             status.jobStatus,
             status.jobAssigned ? "true" : "false",
             status.jobReleased ? "true" : "false",
             status.printerActive ? "true" : "false",
             static_cast<unsigned long>(status.jobStateRaw),
             status.statusField,
             static_cast<unsigned>(status.statusWords[0]),
             static_cast<unsigned>(status.statusWords[1]),
             static_cast<unsigned>(status.statusWords[2]),
             static_cast<unsigned>(status.statusWords[3]),
             static_cast<unsigned>(status.statusWords[4]),
             static_cast<unsigned>(status.statusWords[5]),
             static_cast<unsigned>(status.statusWords[6]),
             static_cast<unsigned>(status.statusWords[7]),
             status.triggerLevel[0] ? "true" : "false",
             status.triggerLevel[1] ? "true" : "false",
             status.triggerLevel[2] ? "true" : "false",
             status.triggerLevel[3] ? "true" : "false",
             status.triggerLevelValid ? "true" : "false");
}

static void PrintManualReaPiJson(int sock, const ManualReaPiProbe &probe)
{
    fdprintf(sock,
             "\"reaPi\":{\"attempted\":%s,\"connected\":%s,\"versionSelected\":%s,"
             "\"deviceInfoOk\":%s,\"installationActivityOk\":%s,\"error\":",
             probe.attempted ? "true" : "false",
             probe.connected ? "true" : "false",
             probe.versionSelected ? "true" : "false",
             probe.deviceInfoOk ? "true" : "false",
             probe.installationActivityOk ? "true" : "false");
    FdPrintJsonString(sock, probe.error);
    fdprintf(sock, ",\"protocolLine\":");
    FdPrintJsonString(sock, probe.protocolLine);
    fdprintf(sock, ",\"deviceInfoError\":");
    FdPrintJsonString(sock, probe.deviceInfoError);
    fdprintf(sock, ",\"diagnosticHint\":");
    FdPrintJsonString(sock, probe.diagnosticHint);
    fdprintf(sock, ",\"lastCommandXml\":");
    FdPrintJsonString(sock, probe.lastCommandXml);
    fdprintf(sock,
             ",\"productSpeedValid\":%s,\"productSpeedMpm\":%.3f,"
             "\"encoder1SpeedValid\":%s,\"encoder1SpeedMpm\":%.3f,"
             "\"encoder2SpeedValid\":%s,\"encoder2SpeedMpm\":%.3f,"
             "\"triggerLevels\":[%s,%s,%s,%s],\"triggerLevelValid\":%s,"
             "\"printSpeedErrorValid\":%s,\"printSpeedErrorCode\":%d,"
             "\"jobSetFileNameValid\":%s,\"jobSetFileName\":",
             probe.productSpeedValid ? "true" : "false",
             static_cast<double>(probe.productSpeed),
             probe.encoder1SpeedValid ? "true" : "false",
             static_cast<double>(probe.encoder1Speed),
             probe.encoder2SpeedValid ? "true" : "false",
             static_cast<double>(probe.encoder2Speed),
             probe.triggerLevel[0] ? "true" : "false",
             probe.triggerLevel[1] ? "true" : "false",
             probe.triggerLevel[2] ? "true" : "false",
             probe.triggerLevel[3] ? "true" : "false",
             probe.triggerLevelValid ? "true" : "false",
             probe.printSpeedErrorValid ? "true" : "false",
             probe.printSpeedErrorCode,
             probe.jobSetFileNameValid ? "true" : "false");
    FdPrintJsonString(sock, probe.jobSetFileName);
    fdprintf(sock, ",\"welcomeXml\":");
    FdPrintJsonStringLimited(sock, probe.welcomeXml, 512);
    fdprintf(sock, ",\"versionSelectXml\":");
    FdPrintJsonString(sock, probe.versionSelectXml);
    fdprintf(sock, ",\"versionSelectFrame\":");
    FdPrintJsonString(sock, probe.versionSelectFrame);
    fdprintf(sock, ",\"versionSelectRxRaw\":");
    FdPrintJsonStringLimited(sock, probe.versionSelectRxRaw, 512);
    fdprintf(sock, ",\"versionXml\":");
    FdPrintJsonStringLimited(sock, probe.versionXml, 512);
    fdprintf(sock, ",\"deviceInfoXml\":");
    FdPrintJsonStringLimited(sock, probe.deviceInfoXml, 512);
    fdprintf(sock, ",\"installationActivityXml\":");
    FdPrintJsonStringLimited(sock, probe.installationActivityXml, 512);
    fdprintf(sock,
             ",\"productionDataOk\":%s,\"printCounterValid\":%s,\"printCounter\":%lu,"
             "\"dateTimeOk\":%s,\"dateTimeText\":",
             probe.productionDataOk ? "true" : "false",
             probe.printCounterValid ? "true" : "false",
             static_cast<unsigned long>(probe.printCounter),
             probe.dateTimeOk ? "true" : "false");
    FdPrintJsonString(sock, probe.dateTimeText);
    fdprintf(sock,
             ",\"networkConfigOk\":%s,\"sensorLevelsOk\":%s,\"ioInputOk\":%s,\"ioOutputOk\":%s,\"labelContentOk\":%s",
             probe.networkConfigOk ? "true" : "false",
             probe.sensorLevelsOk ? "true" : "false",
             probe.ioInputOk ? "true" : "false",
             probe.ioOutputOk ? "true" : "false",
             probe.labelContentOk ? "true" : "false");
    fdprintf(sock, ",\"productionDataXml\":");
    FdPrintJsonStringLimited(sock, probe.productionDataXml, 512);
    fdprintf(sock, ",\"networkConfigXml\":");
    FdPrintJsonStringLimited(sock, probe.networkConfigXml, 512);
    fdprintf(sock, ",\"sensorLevelsXml\":");
    FdPrintJsonStringLimited(sock, probe.sensorLevelsXml, 512);
    fdprintf(sock, ",\"ioInputXml\":");
    FdPrintJsonStringLimited(sock, probe.ioInputXml, 768);
    fdprintf(sock, ",\"ioOutputXml\":");
    FdPrintJsonStringLimited(sock, probe.ioOutputXml, 512);
    fdprintf(sock, ",\"labelContentXml\":");
    FdPrintJsonStringLimited(sock, probe.labelContentXml, 768);
    fdprintf(sock, "}");
}

static int HandleReajetManualStatusApi(int sock, HTTP_Request &req)
{
    char ipText[64]{0};
    char plcPortText[24]{0};
    char piPortText[24]{0};
    char piEnabledText[16]{0};
    char versionText[24]{0};
    if (!GetQueryParam(req.pURL, "ip", ipText, sizeof(ipText)) || !ipText[0])
    {
        fdprintf(sock, "HTTP/1.0 400 Bad Request\r\nPragma: no-cache\r\nContent-Type: application/json\r\n\r\n");
        fdprintf(sock, "{\"ok\":false,\"error\":\"missing_ip\"}");
        return 1;
    }

    IPADDR4 targetIp{};
    if (IsLoopbackIpText(ipText) || !ParseIpv4Text(ipText, targetIp))
    {
        fdprintf(sock, "HTTP/1.0 400 Bad Request\r\nPragma: no-cache\r\nContent-Type: application/json\r\n\r\n");
        fdprintf(sock, "{\"ok\":false,\"error\":\"invalid_ip\"}");
        return 1;
    }
    if (!IsNetworkLinkReady())
    {
        fdprintf(sock, "HTTP/1.0 503 Service Unavailable\r\nPragma: no-cache\r\nContent-Type: application/json\r\n\r\n");
        fdprintf(sock, "{\"ok\":false,\"error\":\"network_not_ready\"}");
        return 1;
    }

    uint16_t plcPort = 22170;
    if (GetQueryParam(req.pURL, "plcPort", plcPortText, sizeof(plcPortText)) && plcPortText[0])
    {
        const int p = atoi(plcPortText);
        if (p < 1 || p > 65535)
        {
            fdprintf(sock, "HTTP/1.0 400 Bad Request\r\nPragma: no-cache\r\nContent-Type: application/json\r\n\r\n");
            fdprintf(sock, "{\"ok\":false,\"error\":\"invalid_plc_port\"}");
            return 1;
        }
        plcPort = static_cast<uint16_t>(p);
    }

    uint16_t piPort = 22171;
    if (GetQueryParam(req.pURL, "piPort", piPortText, sizeof(piPortText)) && piPortText[0])
    {
        const int p = atoi(piPortText);
        if (p < 1 || p > 65535)
        {
            fdprintf(sock, "HTTP/1.0 400 Bad Request\r\nPragma: no-cache\r\nContent-Type: application/json\r\n\r\n");
            fdprintf(sock, "{\"ok\":false,\"error\":\"invalid_pi_port\"}");
            return 1;
        }
        piPort = static_cast<uint16_t>(p);
    }

    CopyString(versionText, sizeof(versionText), static_cast<NBString>(gReaPiVersion).c_str());
    GetQueryParam(req.pURL, "version", versionText, sizeof(versionText));
    if (!versionText[0])
    {
        CopyString(versionText, sizeof(versionText), "2.0");
    }
    const bool piEnabled = !(GetQueryParam(req.pURL, "reaPi", piEnabledText, sizeof(piEnabledText)) &&
                             (strcmp(piEnabledText, "0") == 0 || StrIStartsWith(piEnabledText, "false")));

    gManualReaProbeActive = true;
    ReaPlcLiveStatus manualStatus{};
    char frame[32]{0};
    sniprintf(frame, sizeof(frame), "0008%08lX000000", static_cast<unsigned long>(gReaStatusRequestId++));
    char resp[96]{0};
    const bool useEot = (plcPort == 22169u);
    const bool plcTxOk = SendReajetAsciiTo(targetIp, plcPort, frame, useEot, resp, sizeof(resp), false);
    if (plcTxOk)
    {
        ParseReaPlcStatusResponse(resp, manualStatus);
    }
    char decoded[512]{0};
    DecodeReaResponseHuman(plcTxOk ? resp : "<send_failed>", decoded, sizeof(decoded), nullptr);

    const ManualReaPiProbe *piProbe = &kEmptyManualReaPiProbe;
    if (piEnabled)
    {
        gManualReaPiProbe = kEmptyManualReaPiProbe;
        ProbeReaPiManual(targetIp, piPort, versionText, gManualReaPiProbe);
        piProbe = &gManualReaPiProbe;
        if (piProbe->productSpeedValid)
        {
            manualStatus.clockSpeedMpm = piProbe->productSpeed;
            manualStatus.clockSpeedValid = true;
        }
        if (piProbe->encoder1SpeedValid)
        {
            manualStatus.clockSpeedMpm = piProbe->encoder1Speed;
            manualStatus.clockSpeedValid = true;
        }
        if (piProbe->encoder2SpeedValid)
        {
            manualStatus.clockSpeed2Mpm = piProbe->encoder2Speed;
            manualStatus.clockSpeed2Valid = true;
        }
        if (piProbe->triggerLevelValid)
        {
            for (int i = 0; i < 4; ++i)
            {
                manualStatus.triggerLevel[i] = piProbe->triggerLevel[i];
            }
            manualStatus.triggerLevelValid = true;
        }
        if (piProbe->printSpeedErrorValid)
        {
            manualStatus.printSpeedErrorCode = piProbe->printSpeedErrorCode;
            manualStatus.printSpeedErrorValid = true;
        }
        if (piProbe->jobSetFileNameValid)
        {
            CopyString(manualStatus.jobSetFileName, sizeof(manualStatus.jobSetFileName), piProbe->jobSetFileName);
            manualStatus.jobSetFileNameValid = true;
        }
    }

    if (!manualStatus.jobSetFileNameValid && gReaLiveStatus.jobSetFileNameValid)
    {
        char configuredIpText[64]{0};
        CopyReajetTargetIpText(configuredIpText, sizeof(configuredIpText));
        IPADDR4 configuredIp{};
        if (configuredIpText[0] && ParseIpv4Text(configuredIpText, configuredIp) &&
            configuredIp == targetIp)
        {
            CopyString(manualStatus.jobSetFileName, sizeof(manualStatus.jobSetFileName), gReaLiveStatus.jobSetFileName);
            manualStatus.jobSetFileNameValid = true;
        }
    }

    fdprintf(sock, "HTTP/1.0 200 OK\r\nPragma: no-cache\r\nContent-Type: application/json\r\n\r\n");
    fdprintf(sock,
             "{\"ok\":%s,\"ip\":\"%hI\",\"plcPort\":%u,\"piPort\":%u,\"reaPiEnabled\":%s,\"reaPiVersion\":",
             (manualStatus.pollOk || piProbe->deviceInfoOk || piProbe->installationActivityOk) ? "true" : "false",
             targetIp,
             static_cast<unsigned>(plcPort),
             static_cast<unsigned>(piPort),
             piEnabled ? "true" : "false");
    FdPrintJsonString(sock, versionText);
    fdprintf(sock,
             ",\"clockSpeedValid\":%s,\"clockSpeedMpm\":%.3f,"
             "\"clockSpeed2Valid\":%s,\"clockSpeed2Mpm\":%.3f,"
             "\"printSpeedErrorValid\":%s,\"printSpeedErrorCode\":%d,"
             "\"jobSetFileNameValid\":%s,\"jobSetFileName\":",
             manualStatus.clockSpeedValid ? "true" : "false",
             static_cast<double>(manualStatus.clockSpeedMpm),
             manualStatus.clockSpeed2Valid ? "true" : "false",
             static_cast<double>(manualStatus.clockSpeed2Mpm),
             manualStatus.printSpeedErrorValid ? "true" : "false",
             manualStatus.printSpeedErrorCode,
             manualStatus.jobSetFileNameValid ? "true" : "false");
    FdPrintJsonString(sock, manualStatus.jobSetFileName);
    fdprintf(sock, ",");
    PrintReaPlcStatusJson(sock, manualStatus, decoded);
    fdprintf(sock, ",");
    PrintManualReaPiJson(sock, *piProbe);
    fdprintf(sock, "}");
    gManualReaProbeActive = false;
    return 1;
}

static uint16_t LookupConfiguredTagTypeCode(const char *path)
{
    if (!path || !path[0])
    {
        return 0;
    }

    for (int j = 0; j < gBrowsedTagCount; ++j)
    {
        if (strcmp(gBrowsedTags[j].name, path) == 0)
        {
            return static_cast<uint16_t>(gBrowsedTags[j].symbolType & 0x00FF);
        }
    }

    const char *leftBracket = strchr(path, '[');
    if (leftBracket && leftBracket != path)
    {
        char baseName[kMaxTagPathLen]{0};
        size_t baseLen = static_cast<size_t>(leftBracket - path);
        if (baseLen >= sizeof(baseName))
        {
            baseLen = sizeof(baseName) - 1;
        }
        memcpy(baseName, path, baseLen);
        baseName[baseLen] = '\0';

        for (int j = 0; j < gBrowsedTagCount; ++j)
        {
            if (strcmp(gBrowsedTags[j].name, baseName) == 0)
            {
                return static_cast<uint16_t>(gBrowsedTags[j].symbolType & 0x00FF);
            }
        }
    }

    uint8_t data[16]{0};
    size_t dataLen = 0;
    uint16_t readType = 0;
    char readErr[64]{0};
    if (ReadPlcTagRaw(path, readType, data, sizeof(data), dataLen, readErr, sizeof(readErr)))
    {
        return readType;
    }
    return 0;
}

#endif // BURNERGATEWAY_MAIN_TU

