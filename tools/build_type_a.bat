@echo off
set MSYSTEM=
set IDF_PATH=C:\esp\esp-idf
set LOG_FILE=C:\Users\home\ESP32-c6\tools\build_type_a_log.txt

echo === ESP-IDF Build Type A === > %LOG_FILE%
echo %date% %time% >> %LOG_FILE%

cd /d C:\esp\esp-idf
call C:\esp\esp-idf\export.bat >> %LOG_FILE% 2>&1

echo. >> %LOG_FILE%
echo === Cleaning previous build === >> %LOG_FILE%
cd /d C:\Users\home\ESP32-c6\firmware
if exist build rmdir /s /q build >> %LOG_FILE% 2>&1
if exist sdkconfig del sdkconfig >> %LOG_FILE% 2>&1

REM Create sdkconfig.defaults.type_a with NODE_TYPE_A
echo CONFIG_NODE_TYPE_A=y > sdkconfig.defaults.type_a
echo CONFIG_NODE_TYPE_B=n >> sdkconfig.defaults.type_a

echo. >> %LOG_FILE%
echo === Setting Target ESP32-C6 === >> %LOG_FILE%
call idf.py -DSDKCONFIG_DEFAULTS="sdkconfig.defaults;sdkconfig.defaults.type_a" set-target esp32c6 >> %LOG_FILE% 2>&1
if errorlevel 1 (
    echo ERROR: set-target failed >> %LOG_FILE%
    echo ERROR: set-target failed
    exit /b 1
)

echo. >> %LOG_FILE%
echo === Building Firmware (Type A) === >> %LOG_FILE%
call idf.py build >> %LOG_FILE% 2>&1
if errorlevel 1 (
    echo ERROR: build failed >> %LOG_FILE%
    echo BUILD FAILED
    exit /b 1
)

echo. >> %LOG_FILE%
echo === Build Complete (Type A) === >> %LOG_FILE%
echo BUILD SUCCESS - TYPE A
exit /b 0
