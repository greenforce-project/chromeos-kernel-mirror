// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019-2022 MediaTek Inc.
 * Copyright (c) 2022 BayLibre
 */


#include <linux/delay.h>
#include <linux/io.h>
#include <linux/mfd/syscon.h>
#include <linux/of.h>
#include <linux/phy/phy.h>
#include <linux/platform_device.h>
#include <linux/regmap.h>


#define PHYD_OFFSET			0x0000
#define PHYD_DIG_LAN0_OFFSET		0x1000
#define PHYD_DIG_LAN1_OFFSET		0x1100
#define PHYD_DIG_LAN2_OFFSET		0x1200
#define PHYD_DIG_LAN3_OFFSET		0x1300
#define PHYD_DIG_GLB_OFFSET		0x1400

#define DP_PHY_DIG_PLL_CTL_0		(PHYD_DIG_GLB_OFFSET + 0x10)
#define FORCE_PWORE_STATE_FLDMASK		GENMASK(2, 0)
#define FORCE_PWORE_STATE_VALUE			0x7

#define IPMUX_CONTROL			(PHYD_DIG_GLB_OFFSET + 0x98)
#define EDPTX_DSI_PHYD_SEL_FLDMASK		0x1
#define EDPTX_DSI_PHYD_SEL_FLDMASK_POS		0

#define DP_PHY_DIG_TX_CTL_0		(PHYD_DIG_GLB_OFFSET + 0x74)
#define TX_LN_EN_FLDMASK			0xf
#define DP_PHY_DIG_TX_CTL_0_MT8189	(PHYD_DIG_GLB_OFFSET + 0x74)
#define TX_LN_EN_FLDMASK_MT8189			0xf0

#define MTK_DP_PHY_DIG_PLL_CTL_1	(PHYD_DIG_GLB_OFFSET + 0x14)
#define TPLL_SSC_EN				BIT(8)

#define MTK_DP_PHY_DIG_BIT_RATE		(PHYD_DIG_GLB_OFFSET + 0x3C)
#define BIT_RATE_RBR				0x1
#define BIT_RATE_HBR				0x4
#define BIT_RATE_HBR2				0x7
#define BIT_RATE_HBR3				0x9
#define BIT_RATE_RBR_MT8189			0x0
#define BIT_RATE_HBR_MT8189			0x1
#define BIT_RATE_HBR2_MT8189			0x2
#define BIT_RATE_HBR3_MT8189			0x3

#define MTK_DP_PHY_DIG_SW_RST		(PHYD_DIG_GLB_OFFSET + 0x38)
#define DP_GLB_SW_RST_PHYD			BIT(0)
#define DP_GLB_SW_RST_PHYD_MASK			BIT(0)

#define DRIVING_FORCE 			0x30
#define EDP_TX_LN_VOLT_SWING_VAL_FLDMASK	0x6
#define EDP_TX_LN_VOLT_SWING_VAL_FLDMASK_POS	1
#define EDP_TX_LN_PRE_EMPH_VAL_FLDMASK		0x18
#define EDP_TX_LN_PRE_EMPH_VAL_FLDMASK_POS	3
#define DRIVING_FORCE_MT8189		0x18
#define EDP_TX_LN_VOLT_SWING_FLDMASK_MT8189	GENMASK(2, 1)
#define EDP_TX_LN_VOLT_SWING_FLDMASK_POS_MT8189	1
#define EDP_TX_LN_PRE_EMPH_FLDMASK_MT8189	GENMASK(6, 5)
#define EDP_TX_LN_PRE_EMPH_FLDMASK_POS_MT8189	5

#define MTK_DP_LANE0_DRIVING_PARAM_3	(PHYD_OFFSET + 0x138)
#define MTK_DP_LANE1_DRIVING_PARAM_3	(PHYD_OFFSET + 0x238)
#define MTK_DP_LANE2_DRIVING_PARAM_3	(PHYD_OFFSET + 0x338)
#define MTK_DP_LANE3_DRIVING_PARAM_3	(PHYD_OFFSET + 0x438)
#define XTP_LN_TX_LCTXC0_SW0_PRE0_DEFAULT	BIT(4)
#define XTP_LN_TX_LCTXC0_SW0_PRE1_DEFAULT	(BIT(10) | BIT(12))
#define XTP_LN_TX_LCTXC0_SW0_PRE2_DEFAULT	GENMASK(20, 19)
#define XTP_LN_TX_LCTXC0_SW0_PRE3_DEFAULT	GENMASK(29, 29)
#define DRIVING_PARAM_3_DEFAULT		(XTP_LN_TX_LCTXC0_SW0_PRE0_DEFAULT | \
					 XTP_LN_TX_LCTXC0_SW0_PRE1_DEFAULT | \
					 XTP_LN_TX_LCTXC0_SW0_PRE2_DEFAULT | \
					 XTP_LN_TX_LCTXC0_SW0_PRE3_DEFAULT)

#define XTP_LN_TX_LCTXC0_SW1_PRE0_DEFAULT	GENMASK(4, 3)
#define XTP_LN_TX_LCTXC0_SW1_PRE1_DEFAULT	GENMASK(12, 9)
#define XTP_LN_TX_LCTXC0_SW1_PRE2_DEFAULT	(BIT(18) | BIT(21))
#define XTP_LN_TX_LCTXC0_SW2_PRE0_DEFAULT	GENMASK(29, 29)
#define DRIVING_PARAM_4_DEFAULT		(XTP_LN_TX_LCTXC0_SW1_PRE0_DEFAULT | \
				 	XTP_LN_TX_LCTXC0_SW1_PRE1_DEFAULT | \
				 	XTP_LN_TX_LCTXC0_SW1_PRE2_DEFAULT | \
				 	XTP_LN_TX_LCTXC0_SW2_PRE0_DEFAULT)

#define XTP_LN_TX_LCTXC0_SW2_PRE1_DEFAULT	(BIT(3) | BIT(5))
#define XTP_LN_TX_LCTXC0_SW3_PRE0_DEFAULT	GENMASK(13, 12)
#define DRIVING_PARAM_5_DEFAULT		(XTP_LN_TX_LCTXC0_SW2_PRE1_DEFAULT | \
				 	XTP_LN_TX_LCTXC0_SW3_PRE0_DEFAULT)

#define XTP_LN_TX_LCTXCP1_SW0_PRE0_DEFAULT	0
#define XTP_LN_TX_LCTXCP1_SW0_PRE1_DEFAULT	GENMASK(10, 10)
#define XTP_LN_TX_LCTXCP1_SW0_PRE2_DEFAULT	GENMASK(19, 19)
#define XTP_LN_TX_LCTXCP1_SW0_PRE3_DEFAULT	GENMASK(28, 28)
#define DRIVING_PARAM_6_DEFAULT		(XTP_LN_TX_LCTXCP1_SW0_PRE0_DEFAULT | \
				 	XTP_LN_TX_LCTXCP1_SW0_PRE1_DEFAULT | \
				 	XTP_LN_TX_LCTXCP1_SW0_PRE2_DEFAULT | \
					 XTP_LN_TX_LCTXCP1_SW0_PRE3_DEFAULT)

#define XTP_LN_TX_LCTXCP1_SW1_PRE0_DEFAULT	0
#define XTP_LN_TX_LCTXCP1_SW1_PRE1_DEFAULT	GENMASK(10, 9)
#define XTP_LN_TX_LCTXCP1_SW1_PRE2_DEFAULT	GENMASK(19, 18)
#define XTP_LN_TX_LCTXCP1_SW2_PRE0_DEFAULT	0
#define DRIVING_PARAM_7_DEFAULT		(XTP_LN_TX_LCTXCP1_SW1_PRE0_DEFAULT | \
				 	XTP_LN_TX_LCTXCP1_SW1_PRE1_DEFAULT | \
				 	XTP_LN_TX_LCTXCP1_SW1_PRE2_DEFAULT | \
				 	XTP_LN_TX_LCTXCP1_SW2_PRE0_DEFAULT)

#define XTP_LN_TX_LCTXCP1_SW2_PRE1_DEFAULT	GENMASK(3, 3)
#define XTP_LN_TX_LCTXCP1_SW3_PRE0_DEFAULT	0
#define DRIVING_PARAM_8_DEFAULT	(XTP_LN_TX_LCTXCP1_SW2_PRE1_DEFAULT | \
				 XTP_LN_TX_LCTXCP1_SW3_PRE0_DEFAULT)

struct mtk_dp_phy {
	struct regmap *regs;
	u32 dp_phy_dig_tx_ctl;
	u32 tx_ln_en_mask;
	u8 bit_rate_rbr;
	u8 bit_rate_hbr1;
	u8 bit_rate_hbr2;
	u8 bit_rate_hbr3;
	u32 driving_force;
	u32 swing_value_mask;
	u8 swing_value_pos;
	u32 pre_emph_value_mask;
	u8 pre_emph_value_pos;
};

struct mtk_edp_phy_data  {
	struct regmap *phy_regs;
	u32 edp_ver;
};

enum DPTX_SWING_LEVEL {
	DPTX_SWING0 = 0x00,
	DPTX_SWING1 = 0x01,
	DPTX_SWING2 = 0x02,
	DPTX_SWING3 = 0x03,
};

enum DPTX_PREEMPHASIS_LEVEL {
	DPTX_PREEMPHASIS0 = 0x00,
	DPTX_PREEMPHASIS1 = 0x01,
	DPTX_PREEMPHASIS2 = 0x02,
	DPTX_PREEMPHASIS3 = 0x03,
};

enum DPTX_LANE_NUM {
	DPTX_LANE0 = 0x0,
	DPTX_LANE1 = 0x1,
	DPTX_LANE2 = 0x2,
	DPTX_LANE3 = 0x3,
	DPTX_LANE_MAX,
};

enum DPTX_LANE_COUNT {
	DPTX_LANE_COUNT1 = 0x1,
	DPTX_LANE_COUNT2 = 0x2,
	DPTX_LANE_COUNT4 = 0x4,
};

enum {
	MTK_EDP_SOC_TYPE_NONE  = 0,
	MTK_EDP_SOC_TYPE_MT8196,
	MTK_EDP_SOC_TYPE_MT8189,
};

static void mtk_dptx_phyd_reset_swing_pre(struct mtk_dp_phy *dp_phy)
{
	regmap_update_bits(dp_phy->regs, PHYD_DIG_LAN0_OFFSET + dp_phy->driving_force,
			   dp_phy->swing_value_mask |
			   dp_phy->pre_emph_value_mask, 0x0);
	regmap_update_bits(dp_phy->regs, PHYD_DIG_LAN1_OFFSET + dp_phy->driving_force,
			   dp_phy->swing_value_mask |
			   dp_phy->pre_emph_value_mask, 0x0);
	regmap_update_bits(dp_phy->regs, PHYD_DIG_LAN2_OFFSET + dp_phy->driving_force,
			   dp_phy->swing_value_mask |
			   dp_phy->pre_emph_value_mask, 0x0);
	regmap_update_bits(dp_phy->regs, PHYD_DIG_LAN3_OFFSET + dp_phy->driving_force,
			   dp_phy->swing_value_mask |
			   dp_phy->pre_emph_value_mask, 0x0);
}

static int mtk_dp_phy_init(struct phy *phy)
{
	struct mtk_dp_phy *dp_phy = phy_get_drvdata(phy);

	regmap_update_bits(dp_phy->regs, IPMUX_CONTROL, 0,
			   EDPTX_DSI_PHYD_SEL_FLDMASK);

	regmap_update_bits(dp_phy->regs, DP_PHY_DIG_PLL_CTL_0,
			   FORCE_PWORE_STATE_VALUE,
			   FORCE_PWORE_STATE_FLDMASK);

	return 0;
}

int mtk_dp_phy_configure(struct phy *phy, union phy_configure_opts *opts)
{
	struct mtk_dp_phy *dp_phy = phy_get_drvdata(phy);
	u32 val;

	if (opts->dp.set_rate) {
		switch (opts->dp.link_rate) {
		case 1620:
			val = dp_phy->bit_rate_rbr;
			break;
		case 2700:
			val = dp_phy->bit_rate_hbr1;
			break;
		case 5400:
			val = dp_phy->bit_rate_hbr2;
			break;
		case 8100:
			val = dp_phy->bit_rate_hbr3;
			break;
		default:
			dev_err(&phy->dev,
				"Implementation error, unknown linkrate %x\n",
				opts->dp.link_rate);
			return -EINVAL;
		}
		regmap_write(dp_phy->regs, MTK_DP_PHY_DIG_BIT_RATE, val);
	}

	if (opts->dp.set_lanes) {
		for (val = 0; val < 4; val++) {
			regmap_update_bits(dp_phy->regs, dp_phy->dp_phy_dig_tx_ctl,
					   ((1 << (val + 1)) - 1),
					   dp_phy->tx_ln_en_mask);
		}
	}

	if (opts->dp.set_voltages) {
		switch (opts->dp.lanes) {
		case DPTX_LANE_COUNT1:
			regmap_update_bits(dp_phy->regs, PHYD_DIG_LAN0_OFFSET +
					   dp_phy->driving_force,
					   dp_phy->swing_value_mask |
					   dp_phy->pre_emph_value_mask,
					   opts->dp.voltage[DPTX_LANE0] << dp_phy->swing_value_pos |
					   opts->dp.pre[DPTX_LANE0] << 3);
		break;
		case DPTX_LANE_COUNT2:
			regmap_update_bits(dp_phy->regs, PHYD_DIG_LAN0_OFFSET +
					   dp_phy->driving_force,
					   dp_phy->swing_value_mask |
					   dp_phy->pre_emph_value_mask,
					   opts->dp.voltage[DPTX_LANE0] << dp_phy->swing_value_pos |
					   opts->dp.pre[DPTX_LANE0] << dp_phy->pre_emph_value_pos);
			regmap_update_bits(dp_phy->regs, PHYD_DIG_LAN1_OFFSET +
					   dp_phy->driving_force,
					   dp_phy->swing_value_mask |
					   dp_phy->pre_emph_value_mask,
					   opts->dp.voltage[DPTX_LANE1] << dp_phy->swing_value_pos |
					   opts->dp.pre[DPTX_LANE1] << dp_phy->pre_emph_value_pos);
		break;
		case DPTX_LANE_COUNT4:
			regmap_update_bits(dp_phy->regs, PHYD_DIG_LAN0_OFFSET +
					   dp_phy->driving_force,
					   dp_phy->swing_value_mask |
					   dp_phy->pre_emph_value_mask,
					   opts->dp.voltage[DPTX_LANE0] <<
					   dp_phy->swing_value_pos |
					   opts->dp.pre[DPTX_LANE0] <<
					   dp_phy->pre_emph_value_pos);
			regmap_update_bits(dp_phy->regs, PHYD_DIG_LAN1_OFFSET +
					   dp_phy->driving_force,
					   dp_phy->swing_value_mask |
					   dp_phy->pre_emph_value_mask,
					   opts->dp.voltage[DPTX_LANE1] <<
					   dp_phy->swing_value_pos |
					   opts->dp.pre[DPTX_LANE1] <<
					   dp_phy->pre_emph_value_pos);
			regmap_update_bits(dp_phy->regs, PHYD_DIG_LAN2_OFFSET +
					   dp_phy->driving_force,
					   dp_phy->swing_value_mask |
					   dp_phy->pre_emph_value_mask,
					   opts->dp.voltage[DPTX_LANE2] <<
					   dp_phy->swing_value_pos |
					   opts->dp.pre[DPTX_LANE2] <<
					   dp_phy->pre_emph_value_pos);
			regmap_update_bits(dp_phy->regs, PHYD_DIG_LAN3_OFFSET +
					   dp_phy->driving_force,
					   dp_phy->swing_value_mask |
					   dp_phy->pre_emph_value_mask,
					   opts->dp.voltage[DPTX_LANE3] <<
					   dp_phy->swing_value_pos |
					   opts->dp.pre[DPTX_LANE3] <<
					   dp_phy->pre_emph_value_pos);
		break;
		default:
			dev_err(&phy->dev, "Wrong lanes config: %x\n",
				opts->dp.lanes);
			return -EINVAL;
		}
	}

	regmap_update_bits(dp_phy->regs, MTK_DP_PHY_DIG_PLL_CTL_1,
			   TPLL_SSC_EN, opts->dp.ssc ? 0 : TPLL_SSC_EN);

	return 0;
}

static int mtk_dp_phy_reset(struct phy *phy)
{
	struct mtk_dp_phy *dp_phy = phy_get_drvdata(phy);

	regmap_update_bits(dp_phy->regs, MTK_DP_PHY_DIG_SW_RST,
			   0, DP_GLB_SW_RST_PHYD_MASK);
	usleep_range(50, 200);
	regmap_update_bits(dp_phy->regs, MTK_DP_PHY_DIG_SW_RST,
			   DP_GLB_SW_RST_PHYD, DP_GLB_SW_RST_PHYD_MASK);
	regmap_update_bits(dp_phy->regs, dp_phy->dp_phy_dig_tx_ctl,
			   0x0, dp_phy->tx_ln_en_mask);
	mtk_dptx_phyd_reset_swing_pre(dp_phy);

	return 0;
}

static void mtk_dp_phy_initialize_priv_data(struct mtk_dp_phy *dp_phy,
					    struct mtk_edp_phy_data *regs)
{
	if (regs->edp_ver == MTK_EDP_SOC_TYPE_MT8196) {
		/* Initialize MT8196 specific data */
		dp_phy->dp_phy_dig_tx_ctl = DP_PHY_DIG_TX_CTL_0;
		dp_phy->tx_ln_en_mask = TX_LN_EN_FLDMASK;
		dp_phy->bit_rate_rbr = BIT_RATE_RBR;
		dp_phy->bit_rate_hbr1 = BIT_RATE_HBR;
		dp_phy->bit_rate_hbr2 = BIT_RATE_HBR2;
		dp_phy->bit_rate_hbr3 = BIT_RATE_HBR3;
		dp_phy->driving_force = DRIVING_FORCE;
		dp_phy->swing_value_mask = EDP_TX_LN_VOLT_SWING_VAL_FLDMASK;
		dp_phy->swing_value_pos = EDP_TX_LN_VOLT_SWING_VAL_FLDMASK_POS;
		dp_phy->pre_emph_value_mask = EDP_TX_LN_PRE_EMPH_VAL_FLDMASK;
		dp_phy->pre_emph_value_pos = EDP_TX_LN_PRE_EMPH_VAL_FLDMASK_POS;

	} else if (regs->edp_ver == MTK_EDP_SOC_TYPE_MT8189) {
		/* Initialize MT8189 specific data */
		dp_phy->dp_phy_dig_tx_ctl = DP_PHY_DIG_TX_CTL_0_MT8189;
		dp_phy->tx_ln_en_mask = TX_LN_EN_FLDMASK_MT8189;
		dp_phy->bit_rate_rbr = BIT_RATE_RBR_MT8189;
		dp_phy->bit_rate_hbr1 = BIT_RATE_HBR_MT8189;
		dp_phy->bit_rate_hbr2 = BIT_RATE_HBR2_MT8189;
		dp_phy->bit_rate_hbr3 = BIT_RATE_HBR3_MT8189;
		dp_phy->driving_force = DRIVING_FORCE_MT8189;
		dp_phy->swing_value_mask = EDP_TX_LN_VOLT_SWING_FLDMASK_MT8189;
		dp_phy->swing_value_pos = EDP_TX_LN_VOLT_SWING_FLDMASK_POS_MT8189;
		dp_phy->pre_emph_value_mask = EDP_TX_LN_PRE_EMPH_FLDMASK_MT8189;
		dp_phy->pre_emph_value_pos = EDP_TX_LN_PRE_EMPH_FLDMASK_POS_MT8189;
	}
}

static const struct phy_ops mtk_dp_phy_dev_ops = {
	.init = mtk_dp_phy_init,
	.configure = mtk_dp_phy_configure,
	.reset = mtk_dp_phy_reset,
	.owner = THIS_MODULE,
};

static int mtk_dp_phy_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct mtk_dp_phy *dp_phy;
	struct phy *phy;
	struct mtk_edp_phy_data *regs;

	regs = (struct mtk_edp_phy_data *)dev->platform_data;
	if (!regs)
		return dev_err_probe(dev, -EINVAL,
				     "No data passed, requires mtk_edp_phy_data\n");

	dp_phy = devm_kzalloc(dev, sizeof(*dp_phy), GFP_KERNEL);
	if (!dp_phy)
		return -ENOMEM;

	if (!regs->phy_regs)
		return dev_err_probe(dev, -EINVAL, "phy_regs is NULL");

	dp_phy->regs = regs->phy_regs;
	mtk_dp_phy_initialize_priv_data(dp_phy, regs);
	phy = devm_phy_create(dev, NULL, &mtk_dp_phy_dev_ops);
	if (IS_ERR(phy))
		return dev_err_probe(dev, PTR_ERR(phy),
				     "Failed to create DP PHY\n");

	phy_set_drvdata(phy, dp_phy);
	if (!dev->of_node)
		phy_create_lookup(phy, "edp", dev_name(dev));

	return 0;
}

struct platform_driver mtk_edp_phy_driver = {
	.probe = mtk_dp_phy_probe,
	.driver = {
		.name = "mediatek-edp-phy",
	},
};

module_platform_driver(mtk_edp_phy_driver);

MODULE_AUTHOR("Markus Schneider-Pargmann <msp@baylibre.com>");
MODULE_DESCRIPTION("MediaTek DP PHY Driver");
MODULE_LICENSE("GPL");
