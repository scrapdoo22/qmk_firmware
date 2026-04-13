# NuPhy Kick75

![NuPhy Kick75](https://i.imgur.com/placeholder.jpg)

A 75% mechanical keyboard with knob, RGB matrix, and 2.4G/BLE wireless support.

* Keyboard Maintainer: [NuPhy](https://nuphy.com/)
* Hardware Supported: NuPhy Kick75 (STM32F072)
* Hardware Availability: [nuphy.com](https://nuphy.com/)

Make example for this keyboard (after setting up your build environment):

    make nuphy/kick75/ansi:default

Flashing example for this keyboard:

    make nuphy/kick75/ansi:default:flash

See the [build environment setup](https://docs.qmk.fm/#/getting_started_build_tools) and the [make instructions](https://docs.qmk.fm/#/getting_started_make_guide) for more information. Brand new to QMK? Start with our [Complete Newbs Guide](https://docs.qmk.fm/#/newbs).

## Bootloader

Enter the bootloader:

* **Physical reset**: Hold Esc while plugging in USB
* **Keycode**: Use the `QK_BOOT` keycode (not mapped by default)
