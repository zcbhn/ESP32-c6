# 설계 문서

| 문서 | 번호 | 설명 |
|------|------|------|
| 시스템 사양서 | RBMS-SPEC-001 v2.0 | 시스템 요구사항, 50대 규모 |
| 개발 로드맵 & 아키텍처 | RBMS-DEV-001 | Step 1~5, 펌웨어 모듈 설계 |
| 하드웨어 설계 참조 | RBMS-HW-001 | GPIO 매핑, 회로 참조, BOM |

## 회로도 (SVG)

`schematics/` 디렉토리에 SchemDraw로 생성한 참조 회로도가 있습니다.

- `1_type_b_power.svg` — 배터리 전원부
- `2_type_b_i2c.svg` — SHT30 I2C 연결
- `3_type_b_1wire.svg` — DS18B20 1-Wire + GPIO 전원 스위칭
- `4_type_a_control.svg` — SSR + MOSFET PWM 제어부
- `5_type_b_adc.svg` — 배터리 전압 모니터링
