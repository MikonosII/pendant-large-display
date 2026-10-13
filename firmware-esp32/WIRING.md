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

### IN / spindle PWM (laser power — last hardwire)

The pendant **never** carries PWM. It only sends `M3 S…` / `M5` over UART; **grblHAL’s spindle PWM pin** is what you wire to the PSU.

```text
  grblHAL spindle PWM pin  ──────────────►  PSU IN
  grblHAL GND              ──────────────►  PSU G

  Duty cycle from M3 S{n}  (S0 = 0%, S=$30 = 100%)
  M5 → PWM off
```

| Item | What to do |
|------|------------|
| **Controller pin** | Whatever your grblHAL board map calls **Spindle PWM** / `SPINDLE_PWM_PIN` (SKR, BTT, etc. — check that board’s pin table) |
| **PSU terminal** | **IN** (power / current control). Not K+, not WP. |
| **Ground** | Controller GND ↔ PSU **G** (same ground as the K+ circuit) |
| **Level** | Many LPS want **0–5 V** PWM. If the MCU pin is 3.3 V and the supply is picky, use a 3.3→5 V level shifter on the PWM line only. |
| **grblHAL `$30`** | Max S. Pendant FEED **100%** → `M3 S` = `laser.s_max` in `/settings.cfg` — set that equal to `$30` (default 1000). |
| **Frequency** | Controllers often use ~1–5 kHz spindle PWM; most CO2 LPS accept that on **IN**. Match the PSU manual if it specifies a range. |

**How the two laser wires divide the job**

| Wire | Role | Armed by |
|------|------|----------|
| **K+ → GND** (dout / `M64`) | Enable / “trigger” interlock — beam *allowed* | LASER TEST, or FEED LASER ON |
| **PWM → IN** (`M3 S`) | Power level while allowed | FEED LASER ON only (not LASER TEST) |

So: **LASER TEST** checks the K+ path with PWM off. **FEED LASER ON** closes K+ *and* drives PWM at the FEED %. **LASER OFF** opens K+ (`M65`) and stops PWM (`M5`).

Do **not** put spindle PWM on K+ (that is the enable short-to-ground path). Do **not** put PWM on the CrowPanel.

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

## 4. Bench checklist (fill in pin names)

| # | Confirm | Your value |
|---|---------|------------|
| 1 | Controller board model | ________________ |
| 2 | Digital out for K+ (= `laser.enable_dout`, default P0) | pin ________ / `M64 P_` |
| 3 | Spindle **PWM** pin → PSU **IN** | pin ________ |
| 4 | PSU labels match K+ / IN / G (or note aliases) | ________ |
| 5 | K+→GND enables (vs TH/TL scheme) | yes / no |
| 6 | WP / water interlock satisfied | yes / no |
| 7 | `$30` = `laser.s_max` in settings.cfg | ________ |

Once 2 and 3 are filled, the electrical map is complete.
