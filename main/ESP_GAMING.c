#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "esp_err.h"
#include "esp_log.h"
#include "esp_system.h"
#include "esp_vfs.h"
#include "esp_spiffs.h"

#include "freertos/semphr.h"

#include "axp192.h"
#include "st7735s.h"
#include "fontx.h"
#include "bmpfile.h"
#include "decode_jpeg.h"
#include "decode_png.h"
#include "pngle.h"

#include "game.h"

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 160
#define OFFSET_X 0
#define OFFSET_Y 0
#define GPIO_MOSI 22
#define GPIO_SCLK 23
#define GPIO_CS 18
#define GPIO_DC 19
#define GPIO_RESET 21

#define ESP_INTR_FLAG_DEFAULT 0

#define GPIO_USER1 15
#define GPIO_USER2 5
#define GPIO_USER3 17

static const char *TAG = "ESP_GAMING_MAIN";

ST7735_t dev;
SemaphoreHandle_t xMutexSemaphore_Game = NULL;
QueueHandle_t interuptQueue = NULL;

Game games[10];

//Font Files
FontxFile fx16[2];
FontxFile fx24[2];
FontxFile fx32[2];

void IRAM_ATTR gpio_interrupt_handler(void* arg) {
    int pinNumber = (int)arg;
    gpio_intr_disable(pinNumber); 
    xQueueSendFromISR(interuptQueue, &pinNumber, NULL);
}

void enable_all_intr();
void disable_all_intr();
static void SPIFFS_Directory(char * path);
static void ESP_GAMING_Presentation();
void init_GPIO();
void User_Input_Task(void *params);
void Video_Output_Task(void *params);


void app_main(void) {

	ESP_LOGI(TAG, "Initializing SPIFFS");

	esp_vfs_spiffs_conf_t conf = {
		.base_path = "/spiffs",
		.partition_label = NULL,
		.max_files = 10,
		.format_if_mount_failed =true
	};

	// Use settings defined above toinitialize and mount SPIFFS filesystem.
	// Note: esp_vfs_spiffs_register is anall-in-one convenience function.
	esp_err_t ret =esp_vfs_spiffs_register(&conf);

	if (ret != ESP_OK) {
		if (ret ==ESP_FAIL) {
			ESP_LOGE(TAG, "Failed to mount or format filesystem");
		} else if (ret== ESP_ERR_NOT_FOUND) {
			ESP_LOGE(TAG, "Failed to find SPIFFS partition");
		} else {
			ESP_LOGE(TAG, "Failed to initialize SPIFFS (%s)",esp_err_to_name(ret));
		}
		return;
	}

	size_t total = 0, used = 0;
	ret = esp_spiffs_info(NULL, &total,&used);
	if (ret != ESP_OK) {
		ESP_LOGE(TAG,"Failed to get SPIFFS partition information (%s)",esp_err_to_name(ret));
	} else {
		ESP_LOGI(TAG,"Partition size: total: %d, used: %d", total, used);
	}

	SPIFFS_Directory("/spiffs");


	//InitFontx(fx16,"/spiffs/ILGH16XB.FNT",""); // 8x16Dot Gothic
	InitFontx(fx24,"/spiffs/ILGH24XB.FNT",""); // 12x24Dot Gothic
	InitFontx(fx32,"/spiffs/ILGH32XB.FNT",""); // 16x32Dot Gothic

	InitFontx(fx16,"/spiffs/ILMH16XB.FNT",""); // 8x16Dot Mincyo
	//InitFontx(fx24,"/spiffs/ILMH24XB.FNT",""); // 12x24Dot Mincyo
	//InitFontx(fx32,"/spiffs/ILMH32XB.FNT",""); // 16x32Dot Mincyo

	init_GPIO();

	spi_master_init(&dev, GPIO_MOSI, GPIO_SCLK, GPIO_CS, GPIO_DC, GPIO_RESET);
	lcdInit(&dev, SCREEN_WIDTH, SCREEN_HEIGHT, OFFSET_X, OFFSET_Y);

	interuptQueue = xQueueCreate(10, sizeof(uint32_t));

    gpio_install_isr_service(ESP_INTR_FLAG_DEFAULT);

    gpio_isr_handler_add(GPIO_USER1, gpio_interrupt_handler, (void *)GPIO_USER1);
    gpio_isr_handler_add(GPIO_USER2, gpio_interrupt_handler, (void *)GPIO_USER2);
    gpio_isr_handler_add(GPIO_USER3, gpio_interrupt_handler, (void *)GPIO_USER3);

	disable_all_intr();

	xMutexSemaphore_Game=xSemaphoreCreateMutex();

	if(xSemaphoreTake(xMutexSemaphore_Game, portMAX_DELAY)) {
		lcdFillScreen(&dev,BLACK);
		ESP_GAMING_Presentation();
		xSemaphoreGive(xMutexSemaphore_Game);
	}



}

void enable_all_intr() {
	gpio_intr_enable(GPIO_USER1); 
	gpio_intr_enable(GPIO_USER2); 
	gpio_intr_enable(GPIO_USER3); 
}

void disable_all_intr() {
	gpio_intr_disable(GPIO_USER1); 
	gpio_intr_disable(GPIO_USER2); 
	gpio_intr_disable(GPIO_USER3); 
}

static void SPIFFS_Directory(char * path) {
	DIR* dir = opendir(path);
	assert(dir != NULL);
	while (true) {
		struct dirent*pe = readdir(dir);
		if (!pe) break;
		ESP_LOGI(TAG,"d_name=%s d_ino=%d d_type=%x", pe->d_name,pe->d_ino, pe->d_type);
	}
	closedir(dir);
}

static void ESP_GAMING_Presentation() {
	uint8_t buffer[FontxGlyphBufSize];
	uint8_t fontWidth;
	uint8_t fontHeight;
	GetFontx(fx16, 0, buffer, &fontWidth, &fontHeight);
	ESP_LOGD(TAG,"fontWidth=%d fontHeight=%d",fontWidth,fontHeight);

	uint16_t color;
	uint8_t app1[10];
	uint8_t app2[15];
	strcpy((char *)app1, "E");
	strcpy((char *)app2, "G");

	color = GREEN;
	lcdSetFontDirection(&dev, 0);
	lcdDrawString(&dev, fx16, 30, 15, app1, color);
	lcdDrawString(&dev, fx16, 50, 160, app2, color);

	vTaskDelay(20);

	color = RED;
	lcdSetFontDirection(&dev, 0);
	lcdDrawString(&dev, fx16, 30, 30, app1, color);
	lcdDrawString(&dev, fx16, 50, 145, app2, color);

	vTaskDelay(20);

	color = BLUE;
	lcdSetFontDirection(&dev, 0);
	lcdDrawString(&dev, fx16, 30, 45, app1, color);
	lcdDrawString(&dev, fx16, 50, 130, app2, color);

	vTaskDelay(20);

	color = CYAN;
	lcdSetFontDirection(&dev, 0);
	lcdDrawString(&dev, fx16, 30, 60, app1, color);
	lcdDrawString(&dev, fx16, 50, 115, app2, color);

	vTaskDelay(20);

	strcpy((char *)app1, "ESP");
	strcpy((char *)app2, "GAMING");
	color = WHITE;
	lcdSetFontDirection(&dev, 0);
	lcdDrawString(&dev, fx16, 30, 80, app1, color);
	lcdDrawString(&dev, fx16, 50, 95, app2, color);

	vTaskDelay(100);
}

void init_GPIO() {
    gpio_config_t io_config_but = {
        .pin_bit_mask = (1ULL<<GPIO_USER1)|(1ULL<<GPIO_USER2)|(1ULL<<GPIO_USER3),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_POSEDGE
    };

    gpio_config(&io_config_but);
}

void User_Input_Task(void *params) {
    int pinNumber;

	Game *gameStruct = (Game *)params;

    while (1) {
        if (xQueueReceive(interuptQueue, &pinNumber, portMAX_DELAY)) {
			if(xSemaphoreTake(xMutexSemaphore_Game, portMAX_DELAY)) {
				switch(pinNumber) {
					case GPIO_USER1:
						gameStruct->input1();
						ESP_LOGD("UserInput","Button1 pressed");
						break;
					case GPIO_USER2:
						gameStruct->input2();
						ESP_LOGI("UserInput","Button2 pressed");
						break;
					case GPIO_USER3:
						gameStruct->input3();
						ESP_LOGI("UserInput","Button3 pressed");
						break;
					default:
						ESP_LOGE("UserInput","INPUT not recognized, pinNumber: %u",pinNumber);
						break;
				}
			xSemaphoreGive(xMutexSemaphore_Game);
		}

            vTaskDelay(pdMS_TO_TICKS(150));
            gpio_intr_enable(pinNumber);
        }    
    }
}

void Video_Output_Task(void *params) {
	Game *gameStruct = (Game *)params;

	while(true) {
		if(xSemaphoreTake(xMutexSemaphore_Game, portMAX_DELAY)) {
			gameStruct->video();
			xSemaphoreGive(xMutexSemaphore_Game);
		}
	}
}