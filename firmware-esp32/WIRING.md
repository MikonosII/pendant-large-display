# Wiring — CrowPanel ↔ grblHAL ↔ CO2 PSU

**Tunable pin / port numbers (no rebuild):** edit **`/settings.cfg`** on the TF/SD card (preferred) or LittleFS. Set `board crowpanel-advance-5` (or `-7`) and optional overrides (`laser.enable_dout`, `uart.*`, …). Template: [`data/settings.cfg`](data/settings.cfg). Flash filesystem once with `pio run -t uploadfs` if you use LittleFS only.

Three boxes. The pendant only talks **UART** to the controller. The controller alone drives the laser PSU (**K+** enable + **IN** PWM).

```text
┌─────────────────────┐         UART 115200          ┌──────────────────────┐
│  CrowPanel Advance  │◄────────────────────────────►│  grblHAL controller  │
│  5″ (ST7262)        │   TX↔RX, GND, (no 24 V!)     │  (SKR / etc.)        │
│  ESP32-S3 pendant   │                              │                      │
└─────────────────────┘                              │  Spindle PWM ────────┼──► PSU IN
                                                     │  Digital out Pn ─────┼──► K+ enable
                                                     │  GND ────────────────┼──► PSU G
                                                     └──────────────────────┘
                                                                        │
                                                                        ▼
                                                         ┌──────────────────────────┐
                                                         │  CO2 laser power supply  │
                                                         │  IN  = power (PWM/0–5 V) │
                                                         │  K+  = enable (→GND=on)  │
                                                         │  G   = signal ground     │
                                                         └──────────────────────────┘
```

## 1. CrowPanel ↔ controller (UART)

CrowPanel **UART1-OUT** (PH2.0-4P) → controller UART (TTL **3.3 V only**).

| CrowPanel | Controller | Notes |
|-----------|------------|--------|
| **TX** (IO20) | **RX** | Pendant → grblHAL |
| **RX** (IO19) | **TX** | grblHAL → pendant |
| **GND** | **GND** | Common signal ground |
| — | — | Do **not** feed 5 V / 24 V into the CrowPanel UART header |

* Baud **115200** 8N1 (plain ASCII default; framed optional in SETTINGS).
* Rear DIP: leave UART1 free (not mic/I2S on IO19/IO20).
* Power CrowPanel from its own USB-C / 5 V supply — not from the laser PSU HV side.

## 2. Controller ↔ laser PSU

Assumes a common LPS where **shorting K+ to GND** arms/enables the beam (your “K+ to ground” path), and **IN** sets power.

| Controller function | Typical grblHAL signal | PSU terminal | Pendant command |
|---------------------|------------------------|--------------|-----------------|
| Signal ground | GND | **G** (and often K−) | — |
| Laser power | Spindle **PWM** | **IN** | `M3 S…` / `M5` (FEED LASER ON/OFF) |
| Laser enable | Digital out **Pn** | **K+** (via relay / open-drain to GND) | `M64 Pn` / `M65 Pn` |

Runtime default: `laser.enable_dout 0` → **`M64 P0` / `M65 P0`**.  
Map grblHAL digital output **0** to the pin that drives the K+ path, or change `laser.enable_dout` in `/settings.cfg` and reboot.

### K+ enable circuit (required)

Do **not** tie a 5 V MCU pin straight into K+ unless the PSU docs say that pin is a logic input. For “K+ to ground = on”:

```text
  Controller DOUT (active) ──► relay coil / N-FET gate
                                    │
  PSU K+ ─────► (closed when DOUT active) ─────► PSU G / K− / GND
```

* **LASER TEST** (MPG): toggles **only** this path (`M64`/`M65`).
* **LASER ON** (FEED): `M64` + `M3` at the FEED % → S.
* **LASER OFF**: `M65` + `M5`.

### IN (power)

* Controller spindle PWM → PSU **IN** (and PWM GND → PSU **G**).
* Level: many LPS want **0–5 V** PWM; use a level shifter if the MCU PWM is 3.3 V and the supply is picky.
* `$30` should match your max S (firmware treats **100% → S = $30**, default UI assumes 1000).

### Other PSU terminals (machine side, not pendant)

Wire per your supply (names vary: TH/TL/WP vs H/L/P):

| Terminal | Typical role |
|----------|----------------|
| **WP / P** | Water protect — often must be satisfied (GND or closed switch) or laser will not fire |
| **TH / H** or **TL / L** | Alternate enable polarities on some supplies — unused if you drive **K+** as enable |
| **AC / HV** | Mains and tube — follow PSU manual; earth chassis properly |

## 3. What each UI action drives

| UI | Bus | Bytes / G-code |
|----|-----|----------------|
| Status / DRO | UART | `?` → `<…\|FS:…>` |
| MPG claim | UART | realtime `0x8B` |
| Jog / hold / start | UART | `$J=…` / `!` / `~` |
| LASER TEST | UART → dout | `M64 P0` / `M65 P0` |
| FEED LASER ON | UART → dout + PWM | `M64 P0` + `M3 S…` |
| FEED LASER OFF | UART → dout + PWM | `M65 P0` + `M5` |

## 4. Still to confirm on your bench

1. **Controller board** (exact model) and which physical pin is digital out **P0** (or chosen `n`).
2. **PSU model** — photo of the control connector if labels differ from K+ / IN / G.
3. **Active level** — confirm K+→GND enables (vs TH high / TL low on other supplies).
4. Whether **WP** is already jumpered/satisfied so enable + PWM can actually fire.

Once those four are known, the diagram above can be filled with concrete pin names (e.g. `EXP1-08`, `Spindle PWM`, etc.).
