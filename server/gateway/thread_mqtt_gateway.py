#!/usr/bin/env python3
"""
RBMS Thread UDP -> MQTT Gateway

Thread Border Router(ot-br-posix)를 통해 수신되는
ESP32-C6 노드의 UDP/CBOR 텔레메트리를 MQTT로 변환 발행.

데이터 흐름:
  ESP32-C6 (Thread UDP, ff03::1:5684)
    -> ot-br-posix (Border Router, wpan0)
    -> 이 게이트웨이 (UDP 수신, CBOR 검증)
    -> Mosquitto (MQTT publish, raw CBOR)
    -> mqtt_influx_bridge.py (CBOR 디코딩 -> InfluxDB)

노드 ID: 송신자 IPv6 주소의 마지막 4자리 hex
토픽: rbms/<node_id>/telemetry
"""

import logging
import os
import signal
import socket
import struct
import sys
import time

import cbor2
import paho.mqtt.client as mqtt

# --- Configuration (env vars with defaults) ---
MQTT_HOST = os.environ.get("MQTT_HOST", "localhost")
MQTT_PORT = int(os.environ.get("MQTT_PORT", "1883"))
MQTT_USER = os.environ.get("MQTT_USER", "rbms_bridge")
MQTT_PASS = os.environ.get("MQTT_PASS", "rbms_bridge_pass")

UDP_PORT = int(os.environ.get("THREAD_UDP_PORT", "5684"))
THREAD_IFACE = os.environ.get("THREAD_IFACE", "wpan0")
MULTICAST_GROUP = os.environ.get("THREAD_MULTICAST", "ff03::1")

# CBOR integer key -> field name (firmware cbor_codec.c 와 동일)
VALID_KEYS = {1, 2, 3, 4, 5, 6, 7}

logging.basicConfig(
    level=os.environ.get("LOG_LEVEL", "INFO").upper(),
    format="%(asctime)s [%(levelname)s] %(message)s",
)
log = logging.getLogger("gateway")


def extract_node_id(addr: str) -> str:
    """IPv6 주소에서 node_id 추출 (마지막 4자리 hex)

    예: fd12:3456:789a:1:0:ff:fe00:a8c3 -> a8c3
    """
    parts = addr.split(":")
    last = parts[-1] if parts else "0000"
    return last.lower().zfill(4)


def create_udp_socket() -> socket.socket:
    """Thread 인터페이스에서 멀티캐스트 UDP 수신 소켓 생성"""
    sock = socket.socket(socket.AF_INET6, socket.SOCK_DGRAM, socket.IPPROTO_UDP)
    sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)

    # Bind to all interfaces
    sock.bind(("::", UDP_PORT))
    log.info("UDP socket bound to [::]:%d", UDP_PORT)

    # Join multicast group on Thread interface
    try:
        iface_index = socket.if_nametoindex(THREAD_IFACE)
        mreq = socket.inet_pton(socket.AF_INET6, MULTICAST_GROUP)
        mreq += struct.pack("@I", iface_index)
        sock.setsockopt(socket.IPPROTO_IPV6, socket.IPV6_JOIN_GROUP, mreq)
        log.info("Joined multicast %s on %s (index %d)",
                 MULTICAST_GROUP, THREAD_IFACE, iface_index)
    except OSError as e:
        log.warning("Multicast join failed (%s may not exist yet): %s",
                    THREAD_IFACE, e)
        log.info("Will receive unicast UDP only on port %d", UDP_PORT)

    sock.settimeout(1.0)
    return sock


def validate_cbor(data: bytes) -> bool:
    """CBOR 페이로드 유효성 검증"""
    try:
        payload = cbor2.loads(data)
        if not isinstance(payload, dict):
            return False
        # At least one valid key must be present
        return any(k in VALID_KEYS for k in payload.keys())
    except Exception:
        return False


def on_mqtt_connect(client, userdata, flags, rc):
    if rc == 0:
        log.info("MQTT connected to %s:%d", MQTT_HOST, MQTT_PORT)
    else:
        log.error("MQTT connect failed: rc=%d", rc)


def on_mqtt_disconnect(client, userdata, rc):
    if rc != 0:
        log.warning("MQTT unexpected disconnect: rc=%d, will reconnect", rc)


def main():
    # MQTT client
    mqttc = mqtt.Client(client_id="rbms-gateway")
    mqttc.username_pw_set(MQTT_USER, MQTT_PASS)
    mqttc.on_connect = on_mqtt_connect
    mqttc.on_disconnect = on_mqtt_disconnect
    mqttc.reconnect_delay_set(min_delay=1, max_delay=30)
    mqttc.connect(MQTT_HOST, MQTT_PORT, keepalive=60)
    mqttc.loop_start()

    # UDP socket
    sock = create_udp_socket()
    log.info("Gateway started: Thread UDP :%d -> MQTT %s:%d",
             UDP_PORT, MQTT_HOST, MQTT_PORT)

    running = True

    def stop(sig, frame):
        nonlocal running
        log.info("Shutdown signal received")
        running = False

    signal.signal(signal.SIGTERM, stop)
    signal.signal(signal.SIGINT, stop)

    stats = {"rx": 0, "pub": 0, "err": 0, "invalid": 0}
    stats_interval = 300  # 5 min
    last_stats_time = time.time()

    try:
        while running:
            try:
                data, addr_info = sock.recvfrom(512)
                addr = addr_info[0]  # IPv6 address string
            except socket.timeout:
                # Periodic stats log
                if time.time() - last_stats_time >= stats_interval:
                    log.info("Stats: rx=%d pub=%d err=%d invalid=%d",
                             stats["rx"], stats["pub"],
                             stats["err"], stats["invalid"])
                    last_stats_time = time.time()
                continue
            except OSError as e:
                log.error("UDP recv error: %s", e)
                stats["err"] += 1
                time.sleep(1)
                continue

            stats["rx"] += 1
            node_id = extract_node_id(addr)

            # Validate CBOR before forwarding
            if not validate_cbor(data):
                stats["invalid"] += 1
                log.warning("Invalid CBOR from %s (node %s), %d bytes",
                            addr, node_id, len(data))
                continue

            # Forward raw CBOR to MQTT
            # mqtt_influx_bridge.py handles CBOR decoding
            topic = f"rbms/{node_id}/telemetry"
            result = mqttc.publish(topic, data, qos=1)

            if result.rc == mqtt.MQTT_ERR_SUCCESS:
                stats["pub"] += 1
                log.debug("Node %s: %d bytes -> %s", node_id, len(data), topic)
            else:
                stats["err"] += 1
                log.error("MQTT publish failed for node %s: rc=%d",
                          node_id, result.rc)

    finally:
        sock.close()
        mqttc.loop_stop()
        mqttc.disconnect()
        log.info("Gateway stopped (rx=%d pub=%d err=%d invalid=%d)",
                 stats["rx"], stats["pub"], stats["err"], stats["invalid"])


if __name__ == "__main__":
    main()
