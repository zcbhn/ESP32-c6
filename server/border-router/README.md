# Thread Border Router Setup

## Overview

Thread Border Router는 Thread 메시 네트워크와 IP 네트워크를 연결합니다.
ESP32-C6 노드들의 데이터가 서버(MQTT/InfluxDB/Grafana)에 도달하려면 반드시 필요합니다.

```
ESP32-C6 노드 (Thread) ──802.15.4──> RCP (USB) ──> Linux 서버 (ot-br-posix)
                                                          │
                                                    wpan0 (Thread)
                                                          │
                                                    UDP port 5684
                                                          │
                                                    Gateway → MQTT → InfluxDB → Grafana
```

## 필요 하드웨어

| 항목 | 설명 |
|------|------|
| Linux 서버 | Debian/Ubuntu (RPi, 미니PC, 일반PC 등) |
| RCP 디바이스 | ESP32-C6 DevKit 1개 (RCP 펌웨어 플래시) |
| USB 케이블 | RCP ↔ Linux 서버 연결 |

## RCP 펌웨어 플래시

ESP32-C6 DevKit 하나를 RCP(Radio Co-Processor)로 사용합니다.
개발 PC(ESP-IDF 설치된 환경)에서 플래시합니다.

```bash
# ESP-IDF 환경 활성화
. $IDF_PATH/export.sh

# RCP 예제 빌드
cd $IDF_PATH/examples/openthread/ot_rcp
idf.py set-target esp32c6
idf.py build

# 플래시 (포트는 환경에 맞게 수정)
idf.py -p /dev/ttyUSB0 flash
```

Windows에서:
```powershell
. C:\esp\esp-idf\export.ps1
cd $env:IDF_PATH\examples\openthread\ot_rcp
idf.py set-target esp32c6
idf.py build
idf.py -p COM3 flash
```

## Border Router 설치

RCP를 Linux 서버에 USB로 연결한 후:

```bash
# RCP 디바이스 확인
ls -la /dev/ttyACM* /dev/ttyUSB*

# 설치 실행
sudo ./setup_otbr.sh /dev/ttyACM0
```

환경 변수로 인프라 인터페이스 지정 가능:
```bash
# Wi-Fi 사용 시
sudo INFRA_IFACE=wlan0 ./setup_otbr.sh /dev/ttyACM0

# Ethernet 사용 시 (기본값)
sudo INFRA_IFACE=eth0 ./setup_otbr.sh /dev/ttyACM0
```

## Thread 네트워크 형성

설치 후 Thread 네트워크를 생성합니다:

```bash
# 새 네트워크 데이터셋 생성
sudo ot-ctl dataset init new
sudo ot-ctl dataset commit active
sudo ot-ctl ifconfig up
sudo ot-ctl thread start

# 상태 확인 (leader가 되어야 함)
sudo ot-ctl state

# Active Dataset 확인 (ESP32-C6 노드 연결에 필요)
sudo ot-ctl dataset active -x
```

## ESP32-C6 노드 연결

Border Router의 Active Dataset을 ESP32-C6 펌웨어에 설정해야 합니다.
NVS 또는 menuconfig를 통해 Dataset을 입력합니다.

## 확인 방법

```bash
# Border Router 서비스 상태
sudo systemctl status otbr-agent

# 연결된 Thread 노드 목록
sudo ot-ctl neighbor table

# Thread 네트워크 IPv6 주소
sudo ot-ctl ipaddr

# wpan0 인터페이스 확인
ip addr show wpan0
```
