/*
 * CIP Data Gateway
 *
 * Copyright (c) 2026 Adam G. Sweeney
 *
 * SPDX-License-Identifier: MIT
 * See the LICENSE file in the repository root for full license text.
 */

#ifdef BURNERGATEWAY_MAIN_TU
static int HandlePlcScanApi(int sock, HTTP_Request &req)
{
    (void)req;

    const int count = ScanEnipListIdentity();
    fdprintf(sock, "HTTP/1.0 200 OK\r\nPragma: no-cache\r\nContent-Type: application/json\r\n\r\n");
    fdprintf(sock, "{\"count\":%d,\"devices\":[", count);
    for (int i = 0; i < count; ++i)
    {
        const DiscoveredEnipDevice &d = gLastScanDevices[i];
        if (i > 0)
        {
            fdprintf(sock, ",");
        }
        fdprintf(sock,
                 "{\"ip\":\"%hI\",\"name\":\"%s\",\"vendorId\":%u,\"deviceType\":%u,\"productCode\":%u,\"revision\":\"%u.%u\",\"serial\":%lu,\"micro800\":%s}",
                 d.ip,
                 d.productName[0] ? d.productName : "unknown",
                 d.vendorId,
                 d.deviceType,
                 d.productCode,
                 d.majorRev,
                 d.minorRev,
                 static_cast<unsigned long>(d.serial),
                 d.micro800 ? "true" : "false");
    }
    fdprintf(sock, "]}");
    return 1;
}

static int ParseSaveIndex(const char *url)
{
    if (!url)
    {
        return -1;
    }
    while (*url == '/')
    {
        ++url;
    }

    const char *prefix = "api/plc/save/";
    const size_t prefixLen = strlen(prefix);
    if (strncmp(url, prefix, prefixLen) != 0)
    {
        return -1;
    }
    const char *cursor = url + prefixLen;
    if (!*cursor)
    {
        return -1;
    }

    int value = 0;
    while (*cursor >= '0' && *cursor <= '9')
    {
        value = (value * 10) + (*cursor - '0');
        ++cursor;
    }
    if (*cursor != '\0' && *cursor != '?' && *cursor != '#')
    {
        return -1;
    }
    return value;
}

static bool MatchPlcSaveApi(HTTP_Request &req)
{
    if (req.req != tGet || !req.pURL)
    {
        return false;
    }

    const char *url = req.pURL;
    while (*url == '/')
    {
        ++url;
    }
    return strncmp(url, "api/plc/save/", strlen("api/plc/save/")) == 0;
}

static int HandlePlcSaveApi(int sock, HTTP_Request &req)
{
    const int idx = ParseSaveIndex(req.pURL);
    if (idx < 0 || idx >= gLastScanCount || idx >= kMaxDiscoveredDevices || !gLastScanDevices[idx].valid)
    {
        fdprintf(sock, "HTTP/1.0 400 Bad Request\r\nPragma: no-cache\r\nContent-Type: application/json\r\n\r\n");
        fdprintf(sock, "{\"ok\":false,\"error\":\"invalid_scan_index\"}");
        return 1;
    }

    const DiscoveredEnipDevice &d = gLastScanDevices[idx];
    gPlcTargetIp = d.ip;
    gPlcTargetName = d.productName;
    gPlcTargetVendorId = d.vendorId;
    gPlcTargetProductCode = d.productCode;
    gPlcTargetSerial = d.serial;
    gPlcTargetIsMicro800 = d.micro800;
    SaveConfigToStorage();
    AttemptConnectStoredPlc();

    fdprintf(sock, "HTTP/1.0 200 OK\r\nPragma: no-cache\r\nContent-Type: application/json\r\n\r\n");
    fdprintf(sock,
             "{\"ok\":true,\"ip\":\"%hI\",\"name\":\"%s\",\"micro800\":%s,\"connectOk\":%s,\"connectMessage\":\"%s\"}",
             d.ip,
             d.productName[0] ? d.productName : "unknown",
             d.micro800 ? "true" : "false",
             gPlcConnState.connected ? "true" : "false",
             gPlcConnState.message);
    return 1;
}

static int HandlePlcSelectedApi(int sock, HTTP_Request &req)
{
    (void)req;
    fdprintf(sock, "HTTP/1.0 200 OK\r\nPragma: no-cache\r\nContent-Type: application/json\r\n\r\n");
    fdprintf(sock,
             "{\"ip\":\"%hI\",\"name\":\"%s\",\"vendorId\":%u,\"productCode\":%u,\"serial\":%u,\"micro800\":%s,\"connectAttempted\":%s,\"connectOk\":%s,\"connectMessage\":\"%s\",\"lastAttemptSec\":%u}",
             static_cast<IPADDR4>(gPlcTargetIp),
             static_cast<NBString>(gPlcTargetName).c_str(),
             static_cast<uint32_t>(gPlcTargetVendorId),
             static_cast<uint32_t>(gPlcTargetProductCode),
             static_cast<uint32_t>(gPlcTargetSerial),
             static_cast<bool>(gPlcTargetIsMicro800) ? "true" : "false",
             gPlcConnState.attempted ? "true" : "false",
             gPlcConnState.connected ? "true" : "false",
             gPlcConnState.message,
             gPlcConnState.lastAttemptSec);
    return 1;
}

static int HandlePlcClearApi(int sock, HTTP_Request &req)
{
    (void)req;
    ClearStoredPlcConfig();
    fdprintf(sock, "HTTP/1.0 200 OK\r\nPragma: no-cache\r\nContent-Type: application/json\r\n\r\n");
    fdprintf(sock, "{\"ok\":true,\"ip\":\"0.0.0.0\",\"name\":\"\",\"micro800\":false}");
    return 1;
}

static int HandleReajetClearApi(int sock, HTTP_Request &req)
{
    (void)req;
    ClearStoredReajetConfig();
    fdprintf(sock, "HTTP/1.0 200 OK\r\nPragma: no-cache\r\nContent-Type: application/json\r\n\r\n");
    fdprintf(sock,
             "{\"ok\":true,\"ip\":\"\",\"port\":%u,\"pollMs\":%u,\"connectAttempted\":false,\"connectOk\":false,\"connectMessage\":\"REAJet destination cleared\"}",
             static_cast<uint32_t>(gReajetTargetPort),
             static_cast<uint32_t>(gMappingPollMs));
    return 1;
}

// Forward declarations for helpers used by REA/PLC runtime APIs.
static const char *CipTypeNameFromCode(uint16_t typeCode);
static bool ParseIpv4Text(const char *text, IPADDR4 &ipOut);
static bool IsLoopbackIpText(const char *text);
static void HexEncode(const uint8_t *data, size_t len, char *out, size_t outSize);
static bool ReadPlcTagRaw(const char *tagPath,
                          uint16_t &typeCodeOut,
                          uint8_t *dataOut,
                          size_t dataCap,
                          size_t &dataLenOut,
                          char *errorOut,
                          size_t errorOutSize);
static bool WritePlcTagRaw(const char *tagPath,
                           uint16_t typeCode,
                           const uint8_t *data,
                           size_t dataLen,
                           char *errorOut,
                           size_t errorOutSize);
static void FormatTagValueText(uint16_t typeCode, const uint8_t *data, size_t len, char *out, size_t outSize);
static void DecodeReaResponseHuman(const char *raw, char *decodedOut, size_t decodedOutSize, bool *ackOut);
static bool SendReajetAscii(const char *ascii, bool appendEot, char *respOut, size_t respOutSize);
static bool SendReajetAsciiTo(IPADDR4 targetIp, uint16_t port, const char *ascii, bool appendEot, char *respOut, size_t respOutSize, bool updateConnState);
static void RunReaStatusPollOnce();
static void MaybeRunReaStatusPoll();
static void MaybeRunReaPiService();
static void ReaPiCloseSession();
static bool ReaPiConnectSession();
static void ReaPiPumpEvents();
static void MergeReaPiIntoLiveStatus();
static bool WriteReaStatusToPlc();
static void SetRuntimeLastResponse(const char *raw);
static const char *ReaErrorCodeText(const char *errorCode);
static bool CopyAsciiField(const char *src, size_t srcLen, size_t offset, size_t width, char *out, size_t outSize);
static bool SendReajetAscii(const char *ascii, bool appendEot, char *respOut, size_t respOutSize);
static int HandleReajetManualStatusApi(int sock, HTTP_Request &req);
static bool StrIStartsWith(const char *text, const char *prefix);

static bool MatchReajetConfigSaveApi(HTTP_Request &req)
{
    if (req.req != tGet || !req.pURL)
    {
        return false;
    }
    const char *url = req.pURL;
    while (*url == '/')
    {
        ++url;
    }
    const char *prefix = "api/reajet/config/save";
    const size_t prefixLen = strlen(prefix);
    if (strncmp(url, prefix, prefixLen) != 0)
    {
        return false;
    }
    const char tail = url[prefixLen];
    return (tail == '\0' || tail == '?' || tail == '#');
}

static bool MatchReajetConfigApi(HTTP_Request &req)
{
    if (req.req != tGet || !req.pURL)
    {
        return false;
    }
    const char *url = req.pURL;
    while (*url == '/')
    {
        ++url;
    }
    const char *prefix = "api/reajet/config";
    const size_t prefixLen = strlen(prefix);
    if (strncmp(url, prefix, prefixLen) != 0)
    {
        return false;
    }
    const char tail = url[prefixLen];
    return (tail == '\0' || tail == '?' || tail == '#');
}

static int HandleReajetConfigApi(int sock, HTTP_Request &req)
{
    char probeText[16]{0};
    if (GetQueryParam(req.pURL, "probe", probeText, sizeof(probeText)) &&
        (strcmp(probeText, "1") == 0 || strcmp(probeText, "true") == 0))
    {
        AttemptConnectStoredReajet();
    }

    const uint32_t port = static_cast<uint32_t>(gReajetTargetPort);
    const bool usesEot = (port == 22169u);
    fdprintf(sock, "HTTP/1.0 200 OK\r\nPragma: no-cache\r\nContent-Type: application/json\r\n\r\n");
    fdprintf(sock,
             "{\"ok\":true,\"ip\":\"%s\",\"port\":%u,\"reaPiPort\":%u,\"reaPiEnabled\":%s,\"reaPiReconnectSec\":%u,"
             "\"pollMs\":%u,\"statusPollMs\":%u,"
             "\"statusJobAssignedTag\":\"%s\",\"statusJobReleasedTag\":\"%s\","
             "\"statusPrinterActiveTag\":\"%s\",\"statusJobStateTag\":\"%s\","
             "\"statusPollOkTag\":\"%s\",\"statusClockSpeedTag\":\"%s\",\"statusClockSpeed2Tag\":\"%s\","
             "\"statusTrig1Tag\":\"%s\",\"statusTrig2Tag\":\"%s\",\"statusTrig3Tag\":\"%s\",\"statusTrig4Tag\":\"%s\","
             "\"statusJobStartedStoppedTag\":\"%s\",\"statusPrintSpeedErrorTag\":\"%s\",\"statusReaPiOkTag\":\"%s\","
             "\"protocol\":\"REA-PLC TCP + REA-PI XML\",\"usesEot\":%s,\"connectAttempted\":%s,\"connectOk\":%s,"
             "\"connectMessage\":\"%s\",\"lastAttemptSec\":%u,"
             "\"reaPiConnected\":%s,\"reaPiSubscribed\":%s,\"reaPiLastError\":\"%s\"}",
             static_cast<NBString>(gReajetTargetIpText).c_str(),
             port,
             static_cast<uint32_t>(gReaPiPort),
             static_cast<bool>(gReaPiEnabled) ? "true" : "false",
             static_cast<uint32_t>(gReaPiReconnectSec),
             static_cast<uint32_t>(gMappingPollMs),
             static_cast<uint32_t>(gStatusPollMs),
             static_cast<NBString>(gStatusJobAssignedTag).c_str(),
             static_cast<NBString>(gStatusJobReleasedTag).c_str(),
             static_cast<NBString>(gStatusPrinterActiveTag).c_str(),
             static_cast<NBString>(gStatusJobStateTag).c_str(),
             static_cast<NBString>(gStatusPollOkTag).c_str(),
             static_cast<NBString>(gStatusClockSpeedTag).c_str(),
             static_cast<NBString>(gStatusClockSpeed2Tag).c_str(),
             static_cast<NBString>(gStatusTrig1Tag).c_str(),
             static_cast<NBString>(gStatusTrig2Tag).c_str(),
             static_cast<NBString>(gStatusTrig3Tag).c_str(),
             static_cast<NBString>(gStatusTrig4Tag).c_str(),
             static_cast<NBString>(gStatusJobStartedStoppedTag).c_str(),
             static_cast<NBString>(gStatusPrintSpeedErrorTag).c_str(),
             static_cast<NBString>(gStatusReaPiOkTag).c_str(),
             usesEot ? "true" : "false",
             gReajetConnState.attempted ? "true" : "false",
             gReajetConnState.connected ? "true" : "false",
             gReajetConnState.message,
             gReajetConnState.lastAttemptSec,
             gReaPiSession.connected ? "true" : "false",
             gReaPiSession.subscribed ? "true" : "false",
             gReaPiSession.lastError);
    return 1;
}

static int HandleReajetConfigSaveApi(int sock, HTTP_Request &req)
{
    char ipText[64]{0};
    char portText[24]{0};
    char pollText[24]{0};
    char statusPollText[24]{0};
    char tagBuf[160]{0};
    bool changed = false;
    bool bad = false;
    bool badIp = false;

    if (GetQueryParam(req.pURL, "ip", ipText, sizeof(ipText)))
    {
        if (ipText[0])
        {
            IPADDR4 parsed{};
            if (IsLoopbackIpText(ipText) || !ParseIpv4Text(ipText, parsed))
            {
                bad = true;
                badIp = true;
            }
            else
            {
                gReajetTargetIpText = ipText;
                changed = true;
            }
        }
        else
        {
            gReajetTargetIpText = "";
            changed = true;
        }
    }

    if (GetQueryParam(req.pURL, "port", portText, sizeof(portText)))
    {
        const int port = atoi(portText);
        if (port < 1 || port > 65535)
        {
            bad = true;
        }
        else
        {
            gReajetTargetPort = static_cast<uint32_t>(port);
            changed = true;
        }
    }

    if (GetQueryParam(req.pURL, "pollMs", pollText, sizeof(pollText)))
    {
        int pollMs = atoi(pollText);
        if (pollMs < 10) pollMs = 10;
        if (pollMs > 5000) pollMs = 5000;
        gMappingPollMs = static_cast<uint32_t>(pollMs);
        changed = true;
    }

    if (GetQueryParam(req.pURL, "statusPollMs", statusPollText, sizeof(statusPollText)))
    {
        int statusPollMs = atoi(statusPollText);
        if (statusPollMs < 0) statusPollMs = 0;
        if (statusPollMs > 60000) statusPollMs = 60000;
        gStatusPollMs = static_cast<uint32_t>(statusPollMs);
        gStatusLastPollMs = 0;
        changed = true;
    }

    if (GetQueryParam(req.pURL, "statusJobAssignedTag", tagBuf, sizeof(tagBuf)))
    {
        gStatusJobAssignedTag = tagBuf;
        changed = true;
    }
    if (GetQueryParam(req.pURL, "statusJobReleasedTag", tagBuf, sizeof(tagBuf)))
    {
        gStatusJobReleasedTag = tagBuf;
        changed = true;
    }
    if (GetQueryParam(req.pURL, "statusPrinterActiveTag", tagBuf, sizeof(tagBuf)))
    {
        gStatusPrinterActiveTag = tagBuf;
        changed = true;
    }
    if (GetQueryParam(req.pURL, "statusJobStateTag", tagBuf, sizeof(tagBuf)))
    {
        gStatusJobStateTag = tagBuf;
        changed = true;
    }
    if (GetQueryParam(req.pURL, "statusPollOkTag", tagBuf, sizeof(tagBuf)))
    {
        gStatusPollOkTag = tagBuf;
        changed = true;
    }
    if (GetQueryParam(req.pURL, "statusClockSpeedTag", tagBuf, sizeof(tagBuf)))
    {
        gStatusClockSpeedTag = tagBuf;
        changed = true;
    }
    if (GetQueryParam(req.pURL, "statusClockSpeed2Tag", tagBuf, sizeof(tagBuf)))
    {
        gStatusClockSpeed2Tag = tagBuf;
        changed = true;
    }
    if (GetQueryParam(req.pURL, "statusTrig1Tag", tagBuf, sizeof(tagBuf)))
    {
        gStatusTrig1Tag = tagBuf;
        changed = true;
    }
    if (GetQueryParam(req.pURL, "statusTrig2Tag", tagBuf, sizeof(tagBuf)))
    {
        gStatusTrig2Tag = tagBuf;
        changed = true;
    }
    if (GetQueryParam(req.pURL, "statusTrig3Tag", tagBuf, sizeof(tagBuf)))
    {
        gStatusTrig3Tag = tagBuf;
        changed = true;
    }
    if (GetQueryParam(req.pURL, "statusTrig4Tag", tagBuf, sizeof(tagBuf)))
    {
        gStatusTrig4Tag = tagBuf;
        changed = true;
    }
    if (GetQueryParam(req.pURL, "statusJobStartedStoppedTag", tagBuf, sizeof(tagBuf)))
    {
        gStatusJobStartedStoppedTag = tagBuf;
        changed = true;
    }
    if (GetQueryParam(req.pURL, "statusPrintSpeedErrorTag", tagBuf, sizeof(tagBuf)))
    {
        gStatusPrintSpeedErrorTag = tagBuf;
        changed = true;
    }
    if (GetQueryParam(req.pURL, "statusReaPiOkTag", tagBuf, sizeof(tagBuf)))
    {
        gStatusReaPiOkTag = tagBuf;
        changed = true;
    }

    char reaPiPortText[24]{0};
    char reaPiEnabledText[16]{0};
    char reaPiReconnectText[24]{0};
    if (GetQueryParam(req.pURL, "reaPiPort", reaPiPortText, sizeof(reaPiPortText)))
    {
        const int reaPiPort = atoi(reaPiPortText);
        if (reaPiPort < 1 || reaPiPort > 65535)
        {
            bad = true;
        }
        else
        {
            gReaPiPort = static_cast<uint32_t>(reaPiPort);
            changed = true;
        }
    }
    if (GetQueryParam(req.pURL, "reaPiEnabled", reaPiEnabledText, sizeof(reaPiEnabledText)))
    {
        gReaPiEnabled = (strcmp(reaPiEnabledText, "1") == 0 || strcmp(reaPiEnabledText, "true") == 0);
        changed = true;
    }
    if (GetQueryParam(req.pURL, "reaPiReconnectSec", reaPiReconnectText, sizeof(reaPiReconnectText)))
    {
        int sec = atoi(reaPiReconnectText);
        if (sec < 5) sec = 5;
        if (sec > 600) sec = 600;
        gReaPiReconnectSec = static_cast<uint32_t>(sec);
        changed = true;
    }

    if (bad)
    {
        fdprintf(sock, "HTTP/1.0 400 Bad Request\r\nPragma: no-cache\r\nContent-Type: application/json\r\n\r\n");
        fdprintf(sock, "{\"ok\":false,\"error\":\"%s\"}",
                 badIp ? "invalid_reajet_ip_nonloopback_required" : "invalid_param");
        return 1;
    }

    if (changed)
    {
        SaveConfigToStorage();
    }

    char savedIp[64]{0};
    CopyReajetTargetIpText(savedIp, sizeof(savedIp));
    if (savedIp[0])
    {
        AttemptConnectStoredReajet();
    }
    else
    {
        ReaPiCloseSession();
        SetReajetConnectionState(false, false, IPADDR4{}, 0, "No REAJet IP configured");
    }
    return HandleReajetConfigApi(sock, req);
}

static bool MatchReajetStatusApi(HTTP_Request &req)
{
    if (req.req != tGet || !req.pURL) return false;
    const char *url = req.pURL;
    while (*url == '/') ++url;
    const char *prefix = "api/reajet/status";
    const size_t n = strlen(prefix);
    if (strncmp(url, prefix, n) != 0) return false;
    const char tail = url[n];
    return (tail == '\0' || tail == '?' || tail == '#');
}

static bool MatchReajetManualStatusApi(HTTP_Request &req)
{
    if (req.req != tGet || !req.pURL) return false;
    const char *url = req.pURL;
    while (*url == '/') ++url;
    const char *prefix = "api/reajet/manual_status";
    const size_t n = strlen(prefix);
    if (strncmp(url, prefix, n) != 0) return false;
    const char tail = url[n];
    return (tail == '\0' || tail == '?' || tail == '#');
}

static int HandleReajetStatusApi(int sock, HTTP_Request &req)
{
    (void)req;
    char probeText[16]{0};
    if (GetQueryParam(req.pURL, "probe", probeText, sizeof(probeText)) &&
        (strcmp(probeText, "1") == 0 || strcmp(probeText, "true") == 0) &&
        IsNetworkLinkReady())
    {
        RunReaStatusPollOnce();
        if (static_cast<bool>(gReaPiEnabled) && gReaPiSession.fd < 0)
        {
            (void)ReaPiConnectSession();
        }
        ReaPiPumpEvents();
        MergeReaPiIntoLiveStatus();
    }

    fdprintf(sock, "HTTP/1.0 200 OK\r\nPragma: no-cache\r\nContent-Type: application/json\r\n\r\n");
    fdprintf(sock,
             "{\"ok\":true,\"pollOk\":%s,\"pollCount\":%u,\"pollErrors\":%u,\"lastPollSec\":%u,"
             "\"jobStatus\":\"%s\",\"jobAssigned\":%s,\"jobReleased\":%s,\"printerActive\":%s,"
             "\"jobStateRaw\":%lu,\"deviceStatus\":\"%s\",\"statusField\":\"%s\","
             "\"statusWords\":[%u,%u,%u,%u,%u,%u,%u,%u],"
             "\"triggerLevels\":[%s,%s,%s,%s],\"triggerLevelValid\":%s,"
             "\"clockSpeedValid\":%s,\"clockSpeedMpm\":%.3f,\"clockSpeed2Valid\":%s,\"clockSpeed2Mpm\":%.3f,"
             "\"printSpeedErrorValid\":%s,\"printSpeedErrorCode\":%d,"
             "\"reaPiEnabled\":%s,\"reaPiPort\":%u,\"reaPiConnected\":%s,\"reaPiSubscribed\":%s,"
             "\"reaPiEvents\":%u,\"reaPiLastError\":\"%s\","
             "\"statusPollMs\":%u,\"statusJobAssignedTag\":\"%s\",\"statusJobReleasedTag\":\"%s\","
             "\"statusPrinterActiveTag\":\"%s\",\"statusJobStateTag\":\"%s\",\"statusPollOkTag\":\"%s\","
             "\"statusClockSpeedTag\":\"%s\",\"statusClockSpeed2Tag\":\"%s\","
             "\"statusTrig1Tag\":\"%s\",\"statusTrig2Tag\":\"%s\",\"statusTrig3Tag\":\"%s\",\"statusTrig4Tag\":\"%s\","
             "\"statusJobStartedStoppedTag\":\"%s\",\"statusPrintSpeedErrorTag\":\"%s\",\"statusReaPiOkTag\":\"%s\","
             "\"lastRawResponse\":\"%s\"",
             gReaLiveStatus.pollOk ? "true" : "false",
             gReaLiveStatus.pollCount,
             gReaLiveStatus.pollErrors,
             gReaLiveStatus.lastPollSec,
             gReaLiveStatus.jobStatus,
             gReaLiveStatus.jobAssigned ? "true" : "false",
             gReaLiveStatus.jobReleased ? "true" : "false",
             gReaLiveStatus.printerActive ? "true" : "false",
             static_cast<unsigned long>(gReaLiveStatus.jobStateRaw),
             gReaLiveStatus.deviceStatus,
             gReaLiveStatus.statusField,
             static_cast<unsigned>(gReaLiveStatus.statusWords[0]),
             static_cast<unsigned>(gReaLiveStatus.statusWords[1]),
             static_cast<unsigned>(gReaLiveStatus.statusWords[2]),
             static_cast<unsigned>(gReaLiveStatus.statusWords[3]),
             static_cast<unsigned>(gReaLiveStatus.statusWords[4]),
             static_cast<unsigned>(gReaLiveStatus.statusWords[5]),
             static_cast<unsigned>(gReaLiveStatus.statusWords[6]),
             static_cast<unsigned>(gReaLiveStatus.statusWords[7]),
             gReaLiveStatus.triggerLevel[0] ? "true" : "false",
             gReaLiveStatus.triggerLevel[1] ? "true" : "false",
             gReaLiveStatus.triggerLevel[2] ? "true" : "false",
             gReaLiveStatus.triggerLevel[3] ? "true" : "false",
             gReaLiveStatus.triggerLevelValid ? "true" : "false",
             gReaLiveStatus.clockSpeedValid ? "true" : "false",
             static_cast<double>(gReaLiveStatus.clockSpeedMpm),
             gReaLiveStatus.clockSpeed2Valid ? "true" : "false",
             static_cast<double>(gReaLiveStatus.clockSpeed2Mpm),
             gReaLiveStatus.printSpeedErrorValid ? "true" : "false",
             gReaLiveStatus.printSpeedErrorCode,
             static_cast<bool>(gReaPiEnabled) ? "true" : "false",
             static_cast<uint32_t>(gReaPiPort),
             gReaPiSession.connected ? "true" : "false",
             gReaPiSession.subscribed ? "true" : "false",
             gReaPiSession.eventsReceived,
             gReaPiSession.lastError,
             static_cast<uint32_t>(gStatusPollMs),
             static_cast<NBString>(gStatusJobAssignedTag).c_str(),
             static_cast<NBString>(gStatusJobReleasedTag).c_str(),
             static_cast<NBString>(gStatusPrinterActiveTag).c_str(),
             static_cast<NBString>(gStatusJobStateTag).c_str(),
             static_cast<NBString>(gStatusPollOkTag).c_str(),
             static_cast<NBString>(gStatusClockSpeedTag).c_str(),
             static_cast<NBString>(gStatusClockSpeed2Tag).c_str(),
             static_cast<NBString>(gStatusTrig1Tag).c_str(),
             static_cast<NBString>(gStatusTrig2Tag).c_str(),
             static_cast<NBString>(gStatusTrig3Tag).c_str(),
             static_cast<NBString>(gStatusTrig4Tag).c_str(),
             static_cast<NBString>(gStatusJobStartedStoppedTag).c_str(),
             static_cast<NBString>(gStatusPrintSpeedErrorTag).c_str(),
             static_cast<NBString>(gStatusReaPiOkTag).c_str(),
             gReaLiveStatus.rawResponse);
    fdprintf(sock,
             ",\"reaPiPrintTriggers\":%u,\"reaPiPrintStarts\":%u,\"reaPiPrintEnds\":%u,"
             "\"reaPiPrintAborted\":%u,\"reaPiMissingContent\":%u,\"reaPiPrintRejected\":%u,"
             "\"reaPiLastEvent\":\"%s\"}",
             gReaPiSession.printTriggerCount,
             gReaPiSession.printStartCount,
             gReaPiSession.printEndCount,
             gReaPiSession.printAbortedCount,
             gReaPiSession.missingContentCount,
             gReaPiSession.printRejectedCount,
             gReaPiSession.lastEventName);
    return 1;
}

static const char *NormalizeIpv4Mode(const char *mode)
{
    if (!mode || !mode[0]) return nullptr;
    if (strcmp(mode, "DHCP") == 0) return "DHCP";
    if (strcmp(mode, "Static") == 0) return "Static";
    if (strcmp(mode, "Disabled") == 0) return "Disabled";
    if (strcmp(mode, "DHCP w Fallback") == 0 || strcmp(mode, "DHCP w/ Fallback") == 0) return "DHCP w Fallback";
    return nullptr;
}

static bool IsDualEthernetModule()
{
    const uint16_t cpuId = static_cast<uint16_t>(sim1.ccm.cir >> 6);
    return (cpuId & CPUID_MCF_54417) == CPUID_MCF_54417;
}

static bool MatchNetworkConfigApi(HTTP_Request &req)
{
    if (req.req != tGet || !req.pURL) return false;
    const char *url = req.pURL;
    while (*url == '/') ++url;
    const char *prefix = "api/network/config";
    const size_t n = strlen(prefix);
    if (strncmp(url, prefix, n) != 0) return false;
    const char tail = url[n];
    return (tail == '\0' || tail == '?' || tail == '#');
}

static bool MatchNetworkConfigSaveApi(HTTP_Request &req)
{
    if (req.req != tGet || !req.pURL) return false;
    const char *url = req.pURL;
    while (*url == '/') ++url;
    const char *prefix = "api/network/config/save";
    const size_t n = strlen(prefix);
    if (strncmp(url, prefix, n) != 0) return false;
    const char tail = url[n];
    return (tail == '\0' || tail == '?' || tail == '#');
}

static bool GetNetworkQueryParam(const char *url, int ifNumber, bool allowUnprefixedFallback, const char *key,
                                 char *out, size_t outLen)
{
    char prefixed[48]{0};
    snprintf(prefixed, sizeof(prefixed), "if%d_%s", ifNumber, key);
    if (GetQueryParam(url, prefixed, out, outLen))
    {
        return true;
    }
    if (allowUnprefixedFallback && GetQueryParam(url, key, out, outLen))
    {
        return true;
    }
    return false;
}

static bool HasNetworkQueryParam(const char *url, int ifNumber, bool allowUnprefixedFallback, const char *key)
{
    char scratch[4]{0};
    return GetNetworkQueryParam(url, ifNumber, allowUnprefixedFallback, key, scratch, sizeof(scratch));
}

static void WriteInterfaceJson(int sock, int ifNumber, bool prependComma)
{
    InterfaceBlock *ifBlock = GetInterfaceBlock(ifNumber);
    if (!ifBlock)
    {
        return;
    }

    if (prependComma)
    {
        fdprintf(sock, ",");
    }

    fdprintf(sock,
             "{\"interface\":%d,\"name\":\"%s\",\"mode\":\"%s\","
             "\"staticAddr\":\"%hI\",\"staticMask\":\"%hI\",\"staticGate\":\"%hI\","
             "\"staticDNS1\":\"%hI\",\"staticDNS2\":\"%hI\","
             "\"activeAddr\":\"%hI\",\"activeMask\":\"%hI\",\"activeGate\":\"%hI\","
             "\"activeDNS1\":\"%hI\",\"activeDNS2\":\"%hI\",\"autoIPAddr\":\"%hI\"}",
             ifNumber,
             ifBlock->GetInterfaceName(),
             static_cast<NBString>(ifBlock->ip4.mode).c_str(),
             static_cast<IPADDR4>(ifBlock->ip4.addr),
             static_cast<IPADDR4>(ifBlock->ip4.mask),
             static_cast<IPADDR4>(ifBlock->ip4.gate),
             static_cast<IPADDR4>(ifBlock->ip4.dns1),
             static_cast<IPADDR4>(ifBlock->ip4.dns2),
             InterfaceIP(ifNumber),
             InterfaceMASK(ifNumber),
             InterfaceGate(ifNumber),
             InterfaceDNS(ifNumber),
             InterfaceDNS2(ifNumber),
             InterfaceAutoIP(ifNumber));
}

static bool ApplyInterfaceNetworkSave(const char *url, int ifNumber, bool allowUnprefixedFallback,
                                      InterfaceBlock *ifBlock, const char **errorCode)
{
    char modeText[32]{0};
    char ipText[32]{0};
    char maskText[32]{0};
    char gateText[32]{0};
    char dns1Text[32]{0};
    char dns2Text[32]{0};

    *errorCode = nullptr;

    if (!HasNetworkQueryParam(url, ifNumber, allowUnprefixedFallback, "mode"))
    {
        return false;
    }

    if (!GetNetworkQueryParam(url, ifNumber, allowUnprefixedFallback, "mode", modeText, sizeof(modeText)))
    {
        *errorCode = "missing_mode";
        return false;
    }

    const char *mode = NormalizeIpv4Mode(modeText);
    if (!mode)
    {
        *errorCode = "invalid_mode";
        return false;
    }

    IPADDR4 staticIp{};
    IPADDR4 staticMask{};
    IPADDR4 staticGate{};
    IPADDR4 staticDns1{};
    IPADDR4 staticDns2{};

    const bool needsStatic = (strcmp(mode, "Static") == 0 || strcmp(mode, "DHCP w Fallback") == 0);
    if (needsStatic)
    {
        if (GetNetworkQueryParam(url, ifNumber, allowUnprefixedFallback, "ip", ipText, sizeof(ipText)) && ipText[0])
        {
            if (!ParseIpv4Text(ipText, staticIp) || staticIp.IsNull())
            {
                *errorCode = "invalid_static_ip";
                return false;
            }
        }
        else
        {
            *errorCode = "missing_static_ip";
            return false;
        }

        if (GetNetworkQueryParam(url, ifNumber, allowUnprefixedFallback, "mask", maskText, sizeof(maskText)) && maskText[0])
        {
            if (!ParseIpv4Text(maskText, staticMask) || staticMask.IsNull())
            {
                *errorCode = "invalid_static_mask";
                return false;
            }
        }
        else
        {
            *errorCode = "missing_static_mask";
            return false;
        }
    }

    const bool haveGate = needsStatic &&
                          GetNetworkQueryParam(url, ifNumber, allowUnprefixedFallback, "gateway", gateText, sizeof(gateText));
    const bool haveDns1 = needsStatic &&
                          GetNetworkQueryParam(url, ifNumber, allowUnprefixedFallback, "dns1", dns1Text, sizeof(dns1Text));
    const bool haveDns2 = needsStatic &&
                          GetNetworkQueryParam(url, ifNumber, allowUnprefixedFallback, "dns2", dns2Text, sizeof(dns2Text));

    if (haveGate && gateText[0] && strcmp(gateText, "0.0.0.0") != 0 && !ParseIpv4Text(gateText, staticGate))
    {
        *errorCode = "invalid_static_gateway";
        return false;
    }
    if (haveDns1 && dns1Text[0] && strcmp(dns1Text, "0.0.0.0") != 0 && !ParseIpv4Text(dns1Text, staticDns1))
    {
        *errorCode = "invalid_static_dns1";
        return false;
    }
    if (haveDns2 && dns2Text[0] && strcmp(dns2Text, "0.0.0.0") != 0 && !ParseIpv4Text(dns2Text, staticDns2))
    {
        *errorCode = "invalid_static_dns2";
        return false;
    }

    ifBlock->ip4.mode = mode;
    if (needsStatic)
    {
        ifBlock->ip4.addr = staticIp;
        ifBlock->ip4.mask = staticMask;
        if (haveGate) ifBlock->ip4.gate = staticGate;
        if (haveDns1) ifBlock->ip4.dns1 = staticDns1;
        if (haveDns2) ifBlock->ip4.dns2 = staticDns2;
    }
    return true;
}

static bool ApplyEnet1NetworkSave(const char *url, const char **errorCode)
{
    if (!IsDualEthernetModule())
    {
        return false;
    }

    char modeText[32]{0};
    char ipText[32]{0};
    char maskText[32]{0};
    char gateText[32]{0};
    char dns1Text[32]{0};
    char dns2Text[32]{0};

    *errorCode = nullptr;

    if (!HasNetworkQueryParam(url, 2, false, "mode"))
    {
        return false;
    }

    if (!GetNetworkQueryParam(url, 2, false, "mode", modeText, sizeof(modeText)))
    {
        *errorCode = "missing_mode";
        return false;
    }

    const char *mode = NormalizeIpv4Mode(modeText);
    if (!mode)
    {
        *errorCode = "invalid_mode";
        return false;
    }

    IPADDR4 staticIp{};
    IPADDR4 staticMask{};
    IPADDR4 staticGate{};
    IPADDR4 staticDns1{};
    IPADDR4 staticDns2{};

    const bool needsStatic = (strcmp(mode, "Static") == 0 || strcmp(mode, "DHCP w Fallback") == 0);
    if (needsStatic)
    {
        if (GetNetworkQueryParam(url, 2, false, "ip", ipText, sizeof(ipText)) && ipText[0])
        {
            if (!ParseIpv4Text(ipText, staticIp) || staticIp.IsNull())
            {
                *errorCode = "invalid_static_ip";
                return false;
            }
        }
        else
        {
            *errorCode = "missing_static_ip";
            return false;
        }

        if (GetNetworkQueryParam(url, 2, false, "mask", maskText, sizeof(maskText)) && maskText[0])
        {
            if (!ParseIpv4Text(maskText, staticMask) || staticMask.IsNull())
            {
                *errorCode = "invalid_static_mask";
                return false;
            }
        }
        else
        {
            *errorCode = "missing_static_mask";
            return false;
        }
    }

    const bool haveGate = needsStatic && GetNetworkQueryParam(url, 2, false, "gateway", gateText, sizeof(gateText));
    const bool haveDns1 = needsStatic && GetNetworkQueryParam(url, 2, false, "dns1", dns1Text, sizeof(dns1Text));
    const bool haveDns2 = needsStatic && GetNetworkQueryParam(url, 2, false, "dns2", dns2Text, sizeof(dns2Text));

    if (haveGate && gateText[0] && strcmp(gateText, "0.0.0.0") != 0 && !ParseIpv4Text(gateText, staticGate))
    {
        *errorCode = "invalid_static_gateway";
        return false;
    }
    if (haveDns1 && dns1Text[0] && strcmp(dns1Text, "0.0.0.0") != 0 && !ParseIpv4Text(dns1Text, staticDns1))
    {
        *errorCode = "invalid_static_dns1";
        return false;
    }
    if (haveDns2 && dns2Text[0] && strcmp(dns2Text, "0.0.0.0") != 0 && !ParseIpv4Text(dns2Text, staticDns2))
    {
        *errorCode = "invalid_static_dns2";
        return false;
    }

    enet1.ip4.mode = mode;
    if (needsStatic)
    {
        enet1.ip4.addr = staticIp;
        enet1.ip4.mask = staticMask;
        if (haveGate) enet1.ip4.gate = staticGate;
        if (haveDns1) enet1.ip4.dns1 = staticDns1;
        if (haveDns2) enet1.ip4.dns2 = staticDns2;
    }
    return true;
}

static void WriteEnet1PreviewJson(int sock, bool prependComma)
{
    if (prependComma)
    {
        fdprintf(sock, ",");
    }

    fdprintf(sock,
             "{\"interface\":2,\"name\":\"Ethernet1\",\"mode\":\"%s\","
             "\"staticAddr\":\"%hI\",\"staticMask\":\"%hI\",\"staticGate\":\"%hI\","
             "\"staticDNS1\":\"%hI\",\"staticDNS2\":\"%hI\","
             "\"activeAddr\":\"0.0.0.0\",\"activeMask\":\"0.0.0.0\",\"activeGate\":\"0.0.0.0\","
             "\"activeDNS1\":\"0.0.0.0\",\"activeDNS2\":\"0.0.0.0\",\"autoIPAddr\":\"0.0.0.0\"}",
             static_cast<NBString>(enet1.ip4.mode).c_str(),
             static_cast<IPADDR4>(enet1.ip4.addr),
             static_cast<IPADDR4>(enet1.ip4.mask),
             static_cast<IPADDR4>(enet1.ip4.gate),
             static_cast<IPADDR4>(enet1.ip4.dns1),
             static_cast<IPADDR4>(enet1.ip4.dns2));
}

static int HandleNetworkConfigApi(int sock, HTTP_Request &req)
{
    (void)req;
    const int firstIfNumber = GetFirstInterface();
    InterfaceBlock *firstIfBlock = firstIfNumber ? GetInterfaceBlock(firstIfNumber) : nullptr;
    if (!firstIfBlock)
    {
        fdprintf(sock, "HTTP/1.0 503 Service Unavailable\r\nPragma: no-cache\r\nContent-Type: application/json\r\n\r\n");
        fdprintf(sock, "{\"ok\":false,\"error\":\"no_interface\"}");
        return 1;
    }

    const bool dualEthernetSupported = IsDualEthernetModule();
    const bool etherSwitch = dualEthernetSupported && (bool)DoEtherSwitch;
    const bool independentPorts = dualEthernetSupported && !etherSwitch;

    fdprintf(sock, "HTTP/1.0 200 OK\r\nPragma: no-cache\r\nContent-Type: application/json\r\n\r\n");
    fdprintf(sock,
             "{\"ok\":true,\"dualEthernetSupported\":%s,\"etherSwitch\":%s,\"independentPorts\":%s,\"interfaces\":[",
             dualEthernetSupported ? "true" : "false",
             etherSwitch ? "true" : "false",
             independentPorts ? "true" : "false");

    bool wroteAny = false;
    int interfaceCount = 0;
    for (int ifNumber = firstIfNumber; ifNumber; ifNumber = GetNextInterface(ifNumber))
    {
        WriteInterfaceJson(sock, ifNumber, wroteAny);
        wroteAny = true;
        ++interfaceCount;
    }

    if (dualEthernetSupported && etherSwitch && interfaceCount < 2)
    {
        WriteEnet1PreviewJson(sock, wroteAny);
    }

    fdprintf(sock,
             "],\"interface\":%d,\"name\":\"%s\",\"mode\":\"%s\","
             "\"staticAddr\":\"%hI\",\"staticMask\":\"%hI\",\"staticGate\":\"%hI\","
             "\"staticDNS1\":\"%hI\",\"staticDNS2\":\"%hI\","
             "\"activeAddr\":\"%hI\",\"activeMask\":\"%hI\",\"activeGate\":\"%hI\","
             "\"activeDNS1\":\"%hI\",\"activeDNS2\":\"%hI\",\"autoIPAddr\":\"%hI\"}",
             firstIfNumber,
             firstIfBlock->GetInterfaceName(),
             static_cast<NBString>(firstIfBlock->ip4.mode).c_str(),
             static_cast<IPADDR4>(firstIfBlock->ip4.addr),
             static_cast<IPADDR4>(firstIfBlock->ip4.mask),
             static_cast<IPADDR4>(firstIfBlock->ip4.gate),
             static_cast<IPADDR4>(firstIfBlock->ip4.dns1),
             static_cast<IPADDR4>(firstIfBlock->ip4.dns2),
             InterfaceIP(firstIfNumber),
             InterfaceMASK(firstIfNumber),
             InterfaceGate(firstIfNumber),
             InterfaceDNS(firstIfNumber),
             InterfaceDNS2(firstIfNumber),
             InterfaceAutoIP(firstIfNumber));
    return 1;
}

static int HandleNetworkConfigSaveApi(int sock, HTTP_Request &req)
{
    char rebootText[16]{0};
    char etherSwitchText[16]{0};

    const int firstIfNumber = GetFirstInterface();
    if (!firstIfNumber)
    {
        fdprintf(sock, "HTTP/1.0 503 Service Unavailable\r\nPragma: no-cache\r\nContent-Type: application/json\r\n\r\n");
        fdprintf(sock, "{\"ok\":false,\"error\":\"no_interface\"}");
        return 1;
    }

    bool etherSwitchChanged = false;
    if (GetQueryParam(req.pURL, "etherSwitch", etherSwitchText, sizeof(etherSwitchText)))
    {
        if (!IsDualEthernetModule())
        {
            fdprintf(sock, "HTTP/1.0 400 Bad Request\r\nPragma: no-cache\r\nContent-Type: application/json\r\n\r\n");
            fdprintf(sock, "{\"ok\":false,\"error\":\"ether_switch_not_supported\"}");
            return 1;
        }

        const bool enableSwitch = (strcmp(etherSwitchText, "1") == 0 || StrIStartsWith(etherSwitchText, "true"));
        const bool disableSwitch = (strcmp(etherSwitchText, "0") == 0 || StrIStartsWith(etherSwitchText, "false"));
        if (!enableSwitch && !disableSwitch)
        {
            fdprintf(sock, "HTTP/1.0 400 Bad Request\r\nPragma: no-cache\r\nContent-Type: application/json\r\n\r\n");
            fdprintf(sock, "{\"ok\":false,\"error\":\"invalid_ether_switch\"}");
            return 1;
        }

        const bool nextSwitch = enableSwitch;
        if ((bool)DoEtherSwitch != nextSwitch)
        {
            DoEtherSwitch = nextSwitch;
            etherSwitchChanged = true;
        }
    }

    bool anyInterfaceUpdated = false;
    bool secondInterfaceRegistered = false;
    for (int ifNumber = firstIfNumber; ifNumber; ifNumber = GetNextInterface(ifNumber))
    {
        InterfaceBlock *ifBlock = GetInterfaceBlock(ifNumber);
        if (!ifBlock)
        {
            continue;
        }

        if (ifNumber != firstIfNumber)
        {
            secondInterfaceRegistered = true;
        }

        const bool allowUnprefixedFallback = (ifNumber == firstIfNumber);
        const char *ifError = nullptr;
        if (ApplyInterfaceNetworkSave(req.pURL, ifNumber, allowUnprefixedFallback, ifBlock, &ifError))
        {
            anyInterfaceUpdated = true;
        }
        else if (ifError)
        {
            fdprintf(sock, "HTTP/1.0 400 Bad Request\r\nPragma: no-cache\r\nContent-Type: application/json\r\n\r\n");
            fdprintf(sock, "{\"ok\":false,\"error\":\"%s\",\"interface\":%d}", ifError, ifNumber);
            return 1;
        }
    }

    if (!secondInterfaceRegistered)
    {
        const char *enet1Error = nullptr;
        if (ApplyEnet1NetworkSave(req.pURL, &enet1Error))
        {
            anyInterfaceUpdated = true;
        }
        else if (enet1Error)
        {
            fdprintf(sock, "HTTP/1.0 400 Bad Request\r\nPragma: no-cache\r\nContent-Type: application/json\r\n\r\n");
            fdprintf(sock, "{\"ok\":false,\"error\":\"%s\",\"interface\":2}", enet1Error);
            return 1;
        }
    }

    if (!anyInterfaceUpdated && !etherSwitchChanged)
    {
        fdprintf(sock, "HTTP/1.0 400 Bad Request\r\nPragma: no-cache\r\nContent-Type: application/json\r\n\r\n");
        fdprintf(sock, "{\"ok\":false,\"error\":\"missing_mode\"}");
        return 1;
    }

    if (anyInterfaceUpdated || etherSwitchChanged)
    {
        SaveConfigToStorage();
    }

    const bool reboot = GetQueryParam(req.pURL, "reboot", rebootText, sizeof(rebootText)) &&
                        (strcmp(rebootText, "1") == 0 || StrIStartsWith(rebootText, "true"));

    fdprintf(sock, "HTTP/1.0 200 OK\r\nPragma: no-cache\r\nContent-Type: application/json\r\n\r\n");
    fdprintf(sock,
             "{\"ok\":true,\"saved\":true,\"rebooting\":%s,\"etherSwitchChanged\":%s,"
             "\"message\":\"Saved to flash. Reboot required for IPv4 or Ethernet switch changes to take effect.\"}",
             reboot ? "true" : "false",
             etherSwitchChanged ? "true" : "false");
    if (reboot)
    {
        OSTimeDly(TICKS_PER_SECOND / 2);
        ForceReboot();
    }
    return 1;
}

static int HandlePlcReadApi(int sock, HTTP_Request &req)
{
    char path[160]{0};
    if (!GetQueryParam(req.pURL, "path", path, sizeof(path)))
    {
        fdprintf(sock, "HTTP/1.0 400 Bad Request\r\nPragma: no-cache\r\nContent-Type: application/json\r\n\r\n");
        fdprintf(sock, "{\"ok\":false,\"error\":\"missing_path\"}");
        return 1;
    }
    char normalized[kMaxTagPathLen]{0};
    if (!NormalizeTagPath(path, normalized, sizeof(normalized)))
    {
        fdprintf(sock, "HTTP/1.0 400 Bad Request\r\nPragma: no-cache\r\nContent-Type: application/json\r\n\r\n");
        fdprintf(sock, "{\"ok\":false,\"error\":\"invalid_path\"}");
        return 1;
    }

    uint8_t data[512]{0};
    size_t dataLen = 0;
    uint16_t typeCode = 0;
    char error[96]{0};
    if (!ReadPlcTagRaw(normalized, typeCode, data, sizeof(data), dataLen, error, sizeof(error)))
    {
        fdprintf(sock, "HTTP/1.0 502 Bad Gateway\r\nPragma: no-cache\r\nContent-Type: application/json\r\n\r\n");
        fdprintf(sock, "{\"ok\":false,\"error\":\"%s\"}", error[0] ? error : "read_failed");
        return 1;
    }

    char valueText[256]{0};
    char rawHex[1028]{0};
    FormatTagValueText(typeCode, data, dataLen, valueText, sizeof(valueText));
    HexEncode(data, dataLen, rawHex, sizeof(rawHex));

    fdprintf(sock, "HTTP/1.0 200 OK\r\nPragma: no-cache\r\nContent-Type: application/json\r\n\r\n");
    fdprintf(sock,
             "{\"ok\":true,\"path\":\"%s\",\"typeCode\":%u,\"typeName\":\"%s\",\"valueText\":\"%s\",\"rawHex\":\"%s\",\"dataLen\":%u}",
             normalized,
             static_cast<unsigned>(typeCode),
             CipTypeNameFromCode(typeCode),
             valueText,
             rawHex,
             static_cast<unsigned>(dataLen));
    return 1;
}

static int HandleReajetSendApi(int sock, HTTP_Request &req)
{
    char ascii[900]{0};
    char eotText[8]{0};
    if (!GetQueryParam(req.pURL, "ascii", ascii, sizeof(ascii)) || !ascii[0])
    {
        fdprintf(sock, "HTTP/1.0 400 Bad Request\r\nPragma: no-cache\r\nContent-Type: application/json\r\n\r\n");
        fdprintf(sock, "{\"ok\":false,\"error\":\"missing_ascii\"}");
        return 1;
    }

    char ipText[64]{0};
    CopyReajetTargetIpText(ipText, sizeof(ipText));
    IPADDR4 targetIp{};
    if (IsLoopbackIpText(ipText) || !ParseIpv4Text(ipText, targetIp))
    {
        fdprintf(sock, "HTTP/1.0 400 Bad Request\r\nPragma: no-cache\r\nContent-Type: application/json\r\n\r\n");
        fdprintf(sock, "{\"ok\":false,\"error\":\"invalid_reajet_ip_nonloopback_required\"}");
        return 1;
    }
    const uint16_t port = static_cast<uint16_t>(static_cast<uint32_t>(gReajetTargetPort));
    const bool appendEot = (GetQueryParam(req.pURL, "eot", eotText, sizeof(eotText)) &&
                            (strcmp(eotText, "1") == 0 || strcmp(eotText, "true") == 0));

    char rxAscii[96]{0};
    const bool gotResp = SendReajetAsciiTo(targetIp, port, ascii, appendEot, rxAscii, sizeof(rxAscii), true);
    const size_t txLen = strlen(ascii) + (appendEot ? 1u : 0u);

    char decoded[512]{0};
    bool ack = false;
    if (gotResp)
    {
        DecodeReaResponseHuman(rxAscii, decoded, sizeof(decoded), &ack);
    }

    fdprintf(sock, "HTTP/1.0 200 OK\r\nPragma: no-cache\r\nContent-Type: application/json\r\n\r\n");
    fdprintf(sock,
             "{\"ok\":true,\"sentLen\":%u,\"recvOk\":%s,\"response\":\"%s\",\"responseDecoded\":\"%s\",\"responseAck\":%s}",
             static_cast<unsigned>(txLen),
             gotResp ? "true" : "false",
             gotResp ? rxAscii : "",
             gotResp ? decoded : "",
             (gotResp && ack) ? "true" : "false");
    return 1;
}

static bool MatchMappingsSaveApi(HTTP_Request &req)
{
    if (req.req != tGet || !req.pURL) return false;
    const char *url = req.pURL;
    while (*url == '/') ++url;
    const char *prefix = "api/mappings/save";
    const size_t n = strlen(prefix);
    if (strncmp(url, prefix, n) != 0) return false;
    const char tail = url[n];
    return (tail == '\0' || tail == '?' || tail == '#');
}

static bool MatchMappingsDeleteApi(HTTP_Request &req)
{
    if (req.req != tGet || !req.pURL) return false;
    const char *url = req.pURL;
    while (*url == '/') ++url;
    const char *prefix = "api/mappings/delete";
    const size_t n = strlen(prefix);
    if (strncmp(url, prefix, n) != 0) return false;
    const char tail = url[n];
    return (tail == '\0' || tail == '?' || tail == '#');
}

static bool MatchMappingsClearApi(HTTP_Request &req)
{
    if (req.req != tGet || !req.pURL) return false;
    const char *url = req.pURL;
    while (*url == '/') ++url;
    const char *prefix = "api/mappings/clear";
    const size_t n = strlen(prefix);
    if (strncmp(url, prefix, n) != 0) return false;
    const char tail = url[n];
    return (tail == '\0' || tail == '?' || tail == '#');
}

static bool MatchMappingsRuntimeApi(HTTP_Request &req)
{
    if (req.req != tGet || !req.pURL) return false;
    const char *url = req.pURL;
    while (*url == '/') ++url;
    const char *prefix = "api/mappings/runtime";
    const size_t n = strlen(prefix);
    if (strncmp(url, prefix, n) != 0) return false;
    const char tail = url[n];
    return (tail == '\0' || tail == '?' || tail == '#');
}

static int HandleMappingsListApi(int sock, HTTP_Request &req)
{
    (void)req;
    const bool runtimeRunning = IsMappingRuntimeActive();
    const int activeCount = CountActiveMappings();
    fdprintf(sock, "HTTP/1.0 200 OK\r\nPragma: no-cache\r\nContent-Type: application/json\r\n\r\n");
    fdprintf(sock, "{\"ok\":true,\"count\":%d,\"activeCount\":%d,\"pollMs\":%u,\"runtimeCycles\":%u,\"runtimeRunning\":%s,\"runtimeStoppedByUser\":%s,\"scannerState\":\"%s\",\"reaPlcProtocol\":\"REA-PLC TCP v1.54\",\"reaPlcValidAscii\":\"0x20-0x7E\",\"reaPlcMaxJobFilenameLen\":%u,\"reaPlcMaxContentChars\":%u,\"reaPlcGatewayMaxContentLen\":%u,\"lastTxMappingId\":%d,\"lastTxCmd\":\"%s\",\"lastJob\":\"%s\",\"lastText\":\"%s\",\"lastAsciiStop\":\"%s\",\"lastAsciiJob\":\"%s\",\"lastAsciiText\":\"%s\",\"lastAsciiStart\":\"%s\",\"lastAscii\":\"%s\",\"lastJobStateVerified\":\"%s\",\"lastResponse\":\"%s\",\"lastResponseDecoded\":\"%s\",\"lastResponseAck\":%s,\"lastErrorCode\":%ld,\"lastErrorText\":\"%s\",\"tags\":[",
             gMappingCount,
             activeCount,
             static_cast<uint32_t>(gMappingPollMs),
             gRuntimeCycles,
             runtimeRunning ? "true" : "false",
             gRuntimeStoppedByUser ? "true" : "false",
             MappingScannerStateText(),
             static_cast<unsigned>(kReaGatewayMaxJobFilenameLen),
             static_cast<unsigned>(kReaPlcMaxContentChars),
             static_cast<unsigned>(kReaGatewayMaxParameterBuild),
             gRuntimeLastTxMappingId,
             gRuntimeLastTxCmd,
             gRuntimeLastTxJob,
             gRuntimeLastTxText,
             gRuntimeLastTxAsciiStop,
             gRuntimeLastTxAsciiJob,
             gRuntimeLastTxAsciiText,
             gRuntimeLastTxAsciiStart,
             gRuntimeLastTxAscii,
             gRuntimeLastTxJobStateVerified,
             gRuntimeLastTxResponse,
             gRuntimeLastTxResponseDecoded,
             gRuntimeLastTxResponseAck ? "true" : "false",
             static_cast<long>(gRuntimeLastTxErrorCode),
             gRuntimeLastTxErrorText);
    for (int i = 0; i < gMappingCount; ++i)
    {
        const MappingRecord &m = gMappings[i];
        if (i > 0) fdprintf(sock, ",");
        fdprintf(sock,
                 "{\"id\":%d,\"enabled\":%s,\"jobTag\":\"%s\",\"textTag\":\"%s\",\"triggerTag\":\"%s\",\"responseTag\":\"%s\",\"errorTag\":\"%s\",\"speedTag\":\"%s\",\"destCommand\":\"%s\",\"destTarget\":\"%s\",\"destType\":\"%s\",\"jobChangeWorkflow\":%s,\"reads\":%u,\"sends\":%u,\"errors\":%u}",
                 m.id,
                 m.enabled ? "true" : "false",
                 m.jobTag,
                 m.textTag,
                 m.triggerTag,
                 m.responseTag,
                 m.errorTag,
                 m.speedTag,
                 m.destCommand,
                 m.destTarget,
                 m.destType,
                 m.jobChangeWorkflow ? "true" : "false",
                 m.readCount,
                 m.sendCount,
                 m.errorCount);
    }
    fdprintf(sock, "]}");
    return 1;
}

static int HandleMappingsSaveApi(int sock, HTTP_Request &req)
{
    char idText[24]{0};
    char jobTag[160]{0};
    char textTag[160]{0};
    char triggerTag[160]{0};
    char responseTag[160]{0};
    char errorTag[160]{0};
    char speedTag[160]{0};
    char legacySourceTag[160]{0};
    char destCommand[16]{0};
    char destTarget[256]{0};
    char destType[32]{0};
    char enabledText[16]{0};
    char jobChangeWorkflowText[16]{0};

    if (!GetQueryParam(req.pURL, "id", idText, sizeof(idText)))
    {
        fdprintf(sock, "HTTP/1.0 400 Bad Request\r\nPragma: no-cache\r\nContent-Type: application/json\r\n\r\n");
        fdprintf(sock, "{\"ok\":false,\"error\":\"missing_required\"}");
        return 1;
    }
    GetQueryParam(req.pURL, "jobTag", jobTag, sizeof(jobTag));
    GetQueryParam(req.pURL, "textTag", textTag, sizeof(textTag));
    GetQueryParam(req.pURL, "triggerTag", triggerTag, sizeof(triggerTag));
    GetQueryParam(req.pURL, "responseTag", responseTag, sizeof(responseTag));
    GetQueryParam(req.pURL, "errorTag", errorTag, sizeof(errorTag));
    GetQueryParam(req.pURL, "speedTag", speedTag, sizeof(speedTag));
    if (!textTag[0])
    {
        GetQueryParam(req.pURL, "sourceTag", legacySourceTag, sizeof(legacySourceTag));
        strncpy(textTag, legacySourceTag, sizeof(textTag) - 1);
        textTag[sizeof(textTag) - 1] = '\0';
    }
    GetQueryParam(req.pURL, "destCommand", destCommand, sizeof(destCommand));
    GetQueryParam(req.pURL, "destTarget", destTarget, sizeof(destTarget));
    GetQueryParam(req.pURL, "destType", destType, sizeof(destType));
    GetQueryParam(req.pURL, "enabled", enabledText, sizeof(enabledText));
    GetQueryParam(req.pURL, "jobChangeWorkflow", jobChangeWorkflowText, sizeof(jobChangeWorkflowText));

    if (!triggerTag[0])
    {
        fdprintf(sock, "HTTP/1.0 400 Bad Request\r\nPragma: no-cache\r\nContent-Type: application/json\r\n\r\n");
        fdprintf(sock, "{\"ok\":false,\"error\":\"missing_trigger_tag\"}");
        return 1;
    }

    char jobNorm[kMaxTagPathLen]{0};
    char textNorm[kMaxTagPathLen]{0};
    char triggerNorm[kMaxTagPathLen]{0};
    char responseNorm[kMaxTagPathLen]{0};
    char errorNorm[kMaxTagPathLen]{0};
    char speedNorm[kMaxTagPathLen]{0};
    if (jobTag[0] && !NormalizeTagPath(jobTag, jobNorm, sizeof(jobNorm)))
    {
        fdprintf(sock, "HTTP/1.0 400 Bad Request\r\nPragma: no-cache\r\nContent-Type: application/json\r\n\r\n");
        fdprintf(sock, "{\"ok\":false,\"error\":\"invalid_job_tag\"}");
        return 1;
    }
    if (textTag[0] && !NormalizeTagPath(textTag, textNorm, sizeof(textNorm)))
    {
        fdprintf(sock, "HTTP/1.0 400 Bad Request\r\nPragma: no-cache\r\nContent-Type: application/json\r\n\r\n");
        fdprintf(sock, "{\"ok\":false,\"error\":\"invalid_text_tag\"}");
        return 1;
    }
    if (!NormalizeTagPath(triggerTag, triggerNorm, sizeof(triggerNorm)))
    {
        fdprintf(sock, "HTTP/1.0 400 Bad Request\r\nPragma: no-cache\r\nContent-Type: application/json\r\n\r\n");
        fdprintf(sock, "{\"ok\":false,\"error\":\"invalid_trigger_tag\"}");
        return 1;
    }
    const char *cmdCheck = destCommand[0] ? destCommand : "0004";
    if ((strcmp(cmdCheck, "0004") == 0 || strcmp(cmdCheck, "0005") == 0) && !textNorm[0])
    {
        fdprintf(sock, "HTTP/1.0 400 Bad Request\r\nPragma: no-cache\r\nContent-Type: application/json\r\n\r\n");
        fdprintf(sock, "{\"ok\":false,\"error\":\"missing_text_tag\"}");
        return 1;
    }
    if (strcmp(cmdCheck, "0001") == 0 && !jobNorm[0] && !textNorm[0])
    {
        fdprintf(sock, "HTTP/1.0 400 Bad Request\r\nPragma: no-cache\r\nContent-Type: application/json\r\n\r\n");
        fdprintf(sock, "{\"ok\":false,\"error\":\"missing_job_or_text_tag\"}");
        return 1;
    }
    if (responseTag[0] && !NormalizeTagPath(responseTag, responseNorm, sizeof(responseNorm)))
    {
        fdprintf(sock, "HTTP/1.0 400 Bad Request\r\nPragma: no-cache\r\nContent-Type: application/json\r\n\r\n");
        fdprintf(sock, "{\"ok\":false,\"error\":\"invalid_response_tag\"}");
        return 1;
    }
    if (errorTag[0] && !NormalizeTagPath(errorTag, errorNorm, sizeof(errorNorm)))
    {
        fdprintf(sock, "HTTP/1.0 400 Bad Request\r\nPragma: no-cache\r\nContent-Type: application/json\r\n\r\n");
        fdprintf(sock, "{\"ok\":false,\"error\":\"invalid_error_tag\"}");
        return 1;
    }
    if (speedTag[0] && !NormalizeTagPath(speedTag, speedNorm, sizeof(speedNorm)))
    {
        fdprintf(sock, "HTTP/1.0 400 Bad Request\r\nPragma: no-cache\r\nContent-Type: application/json\r\n\r\n");
        fdprintf(sock, "{\"ok\":false,\"error\":\"invalid_speed_tag\"}");
        return 1;
    }
    const int id = atoi(idText);
    if (id <= 0)
    {
        fdprintf(sock, "HTTP/1.0 400 Bad Request\r\nPragma: no-cache\r\nContent-Type: application/json\r\n\r\n");
        fdprintf(sock, "{\"ok\":false,\"error\":\"invalid_id\"}");
        return 1;
    }

    int idx = FindMappingIndexById(id);
    const bool isNewMapping = (idx < 0);
    if (isNewMapping)
    {
        if (gMappingCount >= kMaxMappings)
        {
            fdprintf(sock, "HTTP/1.0 400 Bad Request\r\nPragma: no-cache\r\nContent-Type: application/json\r\n\r\n");
            fdprintf(sock, "{\"ok\":false,\"error\":\"mapping_full\"}");
            return 1;
        }
        idx = gMappingCount++;
        gMappings[idx] = MappingRecord{};
        gMappings[idx].id = id;
        gMappings[idx].enabled = true;
    }
    MappingRecord &m = gMappings[idx];
    CopyString(m.jobTag, sizeof(m.jobTag), jobNorm);
    CopyString(m.textTag, sizeof(m.textTag), textNorm);
    CopyString(m.triggerTag, sizeof(m.triggerTag), triggerNorm);
    CopyString(m.responseTag, sizeof(m.responseTag), responseNorm);
    CopyString(m.errorTag, sizeof(m.errorTag), errorNorm);
    CopyString(m.speedTag, sizeof(m.speedTag), speedNorm);
    CopyString(m.destCommand, sizeof(m.destCommand), destCommand[0] ? destCommand : "0004");
    CopyString(m.destTarget, sizeof(m.destTarget), destTarget);
    CopyString(m.destType, sizeof(m.destType), destType);
    if (enabledText[0])
    {
        m.enabled = (strcmp(enabledText, "1") == 0 || strcmp(enabledText, "true") == 0);
    }
    if (jobChangeWorkflowText[0])
    {
        m.jobChangeWorkflow = (strcmp(jobChangeWorkflowText, "1") == 0 || strcmp(jobChangeWorkflowText, "true") == 0);
    }
    else if (isNewMapping)
    {
        const char *cmdDefault = destCommand[0] ? destCommand : "0004";
        m.jobChangeWorkflow = (strcmp(cmdDefault, "0004") == 0 || strcmp(cmdDefault, "0005") == 0);
    }
    m.hasLastTriggerValue = false;
    SaveMappingsToConfig(true);

    fdprintf(sock, "HTTP/1.0 200 OK\r\nPragma: no-cache\r\nContent-Type: application/json\r\n\r\n");
    fdprintf(sock, "{\"ok\":true,\"count\":%d}", gMappingCount);
    return 1;
}

static int HandleMappingsDeleteApi(int sock, HTTP_Request &req)
{
    char idText[24]{0};
    if (!GetQueryParam(req.pURL, "id", idText, sizeof(idText)))
    {
        fdprintf(sock, "HTTP/1.0 400 Bad Request\r\nPragma: no-cache\r\nContent-Type: application/json\r\n\r\n");
        fdprintf(sock, "{\"ok\":false,\"error\":\"missing_id\"}");
        return 1;
    }
    const int id = atoi(idText);
    int idx = FindMappingIndexById(id);
    if (idx < 0)
    {
        fdprintf(sock, "HTTP/1.0 404 Not Found\r\nPragma: no-cache\r\nContent-Type: application/json\r\n\r\n");
        fdprintf(sock, "{\"ok\":false,\"error\":\"not_found\"}");
        return 1;
    }
    for (int i = idx; i < gMappingCount - 1; ++i)
    {
        gMappings[i] = gMappings[i + 1];
    }
    if (gMappingCount > 0)
    {
        --gMappingCount;
    }
    SaveMappingsToConfig(true);
    fdprintf(sock, "HTTP/1.0 200 OK\r\nPragma: no-cache\r\nContent-Type: application/json\r\n\r\n");
    fdprintf(sock, "{\"ok\":true,\"count\":%d}", gMappingCount);
    return 1;
}

static int HandleMappingsClearApi(int sock, HTTP_Request &req)
{
    (void)req;
    gMappingCount = 0;
    SaveMappingsToConfig(true);
    fdprintf(sock, "HTTP/1.0 200 OK\r\nPragma: no-cache\r\nContent-Type: application/json\r\n\r\n");
    fdprintf(sock, "{\"ok\":true,\"count\":0}");
    return 1;
}

static void ResetMappingRuntimeStats()
{
    gRuntimeCycles = 0;
    gRuntimeLastTxMappingId = 0;
    gRuntimeLastTxCmd[0] = '\0';
    gRuntimeLastTxJob[0] = '\0';
    gRuntimeLastTxText[0] = '\0';
    gRuntimeLastTxAsciiStop[0] = '\0';
    gRuntimeLastTxAsciiJob[0] = '\0';
    gRuntimeLastTxAsciiText[0] = '\0';
    gRuntimeLastTxAsciiStart[0] = '\0';
    gRuntimeLastTxAscii[0] = '\0';
    gRuntimeLastTxJobStateVerified[0] = '\0';
    SetRuntimeLastResponse("");
    for (int i = 0; i < gMappingCount; ++i)
    {
        gMappings[i].readCount = 0;
        gMappings[i].sendCount = 0;
        gMappings[i].errorCount = 0;
    }
}

static int HandleMappingsRuntimeApi(int sock, HTTP_Request &req)
{
    char action[24]{0};
    if (GetQueryParam(req.pURL, "action", action, sizeof(action)))
    {
        if (strcmp(action, "stop") == 0)
        {
            gRuntimeStoppedByUser = true;
        }
        else if (strcmp(action, "start") == 0)
        {
            gRuntimeStoppedByUser = false;
        }
        else if (strcmp(action, "reset") == 0)
        {
            ResetMappingRuntimeStats();
        }
    }
    const bool runtimeRunning = IsMappingRuntimeActive();
    fdprintf(sock, "HTTP/1.0 200 OK\r\nPragma: no-cache\r\nContent-Type: application/json\r\n\r\n");
    fdprintf(sock,
             "{\"ok\":true,\"runtimeRunning\":%s,\"runtimeStoppedByUser\":%s,\"mappingCount\":%d,\"activeCount\":%d,\"scannerState\":\"%s\",\"runtimeCycles\":%u}",
             runtimeRunning ? "true" : "false",
             gRuntimeStoppedByUser ? "true" : "false",
             gMappingCount,
             CountActiveMappings(),
             MappingScannerStateText(),
             gRuntimeCycles);
    return 1;
}

#endif // BURNERGATEWAY_MAIN_TU

