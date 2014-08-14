#ifndef _TEGRA_XUSB_PADCTL_H_
#define _TEGRA_XUSB_PADCTL_H_

struct tegra_xusb_phy;

struct tegra_xusb_phy *tegra_xusb_phy_get(unsigned int type);

void tegra_xusb_padctl_init(const void *fdt);
int tegra_xusb_phy_prepare(struct tegra_xusb_phy *phy);
int tegra_xusb_phy_enable(struct tegra_xusb_phy *phy);
int tegra_xusb_phy_disable(struct tegra_xusb_phy *phy);
int tegra_xusb_phy_unprepare(struct tegra_xusb_phy *phy);

#endif
