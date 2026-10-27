/*
   +----------------------------------------------------------------------+
   | php-baremetal: esp32-sys-hal                                         |
   +----------------------------------------------------------------------+
   | Copyright (c) 2026 Gianfrancesco Aurecchia                           |
   +----------------------------------------------------------------------+
   | This source file is subject to the MIT license, that is bundled with |
   | this package in the file LICENSE.                                    |
   +----------------------------------------------------------------------+
   | Authors: Gianfrancesco Aurecchia <gianfri@php-baremetal.com>         |
   +----------------------------------------------------------------------+
*/

/* ESP32 sys HAL for ext-sys: restart and deep sleep go through the host's stop (phpbm.h). */
#include <stdio.h>
#include "bm_sys_hal.h"
#include "phpbm.h"
#include "executor.h"
#include "sdkconfig.h"
#include "soc/soc_caps.h"
#include "driver/gpio.h"
#include "esp_sleep.h"
#include "esp_heap_caps.h"
#include "esp_timer.h"
#include "esp_system.h"
#include "esp_chip_info.h"
#include "esp_mac.h"
#include "esp_idf_version.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

void bm_sys_hal_delay_ms(long ms)
{
    if (ms < 0) {
        ms = 0;
    }
    vTaskDelay(pdMS_TO_TICKS(ms));
}

int64_t bm_sys_hal_now_us(void) { return esp_timer_get_time(); }

/* The PHP task (stack in PSRAM) never restarts or sleeps itself: app_main does, after the stop. */
void bm_sys_hal_restart(void)
{
    phpbm_after_stop_t after = { .action = PHPBM_AFTER_STOP_RESTART_NOW, .wake_pin = -1 };
    phpbm_request_after_stop(&after);
}

void bm_sys_hal_deep_sleep(int64_t ms, int pin, bool wake_low)
{
    phpbm_after_stop_t after = {
        .action = PHPBM_AFTER_STOP_DEEP_SLEEP,
        .sleep_us = (uint64_t) ms * 1000,
        .wake_pin = pin,
        .wake_level = wake_low ? 0 : 1,
    };
    phpbm_request_after_stop(&after);
}

const char *bm_sys_hal_wake_pin_error(int pin)
{
    static char error[96];
    if (!GPIO_IS_VALID_GPIO(pin)) {
        snprintf(error, sizeof(error), "must be a GPIO of this chip (GPIO %d does not exist)", pin);
        return error;
    }
#if SOC_PM_SUPPORT_EXT1_WAKEUP && SOC_RTCIO_INPUT_OUTPUT_SUPPORTED
    if (!esp_sleep_is_valid_wakeup_gpio(pin)) {
        snprintf(error, sizeof(error), "must be an RTC/LP GPIO (GPIO %d cannot wake the chip from deep sleep)", pin);
        return error;
    }
    return NULL;
#else
    return "must be null: this chip cannot wake from deep sleep on a pin";
#endif
}

typedef struct {
    int64_t ms;
    int pin;
    bool wake_low;
    const char *cause;
} light_sleep_t;

/* Runs on the executor task: esp_light_sleep_start() suspends the cache, which the PSRAM PHP stack cannot take. */
static esp_err_t light_sleep_run(void *arg)
{
    light_sleep_t *op = arg;
    op->cause = "other";
    esp_sleep_disable_wakeup_source(ESP_SLEEP_WAKEUP_ALL);
    if (op->ms > 0) {
        esp_sleep_enable_timer_wakeup((uint64_t) op->ms * 1000);
    }
    bool pin_set = false;
    if (op->pin >= 0) {
        pin_set = gpio_wakeup_enable((gpio_num_t) op->pin, op->wake_low ? GPIO_INTR_LOW_LEVEL : GPIO_INTR_HIGH_LEVEL) == ESP_OK
                  && esp_sleep_enable_gpio_wakeup() == ESP_OK;
    }
    if (esp_light_sleep_start() == ESP_OK) {
        uint32_t causes = esp_sleep_get_wakeup_causes();
        if (causes & BIT(ESP_SLEEP_WAKEUP_GPIO)) {
            op->cause = "gpio";
        } else if (causes & BIT(ESP_SLEEP_WAKEUP_TIMER)) {
            op->cause = "timer";
        }
    } else if (pin_set && gpio_get_level((gpio_num_t) op->pin) == (op->wake_low ? 0 : 1)) {
        op->cause = "gpio";
    }
    esp_sleep_disable_wakeup_source(ESP_SLEEP_WAKEUP_ALL);
    if (pin_set) {
        gpio_wakeup_disable((gpio_num_t) op->pin);
    }
    return ESP_OK;
}

const char *bm_sys_hal_light_sleep(int64_t ms, int pin, bool wake_low)
{
    light_sleep_t op = { .ms = ms, .pin = pin, .wake_low = wake_low, .cause = "other" };
    executor_run_sync(light_sleep_run, &op, 1000);
    return op.cause;
}

const char *bm_sys_hal_light_wake_pin_error(int pin)
{
    static char error[96];
    if (!GPIO_IS_VALID_GPIO(pin)) {
        snprintf(error, sizeof(error), "must be a GPIO of this chip (GPIO %d does not exist)", pin);
        return error;
    }
    return NULL;
}

const char *bm_sys_hal_wake_cause(void) { return phpbm_wake_cause(); }

const char *bm_sys_hal_reset_reason(void)
{
    switch (esp_reset_reason()) {
        case ESP_RST_POWERON:   return "poweron";
        case ESP_RST_EXT:       return "external";
        case ESP_RST_SW:        return "software";
        case ESP_RST_PANIC:     return "panic";
        case ESP_RST_INT_WDT:   return "int_wdt";
        case ESP_RST_TASK_WDT:  return "task_wdt";
        case ESP_RST_WDT:       return "wdt";
        case ESP_RST_DEEPSLEEP: return "deepsleep";
        case ESP_RST_BROWNOUT:  return "brownout";
        case ESP_RST_SDIO:      return "sdio";
        case ESP_RST_USB:       return "usb";
        case ESP_RST_JTAG:      return "jtag";
        default:                return "unknown";
    }
}

const char *bm_sys_hal_chip_model(void)
{
    esp_chip_info_t info;
    esp_chip_info(&info);
    switch (info.model) {
        case CHIP_ESP32:   return "ESP32";
        case CHIP_ESP32S2: return "ESP32-S2";
        case CHIP_ESP32S3: return "ESP32-S3";
        case CHIP_ESP32C3: return "ESP32-C3";
        case CHIP_ESP32C2: return "ESP32-C2";
        case CHIP_ESP32C6: return "ESP32-C6";
        case CHIP_ESP32H2: return "ESP32-H2";
        case CHIP_ESP32P4: return "ESP32-P4";
        default:           return "unknown";
    }
}

int bm_sys_hal_cpu_freq_mhz(void) { return CONFIG_ESP_DEFAULT_CPU_FREQ_MHZ; }

void bm_sys_hal_mac(uint8_t out[6]) { esp_read_mac(out, ESP_MAC_WIFI_STA); }

const char *bm_sys_hal_sdk_version(void) { return esp_get_idf_version(); }

static uint32_t cap_of(bm_sys_mem_t which)
{
    return which == BM_SYS_MEM_PSRAM ? MALLOC_CAP_SPIRAM : MALLOC_CAP_INTERNAL;
}
size_t bm_sys_hal_mem_free(bm_sys_mem_t which)    { return heap_caps_get_free_size(cap_of(which)); }
size_t bm_sys_hal_mem_total(bm_sys_mem_t which)   { return heap_caps_get_total_size(cap_of(which)); }
size_t bm_sys_hal_mem_largest(bm_sys_mem_t which) { return heap_caps_get_largest_free_block(cap_of(which)); }
