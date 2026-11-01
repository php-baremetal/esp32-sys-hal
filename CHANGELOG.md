# Changelog

## 1.0.2

- Fixed: `bm_sys_hal_light_sleep()` with a wake pin set no pull on it, so a pin with nothing connected floated
  and woke the chip at random. The pin is now an input with the pull opposite to the wake level, and is taken
  back from the RTC domain (where a deep-sleep wake pin is left after the wake, and read as low).
- Fixed: `bm_sys_hal_light_sleep()` returned `"other"` when the chip had not slept (the executor queue stayed
  full, or the sleep could not start), the same as a real wake-up from another source. It now returns `"none"`.

## 1.0.1

- Changed: the author's contact email.
- Docs: projects use `"sdk": "esp32@^2.0"`.

## 1.0.0

Initial release.
