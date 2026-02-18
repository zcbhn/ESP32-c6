#Requires -Version 5.1
<#
.SYNOPSIS
    RBMS 풀 자동화 파이프라인: 빌드 → 플래시 → 모니터 → 검증
.DESCRIPTION
    하나의 명령으로 펌웨어 빌드, 플래시, 시리얼 모니터링, 동작 검증을
    모두 수행합니다. Claude가 직접 실행하고 결과를 확인할 수 있습니다.
.PARAMETER NodeType
    노드 타입: A 또는 B (기본: B)
.PARAMETER Port
    COM 포트 (기본: COM3)
.PARAMETER MonitorDuration
    모니터링 시간(초) (기본: 30)
.PARAMETER SkipBuild
    빌드 단계 건너뛰기
.PARAMETER SkipFlash
    플래시 단계 건너뛰기
.PARAMETER Check
    검증 항목 (기본: all)
.EXAMPLE
    .\tools\auto.ps1                                    # 기본 실행
    .\tools\auto.ps1 -NodeType A -Port COM3             # Type A 풀 노드
    .\tools\auto.ps1 -SkipBuild -SkipFlash              # 모니터+검증만
    .\tools\auto.ps1 -MonitorDuration 60 -Check boot    # 60초 모니터, 부팅만 검증
#>

param(
    [ValidateSet("A", "B")]
    [string]$NodeType = "B",
    [string]$Port = "COM3",
    [int]$MonitorDuration = 30,
    [switch]$SkipBuild,
    [switch]$SkipFlash,
    [switch]$Clean,
    [string]$Check = "all"
)

$ErrorActionPreference = "Stop"
$ProjectRoot = Split-Path $PSScriptRoot -Parent
$LogDir = Join-Path $ProjectRoot "tools\logs"
$Timestamp = Get-Date -Format "yyyyMMdd_HHmmss"
$PipelineLog = Join-Path $LogDir "pipeline_${Timestamp}.log"

if (-not (Test-Path $LogDir)) {
    New-Item -ItemType Directory -Path $LogDir -Force | Out-Null
}

function Log($msg, $color = "White") {
    $ts = Get-Date -Format "HH:mm:ss"
    $line = "[$ts] $msg"
    Write-Host $line -ForegroundColor $color
    Add-Content -Path $PipelineLog -Value $line
}

Write-Host ""
Write-Host "  ============================================" -ForegroundColor Cyan
Write-Host "   RBMS 풀 자동화 파이프라인" -ForegroundColor Cyan
Write-Host "   Node: Type $NodeType | Port: $Port" -ForegroundColor Gray
Write-Host "   Monitor: ${MonitorDuration}s | Check: $Check" -ForegroundColor Gray
Write-Host "  ============================================" -ForegroundColor Cyan
Write-Host ""

$pipelineStart = Get-Date
$stepResults = @{}

# ==========================================
# STEP 1: 빌드
# ==========================================
if (-not $SkipBuild) {
    Log "=== STEP 1/4: 빌드 ===" "Yellow"

    $buildArgs = @("-NodeType", $NodeType)
    if ($Clean) { $buildArgs += "-Clean" }

    try {
        & "$PSScriptRoot\build.ps1" @buildArgs
        $stepResults["build"] = $LASTEXITCODE -eq 0
        if ($stepResults["build"]) {
            Log "빌드 성공" "Green"
        } else {
            Log "빌드 실패 — 파이프라인 중단" "Red"
            exit 1
        }
    } catch {
        Log "빌드 에러: $_" "Red"
        $stepResults["build"] = $false
        exit 1
    }
} else {
    Log "=== STEP 1/4: 빌드 (건너뜀) ===" "Gray"
    $stepResults["build"] = "skipped"
}

# ==========================================
# STEP 2: 플래시
# ==========================================
if (-not $SkipFlash) {
    Log "=== STEP 2/4: 플래시 ===" "Yellow"

    try {
        & "$PSScriptRoot\flash.ps1" -Port $Port
        $stepResults["flash"] = $LASTEXITCODE -eq 0
        if ($stepResults["flash"]) {
            Log "플래시 성공" "Green"
        } else {
            Log "플래시 실패 — 파이프라인 중단" "Red"
            exit 1
        }
    } catch {
        Log "플래시 에러: $_" "Red"
        $stepResults["flash"] = $false
        exit 1
    }
} else {
    Log "=== STEP 2/4: 플래시 (건너뜀) ===" "Gray"
    $stepResults["flash"] = "skipped"
}

# ==========================================
# STEP 3: 모니터
# ==========================================
Log "=== STEP 3/4: 시리얼 모니터 (${MonitorDuration}초) ===" "Yellow"

$monitorLog = Join-Path $LogDir "monitor_${Timestamp}.log"
$monitorArgs = @(
    "$PSScriptRoot\monitor.py",
    "--port", $Port,
    "--duration", $MonitorDuration,
    "--output", $monitorLog,
    "--reset"
)

try {
    python @monitorArgs
    $stepResults["monitor"] = $LASTEXITCODE -eq 0
    if ($stepResults["monitor"]) {
        Log "모니터 캡처 완료: $monitorLog" "Green"
    } else {
        Log "모니터 캡처 실패 (데이터 없음)" "Yellow"
    }
} catch {
    Log "모니터 에러: $_" "Red"
    $stepResults["monitor"] = $false
}

# ==========================================
# STEP 4: 검증
# ==========================================
Log "=== STEP 4/4: 동작 검증 ===" "Yellow"

$validateArgs = @(
    "$PSScriptRoot\validate.py",
    "--log", $monitorLog,
    "--check", $Check,
    "--save"
)

try {
    python @validateArgs
    $validateResult = $LASTEXITCODE
    $stepResults["validate"] = $validateResult -eq 0
} catch {
    Log "검증 에러: $_" "Red"
    $stepResults["validate"] = $false
}

# ==========================================
# 파이프라인 요약
# ==========================================
$pipelineEnd = Get-Date
$totalDuration = ($pipelineEnd - $pipelineStart).TotalSeconds

Write-Host ""
Write-Host "  ============================================" -ForegroundColor Cyan
Write-Host "   파이프라인 완료 (${totalDuration}초)" -ForegroundColor Cyan
Write-Host "  ============================================" -ForegroundColor Cyan

foreach ($step in @("build", "flash", "monitor", "validate")) {
    $result = $stepResults[$step]
    if ($result -eq "skipped") {
        $icon = " SKIP"; $color = "Gray"
    } elseif ($result -eq $true) {
        $icon = " PASS"; $color = "Green"
    } else {
        $icon = " FAIL"; $color = "Red"
    }
    Write-Host "    $icon  $step" -ForegroundColor $color
}

Write-Host ""
Write-Host "  로그 디렉토리: $LogDir" -ForegroundColor Gray
Write-Host "  모니터 로그: $monitorLog" -ForegroundColor Gray
Write-Host "  파이프라인 로그: $PipelineLog" -ForegroundColor Gray
Write-Host ""

# 최종 결과
$allPassed = $true
foreach ($r in $stepResults.Values) {
    if ($r -eq $false) { $allPassed = $false; break }
}

if ($allPassed) {
    Log "파이프라인 성공" "Green"
    exit 0
} else {
    Log "파이프라인 일부 실패" "Red"
    exit 1
}
