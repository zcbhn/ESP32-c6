# 🦎 Reptile Habitat Monitor

**Thread 기반 파충류 사육장 환경 모니터링 & 제어 시스템**

ESP32-C6 + OpenThread + 저전력 배터리 노드로 구성된 오픈소스 파충류 사육 관리 시스템입니다.

[![License: MIT](https://img.shields.io/badge/License-MIT-green.svg)](LICENSE)
[![Platform](https://img.shields.io/badge/Platform-ESP32--C6-blue.svg)]()
[![Protocol](https://img.shields.io/badge/Protocol-Thread%2FOpenThread-orange.svg)]()

---

## 프로젝트 개요

파충류 사육에서 온도·습도 관리는 생존과 직결됩니다. 이 프로젝트는 Thread 메시 네트워크를 활용하여 다수의 사육장을 통합 모니터링하고, 히터/조명을 자동 제어하는 시스템입니다.

### 주요 특징

- **Thread 메시 네트워크** — Wi-Fi 없이 저전력 메시 통신, 노드 추가만으로 확장
- **2가지 노드 타입** — 콘센트 전원 풀노드(Type A) + 배터리 센서노드(Type B)
- **적응형 폴링** — 온도 변화율에 따라 측정 주기를 자동 조절 (30초~5분)
- **배터리 6개월+** — Deep Sleep + GPIO 전원 스위칭 + DC-DC 벅 컨버터
- **PID 온도 제어** — SSR Cycle Skipping 방식, 종별 프리셋 지원
- **일출/일몰 디밍** — MOSFET PWM으로 자연광 시뮬레이션
- **안전 기능** — 과열 차단, 센서 이상 감지, 히터 연속 동작 제한

### 시스템 구성도

```
┌─────────────┐     Thread      ┌─────────────┐
│  Type A     │◄───────────────►│  Type A     │
│  풀 노드     │    (Router)     │  풀 노드     │
│  사육장 #1   │                 │  사육장 #2   │
└──────┬──────┘                 └──────┬──────┘
       │                               │
       │ Thread (mesh)                 │
       │                               │
┌──────┴──────┐                 ┌──────┴──────┐
│  Type B     │                 │  Type B     │
│  배터리 노드  │                 │  배터리 노드  │
│  사육장 #3   │                 │  사육장 #4   │
└─────────────┘                 └─────────────┘
       │
       ▼
┌─────────────────────────────────────────────┐
│  Border Router (Raspberry Pi)               │
│  ┌──────────┐ ┌──────────┐ ┌─────────────┐ │
│  │ Mosquitto│ │ InfluxDB │ │   Grafana   │ │
│  │  (MQTT)  │ │(시계열DB) │ │ (대시보드)   │ │
│  └──────────┘ └──────────┘ └─────────────┘ │
└─────────────────────────────────────────────┘
```

---

## 노드 사양

### Type A — 풀 노드 (콘센트 전원)

| 항목 | 사양 |
|------|------|
| MCU | ESP32-C6-WROOM-1 |
| 전원 | 5V USB 어댑터 → AMS1117-3.3 |
| Thread 역할 | Router (상시 동작) |
| 센서 | SHT30 (I2C) + DS18B20 x2 (1-Wire) |
| 제어 출력 | SSR x2 (히터/UV조명) + MOSFET PWM (LED 디밍) |
| 측정 주기 | 1초 |

### Type B — 배터리 센서 노드

| 항목 | 사양 |
|------|------|
| MCU | ESP32-C6-WROOM-1 |
| 전원 | 21700 배터리 → TP4056 충전 → TPS62740 DC-DC (IQ 0.36µA) |
| Thread 역할 | SED (Sleepy End Device) |
| 센서 | SHT30 (I2C) + DS18B20 x2 (1-Wire, GPIO 전원 스위칭) |
| 측정 주기 | 적응형 30초~300초 |
| 예상 배터리 수명 | 6개월+ (5000mAh 기준) |

---

## 디렉토리 구조

```
reptile-habitat-monitor/
├── README.md
├── LICENSE
├── docs/                          # 설계 문서
│   ├── RBMS-SPEC-001_사양서.pdf
│   ├── RBMS-DEV-001_로드맵_아키텍처.pdf
│   ├── RBMS-HW-001_하드웨어_참조.pdf
│   └── schematics/                # 회로도 SVG
├── firmware/                      # ESP-IDF 펌웨어
│   ├── CMakeLists.txt
│   ├── Kconfig                    # 노드 타입 및 파라미터 설정
│   ├── sdkconfig.defaults
│   ├── main/
│   │   ├── app_main.c             # 엔트리포인트
│   │   ├── app_type_a.c           # Type A 애플리케이션
│   │   └── app_type_b.c           # Type B 애플리케이션
│   └── components/
│       ├── sensor/                # SHT30, DS18B20 드라이버
│       ├── actuator/              # SSR, PWM 디밍 드라이버
│       ├── control/               # PID, 스케줄러, 적응형 폴링
│       ├── safety/                # 안전 감시 모듈
│       ├── comm/                  # Thread 통신, CBOR 코덱
│       ├── power/                 # 전원 관리, 배터리 모니터
│       └── config/                # NVS 설정, 프리셋 관리
├── hardware/                      # 하드웨어 설계
│   ├── kicad/                     # KiCad 회로도 + PCB
│   ├── 3d-case/                   # 케이스 3D 프린트 파일
│   └── reference/                 # 참조 회로도, 데이터시트 링크
├── server/                        # 서버 구성
│   ├── mqtt/                      # Mosquitto 설정
│   ├── grafana/                   # 대시보드 JSON
│   └── scripts/                   # 설치/관리 스크립트
├── presets/                       # 종별 환경 프리셋
│   ├── ball_python.json
│   ├── leopard_gecko.json
│   ├── crested_gecko.json
│   └── corn_snake.json
└── .github/
    ├── ISSUE_TEMPLATE/
    └── workflows/                 # CI (빌드 검증)
```

---

## 빠른 시작

### 필요 장비

| 품목 | 수량 | 용도 |
|------|------|------|
| ESP32-C6-DevKitC-1 | 1+ | 노드 |
| SHT30 브레이크아웃 | 1 | 온습도 센서 |
| DS18B20 방수프로브 | 2 | 핫존/쿨존 온도 |
| Raspberry Pi 3/4/5 | 1 | Border Router |
| SSR (G3MB-202P) | 1~2 | 히터/조명 제어 (Type A) |
| 21700 배터리 + TP4056 | 1 | 배터리 전원 (Type B) |

### 펌웨어 빌드

```bash
# ESP-IDF 설치 (v5.2+)
# https://docs.espressif.com/projects/esp-idf/en/latest/esp32c6/get-started/

# 클론
git clone https://github.com/your-username/reptile-habitat-monitor.git
cd reptile-habitat-monitor/firmware

# 노드 타입 선택
idf.py menuconfig
# → RBMS Configuration → Node Type → TYPE_A 또는 TYPE_B

# 빌드 & 플래시
idf.py build
idf.py -p /dev/ttyUSB0 flash monitor
```

### 서버 설치 (Raspberry Pi)

```bash
cd server/scripts
chmod +x setup.sh
./setup.sh
# Mosquitto + InfluxDB + Grafana 자동 설치 및 설정
```

---

## 펌웨어 아키텍처

4계층 모듈 구조로 설계되어 있습니다.

```
┌─────────────────────────────────────┐
│         Application Layer           │
│   app_type_a  /  app_type_b         │
├─────────────────────────────────────┤
│          Service Layer              │
│  pid_controller | scheduler         │
│  adaptive_poll  | safety_monitor    │
├─────────────────────────────────────┤
│          Driver Layer               │
│  sht30 | ds18b20 | ssr | pwm       │
│  power_mgmt | battery_monitor      │
├─────────────────────────────────────┤
│         Platform Layer              │
│  thread_stack | cbor_codec          │
│  nvs_config   | ota_update          │
└─────────────────────────────────────┘
```

### Type A 동작

FreeRTOS 태스크 기반으로 센서 읽기(1초) → PID 연산 → SSR 제어 → 스케줄링 → Thread 리포트를 병렬 실행합니다.

### Type B 동작

Deep Sleep 중심의 순차 실행 구조입니다.

```
Deep Sleep → Wake → DS18B20 ON → 센서 측정 → CBOR 인코딩
→ Thread TX → 적응형 폴링 주기 계산 → DS18B20 OFF → Deep Sleep
```

wake-up 1회 소요: 약 1.0~1.3초, 약 4.83µAh

---

## 종별 프리셋

JSON 형식으로 종별 최적 환경을 정의합니다. 서버에서 OTA로 배포 가능합니다.

```json
{
  "species": "Ball Python",
  "temp_hot": { "target": 32.0, "min": 30.0, "max": 34.0 },
  "temp_cool": { "target": 26.0, "min": 24.0, "max": 28.0 },
  "humidity": { "target": 60.0, "min": 50.0, "max": 70.0 },
  "light_schedule": {
    "on_hour": 7, "off_hour": 19,
    "sunrise_min": 30, "sunset_min": 30
  },
  "pid": { "kp": 2.0, "ki": 0.5, "kd": 1.0 }
}
```

---

## 회로도

회로도는 [`docs/schematics/`](docs/schematics/) 디렉토리에 SVG 형식으로 제공됩니다.

- Type B 전원부 (배터리 → TP4056 → TPS62740 → 3.3V)
- Type B I2C (SHT30 + 풀업)
- Type B 1-Wire (DS18B20 x2 + BSS138 GND 스위칭)
- Type A 제어부 (SSR + 2N2222 버퍼 + MOSFET PWM)
- Type B ADC (배터리 모니터링 분압기)

---

## 개발 로드맵

| 단계 | 내용 | 기간 | 상태 |
|------|------|------|------|
| Step 1 | 프로토타입 (센서+Thread 기본) | 2~4주 | 🔲 준비 중 |
| Step 2 | Type A 풀 노드 (PID+SSR+디밍) | 2~4주 | 🔲 |
| Step 3 | Type B 배터리 노드 (저전력) | 2~3주 | 🔲 |
| Step 4 | 서버 + 대시보드 | 1~2주 | 🔲 |
| Step 5 | 문서화 + GitHub 공개 | 1~2주 | 🔲 |

총 예상 기간: 약 2~4개월

---

## 관련 문서

| 문서 | 번호 | 내용 |
|------|------|------|
| [시스템 사양서](docs/) | RBMS-SPEC-001 v2.0 | 50대 규모, 배터리 최적화 |
| [개발 로드맵 & 아키텍처](docs/) | RBMS-DEV-001 | Step 1~5, 펌웨어 모듈 설계 |
| [하드웨어 설계 참조](docs/) | RBMS-HW-001 | GPIO 매핑, 회로 참조, BOM |

---

## 기여

이슈, PR 환영합니다.

1. Fork
2. Feature branch 생성 (`git checkout -b feature/amazing-feature`)
3. 커밋 (`git commit -m 'Add amazing feature'`)
4. Push (`git push origin feature/amazing-feature`)
5. Pull Request

---

## 라이선스

MIT License — [LICENSE](LICENSE) 참조

---

## 감사의 말

- [Espressif ESP-IDF](https://github.com/espressif/esp-idf) — ESP32-C6 SDK
- [OpenThread](https://openthread.io/) — Thread 프로토콜 스택
- [tinycbor](https://github.com/niclas/tinycbor) — CBOR 인코딩/디코딩
