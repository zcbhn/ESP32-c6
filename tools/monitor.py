#!/usr/bin/env python3
"""
RBMS 시리얼 모니터 — Claude가 읽을 수 있는 로그 파일로 캡처

사용법:
    python tools/monitor.py                          # 기본: COM3, 30초
    python tools/monitor.py --port COM3 --duration 60
    python tools/monitor.py --port COM3 --until "RBMS Firmware"
    python tools/monitor.py --port COM3 --duration 0  # 무한 (Ctrl+C로 종료)

출력:
    tools/logs/monitor_YYYYMMDD_HHMMSS.log
"""

import argparse
import os
import sys
import time
import re
from datetime import datetime
from pathlib import Path

def check_pyserial():
    """pyserial 설치 확인 및 자동 설치"""
    try:
        import serial
        return serial
    except ImportError:
        print("[!] pyserial이 설치되어 있지 않습니다. 자동 설치 중...")
        os.system(f"{sys.executable} -m pip install pyserial")
        import serial
        return serial


def main():
    parser = argparse.ArgumentParser(description="RBMS 시리얼 모니터 (로그 캡처)")
    parser.add_argument("--port", default="COM3", help="COM 포트 (기본: COM3)")
    parser.add_argument("--baud", type=int, default=115200, help="보드레이트 (기본: 115200)")
    parser.add_argument("--duration", type=int, default=30, help="캡처 시간(초). 0=무한 (기본: 30)")
    parser.add_argument("--until", type=str, default=None, help="이 문자열이 나타나면 종료")
    parser.add_argument("--output", type=str, default=None, help="출력 파일 경로 (기본: 자동 생성)")
    parser.add_argument("--reset", action="store_true", help="모니터 시작 시 DTR/RTS로 리셋")
    args = parser.parse_args()

    serial = check_pyserial()

    # 로그 디렉토리
    project_root = Path(__file__).parent.parent
    log_dir = project_root / "tools" / "logs"
    log_dir.mkdir(parents=True, exist_ok=True)

    # 출력 파일
    if args.output:
        log_file = Path(args.output)
    else:
        timestamp = datetime.now().strftime("%Y%m%d_%H%M%S")
        log_file = log_dir / f"monitor_{timestamp}.log"

    print("=" * 50)
    print(f"  RBMS 시리얼 모니터")
    print(f"  포트: {args.port} @ {args.baud} baud")
    print(f"  시간: {'무한' if args.duration == 0 else f'{args.duration}초'}")
    if args.until:
        print(f"  종료 조건: '{args.until}' 문자열 감지")
    print(f"  로그: {log_file}")
    print("=" * 50)

    try:
        ser = serial.Serial(
            port=args.port,
            baudrate=args.baud,
            timeout=1,
            rts=False,
            dtr=False
        )
    except serial.SerialException as e:
        print(f"\n[에러] {args.port} 열기 실패: {e}")
        print(f"\n[해결 방법]")
        print(f"  1. ESP32-C6 USB 연결 확인")
        print(f"  2. 다른 프로그램이 COM 포트를 사용 중인지 확인")
        print(f"  3. 장치 관리자에서 드라이버 상태 확인")

        # 사용 가능한 포트 목록
        from serial.tools import list_ports
        ports = list_ports.comports()
        if ports:
            print(f"\n  사용 가능한 포트:")
            for p in ports:
                print(f"    {p.device}: {p.description} [{p.hwid}]")
        sys.exit(1)

    # 리셋 수행
    if args.reset:
        print("\n  ESP32 리셋 중...")
        ser.dtr = False
        ser.rts = True
        time.sleep(0.1)
        ser.rts = False
        time.sleep(0.5)

    print(f"\n  캡처 시작... (Ctrl+C로 중단)\n")
    print("-" * 50)

    line_count = 0
    start_time = time.time()
    found_pattern = False

    try:
        with open(log_file, "w", encoding="utf-8") as f:
            # 헤더
            f.write(f"# RBMS Serial Monitor Log\n")
            f.write(f"# Port: {args.port} @ {args.baud}\n")
            f.write(f"# Started: {datetime.now().isoformat()}\n")
            f.write(f"# Duration: {'infinite' if args.duration == 0 else f'{args.duration}s'}\n")
            f.write(f"#\n")
            f.flush()

            while True:
                # 시간 초과 확인
                elapsed = time.time() - start_time
                if args.duration > 0 and elapsed >= args.duration:
                    print(f"\n\n  [{args.duration}초 경과 — 캡처 종료]")
                    break

                # 데이터 읽기
                if ser.in_waiting > 0:
                    try:
                        raw = ser.readline()
                        line = raw.decode("utf-8", errors="replace").rstrip()
                    except Exception:
                        continue

                    if line:
                        ts = f"[{elapsed:8.3f}]"
                        output = f"{ts} {line}"
                        print(output)
                        f.write(f"{output}\n")
                        f.flush()
                        line_count += 1

                        # 패턴 매칭 종료 조건
                        if args.until and args.until in line:
                            print(f"\n\n  [패턴 감지: '{args.until}' — 캡처 종료]")
                            found_pattern = True
                            # 패턴 발견 후 추가 2초간 캡처
                            extra_end = time.time() + 2
                            while time.time() < extra_end:
                                if ser.in_waiting > 0:
                                    try:
                                        raw = ser.readline()
                                        line = raw.decode("utf-8", errors="replace").rstrip()
                                        if line:
                                            elapsed = time.time() - start_time
                                            ts = f"[{elapsed:8.3f}]"
                                            output = f"{ts} {line}"
                                            print(output)
                                            f.write(f"{output}\n")
                                            f.flush()
                                            line_count += 1
                                    except Exception:
                                        pass
                                time.sleep(0.01)
                            break
                else:
                    time.sleep(0.01)

    except KeyboardInterrupt:
        print(f"\n\n  [Ctrl+C — 캡처 중단]")

    finally:
        ser.close()

    # 요약
    elapsed = time.time() - start_time
    print("-" * 50)
    print(f"\n  캡처 완료")
    print(f"  총 라인: {line_count}")
    print(f"  경과 시간: {elapsed:.1f}초")
    print(f"  로그 파일: {log_file}")

    if found_pattern:
        print(f"  패턴 매칭: '{args.until}' 발견됨")

    # 최근 로그 심볼릭 링크 (항상 같은 이름으로 접근 가능)
    latest_link = log_dir / "latest_monitor.log"
    try:
        if latest_link.exists():
            latest_link.unlink()
        # Windows에서 심볼릭 링크 대신 복사
        import shutil
        shutil.copy2(log_file, latest_link)
    except Exception:
        pass

    return 0 if line_count > 0 else 1


if __name__ == "__main__":
    sys.exit(main())
