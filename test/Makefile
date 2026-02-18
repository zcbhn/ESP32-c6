# RBMS Host-based Unit Tests
# Runs on CI without ESP32 hardware using mock ESP-IDF headers
#
# Usage:
#   make            # build and run all tests
#   make test_pid   # build and run PID test only
#   make clean

CC ?= gcc
CFLAGS = -Wall -Wextra -std=c11 -g \
         -I mocks \
         -I unity \
         -I ../firmware/components/control/include \
         -I ../firmware/components/comm/include \
         -I ../firmware/components/config/include

LDFLAGS = -lm

UNITY_SRC = unity/unity.c
FIRMWARE = ../firmware/components

TESTS = test_pid test_cbor_codec test_adaptive_poll

.PHONY: all clean run

all: $(TESTS) run

run: $(TESTS)
	@echo "=== Running Unit Tests ==="
	@pass=0; fail=0; \
	for t in $(TESTS); do \
		echo "--- $$t ---"; \
		./$$t; \
		if [ $$? -eq 0 ]; then pass=$$((pass+1)); else fail=$$((fail+1)); fi; \
	done; \
	echo ""; \
	echo "=== Results: $$pass passed, $$fail failed ===";\
	exit $$fail

test_pid: test_pid.c $(FIRMWARE)/control/pid.c $(UNITY_SRC)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

test_cbor_codec: test_cbor_codec.c $(FIRMWARE)/comm/cbor_codec.c $(UNITY_SRC)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

test_adaptive_poll: test_adaptive_poll.c $(FIRMWARE)/control/adaptive_poll.c $(UNITY_SRC)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

clean:
	rm -f $(TESTS)
