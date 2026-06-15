# CIP Data Gateway

**Industrial print gateway firmware for Allen-Bradley Micro800 PLCs and REAJet inkjet printers**

| | |
|---|---|
| **Platform** | NetBurner MOD5441X ([MOD54415](https://www.netburner.com/products/system-on-modules/mod5441x/) / [MOD54417](https://www.netburner.com/products/system-on-modules/mod5441x/)) |
| **Toolchain** | NetBurner NNDK 3.5.x · NBEclipse |
| **Protocols** | EtherNet/IP (CIP) · REA-PLC TCP v1.54 · REA-PI XML |
| **License** | [MIT](LICENSE) |

The gateway runs on a NetBurner module between plant-floor equipment: it reads EtherNet/IP tags from a Micro800 PLC, executes REA-PLC print commands against a REAJet printer, and exposes a browser-based configuration console. Configuration is stored in flash and survives reboots.

Both **MOD54415** (single Ethernet) and **MOD54417** (dual Ethernet) use the same firmware build. Dual-port network options appear in the web UI only on MOD54417 hardware.

---

## Overview

On each **rising edge** of a configured BOOL trigger tag, the gateway:

1. Reads job name and print text values from the PLC
2. Runs the selected REA-PLC workflow (full job-change sequence or a single command)
3. Writes acknowledgement and error codes back to PLC tags
4. Optionally mirrors printer status (speed, job state, triggers) via REA-PI polling

```
  ┌─────────────────┐         ┌──────────────────────┐         ┌─────────────────┐
  │  Micro800 PLC   │  CIP    │  NetBurner Gateway   │  TCP    │  REAJet Printer │
  │  (EtherNet/IP)  │◄───────►│  CIP Data Gateway    │◄───────►│  REA-PLC / REA-PI│
  │                 │  tags   │  MOD5441X module     │ 22170/71│                 │
  └─────────────────┘         └──────────┬───────────┘         └─────────────────┘
                                         │
                                         │ HTTP :80
                                         ▼
                              ┌──────────────────────┐
                              │  Configuration UI    │
                              │  (embedded web pages)│
                              └──────────────────────┘
```

| Component | Role |
|-----------|------|
| **Micro800 PLC** | Source of job name, print text, trigger bit, and acknowledgement tags |
| **NetBurner gateway** | EtherNet/IP client, mapping engine, HTTP configuration server |
| **REAJet printer** | REA-PLC command target (TCP **22170** default; **22169** with EOT framing); optional REA-PI status (TCP **22171**) |

---

## Key capabilities

- **Micro800 discovery** — UDP/EtherNet/IP scan with Micro800 candidate detection (2080/2085 catalog prefixes)
- **Tag browse and import** — Live PLC tag browse, UDT expansion, and CSV import (`DEMO.csv` included as a reference export)
- **Print mappings** — Up to **64** mappings; BOOL trigger edge detection; configurable poll interval (default **50 ms**, range 10–5000 ms)
- **Job-change workflow** — Automated sequence: Stop (0003) → Assign Job (0001) → Set Content (0004/0005) → Start (0002) → GETSTATUS verification
- **REA-PI status mirror** — Optional XML session for encoder speed, job state, and installation activity; writeback to configurable PLC status tags
- **Dual-Ethernet support** — MOD54417 bridged or independent port modes for segregated PLC/printer subnets
- **Persistent configuration** — NetBurner `config_obj` flash storage for network, devices, tags, and mappings

---

## Repository layout

```
Micro800_ReaJet_Gateway/
├── README.md                 Project documentation (this file)
├── DEMO.csv                  Example UDT/tag export for PLC browse import
└── BurnerGateway/            NBEclipse project — import this folder into Eclipse
    ├── html/                 Web UI (compiled into firmware via comphtml)
    │   ├── index.html        Configuration console home
    │   ├── network.html      IPv4 and dual-Ethernet settings
    │   ├── devices.html      PLC selection and REAJet destination
    │   ├── mapping.html      Tag list, mappings, live runtime
    │   ├── status.html       Manual REA-PLC / REA-PI probe
    │   ├── style.css         Shared styles
    │   └── help.js           Context-sensitive help content
    ├── src/
    │   ├── main.cpp          Application entry; include-based modular runtime
    │   ├── core/             Config, network wait, live status, connection state
    │   ├── enip/             EtherNet/IP (Micro800 scan, tag read/write)
    │   ├── reajet/           REA-PLC framing, REA-PI session, status polling
    │   ├── mapping/          Print mapping scanner and job-change workflow
    │   └── http/             REST-style JSON API handlers and route registration
    ├── Release/              Build outputs (BurnerGateway.bin, .s19, .elf, htmldata.cpp)
    ├── makefile.targets      Custom targets (app-s19, comphtml, load-nbimage)
    └── .settings/            Platform IP, MOD5441X device options
```

> **Note:** Eclipse workspace metadata under `.metadata/` is local IDE state. It is not required to build on another machine if you import the `BurnerGateway` project fresh.

---

## Requirements

| Requirement | Details |
|-------------|---------|
| NetBurner NNDK | **3.5.x** (default install path: `C:\nburn`) |
| IDE | **NBEclipse** (NetBurner plug-in for Eclipse) |
| Target hardware | **MOD5441X** platform — **MOD54415** or **MOD54417** |
| Network | Module must reach Micro800 and REAJet on configured subnets |
| First-time flash | Legacy [AutoUpdate (Windows)](https://www.netburner.com/download/autoupdate-windows/) and `*_APP.s19` image (see [Deploy](#deploy-firmware)) |

---

## Build

1. Open Eclipse with NBEclipse and import **BurnerGateway** (`File → Import → Existing Projects into Workspace`).
2. Confirm project platform: **MOD5441X** (`Project → Properties → NetBurner` or `.settings/nbeclipse.core.prefs`).
3. Select the **Release** configuration.
4. **Project → Build Project**.

### Build outputs

| File | Purpose |
|------|---------|
| `BurnerGateway.bin` | NetBurner 3.x image for `loadapp` / config server (port 20034) |
| `BurnerGateway.s19` | Raw S-record from objcopy — **not** for AutoUpdate |
| `BurnerGateway_APP.s19` | Legacy AutoUpdate image (see below) |
| `BurnerGateway.elf` / `.map` | Debug symbols and size analysis |
| `htmldata.cpp` | Generated embed of `html/` assets |

After editing any file under `html/`, rebuild **Release** so `comphtml` regenerates the embedded web pages.

### AutoUpdate image (first flash on factory MOD5441x)

Normal Release builds produce `.bin` only. For **AutoUpdate**, build `BurnerGateway_APP.s19`:

- **Eclipse:** Build Targets → **Build AutoUpdate S19**, or
- **Shell** (from `BurnerGateway/Release`): `make app-s19`

Use **`BurnerGateway_APP.s19`** in AutoUpdate — not `BurnerGateway.s19`. The raw S19 lacks the platform header AutoUpdate expects.

---

## Deploy firmware

### First load (factory app / new MOD5441x module)

Factory modules ship with a 2.x demo application. **Eclipse "Run as NetBurner Application" will fail** until a 3.x app is installed (the config server on port **20034** is not available on the factory app).

1. Build Release, then run **Build AutoUpdate S19**.
2. Open [AutoUpdate (Windows)](https://www.netburner.com/download/autoupdate-windows/), select the device, and load `Release/BurnerGateway_APP.s19`.
3. **Alternative:** Serial boot monitor (`A` at boot or recovery jumper **TP1**), command `fla`, then send `BurnerGateway.bin`.

On older 5441x modules, you may need `MOD5441X-3p0-Update_APP.s19` from `C:\nburn\platform\MOD5441X\original\` before loading this application.

### Updates (after BurnerGateway is running)

1. Set device IP in project NetBurner settings (or `DEVIP=` on the make command line).
2. **Build Targets → Run As NetBurner Application** (posts `BurnerGateway.bin` to port 20034), or from `Release`:

   ```bat
   make loadapp LOAD_TARGET=NBIMAGE NBIMAGE=BurnerGateway.bin DEVIP=<device-ip>
   ```

---

## Commissioning

Open **`http://<gateway-ip>/`** in a browser. Each configuration page includes a **Help** button with field-level documentation.

### Recommended sequence

| Step | Page | Actions |
|------|------|---------|
| 1 | Network Configuration (`/network.html`) | Set DHCP or static IPv4. **MOD54417 only:** choose bridged ports (single IP) or independent ports (separate PLC/printer subnets). **Save & Reboot** when changing IP or port mode. |
| 2 | Device Configuration (`/devices.html`) | **Scan Micro800 Devices**, select the PLC. Enter REAJet IP, port (default **22170**), mapping scan interval (default **50 ms**), and optional REA-PI settings. **Save REAJet Settings**. |
| 3 | Print Mappings (`/mapping.html`) | **Browse PLC Tags** or import UDT CSV (see `DEMO.csv`). **Add Mapping**: job tag, text tag, BOOL trigger, response/error tags, REA target, job-change workflow. Enable mappings and confirm runtime status (PLC / REAJet / Scanner indicators). |
| 4 | REAJet Status Probe (`/status.html`) | Optional one-shot REA-PLC / REA-PI diagnostics without changing saved configuration. |

---

## Web UI

| Page | URL | Description |
|------|-----|-------------|
| Home | `/` | Navigation to all configuration areas |
| Network | `/network.html` | IPv4 addressing; dual-Ethernet options on MOD54417 |
| Devices | `/devices.html` | PLC selection, REAJet destination, REA-PI options |
| Print Mappings | `/mapping.html` | Tag list, mapping editor, live runtime telemetry |
| Status Probe | `/status.html` | Manual REA-PLC / REA-PI connection test |

---

## Runtime architecture

### Services and ports

| Service | Port | Description |
|---------|------|-------------|
| HTTP / Web UI | **80** | Static pages and JSON API under `/api/...` |
| Config server | **20034** | Firmware update (`BurnerGateway.bin`) after 3.x app is loaded |
| REA-PLC | **22170** (default) | ASCII command channel to REAJet printer |
| REA-PLC (EOT) | **22169** | Same protocol with EOT-terminated framing |
| REA-PI | **22171** | XML status and event subscription channel |

### Firmware modules

Source is organized as include-based translation units from `main.cpp` (`core`, `enip`, `reajet`, `mapping`, `http`) to keep NetBurner `config_obj` instances in a single link unit.

| Module | Responsibility |
|--------|----------------|
| `core` | Persistent config, network readiness, PLC/REAJet connection state, live status |
| `enip` | EtherNet/IP device scan, tag browse, read/write |
| `reajet` | REA-PLC command framing (0001–0008), REA-PI session, GETSTATUS polling |
| `mapping` | Trigger scanner, job-change workflow, PLC writeback |
| `http` | Route registration and JSON API handlers |

### Main loop

After boot, the firmware waits for network (DHCP/AutoIP fallback), reconnects to stored PLC and REAJet targets, then cycles:

- PLC and REAJet connection retry (15 s interval)
- REA GETSTATUS poll (default **500 ms**, configurable; 0 = disabled)
- REA-PI event service (when enabled)
- Mapping scanner at the configured poll interval

### REA-PLC commands

| Code | Command | Typical use |
|------|---------|-------------|
| 0001 | Assign Job | Load job file on printer |
| 0002 | Start Job | Release job for printing |
| 0003 | Stop Job | Stop current job |
| 0004 | Set Label Contents | Update print text (content) |
| 0005 | Set Label Object | Update label object properties |
| 0006 | Change Character Set | Character set selection |
| 0007 | Set EOT | End-of-transmission configuration |
| 0008 | Get Status | Query printer job state |

### Job-change workflow

When **Job Change Workflow** is enabled on a mapping (commands 0004 or 0005), the gateway executes:

```
0003 Stop → 0001 Assign Job → 0004/0005 Set Content → 0002 Start → GETSTATUS verify
```

With workflow disabled, the gateway sends assign + content updates only (no stop/start cycle). Successful responses write to the mapping's response tag; failures write to the error tag.

---

## Supported hardware

Product reference: [MOD5441X System-on-Module](https://www.netburner.com/products/system-on-modules/mod5441x/)

| Module | Ethernet | Notes |
|--------|----------|-------|
| **MOD54415** | Single RJ-45 | Full gateway feature set; one IP; PLC and REAJet on the same LAN (or via plant routing) |
| **MOD54417** | Dual RJ-45 | Same firmware; optional bridged or independent port modes |

Both variants have been tested in production-style Micro800 + REAJet installations.

### MOD54417 dual Ethernet

| Mode | Behavior |
|------|----------|
| **Bridge both ports** (default) | Single LAN, single IP; connect PLC and REAJet to either RJ-45 |
| **Independent ports** | Ethernet0 and Ethernet1 each have their own IP (configure both on Network page; reboot required) |

Uncheck **Bridge both Ethernet ports** on the Network page to split subnets (e.g. PLC on `192.168.1.x`, printer on `192.168.10.x`).

---

## HTTP API

All routes are registered in `BurnerGateway/src/http/http_register.cpp`. Parameters are passed as query strings on GET requests (embedded-server friendly).

### Network

| Method | Endpoint | Description |
|--------|----------|-------------|
| GET | `/api/network/config` | Read IPv4 and dual-Ethernet settings |
| GET | `/api/network/config/save` | Save network configuration |

### PLC

| Method | Endpoint | Description |
|--------|----------|-------------|
| GET | `/api/plc/scan` | Discover EtherNet/IP devices on the network |
| GET | `/api/plc/save/{index}` | Select a discovered PLC |
| GET | `/api/plc/selected` | Read currently selected PLC |
| GET | `/api/plc/clear` | Clear PLC selection |
| GET | `/api/plc/read` | Read a tag value |
| GET | `/api/plc/tags` | List configured tag paths |
| GET | `/api/plc/tags/browse` | Browse tags on connected PLC |
| GET | `/api/plc/tags/add` | Add tag to configuration |
| GET | `/api/plc/tags/import/*` | CSV import staging and commit |
| GET | `/api/plc/tags/expand` | Expand UDT member paths |
| GET | `/api/plc/tags/remove/{index}` | Remove a tag |

### REAJet

| Method | Endpoint | Description |
|--------|----------|-------------|
| GET | `/api/reajet/config` | Read REAJet and REA-PI settings |
| GET | `/api/reajet/config/save` | Save REAJet settings |
| GET | `/api/reajet/config/clear` | Clear REAJet configuration |
| GET | `/api/reajet/status` | Live printer status |
| GET | `/api/reajet/manual_status` | One-shot status probe |
| GET | `/api/reajet/send` | Send manual REA-PLC command |

### Mappings

| Method | Endpoint | Description |
|--------|----------|-------------|
| GET | `/api/mappings/list` | List all print mappings |
| GET | `/api/mappings/save` | Create or update a mapping |
| GET | `/api/mappings/delete` | Remove a mapping |
| GET | `/api/mappings/clear` | Clear all mappings |
| GET | `/api/mappings/runtime` | Live scanner state and last transaction details |

---

## Troubleshooting

| Symptom | Likely cause | Resolution |
|---------|--------------|------------|
| `loadapp` connection timeout | Factory 2.x app still running | Use AutoUpdate + `BurnerGateway_APP.s19` for first flash |
| AutoUpdate "platform does not match" | Wrong image file | Use `BurnerGateway_APP.s19`, not `BurnerGateway.s19` |
| PLC scan returns empty | Wrong subnet, firewall, or no Micro800 present | Verify network, VLAN, and that the PLC is powered and connected |
| REAJet TCP connect failed | Wrong IP/port, printer offline, or routing issue | Confirm REAJet IP, port 22170/22169, and dual-port routing on MOD54417 |
| Mappings never fire | Trigger not BOOL, mapping disabled, or scanner stopped | Check trigger tag type, mapping enable flag, and runtime status on Mapping page |
| Web UI changes not visible | HTML not re-embedded | Rebuild Release after editing files in `html/` |
| REA-PI commands empty | Printer session gate / TargetGUI setting | See Status Probe diagnostics; verify REA-PI is enabled on the printer |

Serial debug output is available on the module console at boot (network info, mapping traces when diagnostics are enabled).

---

## Related documentation

| Resource | Location |
|----------|----------|
| MOD5441X product page | [netburner.com/products/system-on-modules/mod5441x](https://www.netburner.com/products/system-on-modules/mod5441x/) |
| AutoUpdate (Windows) | [netburner.com/download/autoupdate-windows](https://www.netburner.com/download/autoupdate-windows/) |
| MOD5441x recovery | `C:\nburn\docs\NetBurner\Developer\html\page_platform_ref-_m_o_d5441x_recovery.html` |
| 5441x migration guide | `page_migration_guides-_upgrade5441x.html` (same docs tree) |

---

## License

Application source in `BurnerGateway/src/` and `BurnerGateway/html/` is released under the [MIT License](LICENSE).

Copyright (c) 2026 Adam G. Sweeney

The NetBurner SDK and runtime libraries used to build this firmware remain subject to [NetBurner license terms](https://www.netburner.com) and may only be executed on NetBurner hardware per those terms.
