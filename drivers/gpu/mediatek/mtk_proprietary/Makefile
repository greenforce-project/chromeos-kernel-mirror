# SPDX-License-Identifier: GPL-2.0-only
# drm/tegra depends on host1x, so if both drivers are built-in care must be
# taken to initialize them in the correct order. Link order is the only way
# to ensure this currently.

export DEVICE_MODULES_PATH := $(srctree)
export MTK_TOP := $(DEVICE_MODULES_PATH)/drivers/gpu/mediatek/mtk_proprietary

obj-y += hal/
obj-y += gpueb/
obj-y += gpufreq/
obj-y += gpu_bm/
obj-y += ged/
obj-y += gpu_pdma/
obj-$(CONFIG_MTK_GPU_QOS_FRAMEWORK) += qos/
