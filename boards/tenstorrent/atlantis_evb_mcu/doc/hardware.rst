.. SPDX-FileCopyrightText: © 2026 Tenstorrent USA, Inc.
..
.. SPDX-License-Identifier: Apache-2.0

Atlantis EVB BMC Hardware Reference
***********************************

This document maps the STM32F746ZGT6 pins to board signals on the
Tenstorrent Atlantis EVB, based on the schematic "Atlantis EVB v1" Rev 0.95
(sheet 32 "STM32" and the sheets it references), and records what WallaBMC
does with them. The MCU circuit is the SLT one with a different power
scheme, so the devicetree is shared
(``boards/tenstorrent/common/atlantis_mcu.dtsi``) and this document
concentrates on what the EVB does differently. Subsystems that are
identical point at the SLT reference,
``boards/tenstorrent/atlantis_slt_mcu/doc/hardware.rst``.

**Schematic**: Atlantis EVB v1 Rev 0.95 (CoreLab, 2026-10-11). Its change
notice sheet (56) is empty.

Differences from the SLT
========================

- **Supply**: the MCU, PHY, TMP117, EEPROM and the sheet 32 pull-ups run
  from D3V3_SB, made from the ATX +5VSB standby rail. The BMC is alive
  while the host is off and owns the PSU through PS_ON#.
- **Power control**: POWER_5V_EN (PE9) and PWR_GOOD_5V (PG6) are not
  connected; ATX_PSON_MCU (PB10) and ATX_PWR_OK (PB9) replace them. There
  is no on-board 12 V eFuse, no 12 V to 5 V/3.3 V bucks and no button B1.
- **New outputs**: M2_VCC_EN (PC13) switches the M.2 slot 3.3 V;
  SOC_BTM0/1/2 (PD7/PF5/PG14) can pull the SoC BOOT_MODE bits low.
- **No DIP switches** on the JTAG/UART mux selects: their pull-ups leave
  every mux at "no source" until the MCU drives the selects.
- **I2C**: the link to the SoC is SoC I2C0 (through U105), not I2C2; the
  PMIC bus reaches the MCU only after a 0R rework (R930-R933) instead of
  jumpers J67/J68.
- **Handshakes**: PWR1_GD, PWR2_GD and OTP_PWRRDY are plain 15K/18K
  dividers with no level shifter attached at all.
- **BOOT0**: J69 is a 2-pin jumper that must be fitted; open means the ST
  system bootloader.
- **Board ID** reads 11 (SLT: 00). **Console header** is J86, not J70.
- **VCORE enable** goes straight to the regulators (no J17).

MCU overview
============

- **MCU**: STM32F746ZGT6 (U23, LQFP144), Cortex-M7 at 216 MHz, 1 MB flash,
  256 KB SRAM + 64 KB DTCM
- **Supply**: D3V3_SB. ATX +5VSB (J84 pin 9) passes eFuse U91 (TPS25200,
  always enabled, ILIM about 2.4 A) and LDO U92 (TLV75801, 3.3 V, 500 mA)
  on sheet 40. VDDA/VREF+ are filtered by L64. The whole BMC domain shares
  the LDO's 500 mA.
- **Clocks**: 8 MHz crystal Y12 (HSE), 32.768 kHz crystal Y13 (LSE, not yet
  enabled: the RTC runs from LSI until the crystal is verified)
- **RTC backup**: VBAT is the raw CR1220 coin cell in J62 (sheet 35), shared
  with the SoC-side S35390A RTC. Without a cell the MCU RTC loses time on
  every power cycle.
- **BOOT0**: 2-pin jumper J69. R590 10K pulls BOOT0 to D3V3_SB; the jumper
  grounds it through R591 1K (about 0.3 V). **Fit the jumper** to boot from
  flash; without it the MCU sits in the ST system bootloader.
- **Reset**: button B3 on NRST
- **Board ID**: PD10/PD11 pulled up (R911/R912), ID1:ID0 = 11

Debug and console
=================

The FT4232H U42 on USB-C port J39 is USB powered and provides:

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

Headers: J61 SWD (1 SWCLK, 2 SWDIO, 3 D3V3_SB, 4 GND). J86 (20-pin, sheet
53): 1 MCU RX, 2 MCU TX, 3 PA0, 4 PB15, 7 PD4, 9/10/19/20 GND; pins
12/14/16/18 are SoC I2S0 outputs. Pins 5, 6 and 8 are labelled MCU_PG3,
MCU_PG4 and MCU_PD7 but are not connected (those MCU pins are the SD mux
inputs and SOC_BTM0). There is no 3V3 pin on J86.

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
     - MCU console, FT4232 channel C and J86 pins 2/1
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
   * - PA5, PA6, PA11, PA12
     - SYS_JTAG0_SEL0/1, SYS_JTAG1_SEL0/1
     - ``sys_jtag*_sel*`` + gpio-hogs
     - JTAG mux selects, held at 00 (USB bridge U73)
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
   * - PB10
     - ATX_PSON_MCU
     - ``atx_pson`` / ``power-gpio-1``
     - ATX PS_ON, high = PSU on (see Power)
   * - PC13
     - M2_VCC_EN
     - ``m2_vcc_en`` + gpio-hog
     - M.2 slot 3.3 V enable, held low
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
     - MCU_M_SCL / SDA
     - ``i2c2``
     - TMP117 and M24M01 EEPROM, 4.7K pull-ups (R875/R874)
   * - PB8 / PB7
     - MCU_S_SCL / SDA
     - ``i2c1``
     - Level-shifted link to SoC I2C0 (enabled, unused)
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
   * - PB9
     - ATX_PWR_OK
     - ``atx_pwr_ok``
     - In
     - ATX power good, high. Reads 2.5 V (a marginal high) when the PSU
       does not drive it; trust it only after PS_ON
   * - PE13
     - VCORE_0V8_EN
     - ``vcore_0v8_en``
     - Out
     - TPS546E25 x2 CNTL through R940 (0.8 V, 80 A)
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
     - TPS628681 PCIE_0V8 (from P5V)
   * - PE11
     - PCIE_VPH_EN
     - ``pcie_vph_en``
     - Out
     - TPS628682 PCIE_1V8 (from P5V)
   * - PC7 / PC6
     - PG_PH1 / PG_PH2
     - ``pg_ph1`` / ``pg_ph2``
     - In
     - VCORE phases in regulation, high; both clamped by LEDs to about
       2 V (see Known issues)
   * - PG7
     - PWR_GOOD_LPDDR5
     - ``pwr_good_lpddr5``
     - In
     - VDD1 and VDD2 good, high. Pulled up to D3V3_SB, so it reads high
       whenever the regulator is unpowered
   * - PA10
     - PWR_GOOD_0V9
     - ``pwr_good_0v9``
     - In
     - DDR_VDD2L good, high. Same standby pull-up caveat
   * - PF10
     - MPM54304_GPIO
     - ``mpm54304_gpio``
     - In
     - MPM54304 power good (open drain, internal pull-up enabled)
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
   * - PD7 / PF5 / PG14
     - SOC_BTM0 / 1 / 2
     - ``soc_btm*``
     - Out
     - High forces SoC BOOT_MODE[n] low (see Boot mode)
   * - PG0
     - TJ_MAX_3V3
     - ``tj_max``
     - In
     - SoC output through U103, presumably a junction temperature alarm
       (not documented); the DTS asks for a pull-down for when U103 is
       high-Z
   * - PA3, PA4, PD1, PD0
     - GPIO2/8/11/12_3V3
     - ``soc_gpio*``
     - Bidir
     - SoC GPIO_DATA[2/8/11/12] through auto-direction shifter U61. PD0
       is also wired to header J52 pin 18 (see Known issues)
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
     - SNR_ALERT
     - ``tmp117`` alert-gpios
     - In
     - TMP117 alert, active low, 4.7K pull-up
   * - PD12 / PD13
     - MCU_P_SCL / SDA
     - ``i2c4`` (disabled)
     -
     - PMIC bus, connected only after the R930-R933 rework
   * - PF3 / PF4
     - FT4232_GPIO2 / GPIO3
     - ``ft4232_gpio*``
     -
     - Spare lines from the SoC-side FT4232 U73 channel A
   * - PC0
     - FT232_GPIO
     - ``ft232_gpio``
     - In
     - MCU-side FT4232 U42 ADBUS5 through R849, purpose undefined; never
       drive it
   * - PD10 / PD11
     - STM32_ID0 / ID1
     - ``stm32_id*``
     - In
     - Board ID straps, both high on the EVB
   * - PC14 / PC15
     - Y13
     - ``clk_lse`` (disabled)
     -
     - 32.768 kHz crystal, enable once it is seen to start

Not connected or spare
----------------------

PA0, PB15 and PD4 reach header J86 pins 3, 4 and 7 and are free. PB3 and
PB5 are no-connects. PE9 (POWER_5V_EN) and PG6 (PWR_GOOD_5V) carry labels
that go nowhere: P5V is an ATX rail. PA15 and PB4 have 4.7K pull-ups only.
PE4 (ETH_MII_INTR) is wired but not in the DTS.

Ethernet
========

Sheet 34 is the SLT circuit (DP83826E U25, MII, 25 MHz crystal, RJ45 J20,
same straps) with every 3.3 V net moved to D3V3_SB, so the network is up
while the host is off. The SLT reference's MDIO address, ModeSelect and
reset notes apply unchanged.

Host console UART mux
=====================

SoC UART0 and UART1 are level shifted on sheet 25 and routed by
SN74CB3Q3253 muxes U82/U83 (sheet 51) with the SLT encoding:

.. list-table::
   :header-rows: 1

   * - SEL1 (PD14) : SEL0 (PD15)
     - UART0 / UART1 connected to
   * - 0 0
     - SoC-side FT4232 U73 channels C / D (USB-C J71)
   * - 0 1
     - MCU USART2 / USART3
   * - 1 0
     - Test points M24/M25 (UART0) and M28/M29 (UART1)
   * - 1 1
     - nothing (SoC RX lines float)

The selects have 4.7K pull-ups to P3V3 (R726/R727); the 499R grounding
resistors are not fitted and there is no DIP switch, so the muxes idle at
11 until the MCU drives them. WallaBMC hogs SEL1 low and drives SEL0 high
as ``host-console-uart-muxsel`` at init, so while the BMC runs the SoC
console (UART0) is on the console logger, console bridge (TCP port 22) and
web terminal. The muxes are on P3V3 and only exist after PS_ON.

SoC reset
=========

Same circuit as the SLT (sheet 8): SYS_RSTN_3V3 (PG5) and
FT4232_SYS_RSTN_3V3 (U73 ADBUS4) are diode-ORed into Q1, which pulls the
SoC RESETN low; **either input high asserts reset**. R589 (to D3V3_SB)
pulls PG5 high, so the SoC is held in reset throughout standby until
firmware drives the pin low. WallaBMC maps it as ``reset-gpio-1`` with
``GPIO_ACTIVE_HIGH``: ``reset_init()`` releases the SoC at BMC start and
``reset`` pulses it for one second.

SYS_RSTN also pulls the SoC-side PCIE_X16_PERSTN and PCIE_X4_PERSTN low
through D10/D11 (sheets 9/10; the EVB has no x8 slot), where jumpers
J47/J49 can add the SoC's own RST_PCIE GPIOs or a manual strap. Whether
the slot and M.2 PERST# pins follow depends on shifter U108, which Rev
0.95 straps in the wrong direction (see Known issues).

U73 ADBUS4 idles high as DTR# in the FT4232H's default UART mode, so a
USB-C cable on J71 to a host that does not drive it low holds the SoC in
reset regardless of PG5 (see Known issues).

Power
=====

ATX input
---------

The 24-pin ATX connector J84 (sheet 40) supplies P3V3, P5V and ATX_12V
(fuse F1 to P12V, "12V Ready" LED DS3) directly: there are no on-board
bucks for them and no eFuse. All three exist only while PS_ON# is low. The
standby rail +5VSB feeds U91 and U92 to make D3V3_SB for the BMC domain.

- **PS_ON**: ATX_PSON_MCU (PB10) drives Q12 through R917 1K with R918 100K
  holding the gate low; Q12 pulls PS_ON# to ground. High = PSU on. The MCU
  pin is the only thing holding the PSU on: an MCU reset, reflash or crash
  releases Q12 and the host loses power. Jumper J85 is a manual PS_ON, but
  through R834 10K it probably cannot pull PS_ON# below the ATX VIL.
- **PWR_OK**: J84 PWR_OK reaches PB9 through R935 204R with D33 ESD
  protection. R915/R916 (10K to +5VSB, 10K to GND) bias the node to about
  2.5 V when the PSU leaves it open, which the STM32 reads as a marginal
  high. Treat PB9 as valid only after PS_ON is asserted, wait about 1 s for
  it (ATX T3 is at most 500 ms), and treat it going low as PSU loss.
- **In standby** (PS_ON off): the MCU, PHY, TMP117, EEPROM, SWD and console
  work. Everything on P3V3/P5V/P12V is dead: the UART/JTAG muxes, the QSPI
  flash and its mux, the 3.3 V sides of the level shifters other than
  U105/U107, the M.2 switch, the SoC regulators and the fan. Driving mux
  selects and QSPI lines into those
  unpowered parts is harmless (their inputs are specified for VCC = 0).

SoC rails controlled by the MCU
-------------------------------

Every regulator enable has a pull-down, so the SoC rails stay off until
the firmware drives them. The MCU pins are the SLT ones; only step 1
differs, and the M.2 slot is new.

.. list-table::
   :header-rows: 1

   * - Step
     - Enable
     - Regulator
     - Rails
     - Power good
   * - 1
     - ATX_PSON_MCU (PB10)
     - ATX PSU
     - P12V, P5V, P3V3
     - ATX_PWR_OK (PB9)
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
     - M2_VCC_EN (PC13)
     - U90 TPS25200
     - P3V3_M2 (M.2 slot)
     - none; allow the 100 ms PCIe T_PVPERL before releasing reset
   * - 10
     - OTP_PWRRDY (PF2), PWR1_GD (PG1), PWR2_GD (PG2)
     -
     - handshakes to the SoC
     -

Steps 3 to 9 are not ordered relative to each other by the schematic; the
LPDDR5 sub-order VDD1 > VDD2 > 0.9 V > VDDQ (sheet 44) and the IPS-1761
handshake constraints from the SLT reference still apply. Notes on the
power-good inputs:

- PWR_GOOD_LPDDR5 and PWR_GOOD_0V9 are pulled up to D3V3_SB (R684, R761),
  so they read high while their regulators are unpowered. Sample them only
  after PWR_OK and the matching enable, and look for a low-to-high
  transition rather than a level.
- PG_PH1 and PG_PH2 are clamped by LED chains (R185/DS2, R936/DS18) to
  roughly 2 V (the LED part is not on the schematic), between the STM32's
  design-guaranteed VIH of 1.8 V and its production-tested 2.3 V. Measure
  before relying on them; PMBus STATUS_WORD at 0x0D is the authoritative
  source.
- MPM54304_GPIO is the module's open-drain power good (0.2 ms delay) with
  no board pull-up; the DTS enables the internal one.
- The MPM54304 AVDD_0V8 divider (R716 12K / R717 26K) assumes V_Ref4 has
  been programmed to 0.55 V; with the -0001 MTP's 0.6 V reference the rail
  comes up at about 0.88 V at the first enable after each PS_ON (see Known
  issues).

**WallaBMC today** maps only ATX_PSON_MCU to ``power-gpio-1``: ``power on``
turns the PSU on and nothing else, ``power off`` cuts all ATX rails at once,
the three handshakes and the M.2 enable are held low by gpio-hogs, and the
SoC reset is released at BMC start. Bringing the SoC up needs a
board-specific sequencer in ``power.c`` that waits for PWR_OK, walks the
table above with power-good waits, drives the handshakes, and holds
SYS_RSTN until the end. All the GPIO nodes exist in the DTS.

Boot mode
=========

SOC_BTM0/1/2 (PD7/PF5/PG14) each drive a BSS138 (Q13-Q15, 1K series, 100K
gate pull-down, sheet 45) whose drain is BOOT_MODE0/1/2. Sheet 8 pulls
those to DVDD_1V8 through 4.7K and grounds them through DIP switch J44
sections 6/7/8 when ON. So BOOT_MODE[n] = NOT(SOC_BTM[n]) AND NOT(J44
section ON): the MCU can only force a bit to 0, and a DIP section that is
ON wins. With the MCU pins low or floating the DIP switch decides. The
BOOT_MODE[2:0] encoding and its sample point are not in the schematic;
they must come from the SoC documentation. Drive the pins, if at all,
after DVDD_1V8 is up and before SYS_RSTN is released. TEST_MODE moved
from the DIP switch to jumper J79 (fitted = low).

PMIC I2C
========

The regulators share one PMBus (TPS546E25 at 0x0D, TPS628681 at 0x49,
TPS628682 at 0x41, MPM54304 at 0x68) pulled up by R919/R920 4.7K to P3V3.
Sheet 47 selects the master with 0R links: R930/R932 (fitted) connect the
SoC's I2C4 through shifter U107, R931/R933 (not fitted) would connect the
MCU's MCU_P (STM32 ``i2c4``, PD12/PD13, pull-ups R204/R205 to D3V3_SB).
Fitting R931/R933 without removing R930/R932 puts two masters on the bus
through an auto-direction shifter, so the rework must swap them. Sheet 54
draws the MCU as the master; the fitted parts say otherwise. The
TPS546E25 SMB_ALERT lines reach only header J9. ``i2c4`` stays disabled.

JTAG
====

Two SoC TAPs, each behind SN74CB3Q3253 muxes with the UART mux encoding
(SEL1:SEL0 00 FT4232 U73, 01 MCU, 10 header, 11 none):

.. list-table::
   :header-rows: 1

   * - Chain
     - Sheet
     - MCU pins (TCK/TMS/TDI/TDO/TRST)
     - Select
     - Header
   * - JTAG0 (RCPU)
     - 49
     - PF12/PF11/PF13/PF14/PF15
     - PA5 (SEL0), PA6 (SEL1)
     - J40
   * - JTAG1 (Ascalon)
     - 50
     - PG10/PG9/PG11/PG12/PG13
     - PA11 (SEL0), PA12 (SEL1)
     - J73

The selects have 4.7K pull-ups to P3V3 and no switch (the 499R pull-downs
are not fitted), so with the MCU idle both TAPs are unreachable. WallaBMC
hogs all four selects low (00), which keeps the USB JTAG path through U73
working as on an SLT with its switches ON. ``jtag.c`` does not drive the
selects yet and nothing on the board can override the hog, so the BMC's
own bit-bang JTAG is unreachable on the EVB until it does; a test image
can change ``sys_jtag0_sel0_hog`` to ``output-high`` (01), which cuts the
USB path through U73 while it runs.

Boot flash and SD muxes
=======================

The same TS3A27518E muxes and control scheme as the SLT: U21 for the
MX25L12833F boot flash (PE5 EN, PE6/PE7 IN1/IN2, sheet 33) and U20 for SD
slot S2 (PE8 EN, PG3/PG4, sheet 27). WallaBMC hogs all six lines low so
the SoC owns both. The SLT's strap headers are gone: QSPI_MUX_IN1/IN2 have
4.7K pull-ups to P3V3 (R511/R513) that the hogs override, and the SD mux
inputs have no pulls. Slot S1 is wired straight to SoC SDIO0; both slots
gained ESD diodes. U21 and the flash are on P3V3, U20 and the slots on
DVDD_3V3.

I2C buses
=========

.. list-table::
   :header-rows: 1

   * - Schematic name
     - STM32 peripheral
     - Pins
     - Devices
   * - MCU_M
     - I2C2 (``i2c2``)
     - PF1 SCL, PF0 SDA
     - TMP117 (0x49, alert PD3), M24M01 EEPROM (128 KiB at 0x56; the
       driver uses 0x57 for the upper half; write control tied low). 4.7K
       pull-ups to D3V3_SB, fast mode.
   * - MCU_S
     - I2C1 (``i2c1``)
     - PB8 SCL, PB7 SDA
     - SoC I2C0 through TXS0102 U105 (3.3 V side on D3V3_SB, OE from
       DVDD_1V8), 4.7K pull-ups both sides, nothing else on the SoC side.
       Protocol and master undefined; the schematic note reads "ALERT??
       MCU".
   * - MCU_P
     - I2C4 (``i2c4``, disabled)
     - PD12 SCL, PD13 SDA
     - PMIC bus after the R930-R933 rework

Not reachable by the MCU: the PCIe slot and M.2 SMBus (SoC I2C1), the
S35390A RTC (SoC I2C3), the IO connector J83 (SoC I2C2) and HDMI DDC.

Level shifters
==============

PWR1_GD, PWR2_GD and OTP_PWRRDY are 15K/18K dividers (R881-R886, sheet
46) with nothing else attached: the SoC sees 1.8 V when the MCU drives
3.3 V and 0 V when the pin is low or floating. The pins must be push-pull.

GPIO2/8/11/12_3V3 go through TXS0108E U61 with OE from DVDD_1V8: undriven
until MPM54304 is enabled, and no pull-downs (they would fight the
shifter's pull-ups). TJ_MAX_3V3 comes through SN74AVC4T245 U103, a fixed
direction push-pull buffer that is high-Z while DVDD_1V8 is off, so the
DTS gives PG0 a pull-down for whatever reads it. The MCU-facing I2C
shifters U105 and U107 have
their 3.3 V side on D3V3_SB, their 1.8 V side on DVDD_1V8 and OE from
DVDD_1V8.

Jumpers, switches and headers
=============================

.. list-table::
   :header-rows: 1

   * - Ref
     - Function
     - Notes
   * - J69
     - STM32 BOOT0
     - 2-pin; fitted = flash boot, open = ST system bootloader
   * - J84
     - ATX 24-pin input
     - PS_ON# driven by the MCU (PB10)
   * - J85
     - Manual ATX PS_ON
     - 10K in series; probably too weak, see Known issues
   * - J44
     - SoC DIP switch
     - sections 6/7/8 ground BOOT_MODE0/1/2 when ON; 1-5 are I2S0 straps
   * - J79
     - SoC TEST_MODE
     - fitted = low
   * - J47 / J49, J48 / J50
     - PCIe x16 / M.2 PERST# source
     - 2-3 adds the SoC RST_PCIE GPIO, 1-2 the manual strap J48/J50
   * - J9
     - PMBus debug header
     - PMB_CLK, PMB_DATA, SMB_ALERT, PMB_CONTROL
   * - R930-R933
     - PMIC bus master
     - 0R links, SoC fitted, MCU not fitted
   * - R511-R514
     - QSPI mux IN1/IN2 straps
     - 4.7K pull-ups fitted; MCU outputs override
   * - J40 / J73
     - JTAG0 / JTAG1 headers
     - mux select 10
   * - J86
     - MCU console and spare GPIO
     - see Debug and console
   * - J61
     - MCU SWD
     - 1 SWCLK, 2 SWDIO, 3 D3V3_SB, 4 GND
   * - J64
     - Fan
     - power only (1 NC, 2 +12 V, 3 GND); runs whenever PS_ON is on
   * - B2 / B3
     - MCU user button / MCU reset
     -
   * - J62
     - CR1220 for the MCU and SoC RTCs
     -

Flash layout
============

Identical to the SLT: MCUboot in sectors 0-1, NVS storage in sectors 2-3,
``image-0`` (896 KB) in sectors 4-7.

Known issues
============

Found while reading Rev 0.95; the first three affect the BMC design, the
rest are hardware questions.

- **No PS_ON latch** (sheet 40): PS_ON# is held only by Q12 from PB10. Any
  MCU reset (reflash, watchdog, debugger reset, MCUboot before GPIO init)
  drops the host power unsequenced. J85 cannot substitute: R834 10K in
  series leaves PS_ON# above the ATX VIL against the PSU's pull-up (the SLT
  strap used 1K). Hardware should add a latch or pull-up option and change
  R834; firmware should re-assert PB10 as early as possible after a warm
  reset if the host was on.
- **M.2 enable floats** (sheet 24): U90 TPS25200 EN has no pull resistor
  and the datasheet forbids a floating EN, so the slot power is undefined
  whenever PC13 is not driven. A 100K pull-down at U90 matches every other
  enable on the board; until then the gpio-hog keeps it low once the BMC
  runs.
- **Power-good pull-ups on standby** (sheet 44): PWR_GOOD_LPDDR5 and
  PWR_GOOD_0V9 read high with the regulators off; see Power.
- **PERST# shifter direction** (sheet 25): U108 SN74AVC4T245 carries both
  PERST# lines with 2DIR strapped low (R908), which is B to A: the x16 slot
  and the M.2 never receive PERST#, and the SoC-side PERSTN inputs are
  driven by the shifter against the D10/D11 reset diodes. R908 should move
  to DVDD_1V8. Until then nothing the BMC does with SYS_RSTN reaches the
  connectors.
- **MPM54304 AVDD_0V8** (sheet 43): R717 changed from 36K to 26K with the
  note "Set VRef4 = 0.55 V", but nothing on the board can write that
  register before the first MPM54304_EN after 12 V is applied, so the
  0.8 V rail starts at about 0.88 V. Hardware should confirm the MTP that
  is fitted and the SoC's AVDD_0V8 maximum; if the BMC is meant to program
  it, it needs the PMBus (R930-R933) first.
- **PG_PH1 / PG_PH2 levels** (sheet 41): both power-good nodes feed LEDs
  through 1K, clamping the high level to roughly 2 V with no margin to the
  STM32's production-tested VIH. Measure at PC7/PC6.
- **SoC held in reset by the USB bridge** (sheets 8, 52): U73 ADBUS4 drives
  the reset OR directly and idles high as DTR#; see SoC reset.
- **U61 A5 label** (sheet 46): both A4 and A5 of U61 are labelled GPIO12,
  so SoC GPIO_DATA[13] is unconnected and header J52 pin 18 (GPIO13_3V3)
  is a second path onto PD0. Do not use J52 pin 18 until fixed.
- **BOOT0 default** (sheet 32): J69 open selects the ST bootloader; a
  board without the jumper looks dead.
- **Standby LDO budget** (sheet 40): U92 TLV75801 (SOT-23-5, 500 mA)
  drops 5 V to 3.3 V for the MCU at 216 MHz, the PHY, the LEDs and the
  pull-ups; about 0.4 W at 250 mA with no thermal design shown.
- **Stale labels**: J86 pins 5/6/8 (MCU_PG3/PG4/PD7) are not connected;
  sheet 42 still says "12V->3.3V" although the PCIe regulators run from
  P5V; the PE8 net is spelt SDIO_MUX_en on sheet 32 and SDIO_MUX_EN on
  sheet 27.

Bring-up checklist
==================

#. Fit J69. Check PB10 low keeps the PSU off and high turns it on, and
   that PB9 follows PWR_OK (and what it reads with the PSU in standby).
#. Read the PHY ID at MDIO addresses 0 and 1 and fix ``reg`` on
   ``eth_phy`` if needed; measure U25 pin 1 to learn BASIC vs ENHANCED mode.
#. Confirm J44 sections 6/7/8, J47/J49 and the R930-R933 fit.
#. Fit a CR1220 in J62 if the RTC should keep time.
#. Enable ``clk_lse`` and switch ``rtc`` to ``STM32_SRC_LSE RTC_SEL(1)``
   on a test build. If Y13 does not start, the STM32 clock driver spins
   waiting for it and neither MCUboot nor the app prints anything; raise
   ``driving-capability`` on ``clk_lse`` in that case. Make it the default
   once it boots.
#. Check the FT4232 channel A SWD path programs the MCU; J61 is the
   fallback.
#. Measure PG_PH1/PG_PH2 with VCORE on, PWR_GOOD_LPDDR5/PWR_GOOD_0V9 in
   standby, SYS_RSTN with a USB-C cable on J71 to an idle host, and the
   M.2 PERST# behaviour (Known issues).

Feature work
============

#. Power sequencer: PS_ON and PWR_OK first, then the ordered enables with
   power-good waits, the M.2 slot, the handshakes, reset held until the
   end, reverse order on power off, and PB10 re-asserted after a warm
   reset if the host was on.
#. Drive the JTAG mux selects from the JTAG daemon (01 on connect, back to
   00 on disconnect).
#. Boot-mode control through ``soc_btm*`` once the encoding is known.
#. TMP117 and EEPROM support on ``i2c2`` (board temperature, identity);
   use the board ID straps to tell EVB and SLT apart if one image serves
   both.
#. PMBus telemetry over ``i2c4`` after the R930-R933 rework.
#. QSPI flash programming and SD slot S2 access through the muxes.
#. RTC on the LSE crystal once Y13 is verified.
