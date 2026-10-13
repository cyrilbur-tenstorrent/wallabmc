.. SPDX-FileCopyrightText: © 2026 Tenstorrent USA, Inc.
..
.. SPDX-License-Identifier: Apache-2.0

Atlantis SLT BMC Hardware Reference
***********************************

This document maps the STM32F746ZGT6 pins to board signals on the
Tenstorrent Atlantis SLT board, based on the schematic Rev 0.997 (sheet 32
"STM32" and the sheets it references), and records what WallaBMC does with
them. Schematic signal names are used throughout; sheet numbers refer to
Rev 0.997.

**Schematic**: Atlantis SLT Rev 0.997 (CoreLab, 2026-10-08). The Tenstorrent
board review (ticket IPS-1761) is the source for the power-good and OTP
handshake requirements quoted below.

MCU overview
============

- **MCU**: STM32F746ZGT6 (U23, LQFP144), Cortex-M7 at 216 MHz, 1 MB flash,
  256 KB SRAM + 64 KB DTCM
- **Supply**: P3V3 from U50 (TPS548B28, 12 V to 3.3 V). U50 is enabled by a
  fixed divider, so the MCU is powered whenever 12 V is present and cannot
  power itself off. ATX standby power for the BMC was deferred to the EVB.
- **Clocks**: 8 MHz crystal Y12 (HSE), 32.768 kHz crystal Y13 (LSE, not yet
  enabled: the RTC runs from LSI until the crystal is verified)
- **RTC backup**: VBAT is the raw CR1220 coin cell in J62 (no diode-OR to
  3.3 V), shared with the SoC-side S35390A RTC on sheet 35. Without a cell
  the MCU RTC loses time on every power cycle.
- **BOOT0**: header J69. Jumper 1-2 boots from flash, 2-3 selects the ST
  system bootloader, no jumper leaves BOOT0 floating.
- **Reset**: button B3 on NRST
- **Board ID**: PD10/PD11 strapped low on SLT ("STM32ID: 00")

Debug and console
=================

The FT4232H U42 on USB-C port J39 provides:

.. list-table::
   :header-rows: 1

   * - Channel
     - Function
     - Notes
   * - A (ADBUS0-2, ADBUS5)
     - MCU SWD
     - ADBUS0 SWCLK, ADBUS1 (DO) to SWDIO via 470R, ADBUS2 (DI) to SWDIO.
       ADBUS5 is wired to PC0 (``ft232_gpio``), purpose undefined. See
       ``support/README.md``.
   * - B
     - unused
     -
   * - C (CDBUS0/1)
     - MCU console
     - UART8 (PE1 TX, PE0 RX), 115200 8N1. Zephyr console and shell.
   * - D
     - unused
     -

Headers: J61 SWD (1 SWCLK, 2 SWDIO, 3 3V3, 4 GND), J70 UART (1 MCU TX,
2 MCU RX, 3 3V3, 4 GND).

Pin map
=======

Implemented in WallaBMC
-----------------------

.. list-table::
   :header-rows: 1

   * - STM32 pin
     - Signal
     - DTS node / alias
     - Description
   * - PE1 / PE0
     - STM32_UART_TX / RX
     - ``uart8``
     - MCU console, FT4232 channel C and J70
   * - PD5 / PD6
     - STM_USART2_TX / RX
     - ``usart2`` / ``host-console-uart``
     - SoC UART0 through mux U82 (DMA on DMA1 streams 6/5)
   * - PD8 / PD9
     - STM_USART3_TX / RX
     - ``usart3``
     - SoC UART1 through mux U83 (enabled, unused by code)
   * - PD15
     - SYS_UART_SEL0
     - ``sys_uart_sel0`` / ``host-console-uart-muxsel``
     - UART mux S0, driven high while WallaBMC runs
   * - PD14
     - SYS_UART_SEL1
     - ``sys_uart_sel1`` + gpio-hog
     - UART mux S1, held low
   * - PG15
     - LED DS4
     - ``green_led`` / ``status-led``
     - Green LED, active high, Morse "OK" blinker
   * - PG8
     - button B2
     - ``user_button`` / ``reset-button``
     - Active low, internal pull-up. Hold 1 s to clear config
   * - PG5
     - SYS_RSTN_3V3
     - ``sys_rstn`` / ``reset-gpio-1``
     - SoC reset request, active high (see SoC reset)
   * - PE9
     - POWER_5V_EN
     - ``power_5v_en`` / ``power-gpio-1``
     - P5V enable (see Power)
   * - PF12/PF11/PF13/PF14
     - STM32_JTAG0_TCK/TMS/TDI/TDO
     - ``jtag0_*`` / ``jtagtck`` etc.
     - RCPU JTAG bit-bang (TCP port 7777), needs mux select 01
   * - PA1,PA2,PA7,PB0,PB1,PB11-PB13,PC1-PC5,PE2
     - ETH_MII_* / ETH_MDC / ETH_MII_MDIO
     - ``mac``, ``mdio``
     - MII to DP83826E PHY
   * - PE3
     - ETH_RSTN
     - ``eth_phy`` reset-gpios
     - PHY reset, active low
   * - PF1 / PF0
     - I2C6_SCL / SDA
     - ``i2c2``
     - TMP117 and M24M01 EEPROM, 4.7K pull-ups (R873/R874)
   * - PB8 / PB7
     - I2C2_SCL_3V3 / SDA_3V3
     - ``i2c1``
     - Level-shifted link to SoC I2C2 (enabled, unused)
   * - PE5, PE6, PE7
     - QSPI_MUX_EN, IN1, IN2
     - ``qspi_mux_*`` + gpio-hogs
     - Boot flash given to the SoC (all low)
   * - PE8, PG3, PG4
     - SDIO_MUX_EN, IN1, IN2
     - ``sdio_mux_*`` + gpio-hogs
     - SD slot S2 given to the SoC (all low)

Defined in the DTS, not yet used
--------------------------------

.. list-table::
   :header-rows: 1

   * - STM32 pin
     - Signal
     - DTS node
     - Direction
     - Description
   * - PE13
     - VCORE_0V8_EN
     - ``vcore_0v8_en``
     - Out
     - TPS546E25 x2 CNTL via J17 (0.8 V, 80 A)
   * - PE12
     - MPM54304_EN
     - ``mpm54304_en``
     - Out
     - DVDD_3V3, DVDD_1V8, AVDD_1V8, AVDD_0V8
   * - PE15
     - DDR_VDD_EN
     - ``ddr_vdd_en``
     - Out
     - TPS51488 VDD1 1.8 V and VDD2 1.05 V
   * - PB14
     - DDR_VDD2L_EN
     - ``ddr_vdd2l_en``
     - Out
     - TPS746 0.9 V (from VDD1)
   * - PE14
     - DDR_VDDQ_EN
     - ``ddr_vddq_en``
     - Out
     - TPS51488 VDDQ 0.5 V
   * - PE10
     - PCIE_VP_EN
     - ``pcie_vp_en``
     - Out
     - TPS628681 PCIE_0V8
   * - PE11
     - PCIE_VPH_EN
     - ``pcie_vph_en``
     - Out
     - TPS628682 PCIE_1V8
   * - PG6
     - PWR_GOOD_5V
     - ``pwr_good_5v``
     - In
     - P5V in regulation (high)
   * - PC7 / PC6
     - PG_PH1 / PG_PH2
     - ``pg_ph1`` / ``pg_ph2``
     - In
     - VCORE phases in regulation, high. PG_PH2 sits at the ~4.4 V LDO;
       PG_PH1 is clamped by LED DS2 to about 2 V (see Known issues)
   * - PG7
     - PWR_GOOD_LPDDR5
     - ``pwr_good_lpddr5``
     - In
     - VDD1 and VDD2 good (high, ~3.4 V level)
   * - PA10
     - PWR_GOOD_0V9
     - ``pwr_good_0v9``
     - In
     - DDR_VDD2L good (high)
   * - PG1 / PG2
     - PWR1_GD_3V3 / PWR2_GD_3V3
     - ``pwr1_gd`` / ``pwr2_gd`` + gpio-hogs
     - Out
     - Power-good handshakes to the SoC through 15K/18K dividers
       (push-pull only), held low
   * - PF2
     - OTP_PWRRDY_3V3
     - ``otp_pwrrdy`` + gpio-hog
     - Out
     - OTP power-ready handshake to the SoC through a 15K/18K divider,
       held low
   * - PG0
     - TJ_MAX_3V3
     - ``tj_max``
     - In
     - SoC output, presumably a junction temperature alarm (not documented)
   * - PA3, PA4, PD1, PD0
     - GPIO2/8/11/12_3V3
     - ``soc_gpio*``
     - Bidir
     - SoC GPIO_DATA[2/8/11/12] through auto-direction shifters. GPIO11/12
       also on header J53 pins 3/1
   * - PA5 / PA6
     - SYS_JTAG0_SEL0 / SEL1
     - ``sys_jtag0_sel*``
     - Out
     - RCPU JTAG mux select
   * - PA11 / PA12
     - SYS_JTAG1_SEL0 / SEL1
     - ``sys_jtag1_sel*``
     - Out
     - Ascalon JTAG mux select
   * - PG10/PG9/PG11/PG12/PG13
     - STM32_JTAG1_TCK/TMS/TDI/TDO/TRST
     - ``jtag1_*``
     - Out/In
     - Ascalon JTAG bit-bang
   * - PF15
     - STM32_JTAG0_TRST
     - ``jtag0_trst``
     - Out
     - RCPU JTAG TRST, active low, pulled up on both sides
   * - PF6-PF9, PB2, PB6
     - STM_QSPI_*
     - ``quadspi`` (disabled)
     - 
     - MCU access to the boot flash when the mux selects it
   * - PC8-PC12, PD2
     - STM_SDIO_*
     - ``sdmmc1`` (disabled)
     - 
     - MCU access to SD slot S2 when the mux selects it
   * - PA8 / PA9
     - SDIO0_DET / SDIO1_DET
     - ``sdio0_det`` / ``sdio1_det``
     - In
     - Card detect for slots S1 / S2, assumed active low
   * - PD3
     - Sensor_ALERT
     - ``tmp117`` alert-gpios
     - In
     - TMP117 alert, active low, 4.7K pull-up
   * - PD12 / PD13
     - I2C5_SCL / SDA
     - ``i2c4`` (disabled)
     - 
     - PMIC bus when J67/J68 are jumpered 1-2
   * - PF10
     - MPM54304_GPIO
     - ``mpm54304_gpio``
     - In
     - MPM54304 power good (open drain with the -0001 MTP, no board
       pull-up, internal pull-up enabled)
   * - PF3 / PF4
     - FT4232_GPIO2 / GPIO3
     - ``ft4232_gpio*``
     -
     - Spare lines from the SoC-side FT4232 U73 channel A
   * - PC0
     - FT232_GPIO
     - ``ft232_gpio``
     - In
     - MCU-side FT4232 U42 ADBUS5 through R815, purpose undefined; never
       drive it
   * - PD10 / PD11
     - STM32_ID0 / ID1
     - ``stm32_id*``
     - In
     - Board ID straps, both low on SLT
   * - PC14 / PC15
     - Y13
     - ``clk_lse`` (disabled)
     - 
     - 32.768 kHz crystal, enable once it is seen to start

Not connected or spare
----------------------

PA0, PB5, PB9, PB10, PB15, PD4, PD7, PF5 and PG14 only reach test points
M24-M32 on sheet 53. The eFuse enable from U42 ADBUS4 is behind the
unpopulated 0R link R814. PC13 is unconnected. PA15 and PB4 have 4.7K
pull-ups only.

Ethernet
========

- **PHY**: TI DP83826E (U25) in BASIC pin mode, 25 MHz crystal, RJ45 J20
- **Interface**: MII (straps "100M Auto negotiation / MII MAC mode / Full
  Duplex"). RX_ER, CRS and COL are not wired to the MCU; fine for full
  duplex.
- **MDIO address**: no address straps are fitted, so the address is the
  datasheet default. SNLS647I rev I gives PHY_ADD0 = 1 (RX_D3 has an
  internal pull-up) so the DTS uses address 1. Older datasheet revisions
  and app note SNLA338A say 0. **Verify at bring-up** by reading
  PHYIDR1/PHYIDR2 (0x2000/0xA131) at addresses 0 and 1; a wrong address
  logs "No PHY found" and leaves the BMC without network.
- **Reset**: ETH_RSTN (PE3), active low, 4.7K pull-up. The DTS asserts it
  for 100 us and waits 5 ms before the first MDIO access, per the datasheet.
- **Interrupt**: ETH_MII_INTR (PE4), open drain, active low, 4.7K pull-up.
  Not in the DTS: the generic ``ethernet-phy`` driver polls.
- **ModeSelect caveat**: pin 1 is pulled down through 4.7K (R679) instead
  of being shorted to ground as the datasheet asks. If the internal pull-up
  is the 10K used on strap pins, the node sits near 1.06 V, between the
  bootstrap VIL and VIH, so the mode is not guaranteed; measure it. In
  ENHANCED mode the MII and address straps give the same result, pin 21
  becomes PWRDN/INT (held high by R225) and pin 31 a 25 MHz CLKOUT on the
  LED1 net.

Host console UART mux
=====================

SoC UART0 and UART1 are 1.8 V signals level shifted by U89 and then routed
by two SN74CB3Q3253 muxes (U82 for UART0, U83 for UART1) that share the
select lines:

.. list-table::
   :header-rows: 1

   * - SEL1 (PD14) : SEL0 (PD15)
     - UART0 / UART1 connected to
   * - 0 0
     - SoC-side FT4232 U73 channels C / D (USB-C J71)
   * - 0 1
     - MCU USART2 / USART3
   * - 1 0
     - Headers J77 / J78 (1 TX in, 2 RX out, 3 3V3, 4 GND)
   * - 1 1
     - nothing (SoC RX lines float)

Both lines have 4.7K pull-ups and a DIP switch (SW4) that grounds them
through 499R when ON. WallaBMC hogs SEL1 low and drives SEL0 high as
``host-console-uart-muxsel`` at init, so while the BMC runs the SoC
console (UART0) is on the console logger, console bridge (TCP port 22) and
web terminal, and the USB channels see nothing. The MCU pushes about 7 mA
into a closed SW4 section; set SW4 OFF when the BMC owns the selects.

SoC reset
=========

SYS_RSTN_3V3 (PG5) and FT4232__SYS_RSTN_3V3 (U73 ADBUS4) are diode-ORed
into Q1, which pulls the SoC RESETN low: **either input high asserts
reset**. R589 pulls PG5 high, so the SoC is held in reset whenever the MCU
is unpowered, in reset, or has not configured the pin. WallaBMC maps the
pin as ``reset-gpio-1`` with ``GPIO_ACTIVE_HIGH``: ``reset_init()`` releases
the SoC at BMC start and ``reset`` pulses it for one second. SYS_RSTN also
asserts PERST# on the three PCIe slots through D9-D11. RESETN has an
internal pull-up in the SoC.

U73 ADBUS4 has no series resistor or pull-down. It is DTR# in the
FT4232H's default UART mode and sits high unless the host actively drives
it low (a port open with DTR asserted, or an MPSSE layout with that bit
low), so a USB-C cable on J71 to an idle host can hold the SoC in reset
no matter what PG5 does. See Known issues.

Power
=====

What comes up by itself
-----------------------

ATX 12 V (J30) passes fuse F1 and the TPS259830 eFuse U54 (UVLO about
10.8 V, ILIM 18 A) to P12V ("12V Ready" LED DS3), and U50 makes P3V3 for
the MCU, PHY and the QSPI flash. Both FT4232H bridges (U42 on J39, U73 on
J71) run from their own USB VBUS, so they enumerate with or without 12 V.
The eFuse enable is reachable only from push-button B1 and the SoC-side
FT4232 U73 (BDBUS7, R816 fitted) through MOSFET Q12, so **BDBUS7 high
disables the eFuse**; the MCU-side path R814 is not populated. ATX PS_ON
is a jumper on J31. So the MCU cannot power-cycle the board or itself:
"power off" means dropping the SoC rails.

SoC rails controlled by the MCU
-------------------------------

Every enable has a pull-down, so all of these rails stay off until the
firmware drives them. Documented order (schematic sheet 56):
P12V > P3V3 > P5V > VCORE_0V8 > DVDD_3V3 / DDR / PCIe, with LPDDR5
VDD1 > VDD2 > 0.9 V > VDDQ (sheet 44).

.. list-table::
   :header-rows: 1

   * - Step
     - Enable
     - Regulator
     - Rails
     - Power good
   * - 1
     - POWER_5V_EN (PE9)
     - U57 TPS548B28
     - P5V 10 A (USB, HDMI, DDR regulators)
     - PWR_GOOD_5V (PG6)
   * - 2
     - VCORE_0V8_EN (PE13)
     - U22 + U2 TPS546E25
     - VCORE_0V8 80 A
     - PG_PH1 (PC7), PG_PH2 (PC6)
   * - 3
     - MPM54304_EN (PE12)
     - U53 MPM54304
     - DVDD_1V8, DVDD_3V3, AVDD_1V8, AVDD_0V8
     - MPM54304_GPIO (PF10)
   * - 4
     - DDR_VDD_EN (PE15)
     - U26 TPS51488
     - VDD1 1.8 V, VDD2 1.05 V
     - PWR_GOOD_LPDDR5 (PG7)
   * - 5
     - DDR_VDD2L_EN (PB14)
     - U27 TPS746
     - DDR_VDD2L 0.9 V
     - PWR_GOOD_0V9 (PA10)
   * - 6
     - DDR_VDDQ_EN (PE14)
     - U26 TPS51488
     - VDDQ 0.5 V
     - none
   * - 7
     - PCIE_VP_EN (PE10)
     - U51 TPS628681
     - PCIE_0V8
     - none (I2C status only)
   * - 8
     - PCIE_VPH_EN (PE11)
     - U52 TPS628682
     - PCIE_1V8
     - none (I2C status only)
   * - 9
     - OTP_PWRRDY (PF2), PWR1_GD (PG1), PWR2_GD (PG2)
     - 
     - handshakes to the SoC
     - 

Steps 3 to 8 follow the sheet 56 graph, which does not order the MPM54304,
DDR and PCIe groups relative to each other; that order must come from the
SoC power-sequencing specification.

Constraints from the board review (IPS-1761): OTP_PWRRDY, PWR1_GD and
PWR2_GD must be driven low before DVDD_1V8 ramps and asserted only once
VDD/VDDIO are good (200 ns setup for PWRRDY). No inter-rail delays are
documented anywhere; the SoC power-sequencing specification is still
outstanding. The VCORE enable reaches the regulators through jumper J17.
As drawn, the printed default ("3-4 short") would tie CNTL to ground, so
STM32 control needs the VCORE_0V8_EN pin (2 as drawn) linked to a CNTL
pin; check the real header.

**WallaBMC today** maps only POWER_5V_EN to ``power-gpio-1``; ``power on``
raises P5V and nothing else, the three handshakes are held low by gpio-hogs,
and the SoC reset is released at BMC start.
Bringing the SoC up needs a board-specific sequencer in ``power.c`` that
walks the table above with power-good waits, drives the handshakes, and
holds SYS_RSTN until the end. All the GPIO nodes exist in the DTS.

PMIC I2C
--------

The regulators share one I2C/PMBus bus (schematic I2C4): TPS546E25 at
0x0D (PMBus), TPS628681 at 0x49, TPS628682 at 0x41, MPM54304 at 0x68.
Headers J67 (SCL) and J68 (SDA) select the master: 1-2 puts the MCU's I2C5
(STM32 ``i2c4``, PD12/PD13) on the bus, 2-3 the SoC's I2C4. The pull-ups
R179/R180 to the TPS546E25 4.4 V LDO are not fitted (the MPM54304 pins are
rated to 4 V), so in the 1-2 position the only pull-ups are the MCU-side
R204/R205 (4.7K to P3V3) and in the 2-3 position only the U67 shifter's
internal ones while DVDD_1V8 is up.

JTAG
====

Two SoC TAPs, each behind SN74CB3Q3253 muxes with the same select
encoding as the UART mux (SEL1:SEL0 00 FT4232 U73, 01 MCU, 10 header,
11 none) and a DIP switch that forces 00 when ON:

.. list-table::
   :header-rows: 1

   * - Chain
     - Sheet
     - MCU pins (TCK/TMS/TDI/TDO/TRST)
     - Select
     - Switch
     - Header
   * - JTAG0 (RCPU)
     - 51
     - PF12/PF11/PF13/PF14/PF15
     - PA5 (SEL0), PA6 (SEL1)
     - SW2
     - J40
   * - JTAG1 (Ascalon)
     - 50
     - PG10/PG9/PG11/PG12/PG13
     - PA11 (SEL0), PA12 (SEL1)
     - SW3
     - J73

WallaBMC's ``jtagtck``/``jtagtms``/``jtagtdi``/``jtagtdo`` aliases point at
JTAG0. The select lines are not driven by the firmware yet. With both SW2
sections OFF the selects idle at 11 and no source reaches the SoC, so until
``jtag.c`` drives PA6 low and PA5 high on client connect, give JTAG0 to the
MCU by hand: SW2 section on SEL1 ON, section on SEL0 OFF. Hogging the
selects was avoided because it would cut the USB JTAG path that bring-up
relies on.

Boot flash and SD muxes
=======================

- **QSPI**: the 16 MiB MX25L12833F (U19) sits behind TS3A27518E U21.
  EN (PE5) is pulled up, so the flash is isolated from everyone until the
  MCU drives it low; IN1/IN2 (PE6/PE7) both low route it to the SoC, both
  high to the MCU QUADSPI. Never drive IN1 and IN2 differently (it splits
  the bus). WallaBMC hogs all three low so the SoC can boot from NOR as
  soon as it is released from reset, in MCUboot as well as the app. The
  ``quadspi`` node is defined but disabled; programming the flash from the
  BMC is a follow-up.
- **SD**: slot S1 is wired straight to SoC SDIO0 (the MCU sees only its
  card detect on PA8). Slot S2 is behind U20 with the same control scheme
  (PE8 EN, PG3/PG4 IN); WallaBMC hogs them low so S2 belongs to SoC SDIO1.
  U20, both slots and their pull-ups are on DVDD_3V3 and are dead until
  MPM54304 is enabled, so the card-detect inputs read low until then.

I2C buses
=========

.. list-table::
   :header-rows: 1

   * - Schematic name
     - STM32 peripheral
     - Pins
     - Devices
   * - I2C6
     - I2C2 (``i2c2``)
     - PF1 SCL, PF0 SDA
     - TMP117 (0x49, alert PD3), M24M01 EEPROM (128 KiB at 0x56; the
       driver uses 0x57 for the upper half; write control tied low). 4.7K
       pull-ups R873/R874, fast mode.
   * - I2C2
     - I2C1 (``i2c1``)
     - PB8 SCL, PB7 SDA
     - SoC I2C2 through auto-direction shifter U67, 4.7K pull-ups both
       sides. Protocol and master undefined.
   * - I2C5
     - I2C4 (``i2c4``, disabled)
     - PD12 SCL, PD13 SDA
     - Headers J67/J68 (PMIC bus when jumpered 1-2)

Level shifters
==============

All TXS0108E/TXS0102 shifters between the SoC and the MCU have their OE
pulled to DVDD_1V8. Until MPM54304 is enabled, TJ_MAX_3V3 and
GPIO2/8/11/12_3V3 are undriven on the MCU side; firmware must ignore them
until DVDD_1V8 is up, and must not add pull-downs (they would fight the
shifters' internal pull-ups once enabled).

PWR1_GD, PWR2_GD and OTP_PWRRDY do not go through a shifter: 15K/18K
dividers (R864-R867 on sheet 46, R870/R871 on sheet 47) replace the
unfitted 0R links into U61/U85, so the SoC sees 1.8 V when the
MCU drives 3.3 V and 0 V when the pin is low or floating (MCU unpowered or
in reset). The pins must be push-pull: open drain would never reach a
high. The shifters' B ports stay attached to the 3.3 V nets, so once
DVDD_1V8 is up their internal pull-ups lift a floating pin to about 1.5 V
at the SoC; the gpio-hogs keep the three pins driven low from boot and the
sequencer must never leave them as inputs.

Jumpers and switches
====================

.. list-table::
   :header-rows: 1

   * - Ref
     - Function
     - Notes
   * - J69
     - STM32 BOOT0
     - 1-2 flash boot, 2-3 system bootloader, open = floating
   * - J17
     - VCORE enable source
     - VCORE_0V8_EN to CNTL (2-5 as drawn) for STM32 control; the printed
       default "3-4" would hold VCORE off
   * - J67 / J68
     - PMIC I2C master
     - 1-2 STM32 I2C5, 2-3 SoC I2C4
   * - J57 / J58
     - QSPI mux IN1 / IN2 straps
     - 4.7K to 3V3 or GND; MCU outputs override
   * - J79 / J56
     - SD mux IN1 / IN2 straps
     - same
   * - J31
     - ATX PS_ON
     - jumper turns the ATX supply on
   * - SW2 / SW3 / SW4
     - JTAG0 / JTAG1 / UART mux selects
     - One section per SEL line; ON grounds it (0), OFF pulls it up (1).
       Both ON = FT4232, both OFF = no source; the MCU overrides either
   * - B1
     - 12 V eFuse disable
     - board power cycle
   * - B2 / B3
     - MCU user button / MCU reset
     -
   * - J62
     - CR1220 for the MCU and SoC RTCs
     -

Flash layout
============

.. list-table::
   :header-rows: 1

   * - Sectors
     - Offset
     - Size
     - Partition
   * - 0-1 (2 x 32 KB)
     - 0x00000
     - 64 KB
     - ``mcuboot``
   * - 2-3 (2 x 32 KB)
     - 0x10000
     - 64 KB
     - ``storage`` (NVS, two sectors)
   * - 4-7 (128 KB + 3 x 256 KB)
     - 0x20000
     - 896 KB
     - ``image-0``

Known issues
============

None of these is fixed by firmware.

- **PG_PH1 level** (sheet 41): the TPS546E25 PG output is pulled up by
  R178 10K to the ~4.4 V LDO but also feeds LED DS2 through R185 1K, so
  when PG releases the node sits at the LED forward voltage plus a little,
  roughly 2 V (the LED part is not on the schematic). That is between the
  STM32's design-guaranteed VIH of 1.8 V and its production-tested 2.3 V,
  so PC7 may read low or be noisy while VCORE is fine.
  PG_PH2 has no LED and is unaffected. Measure before relying on it; the
  fix is to buffer the LED or remove DS2.
- **SoC held in reset by the USB bridge** (sheets 8, 55): U73 ADBUS4 drives
  the reset OR directly and is DTR#, high unless the host drives it low.
  A J71 cable to a host that has not asserted DTR, or any port close,
  asserts SoC reset. The fix is a pull-down and series resistor, an
  inverted sense, or an EEPROM/host configuration that keeps ADBUS4 low.
- **12 V eFuse and the USB bridge** (sheets 40, 55): with Q12 the eFuse is
  disabled when BDBUS7 is high, and BDBUS7 is RI# with the FT4232H's
  internal pull-up when nothing drives it. Its gate then sits around
  1.9 V, above the BSS138 threshold, so 12 V may stay off or drop while
  J71 is attached to an idle host. Measure P12V with J71 plugged in; a
  smaller R877 or a Schmitt buffer fixes it.

Bring-up checklist
==================

#. Read the PHY ID at MDIO addresses 0 and 1 and fix ``reg`` on
   ``eth_phy`` if needed; measure U25 pin 1 to learn BASIC vs ENHANCED mode.
#. Confirm SW2/SW3/SW4 positions, the J17 jumper, and J67/J68.
#. Fit a CR1220 in J62 if the RTC should keep time.
#. Enable ``clk_lse`` and switch ``rtc`` to ``STM32_SRC_LSE RTC_SEL(1)``
   on a test build. If Y13 does not start, the STM32 clock driver spins
   waiting for it and neither MCUboot nor the app prints anything; raise
   ``driving-capability`` on ``clk_lse`` in that case. Make it the default
   once it boots.
#. Check the FT4232 channel A SWD path programs the MCU; J61 is the
   fallback.
#. Measure PG_PH1 at PC7 with VCORE on, SYS_RSTN and P12V with a USB-C
   cable on J71 to an idle host (Known issues).

Feature work
============

#. Power sequencer: ordered enables with power-good waits, handshake
   outputs, reset held until the end, reverse order on power off.
#. Drive the JTAG mux selects from the JTAG daemon.
#. TMP117 and EEPROM support on ``i2c2`` (board temperature, identity).
#. PMBus telemetry over ``i2c4`` when J67/J68 give the bus to the MCU.
#. QSPI flash programming and SD slot S2 access through the muxes.
#. RTC on the LSE crystal once Y13 is verified.
