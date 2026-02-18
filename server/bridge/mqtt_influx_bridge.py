#!/usr/bin/env python3
"""
RBMS MQTT → InfluxDB Bridge

Mosquitto에서 CBOR 텔레메트리 수신 → InfluxDB에 기록.
Raspberry Pi에서 systemd 서비스로 실행.

토픽: rbms/<node_id>/telemetry
페이로드: CBOR map {1:temp_hot, 2:temp_cool, 3:humidity, 4:battery_v,
                     5:heater_duty, 6:light_duty, 7:safety_status}
"""

import json
import logging
import os
import signal
import sys
import time

import cbor2
import paho.mqtt.client as mqtt
from influxdb import InfluxDBClient

# --- 설정 (환경변수 우선, 기본값 폴백) ---
MQTT_HOST = os.environ.get("MQTT_HOST", "localhost")
MQTT_PORT = int(os.environ.get("MQTT_PORT", "1883"))
MQTT_USER = os.environ.get("MQTT_USER", "rbms_bridge")
MQTT_PASS = os.environ.get("MQTT_PASS", "rbms_bridge_pass")
MQTT_TOPIC = "rbms/+/telemetry"

INFLUX_HOST = os.environ.get("INFLUX_HOST", "localhost")
INFLUX_PORT = int(os.environ.get("INFLUX_PORT", "8086"))
INFLUX_DB = os.environ.get("INFLUX_DB", "rbms")
INFLUX_USER = os.environ.get("INFLUX_USER", "")
INFLUX_PASS = os.environ.get("INFLUX_PASS", "")

# CBOR 정수 키 → 필드 이름 매핑
FIELD_MAP = {
    1: "temp_hot",
    2: "temp_cool",
    3: "humidity",
    4: "battery_v",
    5: "heater_duty",
    6: "light_duty",
    7: "safety",
}

logging.basicConfig(
    level=logging.INFO,
    format="%(asctime)s [%(levelname)s] %(message)s",
)
log = logging.getLogger("bridge")

influx_client = None
write_buffer = []
BUFFER_FLUSH_SIZE = 10
BUFFER_FLUSH_SEC = 5.0
last_flush_time = time.time()


def flush_buffer():
    global write_buffer, last_flush_time
    if not write_buffer:
        return
    try:
        influx_client.write_points(write_buffer)
        log.debug("Flushed %d points to InfluxDB", len(write_buffer))
    except Exception as e:
        log.error("InfluxDB write failed: %s", e)
    write_buffer = []
    last_flush_time = time.time()


def on_connect(client, userdata, flags, rc):
    if rc == 0:
        log.info("MQTT connected, subscribing to %s", MQTT_TOPIC)
        client.subscribe(MQTT_TOPIC, qos=1)
    else:
        log.error("MQTT connect failed: rc=%d", rc)


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

    # InfluxDB 연결 (인증 정보가 있으면 사용)
    influx_kwargs = dict(host=INFLUX_HOST, port=INFLUX_PORT, database=INFLUX_DB)
    if INFLUX_USER:
        influx_kwargs["username"] = INFLUX_USER
        influx_kwargs["password"] = INFLUX_PASS
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
    mqttc.on_message = on_message

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
