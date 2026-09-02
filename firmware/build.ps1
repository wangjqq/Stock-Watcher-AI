# build.ps1 —— 一键构建固件并开启串口监控
#
# 用法（在 firmware 目录下执行）：
#   .\build.ps1                # 构建 + 烧录 + 监控（自动检测串口，未连接则自动重试）
#   .\build.ps1 -Port COM5     # 指定串口
#   .\build.ps1 -NoFlash       # 只构建，不烧录 / 不监控
#   .\build.ps1 -WaitTimeout 120   # 设备串口等待时间（秒，-1 = 无限等待，缺省 60）
#
# 说明：
#   - 自动加载 ESP-IDF 环境（IDF_PATH，缺省 C:\esp\v6.0.2\esp-idf）
#   - 前端产物 firmware/web_dist 缺失时自动构建 web 前端
#   - 首次构建自动 idf.py set-target esp32s3
#   - 构建完成后若未检测到设备串口，会持续自动重试（每 3 秒）直至超时，Ctrl+C 可随时中止

param(
    [string]$Port = "COM5",     # 串口，如 COM5；缺省自动检测
    [switch]$NoFlash,       # 只构建，跳过烧录与监控
    [int]$WaitTimeout = 60, # 设备未连接时自动重试检测的秒数（-1 = 无限等待）
    [int]$BaudRate = 460800 # 烧录波特率（ESP32-S3 原生 USB-Serial-JTAG 不稳定时建议降到 115200）
)

$ErrorActionPreference = "Stop"

# ---------- 1. 加载 ESP-IDF 环境 ----------
$IdfPath = if ($env:IDF_PATH) { $env:IDF_PATH } else { "C:\esp\v6.0.2\esp-idf" }
if (-not (Test-Path "$IdfPath\export.ps1")) {
    Write-Host "[build] 未找到 ESP-IDF：$IdfPath（请先设置 IDF_PATH）" -ForegroundColor Red
    exit 1
}
& "$IdfPath\export.ps1" | Out-Null

$FirmwareDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$RepoDir = Split-Path -Parent $FirmwareDir
Set-Location $FirmwareDir

# ---------- 2. 前端产物缺失时构建 web ----------
$WebDist = "$FirmwareDir\web_dist"
if (-not (Test-Path "$WebDist\index.html")) {
    if (Get-Command node -ErrorAction SilentlyContinue) {
        Write-Host "[build] web_dist 缺失，构建前端..." -ForegroundColor Cyan
        Set-Location "$RepoDir\web"
        if (-not (Test-Path node_modules)) { npm install }
        npm run build
        Set-Location $FirmwareDir
    } else {
        Write-Host "[build] 警告：未安装 Node，跳过前端构建（固件将无内置网页）" -ForegroundColor Yellow
    }
}

# ---------- 3. 设置目标芯片（esp32s3），当前目标不符时执行 ----------
$curTarget = ""
if (Test-Path "$FirmwareDir\sdkconfig") {
    $tline = Select-String -Path "$FirmwareDir\sdkconfig" -Pattern '^CONFIG_IDF_TARGET=' | Select-Object -First 1
    if ($tline) { $curTarget = ($tline.Line -replace '^CONFIG_IDF_TARGET=','').Trim('"') }
}
if ($curTarget -ne "esp32s3") {
    $curDesc = if ($curTarget) { "当前：$curTarget" } else { "未设置" }
    Write-Host "[build] 设置目标 esp32s3（$curDesc）..." -ForegroundColor Cyan
    idf.py set-target esp32s3
    if ($LASTEXITCODE -ne 0) {
        Write-Host "[build] set-target 失败" -ForegroundColor Red
        exit $LASTEXITCODE
    }
}

# ---------- 4. 构建固件 ----------
Write-Host "[build] 构建固件..." -ForegroundColor Cyan
idf.py build
if ($LASTEXITCODE -ne 0) {
    Write-Host "[build] 构建失败" -ForegroundColor Red
    exit $LASTEXITCODE
}

if ($NoFlash) {
    Write-Host "[build] 构建完成（-NoFlash，未烧录）" -ForegroundColor Green
    exit 0
}

# ---------- 5. 检测串口，烧录并监控 ----------
function Find-DevicePort {
    # 排除主板自带 COM1，优先 USB 串口（ESP32-S3 开发板 / CP210x / CH340 等）
    $candidates = @(Get-WmiObject Win32_SerialPort | Where-Object {
        $_.DeviceID -ne 'COM1' -and ($_.Description -match 'USB' -or $_.PNPDeviceID -match 'USB')
    })
    if ($candidates.Count -eq 0) {
        $candidates = @(Get-WmiObject Win32_SerialPort | Where-Object { $_.DeviceID -ne 'COM1' })
    }
    if ($candidates.Count -eq 1) { return $candidates[0].DeviceID }
    return $null
}

if (-not $Port) {
    $Port = Find-DevicePort
    $interval = 3
    $elapsed = 0
    while (-not $Port) {
        if ($WaitTimeout -ge 0 -and $elapsed -ge $WaitTimeout) {
            Write-Host "[build] 等待超时（$WaitTimeout 秒），仍未检测到设备串口" -ForegroundColor Yellow
            $allPorts = @(Get-WmiObject Win32_SerialPort | ForEach-Object { $_.DeviceID })
            Write-Host "        可用串口：$($allPorts -join ' ')"
            Write-Host "        请连接设备后重新运行，或指定 -Port COMx" -ForegroundColor Yellow
            exit 0
        }
        $allPorts = @(Get-WmiObject Win32_SerialPort | ForEach-Object { $_.DeviceID })
        $hint = if ($allPorts.Count) { $allPorts -join ' ' } else { '无' }
        Write-Host "[build] 未检测到设备串口（当前：$hint），${interval}s 后自动重试...（Ctrl+C 可中止）" -ForegroundColor DarkYellow
        Start-Sleep -Seconds $interval
        $elapsed += $interval
        $Port = Find-DevicePort
    }
    Write-Host "[build] 已检测到设备串口：$Port" -ForegroundColor Green
}

Write-Host "[build] 串口 $Port（波特率 $BaudRate），烧录并开启监控..." -ForegroundColor Cyan
idf.py -p $Port -b $BaudRate flash monitor
