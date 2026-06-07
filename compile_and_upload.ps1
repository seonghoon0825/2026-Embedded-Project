$cli = "$env:USERPROFILE\.arduino-cli\arduino-cli.exe"
$sketch = "C:\Users\Jeongseonghun\Desktop\2026-Embedded-Project\SmartTrashCan"

Write-Output "=== Smart Trash Can - Compile & Upload ==="

# Compile
Write-Output "`n>>> Compiling for ESP32..."
& $cli compile --fqbn esp32:esp32:heltec_wifi_lora_32_V3 $sketch 2>&1

if ($LASTEXITCODE -ne 0) {
    Write-Output "`n>>> Compilation FAILED!"
    exit 1
}

Write-Output "`n>>> Compilation SUCCESS!"

# Check for connected board
Write-Output "`n>>> Checking for connected ESP32 board..."
$boardList = & $cli board list --format json 2>$null | ConvertFrom-Json

$esp32Port = $null
foreach ($b in $boardList) {
    if ($b.port.protocol -eq "serial") {
        $esp32Port = $b.port.address
        break
    }
}

if ($esp32Port) {
    Write-Output ">>> Found board on port: $esp32Port"
    Write-Output ">>> Uploading..."
    & $cli upload --fqbn esp32:esp32:heltec_wifi_lora_32_V3 --port $esp32Port $sketch 2>&1
    if ($LASTEXITCODE -eq 0) {
        Write-Output "`n>>> Upload SUCCESS!"
    } else {
        Write-Output "`n>>> Upload FAILED! Check the connection."
    }
} else {
    Write-Output ">>> No ESP32 board connected via USB."
    Write-Output ">>> Please connect your ESP32 and run this script again."
    Write-Output ">>> Or upload manually: arduino-cli upload --fqbn esp32:esp32:heltec_wifi_lora_32_V3 --port COMx $sketch"
}
