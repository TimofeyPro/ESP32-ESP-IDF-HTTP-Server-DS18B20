/*
mail@timofey.pro
https://timofey.pro/esp32
*/
#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "ds18b20.h"

#define delay_tim(x) vTaskDelay((x) / portTICK_PERIOD_MS)
// Temp Sensors are on GPIO26
#define TEMP_BUS 17
#define LED 2
#define HIGH 1
#define LOW 0
#define digitalWrite gpio_set_level

DeviceAddress tempSensors[2];

void getTempAddresses(DeviceAddress *tempSensorAddresses) {
		unsigned int numberFound = 0;
		reset_search();
		// search for 2 addresses on the oneWire protocol
		while (search(tempSensorAddresses[numberFound],true)) {
				numberFound++;
				if (numberFound == 2) break;
		}
		// if 2 addresses aren't found then flash the LED rapidly
		while (numberFound != 2) {
				numberFound = 0;
				digitalWrite(LED, HIGH);
				delay_tim(100);
				digitalWrite(LED, LOW);
				delay_tim(100);
				// search in the loop for the temp sensors as they may hook them up
				reset_search();
				while (search(tempSensorAddresses[numberFound],true)) {
					numberFound++;
					if (numberFound == 2) break;
				}
		}
		return;
}

char html_page1[] = "<!DOCTYPE HTML><html>\n"
                   "<head>\n"
                   "  <title>ESP-IDF BME680 Web Server</title>\n"
                   "  <meta http-equiv=\"refresh\" content=\"20\">\n"
                   "  <meta name=\"viewport\" content=\"width=device-width, initial-scale=1\">\n"
                   "  <style>\n"
                   "    html {font-family: Arial; display: inline-block; text-align: center;}\n"
									 "    p { font-size: 1.0rem; text-align: left; padding-left: 5px;}\n"
                   "    h4 { font-size: 1.2rem; text-align: center;}\n"
                   "    body {  margin: 0;}\n"
                   "    .topnav { overflow: hidden; background-color: #4B1D3F; color: white; font-size: 1.7rem; }\n"
                   "    .content { padding: 20px; }\n"
                   "    .card { background-color: white; box-shadow: 2px 2px 12px 1px rgba(140,140,140,.5); }\n"
                   "    .cards { max-width: 700px; margin: 0 auto; display: grid; grid-gap: 2rem; grid-template-columns: repeat(auto-fit, minmax(300px, 1fr)); }\n"
                   "    .reading { font-size: 1.1rem;  }\n"
									 "    .reading2 { font-size: 0.9rem;  }\n"
                   "    .card.temperature { color: #0e7c7b; }\n"
                   "    .card.humidity { color: #17bebb; }\n"
                   "    .card.pressure { color: #3fca6b; }\n"
                   "    .card.gas { color: #d62246; }\n"
                   "  </style>\n"
                   "</head>\n"
                   "<body>\n"
                   "  <div class=\"topnav\">\n"
                   "    <h3>IoT online data</h3>\n"
                   "  </div>\n";

char html_page2[] = "  <p>MAC address: %02X:%02X:%02X:%02X:%02X:%02X</p>\n";
//char html_page3[] = "  <p>IP address: </p>\n";
char html_page4[] = "  <p>System time: %s</p>\n";

char html_page5[] = "  <div class=\"content\">\n"
                   "    <div class=\"cards\">\n"
                   "      <div class=\"card temperature\">\n"
                   "        <h4>Temperature</h4>\n"
									 " 				<p><span class=\"reading\">Sensor 01: <b>%.2f &deg;C</b></span>\n"
									 " 				<br><span class=\"reading2\">0x%02x 0x%02x 0x%02x 0x%02x 0x%02x 0x%02x 0x%02x 0x%02x</span></p>\n"
									 " 				<p><span class=\"reading\">Sensor 02: <b>%.2f &deg;C</b></span>\n"
									 " 				<br><span class=\"reading2\">0x%02x 0x%02x 0x%02x 0x%02x 0x%02x 0x%02x 0x%02x 0x%02x</span></p>\n"
                   "      </div>\n";

char html_page6[] = "   </div>\n"
                   "  </div>\n"
                   "</body>\n"
                   "</html>";
