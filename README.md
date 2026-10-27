# esp32-sys-hal

The ESP32 side of [ext-sys](package:php-baremetal/ext-sys): it implements the neutral sys HAL
(`bm_sys_hal.h`, shipped by ext-sys) with ESP-IDF, the SDK host component `phpbm` and
[esp32-executor](package:php-baremetal/esp32-executor). It adds no PHP API of its own.

| Property | Value |
| --- | --- |
| Type | `bm-hal` |
| Platform | `esp32` (`extra.baremetal.platform`) |
| Provides | `php-baremetal/sys-hal` |
| Requires | `php-baremetal/esp32-executor` |
| ESP-IDF components (`idf-requires`) | `heap`, `esp_timer`, `esp_system`, `esp_hw_support`, `esp_driver_gpio`, `freertos`, `phpbm` |

`phpbm` is not an ESP-IDF component from Espressif: it is the host component of the php-baremetal SDK, which
every phpbm firmware has. The HAL calls it to restart and to deep sleep after the program stops, and to read the
wake cause. It therefore only builds inside a phpbm project.

## Install

You do not require it. ext-sys requires the virtual `php-baremetal/sys-hal`, and conductor adds this package
(and esp32-executor with it) when the project's SDK is on the `esp32` platform (`"sdk": "esp32@^2.0"`). Require
the extension:

```bash
composer require php-baremetal/ext-sys
```

The functions are documented in [ext-sys](package:php-baremetal/ext-sys#docs/overview.md).

## What it does on ESP32

| ext-sys function | ESP32 implementation |
| --- | --- |
| `sys_delay()`, `delay()` | `vTaskDelay()`; the firmware's tick is 1 ms |
| `sys_uptime_ms()`, `sys_micros()` | `esp_timer_get_time()` |
| `sys_restart()` | `phpbm_request_after_stop()` with a restart: the script ends like `exit()`, then `app_main` calls `esp_restart()` |
| `sys_deep_sleep()` | `phpbm_request_after_stop()` with a deep sleep: after the stop `app_main` arms the timer and the ext1 pin wake, waits 100 ms, then `esp_deep_sleep_start()` |
| `sys_light_sleep()` | `esp_light_sleep_start()` on the executor task, with timer wake and `gpio_wakeup_enable()` level wake |
| `sys_wake_cause()` | the cause the host read once at boot |
| `sys_reset_reason()` | `esp_reset_reason()` |
| `sys_chip_model()` | `esp_chip_info()` |
| `sys_cpu_freq_mhz()` | `CONFIG_ESP_DEFAULT_CPU_FREQ_MHZ` |
| `sys_mac()` | `esp_read_mac()`, Wi-Fi station address |
| `sys_idf_version()` | `esp_get_idf_version()` |
| memory functions | `heap_caps_get_free_size()`, `heap_caps_get_total_size()`, `heap_caps_get_largest_free_block()` on `MALLOC_CAP_INTERNAL` or `MALLOC_CAP_SPIRAM` |

What users notice:

- **The PHP task never restarts or sleeps the chip.** Restart and deep sleep run in `app_main` after PHP has shut
  down and the storage is unmounted; light sleep runs on the executor task, because entering it suspends the
  cache and the PHP task's stack is in PSRAM.
- **Light sleep blocks the executor** for its whole duration (background I2C samplers, NVS writes, `Bus::CORE1`
  buses wait). The call waits at most 1000 ms for a free slot in the executor queue; if none frees up, it returns
  `"other"` without sleeping.
- **Wake pins.** Deep sleep accepts only pins `esp_sleep_is_valid_wakeup_gpio()` accepts: RTC GPIOs on ESP32-S3,
  LP GPIOs on ESP32-P4. The pin gets the opposite pull during the sleep. Light sleep accepts any valid GPIO and
  returns `"gpio"` at once if the pin is already at the wake level.
- **Console.** On boards whose console is the USB-Serial-JTAG, the port drops during light and deep sleep.

## License

MIT. The package also ships `AI_USAGE.md` and a `CHANGELOG.md`.
