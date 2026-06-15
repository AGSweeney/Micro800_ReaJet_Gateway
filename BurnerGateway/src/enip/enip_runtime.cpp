/*
 * CIP Data Gateway
 *
 * Copyright (c) 2026 Adam G. Sweeney
 *
 * SPDX-License-Identifier: MIT
 * See the LICENSE file in the repository root for full license text.
 */

#ifdef BURNERGATEWAY_MAIN_TU
static void WriteLe16(uint8_t *p, uint16_t v)
{
    p[0] = static_cast<uint8_t>(v & 0xFF);
    p[1] = static_cast<uint8_t>((v >> 8) & 0xFF);
}

static void WriteLe32(uint8_t *p, uint32_t v)
{
    p[0] = static_cast<uint8_t>(v & 0xFF);
    p[1] = static_cast<uint8_t>((v >> 8) & 0xFF);
    p[2] = static_cast<uint8_t>((v >> 16) & 0xFF);
    p[3] = static_cast<uint8_t>((v >> 24) & 0xFF);
}

static bool SendAllFd(int fd, const uint8_t *data, size_t len)
{
    static const size_t kIoChunk = 32767;
    size_t sent = 0;
    while (sent < len)
    {
        const size_t remaining = len - sent;
        const size_t chunk = (remaining < kIoChunk) ? remaining : kIoChunk;
        const int rc = write(fd,
                             reinterpret_cast<const char *>(data + sent),
                             static_cast<int>(chunk));
        if (rc <= 0)
        {
            return false;
        }
        sent += static_cast<size_t>(rc);
    }
    return true;
}

static bool RecvExactFd(int fd, uint8_t *out, size_t len, uint32_t waitTicks)
{
    static const size_t kIoChunk = 32767;
    size_t got = 0;
    while (got < len)
    {
        fd_set readSet;
        FD_ZERO(&readSet);
        FD_SET(fd, &readSet);
        const int ready = select(fd + 1, &readSet, nullptr, nullptr, waitTicks);
        if (ready <= 0)
        {
            return false;
        }
        const size_t remaining = len - got;
        const size_t chunk = (remaining < kIoChunk) ? remaining : kIoChunk;
        const int rc = read(fd,
                            reinterpret_cast<char *>(out + got),
                            static_cast<int>(chunk));
        if (rc <= 0)
        {
            return false;
        }
        got += static_cast<size_t>(rc);
    }
    return true;
}

static bool RecvEnipFrame(int fd,
                          uint16_t &command,
                          uint16_t &payloadLen,
                          uint32_t &sessionHandle,
                          uint32_t &status,
                          uint8_t *payload,
                          size_t payloadCap)
{
    uint8_t header[24]{0};
    if (!RecvExactFd(fd, header, sizeof(header), TICKS_PER_SECOND * 2))
    {
        return false;
    }
    command = ReadLe16(header + 0);
    payloadLen = ReadLe16(header + 2);
    sessionHandle = ReadLe32(header + 4);
    status = ReadLe32(header + 8);
    if (payloadLen > payloadCap)
    {
        return false;
    }
    if (payloadLen == 0)
    {
        return true;
    }
    return RecvExactFd(fd, payload, payloadLen, TICKS_PER_SECOND * 2);
}

static bool RegisterEnipSessionFd(int fd, uint32_t &sessionHandleOut)
{
    static const uint16_t kEnipRegisterSession = 0x0065;
    uint8_t req[28]{0};
    WriteLe16(req + 0, kEnipRegisterSession);
    WriteLe16(req + 2, 4);
    req[24] = 0x01;
    req[25] = 0x00;

    if (!SendAllFd(fd, req, sizeof(req)))
    {
        return false;
    }

    uint16_t cmd = 0;
    uint16_t payloadLen = 0;
    uint32_t session = 0;
    uint32_t status = 0;
    uint8_t payload[64]{0};
    if (!RecvEnipFrame(fd, cmd, payloadLen, session, status, payload, sizeof(payload)))
    {
        return false;
    }
    if (cmd != kEnipRegisterSession || status != 0 || payloadLen < 4 || session == 0)
    {
        return false;
    }
    sessionHandleOut = session;
    return true;
}

static void UnregisterEnipSessionFd(int fd, uint32_t sessionHandle)
{
    static const uint16_t kEnipUnregisterSession = 0x0066;
    uint8_t req[24]{0};
    WriteLe16(req + 0, kEnipUnregisterSession);
    WriteLe32(req + 4, sessionHandle);
    SendAllFd(fd, req, sizeof(req));
}

static bool SendEnipRrData(int fd,
                           uint32_t sessionHandle,
                           const uint8_t *cip,
                           size_t cipLen,
                           uint8_t *rrPayload,
                           size_t rrPayloadCap,
                           size_t &rrPayloadLenOut)
{
    static const uint16_t kEnipSendRrData = 0x006F;
    static const uint16_t kCpfDataItem = 0x00B2;
    if (!cip || cipLen == 0 || cipLen > 600)
    {
        return false;
    }

    const uint16_t rrLen = static_cast<uint16_t>(16 + cipLen);
    uint8_t packet[700]{0};
    size_t o = 0;
    WriteLe16(packet + o, kEnipSendRrData); o += 2;
    WriteLe16(packet + o, rrLen); o += 2;
    WriteLe32(packet + o, sessionHandle); o += 4;
    WriteLe32(packet + o, 0); o += 4; // status
    WriteLe32(packet + o, 0); o += 4; // sender context [0..3]
    WriteLe32(packet + o, 0); o += 4; // sender context [4..7]
    WriteLe32(packet + o, 0); o += 4; // options
    WriteLe32(packet + o, 0); o += 4; // interface handle
    WriteLe16(packet + o, 1); o += 2; // timeout
    WriteLe16(packet + o, 2); o += 2; // item count
    WriteLe16(packet + o, 0x0000); o += 2; // null address item
    WriteLe16(packet + o, 0x0000); o += 2;
    WriteLe16(packet + o, kCpfDataItem); o += 2;
    WriteLe16(packet + o, static_cast<uint16_t>(cipLen)); o += 2;
    memcpy(packet + o, cip, cipLen);
    o += cipLen;

    if (!SendAllFd(fd, packet, o))
    {
        return false;
    }

    uint16_t cmd = 0;
    uint16_t payloadLen = 0;
    uint32_t session = 0;
    uint32_t status = 0;
    if (!RecvEnipFrame(fd, cmd, payloadLen, session, status, rrPayload, rrPayloadCap))
    {
        return false;
    }
    if (cmd != kEnipSendRrData || status != 0 || session != sessionHandle)
    {
        return false;
    }
    rrPayloadLenOut = payloadLen;
    return true;
}

static bool ExtractCipFromRrPayload(const uint8_t *rrPayload,
                                    size_t rrLen,
                                    const uint8_t *&cipOut,
                                    size_t &cipLenOut)
{
    if (!rrPayload || rrLen < 8)
    {
        return false;
    }
    size_t offset = 0;
    offset += 4; // interface handle
    offset += 2; // timeout
    const uint16_t itemCount = ReadLe16(rrPayload + offset);
    offset += 2;
    for (uint16_t i = 0; i < itemCount; ++i)
    {
        if (offset + 4 > rrLen)
        {
            return false;
        }
        const uint16_t itemType = ReadLe16(rrPayload + offset);
        const uint16_t itemLen = ReadLe16(rrPayload + offset + 2);
        offset += 4;
        if (offset + itemLen > rrLen)
        {
            return false;
        }
        if (itemType == 0x00B2)
        {
            cipOut = rrPayload + offset;
            cipLenOut = itemLen;
            return true;
        }
        offset += itemLen;
    }
    return false;
}

static int BrowsePlcTagsFromStoredTarget(char *errorOut, size_t errorOutSize)
{
    static const uint8_t kServiceListTags = 0x55;
    static const uint8_t kCipStatusSuccess = 0x00;
    static const uint8_t kCipStatusPartial = 0x06;
    static const uint16_t kEnipPort = 44818;
    gBrowsedTagCount = 0;

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
    const IPADDR4 targetIp = static_cast<IPADDR4>(gPlcTargetIp);
    if (targetIp.IsNull())
    {
        setErr("No PLC selected");
        return 0;
    }

    const int fd = connect(targetIp, kEnipPort, TICKS_PER_SECOND * 5);
    if (fd < 0)
    {
        setErr("TCP connect to PLC failed");
        return 0;
    }

    uint32_t sessionHandle = 0;
    if (!RegisterEnipSessionFd(fd, sessionHandle))
    {
        setErr("ENIP RegisterSession failed");
        close(fd);
        return 0;
    }

    uint16_t nextInstance = 0;
    int pageGuard = 0;
    bool done = false;
    while (!done && pageGuard < 256 && gBrowsedTagCount < kMaxBrowsedTags)
    {
        ++pageGuard;
        uint8_t path[8]{0};
        size_t pathLen = 0;
        path[pathLen++] = 0x20; // class
        path[pathLen++] = 0x6B; // Symbol object
        path[pathLen++] = 0x25; // 16-bit instance segment
        path[pathLen++] = 0x00;
        path[pathLen++] = static_cast<uint8_t>(nextInstance & 0xFF);
        path[pathLen++] = static_cast<uint8_t>((nextInstance >> 8) & 0xFF);

        uint8_t cip[64]{0};
        size_t c = 0;
        cip[c++] = kServiceListTags;
        cip[c++] = static_cast<uint8_t>(pathLen / 2);
        memcpy(cip + c, path, pathLen);
        c += pathLen;
        WriteLe16(cip + c, 4); c += 2;       // attribute count
        WriteLe16(cip + c, 0x0002); c += 2;  // symbol type
        WriteLe16(cip + c, 0x0007); c += 2;  // dimensions
        WriteLe16(cip + c, 0x0008); c += 2;  // dimensions
        WriteLe16(cip + c, 0x0001); c += 2;  // name

        uint8_t rrPayload[1200]{0};
        size_t rrLen = 0;
        if (!SendEnipRrData(fd, sessionHandle, cip, c, rrPayload, sizeof(rrPayload), rrLen))
        {
            setErr("ENIP SendRRData failed");
            break;
        }

        const uint8_t *cipResp = nullptr;
        size_t cipRespLen = 0;
        if (!ExtractCipFromRrPayload(rrPayload, rrLen, cipResp, cipRespLen) || cipRespLen < 4)
        {
            setErr("Invalid CIP payload");
            break;
        }

        const uint8_t service = cipResp[0];
        const uint8_t status = cipResp[2];
        const uint8_t addStatusWords = cipResp[3];
        const size_t payloadOffset = 4 + (static_cast<size_t>(addStatusWords) * 2);
        if (service != static_cast<uint8_t>(kServiceListTags | 0x80) || payloadOffset > cipRespLen)
        {
            setErr("Unexpected list-tags response");
            break;
        }

        const uint8_t *data = cipResp + payloadOffset;
        const size_t dataLen = cipRespLen - payloadOffset;
        size_t offset = 0;
        uint32_t lastInstanceSeen = nextInstance;
        int parsed = 0;

        while (offset + 24 <= dataLen && gBrowsedTagCount < kMaxBrowsedTags)
        {
            const uint32_t instance = ReadLe32(data + offset);
            offset += 4;
            const uint16_t symbolType = ReadLe16(data + offset);
            offset += 2;
            const uint16_t elementLength = ReadLe16(data + offset);
            offset += 2;
            const uint32_t dim0 = ReadLe32(data + offset);
            const uint32_t dim1 = ReadLe32(data + offset + 4);
            const uint32_t dim2 = ReadLe32(data + offset + 8);
            offset += 12;
            const uint16_t nameLen = ReadLe16(data + offset);
            offset += 2;
            if (nameLen == 0 || offset + nameLen > dataLen)
            {
                setErr("Tag parse failed");
                done = true;
                break;
            }

            char name[kMaxTagPathLen]{0};
            size_t n = 0;
            for (uint16_t i = 0; i < nameLen && n + 1 < sizeof(name); ++i)
            {
                char ch = static_cast<char>(data[offset + i]);
                if (IsAllowedTagChar(ch))
                {
                    name[n++] = ch;
                }
                else
                {
                    name[n++] = '_';
                }
            }
            name[n] = '\0';
            offset += nameLen;
            ++parsed;
            lastInstanceSeen = instance;

            if (name[0] == '\0')
            {
                continue;
            }

            bool exists = false;
            for (int i = 0; i < gBrowsedTagCount; ++i)
            {
                if (strcmp(gBrowsedTags[i].name, name) == 0)
                {
                    exists = true;
                    break;
                }
            }
            if (exists)
            {
                continue;
            }

            BrowsedPlcTag &tag = gBrowsedTags[gBrowsedTagCount++];
            memset(tag.name, 0, sizeof(tag.name));
            const size_t nameCopy = strlen(name);
            const size_t safeCopy = (nameCopy < (sizeof(tag.name) - 1)) ? nameCopy : (sizeof(tag.name) - 1);
            memcpy(tag.name, name, safeCopy);
            tag.symbolType = symbolType;
            tag.elementSize = elementLength;
            tag.dim0 = dim0;
            tag.dim1 = dim1;
            tag.dim2 = dim2;
            const uint64_t d0 = (dim0 == 0) ? 1 : dim0;
            const uint64_t d1 = (dim1 == 0) ? 1 : dim1;
            const uint64_t d2 = (dim2 == 0) ? 1 : dim2;
            const uint64_t total = d0 * d1 * d2;
            tag.isArray = (total > 1);
            tag.arrayLength = (total > 0xFFFFFFFFull) ? 0xFFFFFFFFu : static_cast<uint32_t>(total);
            tag.imported = false;
        }

        if (status == kCipStatusPartial && parsed > 0)
        {
            nextInstance = static_cast<uint16_t>(lastInstanceSeen + 1);
            continue;
        }
        if (status == kCipStatusSuccess && parsed > 0 && lastInstanceSeen >= nextInstance)
        {
            nextInstance = static_cast<uint16_t>(lastInstanceSeen + 1);
            continue;
        }
        if (status == kCipStatusSuccess)
        {
            done = true;
            continue;
        }

        setErr("PLC returned list-tags error");
        done = true;
    }

    UnregisterEnipSessionFd(fd, sessionHandle);
    close(fd);
    if (gBrowsedTagCount > 0)
    {
        setErr("");
    }
    return gBrowsedTagCount;
}

static bool ContainsNoCase(const char *text, const char *token)
{
    if (!text || !token || !token[0])
    {
        return false;
    }

    const size_t textLen = strlen(text);
    const size_t tokenLen = strlen(token);
    if (tokenLen > textLen)
    {
        return false;
    }

    for (size_t i = 0; i + tokenLen <= textLen; ++i)
    {
        size_t j = 0;
        for (; j < tokenLen; ++j)
        {
            const unsigned char a = static_cast<unsigned char>(text[i + j]);
            const unsigned char b = static_cast<unsigned char>(token[j]);
            if (std::tolower(a) != std::tolower(b))
            {
                break;
            }
        }
        if (j == tokenLen)
        {
            return true;
        }
    }

    return false;
}

static bool LooksLikeMicro800(uint16_t vendorId, const char *productName)
{
    // Rockwell Automation vendor ID is 0x0001.
    if (vendorId != 0x0001)
    {
        return false;
    }

    // Micro800 families are commonly identified by 2080/2085 catalog prefixes
    // (for example: 2080-L50E-24QWB for a Micro850).
    if (ContainsNoCase(productName, "2080-") ||
        ContainsNoCase(productName, "2085-"))
    {
        return true;
    }

    return ContainsNoCase(productName, "micro8") ||
           ContainsNoCase(productName, "micro800");
}

static int BuildBroadcastTargets(IPADDR4 *targets, int maxTargets)
{
    if (!targets || maxTargets <= 0)
    {
        return 0;
    }

    int count = 0;
    int ifNumber = GetFirstInterface();
    while (ifNumber && count < maxTargets)
    {
        const IPADDR4 ip = InterfaceIP(ifNumber);
        const IPADDR4 mask = InterfaceMASK(ifNumber);
        const uint32_t ipRaw = static_cast<uint32_t>(ip);
        const uint32_t maskRaw = static_cast<uint32_t>(mask);
        if (ipRaw != 0 && maskRaw != 0)
        {
            const uint32_t bcastRaw = (ipRaw & maskRaw) | (~maskRaw);
            const IPADDR4 candidate = IPV4FromConst(bcastRaw);

            bool duplicate = false;
            for (int i = 0; i < count; ++i)
            {
                if (targets[i] == candidate)
                {
                    duplicate = true;
                    break;
                }
            }
            if (!duplicate)
            {
                targets[count++] = candidate;
            }
        }
        ifNumber = GetNextInterface(ifNumber);
    }

    const IPADDR4 fallback = IPADDR4::GlobalBroadCast();
    bool hasFallback = false;
    for (int i = 0; i < count; ++i)
    {
        if (targets[i] == fallback)
        {
            hasFallback = true;
            break;
        }
    }
    if (!hasFallback && count < maxTargets)
    {
        targets[count++] = fallback;
    }

    return count;
}

static void SanitizeProductName(char *text)
{
    if (!text)
    {
        return;
    }

    for (size_t i = 0; text[i] != '\0'; ++i)
    {
        const unsigned char c = static_cast<unsigned char>(text[i]);
        if ((c >= 'a' && c <= 'z') ||
            (c >= 'A' && c <= 'Z') ||
            (c >= '0' && c <= '9') ||
            c == ' ' || c == '_' || c == '-' || c == '.' || c == '/')
        {
            continue;
        }
        text[i] = '_';
    }
}

static int ScanEnipListIdentity()
{
    static const uint16_t kEnipPort = 44818;
    static const uint16_t kLocalScanPort = 44819;
    static const uint16_t kCmdListIdentity = 0x0063;
    static const uint16_t kIdentityItemType = 0x000C;
    static const uint32_t kScanWindowTicks = TICKS_PER_SECOND * 3;
    static const uint32_t kSelectSliceTicks = TICKS_PER_SECOND;
    static const uint32_t kResendTicks = TICKS_PER_SECOND;
    static const uint32_t kQuietTicks = TICKS_PER_SECOND / 2;
    static const int kMaxTargets = 8;
    gLastScanCount = 0;
    memset(gLastScanDevices, 0, sizeof(gLastScanDevices));

    const int firstIf = GetFirstInterface();
    const IPADDR4 sendAnchorIp = (firstIf > 0) ? InterfaceIP(firstIf) : IPADDR4::GlobalBroadCast();
    const int rxSock = CreateRxUdpSocket(kLocalScanPort);
    if (rxSock < 0)
    {
        iprintf("ENIP scan: failed to create RX socket (%d)\r\n", rxSock);
        return 0;
    }

    const int txSock = CreateTxUdpSocket4(sendAnchorIp, kEnipPort, kLocalScanPort);
    if (txSock < 0)
    {
        iprintf("ENIP scan: failed to create TX socket (%d)\r\n", txSock);
        close(rxSock);
        return 0;
    }

    uint8_t request[24] = {0};
    request[0] = static_cast<uint8_t>(kCmdListIdentity & 0xFF);
    request[1] = static_cast<uint8_t>((kCmdListIdentity >> 8) & 0xFF);

    IPADDR4 targets[kMaxTargets];
    const int targetCount = BuildBroadcastTargets(targets, kMaxTargets);
    if (targetCount <= 0)
    {
        iprintf("ENIP scan: no broadcast targets available\r\n");
        close(txSock);
        close(rxSock);
        return 0;
    }

    auto sendRequests = [&]() -> bool {
        bool anySent = false;
        for (int i = 0; i < targetCount; ++i)
        {
            const int sent = sendto4(txSock, request, sizeof(request), targets[i], kEnipPort);
            if (sent == static_cast<int>(sizeof(request)))
            {
                anySent = true;
            }
            else
            {
                iprintf("ENIP scan: send failed to %hI (%d)\r\n", targets[i], sent);
            }
        }
        return anySent;
    };

    if (!sendRequests())
    {
        close(txSock);
        close(rxSock);
        return 0;
    }

    iprintf("ENIP scan: sent ListIdentity to %d target(s), waiting for responses...\r\n", targetCount);

    IPADDR4 foundIps[kMaxDiscoveredDevices];
    int foundCount = 0;
    const uint32_t scanStart = TimeTick;
    uint32_t lastSendTick = scanStart;
    uint32_t lastResponseTick = 0;

    while ((TimeTick - scanStart) < kScanWindowTicks && foundCount < kMaxDiscoveredDevices)
    {
        const uint32_t now = TimeTick;
        if ((now - lastSendTick) >= kResendTicks)
        {
            sendRequests();
            lastSendTick = now;
        }
        if (lastResponseTick != 0 && (now - lastResponseTick) >= kQuietTicks)
        {
            break;
        }

        fd_set readSet;
        FD_ZERO(&readSet);
        FD_SET(rxSock, &readSet);

        const uint32_t elapsed = now - scanStart;
        const uint32_t remaining = (elapsed < kScanWindowTicks) ? (kScanWindowTicks - elapsed) : 0;
        const uint32_t waitTicks = (remaining < kSelectSliceTicks) ? remaining : kSelectSliceTicks;
        if (waitTicks == 0)
        {
            break;
        }

        const int ready = select(rxSock + 1, &readSet, nullptr, nullptr, waitTicks);
        if (ready <= 0)
        {
            continue;
        }

        uint8_t buffer[512] = {0};
        IPADDR4 fromIp{};
        uint16_t localPort = 0;
        uint16_t remotePort = 0;
        const int received = recvfrom4(rxSock, buffer, sizeof(buffer), &fromIp, &localPort, &remotePort);
        if (received < 30)
        {
            continue;
        }
        lastResponseTick = TimeTick;

        const uint16_t cmd = ReadLe16(buffer + 0);
        if (cmd != kCmdListIdentity)
        {
            continue;
        }

        const uint16_t payloadLen = ReadLe16(buffer + 2);
        if (payloadLen == 0)
        {
            continue;
        }

        if (received < 30)
        {
            continue;
        }

        const uint16_t itemCount = ReadLe16(buffer + 24);
        if (itemCount == 0)
        {
            continue;
        }

        size_t offset = 26; // 24-byte ENIP header + 2-byte item count
        if (offset + 4 > static_cast<size_t>(received))
        {
            continue;
        }

        const uint16_t itemType = ReadLe16(buffer + offset);
        offset += 2;
        const uint16_t itemLength = ReadLe16(buffer + offset);
        offset += 2;
        if (itemType != kIdentityItemType || offset + itemLength > static_cast<size_t>(received))
        {
            continue;
        }

        bool duplicate = false;
        for (int i = 0; i < foundCount; ++i)
        {
            if (foundIps[i] == fromIp)
            {
                duplicate = true;
                break;
            }
        }
        if (duplicate)
        {
            continue;
        }

        uint16_t vendorId = 0;
        uint16_t deviceType = 0;
        uint16_t productCode = 0;
        uint8_t majorRev = 0;
        uint8_t minorRev = 0;
        uint32_t serial = 0;
        char productName[33] = {0};

        const uint8_t *itemData = buffer + offset;
        if (itemLength >= 0x18)
        {
            vendorId = ReadLe16(itemData + 0x12);
            deviceType = ReadLe16(itemData + 0x14);
            productCode = ReadLe16(itemData + 0x16);
        }
        if (itemLength >= 0x20)
        {
            majorRev = itemData[0x18];
            minorRev = itemData[0x19];
            serial = ReadLe32(itemData + 0x1C);
        }
        if (itemLength > 0x20)
        {
            const uint8_t nameLen = itemData[0x20];
            const size_t availableName = itemLength - 0x21;
            const size_t copyLen = (nameLen < availableName) ? nameLen : availableName;
            const size_t safeLen = (copyLen < sizeof(productName) - 1) ? copyLen : (sizeof(productName) - 1);
            memcpy(productName, itemData + 0x21, safeLen);
            productName[safeLen] = '\0';
        }

        foundIps[foundCount++] = fromIp;
        const bool micro800 = LooksLikeMicro800(vendorId, productName);
        SanitizeProductName(productName);

        DiscoveredEnipDevice &device = gLastScanDevices[foundCount - 1];
        device.ip = fromIp;
        device.vendorId = vendorId;
        device.deviceType = deviceType;
        device.productCode = productCode;
        device.majorRev = majorRev;
        device.minorRev = minorRev;
        device.serial = serial;
        const size_t nameLen = strlen(productName);
        const size_t copyLen = (nameLen < (sizeof(device.productName) - 1)) ? nameLen : (sizeof(device.productName) - 1);
        memcpy(device.productName, productName, copyLen);
        device.productName[copyLen] = '\0';
        device.micro800 = micro800;
        device.valid = true;

        iprintf("ENIP device %d: IP=%hI Name='%s' Vendor=0x%04X Type=0x%04X Prod=0x%04X Rev=%u.%u Serial=0x%08lX%s\r\n",
                foundCount,
                fromIp,
                productName[0] ? productName : "(unknown)",
                vendorId,
                deviceType,
                productCode,
                majorRev,
                minorRev,
                serial,
                micro800 ? " [Micro800 candidate]" : "");
    }

    gLastScanCount = foundCount;
    close(txSock);
    close(rxSock);
    return foundCount;
}

#endif // BURNERGATEWAY_MAIN_TU

