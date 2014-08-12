#include <common.h>

#include <asm/io.h>
#include <asm/types.h>

#include <asm/arch/powergate.h>
#include <asm/arch/tegra.h>

#define PWRGATE_TOGGLE 0x30
#define  PWRGATE_TOGGLE_START (1 << 8)

#define PWRGATE_STATUS 0x38

static int tegra_powergate_set(enum tegra_powergate id, bool state)
{
	unsigned long value;
	bool old_state;

	value = readl(NV_PA_PMC_BASE + PWRGATE_STATUS);
	old_state = value & (1 << id);

	if (state == old_state)
		return 0;

	writel(PWRGATE_TOGGLE_START | id, NV_PA_PMC_BASE + PWRGATE_TOGGLE);

	return 0;
}

static int tegra_powergate_power_on(enum tegra_powergate id)
{
	return tegra_powergate_set(id, true);
}

int tegra_powergate_power_off(enum tegra_powergate id)
{
	return tegra_powergate_set(id, false);
}

static int tegra_powergate_remove_clamping(enum tegra_powergate id)
{
	unsigned long value;

	if (id == TEGRA_POWERGATE_VDEC)
		value = 1 << TEGRA_POWERGATE_PCIE;
	else if (id == TEGRA_POWERGATE_PCIE)
		value = 1 << TEGRA_POWERGATE_VDEC;
	else
		value = 1 << id;

	writel(value, NV_PA_PMC_BASE + 0x34);

	return 0;
}

int tegra_powergate_sequence_power_up(enum tegra_powergate id,
				      enum periph_id periph)
{
	int err;

	reset_set_enable(periph, 1);

	err = tegra_powergate_power_on(id);
	if (err < 0)
		return err;

	clock_enable(periph);

	udelay(10);

	err = tegra_powergate_remove_clamping(id);
	if (err < 0)
		return err;

	udelay(10);

	reset_set_enable(periph, 0);

	return 0;
}
