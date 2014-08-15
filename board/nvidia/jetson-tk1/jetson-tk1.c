/*
 * (C) Copyright 2014
 * NVIDIA Corporation <www.nvidia.com>
 *
 * SPDX-License-Identifier:     GPL-2.0+
 */

#include <common.h>
#include <errno.h>
#include <asm/arch/gpio.h>
#include <asm/arch/pinmux.h>
#include "pinmux-config-jetson-tk1.h"
#include <i2c.h>
#include <netdev.h>

#define PMU_I2C_ADDRESS 0x40
#define AS3722_DEVICE_ID 0x0c

#define AS3722_SD_VOLTAGE(n) (0x00 + (n))
#define AS3722_GPIO_CONTROL(n) (0x08 + (n))
#define  AS3722_GPIO_CONTROL_MODE_OUTPUT_VDDH (1 << 0)
#define  AS3722_GPIO_CONTROL_MODE_OUTPUT_VDDL (7 << 0)
#define  AS3722_GPIO_CONTROL_INVERT (1 << 7)
#define AS3722_GPIO_SIGNAL_OUT 0x20
#define AS3722_SD_CONTROL 0x4d
#define AS3722_ASIC_ID1 0x90
#define AS3722_ASIC_ID2 0x91

/*
 * Routine: pinmux_init
 * Description: Do individual peripheral pinmux configs
 */
void pinmux_init(void)
{
	pinmux_set_tristate_input_clamping();

	gpio_config_table(jetson_tk1_gpio_inits,
			  ARRAY_SIZE(jetson_tk1_gpio_inits));

	pinmux_config_pingrp_table(jetson_tk1_pingrps,
				   ARRAY_SIZE(jetson_tk1_pingrps));

	pinmux_config_drvgrp_table(jetson_tk1_drvgrps,
				   ARRAY_SIZE(jetson_tk1_drvgrps));
}

#ifdef CONFIG_PCI_TEGRA
static int as3722_read(u8 reg, u8 *value)
{
	int err;

	err = i2c_read(PMU_I2C_ADDRESS, reg, 1, value, 1);
	if (err < 0)
		return err;

	return 0;
}

static int as3722_write(u8 reg, u8 value)
{
	int err;

	err = i2c_write(PMU_I2C_ADDRESS, reg, 1, &value, 1);
	if (err < 0)
		return err;

	return 0;
}

static int as3722_read_id(u8 *id, u8 *revision)
{
	int err;

	err = as3722_read(AS3722_ASIC_ID1, id);
	if (err) {
		error("as3722: failed to read ID1 register: %d\n", err);
		return err;
	}

	err = as3722_read(AS3722_ASIC_ID2, revision);
	if (err) {
		error("as3722: failed to read ID2 register: %d\n", err);
		return err;
	}

	return 0;
}

static int as3722_sd_enable(u8 sd)
{
	u8 value;
	int err;

	err = as3722_read(AS3722_SD_CONTROL, &value);
	if (err) {
		error("as3722: failed to read SD control register: %d\n", err);
		return err;
	}

	value |= 1 << sd;

	err = as3722_write(AS3722_SD_CONTROL, value);
	if (err < 0) {
		error("as3722: failed to write SD control register: %d\n", err);
		return err;
	}

	return 0;
}

static int as3722_sd_set_voltage(u8 sd, u8 value)
{
	int err;

	if (sd > 6)
		return -EINVAL;

	err = as3722_write(AS3722_SD_VOLTAGE(sd), value);
	if (err < 0) {
		error("as3722: failed to write SD%u voltage register: %d\n", sd, err);
		return err;
	}

	return 0;
}

static int as3722_gpio_set(u8 gpio, u8 level)
{
	u8 value;
	int err;

	if (gpio > 7)
		return -EINVAL;

	err = as3722_read(AS3722_GPIO_SIGNAL_OUT, &value);
	if (err < 0) {
		error("as3722: failed to read GPIO signal out register: %d\n",
		      err);
		return err;
	}

	if (level == 0)
		value &= ~(1 << gpio);
	else
		value |= 1 << gpio;

	err = as3722_write(AS3722_GPIO_SIGNAL_OUT, value);
	if (err) {
		error("as3722: failed to set GPIO#%u %s: %d\n", gpio,
		      (level == 0) ? "low" : "high", err);
		return err;
	}

	return 0;
}

static int as3722_gpio_direction_output(u8 gpio, u8 level)
{
	u8 value;
	int err;

	if (gpio > 7)
		return -EINVAL;

	if (level == 0)
		value = AS3722_GPIO_CONTROL_MODE_OUTPUT_VDDL;
	else
		value = AS3722_GPIO_CONTROL_MODE_OUTPUT_VDDH;

	err = as3722_write(AS3722_GPIO_CONTROL(gpio), value);
	if (err) {
		error("as3722: failed to configure GPIO#%u as output: %d\n",
		      gpio, err);
		return err;
	}

	err = as3722_gpio_set(gpio, level);
	if (err < 0) {
		error("as3722: failed to set GPIO#%u high: %d\n", gpio, err);
		return err;
	}

	return 0;
}

int tegra_pcie_board_init(void)
{
	u8 id, revision, value;
	unsigned int old_bus;
	int err;

	old_bus = i2c_get_bus_num();

	err = i2c_set_bus_num(0);
	if (err) {
		error("failed to set I2C bus\n");
		return err;
	}

	err = as3722_read_id(&id, &revision);
	if (err < 0) {
		error("as3722: failed to read ID: %d\n", err);
		return err;
	}

	if (id != AS3722_DEVICE_ID) {
		error("as3722: PMIC is not an AS3722\n");
		return -ENODEV;
	}

	err = as3722_sd_enable(4);
	if (err < 0) {
		error("as3722: failed to enable SD4: %d\n", err);
		return err;
	}

	err = as3722_sd_set_voltage(4, 0x24);
	if (err < 0) {
		error("as3722: failed to set SD4 voltage: %d\n", err);
		return err;
	}

	value = AS3722_GPIO_CONTROL_MODE_OUTPUT_VDDH |
		AS3722_GPIO_CONTROL_INVERT;

	err = as3722_write(AS3722_GPIO_CONTROL(1), value);
	if (err) {
		error("as3722: failed to configure GPIO#1 as output: %d\n", err);
		return err;
	}

	err = as3722_gpio_direction_output(2, 1);
	if (err < 0) {
		error("as3722: failed to set GPIO#2 high: %d\n", err);
		return err;
	}

	i2c_set_bus_num(old_bus);

	return 0;
}

int board_eth_init(bd_t *bis)
{
	return pci_eth_init(bis);
}
#endif /* PCI */
