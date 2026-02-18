#!/bin/bash
# ==============================================================================
# RBMS OpenThread Border Router (OTBR) Setup Script
# Debian/Ubuntu Linux 서버에서 실행
#
# 사전 요구:
#   - Debian/Ubuntu Linux (RPi, 일반 PC, 미니 PC 등)
#   - RCP (Radio Co-Processor) 디바이스 USB 연결
#     - ESP32-C6 DevKit에 RCP 펌웨어 플래시 (아래 RCP 섹션 참조)
#     - 또는 nRF52840 Dongle 등 802.15.4 라디오
#
# 사용법:
#   sudo ./setup_otbr.sh [RCP_DEVICE]
#   예: sudo ./setup_otbr.sh /dev/ttyACM0
#       sudo ./setup_otbr.sh /dev/ttyUSB0
# ==============================================================================

set -euo pipefail

RCP_DEVICE="${1:-/dev/ttyACM0}"
OTBR_DIR="/opt/otbr"
INFRA_IFACE="${INFRA_IFACE:-eth0}"

echo "========================================"
echo " RBMS OpenThread Border Router Setup"
echo "========================================"
echo ""
echo " RCP device:     $RCP_DEVICE"
echo " Infra interface: $INFRA_IFACE"
echo " Install dir:     $OTBR_DIR"
echo ""

# --- Root check ---
if [ "$(id -u)" -ne 0 ]; then
    echo "ERROR: root 권한 필요 (sudo ./setup_otbr.sh)"
    exit 1
fi

# --- RCP device check ---
if [ ! -e "$RCP_DEVICE" ]; then
    echo "WARNING: $RCP_DEVICE not found"
    echo ""
    echo "RCP 디바이스를 USB로 연결했는지 확인하세요."
    echo "ESP32-C6 RCP 펌웨어 플래시 방법:"
    echo ""
    echo "  # ESP-IDF 환경에서 (개발 PC)"
    echo "  cd \$IDF_PATH/examples/openthread/ot_rcp"
    echo "  idf.py set-target esp32c6"
    echo "  idf.py build"
    echo "  idf.py -p /dev/ttyUSB0 flash"
    echo ""
    echo "RCP를 연결 후 다시 실행하세요."
    echo "디바이스 확인: ls -la /dev/ttyACM* /dev/ttyUSB*"
    exit 1
fi

# --- 1. Dependencies ---
echo "[1/5] 시스템 패키지 설치..."
apt-get update -qq
apt-get install -y -qq \
    git build-essential cmake ninja-build \
    libreadline-dev libncurses-dev \
    libdbus-1-dev libavahi-client-dev libavahi-common-dev \
    libjsoncpp-dev libprotobuf-dev protobuf-compiler \
    libboost-dev libboost-filesystem-dev libboost-system-dev \
    libnetfilter-queue-dev \
    radvd iproute2 iputils-ping \
    avahi-daemon

# --- 2. Clone ot-br-posix ---
echo "[2/5] ot-br-posix 다운로드..."
if [ -d "$OTBR_DIR" ]; then
    echo "  $OTBR_DIR 이미 존재, 업데이트..."
    cd "$OTBR_DIR"
    git pull --ff-only || true
else
    git clone https://github.com/openthread/ot-br-posix.git "$OTBR_DIR"
    cd "$OTBR_DIR"
fi
git submodule update --init --recursive

# --- 3. Build ---
echo "[3/5] ot-br-posix 빌드..."
OTBR_OPTIONS="-DOTBR_DBUS=ON \
  -DOTBR_WEB=OFF \
  -DOTBR_REST=ON \
  -DOTBR_BACKBONE_ROUTER=ON \
  -DOTBR_SRP_ADVERTISING_PROXY=ON \
  -DOTBR_MDNS=avahi \
  -DCMAKE_INSTALL_PREFIX=/usr/local"

./script/cmake-build \
    -DCMAKE_BUILD_TYPE=Release \
    $OTBR_OPTIONS

# --- 4. Install ---
echo "[4/5] 설치 및 설정..."

# Install binaries
cd build && cmake --install . && cd ..

# Create otbr-agent config
mkdir -p /etc/default
cat > /etc/default/otbr-agent <<CONF
# OTBR Agent Configuration
# RCP device path
OTBR_AGENT_OPTS="-I wpan0 -B $INFRA_IFACE spinel+hdlc+uart://$RCP_DEVICE?uart-baudrate=460800"
CONF

# systemd service
cat > /etc/systemd/system/otbr-agent.service <<SERVICE
[Unit]
Description=OpenThread Border Router Agent
After=network.target

[Service]
Type=simple
EnvironmentFile=/etc/default/otbr-agent
ExecStart=/usr/local/sbin/otbr-agent \$OTBR_AGENT_OPTS
Restart=always
RestartSec=5

[Install]
WantedBy=multi-user.target
SERVICE

# Firewall rules for Thread
cat > /etc/sysctl.d/60-otbr.conf <<SYSCTL
# Enable IPv6 forwarding for Thread Border Router
net.ipv6.conf.all.forwarding=1
net.ipv6.conf.all.accept_ra=2
SYSCTL
sysctl -p /etc/sysctl.d/60-otbr.conf

# --- 5. Start ---
echo "[5/5] 서비스 시작..."
systemctl daemon-reload
systemctl enable otbr-agent
systemctl start otbr-agent

sleep 3
STATUS=$(systemctl is-active otbr-agent 2>/dev/null || echo "failed")

echo ""
echo "========================================"
echo " OTBR Setup Complete"
echo "========================================"
echo ""
echo " Service status: $STATUS"
echo " RCP device:     $RCP_DEVICE"
echo " Thread iface:   wpan0"
echo " Infra iface:    $INFRA_IFACE"
echo ""
echo " 유용한 명령어:"
echo "   # 서비스 상태"
echo "   sudo systemctl status otbr-agent"
echo ""
echo "   # Thread 네트워크 형성"
echo "   sudo ot-ctl dataset init new"
echo "   sudo ot-ctl dataset commit active"
echo "   sudo ot-ctl ifconfig up"
echo "   sudo ot-ctl thread start"
echo ""
echo "   # Thread 상태 확인"
echo "   sudo ot-ctl state"
echo "   sudo ot-ctl neighbor table"
echo "   sudo ot-ctl ipaddr"
echo ""
echo "   # Dataset 확인 (노드 연결에 필요)"
echo "   sudo ot-ctl dataset active -x"
echo ""
echo " 다음 단계:"
echo "   1. Thread 네트워크 형성 (위 명령어)"
echo "   2. Active Dataset을 ESP32-C6 노드에 설정"
echo "   3. server/scripts/setup.sh 실행 (MQTT + InfluxDB + Grafana)"
echo ""
