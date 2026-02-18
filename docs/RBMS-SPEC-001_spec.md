# RBMS-SPEC-001: 시스템 사양서

**문서 번호**: RBMS-SPEC-001 v2.0
**프로젝트**: Reptile Breeding Management System (RBMS)
**작성일**: 2026-02-19
**상태**: Draft

---

## 1. 시스템 개요

### 1.1 목적

RBMS는 ESP32-C6 MCU와 OpenThread 메시 네트워크를 기반으로 파충류 사육장의 온도, 습도를 모니터링하고 히터 및 조명을 자동 제어하는 시스템이다. 다수의 사육장을 Thread 네트워크로 통합 관리하며, 종별 최적 환경을 프리셋으로 관리한다.

### 1.2 시스템 구성

```
┌─────────────┐     Thread      ┌─────────────┐
│  Type A     │<───────────────>│  Type A     │
│  풀 노드     │    (Router)     │  풀 노드     │
│  사육장 #1   │                 │  사육장 #2   │
└──────┬──────┘                 └──────┬──────┘
       │                               │
       │ Thread (IEEE 802.15.4)        │
       │                               │
┌──────┴──────┐                 ┌──────┴──────┐
│  Type B     │                 │  Type B     │
│  배터리 노드  │                 │  배터리 노드  │
│  사육장 #3   │                 │  사육장 #4   │
└─────────────┘                 └─────────────┘
       │
       ▼
┌─────────────────────────────────────────────┐
│  Border Router (Linux Server / RPi)         │
│  ┌────────┐ ┌──────────┐ ┌──────────┐     │
│  │Gateway │→│Mosquitto │→│ Bridge   │     │
│  │UDP→MQTT│ │  (MQTT)  │ │MQTT→DB   │     │
│  └────────┘ └──────────┘ └────┬─────┘     │
│                               ▼            │
│             ┌──────────┐ ┌─────────────┐  │
│             │ InfluxDB │→│   Grafana   │  │
│             │(시계열DB) │ │ (대시보드)   │  │
│             └──────────┘ └─────────────┘  │
└─────────────────────────────────────────────┘
```

### 1.3 노드 타입

| 구분 | Type A (풀 노드) | Type B (배터리 센서 노드) |
|------|-------------------|--------------------------|
| 전원 | 5V USB → AMS1117-3.3 | 21700 배터리 → TP4056 → TPS62740 |
| Thread 역할 | Router (상시 동작) | SED (Sleepy End Device) |
| 센서 | SHT30 + DS18B20 x2 | SHT30 + DS18B20 x2 |
| 제어 출력 | SSR x2, MOSFET PWM | 없음 |
| 실행 모드 | FreeRTOS 멀티태스크 | Deep Sleep 순차 실행 |
| 측정 주기 | 1초 (고정) | 30~300초 (적응형) |
| 배터리 수명 | N/A | 6개월+ (5000mAh 기준) |

---

## 2. 요구사항

### 2.1 기능 요구사항

| ID | 요구사항 | 우선순위 |
|----|----------|----------|
| FR-01 | SHT30 센서로 온도 및 습도를 측정한다 | 필수 |
| FR-02 | DS18B20 센서 2개로 핫존/쿨존 온도를 측정한다 | 필수 |
| FR-03 | PID 제어로 SSR을 통해 히터 출력을 조절한다 (Type A) | 필수 |
| FR-04 | MOSFET PWM으로 LED 조명을 디밍 제어한다 (Type A) | 필수 |
| FR-05 | 일출/일몰 시뮬레이션으로 자연광 패턴을 재현한다 | 필수 |
| FR-06 | Thread 메시 네트워크로 센서 데이터를 전송한다 | 필수 |
| FR-07 | CBOR 형식으로 데이터를 직렬화한다 | 필수 |
| FR-08 | 종별 프리셋(Ball Python, Leopard Gecko 등)을 지원한다 | 필수 |
| FR-09 | 적응형 폴링으로 배터리를 절약한다 (Type B) | 필수 |
| FR-10 | Deep Sleep 기반 저전력 모드를 지원한다 (Type B) | 필수 |
| FR-11 | NVS에 설정을 영구 저장/복원한다 | 필수 |
| FR-12 | OTA 펌웨어 업데이트를 지원한다 | 선택 |
| FR-13 | 배터리 잔량을 ADC로 모니터링한다 (Type B) | 필수 |
| FR-14 | 서버에서 Grafana 대시보드로 시각화한다 | 필수 |

### 2.2 비기능 요구사항

| ID | 요구사항 | 기준 |
|----|----------|------|
| NR-01 | 배터리 수명 | 5000mAh 기준 6개월 이상 |
| NR-02 | 센서 정확도 | SHT30: +-0.3C / DS18B20: +-0.5C |
| NR-03 | 제어 응답시간 | PID 연산 주기 1초 이내 |
| NR-04 | 네트워크 노드 수 | Thread 메시 최대 50대 |
| NR-05 | 동작 온도 범위 | 0~50C (사육장 환경) |
| NR-06 | 펌웨어 크기 | Flash 4MB 파티션 이내 (OTA 듀얼 슬롯) |
| NR-07 | 데이터 보존 | InfluxDB retention 90일 |
| NR-08 | 안전 응답 시간 | 과열 감지 후 1초 이내 SSR 차단 |

---

## 3. 기능 명세

### 3.1 센서 계측

#### 3.1.1 SHT30 (I2C 온습도 센서)

- **인터페이스**: I2C (400kHz, SDA=GPIO6, SCL=GPIO7)
- **측정 모드**: Single-Shot, High Repeatability (0x2400)
- **측정 대기**: 최대 20ms
- **데이터 형식**: 6바이트 [temp_H, temp_L, temp_CRC, hum_H, hum_L, hum_CRC]
- **CRC 검증**: CRC-8 (polynomial 0x31, init 0xFF)
- **변환 공식**:
  - 온도: `-45.0 + 175.0 * (raw_temp / 65535.0)` (C)
  - 습도: `100.0 * (raw_hum / 65535.0)` (%)
- **오류 복구**: I2C bus recovery (9 clock pulses), Soft Reset (0x30A2), 최대 2회 재시도

#### 3.1.2 DS18B20 (1-Wire 온도 센서)

- **인터페이스**: 1-Wire GPIO bit-bang (DATA=GPIO8)
- **전원 제어**: GPIO 스위칭 (PWR=GPIO9, Type B 전용)
- **해상도**: 12비트 (변환 시간 750ms)
- **센서 수**: 최대 2개 (핫존/쿨존)
- **ROM Search**: 자동 검색 및 64비트 ROM 코드 식별
- **CRC 검증**: Dallas CRC-8 (polynomial 0x8C reflected)
- **변환 공식**: `raw_16bit / 16.0` (C)
- **타이밍 보호**: `portENTER_CRITICAL` / `portEXIT_CRITICAL` 사용

### 3.2 액추에이터 제어 (Type A 전용)

#### 3.2.1 SSR (Solid State Relay) 제어

- **모델**: G3MB-202P x2 (히터, UV 조명)
- **제어 방식**: Cycle Skipping
  - 10초 주기 (100 ticks x 100ms)
  - duty% 만큼 ON (0~100% 정수 분해능)
- **채널**: 최대 2채널 (히터=GPIO10, UV조명=GPIO11)
- **안전 기능**: `ssr_force_off_all()` 즉시 차단

#### 3.2.2 PWM Dimmer (LED 디밍)

- **하드웨어**: MOSFET IRF520 + LEDC 주변장치
- **GPIO**: GPIO3
- **PWM 설정**:
  - 주파수: 1kHz
  - 해상도: 10-bit (0~1023)
  - 클럭: XTAL_CLK (Deep Sleep 호환)
- **밝기 범위**: 0~1000 (사용자 단위)
- **페이드 기능**: `ledc_set_fade_with_time()` 활용, 일출/일몰 점진 전환

### 3.3 PID 온도 제어 (Type A 전용)

- **알고리즘**: Discrete PID (1초 주기)
- **Derivative**: Derivative-on-measurement (setpoint 변경 시 미분 킥 방지)
- **Anti-windup**: Back-calculation 방식
  - 출력 포화 시 적분항을 역산 보정
  - `integral += (output_clamped - output_raw) / Ki`
- **출력 범위**: 0.0~100.0 (SSR duty percent)
- **기본 파라미터** (Ball Python):
  - Kp = 2.0, Ki = 0.5, Kd = 1.0
- **Kconfig 관리**: x100 정수 (200/50/100)

### 3.4 조명 스케줄러

- **시간 기준**: KST (UTC+9), 시스템 타임
- **일출/일몰 시뮬레이션**: 선형 디밍 (0.0 ~ 1.0)
  - 일출: 점등 시각부터 sunrise_min 동안 0→1
  - 일몰: 소등 시각 sunset_min 전부터 1→0
- **자정 경과 지원**: `on_hour > off_hour` 시 야간 스케줄 처리
- **기본값**: 07:00 점등, 19:00 소등, 일출/일몰 각 30분

### 3.5 적응형 폴링 (Type B 전용)

- **원리**: 온도 변화량에 따라 측정 주기를 자동 조절
- **폴링 주기 계산**:
  - `|delta_temp| >= delta_high (1.0C)` → Fast (30초)
  - `|delta_temp| = 0` → Slow (300초)
  - 중간값: 선형 보간 `period = slow - (delta/high) * (slow - fast)`
- **이전 온도**: `RTC_DATA_ATTR` 변수로 Deep Sleep 간 유지

### 3.6 전원 관리 (Type B 전용)

- **Deep Sleep 진입**: Timer Wakeup 사용
- **GPIO Hold**: Deep Sleep 중 등록된 GPIO를 LOW로 고정
- **RTC 도메인 전원 차단**: `RTC_PERIPH`, `XTAL` 도메인 OFF
- **RTC 메모리 보존**: `RTC_DATA_ATTR` 변수 (이전 온도, 부팅 횟수, 배터리 체크 카운터)
- **배터리 모니터링**:
  - ADC 16x 멀티샘플링 (노이즈 4배 감소)
  - Curve Fitting 캘리브레이션 (ESP32-C6 전용)
  - 분압기 보정 (100k/100k, x2)
  - 전압→잔량: 4.2V=100%, 3.0V=0% 선형 보간
  - 저전압 경고: 3.2V 이하
  - 측정 간격: 매 30회 폴링마다 1회 (전력 절약)

---

## 4. 통신 프로토콜

### 4.1 Thread 네트워크

- **표준**: IEEE 802.15.4 기반 Thread 1.3+
- **라디오**: ESP32-C6 내장 802.15.4 라디오 (RADIO_MODE_NATIVE)
- **역할**:
  - Type A: Router (상시 활성, 메시 중계)
  - Type B: SED (Sleepy End Device, Child Timeout 300초)
- **전송**: UDP Multicast (`ff03::1`, Port 5684)
- **수신**: UDP 바인드 (Port 5684)
- **Lock**: 외부 태스크에서 OT API 호출 시 `esp_openthread_lock_acquire/release` 필수
- **Dataset**: Active Operational Dataset 자동 생성 (미존재 시)

### 4.2 CBOR 직렬화

센서 데이터는 CBOR Map 형식으로 인코딩한다. 외부 라이브러리 없이 내장 인코더/디코더를 사용한다.

#### 4.2.1 CBOR 정수 키 매핑

| 키 | 필드명 | 데이터 타입 | 단위 | 비고 |
|----|--------|-------------|------|------|
| 1 | temp_hot | float32 | C | 핫존 온도 (항상 포함) |
| 2 | temp_cool | float32 | C | 쿨존 온도 (항상 포함) |
| 3 | humidity | float32 | % | 상대습도 (항상 포함) |
| 4 | battery_v | float32 | % | 배터리 잔량 (Type B, 옵션) |
| 5 | heater_duty | float32 | % | 히터 출력 (Type A, 옵션) |
| 6 | light_duty | float32 | % | 조명 출력 (Type A, 옵션) |
| 7 | safety | uint | enum | 안전 상태 코드 (옵션) |

#### 4.2.2 CBOR 패킷 구조

```
CBOR Map Header (1 byte): 0xA3 ~ 0xA7 (3~7 fields)
각 필드: Key(1 byte) + Value(5 bytes, float32) 또는 Key(1 byte) + Value(1 byte, uint)
최대 패킷 크기: 43 bytes (7 fields 모두 포함 시)
```

#### 4.2.3 선택적 필드 규칙

- 값이 음수 (-1.0f)인 필드는 인코딩에서 제외
- Type A: `battery_v` 제외, `heater_duty`/`light_duty` 포함
- Type B: `heater_duty`/`light_duty` 제외, `battery_v` 조건부 포함

### 4.3 서버 통신 흐름

```
노드 (CBOR/UDP)  →  Border Router (ot-br-posix)
                         ↓
                    UDP Gateway (Thread → MQTT 변환)
                         ↓
                    Mosquitto MQTT Broker
                    토픽: rbms/<node_id>/telemetry
                         ↓
                    MQTT-InfluxDB Bridge (Python)
                    CBOR 디코딩 → InfluxDB Write
                         ↓
                    InfluxDB (시계열 DB)
                    measurement: "telemetry"
                    tags: {node_id}
                    retention: 90일
                         ↓
                    Grafana 대시보드
```

### 4.4 MQTT 토픽 구조

| 토픽 | 방향 | 페이로드 | 용도 |
|------|------|----------|------|
| `rbms/<node_id>/telemetry` | 노드→서버 | CBOR Map | 센서 데이터 리포트 |
| `rbms/<node_id>/command` | 서버→노드 | CBOR Map | 제어 명령 (예약) |
| `rbms/<node_id>/preset` | 서버→노드 | JSON | 프리셋 OTA 배포 (예약) |
| `rbms/<node_id>/status` | 노드→서버 | JSON | 노드 상태 (예약) |

---

## 5. 안전 기능

### 5.1 안전 검사 파이프라인 (7단계)

안전 모듈은 아래 순서대로 검사를 수행하며, 첫 번째 발견된 이상 상태를 반환한다.

| 단계 | 검사 항목 | 기준 | 결과 상태 |
|------|-----------|------|-----------|
| 1 | 센서 유효범위 | DS18B20: -20~85C | FAULT_SENSOR |
| 2 | Stale 센서 감지 | 마지막 유효 읽기 > 30초 | FAULT_SENSOR_STALE |
| 3 | 듀얼 센서 불일치 | abs(hot - cool) > 15.0C | FAULT_SENSOR_MISMATCH |
| 4 | 온도 변화율 | abs(rate) > 2.0C/초 | FAULT_SENSOR |
| 5 | 과열 검사 | hot > setpoint + 5.0C | FAULT_OVERTEMP |
| 6 | 히터 연속 동작 | 연속 ON > 3600초 | FAULT_HEATER_TIMEOUT |
| 7 | 고온 접근 경고 | hot > setpoint + 3.5C (70%) | WARN_HIGH_TEMP |

### 5.2 안전 상태 코드

```c
typedef enum {
    SAFETY_OK = 0,                  // 정상
    SAFETY_WARN_HIGH_TEMP,          // 고온 접근 경고
    SAFETY_WARN_LOW_TEMP,           // 저온 경고
    SAFETY_FAULT_OVERTEMP,          // 과열 차단
    SAFETY_FAULT_SENSOR,            // 센서 이상/범위 초과/변화율 초과
    SAFETY_FAULT_SENSOR_STALE,      // 센서 데이터 갱신 없음
    SAFETY_FAULT_SENSOR_MISMATCH,   // 듀얼 센서 불일치
    SAFETY_FAULT_HEATER_TIMEOUT,    // 히터 연속 동작 초과
} safety_status_t;
```

### 5.3 긴급 차단 동작

FAULT 수준 이상 감지 시:
1. `safety_emergency_shutdown()` 호출
2. `ssr_force_off_all()` 로 모든 SSR 채널 즉시 OFF
3. `pwm_dimmer_set(0)` 으로 LED 즉시 소등
4. 에러 로그 출력 (`ESP_LOGE`)
5. 다음 안전 검사 통과 시까지 제어 출력 차단 유지

### 5.4 Type A / Type B 안전 차이

| 항목 | Type A | Type B |
|------|--------|--------|
| 센서 유효범위 | 활성 | 활성 |
| Stale 감지 | 활성 (30초) | 비활성 (단발성 읽기) |
| 듀얼 센서 불일치 | 활성 (15.0C) | 활성 (15.0C) |
| 변화율 감시 | 활성 (2.0C/s) | 비활성 (이전 값 없음) |
| 과열 차단 | 활성 → SSR OFF | 활성 → 경고 전송 |
| 히터 타임아웃 | 활성 (3600초) | 비활성 (히터 없음) |

---

## 6. 종별 프리셋

### 6.1 지원 종

| 종 | 핫존 목표 | 쿨존 목표 | 습도 목표 | Kp/Ki/Kd | 과열 오프셋 |
|----|-----------|-----------|-----------|----------|-------------|
| Ball Python (볼파이톤) | 32.0C | 26.0C | 60% | 2.0/0.5/1.0 | +5.0C |
| Leopard Gecko (레오파드 게코) | 32.0C | 24.0C | 40% | 2.0/0.4/0.8 | +5.0C |
| Crested Gecko (크레스티드 게코) | 26.0C | 22.0C | 70% | 1.5/0.3/0.5 | +4.0C |
| Corn Snake (콘스네이크) | 30.0C | 24.0C | 50% | 2.0/0.5/1.0 | +5.0C |

### 6.2 프리셋 JSON 형식

```json
{
  "species": "Ball Python",
  "species_kr": "볼파이톤",
  "temp_hot":  { "target": 32.0, "min": 30.0, "max": 34.0 },
  "temp_cool": { "target": 26.0, "min": 24.0, "max": 28.0 },
  "humidity":  { "target": 60.0, "min": 50.0, "max": 70.0 },
  "light_schedule": {
    "on_hour": 7, "off_hour": 19,
    "sunrise_minutes": 30, "sunset_minutes": 30
  },
  "pid": { "kp": 2.0, "ki": 0.5, "kd": 1.0 },
  "safety": { "overtemp_offset": 5.0, "heater_max_continuous_sec": 3600 }
}
```

### 6.3 프리셋 저장

- NVS blob 형식으로 `preset_t` 구조체를 직렬화하여 저장
- 로드 시 유효성 검증 수행 (온도 범위, PID 계수 부호, NaN 검사)
- 검증 실패 시 기본 프리셋(Ball Python) 자동 로드

---

## 7. 성능 목표

### 7.1 Type B 배터리 수명 계산

**전제 조건**: 21700 배터리 (5000mAh), TPS62740 DC-DC (IQ 0.36uA)

| 상태 | 전류 | 시간 | 에너지 (uAh) |
|------|------|------|-------------|
| Deep Sleep | ~7uA | 평균 150초 | 291.7 |
| Wake + 센서 측정 | ~15mA | 750ms | 3.13 |
| Thread TX | ~15mA | 200ms | 0.83 |
| 기타 (초기화 등) | ~10mA | 300ms | 0.83 |
| **1회 사이클 합계** | - | ~151.25초 | ~296.5 |

- 1회 사이클 평균 소모: ~4.83uAh
- 시간당 사이클: 3600 / 150 = 24회
- 시간당 소모: 24 x 4.83 = ~116uAh
- 일일 소모: 116 x 24 = ~2.78mAh
- **예상 수명**: 5000 / 2.78 = **~1800일 (약 5년)** (이론값)
- **실측 예상**: 변환 효율, 자연 방전 고려 시 **6~12개월**

### 7.2 펌웨어 크기

| 항목 | 크기 |
|------|------|
| 빌드 이미지 (Type A) | ~179KB |
| Flash 파티션 (ota_0) | 1.88MB (0x1D0000) |
| 파티션 여유율 | ~83% |
| NVS 파티션 | 24KB |
| OT Storage | 16KB |

### 7.3 타이밍 성능

| 동작 | 소요 시간 |
|------|-----------|
| Type A 센서 루프 | 1초 (DS18B20 변환 750ms 포함) |
| Type A PID 연산 | < 1ms |
| Type A SSR 갱신 | 100ms 주기 |
| Type A Thread 리포트 | 10초 주기 |
| Type B Wake-to-Sleep | 1.0~1.3초 |
| Type B Thread 연결 대기 | 최대 5초 |

---

## 8. 환경 조건

### 8.1 동작 환경

| 항목 | 범위 |
|------|------|
| 동작 온도 | 0~50C |
| 동작 습도 | 10~90% RH (비결로) |
| 전원 입력 (Type A) | 5V DC, 1A 이상 |
| 전원 입력 (Type B) | 3.0~4.2V (21700 Li-ion) |

### 8.2 센서 유효 범위

| 센서 | 측정 범위 | 정확도 |
|------|-----------|--------|
| SHT30 온도 | -40~125C | +-0.3C |
| SHT30 습도 | 0~100% RH | +-2% RH |
| DS18B20 | -55~125C (유효: -20~85C) | +-0.5C (10~85C) |

### 8.3 Flash 파티션 테이블

```
# Name      Type   SubType  Offset     Size
nvs         data   nvs      0x9000     0x6000   (24KB)
otadata     data   ota      0xF000     0x2000   (8KB)
phy_init    data   phy      0x11000    0x1000   (4KB)
ota_0       app    ota_0    0x20000    0x1D0000 (1.88MB)
ota_1       app    ota_1    0x1F0000   0x1D0000 (1.88MB)
ot_storage  data   nvs      0x3C0000   0x4000   (16KB)
```

---

## 9. 소프트웨어 의존성

### 9.1 펌웨어

| 의존성 | 버전 | 용도 |
|--------|------|------|
| ESP-IDF | v5.2+ | MCU SDK |
| OpenThread | ESP-IDF 내장 | Thread 프로토콜 스택 |
| FreeRTOS | ESP-IDF 내장 | RTOS 커널 |
| CBOR Codec | 자체 구현 | 바이너리 직렬화 |

### 9.2 서버

| 의존성 | 용도 |
|--------|------|
| ot-br-posix | OpenThread Border Router |
| Mosquitto | MQTT 브로커 |
| InfluxDB | 시계열 데이터베이스 |
| Grafana | 대시보드 시각화 |
| Python 3.8+ | MQTT-InfluxDB Bridge |
| cbor2 (Python) | CBOR 디코딩 |
| paho-mqtt (Python) | MQTT 클라이언트 |
| influxdb (Python) | InfluxDB 클라이언트 |

---

## 10. 용어 정의

| 용어 | 정의 |
|------|------|
| RBMS | Reptile Breeding Management System |
| SED | Sleepy End Device (Thread 저전력 자식 노드) |
| SSR | Solid State Relay (무접점 릴레이) |
| Cycle Skipping | 10초 주기에서 ON/OFF 비율로 평균 출력 조절하는 방식 |
| CBOR | Concise Binary Object Representation (RFC 8949) |
| PID | Proportional-Integral-Derivative 제어 |
| NVS | Non-Volatile Storage (ESP-IDF Flash 저장소) |
| OTA | Over-The-Air (무선 펌웨어 업데이트) |
| Border Router | Thread 네트워크와 IP 네트워크를 연결하는 게이트웨이 |
| RTC Memory | Real-Time Clock 영역 메모리 (Deep Sleep 후에도 유지) |
