#!/bin/bash
# RBMS Server Setup Script
# Debian/Ubuntu Linux 서버에서 실행 (RPi, 미니PC, 일반PC 등)
# Mosquitto MQTT + InfluxDB + Grafana + Gateway + Bridge 자동 설치

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
SERVER_DIR="$(dirname "$SCRIPT_DIR")"
RBMS_USER="rbms"
INSTALL_DIR="/opt/rbms"

echo "==========================================="
echo " RBMS Server Setup"
echo " Mosquitto + InfluxDB + Grafana + Gateway"
echo "==========================================="
echo ""
echo " Server dir: $SERVER_DIR"
echo " Install dir: $INSTALL_DIR"
echo ""

# --- Root check ---
if [ "$(id -u)" -ne 0 ]; then
    echo "ERROR: root 권한이 필요합니다 (sudo ./setup.sh)"
    exit 1
fi

# --- 1. System packages ---
echo "[1/9] 시스템 패키지 업데이트..."
apt-get update -qq
apt-get install -y -qq python3 python3-venv python3-pip curl apt-transport-https

# --- 2. Mosquitto MQTT ---
echo "[2/9] Mosquitto MQTT 설치..."
apt-get install -y -qq mosquitto mosquitto-clients

cp "$SERVER_DIR/mosquitto/mosquitto.conf" /etc/mosquitto/mosquitto.conf
cp "$SERVER_DIR/mosquitto/acl.conf" /etc/mosquitto/acl.conf

# MQTT users (bridge + gateway share same credentials)
mosquitto_passwd -b -c /etc/mosquitto/passwd rbms_bridge rbms_bridge_pass
mosquitto_passwd -b /etc/mosquitto/passwd rbms_dashboard rbms_dashboard_pass

systemctl enable mosquitto
systemctl restart mosquitto
echo "  Mosquitto 설치 완료"

# --- 3. InfluxDB (1.8.x) ---
echo "[3/9] InfluxDB 설치..."
if ! command -v influx &> /dev/null; then
    curl -sL https://repos.influxdata.com/influxdata-archive_compat.key | gpg --dearmor > /etc/apt/trusted.gpg.d/influxdata.gpg
    echo "deb https://repos.influxdata.com/debian stable main" > /etc/apt/sources.list.d/influxdb.list
    apt-get update -qq
    apt-get install -y -qq influxdb
fi

systemctl enable influxdb
systemctl start influxdb

sleep 2
influx -execute "CREATE DATABASE rbms" 2>/dev/null || true
influx -execute "CREATE RETENTION POLICY rbms_90d ON rbms DURATION 90d REPLICATION 1 DEFAULT" 2>/dev/null || true

# InfluxDB auth users
INFLUX_ADMIN_PASS=$(openssl rand -base64 16)
INFLUX_WRITER_PASS=$(openssl rand -base64 16)
INFLUX_READER_PASS=$(openssl rand -base64 16)
influx -execute "CREATE USER admin WITH PASSWORD '$INFLUX_ADMIN_PASS' WITH ALL PRIVILEGES" 2>/dev/null || true
influx -execute "CREATE USER rbms_writer WITH PASSWORD '$INFLUX_WRITER_PASS'" 2>/dev/null || true
influx -execute "GRANT WRITE ON rbms TO rbms_writer" 2>/dev/null || true
influx -execute "CREATE USER grafana_reader WITH PASSWORD '$INFLUX_READER_PASS'" 2>/dev/null || true
influx -execute "GRANT READ ON rbms TO grafana_reader" 2>/dev/null || true

echo "  InfluxDB 설치 완료"

# --- 4. Grafana ---
echo "[4/9] Grafana 설치..."
if ! command -v grafana-server &> /dev/null; then
    curl -sL https://apt.grafana.com/gpg.key | gpg --dearmor > /etc/apt/trusted.gpg.d/grafana.gpg
    echo "deb https://apt.grafana.com stable main" > /etc/apt/sources.list.d/grafana.list
    apt-get update -qq
    apt-get install -y -qq grafana
fi

mkdir -p /etc/grafana/provisioning/datasources
mkdir -p /etc/grafana/provisioning/dashboards
mkdir -p /var/lib/grafana/dashboards/rbms

cp "$SERVER_DIR/grafana/provisioning/datasources/influxdb.yml" /etc/grafana/provisioning/datasources/
cp "$SERVER_DIR/grafana/provisioning/dashboards/dashboard.yml" /etc/grafana/provisioning/dashboards/
cp "$SERVER_DIR/grafana/dashboards/rbms_overview.json" /var/lib/grafana/dashboards/rbms/

chown -R grafana:grafana /var/lib/grafana/dashboards

systemctl enable grafana-server
systemctl restart grafana-server
echo "  Grafana 설치 완료 (http://localhost:3000, admin/admin)"

# --- 5. RBMS user + install dir ---
echo "[5/9] RBMS 사용자 및 디렉토리 생성..."
id -u "$RBMS_USER" &>/dev/null || useradd --system --no-create-home "$RBMS_USER"
mkdir -p "$INSTALL_DIR/bridge" "$INSTALL_DIR/gateway"

# Save credentials
cat > "$INSTALL_DIR/.env" <<ENVEOF
# RBMS Server Credentials (auto-generated)
INFLUX_ADMIN_PASS=$INFLUX_ADMIN_PASS
INFLUX_USER=rbms_writer
INFLUX_PASS=$INFLUX_WRITER_PASS
INFLUX_READER_USER=grafana_reader
INFLUX_READER_PASS=$INFLUX_READER_PASS
MQTT_USER=rbms_bridge
MQTT_PASS=rbms_bridge_pass
ENVEOF
chmod 600 "$INSTALL_DIR/.env"
echo "  인증 정보 저장: $INSTALL_DIR/.env"

# --- 6. Gateway (Thread UDP -> MQTT) ---
echo "[6/9] Thread UDP->MQTT 게이트웨이 설치..."
cp "$SERVER_DIR/gateway/thread_mqtt_gateway.py" "$INSTALL_DIR/gateway/"
cp "$SERVER_DIR/gateway/requirements.txt" "$INSTALL_DIR/gateway/"

# --- 7. Bridge (MQTT -> InfluxDB) ---
echo "[7/9] MQTT->InfluxDB 브릿지 설치..."
cp "$SERVER_DIR/bridge/mqtt_influx_bridge.py" "$INSTALL_DIR/bridge/"
cp "$SERVER_DIR/bridge/requirements.txt" "$INSTALL_DIR/bridge/"

# Shared Python venv for both gateway and bridge
python3 -m venv "$INSTALL_DIR/venv"
"$INSTALL_DIR/venv/bin/pip" install -q \
    -r "$INSTALL_DIR/gateway/requirements.txt" \
    -r "$INSTALL_DIR/bridge/requirements.txt"

chown -R "$RBMS_USER":"$RBMS_USER" "$INSTALL_DIR"
echo "  Gateway + Bridge 설치 완료"

# --- 8. systemd services ---
echo "[8/9] systemd 서비스 등록..."
cp "$SERVER_DIR/systemd/rbms-gateway.service" /etc/systemd/system/
cp "$SERVER_DIR/systemd/rbms-bridge.service" /etc/systemd/system/
systemctl daemon-reload

systemctl enable rbms-gateway rbms-bridge
systemctl start rbms-gateway rbms-bridge
echo "  rbms-gateway, rbms-bridge 서비스 시작됨"

# --- 9. Firewall + Verify ---
echo "[9/9] 방화벽 설정 및 검증..."
apt-get install -y -qq ufw
ufw default deny incoming
ufw default allow outgoing
ufw allow ssh
ufw allow 3000/tcp comment "Grafana"
ufw allow 1883/tcp comment "MQTT"
ufw --force enable

echo ""
echo "서비스 상태:"
for svc in mosquitto influxdb grafana-server rbms-gateway rbms-bridge; do
    status=$(systemctl is-active "$svc" 2>/dev/null || echo "inactive")
    printf "  %-20s %s\n" "$svc" "$status"
done

echo ""
echo "==========================================="
echo " RBMS Server Setup 완료"
echo "==========================================="
echo ""
echo " 접속 정보:"
echo "   Grafana:    http://$(hostname -I | awk '{print $1}'):3000  (admin/admin)"
echo "   InfluxDB:   http://localhost:8086"
echo "   MQTT:       localhost:1883"
echo "   인증 정보:   $INSTALL_DIR/.env"
echo ""
echo " 데이터 흐름:"
echo "   Thread UDP :5684 -> rbms-gateway -> MQTT -> rbms-bridge -> InfluxDB -> Grafana"
echo ""
echo " 다음 단계:"
echo "   1. Grafana 초기 비밀번호 변경"
echo "   2. Mosquitto 비밀번호 변경 후 .env 업데이트"
echo "   3. Border Router 미설치 시: server/border-router/setup_otbr.sh 실행"
echo ""
