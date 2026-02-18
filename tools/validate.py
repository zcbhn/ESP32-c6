#!/usr/bin/env python3
"""
RBMS 시리얼 로그 검증 — 캡처된 로그에서 기대 패턴을 확인

사용법:
    python tools/validate.py                                    # 최근 로그 자동 검증
    python tools/validate.py --log tools/logs/monitor_xxx.log   # 특정 로그 검증
    python tools/validate.py --check boot                       # 부팅 검증만
    python tools/validate.py --check sensor                     # 센서 동작 검증
    python tools/validate.py --check all                        # 전체 검증

출력:
    tools/logs/validate_YYYYMMDD_HHMMSS.json  (구조화된 검증 결과)
"""

import argparse
import json
import re
import sys
from datetime import datetime
from pathlib import Path


# 검증 규칙 정의
CHECKS = {
    "boot": {
        "description": "부팅 및 초기화 검증",
        "rules": [
            {
                "name": "firmware_start",
                "pattern": r"RBMS Firmware",
                "required": True,
                "description": "펌웨어 시작 메시지"
            },
            {
                "name": "nvs_init",
                "pattern": r"NVS|nvs",
                "required": True,
                "description": "NVS 플래시 초기화"
            },
            {
                "name": "node_type",
                "pattern": r"Type [AB]|NODE_TYPE|type_[ab]",
                "required": True,
                "description": "노드 타입 식별"
            },
            {
                "name": "no_panic",
                "pattern": r"Guru Meditation|panic|abort|LoadProhibited|StoreProhibited",
                "required": False,
                "invert": True,
                "description": "크래시/패닉 없음"
            },
            {
                "name": "no_brownout",
                "pattern": r"Brownout detector",
                "required": False,
                "invert": True,
                "description": "전압 강하 없음"
            },
        ]
    },
    "sensor": {
        "description": "센서 동작 검증",
        "rules": [
            {
                "name": "sht30_init",
                "pattern": r"[Ss][Hh][Tt]30|sht3x|I2C.*init|i2c.*init",
                "required": True,
                "description": "SHT30 센서 초기화"
            },
            {
                "name": "ds18b20_init",
                "pattern": r"[Dd][Ss]18[Bb]20|1-[Ww]ire|onewire",
                "required": True,
                "description": "DS18B20 센서 초기화"
            },
            {
                "name": "temp_reading",
                "pattern": r"temp|temperature|온도",
                "required": True,
                "description": "온도 읽기 동작"
            },
            {
                "name": "humidity_reading",
                "pattern": r"hum|humidity|습도",
                "required": False,
                "description": "습도 읽기 동작"
            },
            {
                "name": "no_sensor_error",
                "pattern": r"sensor.*error|CRC.*fail|I2C.*timeout|read.*fail",
                "required": False,
                "invert": True,
                "description": "센서 에러 없음"
            },
        ]
    },
    "thread": {
        "description": "Thread 통신 검증",
        "rules": [
            {
                "name": "thread_init",
                "pattern": r"[Oo]pen[Tt]hread|thread.*init|OT.*init",
                "required": True,
                "description": "OpenThread 초기화"
            },
            {
                "name": "thread_role",
                "pattern": r"[Rr]outer|SED|[Ss]leepy|[Ee]nd [Dd]evice|role",
                "required": False,
                "description": "Thread 역할 설정"
            },
            {
                "name": "thread_attached",
                "pattern": r"attached|connected|joined|dataset",
                "required": False,
                "description": "Thread 네트워크 연결"
            },
        ]
    },
    "control": {
        "description": "제어 시스템 검증 (Type A)",
        "rules": [
            {
                "name": "pid_init",
                "pattern": r"PID|pid.*init",
                "required": False,
                "description": "PID 컨트롤러 초기화"
            },
            {
                "name": "ssr_init",
                "pattern": r"SSR|ssr.*init|heater|GPIO.*output",
                "required": False,
                "description": "SSR 출력 초기화"
            },
            {
                "name": "scheduler_init",
                "pattern": r"[Ss]cheduler|schedule|light.*schedule",
                "required": False,
                "description": "스케줄러 초기화"
            },
            {
                "name": "safety_init",
                "pattern": r"[Ss]afety|overtemp|safety.*init",
                "required": False,
                "description": "안전 감시 초기화"
            },
        ]
    },
    "power": {
        "description": "전원 관리 검증 (Type B)",
        "rules": [
            {
                "name": "deep_sleep",
                "pattern": r"[Dd]eep [Ss]leep|sleep.*enter|wakeup|wake.*up",
                "required": False,
                "description": "Deep Sleep 동작"
            },
            {
                "name": "battery",
                "pattern": r"battery|batt|voltage|ADC",
                "required": False,
                "description": "배터리 모니터링"
            },
            {
                "name": "adaptive_poll",
                "pattern": r"adaptive|poll.*interval|polling",
                "required": False,
                "description": "적응형 폴링"
            },
        ]
    },
}


def validate_log(log_path: Path, check_names: list) -> dict:
    """로그 파일을 읽고 검증 규칙을 적용"""

    if not log_path.exists():
        return {
            "status": "ERROR",
            "error": f"로그 파일 없음: {log_path}",
            "checks": {}
        }

    content = log_path.read_text(encoding="utf-8", errors="replace")
    lines = content.split("\n")

    # 헤더 제외
    data_lines = [l for l in lines if not l.startswith("#")]

    results = {
        "status": "UNKNOWN",
        "log_file": str(log_path),
        "total_lines": len(data_lines),
        "timestamp": datetime.now().isoformat(),
        "checks": {},
        "summary": {
            "total": 0,
            "passed": 0,
            "failed": 0,
            "skipped": 0,
            "warnings": 0
        }
    }

    for check_name in check_names:
        if check_name not in CHECKS:
            continue

        check = CHECKS[check_name]
        check_result = {
            "description": check["description"],
            "rules": []
        }

        for rule in check["rules"]:
            invert = rule.get("invert", False)
            match_found = bool(re.search(rule["pattern"], content, re.IGNORECASE))

            if invert:
                passed = not match_found
            else:
                passed = match_found

            # 매칭된 라인 찾기 (최대 3개)
            matched_lines = []
            if match_found:
                for line in data_lines:
                    if re.search(rule["pattern"], line, re.IGNORECASE):
                        matched_lines.append(line.strip())
                        if len(matched_lines) >= 3:
                            break

            rule_result = {
                "name": rule["name"],
                "description": rule["description"],
                "required": rule["required"],
                "passed": passed,
                "matched_lines": matched_lines
            }

            if invert and not passed:
                rule_result["note"] = "비정상 패턴이 감지되었습니다"

            check_result["rules"].append(rule_result)
            results["summary"]["total"] += 1

            if passed:
                results["summary"]["passed"] += 1
            elif rule["required"]:
                results["summary"]["failed"] += 1
            else:
                results["summary"]["warnings"] += 1

        results["checks"][check_name] = check_result

    # 전체 상태 판정
    if results["summary"]["failed"] > 0:
        results["status"] = "FAIL"
    elif results["summary"]["warnings"] > 0:
        results["status"] = "WARN"
    elif results["summary"]["passed"] > 0:
        results["status"] = "PASS"
    else:
        results["status"] = "EMPTY"

    return results


def print_results(results: dict):
    """검증 결과를 보기 좋게 출력"""
    status = results["status"]
    color_map = {"PASS": "\033[92m", "FAIL": "\033[91m", "WARN": "\033[93m", "EMPTY": "\033[90m", "ERROR": "\033[91m"}
    reset = "\033[0m"
    color = color_map.get(status, "")

    print("=" * 60)
    print(f"  RBMS 검증 결과: {color}{status}{reset}")
    print(f"  로그: {results.get('log_file', 'N/A')}")
    print(f"  라인 수: {results.get('total_lines', 0)}")
    print("=" * 60)

    if "error" in results:
        print(f"\n  [에러] {results['error']}")
        return

    for check_name, check_data in results.get("checks", {}).items():
        print(f"\n  [{check_name}] {check_data['description']}")
        print(f"  {'-' * 50}")

        for rule in check_data["rules"]:
            if rule["passed"]:
                icon = "\033[92m PASS\033[0m"
            elif rule["required"]:
                icon = "\033[91m FAIL\033[0m"
            else:
                icon = "\033[93m WARN\033[0m"

            print(f"    {icon}  {rule['description']}")
            if rule.get("matched_lines"):
                for ml in rule["matched_lines"][:2]:
                    print(f"           > {ml[:80]}")
            if rule.get("note"):
                print(f"           ! {rule['note']}")

    s = results["summary"]
    print(f"\n  {'=' * 50}")
    print(f"  합계: {s['passed']} PASS / {s['failed']} FAIL / {s['warnings']} WARN / {s['total']} TOTAL")
    print()


def main():
    parser = argparse.ArgumentParser(description="RBMS 시리얼 로그 검증")
    parser.add_argument("--log", type=str, default=None, help="로그 파일 경로 (기본: 최근 로그)")
    parser.add_argument("--check", type=str, default="all",
                        help="검증 항목: boot, sensor, thread, control, power, all (기본: all)")
    parser.add_argument("--json", action="store_true", help="JSON 형식으로 출력")
    parser.add_argument("--save", action="store_true", help="검증 결과를 JSON 파일로 저장")
    args = parser.parse_args()

    project_root = Path(__file__).parent.parent
    log_dir = project_root / "tools" / "logs"

    # 로그 파일 결정
    if args.log:
        log_path = Path(args.log)
    else:
        latest = log_dir / "latest_monitor.log"
        if latest.exists():
            log_path = latest
        else:
            # 가장 최근 monitor 로그 찾기
            monitor_logs = sorted(log_dir.glob("monitor_*.log"), reverse=True)
            if monitor_logs:
                log_path = monitor_logs[0]
            else:
                print("[에러] 모니터 로그를 찾을 수 없습니다.")
                print("  먼저 monitor.py를 실행하세요.")
                sys.exit(1)

    # 검증 항목 결정
    if args.check == "all":
        check_names = list(CHECKS.keys())
    else:
        check_names = [c.strip() for c in args.check.split(",")]

    # 검증 실행
    results = validate_log(log_path, check_names)

    # 출력
    if args.json:
        print(json.dumps(results, indent=2, ensure_ascii=False))
    else:
        print_results(results)

    # JSON 파일 저장
    if args.save:
        timestamp = datetime.now().strftime("%Y%m%d_%H%M%S")
        out_file = log_dir / f"validate_{timestamp}.json"
        out_file.write_text(json.dumps(results, indent=2, ensure_ascii=False), encoding="utf-8")
        print(f"  결과 저장: {out_file}")

    # 종료 코드
    if results["status"] == "FAIL":
        sys.exit(1)
    elif results["status"] == "ERROR":
        sys.exit(2)
    else:
        sys.exit(0)


if __name__ == "__main__":
    main()
