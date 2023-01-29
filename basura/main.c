/* ADC1 Example
   This example code is in the Public Domain (or CC0 licensed, at your option.)
   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "driver/gpio.h"
#include "driver/adc.h"
#include "esp_adc_cal.h"
#include "esp_log.h"

#include "ssd1306.h"
#include "font8x8_basic.h"

#define tag "SSD1306"
#define tag2 "ESP32"

char cadenaValor[13] = {" pH: "}, valorSimple[4];
char final[13] = {"  "};

#define DEFAULT_VREF    1100        //Use adc2_vref_to_gpio() to obtain a better estimate
#define NO_OF_SAMPLES   64          //Multisampling

static esp_adc_cal_characteristics_t *adc_chars;
#if CONFIG_IDF_TARGET_ESP32
static const adc_channel_t channel = ADC_CHANNEL_6;     //GPIO34 if ADC1, GPIO14 if ADC2
static const adc_bits_width_t width = ADC_WIDTH_BIT_12;
#elif CONFIG_IDF_TARGET_ESP32S2
static const adc_channel_t channel = ADC_CHANNEL_6;     // GPIO7 if ADC1, GPIO17 if ADC2
static const adc_bits_width_t width = ADC_WIDTH_BIT_13;
#endif
static const adc_atten_t atten = ADC_ATTEN_DB_11;
static const adc_unit_t unit = ADC_UNIT_1;


SemaphoreHandle_t xSemaphore = NULL;
TaskHandle_t myTaskHandle = NULL;

SSD1306_t dev;
int pot  = 1;

static void check_efuse(void)
{
#if CONFIG_IDF_TARGET_ESP32
    //Check if TP is burned into eFuse
    if (esp_adc_cal_check_efuse(ESP_ADC_CAL_VAL_EFUSE_TP) == ESP_OK) {
        printf("eFuse Two Point: Supported\n");
    } else {
        printf("eFuse Two Point: NOT supported\n");
    }
    //Check Vref is burned into eFuse
    if (esp_adc_cal_check_efuse(ESP_ADC_CAL_VAL_EFUSE_VREF) == ESP_OK) {
        printf("eFuse Vref: Supported\n");
    } else {
        printf("eFuse Vref: NOT supported\n");
    }
#elif CONFIG_IDF_TARGET_ESP32S2
    if (esp_adc_cal_check_efuse(ESP_ADC_CAL_VAL_EFUSE_TP) == ESP_OK) {
        printf("eFuse Two Point: Supported\n");
    } else {
        printf("Cannot retrieve eFuse Two Point calibration values. Default calibration values will be used.\n");
    }
#else
#error "This example is configured for ESP32/ESP32S2."
#endif
}


static void print_char_val_type(esp_adc_cal_value_t val_type)
{
    if (val_type == ESP_ADC_CAL_VAL_EFUSE_TP) {
        printf("Characterized using Two Point Value\n");
    } else if (val_type == ESP_ADC_CAL_VAL_EFUSE_VREF) {
        printf("Characterized using eFuse Vref\n");
    } else {
        printf("Characterized using Default Vref\n");
    }
}

static void mi_display(void *arg)
{
    //xSemaphoreTake(xSemaphore, portMAX_DELAY);
    ESP_LOGI(tag2, "COJO EL SEMAFORO EN DISPLAY");

    int center = 3, top = 2;
    // PRESENTACION
    ssd1306_clear_screen(&dev, false);
    ssd1306_contrast(&dev, 0xff);
    ssd1306_display_text_x3(&dev, center, "W.Q.T", 5, false);
    vTaskDelay(3000 / portTICK_PERIOD_MS);

    // VALORES GLOBALES
    ssd1306_clear_screen(&dev, false);
    ssd1306_display_text(&dev, 1, final, strlen(final), false);
    vTaskDelay(10000 / portTICK_PERIOD_MS);

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

    // Fade Out
    ssd1306_fadeout(&dev);

    //ESP_LOGI(tag2, "DEJO EL SEMAFORO EN DISPLAY");
    //xSemaphoreGive(xSemaphore);
    //vTaskDelete(NULL);
}

void app_main(void)
{
    xSemaphore = xSemaphoreCreateMutex();
    int contador = 0;
    
    //Check if Two Point or Vref are burned into eFuse
    check_efuse();

    //configure display
    
    i2c_master_init(&dev, CONFIG_SDA_GPIO, CONFIG_SCL_GPIO, CONFIG_RESET_GPIO);
    ssd1306_init(&dev, 128, 64);
    


    //Configure ADC
    if (unit == ADC_UNIT_1) {
        adc1_config_width(width);
        adc1_config_channel_atten(channel, atten);
    } else {
        adc2_config_channel_atten((adc2_channel_t)channel, atten);
    }

    //Characterize ADC
    adc_chars = calloc(1, sizeof(esp_adc_cal_characteristics_t));
    esp_adc_cal_value_t val_type = esp_adc_cal_characterize(unit, atten, width, DEFAULT_VREF, adc_chars);
    print_char_val_type(val_type);

    //Continuously sample ADC1
    do {
        uint32_t adc_reading = 0;
        //Multisampling
        for (int i = 0; i < NO_OF_SAMPLES; i++) {
            if (unit == ADC_UNIT_1) {
                adc_reading += adc1_get_raw((adc1_channel_t)channel);
            } else {
                int raw;
                adc2_get_raw((adc2_channel_t)channel, width, &raw);
                adc_reading += raw;
            }
        }
        adc_reading /= NO_OF_SAMPLES;
        //Convert adc_reading to voltage in mV
        uint32_t voltage = esp_adc_cal_raw_to_voltage(adc_reading, adc_chars);
        printf("Raw: %ld\tVoltage: %ldmV\n", adc_reading, voltage);

        int vol = (int) voltage;
        itoa(vol, valorSimple, 10);
        strcat(cadenaValor, valorSimple);
         
        ssd1306_clear_screen(&dev, false);
        ssd1306_contrast(&dev, 0xff);
        ssd1306_display_text(&dev, 1, cadenaValor, strlen(cadenaValor), false);

        if(contador == 2){
            strcpy(final, cadenaValor);
            ESP_LOGI(tag2, "final: %s", final);
            
        }

        bzero(valorSimple, sizeof(valorSimple));
        bzero(cadenaValor, sizeof(cadenaValor));
        strcpy(cadenaValor, "Valor: ");

        contador++;
        ESP_LOGI(tag2, "contador: %d", contador);
        vTaskDelay(pdMS_TO_TICKS(1000));
        
        
    } while (contador < 3);
    contador = 0;
    
    ESP_LOGI(tag2, "HE SALIDO");
   
    xTaskCreate(mi_display, "task_display", 4096, NULL, 10, &myTaskHandle);
    //xSemaphoreGive(xSemaphore);

    //xSemaphoreTake(xSemaphore, portMAX_DELAY);
    ESP_LOGI(tag2, "COJO EL SEMAFORO EN MAIN Y RESETEO");
    //esp_restart();
}

/*
    xTaskCreate(Display, "task_display", 4096, NULL, 10, &myTaskHandle);
    xTaskCreateToPin(Display, "task_display", 4096, NULL, 10, &myTaskHandle);

*/