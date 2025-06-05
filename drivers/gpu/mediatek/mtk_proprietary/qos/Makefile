#
# Copyright (C) 2025 MediaTek Inc.
#
# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License version 2 as
# published by the Free Software Foundation.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
# GNU General Public License for more details.
#

ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/firmware/arm_scmi

#ccflags-y += -DDEBUG

mtk_qos-objs := mtk_qos_common.o mtk_qos_scmi.o qos-mt8196.o

ifeq ($(CONFIG_MTK_GPU_QOS_FRAMEWORK), y)
obj-$(CONFIG_MTK_GPU_SUPPORT) += mtk_qos.o
endif
