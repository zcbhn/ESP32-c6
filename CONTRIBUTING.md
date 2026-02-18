# 기여 가이드 (Contributing)

RBMS 프로젝트에 기여해 주셔서 감사합니다.

## 시작하기

1. 저장소를 Fork
2. Feature branch 생성: `git checkout -b feature/your-feature`
3. 변경사항 커밋: `git commit -m 'feat: add your feature'`
4. Push: `git push origin feature/your-feature`
5. Pull Request 생성

## 브랜치 전략

| 브랜치 | 용도 |
|--------|------|
| `main` | 안정 릴리즈 |
| `develop` | 개발 통합 |
| `feature/*` | 기능 개발 |
| `fix/*` | 버그 수정 |
| `docs/*` | 문서 수정 |

## 커밋 메시지 규칙

[Conventional Commits](https://www.conventionalcommits.org/) 형식을 따릅니다.

```
<type>(<scope>): <description>

[optional body]
```

### Type

| Type | 설명 |
|------|------|
| `feat` | 새로운 기능 |
| `fix` | 버그 수정 |
| `docs` | 문서 변경 |
| `refactor` | 리팩토링 (기능 변경 없음) |
| `test` | 테스트 추가/수정 |
| `chore` | 빌드, CI 등 기타 |
| `safety` | 안전 관련 변경 |

### Scope (선택)

`sensor`, `actuator`, `control`, `safety`, `comm`, `power`, `config`, `server`, `docs`

### 예시

```
feat(sensor): implement SHT30 I2C driver with CRC validation
fix(safety): correct overtemp threshold calculation
docs: add development guide
chore(ci): add firmware artifact upload
```

## 코드 스타일

### C 코드 (펌웨어)

- **들여쓰기**: 4 spaces (탭 금지)
- **네이밍**: `snake_case` (함수, 변수), `UPPER_SNAKE_CASE` (매크로, 상수)
- **중괄호**: K&R 스타일 (함수 정의만 별도 줄)
- **라인 길이**: 100자 이내 권장
- **헤더 가드**: `#ifndef` / `#define` / `#endif`

```c
// 좋은 예
esp_err_t sht30_read(float *temperature, float *humidity)
{
    if (temperature == NULL || humidity == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    uint8_t raw[6];
    esp_err_t ret = i2c_read_bytes(raw, sizeof(raw));
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "I2C read failed: %s", esp_err_to_name(ret));
        return ret;
    }

    *temperature = convert_temp(raw);
    *humidity = convert_hum(raw + 3);
    return ESP_OK;
}
```

### 에러 처리

- 공개 API는 `esp_err_t` 반환
- 내부 함수는 bool 또는 값 반환 가능
- 치명적 에러만 `ESP_ERROR_CHECK()` 사용 (초기화 등)
- 런타임 에러는 조건 검사 + 로깅

### 로깅

```c
ESP_LOGI(TAG, "정상 동작 정보");
ESP_LOGW(TAG, "경고: 비정상적이나 계속 동작");
ESP_LOGE(TAG, "에러: 기능 실패");
ESP_LOGD(TAG, "디버그: 개발 중에만 필요한 정보");
```

## Pull Request 가이드

### PR 체크리스트

- [ ] 빌드 성공 (Type A, Type B 모두)
- [ ] 안전 관련 변경 시 safety 컴포넌트 리뷰 필수
- [ ] 새 Kconfig 옵션은 help 텍스트 포함
- [ ] 새 컴포넌트는 CMakeLists.txt 의존성 명시
- [ ] 헤더 파일에 공개 API 선언 완료

### 리뷰 기준

1. **안전**: 과열 방지, 센서 이상 처리가 올바른가?
2. **전력**: Type B의 전류 소비에 영향을 주는가?
3. **호환성**: 기존 프리셋/설정과 호환되는가?
4. **Thread**: 메시 네트워크 안정성에 영향을 주는가?

## 이슈 작성

- 버그: `.github/ISSUE_TEMPLATE/bug_report.md` 템플릿 사용
- 기능 제안: `.github/ISSUE_TEMPLATE/feature_request.md` 템플릿 사용
- 제목에 `[BUG]` 또는 `[FEAT]` 접두사 사용

## 라이선스

기여한 코드는 MIT 라이선스로 배포됩니다.
