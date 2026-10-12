# Flashing the Atlantis SLT MCU

The STM32F746 SWD port is wired to channel A of the FT4232H (U42) behind
the USB-C debug port J39, and to the 4-pin header J61. The MCU console
(UART8, 115200 8N1) is FT4232H channel C and header J70. Both are powered
from the USB cable, not from the board.

| Header | Pin 1 | Pin 2 | Pin 3 | Pin 4 |
| --- | --- | --- | --- | --- |
| J61 SWD | SWCLK | SWDIO | 3V3 | GND |
| J70 UART | MCU TX | MCU RX | 3V3 | GND |

## Via the USB debug port

Connect a USB-C cable to J39. OpenOCD drives SWD over the FT4232H with the
board config in this directory:

```
west flash --runner openocd
```

or with the files from a build:

```
openocd -f boards/tenstorrent/atlantis_slt_mcu/support/openocd.cfg \
  -c 'init; halt; flash write_image erase build/mcuboot/zephyr/zephyr.hex; flash write_image erase build/wallabmc/zephyr/zephyr.signed.hex; reset; exit'
```

The `wallabmc-firmware-atlantis_slt_mcu` artifact of a GitHub Actions run
holds the same images as `mcuboot.hex` and `wallabmc.signed.hex`.

The board wires the FT4232H data output (ADBUS1) straight to SWDIO and the
data input (ADBUS2) through 470R, the mirror of the usual OpenOCD
"resistor hack". The dummy `SWDIO_OE` signal in `openocd.cfg` makes the
ftdi driver switch ADBUS1 to an input while the target drives SWDIO; this
path is untested on hardware. If the FT4232H EEPROM on the board programs
a non-default USB ID, adjust `ftdi vid_pid` in `openocd.cfg`.

## Via the SWD header

Any SWD probe on J61 works with the stock OpenOCD STM32F7 target config:

```
openocd -f interface/stlink.cfg -f target/stm32f7x.cfg \
  -c 'init; halt; flash write_image erase build/mcuboot/zephyr/zephyr.hex; flash write_image erase build/wallabmc/zephyr/zephyr.signed.hex; reset; exit'
```
