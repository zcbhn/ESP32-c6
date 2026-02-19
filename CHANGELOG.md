# Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased]

### Added
- OTA firmware update component (`ota_update.c/h`)
- Unit tests for core components (PID, CBOR, scheduler, adaptive_poll)
- cppcheck static analysis in CI/CD pipeline
- Release automation workflow (tag-triggered)
- Hardware reference documents (BOM, pinout, design notes)
- System specification document (RBMS-SPEC-001)
- Development roadmap document (RBMS-DEV-001)
- Hardware design reference document (RBMS-HW-001)

### Fixed
- Bridge `flush_buffer()` data loss: InfluxDB 쓰기 실패 시 버퍼 유지 및 재시도 (최대 3회)
- Bridge MQTT 재연결 시 `reconnect_delay_set` 및 `on_disconnect` 콜백 추가
- Gateway UDP 소켓: wpan0 인터페이스 복구 시 자동 소켓 재생성 및 멀티캐스트 재가입
- Bridge 버퍼 무한 증가 방지 (최대 1000 포인트, 초과 시 오래된 데이터 삭제)

### Security
- MQTT 비밀번호 하드코딩 제거: `MQTT_PASS` 환경변수 필수 (미설정 시 즉시 종료)
- `setup.sh` MQTT/Grafana 비밀번호 `openssl rand`로 랜덤 생성
- MQTT TLS 지원 추가 (`MQTT_TLS`, `MQTT_CA_CERT` 환경변수)
- Mosquitto TLS 리스너 설정 추가 (8883 포트)
- Grafana admin 비밀번호 자동 변경 (`grafana-cli`)
- 통합 TLS 문서 (MQTT + InfluxDB)

## [1.0.0-rc2] - 2026-02-19

### Added
- Thread UDP to MQTT gateway (`thread_mqtt_gateway.py`)
- OpenThread Border Router install script (`setup_otbr.sh`)
- Border Router setup guide with RCP flash instructions
- Gateway systemd service (`rbms-gateway.service`)
- Complete data pipeline: Thread UDP -> Gateway -> MQTT -> Bridge -> InfluxDB -> Grafana

### Changed
- `setup.sh` expanded to 9-step install (added gateway)
- Generalized server setup from "Raspberry Pi" to "Linux server"
- README updated with full server architecture diagram

## [1.0.0-rc1] - 2026-02-18

### Added
- ESP32-C6 firmware with 31 source files (17 .c + 14 .h)
- Type A (Router): FreeRTOS multi-task, PID control, SSR, PWM dimming
- Type B (SED): Deep Sleep sequential loop, adaptive polling (30-300s)
- Sensor drivers: SHT30 (I2C, CRC-8, bus recovery), DS18B20 (1-Wire, portCRITICAL)
- Actuator drivers: SSR cycle skipping (10s period), LEDC PWM (XTAL_CLK, 10-bit)
- PID controller: derivative-on-measurement, back-calculation anti-windup
- Scheduler: overnight (midnight-crossing) support, sunrise/sunset transitions
- Safety monitor: overtemp, sensor fault, stale timeout, dual sensor mismatch, rate monitor
- OpenThread communication: Router/SED, lock-based thread safety, child timeout (300s)
- CBOR codec: lightweight integer key encoder/decoder
- Power management: deep sleep, GPIO hold, RTC domain power-down
- Battery monitor: ADC 16x multisampling, curve-fitting calibration
- NVS config with deinit support
- Preset manager with NVS validation and default fallback
- Watchdog timer (10s, panic reset) on all FreeRTOS tasks
- Custom partition table: no factory, OTA 1.8MB x 2
- Species presets: Ball Python, Leopard Gecko, Crested Gecko, Corn Snake
- Server: Mosquitto MQTT (auth, ACL), InfluxDB, Grafana (8-panel dashboard)
- MQTT-InfluxDB bridge with env-based credentials
- Server setup script with InfluxDB auth users and UFW firewall
- GitHub Actions CI/CD: Type A/B parallel build with artifact upload
- SVG schematics (5 sheets)
- Development tools (build, flash, monitor scripts)

### Fixed
- 35 code audit issues (4 CRITICAL, 9 HIGH, 15 MEDIUM, 7 LOW)

### Security
- Environment-based credentials (no hardcoded secrets)
- InfluxDB role-based access (admin, writer, reader)
- MQTT ACL with per-user topic restrictions
- UFW firewall (SSH, Grafana, MQTT only)

[Unreleased]: https://github.com/zcbhn/ESP32-c6/compare/v1.0.0-rc2...HEAD
[1.0.0-rc2]: https://github.com/zcbhn/ESP32-c6/compare/v1.0.0-rc1...v1.0.0-rc2
[1.0.0-rc1]: https://github.com/zcbhn/ESP32-c6/releases/tag/v1.0.0-rc1
