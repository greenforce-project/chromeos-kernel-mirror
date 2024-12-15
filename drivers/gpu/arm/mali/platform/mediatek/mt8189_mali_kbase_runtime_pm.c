// SPDX-License-Identifier: GPL-2.0
// Copyright (c) 2025 Mediatek Inc.

#include "mali_kbase_config_platform.h"
#include "mali_kbase_runtime_pm.h"

#include <linux/nvmem-consumer.h>

/* number of sign-off opp which avs specified */
#define MT8189_AVS_CNT 3

/* volt(uv*10)/freq(MHz) to amplify slope */
#define VOLT_INTERPOLATE(freq1, volt1, freq2, volt2, freq) \
			(volt1 + \
			(((volt2 - volt1) * 10) / ((freq2 - freq1) / 1000000) * \
			((freq - freq1) / 1000000) / 10))

/* volt should multiples of 6250uv */
#define VOLT_NORMALIZATION(volt) \
			((volt % 6250) ? (volt - (volt % 6250) + 6250) : volt)

/*
 * GPU opp information
 * idx:  index of current opp in framework opp table (freq high to low)
 * freq: frequency of current opp (Hz)
 * volt: voltage of current opp (uV)
 */
struct gpu_opp_info {
	int idx;
	unsigned long freq;
	unsigned long volt;
};

/* Upstream binding only uses one clock */
static const char * const mt8189_gpu_clks[] = { NULL };

const struct mtk_hw_config mt8189_hw_config = {
	.num_pm_domains = 2,
	.num_clks = ARRAY_SIZE(mt8189_gpu_clks),
	.clk_names = mt8189_gpu_clks,
	.mfg_compatible_name = "mediatek,mt8189-mfgcfg",
	.reg_mfg_timestamp = 0x130,
	.reg_mfg_qchannel_con = 0xb4,
	.reg_mfg_debug_sel = 0x170,
	.reg_mfg_debug_top = 0x178,
	.top_tsvalueb_en = 0x3,
	.bus_idle_bit = 0x4,
	.vgpu_min_microvolt = 575000,
	.vgpu_max_microvolt = 900000,
	.vsram_gpu_min_microvolt = 750000,
	.vsram_gpu_max_microvolt = 900000,
	.bias_min_microvolt = 0,
	.bias_max_microvolt = 250000,
	.supply_tolerance_microvolt = 125,
	.gpu_freq_min_khz = 390000,
	.gpu_freq_max_khz = 1100000,
	.auto_suspend_delay_ms = 50,
};

struct mtk_platform_context mt8189_platform_context = {
	.config = &mt8189_hw_config,
};

/* parse avs table from NVMEM framework */
static int avs_parse_efuse(struct kbase_device *kbdev, struct gpu_opp_info *avs_table)
{
	struct mtk_platform_context *ctx = kbdev->platform_context;
	const struct mtk_hw_config *cfg = ctx->config;
	int i, err;
	char avs_cell[12];
	unsigned long freq, volt;
	u32 avs_val, avs_margin[MT8189_AVS_CNT] = {112500, 81250, 75000};
	unsigned long avs_freq[MT8189_AVS_CNT] = {1100000000, 700000000, 390000000};

	for (i = 0; i < MT8189_AVS_CNT; i++) {
		snprintf(avs_cell, sizeof(avs_cell), "avs-bin-%d", i);
		err = nvmem_cell_read_variable_le_u32(kbdev->dev, avs_cell, &avs_val);
		if (err) {
			dev_err(kbdev->dev, "No %s decteced\n", avs_cell);
			return err;
		}

		dev_dbg(kbdev->dev, "Detected %s: 0x%08X\n", avs_cell, avs_val);

		/*
		 * avs efuse format:
		 * [0:7]:  vgpu * 6250uv
		 * [8:18]: freq * 1MHz
		 */
		freq = ((avs_val >> 8) & 0x7FF) * 1000000;
		volt = (avs_val & 0xFF) * 6250 + avs_margin[i];

		if (freq != avs_freq[i]) {
			dev_err(kbdev->dev, "%s invalid sign-off freq %lu (expected %lu)\n",
				avs_cell, freq, avs_freq[i]);
			return -EINVAL;
		}

		if (volt < cfg->vgpu_min_microvolt || volt > cfg->vgpu_max_microvolt) {
			dev_err(kbdev->dev, "%s volt %lu over boundary\n", avs_cell, volt);
			return -EINVAL;
		}

		if (i != 0 && volt > avs_table[i-1].volt) {
			dev_err(kbdev->dev, "%s low freq high volt\n", avs_cell);
			return -EINVAL;
		}

		avs_table[i].idx = -1;
		avs_table[i].volt = volt;
		avs_table[i].freq = freq;
	}

	return 0;
}

/* do linear interpolate for volt of inner opp */
static int avs_interpolate(struct kbase_device *kbdev,
			   struct gpu_opp_info *avs_table, int avs_count,
			   struct gpu_opp_info *opp_table, int opp_count)
{
	int i, j;
	unsigned long volt;

	/* update avs/opp table */
	for (i = 0; i < avs_count; i++) {
		for (j = 0; j < opp_count; j++) {
			if (avs_table[i].freq == opp_table[j].freq) {
				/* update avs opp index */
				avs_table[i].idx = j;

				/* update volt of sign-off opp via avs volt */
				opp_table[j].volt = avs_table[i].volt;

				break;
			}
		}

		if (avs_table[i].idx == -1) {
			dev_err(kbdev->dev, "avs opp %lu not exist in opp framework\n",
				avs_table[i].freq);
			return -EINVAL;
		}
	}

	/* do interpolation in temp opp table */
	for (i = 1; i < avs_count; i++) {
		/* interpolate from small freq opp */
		for (j = avs_table[i].idx-1; j > avs_table[i-1].idx; j--) {
			volt = VOLT_INTERPOLATE(
				avs_table[i].freq, avs_table[i].volt,
				avs_table[i-1].freq, avs_table[i-1].volt,
				opp_table[j].freq);
			volt = VOLT_NORMALIZATION(volt);

			/* update volt of non sign-off opp via interpolation */
			opp_table[j].volt = volt;
		}
	}

	return 0;
}

static int avs_adjust(struct kbase_device *kbdev)
{
	int i, count, err;
	unsigned long freq, volt;
	struct dev_pm_opp *opp;
	struct gpu_opp_info *avs_table = NULL;
	struct gpu_opp_info *opp_table = NULL;

	/* get opp count */
	count = dev_pm_opp_get_opp_count(kbdev->dev);
	if (count <= 0) {
		err = -EINVAL;
		goto skip_avs_bin;
	}

	/* temp opp table for interpolation (freq high to low) */
	opp_table = kmalloc_array(count, sizeof(opp_table[0]), GFP_KERNEL);
	if (!opp_table) {
		err = -ENOMEM;
		goto skip_avs_bin;
	}

	/* initialize temp opp table from opp framework */
	for (i = 0, freq = ULONG_MAX; i < count; i++, freq--) {
		opp = dev_pm_opp_find_freq_floor(kbdev->dev, &freq);
		if (IS_ERR(opp)) {
			err = PTR_ERR(opp);
			goto skip_avs_bin;
		}

		volt = dev_pm_opp_get_voltage(opp);
		dev_pm_opp_put(opp);

		opp_table[i].idx = i;
		opp_table[i].freq = freq;
		opp_table[i].volt = volt;
	}

	/* avs table for sign-off opp (freq high to low) */
	avs_table = kmalloc_array(MT8189_AVS_CNT, sizeof(avs_table[0]), GFP_KERNEL);
	if (!avs_table) {
		err = -ENOMEM;
		goto skip_avs_bin;
	}

	/* parse avs table from NVMEM framework */
	err = avs_parse_efuse(kbdev, avs_table);
	if (err)
		goto skip_avs_bin;

	/* do linear interpolate */
	err = avs_interpolate(kbdev, avs_table, MT8189_AVS_CNT, opp_table, count);
	if (err)
		goto skip_avs_bin;

	/* update opp framework with avs interpolated result
	 * adjust volt from low opp to high opp, to avoid low freq high volt if adjust fail
	 */
	for (i = count - 1; i >= 0; i--) {
		err = dev_pm_opp_adjust_voltage(kbdev->dev, opp_table[i].freq,
			opp_table[i].volt, opp_table[i].volt, opp_table[i].volt);
		if (WARN_ON(err)) {
			dev_err(kbdev->dev, "Failed to set avs voltage %lu - %lu\n",
				opp_table[i].freq, opp_table[i].volt);

			goto skip_avs_bin;
		}
	}

skip_avs_bin:
	kfree(avs_table);
	kfree(opp_table);
	return err;
}

static int platform_init(struct kbase_device *kbdev)
{
	struct mtk_platform_context *ctx = &mt8189_platform_context;
	int err;

	kbdev->platform_context = ctx;

	err = mtk_platform_init(kbdev);
	if (err)
		return err;

	err = avs_adjust(kbdev);
	if (err)
		dev_info(kbdev->dev, "ignore avs, using default OPP table\n");

#if IS_ENABLED(CONFIG_MALI_DEVFREQ)
	kbdev->devfreq_ops.set_frequency = mtk_set_frequency;
	kbdev->devfreq_ops.voltage_range_check = mtk_voltage_range_check_v2;
#endif

	return 0;
}

struct kbase_platform_funcs_conf mt8189_platform_funcs = {
	.platform_init_func = platform_init,
	.platform_term_func = platform_term
};
