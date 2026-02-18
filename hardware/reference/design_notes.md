# RBMS 하드웨어 설계 노트 (Design Notes)

ESP32-C6 기반 RBMS의 회로 설계 결정사항, 전력 분석, 타이밍 고려사항을 정리한다.

---

## 1. 전원 설계 고려사항

### 1.1 Type A: LDO (AMS1117-3.3)

Type A는 USB 5V 상시 전원이므로 설계 단순성을 우선한다.

**선정 근거:**
- 입력: USB 5V (4.75~5.25V)
- 출력: 3.3V, 최대 1A
- Dropout: 1.1V (typ.) -- 5V 입력에서 충분한 여유 (headroom 0.6V)
- ESP32-C6 피크 전류: ~350mA (Thread Radio TX) -- LDO 용량 충분

**열 설계:**
```
소비 전력 = (Vin - Vout) x Iload
         = (5.0 - 3.3) x 0.35
         = 0.595W (피크)
         = (5.0 - 3.3) x 0.08 = 0.136W (평균, Radio idle)

SOT-223 열저항: ~90 C/W (to ambient, 최소 패드)
피크 온도 상승: 0.595 x 90 = 53.6C
주변 온도 40C 기준: 93.6C -- AMS1117 최대 정션 온도 125C 이내
```

**권장사항:**
- PCB GND 패드를 최대한 크게 설계하여 방열 면적 확보
- 입력측 100uF 전해 + 10uF 세라믹, 출력측 22uF 세라믹 이상
- LDO 효율: 3.3/5.0 = 66% -- Type A는 상시 전원이므로 허용 가능

**LDO vs DC-DC 비교 (Type A):**

| 항목 | AMS1117 (LDO) | TPS62740 (DC-DC) |
|------|:--------------:|:-----------------:|
| 효율 (5V->3.3V) | 66% | ~90% |
| 발열 | 있음 | 거의 없음 |
| 노이즈 | 매우 낮음 | 스위칭 리플 존재 |
| 부품 수 | 2~3개 | 5~6개 (인덕터 포함) |
| 비용 | ~$0.10 | ~$1.50 |
| 결론 | **Type A에 적합** | 불필요한 복잡성 |

### 1.2 Type B: DC-DC Buck (TPS62740)

Type B는 배터리 수명이 최우선이므로 초저 quiescent current DC-DC를 사용한다.

**선정 근거:**
- IQ = 0.36uA (typ.) -- Deep Sleep 시 배터리 드레인 최소화
- 입력: 2.2~5.5V -- 21700 셀 전 범위(4.2~2.5V) 커버
- 출력: VSEL 핀으로 3.3V 고정 출력 설정
- 효율: 최대 95% (10mA 이상), 1mA 부하에서도 ~80%
- 출력 전류: 최대 400mA -- ESP32-C6 Thread SED TX에 충분

**VSEL 핀 설정 (3.3V 출력):**

| VSEL1 | VSEL2 | VSEL3 | 출력 전압 |
|:-----:|:-----:|:-----:|:--------:|
| GND | VIN | GND | 3.3V |

> TPS62740 데이터시트 Table 1 참조. VSEL 핀은 내부 풀다운 없으므로 확실히 GND 연결.

**외부 부품:**
- 인덕터: 2.2uH, 차폐형, Isat >= 600mA, DCR <= 0.2ohm
- 입력 캐패시터: 10uF 세라믹 (X5R, 가까이 배치)
- 출력 캐패시터: 4.7uF + 10uF 세라믹 (부하 과도 응답 개선)

**TP4056 + TPS62740 연결:**
```
USB-C 5V --> [TP4056 충전 모듈] --> VBATT (4.2V max)
                                      |
                                      +--> [TPS62740] --> 3.3V (ESP32-C6)
                                      |
                                      +--> [100k/100k 분압] --> ADC GPIO4
```

> TP4056 모듈에 과충전(4.2V)/과방전(2.5V) 보호 IC가 내장된 버전을 사용한다.

---

## 2. Deep Sleep 전류 소모 계산

### 2.1 ESP32-C6 Deep Sleep 전류

ESP32-C6 Deep Sleep 모드에서의 전류 소모 (데이터시트 기준):

| 상태 | 전류 | 비고 |
|------|-----:|------|
| Deep Sleep (RTC timer only) | 7 uA | RTC memory + timer 유지 |
| RTC peripherals OFF | -2 uA | `ESP_PD_DOMAIN_RTC_PERIPH` OFF 설정 |
| XTAL OFF | -1 uA | `ESP_PD_DOMAIN_XTAL` OFF 설정 |
| **ESP32-C6 Deep Sleep 합계** | **~5 uA** | firmware에서 비사용 도메인 OFF |

### 2.2 전체 시스템 Deep Sleep 전류 (Type B)

| 소비원 | 전류 | 비고 |
|--------|-----:|------|
| ESP32-C6 Deep Sleep | 5 uA | 위 계산 |
| TPS62740 IQ | 0.36 uA | 무부하 quiescent |
| 배터리 분압기 누설 | 21 uA | 4.2V / 200k (100k+100k) |
| DS18B20 (OFF) | 0 uA | BSS138로 전원 차단 |
| SHT30 (비활성) | 0.2 uA | I2C idle 상태 |
| BSS138 누설 | <0.1 uA | 무시 가능 |
| TP4056 대기 | ~2 uA | 충전 완료 후 대기 전류 |
| **합계** | **~29 uA** | |

### 2.3 배터리 수명 예측

**Wake 사이클 전류 프로파일:**

| 단계 | 시간 | 평균 전류 | 에너지 (uAh) |
|------|-----:|----------:|------------:|
| DS18B20 전원 ON + 안정화 | 10 ms | 5 mA | 0.014 |
| SHT30 측정 | 20 ms | 1.5 mA | 0.008 |
| DS18B20 변환 대기 | 750 ms | 5 mA | 1.042 |
| DS18B20 읽기 + 연산 | 50 ms | 20 mA | 0.278 |
| Thread SED TX | 200 ms | 80 mA | 4.444 |
| 네트워크 대기 (최대) | 5000 ms | 15 mA | 20.833 |
| **1회 Wake 합계** | **~6 s** | - | **~26.6 uAh** |

> 네트워크 대기는 최대 5초이나, 정상 연결 시 ~500ms로 단축. 실 사용 시 ~8 uAh/cycle 예상.

**배터리 수명 계산 (5000mAh, 5분 주기):**

```
1시간당 Wake 횟수: 60/5 = 12회
시간당 Wake 소모: 26.6 x 12 = 319.2 uAh = 0.319 mAh
시간당 Sleep 소모: 29 uA x 1h = 29 uAh = 0.029 mAh
시간당 총 소모: 0.319 + 0.029 = 0.348 mAh

배터리 수명 = 5000 / 0.348 = 14,368 시간 = 598 일 = ~1.6 년
```

**적응형 폴링 시 (300초 = 5분 slow / 30초 fast):**

| 시나리오 | 평균 주기 | 예상 수명 |
|----------|----------:|----------:|
| 안정 (slow 300s) | 300 s | ~1.6 년 |
| 변동 (fast 30s) | 30 s | ~6 개월 |
| 혼합 (80% slow) | ~246 s | ~1.3 년 |

### 2.4 분압기 전류 최적화

현재 100k+100k 분압기의 상시 누설 21uA는 Deep Sleep 전류(5uA)의 4배이다.

**개선 옵션:**

| 분압기 저항 | 누설 전류 | 전체 Sleep 전류 | 단점 |
|------------|----------:|--------------:|------|
| 100k + 100k (현재) | 21 uA | 29 uA | - |
| 470k + 470k | 4.5 uA | 12 uA | ADC 노이즈 소폭 증가 |
| 1M + 1M | 2.1 uA | 10 uA | ADC 소스 임피던스 높음, 100nF 필터 필요 |
| MOSFET 스위칭 | 0 uA | 8 uA | 부품 1개 추가 (BSS138) |

> 프로토타입에서는 100k+100k로 시작하고, 수명 목표 미달 시 고저항 또는 MOSFET 스위칭으로 전환한다.

---

## 3. 1-Wire 타이밍과 RISC-V Interrupt 보호

### 3.1 1-Wire 프로토콜 타이밍 요구사항

DS18B20의 1-Wire 프로토콜은 마이크로초 단위의 정밀한 타이밍을 요구한다:

| 동작 | 최소 | 일반 | 최대 | 단위 |
|------|-----:|-----:|-----:|------|
| Reset pulse LOW | 480 | 480 | - | us |
| Presence detect wait | - | 70 | - | us |
| Presence pulse | 60 | - | 240 | us |
| Write-1 LOW | 1 | 6 | 15 | us |
| Write-0 LOW | 60 | 60 | 120 | us |
| Read initiate LOW | 1 | 6 | 15 | us |
| Read sample point | - | 9 | 15 | us |
| Time slot | 60 | 65 | - | us |

### 3.2 Interrupt 보호 메커니즘

ESP32-C6의 RISC-V 코어에서 FreeRTOS tick interrupt (1ms) 또는 다른 interrupt가 1-Wire 타이밍 중에 발생하면 비트 에러가 생긴다.

**해결 방법: `portENTER_CRITICAL()` / `portEXIT_CRITICAL()`**

```c
static portMUX_TYPE s_ow_mux = portMUX_INITIALIZER_UNLOCKED;

static void ow_write_bit(int bit)
{
    portENTER_CRITICAL(&s_ow_mux);   // 모든 interrupt 비활성
    ow_set_output(0);
    if (bit) {
        ow_delay_us(6);              // Write-1: 6us LOW
        ow_set_output(1);
        ow_delay_us(54);             // 나머지 slot
    } else {
        ow_delay_us(60);             // Write-0: 60us LOW
        ow_set_output(1);
    }
    ow_delay_us(2);                  // Recovery time
    portEXIT_CRITICAL(&s_ow_mux);    // interrupt 복원
}
```

**Critical Section 지속 시간 분석:**

| 동작 | Critical Section 시간 | 비고 |
|------|---------------------:|------|
| Reset pulse | ~960 us | 가장 긴 구간 (480+70+410) |
| Write 1 byte (8 bits) | ~520 us | 8 x 65us |
| Read 1 byte (8 bits) | ~520 us | 8 x 65us |
| Scratchpad 읽기 (9 bytes) | ~4,680 us | 바이트 단위 critical section |

> **경고**: Reset pulse의 ~960us critical section은 FreeRTOS tick (1ms)과 유사한 길이이다.
> 시스템 tick이 지연될 수 있으나, 단발적이므로 실시간성에 큰 영향은 없다.

### 3.3 타이밍 구현: Busy-Wait vs Timer

```c
static inline void ow_delay_us(uint32_t us)
{
    uint64_t end = esp_timer_get_time() + us;
    while (esp_timer_get_time() < end) { }
}
```

- `esp_timer_get_time()`: 64-bit 마이크로초 카운터 (하드웨어 타이머 기반)
- Busy-wait 방식: critical section 내에서 사용 가능 (sleep/yield 불가)
- RISC-V 160MHz에서 루프 오버헤드: 약 0.1~0.5us (무시 가능)

### 3.4 CRC 검증의 중요성

1-Wire 통신의 노이즈 내성을 CRC로 보강한다:

- **ROM 코드 CRC8**: 센서 검색 시 8바이트 ROM의 CRC 검증 (polynomial 0x8C)
- **Scratchpad CRC8**: 온도 데이터 9바이트의 CRC 검증
- CRC 실패 시 `ESP_ERR_INVALID_CRC` 반환 -- 상위 레이어에서 재시도 또는 stale 데이터 사용

---

## 4. SSR Cycle Skipping vs PWM 비교

### 4.1 왜 SSR에 PWM을 쓰지 않는가

| 항목 | PWM (고속 스위칭) | Cycle Skipping (저속 ON/OFF) |
|------|:------------------:|:----------------------------:|
| 스위칭 주파수 | 1~10 kHz | 0.1 Hz (10초 주기) |
| EMI 방사 | 높음 (고주파 하모닉스) | 매우 낮음 |
| SSR 수명 | 단축 (잦은 스위칭) | 연장 |
| Zero Cross 활용 | 불가 (주기가 안 맞음) | 완벽 활용 |
| 온도 리플 | 매우 작음 | 다소 큼 (~0.5-1C) |
| 히터 응답 | 적합 (관성이 큰 부하) | **적합** (열 관성으로 리플 흡수) |
| 적합 부하 | 조명 디밍 (시각적 깜빡임) | **히터** (열 관성) |

### 4.2 Cycle Skipping 구현

```
10초 주기 (100 ticks x 100ms):

duty = 60% 일 때:
tick:  0  10  20  30  40  50  60  70  80  90  100
SSR:  |ON|ON|ON|ON|ON|ON|OFF|OFF|OFF|OFF|
       ^^^^^^^^^^^^^^^^^^^^^^^^ ^^^^^^^^^^^^
       60 ticks ON              40 ticks OFF
```

**코드:**
```c
void ssr_tick(int tick_100ms)
{
    for (int i = 0; i < SSR_MAX_CHANNELS; i++) {
        if (!s_ch_inited[i] || !s_ch[i].enabled) continue;
        bool on = (tick_100ms < (int)s_ch[i].duty);  // duty=60 -> tick 0~59 ON
        gpio_set_level(s_ch[i].gpio, on ? 1 : 0);
    }
}
```

### 4.3 LED 디밍: PWM이 적합한 이유

LED는 열 관성이 없고 시각적으로 깜빡임이 감지되므로 PWM 디밍을 사용한다:

- LEDC 하드웨어: 1kHz, 10-bit (1024 단계)
- XTAL 클럭 기반: CPU 주파수 변경에 무관한 안정적 PWM
- Fade 기능: 하드웨어 가속 fade (일출/일몰 시뮬레이션)
- IRF520 MOSFET: Low-side 스위칭, DC LED 스트립 전용

### 4.4 G3MB-202P Zero Cross 특성

G3MB-202P SSR은 내부 Zero Cross 회로를 탑재하여:
- GPIO가 HIGH가 되어도 다음 AC zero crossing에서 실제 ON
- GPIO가 LOW가 되면 현재 AC half-cycle 종료 후 OFF
- 결과: 돌입 전류 최소화, EMI 감소, 부하 보호

> Cycle Skipping의 10초 주기에서 Zero Cross는 자연스럽게 작동한다.
> 60Hz AC 기준 10초 = 600 cycles, 사실상 아날로그적 열 제어 효과.

---

## 5. EMI 고려사항

### 5.1 EMI 방사원과 대책

| 방사원 | 주파수 | 영향 | 대책 |
|--------|--------|------|------|
| SSR Zero Cross 스위칭 | 50/60 Hz | 낮음 | Zero Cross 내장으로 최소화 |
| LEDC PWM (LED) | 1 kHz | 중간 | DC 부하만 사용, AC 격리 |
| TPS62740 스위칭 | ~2 MHz | 중간 | 차폐 인덕터, 짧은 배선, 입출력 캐패시터 |
| ESP32-C6 디지털 회로 | 160 MHz + 하모닉스 | 있음 | GND plane, 바이패스 캐패시터 |
| 802.15.4 Radio | 2.4 GHz | 의도적 | 안테나 keep-out zone 준수 |

### 5.2 AC/DC 격리 설계

Type A 노드에서 AC 240V와 DC 3.3V 회로의 격리가 안전의 핵심이다:

```
[AC Side]                    [DC Side]
                 SSR 내부
AC L ---[SSR Load]---+      ESP32-C6
AC N ----------------+        |
                     |      GPIO -> 2N2222 -> SSR Input
                   광결합
                  (SSR 내장)
```

**설계 규칙:**
1. **PCB 격리 거리**: AC 트레이스와 DC 트레이스 사이 최소 6mm (IPC-2221, 240VAC)
2. **슬롯 컷**: AC/DC 경계에 PCB 슬롯을 넣어 creepage distance 확보
3. **GND 분리**: AC side GND와 DC side GND는 물리적으로 분리
4. **SSR 배치**: AC 터미널과 SSR을 PCB 한쪽 가장자리에 집중
5. **터미널 블록 방향**: AC 선이 DC 부품 위를 지나가지 않도록 배치

### 5.3 디지털 노이즈 억제

- **100nF 바이패스 캐패시터**: ESP32-C6 VDD 핀 가까이 (3개, 각 전원핀마다)
- **GND plane**: 4층 PCB 권장 (2층 시 최대한 넓은 GND 영역)
- **I2C 라인**: SHT30까지 30cm 이하, 트위스트 페어 또는 차폐 케이블
- **1-Wire 라인**: 프로브 케이블 3m 이하, 4.7k 풀업 (장거리 시 2.2k로 변경)

---

## 6. PCB 레이아웃 가이드라인

### 6.1 보드 크기 및 레이어 구성

**Type A (권장 크기: 80x50mm):**
- AC 전원부 + SSR이 크기를 지배
- 4층 권장: Top(신호) / GND / Power / Bottom(신호)
- 2층 가능: Top(신호+전원) / Bottom(GND, 최대 면적)

**Type B (권장 크기: 50x30mm):**
- 배터리 홀더가 별도이면 보드 자체는 소형화 가능
- 2층 충분: Top(부품+신호) / Bottom(GND)

### 6.2 컴포넌트 배치 우선순위

**Type A:**
```
+--------------------------------------------------+
|  [USB-C]  [AMS1117]  [100uF]                     |
|                                                    |
|  [ESP32-C6-WROOM-1]     [SHT30]                  |
|  (안테나 이쪽 ->)                                  |
|                          [DS18B20 JST x2]         |
|  [2N2222]  [2N2222]  [IRF520]                     |
|                                                    |
|  [SSR #0]  [SSR #1]  [PWM out]                    |
|  ............AC 격리 슬롯.............              |
|  [터미널: AC IN]  [터미널: Heater]  [터미널: UV]   |
+--------------------------------------------------+
```

**Type B:**
```
+----------------------------------+
|  [TP4056 모듈]                   |
|  [TPS62740] [L] [Cin] [Cout]    |
|                                   |
|  [ESP32-C6-WROOM-1]             |
|  (안테나 이쪽 ->)                |
|                                   |
|  [SHT30]  [BSS138 x2]           |
|  [DS18B20 JST x2]  [ADC 분압기] |
+----------------------------------+
      [배터리 홀더 (아래면 또는 별도)]
```

### 6.3 배선 규칙

| 항목 | 규칙 | 근거 |
|------|------|------|
| AC 트레이스 폭 | 1mm 이상 (2A 기준) | IPC-2221 온도 상승 10C 이하 |
| AC-DC 격리 | 6mm 이상 clearance | 240VAC, 오염도 2 기준 |
| DC 전원 트레이스 | 0.5mm 이상 | 피크 350mA |
| 신호 트레이스 | 0.2~0.3mm | I2C, 1-Wire, GPIO |
| GND via | 열 패드 아래 최소 4개 | LDO, DC-DC 방열 |
| 바이패스 캐패시터 | IC로부터 5mm 이내 | 고주파 디커플링 효과 |

### 6.4 안테나 클리어런스

ESP32-C6-WROOM-1 모듈의 PCB 안테나 성능을 위해:

- 안테나 영역(모듈 끝단 ~10mm)에 **GND plane 금지**
- 안테나 방향으로 PCB 가장자리 배치 (구리 없는 영역 확보)
- 안테나 위/아래 10mm 이내에 부품 배치 금지
- 금속 케이스 사용 시 안테나 방향에 플라스틱 창 확보

### 6.5 열 관리

**Type A 발열 부품:**
- AMS1117-3.3: SOT-223 열 패드를 PCB GND plane에 최대한 넓게 연결
- SSR G3MB-202P: SIP 패키지 자체 방열, 2A 정격 내 사용 시 추가 방열 불필요
- IRF520: TO-220 방열판 장착 가능하나, LED 전류 2A 이하 시 불필요

**Type B:**
- TPS62740: DSBGA 패키지, 효율 95%로 발열 최소
- 발열 관심 부품 없음

### 6.6 테스트 포인트

디버깅 및 품질 검사를 위해 아래 테스트 포인트를 PCB에 확보한다:

| 테스트 포인트 | 용도 |
|--------------|------|
| TP_3V3 | 3.3V 전원 레일 |
| TP_VBATT | 배터리 전압 (Type B) |
| TP_GND | 공통 그라운드 |
| TP_SDA | I2C SDA 신호 확인 |
| TP_SCL | I2C SCL 신호 확인 |
| TP_1W | 1-Wire 데이터 신호 |
| TP_SSR0 | SSR CH0 제어 신호 (Type A) |
| TP_PWM | PWM LED 출력 (Type A) |

---

## 7. 알려진 제한사항 및 향후 개선

### 7.1 현재 설계의 제한사항

| 항목 | 현재 상태 | 영향 | 개선 방안 |
|------|-----------|------|-----------|
| IRF520 Vgs(th) | 2~4V (3.3V 구동 불안정) | LED 최대 밝기 미달 가능 | IRLZ44N (logic level) 교체 |
| 배터리 분압기 누설 | 21uA (100k+100k) | Deep Sleep 전류의 72% | 1M+1M 또는 MOSFET 스위칭 |
| ADC 정밀도 | 12-bit, +/-1~2% | 배터리 잔량 오차 ~5% | Curve Fitting으로 보정 완료 |
| I2C bus recovery | SW 기반 9-pulse | SHT30 행업 시 자동 복구 | 하드웨어 I2C watchdog 추가 고려 |
| 1-Wire critical section | ~960us (reset) | FreeRTOS tick 지연 | 허용 범위 내 (1ms tick) |

### 7.2 향후 고려사항

- **USB-JTAG 디버깅 포트**: GPIO18/19를 외부 커넥터로 노출하여 개발 편의성 향상
- **외부 RTC**: Deep Sleep 정밀도 향상을 위한 외부 RTC IC (DS3231 등) 추가 가능
- **OTA 업데이트**: Thread 네트워크를 통한 무선 펌웨어 업데이트 지원
- **다채널 ADC**: 태양광 패널 전압 모니터링 등 추가 아날로그 입력
- **방수 등급**: IP65 이상의 방수 케이스 설계 (사육장 환경 고습도 대응)
