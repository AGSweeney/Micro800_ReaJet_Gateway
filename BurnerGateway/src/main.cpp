/* Revision: 3.5.7 */

/******************************************************************************
* Copyright 1998-2024 NetBurner, Inc.  ALL RIGHTS RESERVED
*
*    Permission is hereby granted to purchasers of NetBurner Hardware to use or
*    modify this computer program for any use as long as the resultant program
*    is only executed on NetBurner provided hardware.
*
*    No other rights to use this program or its derivatives in part or in
*    whole are granted.
*
*    It may be possible to license this or other NetBurner software for use on
*    non-NetBurner Hardware. Contact sales@Netburner.com for more information.
*
*    NetBurner makes no representation or warranties with respect to the
*    performance of this computer program, and specifically disclaims any
*    responsibility for any damages, special or consequential, connected with
*    the use of this program.
*
* NetBurner
* 16855 W Bernardo Dr
* San Diego, CA 92127
* www.netburner.com
******************************************************************************/

/*
 * CIP Data Gateway
 *
 * Copyright (c) 2026 Adam G. Sweeney
 *
 * SPDX-License-Identifier: MIT
 * See the LICENSE file in the repository root for full license text.
 */

#include <init.h>
#include <nbrtos.h>
#include <netinterface.h>
#include <config_server.h>
#include <config_obj.h>
#include <http.h>
#include <fdprintf.h>
#include <tcp.h>
#include <udp.h>
#include <system.h>
#include <hal.h>
#include <sim.h>
#include <sim5441x.h>
#include <ethervars.h>
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <cstring>

// Transitional include-based modularization keeps all static config objects in one TU.
#define BURNERGATEWAY_MAIN_TU 1
#include "core/core_state.cpp"
#include "enip/enip_runtime.cpp"
#include "http/http_handlers_core.cpp"
#include "reajet/reajet_runtime.cpp"
#include "http/http_handlers_tags.cpp"
#include "mapping/mapping_runtime.cpp"
#include "http/http_register.cpp"
#undef BURNERGATEWAY_MAIN_TU

static void PrintNetworkInfo()
{
    int ifNumber = GetFirstInterface();
    if (!ifNumber)
    {
        iprintf("No network interfaces found.\r\n");
        return;
    }

    while (ifNumber)
    {
        InterfaceBlock *ifBlock = GetInterfaceBlock(ifNumber);
        const char *ifName = ifBlock ? ifBlock->GetInterfaceName() : "Unknown";
        iprintf("Interface %d (%s)\r\n", ifNumber, ifName);
        const IPADDR4 primaryIp = InterfaceIP(ifNumber);
        iprintf("  IP:      %hI\r\n", primaryIp);
#ifdef AUTOIP
        const IPADDR4 autoIp = InterfaceAutoIP(ifNumber);
        if (!autoIp.IsNull())
        {
            iprintf("  AutoIP:  %hI\r\n", autoIp);
        }
#endif
        if (primaryIp.IsNull() && !GetInterfaceIpv4Address(ifNumber).IsNull())
        {
            iprintf("  Use:     %hI (link-local; no DHCP)\r\n", GetInterfaceIpv4Address(ifNumber));
        }
        iprintf("  Mask:    %hI\r\n", InterfaceMASK(ifNumber));
        iprintf("  Gateway: %hI\r\n", InterfaceGate(ifNumber));
        iprintf("  DNS1:    %hI\r\n", InterfaceDNS(ifNumber));
        iprintf("  DNS2:    %hI\r\n", InterfaceDNS2(ifNumber));
        ifNumber = GetNextInterface(ifNumber);
    }
}

/*
 *  Main entry point for the example
 */
void UserMain(void *pd)
{
    init();        // Initialize network stack
    //Enable system diagnostics. Probably should remove for production code.
    EnableSystemDiagnostics();
    LoadTagPathsFromConfig();
    LoadImportedBrowsePathsFromConfig();
    LoadMappingsFromConfig();
    EnableConfigMirror();   // Expose config JSON APIs to our WebUI
    StartHttp();   // Start web server on default port 80
    const bool networkUp = WaitForNetworkWithAutoIpFallback();

    iprintf("Web Application: %s\r\nNNDK Revision: %s\r\n", AppName, GetReleaseTag());
    if (networkUp)
    {
        iprintf("Network is active. Device IP information:\r\n");
        PrintNetworkInfo();
        AttemptConnectStoredPlc();
        AttemptConnectStoredReajet();
        iprintf("ENIP scan is available from WebUI action: /api/plc/scan\r\n");
    }
    else
    {
        iprintf("Warning: network did not become active (no DHCP or AutoIP address).\r\n");
    }

    while (1)
    {
        MaybeRetryStoredPlcConnect();
        MaybeRetryStoredReajetConnect();
        MaybeRunReaStatusPoll();
        MaybeRunReaPiService();
        if (IsMappingRuntimeActive())
        {
            RunMappingsBackgroundOnce();
        }
        uint32_t pollMs = static_cast<uint32_t>(gMappingPollMs);
        if (pollMs < 10u) pollMs = 10u;
        if (pollMs > 5000u) pollMs = 5000u;
        uint32_t delayTicks = (pollMs * TICKS_PER_SECOND + 999u) / 1000u;
        if (delayTicks < 1) delayTicks = 1;
        OSTimeDly(delayTicks);
    }
}
