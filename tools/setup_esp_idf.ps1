#Requires -Version 5.1
<#
.SYNOPSIS
    ESP-IDF v5.2 자동 설치 스크립트 (ESP32-C6 전용)
.DESCRIPTION
    ESP-IDF를 git clone하고, install.ps1로 도구체인을 설치합니다.
    설치 완료 후 export.ps1로 환경변수를 설정합니다.
.PARAMETER IdfPath
    ESP-IDF 설치 경로 (기본값: C:\esp\esp-idf)
.PARAMETER IdfVersion
    ESP-IDF 버전 태그 (기본값: v5.2.3)
#>

param(
    [string]$IdfPath = "C:\esp\esp-idf",
    [string]$IdfVersion = "v5.2.3"
)

$ErrorActionPreference = "Stop"
$ProgressPreference = "SilentlyContinue"

Write-Host "========================================" -ForegroundColor Cyan
Write-Host " ESP-IDF $IdfVersion 자동 설치" -ForegroundColor Cyan
Write-Host " 대상: ESP32-C6" -ForegroundColor Cyan
Write-Host "========================================" -ForegroundColor Cyan
Write-Host ""

# 1. 사전 요구사항 확인
Write-Host "[1/5] 사전 요구사항 확인..." -ForegroundColor Yellow

# Git 확인
$git = Get-Command git -ErrorAction SilentlyContinue
if (-not $git) {
    Write-Error "Git이 설치되어 있지 않습니다. https://git-scm.com/ 에서 설치하세요."
    exit 1
}
Write-Host "  Git: $(git --version)" -ForegroundColor Green

# Python 확인
$python = Get-Command python -ErrorAction SilentlyContinue
if (-not $python) {
    Write-Error "Python이 설치되어 있지 않습니다. https://python.org/ 에서 3.8+ 설치하세요."
    exit 1
}
$pyVer = python --version 2>&1
Write-Host "  Python: $pyVer" -ForegroundColor Green

# 2. ESP-IDF 클론
Write-Host ""
Write-Host "[2/5] ESP-IDF 클론 ($IdfVersion)..." -ForegroundColor Yellow

$IdfParent = Split-Path $IdfPath -Parent
if (-not (Test-Path $IdfParent)) {
    New-Item -ItemType Directory -Path $IdfParent -Force | Out-Null
    Write-Host "  디렉토리 생성: $IdfParent" -ForegroundColor Gray
}

if (Test-Path "$IdfPath\.git") {
    Write-Host "  ESP-IDF가 이미 존재합니다: $IdfPath" -ForegroundColor Gray
    Push-Location $IdfPath
    $currentTag = git describe --tags --exact-match 2>$null
    if ($currentTag -ne $IdfVersion) {
        Write-Host "  버전 변경: $currentTag -> $IdfVersion" -ForegroundColor Gray
        git fetch --tags
        git checkout $IdfVersion
        git submodule update --init --recursive
    } else {
        Write-Host "  이미 $IdfVersion 입니다." -ForegroundColor Green
    }
    Pop-Location
} else {
    Write-Host "  클론 중... (수 분 소요)" -ForegroundColor Gray
    git clone -b $IdfVersion --recursive --depth 1 --shallow-submodules `
        https://github.com/espressif/esp-idf.git $IdfPath
    Write-Host "  클론 완료" -ForegroundColor Green
}

# 3. ESP-IDF 도구 설치
Write-Host ""
Write-Host "[3/5] ESP-IDF 도구 설치 (cmake, ninja, xtensa toolchain)..." -ForegroundColor Yellow
Write-Host "  이 단계는 수 분 소요됩니다..." -ForegroundColor Gray

Push-Location $IdfPath
& python "$IdfPath\tools\idf_tools.py" install-python-env
& .\install.ps1 esp32c6
Pop-Location

Write-Host "  도구 설치 완료" -ForegroundColor Green

# 4. 환경변수 설정 확인
Write-Host ""
Write-Host "[4/5] 환경변수 설정..." -ForegroundColor Yellow

# IDF_PATH를 사용자 환경변수에 등록
[System.Environment]::SetEnvironmentVariable("IDF_PATH", $IdfPath, "User")
Write-Host "  IDF_PATH=$IdfPath (사용자 환경변수 등록)" -ForegroundColor Green

# 현재 세션에도 설정
$env:IDF_PATH = $IdfPath

# export.ps1 실행하여 PATH 등 설정
& "$IdfPath\export.ps1"

# 5. 설치 검증
Write-Host ""
Write-Host "[5/5] 설치 검증..." -ForegroundColor Yellow

$idfPy = Get-Command idf.py -ErrorAction SilentlyContinue
if ($idfPy) {
    Write-Host "  idf.py: 사용 가능" -ForegroundColor Green
} else {
    Write-Host "  [경고] idf.py를 찾을 수 없습니다." -ForegroundColor Red
    Write-Host "  새 터미널에서 다음 명령 실행 후 다시 시도하세요:" -ForegroundColor Yellow
    Write-Host "    & '$IdfPath\export.ps1'" -ForegroundColor White
}

$cmake = Get-Command cmake -ErrorAction SilentlyContinue
if ($cmake) {
    Write-Host "  cmake: $(cmake --version | Select-Object -First 1)" -ForegroundColor Green
}

$ninja = Get-Command ninja -ErrorAction SilentlyContinue
if ($ninja) {
    Write-Host "  ninja: $(ninja --version)" -ForegroundColor Green
}

# USB 드라이버 안내
Write-Host ""
Write-Host "========================================" -ForegroundColor Cyan
Write-Host " 설치 완료!" -ForegroundColor Green
Write-Host "========================================" -ForegroundColor Cyan
Write-Host ""
Write-Host "[USB 드라이버 안내]" -ForegroundColor Yellow
Write-Host "  ESP32-C6 USB JTAG/Serial 드라이버가 'Unknown' 상태인 경우:" -ForegroundColor Gray
Write-Host "  1. 장치 관리자 → USB JTAG/serial debug unit → 드라이버 업데이트" -ForegroundColor White
Write-Host "  2. '컴퓨터에서 드라이버 찾아보기' 선택" -ForegroundColor White
Write-Host "  3. 경로: $IdfPath\tools\espressif-ide" -ForegroundColor White
Write-Host "  또는 Zadig (https://zadig.akeo.ie/) 로 WinUSB 드라이버 설치" -ForegroundColor White
Write-Host ""
Write-Host "[다음 단계]" -ForegroundColor Yellow
Write-Host "  새 PowerShell 열고:" -ForegroundColor Gray
Write-Host "    & '$IdfPath\export.ps1'" -ForegroundColor White
Write-Host "    cd C:\Users\home\ESP32-c6\firmware" -ForegroundColor White
Write-Host "    idf.py build" -ForegroundColor White
