/* MQTT (over TCP) Example

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/

#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include "esp_wifi.h"
#include "esp_system.h"
#include "nvs_flash.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "protocol_examples_common.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "freertos/queue.h"
#include "freertos/event_groups.h"

#include "driver/adc.h"
#include "esp_adc_cal.h"

#include "lwip/sockets.h"
#include "lwip/dns.h"
#include "lwip/netdb.h"
//#include "mqtt_msg.h"
#include "esp_log.h"
#include "mqtt_client.h"
#include "cJSON.h"

#include "ds18b20.h"
#include "driver/gpio.h"

#include "telegram.c"

#include "ssd1306.h"
#include "font8x8_basic.h"

#define tag "SSD1306"
#define tag2 "ESP32"

SSD1306_t dev;
int pot  = 1;


char cadenaPH[14] = {" pH: "}, cpH[5], cadenaORP[13] = {" ORP: "}, corp[4], cadenaTemp[13] = {" Temp: "}, ctemp[4], cadenaTurb[13] = {" Turbidez: "}, cturb[4];
char cadenaValor[13] = {" pH: "}, valorSimple[4];
char final[13] = {"  "};


int center = 3, top = 2;

const int DS_PIN = 14;

#define offset -0.5

static const char *TAG5 = "MQTT_EXAMPLE";

static esp_adc_cal_characteristics_t adc1_chars;

 esp_mqtt_client_handle_t client;

static esp_err_t mqtt_event_handler_cb(esp_mqtt_event_handle_t event)
{

 client = event->client;

 int msg_id;
 // your_context_t *context = event->context;
 switch (event->event_id) {
 case MQTT_EVENT_CONNECTED:
 ESP_LOGI(TAG5, "MQTT_EVENT_CONNECTED");
 break;
 case MQTT_EVENT_DISCONNECTED:
 ESP_LOGI(TAG5, "MQTT_EVENT_DISCONNECTED");
 break;
 case MQTT_EVENT_SUBSCRIBED:
 ESP_LOGI(TAG5, "MQTT_EVENT_SUBSCRIBED");
 break;
 case MQTT_EVENT_UNSUBSCRIBED:
 ESP_LOGI(TAG5, "MQTT_EVENT_UNSUBSCRIBED");
 break;
 case MQTT_EVENT_PUBLISHED:
 ESP_LOGI(TAG5, "MQTT_EVENT_PUBLISHED");
 break;
 case MQTT_EVENT_DATA:
 ESP_LOGI(TAG5, "MQTT_EVENT_DATA");
 printf("TOPIC=%.*s\r\n", event->topic_len, event->topic);
 printf("DATA=%.*s\r\n", event->data_len, event->data);
 break;
 case MQTT_EVENT_ERROR:
 ESP_LOGI(TAG5, "MQTT_EVENT_ERROR");
 break;
 default:
 ESP_LOGI(TAG5, "Other event id:%d", event->event_id);
 break;
 }
 return ESP_OK;
}
static void mqtt_event_handler(void *handler_args, esp_event_base_t base, int32_t event_id,
void *event_data) {
     ESP_LOGD(TAG5, "Event dispatched from event loop base=%s, event_id=%d", base, event_id);
 mqtt_event_handler_cb(event_data);
}
static void mqtt_app_start(void)
{
 esp_mqtt_client_config_t mqtt_cfg = {
    .uri = "mqtt://demo.thingsboard.io",
    .event_handle = mqtt_event_handler,
    .port = 1883,
    .username = "4mvkrGgqNI9YRE62nGk1",
 };
 client = esp_mqtt_client_init(&mqtt_cfg);
 esp_mqtt_client_register_event(client, ESP_EVENT_ANY_ID, mqtt_event_handler, client);
 esp_mqtt_client_start(client);

}

void enviar(float valor, char dato[])
{

 cJSON *root = cJSON_CreateObject();
 cJSON_AddNumberToObject(root, dato, valor);
 char *post_data = cJSON_PrintUnformatted(root);
 esp_mqtt_client_publish(client, "v1/devices/me/telemetry", post_data, 0, 1, 0);
 cJSON_Delete(root);
 free(post_data);
 vTaskDelay(100 / portTICK_PERIOD_MS);


}



void mainTask(void *pvParameters) {
  ds18b20_init(DS_PIN);
  //float temp;
  //float ph;
  //float orp;
  bool local = true;
  float voltage;
  int contador = 0;
  esp_adc_cal_characterize(ADC_UNIT_1, ADC_ATTEN_DB_11, ADC_WIDTH_BIT_DEFAULT, 0, &adc1_chars);

  adc1_config_width(ADC_WIDTH_BIT_DEFAULT);
  adc1_config_channel_atten(ADC1_CHANNEL_4, ADC_ATTEN_DB_11); //32

  adc1_config_channel_atten(ADC1_CHANNEL_5, ADC_ATTEN_DB_11); //33

  do {

    temp = ds18b20_get_temp();
    printf("Temperatura: %0.1f", temp);
    printf("\n");

    float adc_valuePH = adc1_get_raw(ADC1_CHANNEL_4);
    //ph = 3.5*adc_valuePH+offset;
    ph = esp_adc_cal_raw_to_voltage(adc_valuePH, &adc1_chars);
    ph = ph / 270;
    //printf("ADC_CH4 Value: %0.2f\n", adc_value);
    printf("PH: %0.2f", ph);
    printf("\n");

    float adc_valueORP = adc1_get_raw(ADC1_CHANNEL_5);  
    voltage = esp_adc_cal_raw_to_voltage(adc_valueORP, &adc1_chars);
    voltage = (voltage + 700) / 10;
    //printf("ADC_CH5 Value: %0.2f\n", adc_value);
    orp = voltage;
    printf("ORP: %0.2f mV", orp);
    printf("\n");

    vTaskDelay(3000 / portTICK_PERIOD_MS);

    //convertimos temp
    sprintf(ctemp, "%.1f", temp); // Convertir el número a una cadena con dos decimales
    strcat(cadenaTemp, ctemp);         //[strcat(destino, fuente)]
    //convertimos ph
    sprintf(cpH, "%.2f", ph); // Convertir el número a una cadena con dos decimales
    strcat(cadenaPH, cpH);         //[strcat(destino, fuente)]
    //convertimos orp
    sprintf(corp, "%.1f", orp); // Convertir el número a una cadena con dos decimales
    strcat(cadenaORP, corp);         //[strcat(destino, fuente)]

    ssd1306_clear_screen(&dev, false);
    ssd1306_contrast(&dev, 0xff);
    ssd1306_display_text(&dev, 1, cadenaTemp, strlen(cadenaTemp), false);
    ssd1306_display_text(&dev, 3, cadenaPH, strlen(cadenaPH), false);
    ssd1306_display_text(&dev, 5, cadenaORP, strlen(cadenaORP), false);

    bzero(cadenaTemp, sizeof(cadenaTemp));
    bzero(cadenaPH, sizeof(cadenaPH));
    bzero(cadenaORP, sizeof(cadenaORP));
    bzero(ctemp, sizeof(ctemp));
    bzero(cpH, sizeof(cpH));
    bzero(corp, sizeof(corp));

    strcpy(cadenaTemp, " Temp: ");
    strcpy(cadenaPH, " pH: ");
    strcpy(cadenaORP, " ORP: ");

    enviar(temp, "temperatura");
    enviar(ph, "ph");
    enviar(orp, "orp");

    tempmedia = tempmedia + temp;
    orpmedia = orpmedia + orp;    
    phmedia = phmedia + ph; 

    //

      contador++;

  } while (contador < 8);

  contador = 0;

   if (contador == 7) {
      //convertimos temp
    sprintf(ctemp, "%.1f", tempmedia/7); // Convertir el número a una cadena con dos decimales
    strcat(cadenaTemp, ctemp);         //[strcat(destino, fuente)]
    //convertimos ph
    sprintf(cpH, "%.2f", phmedia/7); // Convertir el número a una cadena con dos decimales
    strcat(cadenaPH, cpH);         //[strcat(destino, fuente)]
    //convertimos orp
    sprintf(corp, "%.1f", orpmedia/7); // Convertir el número a una cadena con dos decimales
    strcat(cadenaORP, corp); 
   } else {

      //convertimos temp
    sprintf(ctemp, "%.1f", temp); // Convertir el número a una cadena con dos decimales
    strcat(cadenaTemp, ctemp);         //[strcat(destino, fuente)]
    //convertimos ph
    sprintf(cpH, "%.2f", ph); // Convertir el número a una cadena con dos decimales
    strcat(cadenaPH, cpH);         //[strcat(destino, fuente)]
    //convertimos orp
    sprintf(corp, "%.1f", orp); // Convertir el número a una cadena con dos decimales
    strcat(cadenaORP, corp);
     
   }


  ssd1306_clear_screen(&dev, false);
  ssd1306_contrast(&dev, 0xff);
  ssd1306_display_text(&dev, 1, cadenaTemp, strlen(cadenaTemp), false);
  ssd1306_display_text(&dev, 3, cadenaPH, strlen(cadenaPH), false);
  ssd1306_display_text(&dev, 5, cadenaORP, strlen(cadenaORP), false);
  vTaskDelay(5000 / portTICK_PERIOD_MS);

  // Display Count Down
    ssd1306_clear_screen(&dev, false);
    uint8_t image[24];
    memset(image, 0, sizeof(image));
    ssd1306_display_image(&dev, top, (6 * 8 - 1), image, sizeof(image));
    ssd1306_display_image(&dev, top + 1, (6 * 8 - 1), image, sizeof(image));
    ssd1306_display_image(&dev, top + 2, (6 * 8 - 1), image, sizeof(image));
    for (int font = 0x39; font > 0x30; font--)
    {
        memset(image, 0, sizeof(image));
        ssd1306_display_image(&dev, top + 1, (7 * 8 - 1), image, 8);
        memcpy(image, font8x8_basic_tr[font], 8);
        if (dev._flip)
            ssd1306_flip(image, 8);
        ssd1306_display_image(&dev, top + 1, (7 * 8 - 1), image, 8);
        vTaskDelay(1000 / portTICK_PERIOD_MS);
    }
    vTaskDelay(1000 / portTICK_PERIOD_MS);

    
    if(ph >= 7 && ph <= 8.5){
       pot = 1;
       potable = 1;
    }
     
    else {
      pot = 0;
      potable = 0;
    }

    
    if (local) {
        
      xTaskCreatePinnedToCore(&http_test_task, "http_test_task", 8192*4, NULL, 5, NULL,1);
      local = false;

      }

    if (pot == 1)
    {
        // RESULTADO
        ssd1306_clear_screen(&dev, false);
        ssd1306_display_text_x3(&dev, center, "VALOR", 5, false);
        vTaskDelay(3000 / portTICK_PERIOD_MS);
        ssd1306_clear_screen(&dev, false);
        ssd1306_display_text(&dev, center, "---SI POTABLE---", 16, false);
        vTaskDelay(5000 / portTICK_PERIOD_MS);
    } else {

         // RESULTADO
        ssd1306_clear_screen(&dev, false);
        ssd1306_display_text_x3(&dev, center, "VALOR", 5, false);
        vTaskDelay(3000 / portTICK_PERIOD_MS);
        ssd1306_clear_screen(&dev, false);
        ssd1306_display_text(&dev, center, "---NO POTABLE---", 16, false);
        vTaskDelay(5000 / portTICK_PERIOD_MS);
    }


}


void app_main(void)
{
 
 i2c_master_init(&dev, CONFIG_SDA_GPIO, CONFIG_SCL_GPIO, CONFIG_RESET_GPIO);
 ssd1306_init(&dev, 128, 64);

 ssd1306_clear_screen(&dev, false);
 ssd1306_contrast(&dev, 0xff);
 ssd1306_display_text_x3(&dev, center, "W.Q.T", 5, false);
 vTaskDelay(1000 / portTICK_PERIOD_MS);


 ESP_LOGI(TAG, "[APP] Startup..");
 ESP_LOGI(TAG, "[APP] Free memory: %d bytes", esp_get_free_heap_size());
 ESP_LOGI(TAG, "[APP] IDF version: %s", esp_get_idf_version());
 esp_log_level_set("*", ESP_LOG_INFO);
 
 ssd1306_clear_screen(&dev, false);
 ssd1306_contrast(&dev, 0xff);
 ssd1306_display_text(&dev, 3, "Conectado WIFI", 14, false);

 esp_log_level_set("MQTT_CLIENT", ESP_LOG_VERBOSE);
 esp_log_level_set("MQTT_EXAMPLE", ESP_LOG_VERBOSE);
 esp_log_level_set("TRANSPORT_TCP", ESP_LOG_VERBOSE);
 esp_log_level_set("TRANSPORT_SSL", ESP_LOG_VERBOSE);
 esp_log_level_set("TRANSPORT", ESP_LOG_VERBOSE);
 esp_log_level_set("OUTBOX", ESP_LOG_VERBOSE);
 ESP_ERROR_CHECK(nvs_flash_init());
 ESP_ERROR_CHECK(esp_netif_init());
 ESP_ERROR_CHECK(esp_event_loop_create_default());
 /* This helper function configures Wi-Fi or Ethernet, as selected in menuconfig.
 * Read "Establishing Wi-Fi or Ethernet Connection" section in
 * examples/protocols/README.md for more information about this function.
 */
 ESP_ERROR_CHECK(example_connect());
xTaskCreatePinnedToCore(&mainTask, "mainTask", 2048, NULL, 5, NULL, 0);

 mqtt_app_start();
}