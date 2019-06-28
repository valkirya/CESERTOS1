/* Iván camilo Aranda Casas
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


/* Macros I2C para su configuración */
#define I2C_MASTER_NUM           I2C_NUM_0             
#define I2C_MASTER_TX_BUF_LEN    0                     
#define I2C_MASTER_RX_BUF_LEN    0                    
#define I2C_MASTER_FREQ_HZ       100000
#define I2C_MASTER_SDA_IO        CONFIG_I2C_MASTER_SDA
#define I2C_MASTER_SCL_IO        CONFIG_I2C_MASTER_SCL


/* Macros para las credenciales que permitirán la conexión WI-FI */
#define EXAMPLE_WIFI_SSID CONFIG_WIFI_SSID
#define EXAMPLE_WIFI_PASS CONFIG_WIFI_PASSWORD

/* Macros de la UART */
#define EX_UART_NUM UART_NUM_0
#define BUF_SIZE (1024)
#define RD_BUF_SIZE (BUF_SIZE)

/* Cola para manejar el tipo de evento de la UART */
static QueueHandle_t uart0_queue;

/* Esto es requerido para manejar los eventos de la conexión Wi-FI */
static EventGroupHandle_t wifi_event_group;
const int CONNECTED_BIT = BIT0;

/* Seteamos el tag para los eventos de este código */
static const char *TAG = "CESE2019";

/* Prototipos de las funciones */

/* Tarea para gestionar el evento de interrupción por la UART */
static void uart_event_task(void *pvParameters);
/* Primera tarea que permite obtener hora y fecha por NTP */
static void obtain_time_task(void *pvParameters);
/* Segunda tarea que permite actualizar la hora  */
static void sync_time_task(void *pvParameters);
/* Evento que gestiona los eventos de conexión */
static esp_err_t event_handler(void *ctx, system_event_t *event);

/* Funciones de inicialización */
static void initialize_sntp(void);
static void initialise_wifi(void);
static void initialise_uart(void);
static void i2c_master_init(void);

/*Handle para la eliminación de la tarea "sync_time_task" */
TaskHandle_t xHandle = NULL;
/* Mutex para la sincronización de la tarea */
SemaphoreHandle_t xMutex;


void app_main()
{
    i2c_master_init();
    initialise_uart();
    /*NO SE INICIALIZA LA CONEXIÓN WI-FI YA QUE LA CONEXION NO ES PERSISTENTE*/
    xMutex = xSemaphoreCreateMutex();
    /* TASKS */ 
    xTaskCreate(uart_event_task, "uart_event_task", 2048, NULL, 8, NULL);
    xTaskCreate(sync_time_task, "sync_time_task", 4096, NULL, 4, &xHandle);
    xTaskCreate(obtain_time_task, "obtain_time_task", 4096, NULL, 2, NULL);

}

static void obtain_time_task(void *pvParameters){
    // Configuración de variables de I2C
    i2c_port_t i2c_num = I2C_MASTER_NUM;
    uint8_t address = CONFIG_LCD1602_I2C_ADDRESS;

    
    smbus_info_t * smbus_info = smbus_malloc();
    smbus_init(smbus_info, i2c_num, address);
    smbus_set_timeout(smbus_info, 1000 / portTICK_RATE_MS);

    // Reservamos un espacion en el heap que tendrá una estructura para el envío de datos a por I2C al display 16x2
    i2c_lcd1602_info_t * i2c_lcd1602_info = i2c_lcd1602_malloc();
    i2c_lcd1602_init(i2c_lcd1602_info, smbus_info, true);

    time_t now = 0;
    struct tm timeinfo = { 0 };
    char strftime_buf[64];
    /* Limpiamos la pantalla */
    i2c_lcd1602_clear(i2c_lcd1602_info);
    /* Seteamos la posición del cursor */
    i2c_lcd1602_set_cursor(i2c_lcd1602_info, false);

    /* En caso que no tenga fecha esperaremos a que sea tomada y seteada */
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
        i2c_lcd1602_write_string(i2c_lcd1602_info, strftime_buf);
        strftime(strftime_buf, sizeof(strftime_buf), "%a %b %e %Y", &timeinfo);
        i2c_lcd1602_move_cursor(i2c_lcd1602_info, 0, 1);
        i2c_lcd1602_write_string(i2c_lcd1602_info, strftime_buf);

    }
}

static void sync_time_task(void *pvParameters){
    nvs_flash_init();
    initialise_wifi();
    xEventGroupWaitBits(wifi_event_group, CONNECTED_BIT,
                        false, true, portMAX_DELAY);
    initialize_sntp();

    // Esperamos a que la hora sea tomada
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
    vTaskDelay(2000 / portTICK_PERIOD_MS);
    vTaskDelete( xHandle );
}

static void initialize_sntp(void){
    ESP_LOGI(TAG, "Initializing SNTP");
    /* Seteamos para que el servidor sea el local, solo para temas demostrativos en wireshark*/
    sntp_setservername(0, "pool.ntp.org");
    /* Seteamos el modo de operación del servidor ntp*/
    sntp_setoperatingmode(SNTP_OPMODE_POLL);
    /* Inicializamos el servicio. */
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
    ESP_LOGI(TAG, "Seteando la configuración del SSID para la conexión por WIFI %s...", wifi_config.sta.ssid);
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
    /* Se comprueba que el mutex se generó correctamente */
    if (xMutex!=NULL){
        /* Validamos si el mutex fue tomado */
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
                            xTaskCreate(sync_time_task, "sync_time_task", 4096, NULL, 12, &xHandle);
                            sntp_stop();
                            vTaskDelay(200 / portTICK_PERIOD_MS);
                            break;
                        default:
                            ESP_LOGI(TAG, "uart event type: %d", event.type);
                            break;
                    }
                }
            }
        /*Liberamos la reserva de memoria en el heap*/
        free(dtmp);
        dtmp = NULL;
        /* Liberamos el semaforo*/
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
