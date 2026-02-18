# RBMS ESP32-C6 GPIO 핀 할당표 (Pinout Reference)

ESP32-C6-WROOM-1 모듈의 GPIO 할당 정의.
Kconfig에서 핀 번호를 관리하며, 소스 코드에서 하드코딩하지 않는다.

---

## GPIO 할당 총괄표

| GPIO | 기능 | 방향 | Type A | Type B | Kconfig 키 | 비고 |
|:----:|------|:----:|:------:|:------:|------------|------|
| GPIO3 | PWM LED 출력 / SSR Heater | Output | SSR CH0 (히터) | - | `SSR_HEATER_GPIO` (A) / `DS18B20_POWER_GPIO` (B, 기본 3) | Type A/B에서 Kconfig로 분리 |
| GPIO4 | Battery ADC 입력 | Analog In | - | ADC1_CH4 | (코드 내 ADC_CHANNEL_0) | Type B 전용. 분압기 출력 연결 |
| GPIO5 | SSR UV Light 출력 | Output | SSR CH1 (UV 조명) | - | `SSR_LIGHT_GPIO` | Type A 전용 |
| GPIO6 | I2C SDA | Bidirectional | SHT30 SDA | SHT30 SDA | (코드 내 GPIO_NUM_6) | 10k 외부 풀업 -> 3.3V |
| GPIO7 | I2C SCL | Bidirectional | SHT30 SCL | SHT30 SCL | (코드 내 GPIO_NUM_7) | 10k 외부 풀업 -> 3.3V |
| GPIO8 | 1-Wire 데이터 | Bidirectional | DS18B20 DATA | DS18B20 DATA | `SENSOR_DS18B20_GPIO` (기본 2, 실제 배선 시 GPIO8 권장) | 4.7k 풀업 -> 3.3V |
| GPIO9 | DS18B20 전원 제어 | Output | - | BSS138 Gate | `DS18B20_POWER_GPIO` | Type B 전용. HIGH=전원ON |
| GPIO10 | PWM LED 디밍 | Output | LEDC CH0 | - | `PWM_DIMMING_GPIO` | Type A 전용. IRF520 Gate 구동 |
| GPIO15 | 802.15.4 안테나 | Internal | Thread Radio | Thread Radio | (내부, 사용자 비할당) | 모듈 내부 연결, 외부 배선 금지 |

---

## 상세 핀 설명

### GPIO3 -- SSR Heater (Type A)

```
ESP32-C6 GPIO3 --[1k]-- 2N2222 Base
                         2N2222 Emitter -- GND
                         2N2222 Collector -- SSR Input(-)
                         SSR Input(+) -- 3.3V
```

- **용도**: Type A에서 히터 SSR(G3MB-202P CH0) 제어
- **출력 레벨**: LOW=OFF, HIGH=ON (NPN 드라이버 통해 SSR 트리거)
- **전기적 특성**: GPIO 출력 전류 ~3.3mA (1k 베이스 저항), push-pull
- **제어 방식**: Cycle Skipping -- 10초(100 ticks x 100ms) 주기에서 duty% 만큼 ON
- **Kconfig**: `CONFIG_SSR_HEATER_GPIO` (기본값 3)
- **주의사항**:
  - Safety Monitor에 의해 과열 시 강제 OFF (`ssr_force_off_all()`)
  - 히터 연속 동작 제한 시간: Kconfig `HEATER_MAX_CONTINUOUS` (기본 3600초)

### GPIO4 -- Battery ADC (Type B)

```
VBATT(4.2V max) --[100k]--+--[100k]-- GND
                           |
                     ESP32-C6 GPIO4 (ADC1_CH4)
```

- **용도**: Type B 배터리 전압 모니터링
- **입력 레벨**: 0 ~ 2.1V (4.2V 배터리 / 2 분압)
- **전기적 특성**: ADC1 채널 4, 12-bit 해상도, 12dB 감쇠 (0~3.1V 범위)
- **측정 방식**: 16회 멀티샘플링 -> 평균 -> Curve Fitting 캘리브레이션 -> x2 분압 보정
- **측정 범위**: 3.0V(방전) ~ 4.2V(만충), 3.2V 이하 저전압 경고
- **측정 주기**: `BATTERY_CHECK_INTERVAL` 폴링 횟수마다 1회 (기본 30회, ~2.5시간)
- **주의사항**:
  - ESP32-C6 ADC는 Curve Fitting 캘리브레이션만 지원 (Line Fitting 미지원)
  - 분압 저항에 의한 상시 누설 전류: ~21uA (100k+100k @ 4.2V)
  - Deep Sleep 중에도 누설 발생 -- 고저항(1M+1M) 분압기로 개선 가능하나 노이즈 증가

### GPIO5 -- SSR UV Light (Type A)

```
ESP32-C6 GPIO5 --[1k]-- 2N2222 Base
                         2N2222 Emitter -- GND
                         2N2222 Collector -- SSR Input(-)
                         SSR Input(+) -- 3.3V
```

- **용도**: Type A에서 UV 조명 SSR(G3MB-202P CH1) 제어
- **출력 레벨**: LOW=OFF, HIGH=ON
- **전기적 특성**: GPIO3과 동일한 NPN 드라이버 회로
- **제어 방식**: Scheduler에 의한 시간 기반 ON/OFF (Cycle Skipping도 적용 가능)
- **Kconfig**: `CONFIG_SSR_LIGHT_GPIO` (기본값 5)
- **주의사항**: 스케줄러의 on_hour/off_hour에 따라 조명 주기 자동 제어

### GPIO6 -- I2C SDA (공통)

```
ESP32-C6 GPIO6 --+-- SHT30 SDA
                  |
                 [10k]
                  |
                 3.3V
```

- **용도**: I2C 데이터 라인 (SHT30 온습도 센서)
- **전기적 특성**: Open-drain, 외부 10k 풀업 -> 3.3V
- **통신 속도**: 400kHz (I2C Fast Mode)
- **주의사항**:
  - I2C 버스 hang 시 자동 복구 (SCL 9 clock pulses + STOP 조건)
  - 내부 풀업도 활성화되어 있으나, 안정적 통신을 위해 외부 풀업 필수
  - I2C 라인 길이 30cm 이하 권장 (400kHz 기준)

### GPIO7 -- I2C SCL (공통)

```
ESP32-C6 GPIO7 --+-- SHT30 SCL
                  |
                 [10k]
                  |
                 3.3V
```

- **용도**: I2C 클럭 라인 (SHT30 온습도 센서)
- **전기적 특성**: Open-drain, 외부 10k 풀업 -> 3.3V
- **통신 속도**: 400kHz
- **주의사항**: GPIO6 (SDA)과 동일한 풀업/배선 규칙 적용

### GPIO8 -- 1-Wire Data (공통)

```
ESP32-C6 GPIO8 --+-- DS18B20 #0 DATA --+-- DS18B20 #1 DATA
                  |                      |
                 [4.7k]                 (same bus)
                  |
                 3.3V
```

- **용도**: DS18B20 1-Wire 데이터 버스 (듀얼 센서 공유)
- **전기적 특성**: Open-drain, 외부 4.7k 풀업 -> 3.3V
- **프로토콜**: 1-Wire (Reset 480us + Presence + R/W slots)
- **Kconfig**: `CONFIG_SENSOR_DS18B20_GPIO` (기본값 2, 실제 배선 시 확인)
- **주의사항**:
  - 1-Wire 타이밍이 마이크로초 수준이므로 `portENTER_CRITICAL()` / `portEXIT_CRITICAL()`로 interrupt 보호 필수
  - Reset pulse: 480us LOW -> Release -> 70us 대기 -> Presence 감지
  - 프로브 케이블 총 길이 3m 이하 권장 (풀업 저항값 조정 가능)
  - ROM Search 알고리즘으로 최대 2개 센서 자동 식별 (CRC8 검증)

### GPIO9 -- DS18B20 Power Control (Type B 전용)

```
ESP32-C6 GPIO9 --[10k gate pull-down]-- BSS138 Gate
                                         BSS138 Source -- GND
                                         BSS138 Drain -- DS18B20 VCC (x2)
                                         3.3V --[via BSS138]-- DS18B20 VCC
```

- **용도**: DS18B20 2개의 VCC 전원을 MOSFET으로 스위칭 (배터리 절약)
- **출력 레벨**: HIGH=전원 ON, LOW=전원 OFF
- **전기적 특성**: BSS138 Vgs(th)=0.8~1.5V, 3.3V 게이트로 완전 도통
- **Kconfig**: `CONFIG_DS18B20_POWER_GPIO` (기본값 3, Type B에서만 활성)
- **동작 시퀀스**:
  1. `ds18b20_power_on()` -- GPIO9 HIGH, 10ms 안정화 대기
  2. 센서 검색 + 변환(750ms) + 읽기
  3. `ds18b20_power_off()` -- GPIO9 LOW
  4. Deep Sleep 진입
- **주의사항**:
  - Deep Sleep 시 `gpio_hold_en()`으로 LOW 상태 유지 (의도치 않은 전원 투입 방지)
  - DS18B20 대기 전류 ~1mA/개, 2개 기준 약 2mA 절감

### GPIO10 -- PWM LED Dimming (Type A 전용)

```
ESP32-C6 GPIO10 --[10k gate pull-down]-- IRF520 Gate
                                          IRF520 Source -- GND
                                          IRF520 Drain -- LED Strip(-)
                                          LED Strip(+) -- 12V/24V DC
```

- **용도**: LED 스트립 PWM 디밍 (일출/일몰 시뮬레이션)
- **출력 레벨**: PWM 0~100% (LEDC 10-bit, 0~1023 duty)
- **전기적 특성**: LEDC Timer 0, Channel 0, 1kHz, XTAL 클럭 기반
- **Kconfig**: `CONFIG_PWM_DIMMING_GPIO` (기본값 10)
- **밝기 제어**: 0~1000 brightness 값 -> 0~1023 duty 매핑 (소프트 스케일링)
- **Fade 기능**: `pwm_dimmer_fade_to()` -- LEDC 하드웨어 fade로 부드러운 전환
- **주의사항**:
  - IRF520의 Vgs(th)=2~4V -- 3.3V 게이트 구동 시 부분 도통 영역 가능
  - 고전류 LED(>2A) 사용 시 IRLZ44N (Vgs(th)=1~2V, logic level) 교체 권장
  - 게이트 풀다운 10k 저항: 부팅 시 floating 방지 (LED 깜빡임 차단)

### GPIO15 -- 802.15.4 Antenna (내부)

- **용도**: IEEE 802.15.4 무선 통신 (Thread/Zigbee)
- **상태**: ESP32-C6-WROOM-1 모듈 내부 연결, 사용자 접근 불가
- **주의사항**:
  - 안테나 패턴 주변 GND plane 확보 (모듈 데이터시트 keep-out zone 준수)
  - 모듈 안테나 측 PCB 가장자리에 배치, 금속 케이스 사용 시 안테나 창 필요
  - Thread Router(Type A)와 SED(Type B) 모두 동일 하드웨어 사용

---

## 미사용 GPIO 및 예약 핀

| GPIO | 상태 | 비고 |
|:----:|------|------|
| GPIO0 | Reserved | 부트 모드 선택 (Strapping pin). 외부 회로 연결 시 주의 |
| GPIO1 | Reserved | 부트 모드 선택 (Strapping pin) |
| GPIO2 | Available | Kconfig 기본 DS18B20 데이터 핀 (실제 배선과 일치시킬 것) |
| GPIO12 | Available | JTAG (MTMS). 디버깅 필요 시 확보 |
| GPIO13 | Available | JTAG (MTDI) |
| GPIO14 | Available | 미래 확장용 (추가 센서, I2C 디바이스 등) |
| GPIO18 | USB D- | USB-JTAG/Serial. 펌웨어 다운로드 및 시리얼 모니터 |
| GPIO19 | USB D+ | USB-JTAG/Serial |

---

## Strapping Pin 주의사항

ESP32-C6는 부팅 시 특정 GPIO의 전압 레벨로 동작 모드를 결정한다.

| Pin | 기능 | SPI Boot (정상) | Download Boot | 비고 |
|-----|------|:--------------:|:-------------:|------|
| GPIO8 | Boot Mode | Don't care | 0 | 1-Wire 풀업이 HIGH를 유지하므로 정상 부팅에 영향 없음 |
| GPIO9 | Boot Mode | 1 (내부 풀업) | Don't care | Type B에서 BSS138 gate로 사용 -- 부팅 시 풀업 확인 |
| GPIO15 | JTAG Select | 0 | Don't care | 모듈 내부 처리 |

> **경고**: GPIO8, GPIO9를 외부 회로에 연결할 때 부팅 시 원하지 않는 레벨이 인가되지 않도록 주의.
> 특히 GPIO9에 강한 풀다운이 있으면 Download Boot 모드로 진입할 수 있다.

---

## 전원 핀 요약

| 핀 | 연결 | 비고 |
|----|------|------|
| 3V3 | LDO 출력 (Type A) / TPS62740 출력 (Type B) | 100nF 바이패스 최대한 가까이 |
| GND | 공통 그라운드 | Star topology 권장, AC 회로와 분리 |
| EN | 10k 풀업 -> 3.3V, 100nF -> GND | RC 지연으로 안정적 리셋 보장 |

---

## 배선 다이어그램 참조

회로도 SVG 파일은 `docs/schematics/` 디렉토리에 위치한다:

| 파일 | 내용 |
|------|------|
| `1_type_b_power.svg` | Type B 전원부 (TP4056 + TPS62740) |
| `2_type_b_i2c.svg` | I2C 연결 (SHT30) |
| `3_type_b_1wire.svg` | 1-Wire 연결 (DS18B20 + 전원 스위칭) |
| `4_type_a_control.svg` | Type A 액추에이터 (SSR + PWM) |
| `5_type_b_adc.svg` | Type B 배터리 ADC 분압기 |
