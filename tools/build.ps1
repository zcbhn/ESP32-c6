#Requires -Version 5.1
<#
.SYNOPSIS
    펌웨어 빌드 자동화
.PARAMETER NodeType
    노드 타입: A (풀 노드) 또는 B (배터리 노드)
.PARAMETER Clean
    클린 빌드 수행
.EXAMPLE
    .\tools\build.ps1 -NodeType B
    .\tools\build.ps1 -NodeType A -Clean
#>

param(
    [ValidateSet("A", "B")]
    [string]$NodeType = "B",
    [switch]$Clean
)

$ErrorActionPreference = "Stop"
$ProjectRoot = Split-Path $PSScriptRoot -Parent
$FirmwareDir = Join-Path $ProjectRoot "firmware"
$LogDir = Join-Path $ProjectRoot "tools\logs"
$Timestamp = Get-Date -Format "yyyyMMdd_HHmmss"
$LogFile = Join-Path $LogDir "build_${NodeType}_${Timestamp}.log"

# 로그 디렉토리 생성
if (-not (Test-Path $LogDir)) {
    New-Item -ItemType Directory -Path $LogDir -Force | Out-Null
}

Write-Host "========================================" -ForegroundColor Cyan
Write-Host " RBMS 펌웨어 빌드 - Type $NodeType" -ForegroundColor Cyan
Write-Host " 로그: $LogFile" -ForegroundColor Gray
Write-Host "========================================" -ForegroundColor Cyan

# ESP-IDF 환경 로드
. "$PSScriptRoot\env.ps1"

Push-Location $FirmwareDir

try {
    # sdkconfig.defaults에서 노드 타입 설정
    $sdkconfig = Join-Path $FirmwareDir "sdkconfig.defaults"
    $content = Get-Content $sdkconfig -Raw

    if ($NodeType -eq "A") {
        $content = $content -replace "CONFIG_NODE_TYPE_B=y", "CONFIG_NODE_TYPE_A=y"
        $content = $content -replace "# CONFIG_NODE_TYPE_A is not set", "CONFIG_NODE_TYPE_A=y"
    } else {
        $content = $content -replace "CONFIG_NODE_TYPE_A=y", "CONFIG_NODE_TYPE_B=y"
        $content = $content -replace "# CONFIG_NODE_TYPE_B is not set", "CONFIG_NODE_TYPE_B=y"
    }
    Set-Content $sdkconfig $content
    Write-Host "  노드 타입: TYPE_$NodeType 설정 완료" -ForegroundColor Green

    # sdkconfig 캐시 삭제 (노드 타입 변경 반영)
    $sdkconfigCache = Join-Path $FirmwareDir "sdkconfig"
    if (Test-Path $sdkconfigCache) {
        Remove-Item $sdkconfigCache -Force
        Write-Host "  sdkconfig 캐시 삭제" -ForegroundColor Gray
    }

    # 클린 빌드
    if ($Clean) {
        Write-Host "  클린 빌드 수행 중..." -ForegroundColor Yellow
        idf.py fullclean 2>&1 | Tee-Object -FilePath $LogFile -Append
    }

    # 빌드
    Write-Host ""
    Write-Host "  빌드 시작..." -ForegroundColor Yellow
    $buildStart = Get-Date

    idf.py build 2>&1 | Tee-Object -FilePath $LogFile -Append
    $buildResult = $LASTEXITCODE

    $buildEnd = Get-Date
    $buildDuration = ($buildEnd - $buildStart).TotalSeconds

    if ($buildResult -eq 0) {
        Write-Host ""
        Write-Host "  빌드 성공! (${buildDuration}초)" -ForegroundColor Green

        # 빌드 결과 정보
        $binFile = Join-Path $FirmwareDir "build\rbms_firmware.bin"
        if (Test-Path $binFile) {
            $binSize = (Get-Item $binFile).Length
            Write-Host "  바이너리: $binFile ($([math]::Round($binSize/1024, 1)) KB)" -ForegroundColor Gray
        }
    } else {
        Write-Host ""
        Write-Host "  빌드 실패! (exit code: $buildResult)" -ForegroundColor Red
        Write-Host "  로그 확인: $LogFile" -ForegroundColor Yellow
    }
} finally {
    Pop-Location
}

# 결과 반환
exit $buildResult
