/**
 * This example takes a picture every 5s and print its size on serial monitor.
 */

// =============================== SETUP ======================================

// 1. Board setup (Uncomment):
// #define BOARD_WROVER_KIT
// #define BOARD_ESP32CAM_AITHINKER

/**
 * 2. Kconfig setup
 * 
 * If you have a Kconfig file, copy the content from
 *  https://github.com/espressif/esp32-camera/blob/master/Kconfig into it.
 * In case you haven't, copy and paste this Kconfig file inside the src directory.
 * This Kconfig file has definitions that allows more control over the camera and
 * how it will be initialized.
 */

/**
 * 3. Enable PSRAM on sdkconfig:
 * 
 * CONFIG_ESP32_SPIRAM_SUPPORT=y
 * 
 * More info on
 * https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/kconfig.html#config-esp32-spiram-support
 */

// ================================ CODE ======================================

#include <esp_log.h>
#include <esp_system.h>
#include <nvs_flash.h>
#include <sys/param.h>
#include <string.h>
#include "SSD1306.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "esp_camera.h"

#define BOARD_WROVER_KIT 1

// WROVER-KIT PIN Map
//#ifdef BOARD_WROVER_KIT

//#define CAM_PIN_PWDN -1  //power down is not used
//#define CAM_PIN_RESET -1 //software reset will be performed
//#define CAM_PIN_XCLK 21
//#define CAM_PIN_SIOD 26
//#define CAM_PIN_SIOC 27

//#define CAM_PIN_D7 35
//#define CAM_PIN_D6 34
//#define CAM_PIN_D5 39
//#define CAM_PIN_D4 36
//#define CAM_PIN_D3 19
//#define CAM_PIN_D2 18
//#define CAM_PIN_D1 5
//#define CAM_PIN_D0 4
//#define CAM_PIN_VSYNC 25
//#define CAM_PIN_HREF 23
//#define CAM_PIN_PCLK 22

//#endif

// ESP32Cam (AiThinker) PIN Map
#ifdef BOARD_ESP32CAM_AITHINKER

#define CAM_PIN_PWDN 32
#define CAM_PIN_RESET -1 //software reset will be performed
#define CAM_PIN_XCLK 0
#define CAM_PIN_SIOD 26
#define CAM_PIN_SIOC 27

#define CAM_PIN_D7 35
#define CAM_PIN_D6 34
#define CAM_PIN_D5 39
#define CAM_PIN_D4 36
#define CAM_PIN_D3 21
#define CAM_PIN_D2 19
#define CAM_PIN_D1 18
#define CAM_PIN_D0 5
#define CAM_PIN_VSYNC 25
#define CAM_PIN_HREF 23
#define CAM_PIN_PCLK 22

#endif

static const char *TAG = "example:take_picture";

static camera_config_t camera_config = {
	.pin_pwdn = PWDN_GPIO_NUM,
	.pin_reset = RESET_GPIO_NUM,
	.pin_xclk = XCLK_GPIO_NUM, 
	.pin_sscb_sda = SIOD_GPIO_NUM,  
	.pin_sscb_scl = SIOC_GPIO_NUM,
	.pin_d7 = Y9_GPIO_NUM,
	.pin_d6 = Y8_GPIO_NUM,
	.pin_d5 = Y7_GPIO_NUM, 
	.pin_d4 = Y6_GPIO_NUM,
	.pin_d3 = Y5_GPIO_NUM,
	.pin_d2 = Y4_GPIO_NUM,  
	.pin_d1 = Y3_GPIO_NUM,
	.pin_d0 = Y2_GPIO_NUM,
	.pin_vsync = VSYNC_GPIO_NUM,
	.pin_href = HREF_GPIO_NUM,
	.pin_pclk = PCLK_GPIO_NUM,
	
	.xclk_freq_hz = 20000000,
	.ledc_timer = LEDC_TIMER_0,
	.ledc_channel = LEDC_CHANNEL_0,
	
	.pixel_format = PIXFORMAT_GRAYSCALE,
	.frame_size = FRAMESIZE_CUSTOMIZE,
	.jpeg_quality = 12,
	.fb_count = 1,
};

byte x,y;
uint8_t oldpixel, newpixel;
int quanterror;
static esp_err_t init_camera()

esp_err_t camera_init()
{
	//initialize the camera
	esp_err_t err = esp_camera_init(&camera_config);
	if (err != ESP_OK) 
	{
		Serial.print("Camera Init Failed");
		return err;
	}
	sensor_t * s = esp_camera_sensor_get();
	//initial sensors are flipped vertically and colors are a bit saturated
	if (s->id.PID == OV2640_PID) 
	{
		s->set_vflip(s, 1);//flip it back
		s->set_brightness(s, 1);//up the blightness just a bit
		s->set_contrast(s, 1);
	}
	Serial.print("Camera Init OK");
	return ESP_OK;
}

void my_camera_show(void)
{
	camera_fb_t *fb = esp_camera_fb_get();
	int i = 0;
	for(y = 0; y < SCREEN_HEIGHT; y++)
	{
		for(x = 0;x < SCREEN_WIDTH; x++)
		{
			oldpixel = fb->buf[i]; //保持原始的灰度值
			newpixel = (255*(oldpixel >> 7)); //门槛值128
			fb->buf[i] = newpixel; //缓冲区现在是单通道，0或255
			// floyd-steignburg抖动算法
			quanterror = oldpixel - newpixel; //像素之间的误差
			//将此错误分发给相邻像素：
			//右
			if(x < SCREEN_WIDTH-1)//边界检查...
			{
				fb->buf[(x + 1)+(y * SCREEN_WIDTH)] += ((quanterror * 7)>> 4);
			}
			// 左下
			if((x > 1) && (y < SCREEN_HEIGHT-1))//边界检查...
			{
				fb->buf [(x-1)+((y + 1)* SCREEN_WIDTH)] == ((quanterror * 3)>> 4); 
			}
			//下
			if(y < 63)//边界检查...
			{
				fb->buf [(x)+((y + 1)* SCREEN_WIDTH)] == ((quanterror * 5)>> 4);
			}
			// 右下
			if((x < SCREEN_WIDTH-1) && (y < SCREEN_HEIGHT-1))//边界检查...
			{
				fb->buf [(x + 1) + ((y + 1)* SCREEN_WIDTH)] == (quanterror >> 4);
			}
			// 画这个像素
			switch(fb->buf[i]%2)
			{
			case 0:
				display.setColor(BLACK);
				break;
			case 1:
				display.setColor(WHITE);
				break;
			case 2:
				display.setColor(INVERSE);
				break;
			}
			display.setPixel(x, y);
			i++;
		}
	}
	display.display();
	esp_camera_fb_return(fb);
}


void setup() 
{
	Serial.begin(115200);
	display.init();
	camera_init();
	Serial.setDebugOutput(true);
	Serial.println();
	Serial.print("sys is 1...");
	Serial.print("sys is running!");
}

void loop() 
{
   my_camera_show();
}
