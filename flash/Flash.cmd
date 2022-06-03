@echo off
echo.
echo Available COM ports: 
echo.
wmic path win32_pnpentity get caption /format:table| find "COM" 
echo.
SET /P portNr="Which COM port should be used? Please enter the number: "
echo.
SET COMport=COM%PortNr%
echo Using %COMport% ...
echo.
esptool.py --chip esp32 --port %COMport% --baud 460800 --before default_reset --after hard_reset write_flash -z --flash_mode dio --flash_freq 40m --flash_size detect 0x1000 BinFiles\bootloader_dio_40m.bin 0x8000 BinFiles\partitions.bin 0xe000 BinFiles\boot_app0.bin 0x10000 BinFiles\firmware.bin
pause