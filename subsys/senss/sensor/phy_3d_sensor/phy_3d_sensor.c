/*
 * Copyright (c) 2023 Intel Corporation.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#define DT_DRV_COMPAT zephyr_senss_phy_3d_sensor

#include <stdlib.h>
#include <zephyr/drivers/sensor.h>
#include <zephyr/logging/log.h>
#include <zephyr/sys_clock.h>
#include <zephyr/senss/senss.h>
#include <zephyr/senss/senss_sensor.h>
#include <zephyr/sys/util.h>

#include "phy_3d_sensor.h"

LOG_MODULE_REGISTER(phy_3d_sensor, CONFIG_SENSS_LOG_LEVEL);

static int phy_3d_sensor_init(const struct device *dev,
		const struct senss_sensor_info *info,
		const int *reporter_handles,
		int reporters_count)
{
	return 0;
}

static int phy_3d_sensor_deinit(const struct device *dev)
{
	return 0;
}

static int phy_3d_sensor_read_sample(const struct device *dev,
		void *buf, int size)
{
	return 0;
}

static int phy_3d_sensor_sensitivity_test(const struct device *dev,
		int index, uint32_t sensitivity,
		void *last_sample_buf, int last_sample_size,
		void *current_sample_buf, int current_sample_size)
{
	return 0;
}

static int phy_3d_sensor_set_interval(const struct device *dev, uint32_t value)
{
	return 0;
}

static int phy_3d_sensor_get_interval(const struct device *dev,
		uint32_t *value)
{
	return 0;
}

static int phy_3d_sensor_set_sensitivity(const struct device *dev,
		int index, uint32_t value)
{
	return 0;
}

static int phy_3d_sensor_get_sensitivity(const struct device *dev,
		int index, uint32_t *value)
{
	return 0;
}

static const struct senss_sensor_api phy_3d_sensor_api = {
	.init = phy_3d_sensor_init,
	.deinit = phy_3d_sensor_deinit,
	.set_interval = phy_3d_sensor_set_interval,
	.get_interval = phy_3d_sensor_get_interval,
	.set_sensitivity = phy_3d_sensor_set_sensitivity,
	.get_sensitivity = phy_3d_sensor_get_sensitivity,
	.read_sample = phy_3d_sensor_read_sample,
	.sensitivity_test = phy_3d_sensor_sensitivity_test,
};

static const struct senss_sensor_register_info phy_3d_sensor_reg = {
	.flags = SENSS_SENSOR_FLAG_REPORT_ON_CHANGE,
	.sample_size = sizeof(struct senss_sensor_value_3d_int32),
	.sensitivity_count = PHY_3D_SENSOR_CHANNEL_NUM,
	.version.value = SENSS_SENSOR_VERSION(0, 8, 0, 0),
};

#define SENSS_PHY_3D_SENSOR_DT_DEFINE(_inst)				\
	static struct phy_3d_sensor_context _CONCAT(ctx, _inst) = {	\
		.hw_dev = DEVICE_DT_GET(				\
				DT_PHANDLE(DT_DRV_INST(_inst),		\
				underlying_device)),			\
		.sensor_type = DT_PROP(DT_DRV_INST(_inst), sensor_type),\
	};								\
	SENSS_SENSOR_DT_DEFINE(DT_DRV_INST(_inst),			\
		&phy_3d_sensor_reg, &_CONCAT(ctx, _inst),		\
		&phy_3d_sensor_api);

DT_INST_FOREACH_STATUS_OKAY(SENSS_PHY_3D_SENSOR_DT_DEFINE);
