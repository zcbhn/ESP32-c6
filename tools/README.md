# RBMS 자동화 도구

ESP32-C6 펌웨어의 빌드, 플래시, 모니터링, 검증을 자동화하는 도구 모음입니다.
Claude Code에서 직접 실행하고 결과를 확인할 수 있도록 설계되었습니다.

## 사전 준비

### 1. ESP-IDF 설치

```powershell
# PowerShell에서 실행 (관리자 권한 권장)
Set-ExecutionPolicy -Scope CurrentUser RemoteSigned
.\tools\setup_esp_idf.ps1
```

설치 후 새 터미널에서 환경 로드:
```powershell
& C:\esp\esp-idf\export.ps1
```

### 2. Python 패키지

```bash
pip install pyserial
```

### 3. ESP32-C6 USB 드라이버

ESP32-C6 DevKitC는 내장 USB-JTAG 인터페이스를 사용합니다.
장치 관리자에서 `USB JTAG/serial debug unit`이 정상(OK) 상태인지 확인하세요.

드라이버가 `Unknown` 상태인 경우:
1. 장치 관리자 → 해당 장치 우클릭 → 드라이버 업데이트
2. `컴퓨터에서 드라이버 찾아보기` 선택
3. ESP-IDF 도구 디렉토리 지정

---

## 도구 목록

### `auto.ps1` — 풀 자동화 파이프라인

빌드 → 플래시 → 모니터 → 검증을 한 번에 실행합니다.

```powershell
# 기본 실행 (Type B, COM3, 30초 모니터링)
.\tools\auto.ps1

# Type A 풀 노드 빌드+플래시+60초 모니터링
.\tools\auto.ps1 -NodeType A -MonitorDuration 60

# 모니터+검증만 (빌드/플래시 건너뛰기)
.\tools\auto.ps1 -SkipBuild -SkipFlash

# 클린 빌드 + 부팅 검증만
.\tools\auto.ps1 -Clean -Check boot
```

### `build.ps1` — 빌드

```powershell
.\tools\build.ps1 -NodeType B          # Type B 빌드
.\tools\build.ps1 -NodeType A -Clean   # Type A 클린 빌드
```

### `flash.ps1` — 플래시

```powershell
.\tools\flash.ps1                       # COM3 기본
.\tools\flash.ps1 -Port COM5 -Baud 921600
```

### `monitor.py` — 시리얼 모니터 (로그 캡처)

```bash
# 30초 캡처 (기본)
python tools/monitor.py

# 60초, 특정 문자열 발견 시 종료
python tools/monitor.py --duration 60 --until "RBMS Firmware"

# 무한 캡처 (Ctrl+C로 중단)
python tools/monitor.py --duration 0

# ESP32 리셋 후 캡처
python tools/monitor.py --reset --duration 30
```

### `validate.py` — 로그 검증

```bash
# 최근 로그 자동 검증
python tools/validate.py

# 특정 로그 파일 검증
python tools/validate.py --log tools/logs/monitor_20260218_143000.log

# 부팅만 검증
python tools/validate.py --check boot

# JSON 출력 + 파일 저장
python tools/validate.py --json --save
```

검증 항목:

| 항목 | 설명 |
|------|------|
| `boot` | 펌웨어 시작, NVS 초기화, 노드 타입 식별, 크래시/패닉 없음 |
| `sensor` | SHT30/DS18B20 초기화, 온습도 읽기, 센서 에러 없음 |
| `thread` | OpenThread 초기화, 역할 설정, 네트워크 연결 |
| `control` | PID, SSR, 스케줄러, 안전 감시 (Type A) |
| `power` | Deep Sleep, 배터리, 적응형 폴링 (Type B) |
| `all` | 위 전체 |

### `check_ports.ps1` — USB 포트 확인

```powershell
powershell -ExecutionPolicy Bypass -File tools\check_ports.ps1
```

---

## 로그 파일

모든 로그는 `tools/logs/` 디렉토리에 저장됩니다:

```
tools/logs/
├── build_B_20260218_143000.log     # 빌드 로그
├── flash_20260218_143100.log       # 플래시 로그
├── monitor_20260218_143200.log     # 시리얼 모니터 캡처
├── validate_20260218_143300.json   # 검증 결과 (JSON)
├── pipeline_20260218_143000.log    # 파이프라인 전체 로그
└── latest_monitor.log              # 최근 모니터 로그 (복사본)
```

Claude는 `tools/logs/latest_monitor.log`를 읽어서 ESP32 출력을 확인할 수 있습니다.

---

## Claude Code에서 사용하기

Claude가 직접 실행할 수 있는 명령 예시:

```bash
# 1. 빌드 (PowerShell 경유)
powershell.exe -ExecutionPolicy Bypass -File tools\build.ps1 -NodeType B

# 2. 플래시
powershell.exe -ExecutionPolicy Bypass -File tools\flash.ps1 -Port COM3

# 3. 모니터 캡처 (Python)
python tools/monitor.py --port COM3 --duration 30 --reset

# 4. 로그 확인 (Claude가 직접 읽기)
# → Read 도구로 tools/logs/latest_monitor.log 읽기

# 5. 검증
python tools/validate.py --check boot --json

# 6. 풀 파이프라인
powershell.exe -ExecutionPolicy Bypass -File tools\auto.ps1
```

---

## .gitignore 추가 필요

```
tools/logs/
```
