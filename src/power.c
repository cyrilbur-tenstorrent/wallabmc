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

#define GPIO_RESET DT_ALIAS(reset_gpio_1)
#define GPIO_POWER_LED  DT_ALIAS(power_led)
#define POWER_SEQUENCER DT_COMPAT_GET_ANY_STATUS_OKAY(wallabmc_power_sequencer)

/*
 * A power-good input of a rail. Required inputs must read inactive before
 * the enable is asserted and active afterwards; optional ones only warn.
 */
struct power_good {
	struct gpio_dt_spec gpio;
	const char *name;
	bool optional;
};

/*
 * A host power rail: an enable to drive, the power-good inputs to wait for
 * and a settle time before the next rail comes up. Power off pauses
 * off_delay_ms before a rail that the rails switched off earlier need
 * while they shut down.
 */
struct power_rail {
	struct gpio_dt_spec enable;
	const struct power_good *good;
	uint8_t ngood;
	uint32_t good_timeout_ms;
	uint32_t settle_ms;
	uint32_t off_delay_ms;
	const char *name;
	bool *on;
};

#define GPIO_NAME(node) DT_PROP_OR(node, label, DT_NODE_FULL_NAME(node))

#define RAIL(node) UTIL_CAT(power_rail_, DT_DEP_ORD(node))
#define RAIL_ON(node) UTIL_CAT(power_rail_on_, DT_DEP_ORD(node))
#define RAIL_GOOD(node) UTIL_CAT(power_good_, DT_DEP_ORD(node))

#define RAIL_HAS_GOOD(node)							\
	UTIL_OR(DT_NODE_HAS_PROP(node, power_good),				\
		DT_NODE_HAS_PROP(node, power_good_optional))

#define POWER_GOOD(node, prop, idx, opt)					\
	{ .gpio = GPIO_DT_SPEC_GET(DT_PHANDLE_BY_IDX(node, prop, idx), gpios),	\
	  .name = GPIO_NAME(DT_PHANDLE_BY_IDX(node, prop, idx)),		\
	  .optional = opt },

#define POWER_GOOD_LIST(node, prop, opt)					\
	COND_CODE_1(DT_NODE_HAS_PROP(node, prop),				\
		(DT_FOREACH_PROP_ELEM_VARGS(node, prop, POWER_GOOD, opt)), ())

#define RAIL_GOOD_ARRAY(node)							\
	COND_CODE_1(RAIL_HAS_GOOD(node),					\
		(static const struct power_good RAIL_GOOD(node)[] = {		\
			POWER_GOOD_LIST(node, power_good, false)		\
			POWER_GOOD_LIST(node, power_good_optional, true)	\
		};), ())

/* wallabmc,power-rail nodes */
#define RAIL_DEFINE(node)							\
	RAIL_GOOD_ARRAY(node)							\
	static bool RAIL_ON(node);						\
	static const struct power_rail RAIL(node) = {				\
		.enable = GPIO_DT_SPEC_GET(DT_PHANDLE(node, enable), gpios),	\
		.good = COND_CODE_1(RAIL_HAS_GOOD(node),			\
				    (RAIL_GOOD(node)), (NULL)),			\
		.ngood = DT_PROP_LEN_OR(node, power_good, 0) +			\
			 DT_PROP_LEN_OR(node, power_good_optional, 0),		\
		.good_timeout_ms = DT_PROP(node, power_good_timeout_ms),	\
		.settle_ms = DT_PROP(node, settle_ms),				\
		.off_delay_ms = DT_PROP(node, off_delay_ms),			\
		.name = DT_PROP_OR(DT_PHANDLE(node, enable), label,		\
				   DT_NODE_FULL_NAME(node)),			\
		.on = &RAIL_ON(node),						\
	};

DT_FOREACH_STATUS_OKAY(wallabmc_power_rail, RAIL_DEFINE)

#define SEQUENCE_RAIL(node, prop, idx) &RAIL(DT_PHANDLE_BY_IDX(node, prop, idx)),

/* Both lists are empty on a board without a sequencer */
#if DT_HAS_COMPAT_STATUS_OKAY(wallabmc_power_sequencer)
#define POWER_SEQUENCE(prop) DT_FOREACH_PROP_ELEM(POWER_SEQUENCER, prop, SEQUENCE_RAIL)
#else
#define POWER_SEQUENCE(prop)
#endif

static const struct power_rail *const power_on_sequence[] = {
	POWER_SEQUENCE(power_on_sequence)
};

static const struct power_rail *const power_off_sequence[] = {
	POWER_SEQUENCE(power_off_sequence)
};

BUILD_ASSERT(ARRAY_SIZE(power_on_sequence) == ARRAY_SIZE(power_off_sequence),
	     "power-on-sequence and power-off-sequence list different rails");

static const struct gpio_dt_spec power_led_gpio =
#if DT_NODE_HAS_STATUS_OKAY(GPIO_POWER_LED)
	GPIO_DT_SPEC_GET(GPIO_POWER_LED, gpios);
#else
	{ 0 };
#endif

static const struct gpio_dt_spec gpio_reset =
#if DT_NODE_HAS_STATUS_OKAY(GPIO_RESET)
	GPIO_DT_SPEC_GET(GPIO_RESET, gpios);
#else
	{ 0 };
#endif

/* Serialises the shell, Redfish and boot-time power operations */
static K_MUTEX_DEFINE(power_lock);

static bool system_power_state = false;

bool power_get_state(void)
{
	return system_power_state;
}

static void power_led_set(int value)
{
	if (DT_NODE_HAS_STATUS_OKAY(GPIO_POWER_LED) &&
	    gpio_is_ready_dt(&power_led_gpio)) {
		/* Don't fail the operation on LED failure */
		if (gpio_pin_set_dt(&power_led_gpio, value) < 0)
			LOG_ERR("Could not %s power LED",
				value ? "assert" : "deassert");
	}
}

static int reset_set(int value)
{
	int ret;

	if (!DT_NODE_HAS_STATUS_OKAY(GPIO_RESET) ||
	    !gpio_is_ready_dt(&gpio_reset))
		return 0;

	ret = gpio_pin_set_dt(&gpio_reset, value);
	if (ret < 0)
		LOG_ERR("Could not %s reset GPIO: %d",
			value ? "assert" : "deassert", ret);

	return ret;
}

/* Drive a rail's enable inactive; it is off whatever the GPIO call said */
static int rail_off(const struct power_rail *rail)
{
	int ret;

	if (!*rail->on)
		return 0;

	if (rail->off_delay_ms)
		k_msleep(rail->off_delay_ms);

	ret = gpio_pin_set_dt(&rail->enable, 0);
	if (ret < 0)
		LOG_ERR("Could not deassert %s: %d", rail->name, ret);

	*rail->on = false;

	return ret;
}

/* Power off every rail that is on, in power-off-sequence order */
static int rails_off(void)
{
	int ret = 0;
	int i;

	for (i = 0; i < ARRAY_SIZE(power_off_sequence); i++) {
		int rc = rail_off(power_off_sequence[i]);

		if (rc < 0)
			ret = rc;
	}

	return ret;
}

/*
 * Wait until the power-good inputs of a rail read level. Optional inputs
 * are skipped before the enable (all == false) and only warn on timeout.
 */
static int rail_wait_good(const struct power_rail *rail, int level, bool all)
{
	int64_t deadline = k_uptime_get() + rail->good_timeout_ms;

	for (;;) {
		const struct power_good *late = NULL;
		int i;

		for (i = 0; i < rail->ngood; i++) {
			const struct power_good *good = &rail->good[i];
			int val;

			if (good->optional && !all)
				continue;

			val = gpio_pin_get_dt(&good->gpio);
			if (val < 0) {
				LOG_ERR("%s: could not read %s: %d", rail->name,
					good->name, val);
				return val;
			}

			if (val != level) {
				late = good;
				if (!good->optional)
					break;
			}
		}

		if (!late)
			return 0;

		if (k_uptime_get() >= deadline) {
			if (late->optional) {
				LOG_WRN("%s: optional %s %s after %u ms",
					rail->name, late->name,
					level ? "not good" : "still good",
					rail->good_timeout_ms);
				return 0;
			}

			LOG_ERR("%s: %s %s after %u ms", rail->name, late->name,
				level ? "not good" : "still good",
				rail->good_timeout_ms);
			return -ETIMEDOUT;
		}

		k_msleep(1);
	}
}

static int rail_on(const struct power_rail *rail)
{
	int ret;

	/* A power good that is up before its enable cannot be trusted */
	ret = rail_wait_good(rail, 0, false);
	if (ret < 0)
		return ret;

	ret = gpio_pin_set_dt(&rail->enable, 1);
	if (ret < 0) {
		LOG_ERR("Could not assert %s: %d", rail->name, ret);
		return ret;
	}
	*rail->on = true;

	ret = rail_wait_good(rail, 1, true);
	if (ret < 0)
		return ret;

	LOG_DBG("%s on", rail->name);

	if (rail->settle_ms)
		k_msleep(rail->settle_ms);

	return 0;
}

static int power_on(void)
{
	int i, ret;

	/* Idempotent: a repeated power on must not reset a running host */
	if (system_power_state)
		return 0;

	/* Hold the host in reset while its rails come up */
	ret = reset_set(1);
	if (ret < 0)
		return ret;

	for (i = 0; i < ARRAY_SIZE(power_on_sequence); i++) {
		const struct power_rail *rail = power_on_sequence[i];

		ret = rail_on(rail);
		if (ret < 0) {
			LOG_ERR("Power on failed at %s, powering off", rail->name);
			rails_off();
			return ret;
		}
	}

	ret = reset_set(0);
	if (ret < 0) {
		rails_off();
		return ret;
	}

	power_led_set(1);
	system_power_state = true;

	return 0;
}

static int power_off(void)
{
	int ret, rc;

	/* Idempotent */
	if (!system_power_state)
		return 0;

	/* Assert reset before the rails go away */
	ret = reset_set(1);

	rc = rails_off();
	if (rc < 0)
		ret = rc;

	power_led_set(0);
	system_power_state = false;

	return ret;
}

int power_cycle(void)
{
	int ret;

	k_mutex_lock(&power_lock, K_FOREVER);

	ret = power_off();
	if (ret == 0) {
		k_msleep(2000);
		ret = power_on();
	}

	k_mutex_unlock(&power_lock);

	return ret;
}

int power_set_state(bool on)
{
	bool was_on;
	int ret;

	k_mutex_lock(&power_lock, K_FOREVER);
	was_on = system_power_state;
	if (on)
		ret = power_on();
	else
		ret = power_off();
	k_mutex_unlock(&power_lock);

	if (ret < 0) {
		LOG_ERR("Failed to set power state: %d", ret);
		return ret;
	}

	if (was_on == on)
		LOG_INF("System Power State already: %s", on ? "ON" : "OFF");
	else
		LOG_INF("System Power State changed to: %s", on ? "ON" : "OFF");
	return 0;
}

int power_init(void)
{
	int i, j;

	/*
	 * There is no support for host power state to persist over BMC
	 * reboots yet. Power GPIOs are always initialised inactive and
	 * system_power_state is false at boot.
	 */
	for (i = 0; i < ARRAY_SIZE(power_on_sequence); i++) {
		const struct power_rail *rail = power_on_sequence[i];

		if (!gpio_is_ready_dt(&rail->enable)) {
			LOG_INF("Power GPIO %s not ready\n", rail->name);
			return -1;
		}

		if (gpio_pin_configure_dt(&rail->enable, GPIO_OUTPUT_INACTIVE) < 0) {
			LOG_INF("Could not configure power GPIO %s\n", rail->name);
			return -1;
		}

		for (j = 0; j < rail->ngood; j++) {
			const struct power_good *good = &rail->good[j];

			if (!gpio_is_ready_dt(&good->gpio)) {
				LOG_INF("Power good GPIO %s not ready\n", good->name);
				return -1;
			}

			if (gpio_pin_configure_dt(&good->gpio, GPIO_INPUT) < 0) {
				LOG_INF("Could not configure power good GPIO %s\n",
					good->name);
				return -1;
			}
		}

		/* Every rail that comes up must have a place to go down */
		for (j = 0; j < ARRAY_SIZE(power_off_sequence); j++) {
			if (power_off_sequence[j] == rail)
				break;
		}
		if (j == ARRAY_SIZE(power_off_sequence)) {
			LOG_ERR("%s is not in the power-off-sequence", rail->name);
			return -EINVAL;
		}
	}

	if (DT_NODE_HAS_STATUS_OKAY(GPIO_POWER_LED) &&
	    gpio_is_ready_dt(&power_led_gpio)) {
		if (gpio_pin_configure_dt(&power_led_gpio, GPIO_OUTPUT_INACTIVE) < 0)
			LOG_INF("Could not configure power LED\n");
	}

	return 0;
}

int power_auto_poweron(void)
{
	if (config_host_auto_poweron()) {
		int ret = power_set_state(true);
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
	return power_set_state(true);
}

static int cmd_power_off(const struct shell *sh, size_t argc, char **argv)
{
	ARG_UNUSED(argc);
	ARG_UNUSED(argv);
	return power_set_state(false);
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
	 * Host power is off at BMC boot (see power_init()), so hold the
	 * host in reset until power_on() has brought its rails up.
	 */
	ret = gpio_pin_configure_dt(&gpio_reset, GPIO_OUTPUT_ACTIVE);
	if (ret < 0) {
		LOG_INF("Could not configure reset GPIO\n");
		return -1;
	}

	return 0;
}

/*
 * If there is no reset GPIO, return -ENOTSUP so the caller can fall back
 * to power_cycle() to implement roughly equivalent behaviour.
 */
int power_reset(void)
{
	int ret;

	if (!DT_NODE_HAS_STATUS_OKAY(GPIO_RESET)) {
		/*
		 * Special case if there are also no power GPIOs.
		 * We are faking power and reset control with the
		 * power LED, so continue and blink the LED for reset.
		 */
		if (ARRAY_SIZE(power_on_sequence) > 0) {
			LOG_INF("No reset GPIO");
			return -ENOTSUP;
		}
	}

	k_mutex_lock(&power_lock, K_FOREVER);

	if (!system_power_state) {
		LOG_ERR("Host reset requested with power off\n");
		ret = -1;
		goto out;
	}

	ret = reset_set(1);
	if (ret < 0)
		goto out;

	if (DT_NODE_HAS_STATUS_OKAY(GPIO_POWER_LED) &&
	    gpio_is_ready_dt(&power_led_gpio)) {
		for (int i = 0; i < 10; i++) {
			gpio_pin_set_dt(&power_led_gpio, 0);
			k_msleep(50);
			gpio_pin_set_dt(&power_led_gpio, 1);
			k_msleep(50);
		}
	} else {
		k_msleep(1000);
	}

	ret = reset_set(0);

out:
	k_mutex_unlock(&power_lock);

	return ret;
}

static int cmd_reset(const struct shell *sh, size_t argc, char **argv)
{
	ARG_UNUSED(argc);
	ARG_UNUSED(argv);
	return power_reset();
}

SHELL_CMD_REGISTER(reset, NULL, "Reset.", cmd_reset);
