/*
mail@timofey.pro
https://timofey.pro/esp32

*/
#include <esp_wifi.h>
#include <esp_event.h>
#include <esp_log.h>
#include <esp_system.h>
#include <nvs_flash.h>   //Non-Volatile Storage
#include <sys/param.h>
#include "nvs_flash.h"   //Non-Volatile Storage
#include "esp_netif.h"
#include "esp_eth.h"
#include "protocol_examples_common.h"
#include "protocol_examples_utils.h"
#include "esp_tls_crypto.h"
#include <esp_http_server.h>

#include "dallas.c"
#include "ds18b20.c"

#include <stdio.h>
#include <inttypes.h>
#include "sdkconfig.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_chip_info.h"
#include "esp_flash.h"

#include <time.h>
#include <sys/time.h>
#include <string.h>
#include "freertos/event_groups.h"
#include "esp_attr.h"
#include "esp_sleep.h"
#include "esp_sntp.h"
#include "esp_attr.h"
#include "esp_netif_sntp.h"
#include "lwip/ip_addr.h"


#define EXAMPLE_HTTP_QUERY_KEY_MAX_LEN  (64)
static const char *TAG = "tim-info==>";
/* Variable holding number of times ESP32 restarted since first boot.
 * It is placed into RTC memory using RTC_DATA_ATTR and
 * maintains its value when ESP32 wakes from deep sleep. */
RTC_DATA_ATTR static int boot_count = 0;
/* define a constant MAC_ADDR_SIZE as 6, which is the size of a MAC address in bytes. */
#define MAC_ADDR_SIZE 6

/*SNTP (Simple Network Time Protocol) синхронизация
---------------------------------------------------------------------------------------------*/
#ifndef INET6_ADDRSTRLEN
#define INET6_ADDRSTRLEN 48
#endif
static void obtain_time(void);
#ifdef CONFIG_SNTP_TIME_SYNC_METHOD_CUSTOM
void sntp_sync_time(struct timeval *tv)
{
   settimeofday(tv, NULL);
   ESP_LOGI(TAG, "Time is synchronized from custom code");
   sntp_set_sync_status(SNTP_SYNC_STATUS_COMPLETED);
}
#endif

void time_sync_notification_cb(struct timeval *tv)
{
    ESP_LOGI(TAG, "Notification of a time synchronization event");
}
/*
---------------------------------------------------------------------------------------------*/
#if CONFIG_EXAMPLE_BASIC_AUTH

typedef struct {
    char    *username;
    char    *password;
} basic_auth_info_t;

#define HTTPD_401      "401 UNAUTHORIZED"           /*!< HTTP Response 401 */


static char *http_auth_basic(const char *username, const char *password)
{
    int out;
    char *user_info = NULL;
    char *digest = NULL;
    size_t n = 0;
    asprintf(&user_info, "%s:%s", username, password);
    if (!user_info) {
        ESP_LOGE(TAG, "No enough memory for user information");
        return NULL;
    }
    esp_crypto_base64_encode(NULL, 0, &n, (const unsigned char *)user_info, strlen(user_info));

    /* 6: The length of the "Basic " string
     * n: Number of bytes for a base64 encode format
     * 1: Number of bytes for a reserved which be used to fill zero
    */
    digest = calloc(1, 6 + n + 1);
    if (digest) {
        strcpy(digest, "Basic ");
        esp_crypto_base64_encode((unsigned char *)digest + 6, n, (size_t *)&out, (const unsigned char *)user_info, strlen(user_info));
    }
    free(user_info);
    return digest;
}

/* An HTTP GET handler */
static esp_err_t basic_auth_get_handler(httpd_req_t *req)
{
    char *buf = NULL;
    size_t buf_len = 0;
    basic_auth_info_t *basic_auth_info = req->user_ctx;

    buf_len = httpd_req_get_hdr_value_len(req, "Authorization") + 1;
    if (buf_len > 1) {
        buf = calloc(1, buf_len);
        if (!buf) {
            ESP_LOGE(TAG, "No enough memory for basic authorization");
            return ESP_ERR_NO_MEM;
        }

        if (httpd_req_get_hdr_value_str(req, "Authorization", buf, buf_len) == ESP_OK) {
            ESP_LOGI(TAG, "Found header => Authorization: %s", buf);
        } else {
            ESP_LOGE(TAG, "No auth value received");
        }

        char *auth_credentials = http_auth_basic(basic_auth_info->username, basic_auth_info->password);
        if (!auth_credentials) {
            ESP_LOGE(TAG, "No enough memory for basic authorization credentials");
            free(buf);
            return ESP_ERR_NO_MEM;
        }

        if (strncmp(auth_credentials, buf, buf_len)) {
            ESP_LOGE(TAG, "Not authenticated");
            httpd_resp_set_status(req, HTTPD_401);
            httpd_resp_set_type(req, "application/json");
            httpd_resp_set_hdr(req, "Connection", "keep-alive");
            httpd_resp_set_hdr(req, "WWW-Authenticate", "Basic realm=\"Hello\"");
            httpd_resp_send(req, NULL, 0);
        } else {
            ESP_LOGI(TAG, "Authenticated!");
            char *basic_auth_resp = NULL;
            httpd_resp_set_status(req, HTTPD_200);
            httpd_resp_set_type(req, "application/json");
            httpd_resp_set_hdr(req, "Connection", "keep-alive");
            asprintf(&basic_auth_resp, "{\"authenticated\": true,\"user\": \"%s\"}", basic_auth_info->username);
            if (!basic_auth_resp) {
                ESP_LOGE(TAG, "No enough memory for basic authorization response");
                free(auth_credentials);
                free(buf);
                return ESP_ERR_NO_MEM;
            }
            httpd_resp_send(req, basic_auth_resp, strlen(basic_auth_resp));
            free(basic_auth_resp);
        }
        free(auth_credentials);
        free(buf);
    } else {
        ESP_LOGE(TAG, "No auth header received");
        httpd_resp_set_status(req, HTTPD_401);
        httpd_resp_set_type(req, "application/json");
        httpd_resp_set_hdr(req, "Connection", "keep-alive");
        httpd_resp_set_hdr(req, "WWW-Authenticate", "Basic realm=\"Hello\"");
        httpd_resp_send(req, NULL, 0);
    }

    return ESP_OK;
}

static httpd_uri_t basic_auth = {
    .uri       = "/basic_auth",
    .method    = HTTP_GET,
    .handler   = basic_auth_get_handler,
};

static void httpd_register_basic_auth(httpd_handle_t server)
{
    basic_auth_info_t *basic_auth_info = calloc(1, sizeof(basic_auth_info_t));
    if (basic_auth_info) {
        basic_auth_info->username = CONFIG_EXAMPLE_BASIC_AUTH_USERNAME;
        basic_auth_info->password = CONFIG_EXAMPLE_BASIC_AUTH_PASSWORD;

        basic_auth.user_ctx = basic_auth_info;
        httpd_register_uri_handler(server, &basic_auth);
    }
}
#endif

esp_err_t send_web_page01_mac_address(httpd_req_t *req){
     int response;
     uint8_t mac[MAC_ADDR_SIZE];
     esp_wifi_get_mac(ESP_IF_WIFI_STA, mac);
     //ESP_LOGI("MAC address", "MAC address: %02x:%02x:%02x:%02x:%02x:%02x", mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
		 char response_data[sizeof(html_page2) + 7];
		 memset(response_data, 0, sizeof(response_data));
		 sprintf(response_data, html_page2, mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
     response = httpd_resp_send_chunk(req, response_data, HTTPD_RESP_USE_STRLEN);
     return response;
}

esp_err_t send_web_page4(httpd_req_t *req){
     int response;

    time_t now;
   	char strftime_buf[64];
   	struct tm timeinfo;
   	time(&now);
   	// Set timezone to China Standard Time
   	//setenv("TZ", "CST-8", 1);
   	tzset();
   	localtime_r(&now, &timeinfo);
   	strftime(strftime_buf, sizeof(strftime_buf), "%c", &timeinfo);
   	ESP_LOGI(TAG, "The current ESP32-DevKitC V4: ESP32-WROOM-32E system date/time is: %s", strftime_buf);
   	printf("------------------------------\n");

     char response_data[sizeof(html_page5) + 50];
     memset(response_data, 0, sizeof(response_data));
     sprintf(response_data, html_page4, strftime_buf);
     response = httpd_resp_send_chunk(req, response_data, HTTPD_RESP_USE_STRLEN);
     return response;
}

esp_err_t send_web_page5(httpd_req_t *req){
     int response;
     ds18b20_requestTemperatures();
     float temp30 = ds18b20_getTempC((DeviceAddress *)tempSensors[0]);
     float temp40 = ds18b20_getTempC((DeviceAddress *)tempSensors[1]);
       if (temp30 > 110 || temp30 < -50) {
         printf("Error: %0.2fC on Sensor№1. Requested new temperature data.\n",  temp30);
         delay_tim(100);
         ds18b20_requestTemperatures();
         temp30 = ds18b20_getTempC((DeviceAddress *)tempSensors[0]);
         }
       if (temp40 > 110 || temp40 < -50) {
         printf("Error: %0.2fC on Sensor№2. Requested new temperature data.\n",  temp40);
         delay_tim(100);
         ds18b20_requestTemperatures();
         temp40 = ds18b20_getTempC((DeviceAddress *)tempSensors[1]);
       }
     printf("Temperatures on mobile screen: %0.2fC %0.2fC\n",  temp30, temp40);
     char response_data[sizeof(html_page5) + 25];
     memset(response_data, 0, sizeof(response_data));
     sprintf(response_data, html_page5,  temp30, tempSensors[0][0],tempSensors[0][1],tempSensors[0][2],tempSensors[0][3],tempSensors[0][4],tempSensors[0][5],tempSensors[0][6],tempSensors[0][7], temp40,tempSensors[1][0],tempSensors[1][1],tempSensors[1][2],tempSensors[1][3],tempSensors[1][4],tempSensors[1][5],tempSensors[1][6],tempSensors[1][7]);
     response = httpd_resp_send_chunk(req, response_data, HTTPD_RESP_USE_STRLEN);
     return response;
}

esp_err_t sensor_get_handler(httpd_req_t *req)
{
    char*  buf;
    size_t buf_len;
    /* Get header value string length and allocate memory for length + 1,
     * extra byte for null termination */
    buf_len = httpd_req_get_hdr_value_len(req, "Host") + 1;


    if (buf_len > 1) {
        buf = malloc(buf_len);
        /* Скопируем заголовок "Host" в буфер и выведем его в лог, затем освободим память, запрошенную под этот буфер */
        if (httpd_req_get_hdr_value_str(req, "Host", buf, buf_len) == ESP_OK) {
            ESP_LOGI(TAG, "Found header => Host: %s", buf);
            // Выведем длину заголовка "Host" на экран c учетом "extra byte for null termination"
            printf("«Host» header length: %d\n", buf_len);
        }
        free(buf);
    }

    httpd_resp_set_type(req, "text/html");
    httpd_resp_send_chunk(req, html_page1, HTTPD_RESP_USE_STRLEN);
    send_web_page01_mac_address(req);
    //httpd_resp_send_chunk(req, html_page3, HTTPD_RESP_USE_STRLEN);
    //httpd_resp_send_chunk(req, html_page4, HTTPD_RESP_USE_STRLEN);
    send_web_page4(req);

    send_web_page5(req);
    httpd_resp_send_chunk(req, html_page6, HTTPD_RESP_USE_STRLEN);
    httpd_resp_send_chunk(req, NULL, 0);

    /* After sending the HTTP response the old HTTP request
     * headers are lost. Check if HTTP request headers can be read now. */
    if (httpd_req_get_hdr_value_len(req, "Host") == 0) {
        ESP_LOGI(TAG, "Request headers lost");
    }

    return ESP_OK;
}

static const httpd_uri_t sensor = {
    .uri       = "/sensor",
    .method    = HTTP_GET,
    .handler   = sensor_get_handler,
    /* Let's pass response string in user
     * context to demonstrate it's usage */
    .user_ctx  = NULL
};

static httpd_handle_t start_webserver(void)
{
    httpd_handle_t server = NULL;
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.lru_purge_enable = true;

    // Start the httpd server
    ESP_LOGI(TAG, "Starting server on port: '%d'", config.server_port);
    if (httpd_start(&server, &config) == ESP_OK) {
        // Set URI handlers
        ESP_LOGI(TAG, "Registering URI handlers");

        httpd_register_uri_handler(server, &sensor);

        //httpd_register_uri_handler(server, &echo);
        //httpd_register_uri_handler(server, &ctrl);
        #if CONFIG_EXAMPLE_BASIC_AUTH
        httpd_register_basic_auth(server);
        #endif
        return server;
    }

    ESP_LOGI(TAG, "Error starting server!");
    return NULL;
}

static esp_err_t stop_webserver(httpd_handle_t server)
{
    // Stop the httpd server
    return httpd_stop(server);
}

static void disconnect_handler(void* arg, esp_event_base_t event_base,
                               int32_t event_id, void* event_data)
{
    httpd_handle_t* server = (httpd_handle_t*) arg;
    if (*server) {
        ESP_LOGI(TAG, "Stopping webserver");
        if (stop_webserver(*server) == ESP_OK) {
            *server = NULL;
        } else {
            ESP_LOGE(TAG, "Failed to stop http server");
        }
    }
}

static void connect_handler(void* arg, esp_event_base_t event_base,
                            int32_t event_id, void* event_data)
{
    httpd_handle_t* server = (httpd_handle_t*) arg;
    if (*server == NULL) {
        ESP_LOGI(TAG, "Starting webserver");
        *server = start_webserver();
    }
}


void app_main(void)
{
  ++boot_count;
  ESP_LOGI(TAG, "Boot count: %d", boot_count);
  printf("--------------------------------------------------------------------------\n");
  esp_chip_info_t chip_info;
  uint32_t flash_size;
  esp_chip_info(&chip_info);
  printf("This is %s chip with %d CPU core(s), WiFi%s%s%s, ",
         CONFIG_IDF_TARGET,
         chip_info.cores,
         (chip_info.features & CHIP_FEATURE_BT) ? "/BT" : "",
         (chip_info.features & CHIP_FEATURE_BLE) ? "/BLE" : "",
         (chip_info.features & CHIP_FEATURE_IEEE802154) ? ", 802.15.4 (Zigbee/Thread)" : "");

  unsigned major_rev = chip_info.revision / 100;
  unsigned minor_rev = chip_info.revision % 100;
  printf("silicon revision v%d.%d, ", major_rev, minor_rev);
  if(esp_flash_get_size(NULL, &flash_size) != ESP_OK) {
      printf("Get flash size failed");
      return;
  }

  printf("%" PRIu32 "MB %s flash\n", flash_size / (uint32_t)(1024 * 1024),
         (chip_info.features & CHIP_FEATURE_EMB_FLASH) ? "embedded" : "external");

  printf("Minimum free heap size: %" PRIu32 " bytes\n", esp_get_minimum_free_heap_size());
  printf("--------------------------------------------------------------------------\n");
  /* ----------------------------------------------------------------------------------- */


  static httpd_handle_t server = NULL;
  /* для хранения настроек WiFi запускаем работу библиотеки «nvs_flash.h» по работе с
     знергонезависимым хранилищем данных в памяти контроллера (Non-Volatile Storage, NVS)  */
  ESP_ERROR_CHECK(nvs_flash_init());
  /* запускаем библиотеку ESP-NETIF, использующую lwIP (Lightweight TCP/IP stack).   */
  ESP_ERROR_CHECK(esp_netif_init());
  /* запускаем Event Loop Library, которая позволяет компонентам объявлять события (declare events),
   для которых другие компоненты могут регистрировать обработчики (handlers) — код, который будет
   выполняться при возникновении этих событий.  */
  ESP_ERROR_CHECK(esp_event_loop_create_default());

  /* a simple helper function, example_connect(), to establish Wi-Fi and/or Ethernet connection. This function
   has a very simple behavior: block until connection is established and IP address is obtained, then return.
   The simple example_connect() function does not handle timeouts, does not gracefully handle various error conditions,
   and is only suited for use in examples!!!!!!  */
  ESP_ERROR_CHECK(example_connect());

/* Register event handlers to stop the server when Wi-Fi or Ethernet is disconnected, and re-start it upon connection. */
#ifdef CONFIG_EXAMPLE_CONNECT_WIFI
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &connect_handler, &server));
    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, WIFI_EVENT_STA_DISCONNECTED, &disconnect_handler, &server));
#endif // CONFIG_EXAMPLE_CONNECT_WIFI
#ifdef CONFIG_EXAMPLE_CONNECT_ETHERNET
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_ETH_GOT_IP, &connect_handler, &server));
    ESP_ERROR_CHECK(esp_event_handler_register(ETH_EVENT, ETHERNET_EVENT_DISCONNECTED, &disconnect_handler, &server));
#endif // CONFIG_EXAMPLE_CONNECT_ETHERNET

/* Start the server for the first time */
server = start_webserver();
  /* --------------------------------------------------------------------------------------------------------------- */
  ESP_LOGI(TAG, "Initializing SNTP");
  time_t now;
  struct tm timeinfo;
  time(&now);
  localtime_r(&now, &timeinfo);
  // Is time set? If not, tm_year will be (1970 - 1900).
  if (timeinfo.tm_year < (2016 - 1900)) {
      ESP_LOGI(TAG, "Time is not set yet. Connecting to WiFi and getting time over NTP.");
      obtain_time();
      // update 'now' variable with current time
      time(&now);
  }

  char strftime_buf[64];


  // Set timezone to Eastern Standard Time and print local time
  setenv("TZ", "EST5EDT,M3.2.0/2,M11.1.0", 1);
  tzset();
  localtime_r(&now, &timeinfo);
  strftime(strftime_buf, sizeof(strftime_buf), "%c", &timeinfo);
  ESP_LOGI(TAG, "The current date/time in New York is: %s", strftime_buf);

  // Set timezone to China Standard Time
  setenv("TZ", "CST-8", 1);
  tzset();
  localtime_r(&now, &timeinfo);
  strftime(strftime_buf, sizeof(strftime_buf), "%c", &timeinfo);
  ESP_LOGI(TAG, "The current date/time in Shanghai is: %s", strftime_buf);

  // Set timezone to Moscow
  setenv("TZ", "MSK-3", 1);
  tzset();
  localtime_r(&now, &timeinfo);
  strftime(strftime_buf, sizeof(strftime_buf), "%c", &timeinfo);
  ESP_LOGI(TAG, "The current date/time in Moscow is: %s", strftime_buf);

  /* ---------------------------------------------------------------------------------------------------------------
  Sensors section */
  gpio_reset_pin(LED);
  /* Set the GPIO as a push/pull output */
  gpio_set_direction(LED, GPIO_MODE_OUTPUT);

ds18b20_init(TEMP_BUS);
getTempAddresses(tempSensors);
ds18b20_setResolution(tempSensors,2,10);

printf("Address 0: 0x%02x 0x%02x 0x%02x 0x%02x 0x%02x 0x%02x 0x%02x 0x%02x \n", tempSensors[0][0],tempSensors[0][1],tempSensors[0][2],tempSensors[0][3],tempSensors[0][4],tempSensors[0][5],tempSensors[0][6],tempSensors[0][7]);
printf("Address 1: 0x%02x 0x%02x 0x%02x 0x%02x 0x%02x 0x%02x 0x%02x 0x%02x \n", tempSensors[1][0],tempSensors[1][1],tempSensors[1][2],tempSensors[1][3],tempSensors[1][4],tempSensors[1][5],tempSensors[1][6],tempSensors[1][7]);


  ds18b20_requestTemperatures();
  float temp1 = ds18b20_getTempF((DeviceAddress *)tempSensors[0]);
  float temp2 = ds18b20_getTempF((DeviceAddress *)tempSensors[1]);
  float temp3 = ds18b20_getTempC((DeviceAddress *)tempSensors[0]);
  float temp4 = ds18b20_getTempC((DeviceAddress *)tempSensors[1]);
  printf("Temperatures: %0.2fF %0.2fF\n", temp1,temp2);
  printf("Temperatures: %0.2fC %0.2fC\n", temp3,temp4);
}

/* System time
---------------------------------------------------------------------------------------------*/
static void print_servers(void)
{
    ESP_LOGI(TAG, "List of configured NTP servers:");

    for (uint8_t i = 0; i < SNTP_MAX_SERVERS; ++i){
        if (esp_sntp_getservername(i)){
            ESP_LOGI(TAG, "server %d: %s", i, esp_sntp_getservername(i));
        } else {
            // we have either IPv4 or IPv6 address, let's print it
            char buff[INET6_ADDRSTRLEN];
            ip_addr_t const *ip = esp_sntp_getserver(i);
            if (ipaddr_ntoa_r(ip, buff, INET6_ADDRSTRLEN) != NULL)
                ESP_LOGI(TAG, "server %d: %s", i, buff);
        }
    }
}

static void obtain_time(void)
{

#if LWIP_DHCP_GET_NTP_SRV
    /**
     * NTP server address could be acquired via DHCP,
     * see following menuconfig options:
     * 'LWIP_DHCP_GET_NTP_SRV' - enable STNP over DHCP
     * 'LWIP_SNTP_DEBUG' - enable debugging messages
     *
     * NOTE: This call should be made BEFORE esp acquires IP address from DHCP,
     * otherwise NTP option would be rejected by default.
     */
    ESP_LOGI(TAG, "Initializing SNTP");
    esp_sntp_config_t config = ESP_NETIF_SNTP_DEFAULT_CONFIG(CONFIG_SNTP_TIME_SERVER);
    config.start = false;                       // start SNTP service explicitly (after connecting)
    config.server_from_dhcp = true;             // accept NTP offers from DHCP server, if any (need to enable *before* connecting)
    config.renew_servers_after_new_IP = true;   // let esp-netif update configured SNTP server(s) after receiving DHCP lease
    config.index_of_first_server = 1;           // updates from server num 1, leaving server 0 (from DHCP) intact
    // configure the event on which we renew servers
#ifdef CONFIG_EXAMPLE_CONNECT_WIFI
    config.ip_event_to_renew = IP_EVENT_STA_GOT_IP;
#else
    config.ip_event_to_renew = IP_EVENT_ETH_GOT_IP;
#endif
    config.sync_cb = time_sync_notification_cb; // only if we need the notification function
    esp_netif_sntp_init(&config);

#endif /* LWIP_DHCP_GET_NTP_SRV */


#if LWIP_DHCP_GET_NTP_SRV
    ESP_LOGI(TAG, "Starting SNTP");
    esp_netif_sntp_start();
#if LWIP_IPV6 && SNTP_MAX_SERVERS > 2
    /* This demonstrates using IPv6 address as an additional SNTP server
     * (statically assigned IPv6 address is also possible)
     */
    ip_addr_t ip6;
    if (ipaddr_aton("2a01:3f7::1", &ip6)) {    // ipv6 ntp source "ntp.netnod.se"
        esp_sntp_setserver(2, &ip6);
    }
#endif  /* LWIP_IPV6 */

#else
    ESP_LOGI(TAG, "Initializing and starting SNTP");
#if CONFIG_LWIP_SNTP_MAX_SERVERS > 1
    /* This demonstrates configuring more than one server
     */
    esp_sntp_config_t config = ESP_NETIF_SNTP_DEFAULT_CONFIG_MULTIPLE(2,
                               ESP_SNTP_SERVER_LIST(CONFIG_SNTP_TIME_SERVER, "pool.ntp.org" ) );
#else
    /*
     * This is the basic default config with one server and starting the service
     */
    esp_sntp_config_t config = ESP_NETIF_SNTP_DEFAULT_CONFIG(CONFIG_SNTP_TIME_SERVER);
#endif
    config.sync_cb = time_sync_notification_cb;     // Note: This is only needed if we want
#ifdef CONFIG_SNTP_TIME_SYNC_METHOD_SMOOTH
    config.smooth_sync = true;
#endif

    esp_netif_sntp_init(&config);
#endif

    print_servers();

    // wait for time to be set
    time_t now = 0;
    struct tm timeinfo = { 0 };
    int retry = 0;
    const int retry_count = 15;
    while (esp_netif_sntp_sync_wait(2000 / portTICK_PERIOD_MS) == ESP_ERR_TIMEOUT && ++retry < retry_count) {
        ESP_LOGI(TAG, "Waiting for system time to be set... (%d/%d)", retry, retry_count);
    }
    time(&now);
    localtime_r(&now, &timeinfo);

    esp_netif_sntp_deinit();
}
