#!/bin/bash
# RBMS Server Setup Script
# Raspberry Pi (Debian/Ubuntu)에서 실행
# Mosquitto MQTT + InfluxDB + Grafana + Bridge 자동 설치

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
SERVER_DIR="$(dirname "$SCRIPT_DIR")"
RBMS_USER="rbms"
INSTALL_DIR="/opt/rbms"

echo "=== RBMS Server Setup ==="
echo "Server dir: $SERVER_DIR"
echo ""

# --- 사전 검사 ---
if [ "$(id -u)" -ne 0 ]; then
    echo "ERROR: root 권한이 필요합니다 (sudo ./setup.sh)"
    exit 1
fi

# --- 1. 시스템 패키지 업데이트 ---
echo "[1/7] 시스템 패키지 업데이트..."
apt-get update -qq
apt-get install -y -qq python3 python3-venv python3-pip curl apt-transport-https

# --- 2. Mosquitto MQTT 설치 ---
echo "[2/7] Mosquitto MQTT 설치..."
apt-get install -y -qq mosquitto mosquitto-clients

cp "$SERVER_DIR/mosquitto/mosquitto.conf" /etc/mosquitto/mosquitto.conf
cp "$SERVER_DIR/mosquitto/acl.conf" /etc/mosquitto/acl.conf

# 브릿지 사용자 비밀번호 설정
mosquitto_passwd -b -c /etc/mosquitto/passwd rbms_bridge rbms_bridge_pass
mosquitto_passwd -b /etc/mosquitto/passwd rbms_dashboard rbms_dashboard_pass

systemctl enable mosquitto
systemctl restart mosquitto
echo "  Mosquitto 설치 완료"

# --- 3. InfluxDB 설치 (1.8.x) ---
echo "[3/7] InfluxDB 설치..."
if ! command -v influx &> /dev/null; then
    curl -sL https://repos.influxdata.com/influxdata-archive_compat.key | gpg --dearmor > /etc/apt/trusted.gpg.d/influxdata.gpg
    echo "deb https://repos.influxdata.com/debian stable main" > /etc/apt/sources.list.d/influxdb.list
    apt-get update -qq
    apt-get install -y -qq influxdb
fi

systemctl enable influxdb
systemctl start influxdb

# DB 및 사용자 생성 (이미 있으면 무시)
sleep 2
influx -execute "CREATE DATABASE rbms" 2>/dev/null || true
influx -execute "CREATE RETENTION POLICY rbms_90d ON rbms DURATION 90d REPLICATION 1 DEFAULT" 2>/dev/null || true

# InfluxDB 인증 사용자 생성
INFLUX_ADMIN_PASS=$(openssl rand -base64 16)
INFLUX_WRITER_PASS=$(openssl rand -base64 16)
INFLUX_READER_PASS=$(openssl rand -base64 16)
influx -execute "CREATE USER admin WITH PASSWORD '$INFLUX_ADMIN_PASS' WITH ALL PRIVILEGES" 2>/dev/null || true
influx -execute "CREATE USER rbms_writer WITH PASSWORD '$INFLUX_WRITER_PASS'" 2>/dev/null || true
influx -execute "GRANT WRITE ON rbms TO rbms_writer" 2>/dev/null || true
influx -execute "CREATE USER grafana_reader WITH PASSWORD '$INFLUX_READER_PASS'" 2>/dev/null || true
influx -execute "GRANT READ ON rbms TO grafana_reader" 2>/dev/null || true

# 인증 정보를 환경파일에 저장
cat > "$INSTALL_DIR/.env" <<ENVEOF
INFLUX_ADMIN_PASS=$INFLUX_ADMIN_PASS
INFLUX_USER=rbms_writer
INFLUX_PASS=$INFLUX_WRITER_PASS
INFLUX_READER_USER=grafana_reader
INFLUX_READER_PASS=$INFLUX_READER_PASS
ENVEOF
chmod 600 "$INSTALL_DIR/.env"
echo "  InfluxDB 설치 완료 (DB: rbms, 인증 정보: $INSTALL_DIR/.env)"

# --- 4. Grafana 설치 ---
echo "[4/7] Grafana 설치..."
if ! command -v grafana-server &> /dev/null; then
    curl -sL https://apt.grafana.com/gpg.key | gpg --dearmor > /etc/apt/trusted.gpg.d/grafana.gpg
    echo "deb https://apt.grafana.com stable main" > /etc/apt/sources.list.d/grafana.list
    apt-get update -qq
    apt-get install -y -qq grafana
fi

# 프로비저닝 파일 복사
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

# --- 5. RBMS 사용자 및 Bridge 설치 ---
echo "[5/7] Bridge 서비스 설치..."
id -u "$RBMS_USER" &>/dev/null || useradd --system --no-create-home "$RBMS_USER"

mkdir -p "$INSTALL_DIR/bridge"
cp "$SERVER_DIR/bridge/mqtt_influx_bridge.py" "$INSTALL_DIR/bridge/"
cp "$SERVER_DIR/bridge/requirements.txt" "$INSTALL_DIR/bridge/"

# Python 가상환경
python3 -m venv "$INSTALL_DIR/venv"
"$INSTALL_DIR/venv/bin/pip" install -q -r "$INSTALL_DIR/bridge/requirements.txt"

chown -R "$RBMS_USER":"$RBMS_USER" "$INSTALL_DIR"
echo "  Bridge 설치 완료"

# --- 6. systemd 서비스 등록 ---
echo "[6/7] systemd 서비스 등록..."
cp "$SERVER_DIR/systemd/rbms-bridge.service" /etc/systemd/system/
systemctl daemon-reload
systemctl enable rbms-bridge
systemctl start rbms-bridge
echo "  rbms-bridge 서비스 시작됨"

# --- 7. 방화벽 설정 ---
echo "[7/8] 방화벽 설정..."
apt-get install -y -qq ufw
ufw default deny incoming
ufw default allow outgoing
ufw allow ssh
ufw allow 3000/tcp comment "Grafana"
ufw allow 1883/tcp comment "MQTT"
ufw --force enable
echo "  UFW 방화벽 활성화 (SSH, Grafana, MQTT 허용)"

# --- 8. 검증 ---
echo "[8/8] 설치 검증..."
echo ""
echo "서비스 상태:"
for svc in mosquitto influxdb grafana-server rbms-bridge; do
    status=$(systemctl is-active "$svc" 2>/dev/null || echo "inactive")
    printf "  %-20s %s\n" "$svc" "$status"
done

echo ""
echo "=== RBMS Server Setup 완료 ==="
echo ""
echo "접속 정보:"
echo "  Grafana:    http://$(hostname -I | awk '{print $1}'):3000  (admin/admin)"
echo "  InfluxDB:   http://localhost:8086"
echo "  MQTT:       localhost:1883"
echo ""
echo "다음 단계:"
echo "  1. Grafana 초기 비밀번호 변경"
echo "  2. Mosquitto 비밀번호 변경 (mosquitto_passwd)"
echo "  3. Thread Border Router 설정"
