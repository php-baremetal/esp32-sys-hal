# Changelog

## 1.0.3

- Fixed: `bm_sys_hal_mem_largest()` (`sys_psram_largest_free()`) walked every block of the
  heap with the heap lock held and the interrupts of the calling core off for 7-17 ms on 8 MB of PSRAM. An RGB
  panel whose interrupt runs on that core lost its refills, and the picture rolled when a program read the largest
  free PSRAM block. It now finds the size with a short search of allocate-and-free tries, each holding the lock
  for microseconds, so interrupts are no longer held off for milliseconds. The value is the same.

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
