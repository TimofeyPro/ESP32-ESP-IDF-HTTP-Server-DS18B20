# ESP32 Web Server (ESP-IDF) displaying temperature data from two DS18B20 sensors.
# ESP32 Web Server (ESP-IDF), отражающий данные о температуре с двух датчиков DS18B20.

The Espressif library https://github.com/espressif/esp-idf/tree/master/examples/peripherals/rmt/onewire_ds18b20 is not stable yet:
https://github.com/espressif/esp-idf/issues/10790 (Example Onewire DS18B20 does not work). Therefore, a solution re DS18B20 Sensors
is taken from feelfreelinux: https://github.com/feelfreelinux/ds18b20

Библиотека Espressif https://github.com/espressif/esp-idf/tree/master/examples/peripherals/rmt/onewire_ds18b20 стабильно пока не работает:
https://github.com/espressif/esp-idf/issues/10790 (Example Onewire DS18B20 does not work). Поэтому решение в части получения данных с датчика взято у feelfreelinux: https://github.com/feelfreelinux/ds18b20


## Структура / Project structure

    ```
    ├── CMakeLists.txt
    ├── main
    │   ├── CMakeLists.txt
    │   ├── main.c
    ├── components
    │   ├── CMakeLists.txt    
    │   ├── dallas.c
    │   ├── ds18b20.c  
    │   └── include  
    │         ├── CMakeLists.txt  
    │         └── ds18b20.h
    └── README.md                  This is the file you are currently reading
    ```

## Пояснения / a few tips

Про "REQUIRES esp_timer": https://esp32developer.com/programming-in-c-c/compilers-and-ides/esp-idf/compiler/requires-list

### Configure the project

```
idf.py menuconfig
```
* Open the project configuration menu (`idf.py menuconfig`) to configure Wi-Fi.


```

## Troubleshooting
* `idf.py menuconfig` → Component config → HTTP Server → (2048) Max HTTP Request Header Length
