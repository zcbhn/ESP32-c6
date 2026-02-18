# RBMS-DEV-001: 개발 로드맵 & 아키텍처

**문서 번호**: RBMS-DEV-001
**프로젝트**: Reptile Breeding Management System (RBMS)
**작성일**: 2026-02-19
**상태**: Draft

---

## 1. 개발 단계 (Roadmap)

### 1.1 전체 개발 일정

| 단계 | 내용 | 예상 기간 | 산출물 |
|------|------|-----------|--------|
| Step 1 | 프로토타입 (센서 + Thread 기본) | 2~4주 | 센서 드라이버, Thread 통신 |
| Step 2 | Type A 풀 노드 (PID + SSR + 디밍) | 2~4주 | 완전한 Type A 펌웨어 |
| Step 3 | Type B 배터리 노드 (저전력) | 2~3주 | 완전한 Type B 펌웨어 |
| Step 4 | 서버 + 대시보드 | 1~2주 | 서버 스택 배포 |
| Step 5 | 문서화 + GitHub 공개 | 1~2주 | 기술 문서, CI/CD |

**총 예상 기간**: 약 2~4개월

### 1.2 Step 1: 프로토타입

**목표**: 기본 센서 읽기 및 Thread 네트워크 통신 확인

| 작업 | 상세 | 완료 기준 |
|------|------|-----------|
| SHT30 드라이버 | I2C 초기화, Single-Shot 측정, CRC 검증 | 온습도 값 UART 출력 |
| DS18B20 드라이버 | 1-Wire bit-bang, ROM Search, 듀얼 센서 | 2개 센서 온도 출력 |
| Thread 초기화 | OpenThread 스택 초기화, Dataset 생성 | Leader/Router 역할 획득 |
| UDP 송수신 | CBOR 인코딩 후 UDP Multicast 전송 | 패킷 도달 확인 |
| CBOR 코덱 | 정수 키 기반 인코더/디코더 | 인코딩→디코딩 왕복 검증 |

### 1.3 Step 2: Type A 풀 노드

**목표**: FreeRTOS 기반 멀티태스크 환경에서 PID 제어 및 스케줄링 동작

| 작업 | 상세 | 완료 기준 |
|------|------|-----------|
| SSR 드라이버 | 2채널 Cycle Skipping, force_off | 히터 ON/OFF 확인 |
| PWM Dimmer | LEDC 1kHz/10-bit, Fade 기능 | LED 디밍 확인 |
| PID 컨트롤러 | Derivative-on-measurement, Anti-windup | 목표 온도 +-0.5C 유지 |
| 스케줄러 | 일출/일몰 디밍, 자정 경과 지원 | 시간대별 조명 자동 전환 |
| Safety Monitor | 7단계 검사 파이프라인 | 과열 시 SSR 차단 확인 |
| FreeRTOS 태스크 | 4개 태스크 생성 및 우선순위 조정 | WDT 리셋 없이 안정 동작 |
| NVS 설정 | 프리셋 저장/복원, 유효성 검증 | 전원 재투입 후 설정 유지 |

### 1.4 Step 3: Type B 배터리 노드

**목표**: Deep Sleep 기반 저전력 동작으로 배터리 수명 6개월+ 달성

| 작업 | 상세 | 완료 기준 |
|------|------|-----------|
| Deep Sleep | Timer Wakeup, GPIO Hold, RTC PD | 전류 < 10uA (Sleep) |
| Power Management | GPIO 전원 스위칭, RTC 메모리 활용 | DS18B20 OFF 시 누설 없음 |
| 배터리 모니터 | ADC 16x 멀티샘플링, Curve Fitting Cal | 전압 오차 < 50mV |
| 적응형 폴링 | 온도 변화율 기반 주기 조절 | 안정 시 300초, 급변 시 30초 |
| SED 최적화 | Child Timeout, Poll Period 설정 | 네트워크 유지 + 저전력 |
| 순차 실행 | Wake→측정→전송→Sleep 단일 흐름 | 1회 사이클 < 1.5초 |

### 1.5 Step 4: 서버 + 대시보드

**목표**: Border Router부터 Grafana까지 데이터 파이프라인 완성

| 작업 | 상세 | 완료 기준 |
|------|------|-----------|
| ot-br-posix | Border Router 설치 및 네트워크 구성 | Thread 노드 데이터 수신 |
| UDP Gateway | Thread UDP → MQTT 변환 | MQTT 토픽에 데이터 게시 |
| Mosquitto | 인증, ACL, 보안 설정 | 인증된 클라이언트만 접근 |
| MQTT-InfluxDB Bridge | Python CBOR 디코딩, 버퍼 쓰기 | InfluxDB에 데이터 기록 |
| InfluxDB | DB 생성, 90일 retention policy | 쿼리로 데이터 확인 |
| Grafana | 대시보드 프로비저닝, 패널 구성 | 실시간 온습도 그래프 표시 |

### 1.6 Step 5: 문서화 + 공개

**목표**: 재현 가능한 수준의 기술 문서 및 오픈소스 공개

| 작업 | 상세 | 완료 기준 |
|------|------|-----------|
| 시스템 사양서 | RBMS-SPEC-001 | Markdown/PDF 완성 |
| 로드맵/아키텍처 | RBMS-DEV-001 | Markdown/PDF 완성 |
| 하드웨어 참조 | RBMS-HW-001 | 회로도, BOM, PCB 가이드 |
| CI/CD | GitHub Actions 빌드 검증 | PR 시 자동 빌드 통과 |
| README | 빠른 시작 가이드 | 클론→빌드→동작 재현 |

---

## 2. 펌웨어 아키텍처

### 2.1 4계층 모듈 구조

```
┌───────────────────────────────────────────────────────┐
│                  Application Layer                     │
│                                                        │
│    app_type_a.c              app_type_b.c              │
│    (FreeRTOS 4-Task)         (Deep Sleep Sequential)   │
│                                                        │
├───────────────────────────────────────────────────────┤
│                   Service Layer                        │
│                                                        │
│  pid_controller  │  scheduler  │  adaptive_poll        │
│  safety_monitor  │             │                       │
│                                                        │
├───────────────────────────────────────────────────────┤
│                   Driver Layer                         │
│                                                        │
│  sht30     │ ds18b20  │  ssr       │ pwm_dimmer        │
│  power_mgmt│ battery_monitor                           │
│                                                        │
├───────────────────────────────────────────────────────┤
│                  Platform Layer                        │
│                                                        │
│  thread_node  │  cbor_codec  │  nvs_config             │
│  preset_manager│  ota_update  │                        │
│                                                        │
└───────────────────────────────────────────────────────┘
```

#### 계층별 역할

| 계층 | 역할 | 의존 방향 |
|------|------|-----------|
| Application | 노드 타입별 진입점, 태스크 구성, 컴포넌트 조합 | 아래 3개 계층 사용 |
| Service | 제어 알고리즘, 안전 정책, 스케줄링 로직 | Driver/Platform 사용 |
| Driver | 센서/액추에이터/전원 하드웨어 추상화 | ESP-IDF HAL 사용 |
| Platform | 네트워크 통신, 직렬화, 설정 영속화 | ESP-IDF 서비스 사용 |

### 2.2 컴포넌트 의존성 그래프

```
main
 ├── sensor     → driver, esp_timer
 ├── actuator   → driver
 ├── control    → log
 ├── safety     → log, esp_timer
 ├── comm       → log, openthread, esp_netif, esp_event, vfs
 ├── power      → log, esp_adc, esp_hw_support, driver
 ├── config     → log, nvs_flash
 └── nvs_flash
```

### 2.3 소스 파일 구조

```
firmware/
├── CMakeLists.txt                    # 프로젝트 빌드 설정
├── Kconfig                           # 노드 타입 및 파라미터 설정
├── partitions.csv                    # Flash 파티션 테이블
├── sdkconfig.defaults.type_a         # Type A 기본 설정
├── main/
│   ├── CMakeLists.txt                # 조건부 소스 선택
│   ├── app_main.c                    # 엔트리포인트 (NVS 초기화, 타입 디스패치)
│   ├── app_type_a.c                  # Type A 풀 노드 (FreeRTOS 태스크)
│   └── app_type_b.c                  # Type B 센서 노드 (Deep Sleep 루프)
└── components/
    ├── sensor/
    │   ├── include/sht30.h
    │   ├── include/ds18b20.h
    │   ├── sht30.c
    │   ├── ds18b20.c
    │   └── CMakeLists.txt
    ├── actuator/
    │   ├── include/ssr.h
    │   ├── include/pwm_dimmer.h
    │   ├── ssr.c
    │   ├── pwm_dimmer.c
    │   └── CMakeLists.txt
    ├── control/
    │   ├── include/pid.h
    │   ├── include/scheduler.h
    │   ├── include/adaptive_poll.h
    │   ├── pid.c
    │   ├── scheduler.c
    │   ├── adaptive_poll.c
    │   └── CMakeLists.txt
    ├── safety/
    │   ├── include/safety_monitor.h
    │   ├── safety_monitor.c
    │   └── CMakeLists.txt
    ├── comm/
    │   ├── include/thread_node.h
    │   ├── include/cbor_codec.h
    │   ├── thread_node.c
    │   ├── cbor_codec.c
    │   └── CMakeLists.txt
    ├── power/
    │   ├── include/power_mgmt.h
    │   ├── include/battery_monitor.h
    │   ├── power_mgmt.c
    │   ├── battery_monitor.c
    │   └── CMakeLists.txt
    └── config/
        ├── include/nvs_config.h
        ├── include/preset_manager.h
        ├── nvs_config.c
        ├── preset_manager.c
        └── CMakeLists.txt
```

---

## 3. FreeRTOS 태스크 구조 (Type A)

### 3.1 태스크 목록

| 태스크 | 함수 | 우선순위 | 스택 | 주기 | 역할 |
|--------|------|----------|------|------|------|
| safety | `safety_task()` | 6 (최고) | 4096 | 1초 | 안전 검사, 긴급 차단 |
| sensor | `sensor_task()` | 5 | 4096 | 1초 | SHT30/DS18B20 측정 |
| control | `control_task()` | 4 | 4096 | 100ms | PID 연산, SSR 제어, 스케줄러 |
| thread | `thread_task()` | 3 (최저) | 4096 | 10초 | CBOR 인코딩, Thread 전송 |
| ot_main | `thread_main_task()` | 5 | 6144 | - | OpenThread mainloop |

### 3.2 태스크 간 데이터 공유

```c
/* 공유 변수 (volatile) — 태스크 간 데이터 전달 */
static volatile float s_temp_hot  = 0.0f;   // sensor → control, safety, thread
static volatile float s_temp_cool = 0.0f;   // sensor → control, safety, thread
static volatile float s_humidity  = 0.0f;   // sensor → thread
static volatile safety_status_t s_safety;    // safety → control
```

- sensor 태스크가 값을 갱신하고, control/safety/thread 태스크가 읽음
- `volatile` 키워드로 컴파일러 최적화 방지
- 단일 writer 패턴이므로 mutex 없이 안전 (atomic float read/write)

### 3.3 Watchdog 설정

- 각 태스크는 `esp_task_wdt_add(NULL)` 로 WDT에 등록
- 매 루프에서 `esp_task_wdt_reset()` 호출
- 태스크 블로킹 시 WDT 리셋 발생 → 문제 진단

### 3.4 Type A 실행 흐름

```
app_main()
  │
  ├── nvs_flash_init()
  ├── [Kconfig: NODE_TYPE_A]
  │
  └── app_type_a_start()
        │
        ├── nvs_config_init()
        ├── preset_load(&s_preset)
        │
        ├── sht30_init(I2C_NUM_0, GPIO6, GPIO7, 0x44)
        ├── ds18b20_init(GPIO8, GPIO_NUM_NC)    // 상시 전원
        │
        ├── ssr_init(0, GPIO10, "heater")
        ├── ssr_init(1, GPIO11, "uv_light")
        ├── pwm_dimmer_init(GPIO3)
        │
        ├── pid_init(&s_pid, Kp, Ki, Kd)
        ├── pid_set_setpoint(&s_pid, preset.temp_hot.target)
        │
        ├── scheduler_init()
        ├── scheduler_set_light(&light_sched)
        │
        ├── safety_init(&safety_cfg)
        │
        ├── thread_node_init(true)     // Router 모드
        ├── thread_node_start()
        │
        ├── xTaskCreate(sensor_task,  prio=5)
        ├── xTaskCreate(control_task, prio=4)
        ├── xTaskCreate(safety_task,  prio=6)
        └── xTaskCreate(thread_task,  prio=3)
```

### 3.5 Type B 실행 흐름 (Deep Sleep 순차)

```
app_main()
  │
  ├── nvs_flash_init()
  ├── [Kconfig: NODE_TYPE_B]
  │
  └── app_type_b_start()
        │
        ├── s_boot_count++
        ├── power_mgmt_init()
        ├── nvs_config_init()
        ├── preset_load(&preset)
        │
        ├── [DS18B20 전원 ON]
        │   ds18b20_init(GPIO8, GPIO9)
        │   ds18b20_power_on()
        │
        ├── [SHT30 측정]
        │   sht30_init(I2C_NUM_0, GPIO6, GPIO7, 0x44)
        │   sht30_read(&sht_data)
        │
        ├── [DS18B20 변환 + 읽기]
        │   ds18b20_search(&count)
        │   ds18b20_start_conversion()
        │   vTaskDelay(750ms)
        │   ds18b20_read_temp(0, &temp_hot)
        │   ds18b20_read_temp(1, &temp_cool)
        │
        ├── [안전 검사]
        │   safety_init(&scfg)
        │   safety_check(temp_hot, temp_cool, ...)
        │
        ├── [배터리 (조건부, 30회마다)]
        │   battery_monitor_init(ADC_CHANNEL_0)
        │   battery_monitor_read_percent()
        │   battery_monitor_deinit()
        │
        ├── [CBOR 인코딩 + Thread 전송]
        │   cbor_encode_report(&report, buf, ...)
        │   thread_node_init(false)    // SED 모드
        │   thread_node_start()
        │   [연결 대기 최대 5초]
        │   thread_node_send(buf, len)
        │   thread_node_stop()
        │
        ├── [적응형 폴링]
        │   adaptive_poll_init(&apcfg)
        │   sleep_sec = adaptive_poll_calc(temp_hot, s_prev_temp)
        │   s_prev_temp = temp_hot      // RTC 메모리 저장
        │
        ├── [센서 해제]
        │   ds18b20_power_off()
        │   ds18b20_deinit()
        │   sht30_deinit()
        │
        └── [Deep Sleep 진입]
            power_mgmt_deep_sleep(sleep_sec)
            // 이후 코드 실행 안 됨 → Timer Wakeup → app_main() 재진입
```

---

## 4. 데이터 흐름

### 4.1 Type A 센서 → 서버 데이터 흐름

```
[SHT30 / DS18B20]
      │ (I2C / 1-Wire)
      ▼
  sensor_task (1초 주기)
      │ volatile 변수에 기록
      ▼
  control_task (100ms 주기)
      │ PID 연산 → SSR duty 설정
      │ scheduler_tick() → PWM 설정
      ▼
  safety_task (1초 주기)
      │ 7단계 안전 검사
      │ FAULT 시 → ssr_force_off_all() + pwm_dimmer_set(0)
      ▼
  thread_task (10초 주기)
      │ sensor_report_t 구조체 조립
      │ cbor_encode_report() → 최대 43 bytes
      │ esp_openthread_lock_acquire()
      │ thread_node_send() → UDP Multicast ff03::1:5684
      │ esp_openthread_lock_release()
      ▼
  [Thread Radio] → [Border Router] → [UDP Gateway]
      ▼
  [Mosquitto MQTT]
      │ 토픽: rbms/<node_id>/telemetry
      ▼
  [mqtt_influx_bridge.py]
      │ cbor2.loads() → FIELD_MAP으로 키 변환
      │ 버퍼 (10개 또는 5초 주기) 일괄 쓰기
      ▼
  [InfluxDB]
      │ measurement: "telemetry"
      │ tags: {node_id}
      │ fields: {temp_hot, temp_cool, humidity, heater_duty, light_duty, safety}
      ▼
  [Grafana Dashboard]
```

### 4.2 제어 루프 데이터 흐름 (Type A)

```
DS18B20 temp_hot
      │
      ▼
  pid_compute(measurement=temp_hot, setpoint=preset.temp_hot.target, dt=1.0)
      │
      │  P = Kp * error
      │  I += error * dt → back-calc anti-windup
      │  D = -Kd * (measurement - prev) / dt   (derivative-on-measurement)
      │
      ▼
  output = clamp(P + I + D, 0.0, 100.0)
      │
      ▼
  ssr_set_duty(0, output)
      │
      ▼
  ssr_tick(counter % 100)
      │ 10초 주기, duty% 시간만큼 GPIO HIGH
      ▼
  [SSR G3MB-202P] → [히터 전원 ON/OFF]
```

### 4.3 조명 제어 데이터 흐름 (Type A)

```
시스템 시각 (RTC / SNTP)
      │
      ▼
  scheduler_tick()
      │ is_in_light_period() → 점등 구간 판단
      │ 일출 전이: dimming = elapsed / sunrise_min
      │ 일몰 전이: dimming = remaining / sunset_min
      │ 정상 점등: dimming = 1.0
      ▼
  scheduler_is_light_on() → true/false
  scheduler_get_dimming() → 0.0 ~ 1.0
      │
      ▼
  pwm_dimmer_set(dimming * 1000)
      │ LEDC duty = brightness * 1023 / 1000
      ▼
  [MOSFET IRF520] → [LED Strip]
```

---

## 5. API 레퍼런스 요약

### 5.1 sensor 컴포넌트

#### sht30.h

```c
esp_err_t sht30_init(i2c_port_t port, gpio_num_t sda, gpio_num_t scl, uint8_t addr);
esp_err_t sht30_read(sht30_data_t *data);
esp_err_t sht30_reset(void);
esp_err_t sht30_deinit(void);
```

- `sht30_data_t`: `{ float temperature; float humidity; }`
- I2C 400kHz, CRC-8 검증, bus recovery (9 clock pulses), 최대 2회 재시도

#### ds18b20.h

```c
esp_err_t ds18b20_init(gpio_num_t data_gpio, gpio_num_t power_gpio);
esp_err_t ds18b20_power_on(void);
esp_err_t ds18b20_power_off(void);
esp_err_t ds18b20_search(int *count);
esp_err_t ds18b20_start_conversion(void);
esp_err_t ds18b20_read_temp(int idx, float *temperature);
const ds18b20_sensor_t *ds18b20_get_sensors(void);
esp_err_t ds18b20_deinit(void);
```

- `ds18b20_sensor_t`: `{ uint8_t rom[8]; float temperature; bool valid; }`
- `DS18B20_MAX_SENSORS = 2`
- 1-Wire bit-bang, `portENTER_CRITICAL` 타이밍 보호

### 5.2 actuator 컴포넌트

#### ssr.h

```c
esp_err_t ssr_init(int channel, gpio_num_t gpio, const char *name);
esp_err_t ssr_set_duty(int channel, uint8_t duty_percent);
uint8_t   ssr_get_duty(int channel);
esp_err_t ssr_force_off(int channel);
esp_err_t ssr_force_off_all(void);
void      ssr_tick(int tick_100ms);
```

- `SSR_MAX_CHANNELS = 2`
- Cycle Skipping: `ssr_tick()` 을 100ms 주기로 호출, tick_100ms = 0~99

#### pwm_dimmer.h

```c
esp_err_t pwm_dimmer_init(gpio_num_t gpio);
esp_err_t pwm_dimmer_set(uint16_t brightness);           // 0~1000
esp_err_t pwm_dimmer_fade_to(uint16_t target, uint32_t duration_ms);
uint16_t  pwm_dimmer_get(void);
esp_err_t pwm_dimmer_deinit(void);
```

- LEDC Timer 0, Channel 0, 1kHz, 10-bit, XTAL_CLK

### 5.3 control 컴포넌트

#### pid.h

```c
esp_err_t pid_init(pid_ctrl_t *pid, float kp, float ki, float kd);
esp_err_t pid_set_limits(pid_ctrl_t *pid, float min_out, float max_out);
esp_err_t pid_set_setpoint(pid_ctrl_t *pid, float setpoint);
float     pid_compute(pid_ctrl_t *pid, float measurement, float dt);
void      pid_reset(pid_ctrl_t *pid);
```

- `pid_ctrl_t`: `{ kp, ki, kd, setpoint, integral, prev_measurement, out_min, out_max }`

#### scheduler.h

```c
esp_err_t scheduler_init(void);
esp_err_t scheduler_set_light(const light_schedule_t *sched);
esp_err_t scheduler_set_time(uint8_t hour, uint8_t minute);
bool      scheduler_is_light_on(void);
float     scheduler_get_dimming(void);
void      scheduler_tick(void);
```

- `light_schedule_t`: `{ on_hour, off_hour, sunrise_min, sunset_min }`

#### adaptive_poll.h

```c
esp_err_t adaptive_poll_init(const adaptive_poll_config_t *cfg);
uint32_t  adaptive_poll_calc(float current_temp, float prev_temp);
```

- `adaptive_poll_config_t`: `{ period_fast_s, period_slow_s, delta_high }`

### 5.4 safety 컴포넌트

#### safety_monitor.h

```c
esp_err_t       safety_init(const safety_config_t *cfg);
safety_status_t safety_check(float temp_hot, float temp_cool, float setpoint, float humidity);
void            safety_heater_tick(bool heater_on);
void            safety_heater_timer_reset(void);
esp_err_t       safety_emergency_shutdown(void);
const char     *safety_status_str(safety_status_t status);
```

- `safety_config_t`: `{ overtemp_offset, heater_max_sec, stale_timeout_sec, sensor_mismatch_c, rate_max_per_sec }`

### 5.5 comm 컴포넌트

#### thread_node.h

```c
esp_err_t thread_node_init(bool is_router);
esp_err_t thread_node_start(void);
esp_err_t thread_node_send(const uint8_t *data, size_t len);
esp_err_t thread_node_set_rx_callback(thread_rx_cb_t cb);
esp_err_t thread_node_set_poll_period(uint32_t period_ms);
bool      thread_node_is_connected(void);
esp_err_t thread_node_stop(void);
```

- `thread_rx_cb_t`: `void (*)(const uint8_t *data, size_t len)`
- UDP Port: 5684, Multicast: `ff03::1`

#### cbor_codec.h

```c
esp_err_t cbor_encode_report(const sensor_report_t *report,
                              uint8_t *buf, size_t buf_size, size_t *out_len);
esp_err_t cbor_decode_report(const uint8_t *buf, size_t len, sensor_report_t *report);
```

- `sensor_report_t`: `{ temp_hot, temp_cool, humidity, battery_pct, heater_duty, light_duty, safety_status }`
- 최소 버퍼: 64 bytes

### 5.6 power 컴포넌트

#### power_mgmt.h

```c
esp_err_t                power_mgmt_init(void);
esp_sleep_wakeup_cause_t power_mgmt_wakeup_cause(void);
bool                     power_mgmt_is_first_boot(void);
esp_err_t                power_mgmt_register_hold_gpio(int gpio);
void                     power_mgmt_deep_sleep(uint32_t sleep_sec);
```

#### battery_monitor.h

```c
esp_err_t battery_monitor_init(adc_channel_t channel);
float     battery_monitor_read_voltage(void);
float     battery_monitor_read_percent(void);
bool      battery_monitor_is_low(void);
esp_err_t battery_monitor_deinit(void);
```

- ADC Unit 1, 12-bit, Attenuation 12dB, Curve Fitting Cal
- 분압비 x2, 만충 4.2V, 방전 3.0V, 저전압 3.2V

### 5.7 config 컴포넌트

#### nvs_config.h

```c
esp_err_t nvs_config_init(void);
esp_err_t nvs_config_save_blob(const char *key, const void *data, size_t len);
esp_err_t nvs_config_load_blob(const char *key, void *data, size_t max_len, size_t *out_len);
esp_err_t nvs_config_save_u32(const char *key, uint32_t value);
esp_err_t nvs_config_load_u32(const char *key, uint32_t *value);
esp_err_t nvs_config_erase(const char *key);
esp_err_t nvs_config_deinit(void);
```

- NVS Namespace: `"rbms"`

#### preset_manager.h

```c
esp_err_t preset_load_default(preset_t *preset);
esp_err_t preset_load(preset_t *preset);
esp_err_t preset_save(const preset_t *preset);
```

- `preset_t`: species, temp_hot, temp_cool, humidity, light, pid, safety 구조체
- NVS Key: `"preset"`, blob 직렬화
- 유효성 검증: 온도 범위 -20~80C, PID >= 0, NaN 검사, 시간 0~23

---

## 6. 빌드 방법

### 6.1 필수 환경

| 도구 | 버전 | 비고 |
|------|------|------|
| ESP-IDF | v5.2+ | ESP32-C6 지원 필수 |
| Python | 3.8+ | ESP-IDF 빌드 도구 |
| CMake | 3.16+ | ESP-IDF 내장 |
| Ninja | - | ESP-IDF 내장 |

### 6.2 ESP-IDF 설치

```bash
# Linux/macOS
mkdir -p ~/esp && cd ~/esp
git clone -b v5.2 --recursive https://github.com/espressif/esp-idf.git
cd esp-idf && ./install.sh esp32c6
source export.sh

# Windows
# ESP-IDF Tools Installer 사용 (설치 경로: C:\esp\esp-idf)
# 또는 tools/install_idf.bat 실행
```

### 6.3 펌웨어 빌드

```bash
cd firmware

# 타겟 설정
idf.py set-target esp32c6

# 노드 타입 선택 (menuconfig)
idf.py menuconfig
# → RBMS Configuration → Node Type → TYPE_A 또는 TYPE_B

# 빌드
idf.py build

# 플래시 & 모니터
idf.py -p COM3 flash monitor    # Windows
idf.py -p /dev/ttyUSB0 flash monitor  # Linux
```

### 6.4 스크립트 빌드 (Windows)

```bash
# Type B (기본, 배터리 SED)
tools/build_firmware.bat

# Type A (Router, 전원 공급)
tools/build_type_a.bat
```

### 6.5 Kconfig 주요 설정

| 메뉴 경로 | 설정 | 기본값 | 설명 |
|-----------|------|--------|------|
| Node Type | `NODE_TYPE_A` / `NODE_TYPE_B` | TYPE_B | 노드 타입 선택 |
| Sensor | `SENSOR_SHT30_ADDR` | 0x44 | SHT30 I2C 주소 |
| Sensor | `SENSOR_DS18B20_GPIO` | 2 | DS18B20 데이터 핀 |
| Sensor | `DS18B20_POWER_GPIO` | 3 | DS18B20 전원 핀 (Type B) |
| Actuator | `SSR_HEATER_GPIO` | 3 | SSR 히터 GPIO (Type A) |
| Actuator | `SSR_LIGHT_GPIO` | 5 | SSR UV 조명 GPIO (Type A) |
| Actuator | `PWM_DIMMING_GPIO` | 10 | LED 디밍 GPIO (Type A) |
| PID | `PID_KP` / `KI` / `KD` | 200/50/100 | PID 파라미터 x100 |
| Adaptive | `POLL_PERIOD_FAST` | 30 | 빠른 폴링 (초) |
| Adaptive | `POLL_PERIOD_SLOW` | 300 | 느린 폴링 (초) |
| Adaptive | `POLL_DELTA_HIGH` | 10 | 급변 임계값 x10 (C) |
| Adaptive | `BATTERY_CHECK_INTERVAL` | 30 | 배터리 체크 간격 (폴링 횟수) |
| Safety | `SAFETY_OVERTEMP_OFFSET` | 50 | 과열 오프셋 x10 (C) |
| Safety | `HEATER_MAX_CONTINUOUS` | 3600 | 히터 최대 연속 시간 (초) |

### 6.6 조건부 컴파일

`main/CMakeLists.txt` 에서 노드 타입에 따라 소스 파일을 선택적으로 빌드한다.

```cmake
set(MAIN_SRCS "app_main.c")

if(CONFIG_NODE_TYPE_A)
    list(APPEND MAIN_SRCS "app_type_a.c")
endif()

if(CONFIG_NODE_TYPE_B)
    list(APPEND MAIN_SRCS "app_type_b.c")
endif()
```

`app_main.c` 에서 전처리기로 분기:

```c
#if defined(CONFIG_NODE_TYPE_A)
    app_type_a_start();
#elif defined(CONFIG_NODE_TYPE_B)
    app_type_b_start();
#endif
```

---

## 7. 코드 컨벤션

### 7.1 일반 규칙

| 항목 | 규칙 |
|------|------|
| 언어 | C (ESP-IDF 스타일) |
| 들여쓰기 | 4 spaces (탭 없음) |
| 함수 네이밍 | snake_case (`sht30_read`, `pid_compute`) |
| 상수/매크로 | UPPER_CASE (`SSR_MAX_CHANNELS`, `CBOR_FLOAT32`) |
| 헤더 가드 | `#ifndef RBMS_COMPONENT_H` / `#define` / `#endif` |
| C++ 호환 | `extern "C" { }` 블록 |
| 에러 반환 | `esp_err_t` (`ESP_OK`, `ESP_ERR_*`) |
| 로깅 | `ESP_LOGI`, `ESP_LOGW`, `ESP_LOGE` (컴포넌트별 TAG) |
| 주석 | 한국어 (코드 내), 영어 (API 문서) |

### 7.2 컴포넌트 구조 표준

```
components/<name>/
├── CMakeLists.txt          # idf_component_register(SRCS, INCLUDE_DIRS, REQUIRES)
├── include/
│   └── <name>.h            # 공개 API 헤더 (타입 정의, 함수 선언)
└── <name>.c                # 구현 (static 내부 함수, 모듈 상태)
```

### 7.3 GPIO 핀 매핑

GPIO 핀 번호는 Kconfig에서 관리하며, 소스 코드에 하드코딩하지 않는다.
`CONFIG_SENSOR_DS18B20_GPIO`, `CONFIG_SSR_HEATER_GPIO` 등의 매크로를 사용한다.

---

## 8. 디버깅 가이드

### 8.1 UART 모니터

```bash
idf.py -p COM3 monitor
```

### 8.2 로그 레벨 제어

`menuconfig` > Component config > Log > Default log verbosity 에서 설정.
또는 코드에서 `esp_log_level_set("TAG", ESP_LOG_DEBUG)` 사용.

### 8.3 일반적인 문제 해결

| 증상 | 원인 | 해결 |
|------|------|------|
| I2C timeout | SHT30 미연결, 풀업 누락 | 배선 확인, 4.7k 풀업 추가 |
| 1-Wire CRC error | DS18B20 접촉 불량 | 커넥터 점검, 4.7k 풀업 |
| Thread not joining | Dataset 불일치 | Border Router dataset 확인 |
| Brownout reset | 전원 부족 | 커패시터 추가, 배터리 충전 |
| WDT reset | 태스크 블로킹 | vTaskDelay 확인, WDT timeout 조정 |
| NVS 초기화 실패 | 파티션 깨짐 | `nvs_flash_erase()` 후 재시도 |
| ADC 값 부정확 | 캘리브레이션 미적용 | Curve Fitting 활성화 확인 |
