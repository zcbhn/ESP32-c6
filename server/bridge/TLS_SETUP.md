# InfluxDB TLS Setup

## Overview

MQTT-InfluxDB Bridge와 InfluxDB 간 통신을 TLS로 암호화합니다.
LAN 외부에서 접근하거나 보안이 중요한 환경에서 설정합니다.

## 1. Self-Signed Certificate 생성

```bash
# 인증서 디렉토리 생성
sudo mkdir -p /opt/rbms/certs
cd /opt/rbms/certs

# CA 키 및 인증서 생성 (10년)
openssl genrsa -out ca-key.pem 4096
openssl req -new -x509 -key ca-key.pem -out ca.pem -days 3650 \
    -subj "/CN=RBMS CA"

# InfluxDB 서버 키 및 인증서
openssl genrsa -out server-key.pem 2048
openssl req -new -key server-key.pem -out server.csr \
    -subj "/CN=localhost"
openssl x509 -req -in server.csr -CA ca.pem -CAkey ca-key.pem \
    -CAcreateserial -out server.pem -days 3650

# 권한 설정
sudo chown influxdb:influxdb server-key.pem server.pem
sudo chmod 600 server-key.pem
sudo chmod 644 ca.pem server.pem
```

## 2. InfluxDB TLS 설정

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

## 3. Bridge 환경변수 설정

`/opt/rbms/.env` 에 추가:

```bash
INFLUX_SSL=true
INFLUX_SSL_CERT=/opt/rbms/certs/ca.pem
INFLUX_PORT=8086
```

```bash
sudo systemctl restart rbms-bridge
```

## 4. Grafana Datasource 업데이트

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

## 5. 검증

```bash
# TLS 연결 테스트
curl -k https://localhost:8086/ping

# CA 인증서로 검증
curl --cacert /opt/rbms/certs/ca.pem https://localhost:8086/ping

# Bridge 로그 확인
sudo journalctl -u rbms-bridge -f
```
