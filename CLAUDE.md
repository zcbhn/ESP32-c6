# CLAUDE.md — Reptile Habitat Monitor (RBMS)

## 프로젝트 개요

ESP32-C6 + OpenThread 기반 파충류 사육장 환경 모니터링 & 제어 시스템.
Thread 메시 네트워크로 다수의 사육장을 통합 관리하며, 히터/조명을 자동 제어한다.

## 핵심 개념

- **Type A 노드**: 콘센트 전원, Thread Router, FreeRTOS 멀티태스크, SSR/PWM 제어
- **Type B 노드**: 배터리 전원, Thread SED, Deep Sleep 기반 순차 실행, 센서 전용
- **Thread**: IEEE 802.15.4 기반 저전력 메시 네트워크 (Wi-Fi 불필요)
- **CBOR**: 경량 바이너리 직렬화 (정수 키 사용)

## 빌드 방법

```bash
# ESP-IDF v5.2.3 (설치 경로: C:\esp\esp-idf)

# Type B (기본, 배터리 SED)
tools/build_firmware.bat

# Type A (Router, 전원 공급)
tools/build_type_a.bat

# 수동 빌드
cd firmware
idf.py set-target esp32c6
idf.py build
idf.py -p COM3 flash monitor
```

- 타겟: `esp32c6`
- Kconfig에서 `NODE_TYPE_A` 또는 `NODE_TYPE_B` 선택
- `sdkconfig.defaults`에 OpenThread, PM, Watchdog, 4MB Flash 설정

## 디렉토리 구조

```
firmware/
├── main/
│   ├── app_main.c          # 엔트리포인트 (NVS 초기화 → 노드 타입 디스패치)
│   ├── app_type_a.c        # Type A 풀 노드 (FreeRTOS 태스크)
│   └── app_type_b.c        # Type B 센서 노드 (Deep Sleep 루프)
└── components/
    ├── sensor/              # SHT30 (I2C, 버스 복구), DS18B20 (1-Wire 듀얼)
    ├── actuator/            # SSR (Cycle Skipping 2ch), PWM Dimmer (LEDC fade)
    ├── control/             # PID (derivative-on-measurement), 스케줄러, 적응형 폴링
    ├── safety/              # 과열/센서이상/stale/불일치/변화율 감시, 히터 타임아웃
    ├── comm/                # OpenThread Router/SED (lock 안전), CBOR 정수키 코덱
    ├── power/               # Deep Sleep (GPIO hold), 배터리 ADC (16x 멀티샘플링)
    └── config/              # NVS blob 설정, 종별 프리셋 JSON 관리
server/
├── mosquitto/               # MQTT 브로커 설정 (인증, ACL)
├── grafana/                 # 대시보드 JSON, 프로비저닝
├── bridge/                  # MQTT→InfluxDB Python 브릿지 (CBOR 디코딩)
├── systemd/                 # rbms-bridge 서비스 파일
└── scripts/setup.sh         # Raspberry Pi 자동 설치 (7단계)
```

## 아키텍처 (4계층)

1. **Application Layer** — app_type_a.c / app_type_b.c
2. **Service Layer** — PID, Scheduler, Adaptive Poll, Safety Monitor
3. **Driver Layer** — SHT30, DS18B20, SSR, PWM, Power Mgmt, Battery
4. **Platform Layer** — Thread Stack, CBOR Codec, NVS Config

## 코드 컨벤션

- **언어**: C (ESP-IDF 스타일)
- **네이밍**: snake_case (함수, 변수), UPPER_CASE (매크로, 상수)
- **로깅**: `ESP_LOGI`, `ESP_LOGW`, `ESP_LOGE` 사용 (컴포넌트별 TAG)
- **에러 처리**: `esp_err_t` 반환, `ESP_ERROR_CHECK()` 또는 조건 검사
- **헤더 가드**: `#ifndef COMPONENT_H`, `#define COMPONENT_H`, `#endif`
- **주석 언어**: 한국어 (코드 내 주석), 영어 (API 문서)
- **들여쓰기**: 4 spaces (탭 없음)

## ESP-IDF 컴포넌트 의존성 (CMakeLists.txt)

```
sensor     → driver esp_timer
actuator   → driver
control    → log
safety     → log esp_timer
comm       → log openthread esp_netif esp_event vfs
power      → log esp_adc esp_hw_support driver
config     → log nvs_flash
main       → sensor actuator control safety comm power config nvs_flash
```

> **주의**: ESP-IDF v5.2에서 `esp_log` 컴포넌트 이름은 `log`
> `esp_openthread`는 별도 컴포넌트가 아님, `openthread`만 사용

## 안전 시스템 (Safety Monitor)

7단계 검사 파이프라인:
1. 센서 유효범위 검사 (DS18B20: -20~85°C)
2. Stale 센서 감지 (마지막 유효 읽기 타임아웃)
3. 듀얼 센서 불일치 (핫존 vs 쿨존 차이)
4. 온도 변화율 감시 (물리적 불가능한 변화)
5. 과열 검사 (setpoint + offset)
6. 히터 연속 동작 제한
7. 고온 접근 경고

## 현재 상태

- **펌웨어**: 31/31 파일 구현 완료 (100%)
- **Type A 빌드**: ✅ 성공 (179KB, 83% 파티션 여유)
- **Type B 빌드**: ✅ 성공
- **서버**: 9개 구성 파일 완료 (Mosquitto/Grafana/InfluxDB/Bridge)
- **프리셋**: JSON 4종 완료
- **남은 작업**: 하드웨어 PCB 설계, PDF 문서 생성, CI/CD 개선

## 주의사항

- GPIO 핀 매핑은 Kconfig에 정의됨 — 하드코딩 금지
- PID 파라미터는 Kconfig에서 x100 정수로 관리 (2.0 → 200)
- Type B의 DS18B20은 GPIO 전원 스위칭 사용 (배터리 절약)
- 안전 기능은 최우선 — 과열 차단은 모든 제어보다 우선
- OpenThread API는 mainloop 시작 후 lock 필수 (`esp_openthread_lock_acquire/release`)
- ESP32-C6 ADC 캘리브레이션은 curve_fitting 사용 (line_fitting 미지원)
