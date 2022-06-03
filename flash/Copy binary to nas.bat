@echo off
xcopy "..\.pio\build\esp32dev\firmware.bin" "\\10.6.1.130\raspberrypi3_nas\Projects\ESP\CryptoPaper\BinFiles\" /v /f /y
xcopy "..\.pio\build\esp32dev\partitions.bin" "\\10.6.1.130\raspberrypi3_nas\Projects\ESP\CryptoPaper\BinFiles\" /v /f /y
xcopy "%userprofile%\.platformio\packages\framework-arduinoespressif32\tools\sdk\esp32\bin\bootloader_dio_40m.bin" "\\10.6.1.130\raspberrypi3_nas\Projects\ESP\CryptoPaper\BinFiles\" /v /f /y
xcopy "%userprofile%\.platformio\packages\framework-arduinoespressif32\tools\partitions\boot_app0.bin" "\\10.6.1.130\raspberrypi3_nas\Projects\ESP\CryptoPaper\BinFiles\" /v /f /y
pause