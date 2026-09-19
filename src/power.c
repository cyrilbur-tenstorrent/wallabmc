/*
 * SPDX-FileCopyrightText: © 2025-2026 Tenstorrent USA, Inc.
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/shell/shell.h>
#include <stdlib.h>
#include <ctype.h>
#include <zephyr/sys/util.h>
#include <zephyr/kernel.h>
#include <zephyr/drivers/gpio.h>

#define LOG_LEVEL CONFIG_LOG_DEFAULT_LEVEL
#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(wallabmc_power);

#include "config.h"
#include "power.h"

#define GPIO_POWER_1 DT_ALIAS(power_gpio_1)
#define GPIO_POWER_2 DT_ALIAS(power_gpio_2)
#define GPIO_RESET DT_ALIAS(reset_gpio_1)

static const struct gpio_dt_spec power_gpios[] = {
#if DT_NODE_HAS_STATUS_OKAY(GPIO_POWER_1)
	GPIO_DT_SPEC_GET(GPIO_POWER_1, gpios),
#endif
#if DT_NODE_HAS_STATUS_OKAY(GPIO_POWER_2)
	GPIO_DT_SPEC_GET(GPIO_POWER_2, gpios),
#endif
};

static const struct gpio_dt_spec gpio_reset =
#if DT_NODE_HAS_STATUS_OKAY(GPIO_RESET)
	GPIO_DT_SPEC_GET(GPIO_RESET, gpios);
#else
	{ 0 };
#endif

static bool system_power_state = false;

bool power_get_state(void)
{
	return system_power_state;
}

static int power_on(void)
{
	int i, ret;

	for (i = 0; i < ARRAY_SIZE(power_gpios); i++) {
		ret = gpio_pin_set_dt(&power_gpios[i], 1);
		if (ret < 0) {
			LOG_ERR("Could not assert power GPIO %d", i);
			return -1;
		}
	}

	system_power_state = true;

	return 0;
}

static int power_off(void)
{
	int i, ret;

	for (i = 0; i < ARRAY_SIZE(power_gpios); i++) {
		ret = gpio_pin_set_dt(&power_gpios[i], 0);
		if (ret < 0) {
			LOG_ERR("Could not deassert power GPIO %d", i);
			return -1;
		}
	}

	system_power_state = false;

	return 0;
}

int power_set_state(bool on)
{
	int ret;

	if (on)
		ret = power_on();
	else
		ret = power_off();

	if (ret < 0) {
		LOG_ERR("Failed to set power state: %d", ret);
		return ret;
	}

	LOG_INF("System Power State changed to: %s", power_get_state() ? "ON" : "OFF");
	return 0;
}

int power_init(void)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(power_gpios); i++) {
		if (!gpio_is_ready_dt(&power_gpios[i])) {
			LOG_INF("Power GPIO %d not ready\n", i);
			return -1;
		}

		if (gpio_pin_configure_dt(&power_gpios[i], GPIO_OUTPUT_INACTIVE) < 0) {
			LOG_INF("Could not configure power GPIO %d\n", i);
			return -1;
		}
	}

	return 0;
}

int power_auto_poweron(void)
{
	if (config_host_auto_poweron()) {
		int ret = power_on();
		if (ret < 0) {
			LOG_ERR("Auto power-on failed: %d", ret);
			return ret;
		}
	}

	return 0;
}

static int cmd_power_on(const struct shell *sh, size_t argc, char **argv)
{
	ARG_UNUSED(argc);
	ARG_UNUSED(argv);
	return power_on();
}

static int cmd_power_off(const struct shell *sh, size_t argc, char **argv)
{
	ARG_UNUSED(argc);
	ARG_UNUSED(argv);
	return power_off();
}

SHELL_STATIC_SUBCMD_SET_CREATE(sub_power_cmds,
	SHELL_CMD(on,    NULL, "Power on.", cmd_power_on),
	SHELL_CMD(off,   NULL, "Power off.", cmd_power_off),
	SHELL_SUBCMD_SET_END
);

SHELL_CMD_REGISTER(power, &sub_power_cmds, "Power commands", NULL);

int reset_init(void)
{
	int ret;

	if (!DT_NODE_HAS_STATUS_OKAY(GPIO_RESET)) {
		/* No reset GPIO */
		return 0;
	}

	if (!gpio_is_ready_dt(&gpio_reset)) {
		LOG_INF("Reset GPIO not ready\n");
		return -1;
	}

	/*
	 * Start inactive so the pin is hi-Z (open-drain) / driven inactive
	 * at boot. GPIO_OUTPUT_ACTIVE would briefly assert reset to the host
	 * every time the BMC starts up.
	 */
	ret = gpio_pin_configure_dt(&gpio_reset, GPIO_OUTPUT_INACTIVE);
	if (ret < 0) {
		LOG_INF("Could not configure reset GPIO\n");
		return -1;
	}

	return 0;
}

int power_reset(void)
{
	int ret;

	if (!DT_NODE_HAS_STATUS_OKAY(GPIO_RESET)) {
		LOG_ERR("No reset GPIO");
		return -1;
	}
	ret = gpio_pin_set_dt(&gpio_reset, 1);
	if (ret < 0) {
		LOG_INF("Could not toggle RESET GPIO\n");
		return -1;
	}

	k_msleep(1000);

	ret = gpio_pin_set_dt(&gpio_reset, 0);
	if (ret < 0) {
		LOG_INF("Could not toggle RESET GPIO\n");
		return -1;
	}

	return 0;
}

static int cmd_reset(const struct shell *sh, size_t argc, char **argv)
{
	ARG_UNUSED(argc);
	ARG_UNUSED(argv);
	return power_reset();
}

SHELL_CMD_REGISTER(reset, NULL, "Reset.", cmd_reset);
