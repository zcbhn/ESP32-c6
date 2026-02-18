#Requires -Version 5.1
<#
.SYNOPSIS
    펌웨어 플래시 자동화
.PARAMETER Port
    COM 포트 (기본값: COM3)
.PARAMETER Baud
    플래시 속도 (기본값: 460800)
.EXAMPLE
    .\tools\flash.ps1
    .\tools\flash.ps1 -Port COM3 -Baud 921600
#>

param(
    [string]$Port = "COM3",
    [int]$Baud = 460800
)

$ErrorActionPreference = "Stop"
$ProjectRoot = Split-Path $PSScriptRoot -Parent
$FirmwareDir = Join-Path $ProjectRoot "firmware"
$LogDir = Join-Path $ProjectRoot "tools\logs"
$Timestamp = Get-Date -Format "yyyyMMdd_HHmmss"
$LogFile = Join-Path $LogDir "flash_${Timestamp}.log"

if (-not (Test-Path $LogDir)) {
    New-Item -ItemType Directory -Path $LogDir -Force | Out-Null
}

Write-Host "========================================" -ForegroundColor Cyan
Write-Host " RBMS 펌웨어 플래시" -ForegroundColor Cyan
Write-Host " 포트: $Port / 속도: $Baud" -ForegroundColor Gray
Write-Host " 로그: $LogFile" -ForegroundColor Gray
Write-Host "========================================" -ForegroundColor Cyan

# ESP-IDF 환경 로드
. "$PSScriptRoot\env.ps1"

# 포트 존재 확인
$ports = [System.IO.Ports.SerialPort]::getportnames()
if ($Port -notin $ports) {
    Write-Host ""
    Write-Host "  [경고] $Port 를 찾을 수 없습니다." -ForegroundColor Red
    Write-Host "  사용 가능한 포트: $($ports -join ', ')" -ForegroundColor Yellow
    Write-Host ""
    Write-Host "  ESP32-C6 USB 연결을 확인하세요." -ForegroundColor Yellow
    Write-Host "  드라이버가 'Unknown' 상태이면 장치 관리자에서 드라이버를 업데이트하세요." -ForegroundColor Gray
    exit 1
}

Push-Location $FirmwareDir

try {
    # 빌드 결과 확인
    $binFile = Join-Path $FirmwareDir "build\rbms_firmware.bin"
    if (-not (Test-Path $binFile)) {
        Write-Host "  빌드 결과물이 없습니다. 먼저 build.ps1을 실행하세요." -ForegroundColor Red
        exit 1
    }

    Write-Host "  플래시 시작..." -ForegroundColor Yellow
    idf.py -p $Port -b $Baud flash 2>&1 | Tee-Object -FilePath $LogFile -Append
    $flashResult = $LASTEXITCODE

    if ($flashResult -eq 0) {
        Write-Host ""
        Write-Host "  플래시 성공!" -ForegroundColor Green
    } else {
        Write-Host ""
        Write-Host "  플래시 실패! (exit code: $flashResult)" -ForegroundColor Red
        Write-Host ""
        Write-Host "  [해결 방법]" -ForegroundColor Yellow
        Write-Host "  1. ESP32-C6의 BOOT 버튼을 누른 채 RESET 버튼 클릭" -ForegroundColor White
        Write-Host "  2. BOOT 버튼 해제 후 다시 플래시 시도" -ForegroundColor White
        Write-Host "  3. USB 케이블 재연결" -ForegroundColor White
    }
} finally {
    Pop-Location
}

exit $flashResult
