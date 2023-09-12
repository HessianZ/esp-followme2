//
// Created by Hessian on 2023/9/12.
//

#include <esp_log.h>
#include <driver/gpio.h>
#include <esp_timer.h>
#include "page_led.h"
#include "led_strip.h"

static const char *TAG = "PAGE_LED";

LV_FONT_DECLARE(font_OPPOSans_L_16);
static const lv_font_t *main_font = &font_OPPOSans_L_16;

#define NEOPIX_GPIO GPIO_NUM_33
#define NEOPIX_PWR_GPIO GPIO_NUM_34


static bool gpio_initialized = false;
static led_strip_handle_t led_strip = NULL;
static uint8_t led_color = 0;
static bool led_dir = true;
static esp_timer_handle_t periodic_timer;
static uint32_t r = 0, g = 0, b = 0;

static inline void led_breath(uint32_t *color)
{
    if (led_dir) {
        (*color) ++;
        if (*color == 0xff) {
            led_dir = false;
        }
    } else {
        (*color) --;
        if (*color == 0) {
            led_dir = true;
        }
    }
}

static void update_led_task(void *arg)
{
    switch (led_color) {
        case 0:
            led_breath(&r);
            break;
        case 1:
            led_breath(&g);
            break;
        case 2:
            led_breath(&b);
            break;
    }
    led_strip_set_pixel(led_strip, 0, r, g, b);
    led_strip_refresh(led_strip);
}

static void page_led_init(void)
{
    esp_err_t err;
    if (!gpio_initialized) {
        gpio_config_t io_conf = {
                .mode = GPIO_MODE_OUTPUT,
                .pin_bit_mask = (1ULL<<NEOPIX_PWR_GPIO),
                .intr_type = GPIO_INTR_DISABLE,
                .pull_down_en = 0,
                .pull_up_en = 0,
        };
        err = gpio_config(&io_conf);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "configure GPIO for NEOPIX_PWR failed");
        }
        gpio_initialized = true;
    }

    err = gpio_set_level(NEOPIX_PWR_GPIO, 1);

    if (err != ESP_OK) {
        ESP_LOGE(TAG, "gpio_set_level GPIO for NEOPIX_PWR failed");
    }

    ESP_LOGI(TAG, "NEOPIX_PWR ON");


/* LED strip initialization with the GPIO and pixels number*/
    led_strip_config_t strip_config = {
            .strip_gpio_num = NEOPIX_GPIO, // The GPIO that connected to the LED strip's data line
            .max_leds = 1, // The number of LEDs in the strip,
            .led_pixel_format = LED_PIXEL_FORMAT_GRB, // Pixel format of your LED strip
            .led_model = LED_MODEL_WS2812, // LED strip model
            .flags.invert_out = false, // whether to invert the output signal (useful when your hardware has a level inverter)
    };

    led_strip_rmt_config_t rmt_config = {
            .clk_src = RMT_CLK_SRC_DEFAULT, // different clock source can lead to different power consumption
            .resolution_hz = 10 * 1000 * 1000, // 10MHz
            .flags.with_dma = false, // whether to enable the DMA feature
    };

    ESP_ERROR_CHECK(led_strip_new_rmt_device(&strip_config, &rmt_config, &led_strip));


    const esp_timer_create_args_t periodic_timer_args = {
            .callback = &update_led_task,
            /* name is optional, but may help identify the timer when debugging */
            .name = "periodic"
    };

    ESP_ERROR_CHECK(esp_timer_create(&periodic_timer_args, &periodic_timer));
}

void page_led_render(lv_obj_t *parent)
{
    ESP_LOGD(TAG, "render start");

    if (parent == NULL) {
        return;
    }

    page_led_init();

    lv_obj_t *lab_text = lv_label_create(parent);
    lv_label_set_recolor(lab_text, true);
    lv_obj_set_style_text_font(lab_text, main_font, LV_PART_MAIN);
    lv_obj_set_align(lab_text, LV_ALIGN_CENTER);
    lv_label_set_text_fmt(lab_text, "#F2C161 LED控制#\n#CCCCCC 长按Boot0切换LED颜色#");

    // 开始呼吸灯 1000us = 1ms
    ESP_ERROR_CHECK(esp_timer_start_periodic(periodic_timer, 1000));

    ESP_LOGD(TAG, "render end");
}

void page_led_destroy()
{
    ESP_LOGD(TAG, "destroy start");

    ESP_ERROR_CHECK(gpio_set_level(NEOPIX_PWR_GPIO, 0));

    if (periodic_timer != NULL) {
        ESP_ERROR_CHECK(esp_timer_stop(periodic_timer));
        ESP_ERROR_CHECK(esp_timer_delete(periodic_timer));
        periodic_timer = NULL;
    }

    if (led_strip != NULL) {
        led_strip_clear(led_strip);
        led_strip_refresh(led_strip);
        led_strip_del(led_strip);
        led_strip = NULL;
    }

    ESP_LOGD(TAG, "destroy end");
}

void page_led_next_color()
{
    r = 0, g = 0, b = 0;
    led_dir = true;

    led_color ++;
    if (led_color > 2) {
        led_color = 0;
    }
}