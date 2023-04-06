#include <stdio.h>
#include <string.h>
#include <sys/param.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_system.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "nvs_flash.h"
#include "freertos/semphr.h"
#include "freertos/queue.h"
#include "driver/gpio.h"
#include "esp_rom_gpio.h"
#define PORT 8080
#define EXAMPLE_ESP_WIFI_SSID      "SFM IoT" //SFM ETG_1"//SFM ETG3-1"    //SFM ETG2_1   $ko@48:&               // SFM IoT   obdel1833f2    //  SFM ETG4 1611A8EFA8
#define EXAMPLE_ESP_WIFI_PASS       "obdel1833f2"// SfM@1973        kE@42,3;        
#define EXAMPLE_ESP_MAXIMUM_RETRY  10
static const char *TAG = "socket_server";

static EventGroupHandle_t s_wifi_event_group;
static EventGroupHandle_t event_group;
#define WIFI_CONNECTED_BIT BIT0
#define WIFI_FAIL_BIT      BIT1

static int s_retry_num = 0;

#define RELAY_PIN 25







static void event_handler(void* arg, esp_event_base_t event_base,
                                int32_t event_id, void* event_data)
{
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        if (s_retry_num < EXAMPLE_ESP_MAXIMUM_RETRY) {
            esp_wifi_connect();
            vTaskDelay(500);
            s_retry_num++;
            printf("failed for %d",s_retry_num);
            ESP_LOGI(TAG, "retry to connect to the AP");
        } else {
            esp_restart();
            //xEventGroupSetBits(s_wifi_event_group, WIFI_FAIL_BIT);
        }
        ESP_LOGI(TAG,"connect to the AP fail");
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t* event = (ip_event_got_ip_t*) event_data;
        ESP_LOGI(TAG, "got ip:" IPSTR, IP2STR(&event->ip_info.ip));
        s_retry_num = 0;
        xEventGroupSetBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
    }
}

void wifi_init_sta(void)
{

    s_wifi_event_group = xEventGroupCreate();

    ESP_ERROR_CHECK(esp_netif_init());

    ESP_ERROR_CHECK(esp_event_loop_create_default());
    //esp_netif_t *my_sta = 
    esp_netif_create_default_wifi_sta();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));


    esp_event_handler_instance_t instance_any_id;
    esp_event_handler_instance_t instance_got_ip;

    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT,
                                                        ESP_EVENT_ANY_ID,
                                                        &event_handler,
                                                        NULL,
                                                        &instance_any_id));

    ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT,
                                                        IP_EVENT_STA_GOT_IP,
                                                        &event_handler,
                                                        NULL,
                                                        &instance_got_ip));

    wifi_config_t wifi_config = {
        .sta = {
            .ssid = EXAMPLE_ESP_WIFI_SSID,
            .password = EXAMPLE_ESP_WIFI_PASS,

        },
    };
    
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA) );
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config) );

    ESP_ERROR_CHECK(esp_wifi_start() );

    ESP_LOGI(TAG, "wifi_init_sta finished.");

    /* Waiting until either the connection is established (WIFI_CONNECTED_BIT) or connection failed for the maximum
     * number of re-tries (WIFI_FAIL_BIT). The bits are set by event_handler() (see above) */
    EventBits_t bits = xEventGroupWaitBits(s_wifi_event_group,
            WIFI_CONNECTED_BIT | WIFI_FAIL_BIT,
            pdFALSE,
            pdFALSE,
            portMAX_DELAY);

    /* xEventGroupWaitBits() returns the bits before the call returned, hence we can test which event actually
     * happened. */
    if (bits & WIFI_CONNECTED_BIT) {
        ESP_LOGI(TAG, "connected to ap SSID:%s password:%s",
                 EXAMPLE_ESP_WIFI_SSID, EXAMPLE_ESP_WIFI_PASS);
    } else if (bits & WIFI_FAIL_BIT) {
        ESP_LOGI(TAG, "Failed to connect to SSID:%s, password:%s",
                 EXAMPLE_ESP_WIFI_SSID, EXAMPLE_ESP_WIFI_PASS);
    } else {
        ESP_LOGE(TAG, "UNEXPECTED EVENT");
    }
    //}
    //vTaskDelay(1000);

}
static void start_socket_server(void *pvParameters)
{
    int server_fd, new_socket, valread;
    struct sockaddr_in address;
    int opt = 1;
    int addrlen = sizeof(address);
    char buffer[1024] = {0};
    const char *hello = "Hello from server";
       
    // Creating socket file descriptor
    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
        ESP_LOGE(TAG, "socket failed");
        vTaskDelete(NULL);
        return;
    }
       
    // Forcefully attaching socket to the port 8080
    /*if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT,
                                                  &opt, sizeof(opt))) {
        ESP_LOGE(TAG, "setsockopt failed");
        vTaskDelete(NULL);
        return;
    }*/
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons( PORT );
       
    // Forcefully attaching socket to the port 8080
    if (bind(server_fd, (struct sockaddr *)&address, 
                                 sizeof(address))<0) {
        ESP_LOGE(TAG, "bind failed");
        vTaskDelete(NULL);
        return;
    }
    if (listen(server_fd, 3) < 0) {
        ESP_LOGE(TAG, "listen failed");
        vTaskDelete(NULL);
        return;
    }
    while(1) {
        ESP_LOGI(TAG, "waiting for connection...");
        //ESP_LOGI(TAG, "ip addr.. %s.",INADDR_ANY);

        if ((new_socket = accept(server_fd, (struct sockaddr *)&address, 
                       (socklen_t*)&addrlen))<0) {
            ESP_LOGE(TAG, "accept failed");
            continue;
        }
        ESP_LOGI(TAG, "connection accepted from %s:%d", 
                 inet_ntoa(address.sin_addr), ntohs(address.sin_port));
        valread = read(new_socket, buffer, 1024);
        ESP_LOGI(TAG, "received message: %s", buffer);
        if (strstr(buffer, "on") != NULL) {
            printf("The word \'ON\' was found in the buffer.");
        gpio_set_level(RELAY_PIN, 1);
        //printf("The word \'ON\' was found in the buffer.");
        vTaskDelay(100);
        gpio_set_level(RELAY_PIN, 0);
        printf("The word \'ON\' was found in the buffer.");

                }
        send(new_socket, hello, strlen(hello), 0);
        ESP_LOGI(TAG, "Hello message sent");
        close(new_socket);
    }
    vTaskDelete(NULL);
}

void app_main(void)
{
    gpio_set_direction(14, GPIO_MODE_OUTPUT);
    gpio_set_level(14,1);
    gpio_set_direction(RELAY_PIN, GPIO_MODE_OUTPUT);

    ESP_LOGE(TAG, "gpio init done 1");
    gpio_set_level(RELAY_PIN, 0);

    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
      ESP_ERROR_CHECK(nvs_flash_erase());
      ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);
    ESP_LOGI(TAG, "ESP_WIFI_MODE_STA");
    wifi_init_sta();

    xTaskCreate(&start_socket_server, "socket_server", 5000, NULL, 5, NULL);
}