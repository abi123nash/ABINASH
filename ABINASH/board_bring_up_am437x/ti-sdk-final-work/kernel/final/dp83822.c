// SPDX-License-Identifier: GPL-2.0
/* Driver for the Texas Instruments DP83822, DP83825 and DP83826 PHYs.
 *
 * Copyright (C) 2017 Texas Instruments Inc.
 */

#include <linux/ethtool.h>
#include <linux/etherdevice.h>
#include <linux/kernel.h>
#include <linux/mii.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/phy.h>
#include <linux/netdevice.h>
#include <linux/of_gpio.h>
#include <linux/gpio.h>
#include <linux/delay.h>

#define DP83822_PHY_ID	        0x2000a240
#define DP83825S_PHY_ID		0x2000a140
#define DP83825I_PHY_ID		0x2000a150
#define DP83825CM_PHY_ID	0x2000a160
#define DP83825CS_PHY_ID	0x2000a170
#define DP83826C_PHY_ID		0x2000a130
#define DP83826NC_PHY_ID	0x2000a110

#define DP83822_DEVADDR		0x1f

#define MII_DP83822_CTRL_2	0x0a
#define MII_DP83822_PHYSTS	0x10
#define MII_DP83822_PHYSCR	0x11
#define MII_DP83822_MISR1	0x12
#define MII_DP83822_MISR2	0x13
#define MII_DP83822_FCSCR	0x14
#define MII_DP83822_RCSR	0x17
#define MII_DP83822_RESET_CTRL	0x1f
#define MII_DP83822_GENCFG	0x465
#define MII_DP83822_SOR1	0x467

/* GENCFG */
#define DP83822_SIG_DET_LOW	BIT(0)

/* Control Register 2 bits */
#define DP83822_FX_ENABLE	BIT(14)

#define DP83822_HW_RESET	BIT(15)
#define DP83822_SW_RESET	BIT(14)

/* PHY STS bits */
#define DP83822_PHYSTS_DUPLEX			BIT(2)
#define DP83822_PHYSTS_10			BIT(1)
#define DP83822_PHYSTS_LINK			BIT(0)

/* PHYSCR Register Fields */
#define DP83822_PHYSCR_INT_OE		BIT(0) /* Interrupt Output Enable */
#define DP83822_PHYSCR_INTEN		BIT(1) /* Interrupt Enable */

/* MISR1 bits */
#define DP83822_RX_ERR_HF_INT_EN	BIT(0)
#define DP83822_FALSE_CARRIER_HF_INT_EN	BIT(1)
#define DP83822_ANEG_COMPLETE_INT_EN	BIT(2)
#define DP83822_DUP_MODE_CHANGE_INT_EN	BIT(3)
#define DP83822_SPEED_CHANGED_INT_EN	BIT(4)
#define DP83822_LINK_STAT_INT_EN	BIT(5)
#define DP83822_ENERGY_DET_INT_EN	BIT(6)
#define DP83822_LINK_QUAL_INT_EN	BIT(7)

/* MISR2 bits */
#define DP83822_JABBER_DET_INT_EN	BIT(0)
#define DP83822_WOL_PKT_INT_EN		BIT(1)
#define DP83822_SLEEP_MODE_INT_EN	BIT(2)
#define DP83822_MDI_XOVER_INT_EN	BIT(3)
#define DP83822_LB_FIFO_INT_EN		BIT(4)
#define DP83822_PAGE_RX_INT_EN		BIT(5)
#define DP83822_ANEG_ERR_INT_EN		BIT(6)
#define DP83822_EEE_ERROR_CHANGE_INT_EN	BIT(7)

/* INT_STAT1 bits */
#define DP83822_WOL_INT_EN	BIT(4)
#define DP83822_WOL_INT_STAT	BIT(12)

#define MII_DP83822_RXSOP1	0x04a5
#define	MII_DP83822_RXSOP2	0x04a6
#define	MII_DP83822_RXSOP3	0x04a7

/* WoL Registers */
#define	MII_DP83822_WOL_CFG	0x04a0
#define	MII_DP83822_WOL_STAT	0x04a1
#define	MII_DP83822_WOL_DA1	0x04a2
#define	MII_DP83822_WOL_DA2	0x04a3
#define	MII_DP83822_WOL_DA3	0x04a4

/* WoL bits */
#define DP83822_WOL_MAGIC_EN	BIT(0)
#define DP83822_WOL_SECURE_ON	BIT(5)
#define DP83822_WOL_EN		BIT(7)
#define DP83822_WOL_INDICATION_SEL BIT(8)
#define DP83822_WOL_CLR_INDICATION BIT(11)

/* RSCR bits */
#define DP83822_RX_CLK_SHIFT	BIT(12)
#define DP83822_TX_CLK_SHIFT	BIT(11)

/* SOR1 mode */
#define DP83822_STRAP_MODE1	0
#define DP83822_STRAP_MODE2	BIT(0)
#define DP83822_STRAP_MODE3	BIT(1)
#define DP83822_STRAP_MODE4	GENMASK(1, 0)

#define DP83822_COL_STRAP_MASK	GENMASK(11, 10)
#define DP83822_COL_SHIFT	10
#define DP83822_RX_ER_STR_MASK	GENMASK(9, 8)
#define DP83822_RX_ER_SHIFT	8

#define MII_DP83822_FIBER_ADVERTISE    (ADVERTISED_TP | ADVERTISED_MII | \
					ADVERTISED_FIBRE | ADVERTISED_BNC |  \
					ADVERTISED_Pause | ADVERTISED_Asym_Pause | \
					ADVERTISED_100baseT_Full)

unsigned int link_status_phy0;
unsigned int link_status_phy1;
unsigned int link_data_phy0;
unsigned int link_data_phy1;
struct dp83822_private {
	bool fx_signal_det_low;
	int fx_enabled;
	u16 fx_sd_enable;
};

static int dp83822_ack_interrupt(struct phy_device *phydev)
{
	int err;

	err = phy_read(phydev, MII_DP83822_MISR1);
	if (err < 0)
		return err;

	err = phy_read(phydev, MII_DP83822_MISR2);
	if (err < 0)
		return err;

	return 0;
}

static int dp83822_set_wol(struct phy_device *phydev,
			   struct ethtool_wolinfo *wol)
{
	struct net_device *ndev = phydev->attached_dev;
	u16 value;
	const u8 *mac;

	if (wol->wolopts & (WAKE_MAGIC | WAKE_MAGICSECURE)) {
		mac = (const u8 *)ndev->dev_addr;

		if (!is_valid_ether_addr(mac))
			return -EINVAL;

		/* MAC addresses start with byte 5, but stored in mac[0].
		 * 822 PHYs store bytes 4|5, 2|3, 0|1
		 */
		phy_write_mmd(phydev, DP83822_DEVADDR, MII_DP83822_WOL_DA1,
			      (mac[1] << 8) | mac[0]);
		phy_write_mmd(phydev, DP83822_DEVADDR, MII_DP83822_WOL_DA2,
			      (mac[3] << 8) | mac[2]);
		phy_write_mmd(phydev, DP83822_DEVADDR, MII_DP83822_WOL_DA3,
			      (mac[5] << 8) | mac[4]);

		value = phy_read_mmd(phydev, DP83822_DEVADDR,
				     MII_DP83822_WOL_CFG);
		if (wol->wolopts & WAKE_MAGIC)
			value |= DP83822_WOL_MAGIC_EN;
		else
			value &= ~DP83822_WOL_MAGIC_EN;

		if (wol->wolopts & WAKE_MAGICSECURE) {
			phy_write_mmd(phydev, DP83822_DEVADDR,
				      MII_DP83822_RXSOP1,
				      (wol->sopass[1] << 8) | wol->sopass[0]);
			phy_write_mmd(phydev, DP83822_DEVADDR,
				      MII_DP83822_RXSOP2,
				      (wol->sopass[3] << 8) | wol->sopass[2]);
			phy_write_mmd(phydev, DP83822_DEVADDR,
				      MII_DP83822_RXSOP3,
				      (wol->sopass[5] << 8) | wol->sopass[4]);
			value |= DP83822_WOL_SECURE_ON;
		} else {
			value &= ~DP83822_WOL_SECURE_ON;
		}

		/* Clear any pending WoL interrupt */
		phy_read(phydev, MII_DP83822_MISR2);

		value |= DP83822_WOL_EN | DP83822_WOL_INDICATION_SEL |
			 DP83822_WOL_CLR_INDICATION;

		return phy_write_mmd(phydev, DP83822_DEVADDR,
				     MII_DP83822_WOL_CFG, value);
	} else {
		return phy_clear_bits_mmd(phydev, DP83822_DEVADDR,
					  MII_DP83822_WOL_CFG, DP83822_WOL_EN);
	}
}

static void dp83822_get_wol(struct phy_device *phydev,
			    struct ethtool_wolinfo *wol)
{
	int value;
	u16 sopass_val;

	wol->supported = (WAKE_MAGIC | WAKE_MAGICSECURE);
	wol->wolopts = 0;

	value = phy_read_mmd(phydev, DP83822_DEVADDR, MII_DP83822_WOL_CFG);

	if (value & DP83822_WOL_MAGIC_EN)
		wol->wolopts |= WAKE_MAGIC;

	if (value & DP83822_WOL_SECURE_ON) {
		sopass_val = phy_read_mmd(phydev, DP83822_DEVADDR,
					  MII_DP83822_RXSOP1);
		wol->sopass[0] = (sopass_val & 0xff);
		wol->sopass[1] = (sopass_val >> 8);

		sopass_val = phy_read_mmd(phydev, DP83822_DEVADDR,
					  MII_DP83822_RXSOP2);
		wol->sopass[2] = (sopass_val & 0xff);
		wol->sopass[3] = (sopass_val >> 8);

		sopass_val = phy_read_mmd(phydev, DP83822_DEVADDR,
					  MII_DP83822_RXSOP3);
		wol->sopass[4] = (sopass_val & 0xff);
		wol->sopass[5] = (sopass_val >> 8);

		wol->wolopts |= WAKE_MAGICSECURE;
	}

	/* WoL is not enabled so set wolopts to 0 */
	if (!(value & DP83822_WOL_EN))
		wol->wolopts = 0;
}

static int dp83822_config_intr(struct phy_device *phydev)
{
	struct dp83822_private *dp83822 = phydev->priv;
	int misr_status;
	int physcr_status;
	int err;

	if (phydev->interrupts == PHY_INTERRUPT_ENABLED) {
		misr_status = phy_read(phydev, MII_DP83822_MISR1);
		if (misr_status < 0)
			return misr_status;

		misr_status |= (DP83822_RX_ERR_HF_INT_EN |
				DP83822_FALSE_CARRIER_HF_INT_EN |
				DP83822_LINK_STAT_INT_EN |
				DP83822_ENERGY_DET_INT_EN |
				DP83822_LINK_QUAL_INT_EN);

		if (!dp83822->fx_enabled)
			misr_status |= DP83822_ANEG_COMPLETE_INT_EN |
				       DP83822_DUP_MODE_CHANGE_INT_EN |
				       DP83822_SPEED_CHANGED_INT_EN;


		err = phy_write(phydev, MII_DP83822_MISR1, misr_status);
		if (err < 0)
			return err;

		misr_status = phy_read(phydev, MII_DP83822_MISR2);
		if (misr_status < 0)
			return misr_status;

		misr_status |= (DP83822_JABBER_DET_INT_EN |
				DP83822_SLEEP_MODE_INT_EN |
				DP83822_LB_FIFO_INT_EN |
				DP83822_PAGE_RX_INT_EN |
				DP83822_EEE_ERROR_CHANGE_INT_EN);

		if (!dp83822->fx_enabled)
			misr_status |= DP83822_MDI_XOVER_INT_EN |
				       DP83822_ANEG_ERR_INT_EN |
				       DP83822_WOL_PKT_INT_EN;

		err = phy_write(phydev, MII_DP83822_MISR2, misr_status);
		if (err < 0)
			return err;

		physcr_status = phy_read(phydev, MII_DP83822_PHYSCR);
		if (physcr_status < 0)
			return physcr_status;

		physcr_status |= DP83822_PHYSCR_INT_OE | DP83822_PHYSCR_INTEN;

	} else {
		err = phy_write(phydev, MII_DP83822_MISR1, 0);
		if (err < 0)
			return err;

		err = phy_write(phydev, MII_DP83822_MISR1, 0);
		if (err < 0)
			return err;

		physcr_status = phy_read(phydev, MII_DP83822_PHYSCR);
		if (physcr_status < 0)
			return physcr_status;

		physcr_status &= ~DP83822_PHYSCR_INTEN;
	}

	return phy_write(phydev, MII_DP83822_PHYSCR, physcr_status);
}

static int dp8382x_disable_wol(struct phy_device *phydev)
{
	int value = DP83822_WOL_EN | DP83822_WOL_MAGIC_EN |
		    DP83822_WOL_SECURE_ON;

	return phy_clear_bits_mmd(phydev, DP83822_DEVADDR,
				  MII_DP83822_WOL_CFG, value);
}




static int dp83822_read_status(struct phy_device *phydev)
{
	struct dp83822_private *dp83822 = phydev->priv;
	int status = phy_read(phydev, MII_DP83822_PHYSTS);
	int ctrl2;
	int ret;
	static int prev_lstatus[2] = {-1, -1};
	int lstatus = 1;
           if (dp83822->fx_enabled) {
		if (status & DP83822_PHYSTS_LINK) {
			phydev->speed = SPEED_UNKNOWN;
			phydev->duplex = DUPLEX_UNKNOWN;
		} else {
                        lstatus = 0;
			ctrl2 = phy_read(phydev, MII_DP83822_CTRL_2);
			if (ctrl2 < 0)
				return ctrl2;

			if (!(ctrl2 & DP83822_FX_ENABLE)) {
				ret = phy_write(phydev, MII_DP83822_CTRL_2,
						DP83822_FX_ENABLE | ctrl2);
				if (ret < 0)
					return ret;
			}
		}
	}

	ret = genphy_read_status(phydev);
	if (ret)
		return ret;

	if (status < 0)
		return status;

	if (status & DP83822_PHYSTS_DUPLEX)
		phydev->duplex = DUPLEX_FULL;
	else
		phydev->duplex = DUPLEX_HALF;

	if (status & DP83822_PHYSTS_10)
		phydev->speed = SPEED_10;
	else
		phydev->speed = SPEED_100;

#if 1
	if (status & DP83822_PHYSTS_LINK)
	{
	     if ((0 == phydev->mdio.addr) && (prev_lstatus[phydev->mdio.addr] != 1))
	     {
                 gpio_set_value(link_status_phy0, 0);
                 gpio_set_value(link_data_phy0, 0);
	     }
	     else
	     if (prev_lstatus[phydev->mdio.addr] != 1)
	     {
                 gpio_set_value(link_status_phy1, 0);
                 gpio_set_value(link_data_phy1, 0);
	     }
	     prev_lstatus[phydev->mdio.addr] = 1;
	}
	else
	{
	     if ((0 == phydev->mdio.addr) && (prev_lstatus[phydev->mdio.addr] != 0))
	     {
                 gpio_set_value(link_status_phy0, 1);
                 gpio_set_value(link_data_phy0, 1);
	     }
	     else
	     if (prev_lstatus[phydev->mdio.addr] != 0)
	     {
                 gpio_set_value(link_status_phy1, 1);
                 gpio_set_value(link_data_phy1, 1);
	     }
	     prev_lstatus[phydev->mdio.addr] = 0;
	}
#endif
	return 0;
}

static int dp83822_config_init(struct phy_device *phydev)
{
	struct dp83822_private *dp83822 = phydev->priv;
	struct device *dev = &phydev->mdio.dev;
	int rgmii_delay;
	s32 rx_int_delay;
	s32 tx_int_delay;
	int err = 0;
	int bmcr;

	if (phy_interface_is_rgmii(phydev)) {
		rx_int_delay = phy_get_internal_delay(phydev, dev, NULL, 0,
						      true);

		if (rx_int_delay <= 0)
			rgmii_delay = 0;
		else
			rgmii_delay = DP83822_RX_CLK_SHIFT;

		tx_int_delay = phy_get_internal_delay(phydev, dev, NULL, 0,
						      false);
		if (tx_int_delay <= 0)
			rgmii_delay &= ~DP83822_TX_CLK_SHIFT;
		else
			rgmii_delay |= DP83822_TX_CLK_SHIFT;

		if (rgmii_delay) {
			err = phy_set_bits_mmd(phydev, DP83822_DEVADDR,
					       MII_DP83822_RCSR, rgmii_delay);
			if (err)
				return err;
		}
	}

	if (dp83822->fx_enabled) {
		err = phy_modify(phydev, MII_DP83822_CTRL_2,
				 DP83822_FX_ENABLE, 1);
		if (err < 0)
			return err;

		/* Only allow advertising what this PHY supports */
		linkmode_and(phydev->advertising, phydev->advertising,
			     phydev->supported);

		linkmode_set_bit(ETHTOOL_LINK_MODE_FIBRE_BIT,
				 phydev->supported);
		linkmode_set_bit(ETHTOOL_LINK_MODE_FIBRE_BIT,
				 phydev->advertising);

		/* Auto neg is not supported in fiber mode */
		bmcr = phy_read(phydev, MII_BMCR);
		if (bmcr < 0)
			return bmcr;

		if (bmcr & BMCR_ANENABLE) {
			err =  phy_modify(phydev, MII_BMCR, BMCR_ANENABLE, 0);
			if (err < 0)
				return err;
		}
		phydev->autoneg = AUTONEG_DISABLE;
		linkmode_clear_bit(ETHTOOL_LINK_MODE_Autoneg_BIT,
				   phydev->supported);
		linkmode_clear_bit(ETHTOOL_LINK_MODE_Autoneg_BIT,
				   phydev->advertising);

		/* Setup fiber advertisement */
		err = phy_modify_changed(phydev, MII_ADVERTISE,
					 MII_DP83822_FIBER_ADVERTISE,
					 MII_DP83822_FIBER_ADVERTISE);

		if (err < 0)
			return err;

		if (dp83822->fx_signal_det_low) {
			err = phy_set_bits_mmd(phydev, DP83822_DEVADDR,
					       MII_DP83822_GENCFG,
					       DP83822_SIG_DET_LOW);
			if (err)
				return err;
		}
	}
	return dp8382x_disable_wol(phydev);
}

static int dp8382x_config_init(struct phy_device *phydev)
{
	return dp8382x_disable_wol(phydev);
}

static int dp83822_phy_reset(struct phy_device *phydev)
{
	int err;

	err = phy_write(phydev, MII_DP83822_RESET_CTRL, DP83822_SW_RESET);
	if (err < 0)
		return err;

	return phydev->drv->config_init(phydev);
}

#ifdef CONFIG_OF_MDIO

#define __LED_STATUS__

static int dp83822_of_init(struct phy_device *phydev)
{
	struct dp83822_private *dp83822 = phydev->priv;
	struct device *dev = &phydev->mdio.dev;
        struct device_node *np = dev->of_node;
        static int read_dts = 0;
	/* Signal detection for the PHY is only enabled if the FX_EN and the
	 * SD_EN pins are strapped. Signal detection can only enabled if FX_EN
	 * is strapped otherwise signal detection is disabled for the PHY.
	 */
	if (dp83822->fx_enabled && dp83822->fx_sd_enable)
		dp83822->fx_signal_det_low = device_property_present(dev,
								     "ti,link-loss-low");
	if (!dp83822->fx_enabled)
		dp83822->fx_enabled = device_property_present(dev,
							      "ti,fiber-mode");

#ifdef __LED_STATUS__

	if (0 == read_dts)
	{
            printk("Reading GPIO pin from the DTS %s \r\n", np->name);
            link_status_phy0 = of_get_named_gpio(np, "link-status-phy0", 0);
            link_status_phy1 = of_get_named_gpio(np, "link-status-phy1", 0);
            link_data_phy0 = of_get_named_gpio(np, "link-data-phy0", 0);
            link_data_phy1 = of_get_named_gpio(np, "link-data-phy1", 0);
            printk("Link Phy0 Status GPIO = %d \r\n", link_status_phy0); 
            printk("Link Phy1 Status GPIO = %d \r\n", link_status_phy1); 
            printk("Link Phy0 Data GPIO = %d \r\n", link_data_phy0); 
            printk("Link Phy1 Data GPIO = %d \r\n", link_data_phy1); 


	    devm_gpio_request_one(dev, link_status_phy0,
                                        GPIOF_OUT_INIT_HIGH, "phy0 link status");
	    devm_gpio_request_one(dev, link_status_phy1,
                                        GPIOF_OUT_INIT_HIGH, "phy1 link status");
	    devm_gpio_request_one(dev, link_data_phy0,
                                        GPIOF_OUT_INIT_HIGH, "phy0 data");
	    devm_gpio_request_one(dev, link_data_phy1,
                                        GPIOF_OUT_INIT_HIGH, "phy1 data");
	    read_dts = 1;
	}
#endif	
	return 0;
}
#else
static int dp83822_of_init(struct phy_device *phydev)
{
	return 0;
}
#endif /* CONFIG_OF_MDIO */

static int dp83822_read_straps(struct phy_device *phydev)
{
	struct dp83822_private *dp83822 = phydev->priv;
	int fx_enabled, fx_sd_enable;
	int val;

	val = phy_read_mmd(phydev, DP83822_DEVADDR, MII_DP83822_SOR1);
	if (val < 0)
		return val;

	fx_enabled = (val & DP83822_COL_STRAP_MASK) >> DP83822_COL_SHIFT;
	if (fx_enabled == DP83822_STRAP_MODE2 ||
	    fx_enabled == DP83822_STRAP_MODE3)
		dp83822->fx_enabled = 1;

	if (dp83822->fx_enabled) {
		fx_sd_enable = (val & DP83822_RX_ER_STR_MASK) >> DP83822_RX_ER_SHIFT;
		if (fx_sd_enable == DP83822_STRAP_MODE3 ||
		    fx_sd_enable == DP83822_STRAP_MODE4)
			dp83822->fx_sd_enable = 1;
	}

	return 0;
}

static int dp83822_probe(struct phy_device *phydev)
{
	struct dp83822_private *dp83822;
	int ret;

	dp83822 = devm_kzalloc(&phydev->mdio.dev, sizeof(*dp83822),
			       GFP_KERNEL);
	if (!dp83822)
		return -ENOMEM;

	phydev->priv = dp83822;

	ret = dp83822_read_straps(phydev);
	if (ret)
		return ret;

	dp83822_of_init(phydev);

	return 0;
}

static int dp83822_suspend(struct phy_device *phydev)
{
	int value;

	value = phy_read_mmd(phydev, DP83822_DEVADDR, MII_DP83822_WOL_CFG);

	if (!(value & DP83822_WOL_EN))
		genphy_suspend(phydev);

	return 0;
}

static int dp83822_resume(struct phy_device *phydev)
{
	int value;

	genphy_resume(phydev);

	value = phy_read_mmd(phydev, DP83822_DEVADDR, MII_DP83822_WOL_CFG);

	phy_write_mmd(phydev, DP83822_DEVADDR, MII_DP83822_WOL_CFG, value |
		      DP83822_WOL_CLR_INDICATION);

	return 0;
}

#define DP83822_PHY_DRIVER(_id, _name)				\
	{							\
		PHY_ID_MATCH_MODEL(_id),			\
		.name		= (_name),			\
		/* PHY_BASIC_FEATURES */			\
		.probe          = dp83822_probe,		\
		.soft_reset	= dp83822_phy_reset,		\
		.config_init	= dp83822_config_init,		\
		.read_status	= dp83822_read_status,		\
		.get_wol = dp83822_get_wol,			\
		.set_wol = dp83822_set_wol,			\
		.ack_interrupt = dp83822_ack_interrupt,		\
		.config_intr = dp83822_config_intr,		\
		.suspend = dp83822_suspend,			\
		.resume = dp83822_resume,			\
	}

#define DP8382X_PHY_DRIVER(_id, _name)				\
	{							\
		PHY_ID_MATCH_MODEL(_id),			\
		.name		= (_name),			\
		/* PHY_BASIC_FEATURES */			\
		.soft_reset	= dp83822_phy_reset,		\
		.config_init	= dp8382x_config_init,		\
		.get_wol = dp83822_get_wol,			\
		.set_wol = dp83822_set_wol,			\
		.ack_interrupt = dp83822_ack_interrupt,		\
		.config_intr = dp83822_config_intr,		\
		.suspend = dp83822_suspend,			\
		.resume = dp83822_resume,			\
	}

static struct phy_driver dp83822_driver[] = {
	DP83822_PHY_DRIVER(DP83822_PHY_ID, "TI DP83822"),
	DP8382X_PHY_DRIVER(DP83825I_PHY_ID, "TI DP83825I"),
	DP8382X_PHY_DRIVER(DP83826C_PHY_ID, "TI DP83826C"),
	DP8382X_PHY_DRIVER(DP83826NC_PHY_ID, "TI DP83826NC"),
	DP8382X_PHY_DRIVER(DP83825S_PHY_ID, "TI DP83825S"),
	DP8382X_PHY_DRIVER(DP83825CM_PHY_ID, "TI DP83825M"),
	DP8382X_PHY_DRIVER(DP83825CS_PHY_ID, "TI DP83825CS"),
};
module_phy_driver(dp83822_driver);

static struct mdio_device_id __maybe_unused dp83822_tbl[] = {
	{ DP83822_PHY_ID, 0xfffffff0 },
	{ DP83825I_PHY_ID, 0xfffffff0 },
	{ DP83826C_PHY_ID, 0xfffffff0 },
	{ DP83826NC_PHY_ID, 0xfffffff0 },
	{ DP83825S_PHY_ID, 0xfffffff0 },
	{ DP83825CM_PHY_ID, 0xfffffff0 },
	{ DP83825CS_PHY_ID, 0xfffffff0 },
	{ },
};
MODULE_DEVICE_TABLE(mdio, dp83822_tbl);

MODULE_DESCRIPTION("Texas Instruments DP83822 PHY driver");
MODULE_AUTHOR("Dan Murphy <dmurphy@ti.com");
MODULE_LICENSE("GPL v2");
