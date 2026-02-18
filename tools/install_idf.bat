@echo off
set MSYSTEM=
set IDF_PATH=C:\esp\esp-idf
cd /d %IDF_PATH%

echo === Installing ESP-IDF Python Environment ===
C:\Users\home\AppData\Local\Programs\Python\Python312\python.exe tools\idf_tools.py install-python-env
if errorlevel 1 (
    echo ERROR: Python env install failed
    exit /b 1
)

echo === Installing ESP-IDF Tools for ESP32-C6 ===
cd /d C:\esp\esp-idf
call C:\esp\esp-idf\install.bat esp32c6
if errorlevel 1 (
    echo ERROR: Tools install failed
    exit /b 1
)

echo === Verifying installation ===
cd /d C:\esp\esp-idf
call C:\esp\esp-idf\export.bat
idf.py --version
cmake --version
ninja --version

echo === ESP-IDF Installation Complete ===
