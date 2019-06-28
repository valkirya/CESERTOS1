/* LwIP SNTP example

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/
#include <string.h>
#include <time.h>
#include <sys/time.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "esp_event_loop.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "lwip/err.h"
#include "lwip/apps/sntp.h"
#include "driver/uart.h"
#include "smbus.h"
#include "i2c-lcd1602.h"


/* The examples use simple WiFi configuration that you can set via
   'make menuconfig'.

   If you'd rather not, just change the below entries to strings with
   the config you want - ie #define EXAMPLE_WIFI_SSID "mywifissid"
*/

#define I2C_MASTER_NUM           I2C_NUM_0
#define I2C_MASTER_TX_BUF_LEN    0                     // 
#define I2C_MASTER_RX_BUF_LEN    0                     // 
#define I2C_MASTER_FREQ_HZ       100000
#define I2C_MASTER_SDA_IO        CONFIG_I2C_MASTER_SDA
#define I2C_MASTER_SCL_IO        CONFIG_I2C_MASTER_SCL

#define EXAMPLE_WIFI_SSID CONFIG_WIFI_SSID
#define EXAMPLE_WIFI_PASS CONFIG_WIFI_PASSWORD

/*Macros de la UART*/
#define EX_UART_NUM UART_NUM_0
#define BUF_SIZE (1024)
#define RD_BUF_SIZE (BUF_SIZE)
static QueueHandle_t uart0_queue;

/* FreeRTOS event group to signal when we are connected & ready to make a request */
static EventGroupHandle_t wifi_event_group;

/* The event group allows multiple bits for each event,
   but we only care about one event - are we connected
   to the AP with an IP? */
const int CONNECTED_BIT = BIT0;

static const char *TAG = "CESE2019";

/* Variable holding number of times ESP32 restarted since first boot.
 * It is placed into RTC memory using RTC_DATA_ATTR and
 * maintains its value when ESP32 wakes from deep sleep.
 */
static void uart_event_task(void *pvParameters);
static void obtain_time(void *pvParameters);
static void sync_time(void *pvParameters);
static void initialize_sntp(void);
static void initialise_wifi(void);
static void initialise_uart(void);
static esp_err_t event_handler(void *ctx, system_event_t *event);
 static void i2c_master_init(void);

TaskHandle_t xHandle = NULL;
SemaphoreHandle_t xMutex;


void app_main()
{
    i2c_master_init();
    initialise_uart();
    xMutex = xSemaphoreCreateMutex();
    /* Create a task obtain_time */ 
    //Create a task to handler UART event from ISR
    xTaskCreate(uart_event_task, "uart_event_task", 2048, NULL, 8, NULL);
    xTaskCreate(sync_time, "sync_time", 4096, NULL, 4, &xHandle);
    xTaskCreate(obtain_time, "obtain_time", 4096, NULL, 2, NULL);

}

static void obtain_time(void *pvParameters){
    // Set up I2C
    i2c_port_t i2c_num = I2C_MASTER_NUM;
    uint8_t address = CONFIG_LCD1602_I2C_ADDRESS;

    // Set up the SMBus
    smbus_info_t * smbus_info = smbus_malloc();
    smbus_init(smbus_info, i2c_num, address);
    smbus_set_timeout(smbus_info, 1000 / portTICK_RATE_MS);

    // Set up the LCD1602 device with backlight off
    i2c_lcd1602_info_t * i2c_lcd1602_info = i2c_lcd1602_malloc();
    i2c_lcd1602_init(i2c_lcd1602_info, smbus_info, true);

    time_t now = 0;
    struct tm timeinfo = { 0 };
    char strftime_buf[64];

    i2c_lcd1602_clear(i2c_lcd1602_info);
    i2c_lcd1602_set_cursor(i2c_lcd1602_info, false);

    while(timeinfo.tm_year < (2019 - 1900)){
        ESP_LOGI(TAG, "Obteniendo la hora...");
        time(&now);
        localtime_r(&now, &timeinfo);
        vTaskDelay(2000 / portTICK_PERIOD_MS);
    }

    while(true) {
        /* Solicitamos la hora actual */
        time(&now);
        /* UTC + 3 Corresponde a la zona horaria Buenos Aires Argentinas. */
        setenv("TZ", "UTC+3", 1);
        /* Seteamos la zona horaria */
        tzset();
        localtime_r(&now, &timeinfo);
        strftime(strftime_buf, sizeof(strftime_buf), "%H:%M:%S", &timeinfo);
        i2c_lcd1602_move_cursor(i2c_lcd1602_info, 0, 0);
        //ESP_LOGI(TAG, "%s", strftime_buf);
        i2c_lcd1602_write_string(i2c_lcd1602_info, strftime_buf);
        strftime(strftime_buf, sizeof(strftime_buf), "%a %b %e %Y", &timeinfo);
        i2c_lcd1602_move_cursor(i2c_lcd1602_info, 0, 1);
        i2c_lcd1602_write_string(i2c_lcd1602_info, strftime_buf);

    }
}

static void sync_time(void *pvParameters){
    nvs_flash_init();
    initialise_wifi();
    xEventGroupWaitBits(wifi_event_group, CONNECTED_BIT,
                        false, true, portMAX_DELAY);
    initialize_sntp();

    // wait for time to be set
    time_t now = 0;
    struct tm timeinfo = { 0 };
    char strftime_buf[64];
    while(timeinfo.tm_year < (2019 - 1900)) {
        vTaskDelay(1000 / portTICK_PERIOD_MS);
        time(&now);
        localtime_r(&now, &timeinfo);
    }
    strftime(strftime_buf, sizeof(strftime_buf), "%c", &timeinfo);
    ESP_LOGI(TAG, "Hora y Fecha Actualizada para Buenos Aires: %s", strftime_buf);
    esp_wifi_stop();
    vTaskDelay(2000 / portTICK_PERIOD_MS);    vTaskDelay(2000 / portTICK_PERIOD_MS);

    vTaskDelete( xHandle );
}

static void initialize_sntp(void){
    ESP_LOGI(TAG, "Initializing SNTP");
    sntp_setservername(0, "pool.ntp.org");
    sntp_setoperatingmode(SNTP_OPMODE_POLL);
    sntp_init();

}

static void initialise_wifi(void){
    tcpip_adapter_init();
    wifi_event_group = xEventGroupCreate();
    esp_event_loop_init(event_handler, NULL);
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    esp_wifi_init(&cfg);
    esp_wifi_set_storage(WIFI_STORAGE_RAM);
    wifi_config_t wifi_config = {
        .sta = {
            .ssid = EXAMPLE_WIFI_SSID,
            .password = EXAMPLE_WIFI_PASS,
        },
    };
    ESP_LOGI(TAG, "Setting WiFi configuration SSID %s...", wifi_config.sta.ssid);
    esp_wifi_set_mode(WIFI_MODE_STA);
    esp_wifi_set_config(ESP_IF_WIFI_STA, &wifi_config);
    esp_wifi_start();
}

static esp_err_t event_handler(void *ctx, system_event_t *event){
    switch(event->event_id) {
    case SYSTEM_EVENT_STA_START:
        esp_wifi_connect();
        break;
    case SYSTEM_EVENT_STA_GOT_IP:
        xEventGroupSetBits(wifi_event_group, CONNECTED_BIT);
        break;
    case SYSTEM_EVENT_STA_DISCONNECTED:
        /* This is a workaround as ESP32 WiFi libs don't currently
           auto-reassociate. */
        esp_wifi_connect();
        xEventGroupClearBits(wifi_event_group, CONNECTED_BIT);
        break;
    default:
        break;
    }
    return ESP_OK;
}

static void initialise_uart(void){
    esp_log_level_set(TAG, ESP_LOG_INFO);

    /* Configure parameters of an UART driver,
     * communication pins and install the driver */
    uart_config_t uart_config = {
        .baud_rate = 115200,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE
    };
    uart_param_config(EX_UART_NUM, &uart_config);
    //Set UART log level
    esp_log_level_set(TAG, ESP_LOG_INFO);
    //Set UART pins (using UART0 default pins ie no changes.)
    uart_set_pin(EX_UART_NUM, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);
    //Install UART driver, and get the queue.
    uart_driver_install(EX_UART_NUM, BUF_SIZE * 2, BUF_SIZE * 2, 20, &uart0_queue, 0);
    //Reset the pattern queue length to record at most 20 pattern positions.
    uart_pattern_queue_reset(EX_UART_NUM, 20);
}

static void uart_event_task(void *pvParameters){
    if (xMutex!=NULL){
        if(xSemaphoreTake(xMutex, portMAX_DELAY)==pdTRUE){
            uart_event_t event;
            uint8_t* dtmp = (uint8_t*) malloc(RD_BUF_SIZE);
            for(;;) {
                //Waiting for UART event.
                if(xQueueReceive(uart0_queue, (void * )&event, (portTickType)portMAX_DELAY)) {
                    switch(event.type) {
                        //Event of UART receving data
                        /*We'd better handler data event fast, there would be much more data events than
                        other types of events. If we take too much time on data event, the queue might
                        be full.*/
                        case UART_DATA:
                            xTaskCreate(sync_time, "sync_time", 4096, NULL, 12, &xHandle);
                            sntp_stop();
                            vTaskDelay(200 / portTICK_PERIOD_MS);
                            break;
                        default:
                            ESP_LOGI(TAG, "uart event type: %d", event.type);
                            break;
                    }
                }
            }
        free(dtmp);
        dtmp = NULL;
        xSemaphoreGive(xMutex);
        }
    }
    vTaskDelete(NULL);
}

static void i2c_master_init(void){
    int i2c_master_port = I2C_MASTER_NUM;
    i2c_config_t conf;
    conf.mode = I2C_MODE_MASTER;
    conf.sda_io_num = I2C_MASTER_SDA_IO;
    conf.sda_pullup_en = GPIO_PULLUP_DISABLE;  // GY-2561 provides 10kΩ pullups
    conf.scl_io_num = I2C_MASTER_SCL_IO;
    conf.scl_pullup_en = GPIO_PULLUP_DISABLE;  // GY-2561 provides 10kΩ pullups
    conf.master.clk_speed = I2C_MASTER_FREQ_HZ;
    i2c_param_config(i2c_master_port, &conf);
    i2c_driver_install(i2c_master_port, conf.mode,
                       I2C_MASTER_RX_BUF_LEN,
                       I2C_MASTER_TX_BUF_LEN, 0);
}
