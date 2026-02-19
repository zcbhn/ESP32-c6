# RBMS TLS Setup

## Overview

RBMS 서버 컴포넌트 간 통신을 TLS로 암호화합니다.
LAN 외부에서 접근하거나 보안이 중요한 환경에서 설정합니다.

- **MQTT TLS (8883)**: Gateway/Bridge ↔ Mosquitto 통신 암호화
- **InfluxDB TLS (8086)**: Bridge ↔ InfluxDB 통신 암호화

## 1. Self-Signed Certificate 생성

MQTT와 InfluxDB 모두 동일한 CA를 사용합니다.

```bash
# 인증서 디렉토리 생성
sudo mkdir -p /opt/rbms/certs
cd /opt/rbms/certs

# CA 키 및 인증서 생성 (10년)
openssl genrsa -out ca-key.pem 4096
openssl req -new -x509 -key ca-key.pem -out ca.pem -days 3650 \
    -subj "/CN=RBMS CA"

# MQTT 서버 인증서
openssl genrsa -out mqtt-server-key.pem 2048
openssl req -new -key mqtt-server-key.pem -out mqtt-server.csr \
    -subj "/CN=localhost"
openssl x509 -req -in mqtt-server.csr -CA ca.pem -CAkey ca-key.pem \
    -CAcreateserial -out mqtt-server.pem -days 3650

# InfluxDB 서버 인증서
openssl genrsa -out server-key.pem 2048
openssl req -new -key server-key.pem -out server.csr \
    -subj "/CN=localhost"
openssl x509 -req -in server.csr -CA ca.pem -CAkey ca-key.pem \
    -CAcreateserial -out server.pem -days 3650

# 권한 설정
sudo chown mosquitto:mosquitto mqtt-server-key.pem mqtt-server.pem
sudo chown influxdb:influxdb server-key.pem server.pem
sudo chmod 600 mqtt-server-key.pem server-key.pem
sudo chmod 644 ca.pem mqtt-server.pem server.pem

# CSR 정리
rm -f *.csr
```

## 2. MQTT TLS 설정 (8883)

### 2.1 Mosquitto 설정

`/etc/mosquitto/mosquitto.conf` 에서 TLS 리스너 주석 해제:

```conf
listener 8883 0.0.0.0
protocol mqtt
cafile /opt/rbms/certs/ca.pem
certfile /opt/rbms/certs/mqtt-server.pem
keyfile /opt/rbms/certs/mqtt-server-key.pem
tls_version tlsv1.2
```

```bash
sudo systemctl restart mosquitto
```

### 2.2 Gateway/Bridge 환경변수

`/opt/rbms/.env` 수정:

```bash
MQTT_PORT=8883
MQTT_TLS=true
MQTT_CA_CERT=/opt/rbms/certs/ca.pem
```

```bash
sudo systemctl restart rbms-gateway rbms-bridge
```

### 2.3 검증

```bash
# TLS 연결 테스트
mosquitto_pub --cafile /opt/rbms/certs/ca.pem \
    -h localhost -p 8883 \
    -u rbms_bridge -P "$(grep MQTT_PASS /opt/rbms/.env | cut -d= -f2)" \
    -t "test/tls" -m "hello"

# 로그 확인
sudo journalctl -u rbms-gateway -u rbms-bridge --since "5 min ago"
```

## 3. InfluxDB TLS 설정

### 3.1 InfluxDB 설정

`/etc/influxdb/influxdb.conf` 수정:

```toml
[http]
  https-enabled = true
  https-certificate = "/opt/rbms/certs/server.pem"
  https-private-key = "/opt/rbms/certs/server-key.pem"
```

```bash
sudo systemctl restart influxdb
```

### 3.2 Bridge 환경변수

`/opt/rbms/.env` 에 추가:

```bash
INFLUX_SSL=true
INFLUX_SSL_CERT=/opt/rbms/certs/ca.pem
```

```bash
sudo systemctl restart rbms-bridge
```

### 3.3 Grafana Datasource 업데이트

`/etc/grafana/provisioning/datasources/influxdb.yml` 수정:

```yaml
datasources:
  - name: RBMS-InfluxDB
    type: influxdb
    url: https://localhost:8086
    jsonData:
      tlsSkipVerify: false
      tlsAuthWithCACert: true
    secureJsonData:
      tlsCACert: |
        <ca.pem 내용 붙여넣기>
```

### 3.4 검증

```bash
# TLS 연결 테스트
curl -k https://localhost:8086/ping

# CA 인증서로 검증
curl --cacert /opt/rbms/certs/ca.pem https://localhost:8086/ping

# Bridge 로그 확인
sudo journalctl -u rbms-bridge -f
```

## 4. 전체 TLS 검증

모든 TLS 설정 완료 후:

```bash
# 서비스 재시작
sudo systemctl restart mosquitto influxdb rbms-gateway rbms-bridge

# 전체 서비스 상태 확인
for svc in mosquitto influxdb grafana-server rbms-gateway rbms-bridge; do
    printf "%-20s %s\n" "$svc" "$(systemctl is-active $svc)"
done

# Gateway -> MQTT TLS -> Bridge -> InfluxDB TLS 전체 파이프라인 확인
sudo journalctl -u rbms-gateway -u rbms-bridge --since "1 min ago" --no-pager
```
