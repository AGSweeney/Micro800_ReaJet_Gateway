/*
 * CIP Data Gateway
 *
 * Copyright (c) 2026 Adam G. Sweeney
 *
 * SPDX-License-Identifier: MIT
 * See the LICENSE file in the repository root for full license text.
 */

var GW_HELP = {
  "index.html": {
    title: "Configuration Console Help",
    html:
      "<p>This gateway connects a <b>Micro800 PLC</b> to a <b>REAJet printer</b>. Use the cards below in order when setting up a new line.</p>" +
      "<h3>Recommended setup order</h3>" +
      "<ol>" +
      "<li><b>Network Configuration</b>  - Put the gateway on the plant network (or split PLC and printer onto separate ports on MOD54417).</li>" +
      "<li><b>Device Configuration</b>  - Scan for and select the PLC, then enter the REAJet IP and ports.</li>" +
      "<li><b>Print Mappings</b>  - Import or browse PLC tags, then create mappings that send print data when a trigger bit rises.</li>" +
      "<li><b>REAJet Status Probe</b>  - Optional diagnostics page; does not change saved settings.</li>" +
      "</ol>" +
      "<h3>What is saved where?</h3>" +
      "<ul>" +
      "<li>Network, device, mapping, and tag settings are stored in the gateway flash memory.</li>" +
      "<li>Changes to network IP or dual-port mode require a <b>reboot</b> to take effect.</li>" +
      "<li>The status probe page is read-only unless you explicitly save elsewhere.</li>" +
      "</ul>" +
      "<h3>Typical wiring</h3>" +
      "<p>PLC on one Ethernet port/subnet, REAJet on the same LAN or on the second port (MOD54417 independent mode). The gateway reads PLC tags and sends REA-PLC commands to the printer.</p>"
  },

  "network.html": {
    title: "Network Configuration Help",
    html:
      "<p>Configure how the gateway gets its IP address(es). After changing settings here, use <b>Save &amp; Reboot</b> unless you only saved for later.</p>" +
      "<h3>IPv4 mode (each Ethernet interface)</h3>" +
      "<ul>" +
      "<li><b>DHCP</b>  - The gateway requests an address from your plant DHCP server. Use this on a normal managed network. If no server is present, the device falls back to link-local <b>169.254.x.x</b> (AutoIP) after about 30 seconds.</li>" +
      "<li><b>DHCP w Fallback</b>  - Tries DHCP first; if that fails, uses the static IP, mask, and gateway you enter below.</li>" +
      "<li><b>Static</b>  - Fixed IP at all times. Enter IP, mask, gateway, and DNS as required by your IT group.</li>" +
      "<li><b>Disabled</b>  - Turns IPv4 off on that interface (advanced; rarely needed).</li>" +
      "</ul>" +
      "<h3>Static fields (Static or DHCP w Fallback)</h3>" +
      "<ul>" +
      "<li><b>Static IP</b>  - Address operators and your PC will use to open this web page.</li>" +
      "<li><b>Mask</b>  - Subnet mask (often 255.255.255.0 on a /24 network).</li>" +
      "<li><b>Gateway</b>  - Router to other subnets; use 0.0.0.0 if everything is on one subnet.</li>" +
      "<li><b>DNS 1 / DNS 2</b>  - Optional name servers; 0.0.0.0 is fine if you connect by IP only.</li>" +
      "</ul>" +
      "<h3>Dual Ethernet Port Mode (MOD54417 only)</h3>" +
      "<ul>" +
      "<li><b>Bridge both Ethernet ports</b> (switch mode, default)  - Both RJ-45 jacks act as one network with <b>one IP</b>. Plug the PLC into one jack and the REAJet into the other.</li>" +
      "<li><b>Independent ports</b> (unchecked)  - Each port is its own interface with its own IP. Example: Ethernet0 = PLC subnet, Ethernet1 = printer subnet. Requires reboot.</li>" +
      "</ul>" +
      "<h3>Buttons</h3>" +
      "<ul>" +
      "<li><b>Refresh</b>  - Reload current settings from the gateway.</li>" +
      "<li><b>Save to Flash</b>  - Store settings; reboot later to apply network changes.</li>" +
      "<li><b>Save &amp; Reboot</b>  - Save and restart immediately. A wait dialog appears; the page reloads when the gateway is back.</li>" +
      "</ul>" +
      "<h3>Bench / laptop direct cable</h3>" +
      "<p>Leave mode on DHCP, set your PC adapter to obtain an IP automatically, wait for both sides to get 169.254.x.x addresses, then browse to the gateway AutoIP shown on this page.</p>"
  },

  "devices.html": {
    title: "Device Configuration Help",
    html:
      "<p>Select which PLC the gateway talks to and where the REAJet printer lives on the network.</p>" +
      "<h3>Micro800 PLC</h3>" +
      "<ul>" +
      "<li><b>Scan Micro800 Devices</b>  - Broadcasts an EtherNet/IP scan on the gateway network and lists Allen-Bradley Micro800-class devices.</li>" +
      "<li><b>Select</b>  - Saves that PLC IP as the active target for tag reads and mapping scans.</li>" +
      "<li><b>Refresh</b>  - Shows the currently saved PLC without scanning again.</li>" +
      "<li><b>Remove PLC</b>  - Clears the stored PLC; mappings cannot read tags until a new PLC is selected.</li>" +
      "</ul>" +
      "<p>The PLC must be reachable on the same network (or the PLC-facing port in dual-port mode). Firewall rules must allow EtherNet/IP (TCP/44818 and UDP/2222).</p>" +
      "<h3>REAJet destination</h3>" +
      "<ul>" +
      "<li><b>REAJet IP</b>  - Printer IP address for REA-PLC commands (default port 22170).</li>" +
      "<li><b>REAJet Port</b>  - TCP port for REA-PLC protocol (factory default 22170).</li>" +
      "<li><b>Mapping scan interval (ms)</b>  - How often the gateway polls PLC <b>trigger</b> tags for all print mappings (10-5000 ms, default 50). Lower = faster response, more PLC load.</li>" +
      "</ul>" +
      "<p><b>Save REAJet Settings</b> stores the IP/port/interval and tests TCP connectivity. Status tags and REA-PI options are configured through the gateway API/NVS (not shown on this page) and are used for background status polling.</p>" +
      "<h3>Status pills after save</h3>" +
      "<ul>" +
      "<li><b>TCP OK</b>  - Gateway opened a REA-PLC socket to the printer.</li>" +
      "<li><b>Framing EOT</b>  - Long payloads use EOT framing on port 22169 when required by the printer firmware.</li>" +
      "</ul>" +
      "<h3>Next step</h3>" +
      "<p>Go to <b>Print Mappings</b>, browse or import PLC tags, then add mappings.</p>"
  },

  "status.html": {
    title: "REAJet Status Probe Help",
    html:
      "<p>This page is for <b>manual diagnostics</b>. It contacts a REAJet once and displays raw status. It does <b>not</b> replace Device Configuration unless you copy values yourself.</p>" +
      "<h3>Probe target fields</h3>" +
      "<ul>" +
      "<li><b>REAJet IP</b>  - Printer to test.</li>" +
      "<li><b>REA-PLC Port</b>  - Usually 22170. Used for binary GETSTATUS-style commands.</li>" +
      "<li><b>REA-PI Port</b>  - Usually 22171. XML protocol for detailed printer information.</li>" +
      "<li><b>REA-PI Version</b>  - Protocol version string sent during REA-PI handshake (default 2.0).</li>" +
      "<li><b>Include REA-PI</b>  - <b>No</b> = REA-PLC only (faster). <b>Yes</b> = also run REA-PI XML queries (speed, counters, IO, label content, etc.).</li>" +
      "</ul>" +
      "<h3>Buttons</h3>" +
      "<ul>" +
      "<li><b>Probe Status</b>  - Run one-shot diagnostics with the IP/ports above.</li>" +
      "<li><b>Use Configured REAJet</b>  - Fill the form from Device Configuration saved settings.</li>" +
      "</ul>" +
      "<h3>Reading the results</h3>" +
      "<ul>" +
      "<li><b>Status Summary</b>  - Job assigned/released, printer active, line speed, triggers, job file name.</li>" +
      "<li><b>REA-PLC GETSTATUS</b>  - Low-level device/job status bytes and decoded text.</li>" +
      "<li><b>REA-PI XML</b>  - Expanded data when REA-PI is enabled (device info, production data, sensors, label content). Expand XML blocks to troubleshoot communication.</li>" +
      "</ul>" +
      "<p>Green pills = success. Red = failure or missing data. Use this page to verify cabling and IP before commissioning mappings.</p>"
  },

  "mapping.html": {
    title: "Print Mappings Help",
    html:
      "<p>Mappings watch a PLC <b>trigger bit</b>. When the bit rises (false -> true), the gateway reads job/text tags and sends REA-PLC commands to the REAJet, then writes acknowledgement tags back to the PLC.</p>" +
      "<h3>Runtime status area</h3>" +
      "<ul>" +
      "<li><b>Scanner</b>  - Mapping poll task running or stopped.</li>" +
      "<li><b>PLC / REAJet</b>  - Connection health for tag reads and printer TCP.</li>" +
      "<li><b>Mappings / Cycles</b>  - Count of configured mappings and poll cycles executed.</li>" +
      "<li><b>Last Activity</b>  - Last job, text, command, ASCII sent, printer response, and error code for troubleshooting.</li>" +
      "<li><b>Stop Scanner / Start Scanner</b>  - Pauses mapping triggers without deleting configuration.</li>" +
      "</ul>" +
      "<h3>Mapping list columns</h3>" +
      "<ul>" +
      "<li><b>Job / Text / Trigger tags</b>  - PLC tag paths (must be imported or browsed first).</li>" +
      "<li><b>REA Command / Target</b>  - REA-PLC command code and object name on the printer.</li>" +
      "<li><b>On</b>  - Mapping enabled or disabled.</li>" +
      "</ul>" +
      "<h3>Add / edit mapping fields</h3>" +
      "<ul>" +
      "<li><b>Job Tag</b>  - PLC value sent as print job name/number (required for job-change workflow).</li>" +
      "<li><b>Text Tag</b>  - PLC string sent as label content (required for update commands 0004/0005).</li>" +
      "<li><b>Trigger Tag (BOOL)</b>  - Rising edge starts one print sequence.</li>" +
      "<li><b>Response Tag</b>  - PLC BOOL/DINT the gateway sets when the printer acknowledges (cleared on trigger fall).</li>" +
      "<li><b>Error Tag</b>  - PLC tag receiving error/status code on failure.</li>" +
      "<li><b>Speed Tag (REAL)</b>  - After a successful job-change start, gateway writes live line speed (m/min) from REA-PI cache.</li>" +
      "<li><b>Job change workflow</b>  - Full stop/assign/update/start sequence (see flowchart below). Turn off for simple assign+update only.</li>" +
      "<li><b>REA-PLC Command</b>  - Default 0004 (overwrite object contents). Workflow mode uses 0004 or 0005 internally after assign.</li>" +
      "<li><b>Destination Type / REA Target</b>  - Printer object type and name (from label/job setup on REAJet).</li>" +
      "<li><b>Enable Mapping</b>  - Uncheck to keep definition but ignore triggers.</li>" +
      "</ul>" +
      "<h3>Configured tags &amp; browse</h3>" +
      "<ul>" +
      "<li><b>Browse PLC Tags</b>  - Reads tag database from the selected Micro800 (or import UDT CSV from Studio 5000 export).</li>" +
      "<li>Tags must appear in the configured list before you can pick them in a mapping.</li>" +
      "</ul>" +
      "<h3>Import / export</h3>" +
      "<ul>" +
      "<li><b>Export Mappings</b>  - Download JSON backup.</li>" +
      "<li><b>Import Mappings</b>  - Restore from JSON.</li>" +
      "<li><b>Reset Stats</b>  - Clears runtime counters only.</li>" +
      "</ul>" +
      "<h3>Mapping trigger flowchart</h3>" +
      "<p>One rising edge on the trigger bit runs one sequence:</p>" +
      "<div class=\"help-flow\">" +
        "<div class=\"help-flow-step\">Scanner polls trigger tag every <i>Mapping scan interval</i> (Devices page)</div>" +
        "<div class=\"help-flow-arrow\" aria-hidden=\"true\"></div>" +
        "<div class=\"help-flow-step help-flow-decision\">Trigger BOOL rose (0 -> 1)?</div>" +
        "<div class=\"help-flow-step help-flow-note\">Yes - continue below</div>" +
        "<div class=\"help-flow-step\">Clear response tag; read Job tag and Text tag from PLC</div>" +
        "<div class=\"help-flow-arrow\" aria-hidden=\"true\"></div>" +
        "<div class=\"help-flow-step help-flow-decision\">Job change workflow enabled?</div>" +
        "<div class=\"help-flow-branch\">" +
          "<div class=\"help-flow-col\">" +
            "<div class=\"help-flow-label\">Yes  - full job change</div>" +
            "<div class=\"help-flow-step\">0003 Stop print job</div>" +
            "<div class=\"help-flow-arrow\" aria-hidden=\"true\"></div>" +
            "<div class=\"help-flow-step\">0001 Assign print job (job tag value)</div>" +
            "<div class=\"help-flow-arrow\" aria-hidden=\"true\"></div>" +
            "<div class=\"help-flow-step\">0004 or 0005 Update text/properties</div>" +
            "<div class=\"help-flow-arrow\" aria-hidden=\"true\"></div>" +
            "<div class=\"help-flow-step\">0002 Start print job</div>" +
            "<div class=\"help-flow-arrow\" aria-hidden=\"true\"></div>" +
            "<div class=\"help-flow-step\">Write speed to Speed tag (if configured)</div>" +
          "</div>" +
          "<div class=\"help-flow-col\">" +
            "<div class=\"help-flow-label\">No  - simple update</div>" +
            "<div class=\"help-flow-step\">0001 Assign (if job present)</div>" +
            "<div class=\"help-flow-arrow\" aria-hidden=\"true\"></div>" +
            "<div class=\"help-flow-step\">Single 0004/0005 command with job + text</div>" +
          "</div>" +
        "</div>" +
        "<div class=\"help-flow-arrow\" aria-hidden=\"true\"></div>" +
        "<div class=\"help-flow-step\">Write Response tag (OK) or Error tag (fault)</div>" +
        "<div class=\"help-flow-arrow\" aria-hidden=\"true\"></div>" +
        "<div class=\"help-flow-step\">When trigger falls (1 -> 0): reset response tag for next cycle</div>" +
      "</div>" +
      "<p class=\"hint\">Workflow OFF still requires text for 0004/0005 commands. Job tag alone is not enough for content updates.</p>"
  }
};

function getHelpPageKey() {
  var path = window.location.pathname || "";
  var name = path.substring(path.lastIndexOf("/") + 1);
  if (!name || name === "") {
    return "index.html";
  }
  return name;
}

function ensureHelpModal() {
  if (document.getElementById("pageHelpModal")) {
    return;
  }
  var overlay = document.createElement("div");
  overlay.className = "modal-overlay";
  overlay.id = "pageHelpModal";
  overlay.setAttribute("aria-hidden", "true");
  overlay.innerHTML =
    "<div class=\"modal modal-wide help-modal\" role=\"dialog\" aria-labelledby=\"pageHelpTitle\" aria-modal=\"true\">" +
      "<div class=\"modal-header\">" +
        "<h2 id=\"pageHelpTitle\">Help</h2>" +
        "<button class=\"modal-close\" type=\"button\" onclick=\"hidePageHelp()\" aria-label=\"Close\">&times;</button>" +
      "</div>" +
      "<div class=\"modal-content help-content\" id=\"pageHelpBody\"></div>" +
      "<div class=\"modal-footer actions\">" +
        "<button class=\"button primary\" type=\"button\" onclick=\"hidePageHelp()\">Close</button>" +
      "</div>" +
    "</div>";
  document.body.appendChild(overlay);
  overlay.addEventListener("click", function(evt) {
    if (evt.target === overlay) {
      hidePageHelp();
    }
  });
  document.addEventListener("keydown", function(evt) {
    if (evt.key === "Escape") {
      hidePageHelp();
    }
  });
}

function openPageHelp() {
  ensureHelpModal();
  var key = getHelpPageKey();
  var page = GW_HELP[key];
  if (!page) {
    return;
  }
  document.getElementById("pageHelpTitle").textContent = page.title;
  document.getElementById("pageHelpBody").innerHTML = page.html;
  var modal = document.getElementById("pageHelpModal");
  modal.classList.add("show");
  modal.setAttribute("aria-hidden", "false");
  document.body.style.overflow = "hidden";
}

function hidePageHelp() {
  var modal = document.getElementById("pageHelpModal");
  if (!modal) {
    return;
  }
  modal.classList.remove("show");
  modal.setAttribute("aria-hidden", "true");
  if (!document.querySelector(".modal-overlay.show")) {
    document.body.style.overflow = "";
  }
}

ensureHelpModal();
