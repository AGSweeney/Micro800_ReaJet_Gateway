/*
 * CIP Data Gateway
 *
 * Copyright (c) 2026 Adam G. Sweeney
 *
 * SPDX-License-Identifier: MIT
 * See the LICENSE file in the repository root for full license text.
 */

#ifdef BURNERGATEWAY_MAIN_TU
const char *AppName = "CIP Data Gateway";
static const int kMaxDiscoveredDevices = 16;

struct DiscoveredEnipDevice
{
    IPADDR4 ip{};
    uint16_t vendorId{0};
    uint16_t deviceType{0};
    uint16_t productCode{0};
    uint8_t majorRev{0};
    uint8_t minorRev{0};
    uint32_t serial{0};
    char productName[33]{0};
    bool micro800{false};
    bool valid{false};
};

static DiscoveredEnipDevice gLastScanDevices[kMaxDiscoveredDevices];
static int gLastScanCount = 0;

// Persistent gateway configuration stored in flash via NetBurner config system.
static config_obj gGatewayCfg{appdata, "Gateway", "Gateway persistent settings"};
static config_IPADDR4 gPlcTargetIp{gGatewayCfg, "0.0.0.0", "PlcTargetIp", "Selected Micro800 IP"};
static config_string gPlcTargetName{gGatewayCfg, "", "PlcTargetName", "Selected Micro800 product"};
static config_uint gPlcTargetVendorId{gGatewayCfg, 0, "PlcTargetVendorId", "Selected PLC vendor ID"};
static config_uint gPlcTargetProductCode{gGatewayCfg, 0, "PlcTargetProductCode", "Selected PLC product code"};
static config_uint gPlcTargetSerial{gGatewayCfg, 0, "PlcTargetSerial", "Selected PLC serial"};
static config_bool gPlcTargetIsMicro800{gGatewayCfg, false, "PlcTargetIsMicro800", "Selected PLC is Micro800"};
static const uint32_t kPlcReconnectIntervalSec = 15;
static config_string gPlcTagPathsBlob{gGatewayCfg, "", "PlcTagPaths", "Newline-separated PLC tag paths"};
static config_string gImportedBrowsePathsBlob{gGatewayCfg, "", "ImportedBrowsePaths", "Newline-separated imported browse-only PLC tag paths"};
static config_string gReajetTargetIpText{gGatewayCfg, "", "ReaJetTargetIp", "REAJet target IP address"};
static config_uint gReajetTargetPort{gGatewayCfg, 22170, "ReaJetTargetPort", "REAJet REA-PLC TCP port (22169/22170)"};
static config_uint gMappingPollMs{gGatewayCfg, 50, "MappingPollMs", "Mapping runtime poll interval milliseconds"};
static config_uint gStatusPollMs{gGatewayCfg, 500, "StatusPollMs", "REA GETSTATUS poll interval ms (0=disabled)"};
static config_bool gReaPiEnabled{gGatewayCfg, true, "ReaPiEnabled", "Enable REA-PI XML status/event client on ReaPiPort"};
static config_uint gReaPiPort{gGatewayCfg, 22171, "ReaPiPort", "REAJet REA-PI XML TCP port"};
static config_uint gReaPiReconnectSec{gGatewayCfg, 30, "ReaPiReconnectSec", "REA-PI reconnect interval seconds"};
static config_string gStatusJobAssignedTag{gGatewayCfg, "", "StatusJobAssignedTag", "PLC BOOL tag: job assigned"};
static config_string gStatusJobReleasedTag{gGatewayCfg, "", "StatusJobReleasedTag", "PLC BOOL tag: job released (print enabled)"};
static config_string gStatusPrinterActiveTag{gGatewayCfg, "", "StatusPrinterActiveTag", "PLC BOOL tag: printer active"};
static config_string gStatusJobStateTag{gGatewayCfg, "", "StatusJobStateTag", "PLC DINT tag: raw job status field"};
static config_string gStatusPollOkTag{gGatewayCfg, "", "StatusPollOkTag", "PLC BOOL tag: last GETSTATUS succeeded"};
static config_string gStatusClockSpeedTag{gGatewayCfg, "", "StatusClockSpeedTag", "PLC REAL tag: encoder 1 / product speed m/min"};
static config_string gStatusClockSpeed2Tag{gGatewayCfg, "", "StatusClockSpeed2Tag", "PLC REAL tag: encoder 2 speed m/min"};
static config_string gStatusTrig1Tag{gGatewayCfg, "", "StatusTrig1Tag", "PLC BOOL tag: install activity trigger 1 level"};
static config_string gStatusTrig2Tag{gGatewayCfg, "", "StatusTrig2Tag", "PLC BOOL tag: install activity trigger 2 level"};
static config_string gStatusTrig3Tag{gGatewayCfg, "", "StatusTrig3Tag", "PLC BOOL tag: install activity trigger 3 level"};
static config_string gStatusTrig4Tag{gGatewayCfg, "", "StatusTrig4Tag", "PLC BOOL tag: install activity trigger 4 level"};
static config_string gStatusJobStartedStoppedTag{gGatewayCfg, "", "StatusJobStartedStoppedTag", "PLC BOOL tag: job started/running"};
static config_string gStatusPrintSpeedErrorTag{gGatewayCfg, "", "StatusPrintSpeedErrorTag", "PLC INT tag: print speed error code"};
static config_string gStatusReaPiOkTag{gGatewayCfg, "", "StatusReaPiOkTag", "PLC BOOL tag: REA-PI session subscribed"};
static config_string gMappingsBlob{gGatewayCfg, "", "MappingsBlob", "Serialized mapping records"};

static const int kMaxPlcTags = 128;
static const int kMaxTagPathLen = 96;
static char gPlcTags[kMaxPlcTags][kMaxTagPathLen]{};
static int gPlcTagCount = 0;
static const int kMaxBrowsedTags = 512;

struct BrowsedPlcTag
{
    char name[kMaxTagPathLen]{0};
    uint16_t symbolType{0};
    uint16_t elementSize{0};
    uint32_t dim0{0};
    uint32_t dim1{0};
    uint32_t dim2{0};
    bool isArray{false};
    uint32_t arrayLength{0};
    bool imported{false};
};

static BrowsedPlcTag gBrowsedTags[kMaxBrowsedTags];
static int gBrowsedTagCount = 0;
static const int kMaxImportedBrowseTags = 512;
static char gImportedBrowseTags[kMaxImportedBrowseTags][kMaxTagPathLen]{};
static int gImportedBrowseTagCount = 0;

static const int kMaxMappings = 64;
// REA-PLC TCP protocol limits per manual v1.54 (not RTA EtherNet/IP REA_PI_STRING_12/32).
static const size_t kReaPlcMaxContentChars = 65535u;       // §2.3.5 content length field (4 hex digits)
static const size_t kReaPlcMaxParameterChars = 0xFFFFFFu;   // §2.2 parameter length field (6 hex digits)
static const size_t kReaGatewayMaxJobFilenameLen = 255u;    // protocol: variable; gateway read buffer cap
static const size_t kReaGatewayMaxValueTextLen = 512u;      // PLC tag -> REA value buffer
static const size_t kReaGatewayParameterBufSize = 900u;     // BuildReajetAsciiFrameCmd parameter[]
static const size_t kReaGatewayMaxParameterBuild = 850u;    // headroom for id block + content in parameter[]
struct MappingRecord
{
    int id{0};
    bool enabled{true};
    char jobTag[kMaxTagPathLen]{0};
    char textTag[kMaxTagPathLen]{0};
    char responseTag[kMaxTagPathLen]{0};
    char errorTag[kMaxTagPathLen]{0};
    char speedTag[kMaxTagPathLen]{0};
    char triggerTag[kMaxTagPathLen]{0};
    char destCommand[8]{0};
    char destTarget[192]{0};
    char destType[24]{0};
    bool jobChangeWorkflow{false};
    bool hasLastTriggerValue{false};
    bool lastTriggerValue{false};
    uint32_t readCount{0};
    uint32_t sendCount{0};
    uint32_t errorCount{0};
};
static MappingRecord gMappings[kMaxMappings];
static int gMappingCount = 0;
static uint32_t gRuntimeCycles = 0;
static uint32_t gRuntimeLastRunMs = 0;
static int gRuntimeLastTxMappingId = 0;
static char gRuntimeLastTxJob[kReaGatewayMaxValueTextLen]{0};
static char gRuntimeLastTxText[kReaGatewayMaxValueTextLen]{0};
static char gRuntimeLastTxAsciiStop[1000]{0};
static char gRuntimeLastTxAsciiJob[1000]{0};
static char gRuntimeLastTxAsciiText[1000]{0};
static char gRuntimeLastTxAsciiStart[1000]{0};
static char gRuntimeLastTxAscii[1000]{0};
static char gRuntimeLastTxCmd[8]{0};
static char gRuntimeLastTxJobStateVerified[96]{0};
static char gRuntimeLastTxResponse[96]{0};
static char gRuntimeLastTxResponseDecoded[512]{0};
static bool gRuntimeLastTxResponseAck = false;
static int32_t gRuntimeLastTxErrorCode = 0;
static char gRuntimeLastTxErrorText[96]{0};
static bool gRuntimeStoppedByUser = false;

struct ReaPlcLiveStatus
{
    char rawResponse[96]{0};
    char jobStatus[9]{0};
    char deviceStatus[5]{0};
    char statusField[33]{0};
    bool jobAssigned{false};
    bool jobReleased{false};
    bool printerActive{false};
    bool pollOk{false};
    uint32_t jobStateRaw{0};
    uint16_t statusWords[8]{0};
    bool triggerLevel[4]{false, false, false, false};
    bool triggerLevelValid{false};
    uint32_t pollCount{0};
    uint32_t pollErrors{0};
    uint32_t lastPollSec{0};
    float clockSpeedMpm{0.0f};
    float clockSpeed2Mpm{0.0f};
    bool clockSpeedValid{false};
    bool clockSpeed2Valid{false};
    int printSpeedErrorCode{0};
    bool printSpeedErrorValid{false};
    char jobSetFileName[64]{0};
    bool jobSetFileNameValid{false};
};

struct ReaPiSessionState
{
    int fd{-1};
    bool versionSelected{false};
    bool subscribed{false};
    bool connected{false};
    uint32_t nextCmdId{1};
    uint32_t connectAttempts{0};
    uint32_t lastConnectSec{0};
    uint32_t lastKeepAliveSec{0};
    uint32_t eventsReceived{0};
    uint32_t commandErrors{0};
    uint32_t printTriggerCount{0};
    uint32_t printStartCount{0};
    uint32_t printEndCount{0};
    uint32_t printAbortedCount{0};
    uint32_t missingContentCount{0};
    uint32_t printRejectedCount{0};
    char lastEventName[32]{0};
    char lastError[96]{0};
    char lastEventXml[512]{0};
    float productSpeed{0.0f};
    float encoder1Speed{0.0f};
    float encoder2Speed{0.0f};
    bool productSpeedValid{false};
    bool encoder1SpeedValid{false};
    bool encoder2SpeedValid{false};
    bool triggerLevel[4]{false, false, false, false};
    bool triggerLevelValid{false};
};

static ReaPlcLiveStatus gReaLiveStatus{};
static ReaPiSessionState gReaPiSession{};
static uint32_t gStatusLastPollMs = 0;
static uint32_t gReaStatusRequestId = 1;
static const size_t kReaPiXmlBufSize = 2048;
static config_string gReaPiVersion{gGatewayCfg, "2.0", "ReaPiVersion", "REA-PI XML protocol version (e.g. 2.0 or 3.6)"};
static const uint8_t kReaPiBanner[] = {0x02, 'R', 'E', 'A', '-', 'P', 'I', 0x03, '\n'};
static const size_t kReaPiBannerLen = sizeof(kReaPiBanner);

struct PlcConnectionState
{
    bool attempted{false};
    bool connected{false};
    IPADDR4 ip{};
    uint32_t lastAttemptSec{0};
    char message[96]{0};
};

static PlcConnectionState gPlcConnState{};

struct ReajetConnectionState
{
    bool attempted{false};
    bool connected{false};
    IPADDR4 ip{};
    uint16_t port{0};
    uint32_t lastAttemptSec{0};
    char message[96]{0};
};

static ReajetConnectionState gReajetConnState{};

static bool ParseIpv4Text(const char *text, IPADDR4 &ipOut);
static bool IsLoopbackIpText(const char *text);

static void CopyString(char *dst, size_t dstSize, const char *src)
{
    if (!dst || dstSize == 0)
    {
        return;
    }
    dst[0] = '\0';
    if (!src)
    {
        return;
    }
    sniprintf(dst, dstSize, "%s", src);
}

static void CopyReajetTargetIpText(char *out, size_t outSize)
{
    if (!out || outSize == 0)
    {
        return;
    }
    out[0] = '\0';
    const NBString ip = gReajetTargetIpText;
    CopyString(out, outSize, ip.c_str());
}

static IPADDR4 GetInterfaceIpv4Address(int ifNumber)
{
    if (!ifNumber)
    {
        return IPADDR4{};
    }
    IPADDR4 ip = InterfaceIP(ifNumber);
    if (!ip.IsNull())
    {
        return ip;
    }
#ifdef AUTOIP
    ip = InterfaceAutoIP(ifNumber);
    if (!ip.IsNull())
    {
        return ip;
    }
#endif
    return IPADDR4{};
}

static bool IsNetworkLinkReady()
{
    const int ifNumber = GetFirstInterface();
    if (!ifNumber)
    {
        return false;
    }
    if (HaveActiveNetwork(ifNumber))
    {
        return true;
    }
    return !GetInterfaceIpv4Address(ifNumber).IsNull();
}

static void EnsureAutoIpEnabled()
{
    const int ifNumber = GetFirstInterface();
    if (!ifNumber)
    {
        return;
    }
    InterfaceBlock *ifBlock = GetInterfaceBlock(ifNumber);
    if (!ifBlock)
    {
        return;
    }
    if (!static_cast<bool>(ifBlock->ip4.autoip))
    {
        ifBlock->ip4.autoip = true;
        iprintf("Enabled AutoIP fallback for %s\r\n", ifBlock->GetInterfaceName());
    }
}

#ifdef AUTOIP
static void KickAutoIpIfNeeded(int ifNumber)
{
    if (!ifNumber || !InterfaceIP(ifNumber).IsNull())
    {
        return;
    }
    if (!InterfaceAutoIP(ifNumber).IsNull())
    {
        return;
    }

    InterfaceBlock *ifBlock = GetInterfaceBlock(ifNumber);
    if (!ifBlock || !static_cast<bool>(ifBlock->ip4.autoip))
    {
        return;
    }

    iprintf("No DHCP address; starting AutoIP negotiation (169.254.x.x)...\r\n");
    ifBlock->AutoClient.restart();
}
#endif

static bool WaitForNetworkWithAutoIpFallback()
{
    const int ifNumber = GetFirstInterface();
    if (!ifNumber)
    {
        return false;
    }

    EnsureAutoIpEnabled();

    if (WaitForActiveNetwork(TICKS_PER_SECOND * 30))
    {
        return true;
    }

#ifdef AUTOIP
    KickAutoIpIfNeeded(ifNumber);

    iprintf("Waiting up to 30s for AutoIP link-local address...\r\n");
    const uint32_t deadline = TimeTick + (TICKS_PER_SECOND * 30);
    while (TimeTick < deadline)
    {
        if (HaveActiveNetwork(ifNumber))
        {
            return true;
        }

        const IPADDR4 ip = GetInterfaceIpv4Address(ifNumber);
        if (!ip.IsNull())
        {
            iprintf("AutoIP ready: %hI\r\n", ip);
            return true;
        }
        OSTimeDly(TICKS_PER_SECOND / 4);
    }
#endif

    return false;
}

static void SetPlcConnectionState(bool attempted, bool connected, IPADDR4 ip, const char *message)
{
    gPlcConnState.attempted = attempted;
    gPlcConnState.connected = connected;
    gPlcConnState.ip = ip;
    gPlcConnState.lastAttemptSec = static_cast<uint32_t>(Secs);
    if (message)
    {
        const size_t len = strlen(message);
        const size_t copyLen = (len < (sizeof(gPlcConnState.message) - 1)) ? len : (sizeof(gPlcConnState.message) - 1);
        memcpy(gPlcConnState.message, message, copyLen);
        gPlcConnState.message[copyLen] = '\0';
    }
    else
    {
        gPlcConnState.message[0] = '\0';
    }
}

static void NotifyPlcCommunicationFailure(const char *message)
{
    const IPADDR4 targetIp = static_cast<IPADDR4>(gPlcTargetIp);
    if (targetIp.IsNull())
    {
        return;
    }
    SetPlcConnectionState(true, false, targetIp, message ? message : "PLC communication failed");
}

static void NotifyPlcCommunicationSuccess()
{
    const IPADDR4 targetIp = static_cast<IPADDR4>(gPlcTargetIp);
    if (targetIp.IsNull())
    {
        return;
    }
    SetPlcConnectionState(true, true, targetIp, "PLC ENIP ok");
}

static bool AttemptConnectStoredPlc()
{
    const IPADDR4 targetIp = static_cast<IPADDR4>(gPlcTargetIp);
    if (targetIp.IsNull())
    {
        SetPlcConnectionState(false, false, targetIp, "No stored PLC selected");
        return false;
    }
    if (!IsNetworkLinkReady())
    {
        SetPlcConnectionState(true, false, targetIp, "Network not ready");
        return false;
    }

    const int fd = connect(targetIp, 44818, TICKS_PER_SECOND * 3);
    if (fd < 0)
    {
        SetPlcConnectionState(true, false, targetIp, "TCP connect to 44818 failed");
        iprintf("PLC auto-connect failed: %hI\r\n", targetIp);
        return false;
    }

    close(fd);
    SetPlcConnectionState(true, true, targetIp, "TCP connect to 44818 ok");
    iprintf("PLC auto-connect ok: %hI\r\n", targetIp);
    return true;
}

static void SetReajetConnectionState(bool attempted, bool connected, IPADDR4 ip, uint16_t port, const char *message)
{
    gReajetConnState.attempted = attempted;
    gReajetConnState.connected = connected;
    gReajetConnState.ip = ip;
    gReajetConnState.port = port;
    gReajetConnState.lastAttemptSec = static_cast<uint32_t>(Secs);
    if (message)
    {
        const size_t len = strlen(message);
        const size_t copyLen = (len < (sizeof(gReajetConnState.message) - 1)) ? len : (sizeof(gReajetConnState.message) - 1);
        memcpy(gReajetConnState.message, message, copyLen);
        gReajetConnState.message[copyLen] = '\0';
    }
    else
    {
        gReajetConnState.message[0] = '\0';
    }
}

static bool AttemptConnectStoredReajet()
{
    char ipText[64]{0};
    CopyReajetTargetIpText(ipText, sizeof(ipText));
    IPADDR4 targetIp{};
    if (!ipText || !ipText[0])
    {
        SetReajetConnectionState(false, false, targetIp, 0, "No REAJet IP configured");
        return false;
    }
    if (IsLoopbackIpText(ipText) || !ParseIpv4Text(ipText, targetIp))
    {
        SetReajetConnectionState(true, false, targetIp, 0, "Invalid REAJet IP");
        return false;
    }

    const uint16_t port = static_cast<uint16_t>(static_cast<uint32_t>(gReajetTargetPort));
    if (port < 1)
    {
        SetReajetConnectionState(true, false, targetIp, port, "Invalid REAJet port");
        return false;
    }
    if (!IsNetworkLinkReady())
    {
        SetReajetConnectionState(true, false, targetIp, port, "Network not ready");
        return false;
    }

    const int fd = connect(targetIp, port, TICKS_PER_SECOND * 3);
    if (fd < 0)
    {
        char msg[96]{0};
        sniprintf(msg, sizeof(msg), "TCP connect to %u failed", static_cast<unsigned>(port));
        SetReajetConnectionState(true, false, targetIp, port, msg);
        iprintf("REAJet auto-connect failed: %hI:%u\r\n", targetIp, static_cast<unsigned>(port));
        return false;
    }

    close(fd);
    char msg[96]{0};
    sniprintf(msg, sizeof(msg), "TCP connect to %u ok", static_cast<unsigned>(port));
    SetReajetConnectionState(true, true, targetIp, port, msg);
    iprintf("REAJet auto-connect ok: %hI:%u\r\n", targetIp, static_cast<unsigned>(port));
    return true;
}

static void ClearStoredPlcConfig()
{
    gPlcTargetIp = IPADDR4{};
    gPlcTargetName = "";
    gPlcTargetVendorId = 0;
    gPlcTargetProductCode = 0;
    gPlcTargetSerial = 0;
    gPlcTargetIsMicro800 = false;
    SetPlcConnectionState(false, false, IPADDR4{}, "PLC selection cleared");
    SaveConfigToStorage();
    iprintf("PLC target cleared from config.\r\n");
}

static void ClearStoredReajetConfig();

static bool IsAllowedTagChar(char c)
{
    return (c >= 'a' && c <= 'z') ||
           (c >= 'A' && c <= 'Z') ||
           (c >= '0' && c <= '9') ||
           c == '_' || c == '.' || c == '[' || c == ']' || c == ':';
}

static bool NormalizeTagPath(const char *input, char *out, size_t outSize)
{
    if (!input || !out || outSize < 2)
    {
        return false;
    }

    while (*input == ' ' || *input == '\t')
    {
        ++input;
    }

    size_t n = 0;
    while (*input && n + 1 < outSize)
    {
        if (*input != ' ' && *input != '\t' && *input != '\r' && *input != '\n')
        {
            if (!IsAllowedTagChar(*input))
            {
                return false;
            }
            out[n++] = *input;
        }
        ++input;
    }
    out[n] = '\0';
    return n > 0;
}

static void EscapeField(const char *in, char *out, size_t outSize)
{
    if (!out || outSize < 2)
    {
        return;
    }
    out[0] = '\0';
    if (!in)
    {
        return;
    }
    size_t w = 0;
    for (size_t i = 0; in[i] != '\0' && (w + 2) < outSize; ++i)
    {
        const char ch = in[i];
        if (ch == '\\' || ch == '\t' || ch == '\n' || ch == '\r')
        {
            out[w++] = '\\';
            if (ch == '\t') out[w++] = 't';
            else if (ch == '\n') out[w++] = 'n';
            else if (ch == '\r') out[w++] = 'r';
            else out[w++] = '\\';
        }
        else
        {
            out[w++] = ch;
        }
    }
    out[w] = '\0';
}

static void UnescapeField(const char *in, char *out, size_t outSize)
{
    if (!out || outSize < 2)
    {
        return;
    }
    out[0] = '\0';
    if (!in)
    {
        return;
    }
    size_t w = 0;
    for (size_t i = 0; in[i] != '\0' && (w + 1) < outSize; ++i)
    {
        char ch = in[i];
        if (ch == '\\' && in[i + 1] != '\0')
        {
            const char n2 = in[++i];
            if (n2 == 't') ch = '\t';
            else if (n2 == 'n') ch = '\n';
            else if (n2 == 'r') ch = '\r';
            else ch = n2;
        }
        out[w++] = ch;
    }
    out[w] = '\0';
}

static int FindMappingIndexById(int id)
{
    for (int i = 0; i < gMappingCount; ++i)
    {
        if (gMappings[i].id == id)
        {
            return i;
        }
    }
    return -1;
}

static int CountActiveMappings()
{
    int active = 0;
    for (int i = 0; i < gMappingCount; ++i)
    {
        if (!gMappings[i].enabled || !gMappings[i].triggerTag[0])
        {
            continue;
        }
        const char *cmd = gMappings[i].destCommand[0] ? gMappings[i].destCommand : "0004";
        if ((strcmp(cmd, "0004") == 0 || strcmp(cmd, "0005") == 0) && !gMappings[i].textTag[0])
        {
            continue;
        }
        if (strcmp(cmd, "0001") == 0 && !gMappings[i].jobTag[0] && !gMappings[i].textTag[0])
        {
            continue;
        }
        ++active;
    }
    return active;
}

static bool IsMappingRuntimeActive()
{
    return (CountActiveMappings() > 0 && !gRuntimeStoppedByUser);
}

static const char *MappingScannerStateText()
{
    if (gMappingCount <= 0)
    {
        return "idle_no_mappings";
    }
    if (CountActiveMappings() <= 0)
    {
        return "idle_no_active_mappings";
    }
    if (gRuntimeStoppedByUser)
    {
        return "stopped";
    }
    return "running";
}

static void SaveMappingsToConfig(bool persistFlash)
{
    NBString blob;
    for (int i = 0; i < gMappingCount; ++i)
    {
        const MappingRecord &m = gMappings[i];
        char jobEsc[192]{0};
        char textEsc[192]{0};
        char triggerEsc[192]{0};
        char responseEsc[192]{0};
        char errorEsc[192]{0};
        char speedEsc[192]{0};
        char dstEsc[384]{0};
        char typeEsc[64]{0};
        EscapeField(m.jobTag, jobEsc, sizeof(jobEsc));
        EscapeField(m.textTag, textEsc, sizeof(textEsc));
        EscapeField(m.triggerTag, triggerEsc, sizeof(triggerEsc));
        EscapeField(m.responseTag, responseEsc, sizeof(responseEsc));
        EscapeField(m.errorTag, errorEsc, sizeof(errorEsc));
        EscapeField(m.speedTag, speedEsc, sizeof(speedEsc));
        EscapeField(m.destTarget, dstEsc, sizeof(dstEsc));
        EscapeField(m.destType, typeEsc, sizeof(typeEsc));
        char line[1000]{0};
        sniprintf(line,
                  sizeof(line),
                  "%d\t%d\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%d",
                  m.id,
                  m.enabled ? 1 : 0,
                  jobEsc,
                  textEsc,
                  triggerEsc,
                  responseEsc,
                  errorEsc,
                  m.destCommand,
                  dstEsc,
                  typeEsc,
                  speedEsc,
                  m.jobChangeWorkflow ? 1 : 0);
        blob += line;
        blob += "\n";
    }
    gMappingsBlob = blob;
    if (persistFlash)
    {
        SaveConfigToStorage();
    }
}

static void LoadMappingsFromConfig()
{
    gMappingCount = 0;
    const NBString blob = static_cast<NBString>(gMappingsBlob);
    const char *p = blob.c_str();
    while (*p && gMappingCount < kMaxMappings)
    {
        char line[1000]{0};
        size_t l = 0;
        while (*p && *p != '\n' && l + 1 < sizeof(line))
        {
            line[l++] = *p++;
        }
        if (*p == '\n')
        {
            ++p;
        }
        line[l] = '\0';
        if (!line[0])
        {
            continue;
        }

        char *fields[12]{};
        int fieldCount = 0;
        char *cursor = line;
        fields[fieldCount++] = cursor;
        while (*cursor && fieldCount < 12)
        {
            if (*cursor == '\t')
            {
                *cursor = '\0';
                fields[fieldCount++] = cursor + 1;
            }
            ++cursor;
        }
        if (fieldCount < 6)
        {
            continue;
        }

        MappingRecord m{};
        m.id = atoi(fields[0]);
        m.enabled = (atoi(fields[1]) != 0);
        char jobUn[192]{0};
        char textUn[192]{0};
        char triggerUn[192]{0};
        char responseUn[192]{0};
        char errorUn[192]{0};
        char speedUn[192]{0};
        char dstUn[384]{0};
        char typeUn[64]{0};
        const char *cmdField = nullptr;
        if (fieldCount >= 10)
        {
            UnescapeField(fields[2], jobUn, sizeof(jobUn));
            UnescapeField(fields[3], textUn, sizeof(textUn));
            UnescapeField(fields[4], triggerUn, sizeof(triggerUn));
            UnescapeField(fields[5], responseUn, sizeof(responseUn));
            UnescapeField(fields[6], errorUn, sizeof(errorUn));
            cmdField = fields[7];
            UnescapeField(fields[8], dstUn, sizeof(dstUn));
            UnescapeField(fields[9], typeUn, sizeof(typeUn));
            if (fieldCount >= 11)
            {
                UnescapeField(fields[10], speedUn, sizeof(speedUn));
            }
        }
        else if (fieldCount >= 9)
        {
            UnescapeField(fields[2], jobUn, sizeof(jobUn));
            UnescapeField(fields[3], textUn, sizeof(textUn));
            UnescapeField(fields[4], responseUn, sizeof(responseUn));
            UnescapeField(fields[5], errorUn, sizeof(errorUn));
            cmdField = fields[6];
            UnescapeField(fields[7], dstUn, sizeof(dstUn));
            UnescapeField(fields[8], typeUn, sizeof(typeUn));
        }
        else if (fieldCount >= 8)
        {
            UnescapeField(fields[2], jobUn, sizeof(jobUn));
            UnescapeField(fields[3], textUn, sizeof(textUn));
            UnescapeField(fields[4], responseUn, sizeof(responseUn));
            cmdField = fields[5];
            UnescapeField(fields[6], dstUn, sizeof(dstUn));
            UnescapeField(fields[7], typeUn, sizeof(typeUn));
        }
        else if (fieldCount >= 7)
        {
            UnescapeField(fields[2], jobUn, sizeof(jobUn));
            UnescapeField(fields[3], textUn, sizeof(textUn));
            cmdField = fields[4];
            UnescapeField(fields[5], dstUn, sizeof(dstUn));
            UnescapeField(fields[6], typeUn, sizeof(typeUn));
        }
        else
        {
            // Legacy blob: sourceTag was stored in field[2].
            UnescapeField(fields[2], textUn, sizeof(textUn));
            cmdField = fields[3];
            UnescapeField(fields[4], dstUn, sizeof(dstUn));
            UnescapeField(fields[5], typeUn, sizeof(typeUn));
        }
        if (jobUn[0] && !NormalizeTagPath(jobUn, m.jobTag, sizeof(m.jobTag)))
        {
            continue;
        }
        if (!textUn[0] || !NormalizeTagPath(textUn, m.textTag, sizeof(m.textTag)))
        {
            continue;
        }
        if (triggerUn[0] && !NormalizeTagPath(triggerUn, m.triggerTag, sizeof(m.triggerTag)))
        {
            continue;
        }
        if (responseUn[0] && !NormalizeTagPath(responseUn, m.responseTag, sizeof(m.responseTag)))
        {
            continue;
        }
        if (errorUn[0] && !NormalizeTagPath(errorUn, m.errorTag, sizeof(m.errorTag)))
        {
            continue;
        }
        if (speedUn[0] && !NormalizeTagPath(speedUn, m.speedTag, sizeof(m.speedTag)))
        {
            continue;
        }
        strncpy(m.destCommand, cmdField, sizeof(m.destCommand) - 1);
        m.destCommand[sizeof(m.destCommand) - 1] = '\0';
        strncpy(m.destTarget, dstUn, sizeof(m.destTarget) - 1);
        m.destTarget[sizeof(m.destTarget) - 1] = '\0';
        strncpy(m.destType, typeUn, sizeof(m.destType) - 1);
        m.destType[sizeof(m.destType) - 1] = '\0';
        if (fieldCount >= 12)
        {
            m.jobChangeWorkflow = (atoi(fields[11]) != 0);
        }
        else if (cmdField && (strcmp(cmdField, "0004") == 0 || strcmp(cmdField, "0005") == 0))
        {
            m.jobChangeWorkflow = true;
        }
        gMappings[gMappingCount++] = m;
    }
}

static void LoadTagPathsFromConfig()
{
    gPlcTagCount = 0;
    const NBString blob = static_cast<NBString>(gPlcTagPathsBlob);
    const char *p = blob.c_str();
    if (!p || !*p)
    {
        return;
    }

    while (*p && gPlcTagCount < kMaxPlcTags)
    {
        char line[kMaxTagPathLen]{0};
        size_t n = 0;
        while (*p && *p != '\n' && n + 1 < sizeof(line))
        {
            line[n++] = *p++;
        }
        line[n] = '\0';
        if (*p == '\n')
        {
            ++p;
        }

        char normalized[kMaxTagPathLen]{0};
        if (NormalizeTagPath(line, normalized, sizeof(normalized)))
        {
            bool exists = false;
            for (int i = 0; i < gPlcTagCount; ++i)
            {
                if (strcmp(gPlcTags[i], normalized) == 0)
                {
                    exists = true;
                    break;
                }
            }
            if (!exists)
            {
                const size_t len = strlen(normalized);
                const size_t copyLen = (len < (kMaxTagPathLen - 1)) ? len : (kMaxTagPathLen - 1);
                memcpy(gPlcTags[gPlcTagCount], normalized, copyLen);
                gPlcTags[gPlcTagCount][copyLen] = '\0';
                ++gPlcTagCount;
            }
        }
    }
}

static void SaveTagPathsToConfig(bool persistFlash)
{
    NBString joined;
    for (int i = 0; i < gPlcTagCount; ++i)
    {
        if (i > 0)
        {
            joined += "\n";
        }
        joined += gPlcTags[i];
    }
    gPlcTagPathsBlob = joined;
    if (persistFlash)
    {
        SaveConfigToStorage();
    }
}

static void LoadImportedBrowsePathsFromConfig()
{
    gImportedBrowseTagCount = 0;
    const NBString blob = static_cast<NBString>(gImportedBrowsePathsBlob);
    const char *p = blob.c_str();
    if (!p || !*p)
    {
        return;
    }

    while (*p && gImportedBrowseTagCount < kMaxImportedBrowseTags)
    {
        char line[kMaxTagPathLen]{0};
        size_t n = 0;
        while (*p && *p != '\n' && n + 1 < sizeof(line))
        {
            line[n++] = *p++;
        }
        line[n] = '\0';
        if (*p == '\n')
        {
            ++p;
        }

        char normalized[kMaxTagPathLen]{0};
        if (!NormalizeTagPath(line, normalized, sizeof(normalized)))
        {
            continue;
        }

        bool exists = false;
        for (int i = 0; i < gImportedBrowseTagCount; ++i)
        {
            if (strcmp(gImportedBrowseTags[i], normalized) == 0)
            {
                exists = true;
                break;
            }
        }
        if (exists)
        {
            continue;
        }

        const size_t len = strlen(normalized);
        const size_t copyLen = (len < (kMaxTagPathLen - 1)) ? len : (kMaxTagPathLen - 1);
        memcpy(gImportedBrowseTags[gImportedBrowseTagCount], normalized, copyLen);
        gImportedBrowseTags[gImportedBrowseTagCount][copyLen] = '\0';
        ++gImportedBrowseTagCount;
    }
}

static void SaveImportedBrowsePathsToConfig(bool persistFlash)
{
    NBString joined;
    for (int i = 0; i < gImportedBrowseTagCount; ++i)
    {
        if (i > 0)
        {
            joined += "\n";
        }
        joined += gImportedBrowseTags[i];
    }
    gImportedBrowsePathsBlob = joined;
    if (persistFlash)
    {
        SaveConfigToStorage();
    }
}

static bool AddImportedBrowsePath(const char *path)
{
    if (gImportedBrowseTagCount >= kMaxImportedBrowseTags)
    {
        return false;
    }

    char normalized[kMaxTagPathLen]{0};
    if (!NormalizeTagPath(path, normalized, sizeof(normalized)))
    {
        return false;
    }

    for (int i = 0; i < gImportedBrowseTagCount; ++i)
    {
        if (strcmp(gImportedBrowseTags[i], normalized) == 0)
        {
            return true;
        }
    }

    const size_t len = strlen(normalized);
    const size_t copyLen = (len < (kMaxTagPathLen - 1)) ? len : (kMaxTagPathLen - 1);
    memcpy(gImportedBrowseTags[gImportedBrowseTagCount], normalized, copyLen);
    gImportedBrowseTags[gImportedBrowseTagCount][copyLen] = '\0';
    ++gImportedBrowseTagCount;
    return true;
}

static void ClearImportedBrowsePaths()
{
    gImportedBrowseTagCount = 0;
    for (int i = 0; i < kMaxImportedBrowseTags; ++i)
    {
        gImportedBrowseTags[i][0] = '\0';
    }
}

static void MergeImportedBrowsePathsIntoBrowseCache()
{
    for (int i = 0; i < gImportedBrowseTagCount && gBrowsedTagCount < kMaxBrowsedTags; ++i)
    {
        const char *path = gImportedBrowseTags[i];
        if (!path[0])
        {
            continue;
        }

        bool exists = false;
        for (int j = 0; j < gBrowsedTagCount; ++j)
        {
            const BrowsedPlcTag &existingTag = gBrowsedTags[j];
            if (strcmp(existingTag.name, path) == 0)
            {
                exists = true;
                break;
            }

            // If PLC browse already includes an array base tag, skip imported
            // element paths such as "Tag[0]" to avoid duplicate rows.
            if (existingTag.isArray)
            {
                const size_t baseLen = strlen(existingTag.name);
                if (strncmp(path, existingTag.name, baseLen) == 0 && path[baseLen] == '[')
                {
                    exists = true;
                    break;
                }
            }
        }
        if (exists)
        {
            continue;
        }

        BrowsedPlcTag &tag = gBrowsedTags[gBrowsedTagCount++];
        memset(tag.name, 0, sizeof(tag.name));
        const size_t nameLen = strlen(path);
        const size_t copyLen = (nameLen < (sizeof(tag.name) - 1)) ? nameLen : (sizeof(tag.name) - 1);
        memcpy(tag.name, path, copyLen);
        tag.symbolType = 0;
        tag.elementSize = 0;
        tag.dim0 = 0;
        tag.dim1 = 0;
        tag.dim2 = 0;
        tag.isArray = false;
        tag.arrayLength = 0;
        tag.imported = true;
    }
}

static bool AddPlcTagPath(const char *path)
{
    if (gPlcTagCount >= kMaxPlcTags)
    {
        return false;
    }
    char normalized[kMaxTagPathLen]{0};
    if (!NormalizeTagPath(path, normalized, sizeof(normalized)))
    {
        return false;
    }

    for (int i = 0; i < gPlcTagCount; ++i)
    {
        if (strcmp(gPlcTags[i], normalized) == 0)
        {
            return true;
        }
    }

    const size_t len = strlen(normalized);
    const size_t copyLen = (len < (kMaxTagPathLen - 1)) ? len : (kMaxTagPathLen - 1);
    memcpy(gPlcTags[gPlcTagCount], normalized, copyLen);
    gPlcTags[gPlcTagCount][copyLen] = '\0';
    ++gPlcTagCount;
    return true;
}

static bool UrlDecode(const char *in, char *out, size_t outSize)
{
    if (!in || !out || outSize < 2)
    {
        return false;
    }

    size_t o = 0;
    for (size_t i = 0; in[i] != '\0' && o + 1 < outSize; ++i)
    {
        if (in[i] == '+')
        {
            out[o++] = ' ';
        }
        else if (in[i] == '%' && isxdigit(static_cast<unsigned char>(in[i + 1])) && isxdigit(static_cast<unsigned char>(in[i + 2])))
        {
            char hex[3] = {in[i + 1], in[i + 2], '\0'};
            out[o++] = static_cast<char>(strtol(hex, nullptr, 16));
            i += 2;
        }
        else
        {
            out[o++] = in[i];
        }
    }
    out[o] = '\0';
    return true;
}

static bool GetQueryParam(const char *url, const char *key, char *out, size_t outSize)
{
    if (!url || !key || !out || outSize < 2)
    {
        return false;
    }

    const char *q = strchr(url, '?');
    if (!q)
    {
        return false;
    }
    ++q;
    const size_t keyLen = strlen(key);

    while (*q)
    {
        const char *amp = strchr(q, '&');
        const char *end = amp ? amp : (q + strlen(q));
        const char *eq = static_cast<const char *>(memchr(q, '=', static_cast<size_t>(end - q)));
        if (eq)
        {
            const size_t klen = static_cast<size_t>(eq - q);
            if (klen == keyLen && strncmp(q, key, keyLen) == 0)
            {
                char encoded[1024]{0};
                size_t vlen = static_cast<size_t>(end - (eq + 1));
                if (vlen >= sizeof(encoded))
                {
                    vlen = sizeof(encoded) - 1;
                }
                memcpy(encoded, eq + 1, vlen);
                encoded[vlen] = '\0';
                return UrlDecode(encoded, out, outSize);
            }
        }
        if (!amp)
        {
            break;
        }
        q = amp + 1;
    }
    return false;
}

static void MaybeRetryStoredPlcConnect()
{
    if (!IsNetworkLinkReady())
    {
        return;
    }

    const IPADDR4 targetIp = static_cast<IPADDR4>(gPlcTargetIp);
    if (targetIp.IsNull())
    {
        return;
    }

    if (gPlcConnState.connected)
    {
        return;
    }

    const uint32_t nowSec = static_cast<uint32_t>(Secs);
    const uint32_t elapsed = nowSec - gPlcConnState.lastAttemptSec;
    if (!gPlcConnState.attempted || elapsed >= kPlcReconnectIntervalSec)
    {
        iprintf("PLC reconnect retry: %hI\r\n", targetIp);
        AttemptConnectStoredPlc();
    }
}

static void MaybeRetryStoredReajetConnect()
{
    if (!IsNetworkLinkReady())
    {
        return;
    }

    char ipText[64]{0};
    CopyReajetTargetIpText(ipText, sizeof(ipText));
    if (!ipText[0])
    {
        return;
    }

    if (gReajetConnState.connected)
    {
        return;
    }

    const uint32_t nowSec = static_cast<uint32_t>(Secs);
    const uint32_t elapsed = nowSec - gReajetConnState.lastAttemptSec;
    if (!gReajetConnState.attempted || elapsed >= kPlcReconnectIntervalSec)
    {
        AttemptConnectStoredReajet();
    }
}

static uint16_t ReadLe16(const uint8_t *p)
{
    return static_cast<uint16_t>(p[0]) |
           (static_cast<uint16_t>(p[1]) << 8);
}

static uint32_t ReadLe32(const uint8_t *p)
{
    return static_cast<uint32_t>(p[0]) |
           (static_cast<uint32_t>(p[1]) << 8) |
           (static_cast<uint32_t>(p[2]) << 16) |
           (static_cast<uint32_t>(p[3]) << 24);
}

#endif // BURNERGATEWAY_MAIN_TU

