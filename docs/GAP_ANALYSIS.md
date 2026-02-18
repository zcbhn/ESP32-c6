# RBMS Gap Analysis

> Date: 2026-02-19 (Server Gateway Update)
> Version: v1.0.0-rc2 (35 audit issues + server gateway + OTBR setup)

---

## 1. Overall Status

```
██████████ Build/Structure      100%  OK
██████████ Preset JSON           100%  OK
██████████ Firmware              100%  OK  (31/31 files, Type A/B build success)
██████████ Server                100%  OK  (Mosquitto, InfluxDB, Grafana, Bridge)
██████████ GitHub CI/Build       100%  OK  (Type A/B parallel build + artifacts)
██████████ Code Quality/Safety   100%  OK  (35 audit issues + 16 web improvements)
███░░░░░░░ Documentation          30%  !!  (PDF not generated)
░░░░░░░░░░ Hardware Design         0%  XX  (SVG schematic excluded)
```

---

## 2. Firmware Component Details

### 2.1 All Components Implemented

| Component | Files | Status | Key Features |
|-----------|-------|--------|-------------|
| **sensor** | `sht30.c/h` | OK | I2C temp/humidity, CRC-8, **I2C bus recovery (9-clock)**, auto retry |
| **sensor** | `ds18b20.c/h` | OK | 1-Wire dual sensor, GPIO power control, **interrupt protection**, **rom[] init** |
| **actuator** | `ssr.c/h` | OK | Cycle Skipping (10s period), 2 channels |
| **actuator** | `pwm_dimmer.c/h` | OK | LEDC **XTAL_CLK**/10-bit, fade, **thread-safe API** |
| **control** | `pid.c/h` | OK | **Derivative-on-measurement**, **back-calculation anti-windup** |
| **control** | `scheduler.c/h` | OK | System time, **overnight (midnight-crossing) schedule**, sunrise/sunset transition |
| **control** | `adaptive_poll.c/h` | OK | Temperature delta adaptive polling (30-300s), **zero-division guard** |
| **safety** | `safety_monitor.c/h` | OK | Overtemp/sensor fault/**stale timeout**/**dual sensor mismatch**/**rate monitor** |
| **comm** | `thread_node.c/h` | OK | OpenThread Router/SED, **lock-based thread safety**, **re-init safe**, **child timeout** |
| **comm** | `cbor_codec.c/h` | OK | Lightweight CBOR integer key encoder/decoder, **bounds checking** |
| **power** | `power_mgmt.c/h` | OK | Deep Sleep, **GPIO hold**, timer wakeup, **RTC domain power-down** |
| **power** | `battery_monitor.c/h` | OK | ADC curve-fitting calibration, **16x multisampling**, **ADC error handling** |
| **config** | `nvs_config.c/h` | OK | NVS blob read/write, **deinit support** |
| **config** | `preset_manager.c/h` | OK | Preset NVS storage, **NVS load validation** |
| **main** | `app_main.c` | OK | NVS init, node type dispatch |
| **main** | `app_type_a.c` | OK | FreeRTOS multi-task, **volatile shared vars**, **NaN guard**, **WDT subscription** |
| **main** | `app_type_b.c` | OK | Deep Sleep sequential loop |

### 2.2 Build Results

| Type | Binary Size | Partition Free | Errors | Warnings |
|------|------------|----------------|--------|----------|
| Type A (Router) | 179 KB | 91% (1.8MB OTA) | 0 | 0 |
| Type B (SED) | 179 KB | 91% (1.8MB OTA) | 0 | 0 |

### 2.3 Code Audit Results (35 Issues)

| Severity | Found | Fixed | Status |
|----------|-------|-------|--------|
| CRITICAL | 4 | 4 | 100% |
| HIGH | 9 | 9 | 100% |
| MEDIUM | 15 | 15 | 100% |
| LOW | 7 | 7 | 100% |

### 2.4 Web-Based Improvements Applied

| Item | Source | Status |
|------|--------|--------|
| OpenThread lock acquire/release | ESP-IDF docs | Applied |
| ADC 16x multisampling + error handling | ESP-IDF ADC guide | Applied |
| PID derivative-on-measurement | Control engineering best practice | Applied |
| PID back-calculation anti-windup | Control engineering best practice | Applied |
| SHT30 I2C bus recovery (9-clock) | I2C spec, Sensirion app note | Applied |
| Safety stale reading timeout | IoT safety design pattern | Applied |
| Safety dual sensor cross-validation | Industrial safety standard | Applied |
| Safety temperature rate monitoring | Physical anomaly detection | Applied |
| Deep Sleep GPIO hold + RTC power-down | ESP32-C6 Deep Sleep guide | Applied |
| NVS preset validation | Embedded data integrity | Applied |
| 1-Wire interrupt protection (portCRITICAL) | RISC-V timing protection | Applied |
| Watchdog timer (10s, panic reset) | ESP32-C6 WDT guide | Applied |
| LEDC XTAL_CLK + thread-safe API | ESP-IDF LEDC docs | Applied |
| Custom partition table (no factory) | ESP-IDF partition guide | Applied |
| OpenThread child timeout (300s) | OpenThread SED best practice | Applied |
| Server security hardening | IoT security best practice | Applied |

---

## 3. Server Configuration

### 3.1 Implemented

| File | Description |
|------|-------------|
| `server/mosquitto/mosquitto.conf` | MQTT broker (auth, ACL, logging) |
| `server/mosquitto/acl.conf` | Topic-based access control |
| `server/grafana/provisioning/datasources/influxdb.yml` | InfluxDB datasource |
| `server/grafana/provisioning/dashboards/dashboard.yml` | Dashboard provisioning |
| `server/grafana/dashboards/rbms_overview.json` | 8-panel dashboard |
| `server/bridge/mqtt_influx_bridge.py` | CBOR-to-InfluxDB bridge (env-based auth) |
| `server/bridge/requirements.txt` | Python dependencies |
| `server/systemd/rbms-bridge.service` | systemd service (EnvironmentFile) |
| `server/scripts/setup.sh` | 9-step auto install (InfluxDB auth + UFW firewall) |
| `server/gateway/thread_mqtt_gateway.py` | Thread UDP→MQTT gateway (CBOR forwarding) |
| `server/gateway/requirements.txt` | Gateway Python dependencies |
| `server/systemd/rbms-gateway.service` | Gateway systemd service |
| `server/border-router/setup_otbr.sh` | OpenThread Border Router install script |
| `server/border-router/README.md` | OTBR setup guide (RCP flash, network formation) |

---

## 4. Remaining Work

### 4.1 Hardware (Not Started)

| Path | Type | Priority |
|------|------|----------|
| `hardware/kicad/` | KiCad PCB design | P2 |
| `hardware/3d-case/` | 3D print STL | P3 |
| `hardware/reference/` | Datasheet links | P3 |

### 4.2 Documentation (Partial)

| Item | Status | Priority |
|------|--------|----------|
| `docs/RBMS-SPEC-001_spec.pdf` | Not generated | P2 |
| `docs/RBMS-DEV-001_roadmap.pdf` | Not generated | P2 |
| `docs/RBMS-HW-001_hardware.pdf` | Not generated | P2 |
| `CHANGELOG.md` | Not generated | P3 |

### 4.3 CI/CD Improvements

| Item | Status | Priority |
|------|--------|----------|
| Build artifact upload | Implemented | - |
| Static analysis (cppcheck) | Not implemented | P2 |
| Unit tests (Unity) | Not implemented | P2 |
| Release automation | Not implemented | P3 |

### 4.4 Future Firmware Improvements

| Item | Status | Priority |
|------|--------|----------|
| DS18B20 RMT driver migration | Not implemented (GPIO bit-bang OK with portCRITICAL) | P2 |
| OTA firmware update support | Partition table ready, code not implemented | P2 |
| InfluxDB TLS encryption | Not implemented | P2 |

---

## 5. Risk Items

| Risk | Impact | Mitigation | Status |
|------|--------|-----------|--------|
| OpenThread SED stability | Type B battery life | Lock-based thread safety + child timeout | Mitigated |
| DS18B20 1-Wire timing | RISC-V sensor read failure | portCRITICAL interrupt protection | Mitigated |
| SSR Cycle Skipping EMI | EMI interference | Zero-crossing SSR review needed | Warning |
| NVS write lifetime | Flash wear | Write minimization applied | Mitigated |
| NVS data corruption | Preset error | Validation + default fallback | Mitigated |
| Battery self-discharge | 6-month target | GPIO hold, adaptive polling, RTC power-down | Mitigated |
| I2C bus hang | Sensor read failure | 9-clock bus recovery | Mitigated |
| PID setpoint kick | Temperature oscillation | Derivative-on-measurement | Mitigated |
| PID output NaN | Heater runaway | isnanf guard | Mitigated |
| Task hang/deadlock | Heater stuck ON | Watchdog timer (10s, panic reset) | Mitigated |

---

## 6. Summary Statistics

- **Total source files**: 31
- **Implementation complete**: 31 (100%)
- **Header API declarations**: 16 / 16 (100%)
- **Build success**: Type A OK, Type B OK
- **Server implementation**: 100% (14 files)
- **Code audit**: 35/35 issues fixed (100%)
- **Web improvements**: 16 items applied
- **Missing directories**: 3 (hardware)
- **Missing documents**: 3 PDF
- **Data pipeline**: Complete (Thread UDP → Gateway → MQTT → Bridge → InfluxDB → Grafana)
