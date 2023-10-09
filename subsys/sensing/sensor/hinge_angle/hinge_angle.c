/*
 * Copyright (c) 2023 Intel Corporation.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdlib.h>
#include <zephyr/drivers/sensor.h>
#include <zephyr/logging/log.h>
#include <zephyr/sys/util.h>
#include <zephyr/sensing/sensing_sensor.h>

LOG_MODULE_REGISTER(hinge_angle, CONFIG_SENSING_LOG_LEVEL);

#define HINGE_ANGLE_ACC_INTERVAL_US          100000

static struct sensing_sensor_register_info hinge_reg = {
	.flags = SENSING_SENSOR_FLAG_REPORT_ON_CHANGE,
	.sample_size = sizeof(struct sensing_sensor_value_q31),
	.sensitivity_count = 1,
	.version.value = SENSING_SENSOR_VERSION(1, 0, 0, 0),
};

struct hinge_angle_context {
	uint32_t interval;
	uint32_t sensitivity;
	sensing_sensor_handle_t base_acc_handle;
	sensing_sensor_handle_t lid_acc_handle;
	void *algo_handle;
};

static int hinge_init(const struct device *dev)
{
	LOG_INF("[%s] name: %s", __func__, dev->name);

	return 0;
}

static int hinge_set_interval(const struct device *dev, uint32_t value)
{
	int ret;

	struct sensing_sensor_config base_acc_config;
	struct sensing_sensor_config lid_acc_config;
	struct hinge_angle_context *ctx = sensing_sensor_get_ctx_data(dev);
	uint32_t acc_interval = value ? HINGE_ANGLE_ACC_INTERVAL_US : 0;

	base_acc_config.attri = SENSING_SENSOR_ATTRIBUTE_INTERVAL;
	base_acc_config.interval = HINGE_ANGLE_ACC_INTERVAL_US;
	ret = sensing_set_config(ctx->base_acc_handle, &base_acc_config, 1);
	if (ret) {
		LOG_ERR("base_acc sensing_set_interval error:%d\n", ret);
	}

	lid_acc_config.attri = SENSING_SENSOR_ATTRIBUTE_INTERVAL;
	lid_acc_config.interval = HINGE_ANGLE_ACC_INTERVAL_US;
	ret = sensing_set_config(ctx->lid_acc_handle, &lid_acc_config, 1);
	if (ret) {
		LOG_ERR("lid_acc sensing_set_interval error:%d\n", ret);
	}

	ctx->interval = value;
	LOG_INF("[%s] name: %s, value %d acc_interval %d", __func__, dev->name,
		value, acc_interval);


	return 0;
}

static int hinge_attr_set(const struct device *dev,
		enum sensor_channel chan,
		enum sensor_attribute attr,
		const struct sensor_value *val)
{
	const struct phy_3d_sensor_config *cfg = dev->config;
	struct phy_3d_sensor_data *data = dev->data;
	int ret = 0;

	switch (attr) {
	case SENSOR_ATTR_HYSTERESIS:
		ret = phy_3d_sensor_attr_set_hyst(dev, chan, val);
		break;

	default:
		chan = data->custom->chan_all;
		ret = sensor_attr_set(cfg->hw_dev, chan, attr, val);
		break;
	}

	LOG_INF("%s:%s attr:%d ret:%d", __func__, dev->name, attr, ret);
	return ret;
}

static int hinge_submit(const struct device *dev,
		struct rtio_iodev_sqe *sqe)
{
	const struct phy_3d_sensor_config *cfg = dev->config;
	struct phy_3d_sensor_data *data = dev->data;
	struct sensing_sensor_value_3d_q31 *sample;
	struct sensor_value value[PHY_3D_SENSOR_CHANNEL_NUM];
	uint32_t buffer_len;
	int i, ret;

	ret = rtio_sqe_rx_buf(sqe, sizeof(*sample), sizeof(*sample),
			(uint8_t **)&sample, &buffer_len);
	if (ret) {
		rtio_iodev_sqe_err(sqe, ret);
		return ret;
	}

	ret = sensor_sample_fetch_chan(cfg->hw_dev, data->custom->chan_all);
	if (ret) {
		LOG_ERR("%s: sample fetch failed: %d", dev->name, ret);
		rtio_iodev_sqe_err(sqe, ret);
		return ret;
	}

	ret = sensor_channel_get(cfg->hw_dev, data->custom->chan_all, value);
	if (ret) {
		LOG_ERR("%s: channel get failed: %d", dev->name, ret);
		rtio_iodev_sqe_err(sqe, ret);
		return ret;
	}

	for (i = 0; i < ARRAY_SIZE(value); ++i) {
		sample->readings[0].v[i] =
			data->custom->sensor_value_to_q31(&value[i]);
	}

	sample->header.reading_count = 1;
	sample->shift = data->custom->shift;

	LOG_DBG("%s: Sample data:\t x: %d, y: %d, z: %d",
			dev->name,
			sample->readings[0].x,
			sample->readings[0].y,
			sample->readings[0].z);

	rtio_iodev_sqe_ok(sqe, 0);
	return 0;
}

static const struct sensor_driver_api hinge_api = {
	.attr_set = hinge_attr_set,
	.submit = hinge_submit,
};

#define DT_DRV_COMPAT zephyr_sensing_hinge_angle
#define SENSING_HINGE_ANGLE_DT_DEFINE(_inst) \
	static struct hinge_angle_context _CONCAT(hinge_ctx, _inst); 	\
	SENSING_SENSOR_DT_INST_DEFINE(_inst, &hinge_reg,		\
		&hinge_init, NULL,					\
		&_CONCAT(hinge_ctx, _inst), NULL,			\
		POST_KERNEL, CONFIG_SENSOR_INIT_PRIORITY,		\
		&hinge_api);

DT_INST_FOREACH_STATUS_OKAY(SENSING_HINGE_ANGLE_DT_DEFINE);
