/*
 * CIP Data Gateway
 *
 * Copyright (c) 2026 Adam G. Sweeney
 *
 * SPDX-License-Identifier: MIT
 * See the LICENSE file in the repository root for full license text.
 */

#ifdef BURNERGATEWAY_MAIN_TU
static bool MappingReajetStepOk(const MappingRecord &m,
                                const char *cmd,
                                const char *jobValue,
                                const char *contentValue,
                                bool useEot,
                                bool writeFinalAck)
{
    if (!SendMappingReajetCommand(m, cmd, jobValue, contentValue, useEot, writeFinalAck))
    {
        if (!writeFinalAck)
        {
            WriteMappingResponseToPlc(m, false, gRuntimeLastTxErrorCode);
        }
        return false;
    }
    if (!gRuntimeLastTxResponseAck)
    {
        if (strcmp(cmd, "0003") == 0 && ReaPlcStopAlreadyStoppedResponse(gRuntimeLastTxResponse))
        {
            return true;
        }
        if (!writeFinalAck)
        {
            WriteMappingResponseToPlc(m, false, gRuntimeLastTxErrorCode);
        }
        return false;
    }
    return true;
}

static bool RunMappingJobChangeWorkflow(MappingRecord &m,
                                        const char *contentCmd,
                                        const char *jobValue,
                                        const char *textValue,
                                        bool haveJob,
                                        bool haveText,
                                        bool useEot)
{
    gReaLiveStatus.clockSpeedValid = false;
    gReaPiSession.productSpeedValid = false;
    gReaPiSession.encoder1SpeedValid = false;

    if (!MappingReajetStepOk(m, "0003", nullptr, nullptr, useEot, false))
    {
        m.errorCount++;
        return false;
    }
    m.sendCount++;

    if (haveJob)
    {
        if (!MappingReajetStepOk(m, "0001", jobValue, nullptr, useEot, false))
        {
            m.errorCount++;
            return false;
        }
        m.sendCount++;
    }

    if (haveText)
    {
        if (!MappingReajetStepOk(m, contentCmd, haveJob ? jobValue : nullptr, textValue, useEot, false))
        {
            m.errorCount++;
            return false;
        }
        m.sendCount++;
    }

    if (!MappingReajetStepOk(m, "0002", nullptr, nullptr, useEot, false))
    {
        m.errorCount++;
        return false;
    }
    m.sendCount++;

    char verifiedJobStatus[9]{0};
    bool jobReleased = false;
    if (PollReajetJobStatus(useEot, verifiedJobStatus, sizeof(verifiedJobStatus), &jobReleased))
    {
        AppendRuntimeJobStateVerified(verifiedJobStatus, jobReleased);
        if (!jobReleased)
        {
            iprintf("Mapping workflow id=%d start ACK but GETSTATUS not released (%s)\r\n",
                    m.id,
                    verifiedJobStatus);
            WriteMappingResponseToPlc(m, false, gRuntimeLastTxErrorCode);
            m.errorCount++;
            return false;
        }
    }
    else
    {
        iprintf("Mapping workflow id=%d start sent but GETSTATUS verify failed\r\n", m.id);
    }

    if (m.speedTag[0])
    {
        if (!WriteMappingSpeedToPlc(m))
        {
            WriteMappingResponseToPlc(m, false, gRuntimeLastTxErrorCode);
            m.errorCount++;
            return false;
        }
    }

    WriteMappingResponseToPlc(m, true, gRuntimeLastTxErrorCode);
    return true;
}

static void RunMappingsBackgroundOnce()
{
    char ipText[64]{0};
    CopyReajetTargetIpText(ipText, sizeof(ipText));
    IPADDR4 reajetIp{};
    const bool reajetReady = ParseIpv4Text(ipText, reajetIp);
    (void)reajetIp;
    const bool useEot = (static_cast<uint32_t>(gReajetTargetPort) == 22169u);

    for (int i = 0; i < gMappingCount; ++i)
    {
        MappingRecord &m = gMappings[i];
        if (!m.enabled || !m.triggerTag[0])
        {
            continue;
        }

        bool triggerValue = false;
        if (!ReadMappingPlcBool(m.triggerTag, triggerValue))
        {
            m.errorCount++;
            continue;
        }
        m.readCount++;

        if (!m.hasLastTriggerValue)
        {
            m.lastTriggerValue = triggerValue;
            m.hasLastTriggerValue = true;
            continue;
        }

        const bool triggerChanged = (triggerValue != m.lastTriggerValue);
        const bool triggerRose = triggerChanged && triggerValue;
        const bool triggerFell = triggerChanged && !triggerValue;
        m.lastTriggerValue = triggerValue;

        if (triggerFell)
        {
            ResetMappingResponseToPlc(m);
            continue;
        }
        if (!triggerRose)
        {
            continue;
        }

        if (!reajetReady)
        {
            m.errorCount++;
            continue;
        }

        ResetMappingResponseToPlc(m);

        char jobValue[kReaGatewayMaxValueTextLen]{0};
        char textValue[kReaGatewayMaxValueTextLen]{0};
        bool haveJob = false;
        bool haveText = false;
        if (m.jobTag[0])
        {
            if (!ReadMappingPlcValue(m.jobTag, jobValue, sizeof(jobValue)))
            {
                m.errorCount++;
                continue;
            }
            haveJob = (jobValue[0] != '\0');
        }
        if (m.textTag[0])
        {
            if (!ReadMappingPlcValue(m.textTag, textValue, sizeof(textValue)))
            {
                m.errorCount++;
                continue;
            }
            haveText = (textValue[0] != '\0');
        }

        SetRuntimeLastTxPayload(haveJob ? jobValue : "", haveText ? textValue : "");

        const char *cmd = m.destCommand[0] ? m.destCommand : "0004";
        bool commandOk = false;
        if (m.jobChangeWorkflow &&
            (strcmp(cmd, "0004") == 0 || strcmp(cmd, "0005") == 0) &&
            (haveJob || haveText))
        {
            commandOk = RunMappingJobChangeWorkflow(m,
                                                    cmd,
                                                    haveJob ? jobValue : nullptr,
                                                    haveText ? textValue : nullptr,
                                                    haveJob,
                                                    haveText,
                                                    useEot);
        }
        else if (strcmp(cmd, "0001") == 0)
        {
            const char *jobForCmd = haveJob ? jobValue : (haveText ? textValue : nullptr);
            if (!jobForCmd || !jobForCmd[0])
            {
                m.errorCount++;
                continue;
            }
            if (SendMappingReajetCommand(m, "0001", jobForCmd, nullptr, useEot, true))
            {
                m.sendCount++;
                commandOk = true;
            }
            else
            {
                m.errorCount++;
            }
        }
        else if (strcmp(cmd, "0004") == 0 || strcmp(cmd, "0005") == 0)
        {
            if (!haveText)
            {
                m.errorCount++;
                continue;
            }
            iprintf("Mapping id=%d workflow OFF: assign+update only (no 0003 stop / 0002 start)\r\n", m.id);
            if (haveJob)
            {
                if (!SendMappingReajetCommand(m, "0001", jobValue, nullptr, useEot, false))
                {
                    m.errorCount++;
                    continue;
                }
                m.sendCount++;
            }
            if (SendMappingReajetCommand(m, cmd, haveJob ? jobValue : nullptr, textValue, useEot, true))
            {
                m.sendCount++;
                commandOk = true;
                char verifiedJobStatus[9]{0};
                bool jobReleased = false;
                if (PollReajetJobStatus(useEot, verifiedJobStatus, sizeof(verifiedJobStatus), &jobReleased))
                {
                    AppendRuntimeJobStateVerified(verifiedJobStatus, jobReleased);
                }
                strncat(gRuntimeLastTxResponseDecoded,
                        " | Workflow OFF: stop/start not sent",
                        sizeof(gRuntimeLastTxResponseDecoded) - strlen(gRuntimeLastTxResponseDecoded) - 1);
            }
            else
            {
                m.errorCount++;
            }
        }
        else
        {
            const char *jobForCmd = haveJob ? jobValue : nullptr;
            if (SendMappingReajetCommand(m, cmd, jobForCmd, haveText ? textValue : nullptr, useEot, true))
            {
                m.sendCount++;
                commandOk = true;
            }
            else
            {
                m.errorCount++;
            }
        }

        // Per-mapping speed readback for single-command mappings (workflow mode
        // writes speed inside RunMappingJobChangeWorkflow after start).
        if (commandOk && m.speedTag[0] && !m.jobChangeWorkflow)
        {
            if (!WriteMappingSpeedToPlc(m))
            {
                m.errorCount++;
            }
        }
    }
    gRuntimeCycles++;
    gRuntimeLastRunMs = static_cast<uint32_t>(Secs * 1000);
}

#endif // BURNERGATEWAY_MAIN_TU

