#Requires -Version 5.1
<#
.SYNOPSIS
    ESP-IDF 환경변수 설정 헬퍼
.DESCRIPTION
    다른 스크립트에서 dot-source로 호출하여 ESP-IDF 환경을 설정합니다.
    사용법: . .\tools\env.ps1
#>

$IdfPath = if ($env:IDF_PATH) { $env:IDF_PATH } else { "C:\esp\esp-idf" }

if (-not (Test-Path "$IdfPath\export.ps1")) {
    Write-Error "ESP-IDF를 찾을 수 없습니다: $IdfPath"
    Write-Error "tools\setup_esp_idf.ps1 을 먼저 실행하세요."
    exit 1
}

# ESP-IDF 환경변수 로드
& "$IdfPath\export.ps1"
