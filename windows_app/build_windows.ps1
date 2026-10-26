$ErrorActionPreference = 'Stop'
Push-Location $PSScriptRoot
try {
    if (-not (Test-Path -LiteralPath '.venv\Scripts\python.exe')) {
        python -m venv .venv
        if ($LASTEXITCODE -ne 0) { throw 'Не удалось создать Python-окружение.' }
    }
    & .venv\Scripts\python.exe -m pip install -r requirements.txt
    if ($LASTEXITCODE -ne 0) { throw 'Не удалось установить зависимости.' }
    & .venv\Scripts\python.exe -m unittest test_protocol -v
    if ($LASTEXITCODE -ne 0) { throw 'Тесты не прошли.' }
    & .venv\Scripts\python.exe -m PyInstaller --noconfirm --onefile --windowed --name ESP32-Network app.py
    if ($LASTEXITCODE -ne 0) { throw 'Не удалось собрать exe.' }
    Write-Host 'Готово: windows_app\dist\ESP32-Network.exe'
} finally {
    Pop-Location
}
