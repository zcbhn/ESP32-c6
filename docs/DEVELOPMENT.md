# 개발 가이드

## 개발환경 설정

### 필수 도구

| 도구 | 버전 | 용도 |
|------|------|------|
| ESP-IDF | v5.2+ | ESP32-C6 SDK |
| Python | 3.8+ | ESP-IDF 빌드 도구 |
| Git | 2.x | 버전 관리 |
| USB 드라이버 | - | ESP32 시리얼 통신 |

### ESP-IDF 설치

```bash
# Linux/macOS
mkdir -p ~/esp
cd ~/esp
git clone -b v5.2 --recursive https://github.com/espressif/esp-idf.git
cd esp-idf
./install.sh esp32c6
source export.sh

# Windows (ESP-IDF Tools Installer 권장)
# https://docs.espressif.com/projects/esp-idf/en/latest/esp32c6/get-started/windows-setup.html
```

### 프로젝트 클론

```bash
git clone https://github.com/your-username/reptile-habitat-monitor.git
cd reptile-habitat-monitor
```

---

## 펌웨어 빌드

### 기본 빌드 과정

```bash
cd firmware

# 1. 노드 타입 선택
idf.py menuconfig
# → RBMS Configuration → Node Type → TYPE_A 또는 TYPE_B

# 2. 빌드
idf.py build

# 3. 플래시 & 모니터
idf.py -p /dev/ttyUSB0 flash monitor
# Windows: idf.py -p COM3 flash monitor
```

### 노드 타입별 빌드

```bash
# Type A (풀 노드) 빌드
# sdkconfig.defaults에서 CONFIG_NODE_TYPE_A=y 설정 후:
idf.py build

# Type B (배터리 노드) 빌드
# sdkconfig.defaults에서 CONFIG_NODE_TYPE_B=y 설정 후:
idf.py build
```

### Kconfig 주요 설정

| 메뉴 경로 | 설정 | 기본값 | 설명 |
|-----------|------|--------|------|
| Node Type | `NODE_TYPE_A` / `NODE_TYPE_B` | TYPE_B | 노드 타입 |
| Sensor | `SENSOR_SHT30_ADDR` | 0x44 | SHT30 I2C 주소 |
| Sensor | `SENSOR_DS18B20_GPIO` | 2 | DS18B20 데이터 핀 |
| Sensor | `DS18B20_POWER_GPIO` | 3 | DS18B20 전원 핀 (Type B) |
| Actuator | `SSR_HEATER_GPIO` | 3 | SSR 히터 출력 (Type A) |
| Actuator | `SSR_LIGHT_GPIO` | 5 | SSR UV 조명 출력 (Type A) |
| Actuator | `PWM_DIMMING_GPIO` | 10 | LED 디밍 PWM (Type A) |
| PID | `PID_KP` / `KI` / `KD` | 200/50/100 | PID 파라미터 x100 |
| Adaptive | `POLL_PERIOD_FAST` | 30 | 빠른 폴링 주기 (초) |
| Adaptive | `POLL_PERIOD_SLOW` | 300 | 느린 폴링 주기 (초) |
| Safety | `SAFETY_OVERTEMP_OFFSET` | 50 | 과열 오프셋 x10 (°C) |
| Safety | `HEATER_MAX_CONTINUOUS` | 3600 | 히터 최대 연속 시간 (초) |

---

## 컴포넌트 구현 가이드

### 컴포넌트 구조 표준

각 컴포넌트는 다음 구조를 따른다:

```
components/example/
├── CMakeLists.txt          # 빌드 설정
├── include/
│   └── example.h           # 공개 API 헤더
└── example.c               # 구현
```

### CMakeLists.txt 표준

```cmake
idf_component_register(
    SRCS "example.c"
    INCLUDE_DIRS "include"
    REQUIRES driver esp_log    # ESP-IDF 의존성
    PRIV_REQUIRES nvs_flash    # 내부 의존성
)
```

### 헤더 파일 표준

```c
#ifndef EXAMPLE_H
#define EXAMPLE_H

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

// 타입 정의
typedef struct {
    float value;
} example_data_t;

// 초기화/해제
esp_err_t example_init(void);
esp_err_t example_deinit(void);

// 기능
esp_err_t example_read(example_data_t *data);

#ifdef __cplusplus
}
#endif

#endif // EXAMPLE_H
```

### 구현 파일 표준

```c
#include "example.h"
#include "esp_log.h"

static const char *TAG = "example";

esp_err_t example_init(void)
{
    ESP_LOGI(TAG, "Initializing example");
    // 구현
    return ESP_OK;
}
```

---

## 컴포넌트별 구현 명세

### sensor — 센서 드라이버

#### SHT30 (I2C 온습도 센서)

필요 API:
```c
esp_err_t sht30_init(i2c_port_t port, uint8_t addr);
esp_err_t sht30_read(float *temperature, float *humidity);
esp_err_t sht30_reset(void);
```

구현 포인트:
- I2C 마스터 초기화 (400kHz)
- Single-Shot 측정 모드 (커맨드: 0x2400)
- CRC-8 검증 (polynomial: 0x31)
- 측정 대기: 최대 15ms

#### DS18B20 (1-Wire 온도 센서)

필요 API:
```c
esp_err_t ds18b20_init(gpio_num_t data_gpio, gpio_num_t power_gpio);
esp_err_t ds18b20_power_on(void);
esp_err_t ds18b20_power_off(void);
esp_err_t ds18b20_read_temp(int sensor_idx, float *temperature);
esp_err_t ds18b20_start_conversion(void);
```

구현 포인트:
- 1-Wire 프로토콜 (Reset → ROM Command → Function Command)
- 12비트 해상도 (변환 시간 750ms)
- 다중 센서: ROM Search로 개별 식별
- Type B: GPIO 전원 스위칭 (`DS18B20_POWER_GPIO`)
- CRC-8 검증

### actuator — 출력 드라이버

#### SSR (Solid State Relay)

필요 API:
```c
esp_err_t ssr_init(gpio_num_t gpio, const char *name);
esp_err_t ssr_set_duty(uint8_t duty_percent);  // 0-100
esp_err_t ssr_force_off(void);
```

구현 포인트:
- Cycle Skipping: 10초 주기에서 duty_percent만큼 ON
- GPIO 출력 (High = ON)
- 안전: force_off는 즉시 차단

#### PWM Dimmer (LEDC)

필요 API:
```c
esp_err_t pwm_dimmer_init(gpio_num_t gpio);
esp_err_t pwm_dimmer_set_brightness(uint16_t brightness);  // 0-1000
esp_err_t pwm_dimmer_fade_to(uint16_t target, uint32_t duration_ms);
```

구현 포인트:
- LEDC 타이머 설정 (1kHz, 10비트 해상도)
- 점진적 페이드: `ledc_set_fade_with_time()` 사용
- 일출/일몰 시뮬레이션 (30분 gradual transition)

### control — 제어 알고리즘

#### PID Controller

필요 API:
```c
esp_err_t pid_init(float kp, float ki, float kd);
float pid_compute(float setpoint, float measurement, float dt);
esp_err_t pid_reset(void);
esp_err_t pid_set_limits(float min_output, float max_output);
```

구현 포인트:
- Anti-windup: 적분항 클램핑
- 출력 범위: 0~100 (SSR duty percent)
- dt: 1초 (Type A 측정 주기)

#### Scheduler

필요 API:
```c
esp_err_t scheduler_init(void);
esp_err_t scheduler_set_light_schedule(uint8_t on_hour, uint8_t off_hour,
                                        uint8_t sunrise_min, uint8_t sunset_min);
bool scheduler_is_light_on(void);
float scheduler_get_dimming_level(void);  // 0.0~1.0
```

구현 포인트:
- SNTP 또는 Thread 네트워크 시간 동기화
- 일출/일몰 구간에서 선형 또는 S-curve 디밍

#### Adaptive Poll (Type B 전용)

필요 API:
```c
esp_err_t adaptive_poll_init(void);
uint32_t adaptive_poll_get_interval(float current_temp, float prev_temp);
```

구현 포인트:
- |delta_temp| > 1.0°C → 30초 (빠른 폴링)
- |delta_temp| < 0.2°C → 300초 (느린 폴링)
- 중간값: 선형 보간
- RTC 메모리에 이전 온도 저장

### safety — 안전 감시

필요 API:
```c
esp_err_t safety_init(float overtemp_offset, uint32_t max_heater_sec);
safety_status_t safety_check(float temp_hot, float temp_cool,
                              float setpoint_hot, float setpoint_cool);
esp_err_t safety_emergency_shutdown(void);
```

구현 포인트:
- 과열: temp > setpoint + offset → 즉시 SSR OFF
- 센서 이상: -55°C 또는 +125°C 범위 초과 → 알림
- 히터 연속: 타이머 초과 → 강제 OFF + 쿨다운

### comm — 통신

#### Thread Node

필요 API:
```c
esp_err_t thread_node_init(bool is_router);
esp_err_t thread_node_send(const uint8_t *data, size_t len);
esp_err_t thread_node_set_rx_callback(void (*cb)(const uint8_t*, size_t));
```

구현 포인트:
- Type A: Router (상시 활성)
- Type B: SED (poll period = adaptive_poll 결과)
- CoAP over Thread로 데이터 전송
- Border Router로 메시지 전달

#### CBOR Codec

필요 API:
```c
esp_err_t cbor_encode_sensor_data(uint8_t *buf, size_t buf_size, size_t *out_len,
                                   float temp_hot, float temp_cool,
                                   float humidity, float battery_pct);
esp_err_t cbor_decode_command(const uint8_t *buf, size_t len,
                               command_t *cmd);
```

구현 포인트:
- tinycbor 라이브러리 사용
- CBOR Map 형식: {"th": temp_hot, "tc": temp_cool, "h": humidity, "b": battery}

### power — 전원 관리

필요 API:
```c
esp_err_t power_mgmt_init(void);
esp_err_t power_enter_deep_sleep(uint64_t sleep_us);
esp_err_t power_gpio_sensor_on(void);
esp_err_t power_gpio_sensor_off(void);

esp_err_t battery_monitor_init(adc_channel_t channel);
float battery_monitor_read_percent(void);
```

구현 포인트:
- Deep Sleep: `esp_deep_sleep_start()`
- RTC 메모리: `RTC_DATA_ATTR` 변수로 상태 보존
- 배터리 ADC: 분압기 보정 (100k/100k → x2)
- 충전 상태: 4.2V=100%, 3.0V=0% 선형 보간

### config — 설정 관리

필요 API:
```c
esp_err_t nvs_config_init(void);
esp_err_t nvs_config_save(const char *key, const void *data, size_t len);
esp_err_t nvs_config_load(const char *key, void *data, size_t max_len);

esp_err_t preset_manager_init(void);
esp_err_t preset_manager_load(const char *species, preset_t *preset);
```

구현 포인트:
- NVS 파티션: `nvs_open()` → `nvs_get_blob()` / `nvs_set_blob()`
- 프리셋: JSON 파싱 (cJSON 또는 컴파일타임 내장)

---

## 디버깅

### UART 모니터

```bash
idf.py -p /dev/ttyUSB0 monitor
# 로그 레벨 변경: menuconfig → Component config → Log → Default log verbosity
```

### 유용한 로그 필터

```bash
# 특정 컴포넌트만 보기
idf.py monitor | grep -E "\[(sht30|ds18b20|pid)\]"
```

### 일반적인 문제

| 증상 | 원인 | 해결 |
|------|------|------|
| I2C timeout | SHT30 미연결 / 풀업 누락 | 배선 확인, 4.7kΩ 풀업 |
| 1-Wire CRC error | DS18B20 접촉 불량 | 커넥터 점검, 4.7kΩ 풀업 |
| Thread not joining | Dataset 불일치 | Border Router dataset 확인 |
| Brownout reset | 배터리 부족 / 전류 부족 | 전원 확인, 커패시터 추가 |
| WDT reset | 태스크 블로킹 | vTaskDelay 확인, WDT timeout 조정 |
