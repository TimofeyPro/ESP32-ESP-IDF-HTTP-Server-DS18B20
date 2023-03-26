# ESP32 Web Server (ESP-IDF v5.0.1) displaying temperature data from two DS18B20 sensors.
# ESP32 Web Server (ESP-IDF v5.0.1), отражающий данные о температуре с двух датчиков DS18B20.

The Espressif library https://github.com/espressif/esp-idf/tree/master/examples/peripherals/rmt/onewire_ds18b20 is not stable yet:
https://github.com/espressif/esp-idf/issues/10790 (Example Onewire DS18B20 does not work). Therefore, a solution re DS18B20 Sensors
is taken from feelfreelinux: https://github.com/feelfreelinux/ds18b20

Библиотека Espressif https://github.com/espressif/esp-idf/tree/master/examples/peripherals/rmt/onewire_ds18b20 стабильно пока не работает:
https://github.com/espressif/esp-idf/issues/10790 (Example Onewire DS18B20 does not work). Поэтому решение в части получения данных с датчика взято у feelfreelinux: https://github.com/feelfreelinux/ds18b20

Temperature readings from two sensors + their 64-bit serial number, which is used to address sensors on the 1-Wire bus, are displayed on the mobile phone screen at the address “http://yourIP/sensor” as well as MAC address of the controller and the system time of the controller (the time is displayed at the time the browser was updated).
This solution synchronizes the system time of the controller with the SNTP (Simple Network Time Protocol) server [pool.ntp.org] via WiFI using the standard example: using "LwIP SNTP module and time functions".

## Структура / Project structure

    ```
    ├── CMakeLists.txt
    ├── main
    │   ├── CMakeLists.txt
    │   ├── Kconfig.projbuild
    │   ├── main.c
    ├── components
    │   ├── CMakeLists.txt    
    │   ├── dallas.c
    │   ├── ds18b20.c  
    │   └── include  
    │         └── ds18b20.h
    └── README.md                  This is the file you are currently reading
    ```

### Required actions
```
mkdir -p ~/esp
cd ~/esp
git clone --recursive https://github.com/espressif/esp-idf.git
git clone --recursive https://github.com/TimofeyPro/ESP32-ESP-IDF-HTTP-Server-DS18B20.git
. $HOME/esp/esp-idf/export.sh
idf.py menuconfig
idf.py build
sudo chmod a+rw /dev/ttyUSB0
idf.py -p /dev/ttyUSB0 flash monitorg
```
* Open the project configuration menu (`idf.py menuconfig`) to configure Wi-Fi.
* Check the data on mibile at http://yourIP/sensor

## Пояснения / a few tips
Про "REQUIRES esp_timer": https://esp32developer.com/programming-in-c-c/compilers-and-ides/esp-idf/compiler/requires-list

## Troubleshooting
* `idf.py menuconfig` → Component config → HTTP Server → (2048) Max HTTP Request Header Length
