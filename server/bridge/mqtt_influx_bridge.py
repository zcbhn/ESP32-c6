#!/usr/bin/env python3
"""
RBMS MQTT -> InfluxDB Bridge

Mosquitto에서 CBOR 텔레메트리 수신 -> InfluxDB에 기록.
Linux 서버에서 systemd 서비스로 실행.

TLS 지원:
  InfluxDB TLS: INFLUX_SSL=true, INFLUX_SSL_CERT=/path/to/ca.pem
  MQTT TLS:     MQTT_TLS=true, MQTT_CA_CERT=/path/to/ca.pem

토픽: rbms/<node_id>/telemetry
페이로드: CBOR map {1:temp_hot, 2:temp_cool, 3:humidity, 4:battery_v,
                     5:heater_duty, 6:light_duty, 7:safety_status}
"""

import logging
import os
import signal
import ssl
import sys
import time

import cbor2
import paho.mqtt.client as mqtt
from influxdb import InfluxDBClient

# --- 설정 (환경변수) ---
MQTT_HOST = os.environ.get("MQTT_HOST", "localhost")
MQTT_PORT = int(os.environ.get("MQTT_PORT", "1883"))
MQTT_USER = os.environ.get("MQTT_USER", "rbms_bridge")
MQTT_PASS = os.environ.get("MQTT_PASS")
MQTT_TLS = os.environ.get("MQTT_TLS", "false").lower() in ("true", "1", "yes")
MQTT_CA_CERT = os.environ.get("MQTT_CA_CERT", "")
MQTT_TOPIC = "rbms/+/telemetry"

INFLUX_HOST = os.environ.get("INFLUX_HOST", "localhost")
INFLUX_PORT = int(os.environ.get("INFLUX_PORT", "8086"))
INFLUX_DB = os.environ.get("INFLUX_DB", "rbms")
INFLUX_USER = os.environ.get("INFLUX_USER", "")
INFLUX_PASS = os.environ.get("INFLUX_PASS", "")
INFLUX_SSL = os.environ.get("INFLUX_SSL", "false").lower() in ("true", "1", "yes")
INFLUX_SSL_CERT = os.environ.get("INFLUX_SSL_CERT", "")  # CA cert path

# CBOR 정수 키 -> 필드 이름 매핑
FIELD_MAP = {
    1: "temp_hot",
    2: "temp_cool",
    3: "humidity",
    4: "battery_v",
    5: "heater_duty",
    6: "light_duty",
    7: "safety",
}

# 버퍼 설정
BUFFER_FLUSH_SIZE = 10
BUFFER_FLUSH_SEC = 5.0
BUFFER_MAX_SIZE = 1000  # 무한 증가 방지

logging.basicConfig(
    level=os.environ.get("LOG_LEVEL", "INFO").upper(),
    format="%(asctime)s [%(levelname)s] %(message)s",
)
log = logging.getLogger("bridge")

influx_client = None
write_buffer = []
last_flush_time = time.time()
retry_count = 0
MAX_RETRY = 3


def flush_buffer():
    global write_buffer, last_flush_time, retry_count
    if not write_buffer:
        return
    try:
        influx_client.write_points(write_buffer)
        log.debug("Flushed %d points to InfluxDB", len(write_buffer))
        write_buffer = []
        retry_count = 0
    except Exception as e:
        retry_count += 1
        log.error("InfluxDB write failed (attempt %d/%d): %s",
                  retry_count, MAX_RETRY, e)
        if retry_count >= MAX_RETRY:
            dropped = len(write_buffer)
            write_buffer = []
            retry_count = 0
            log.error("Dropped %d points after %d failed attempts",
                      dropped, MAX_RETRY)
    last_flush_time = time.time()


def on_connect(client, userdata, flags, rc):
    if rc == 0:
        log.info("MQTT connected, subscribing to %s", MQTT_TOPIC)
        client.subscribe(MQTT_TOPIC, qos=1)
    else:
        log.error("MQTT connect failed: rc=%d", rc)


def on_disconnect(client, userdata, rc):
    if rc != 0:
        log.warning("MQTT unexpected disconnect: rc=%d, will reconnect", rc)


def on_message(client, userdata, msg):
    global write_buffer
    try:
        # 토픽에서 node_id 추출: rbms/<node_id>/telemetry
        parts = msg.topic.split("/")
        if len(parts) != 3:
            return
        node_id = parts[1]

        # CBOR 디코딩
        data = cbor2.loads(msg.payload)
        if not isinstance(data, dict):
            log.warning("Invalid CBOR payload from %s", node_id)
            return

        # 필드 변환
        fields = {}
        for key, value in data.items():
            field_name = FIELD_MAP.get(key)
            if field_name and isinstance(value, (int, float)):
                fields[field_name] = float(value)

        if not fields:
            return

        point = {
            "measurement": "telemetry",
            "tags": {"node_id": node_id},
            "fields": fields,
        }

        # 버퍼 최대 크기 초과 시 가장 오래된 데이터 삭제
        if len(write_buffer) >= BUFFER_MAX_SIZE:
            dropped = len(write_buffer) - BUFFER_MAX_SIZE // 2
            write_buffer = write_buffer[dropped:]
            log.warning("Buffer overflow: dropped %d oldest points", dropped)

        write_buffer.append(point)

        # 버퍼 플러시 조건
        if len(write_buffer) >= BUFFER_FLUSH_SIZE:
            flush_buffer()

    except Exception as e:
        log.error("Message processing error: %s", e)


def periodic_flush():
    global last_flush_time
    if time.time() - last_flush_time >= BUFFER_FLUSH_SEC:
        flush_buffer()


def main():
    global influx_client

    # MQTT 비밀번호 필수 확인
    if not MQTT_PASS:
        log.error("MQTT_PASS environment variable is required")
        sys.exit(1)

    # InfluxDB 연결
    influx_kwargs = dict(host=INFLUX_HOST, port=INFLUX_PORT, database=INFLUX_DB)
    if INFLUX_USER:
        influx_kwargs["username"] = INFLUX_USER
        influx_kwargs["password"] = INFLUX_PASS
    if INFLUX_SSL:
        influx_kwargs["ssl"] = True
        influx_kwargs["verify_ssl"] = True
        if INFLUX_SSL_CERT:
            influx_kwargs["ssl_ca_cert"] = INFLUX_SSL_CERT
        log.info("InfluxDB TLS enabled (cert: %s)", INFLUX_SSL_CERT or "system CA")
    influx_client = InfluxDBClient(**influx_kwargs)
    influx_client.create_database(INFLUX_DB)
    log.info("InfluxDB connected: %s:%d/%s", INFLUX_HOST, INFLUX_PORT, INFLUX_DB)

    # retention policy (90일)
    influx_client.create_retention_policy(
        "rbms_90d", "90d", 1, database=INFLUX_DB, default=True
    )

    # MQTT 연결
    mqttc = mqtt.Client(client_id="rbms-bridge")
    mqttc.username_pw_set(MQTT_USER, MQTT_PASS)
    mqttc.on_connect = on_connect
    mqttc.on_disconnect = on_disconnect
    mqttc.on_message = on_message
    mqttc.reconnect_delay_set(min_delay=1, max_delay=30)

    # MQTT TLS
    if MQTT_TLS:
        tls_kwargs = {}
        if MQTT_CA_CERT:
            tls_kwargs["ca_certs"] = MQTT_CA_CERT
        mqttc.tls_set(**tls_kwargs)
        log.info("MQTT TLS enabled (cert: %s)", MQTT_CA_CERT or "system CA")

    mqttc.connect(MQTT_HOST, MQTT_PORT, keepalive=60)
    mqttc.loop_start()
    log.info("Bridge started")

    # 시그널 핸들러
    running = True

    def stop(sig, frame):
        nonlocal running
        running = False

    signal.signal(signal.SIGTERM, stop)
    signal.signal(signal.SIGINT, stop)

    try:
        while running:
            periodic_flush()
            time.sleep(1)
    finally:
        flush_buffer()
        mqttc.loop_stop()
        mqttc.disconnect()
        log.info("Bridge stopped")


if __name__ == "__main__":
    main()
