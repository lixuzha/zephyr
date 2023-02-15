/*
 * Copyright (c) 2023 Intel Corporation.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/logging/log.h>
#include <zephyr/sys_clock.h>
#include <zephyr/senss/senss.h>

#define MAX_SENSOR_COUNT 128
LOG_MODULE_REGISTER(main, LOG_LEVEL_INF);

static void acc_data_event_callback(int handle, void *buf, int size)
{
	return;
}

static void hinge_angle_data_event_callback(int handle, void *buf, int size)
{
	return;
}

void main(void)
{
	const struct senss_callback_list base_acc_cb_list = {
		.on_data_event = &acc_data_event_callback,
	};
	const struct senss_callback_list lid_acc_cb_list = {
		.on_data_event = &acc_data_event_callback,
	};
	const struct senss_callback_list hinge_angle_cb_list = {
		.on_data_event = &hinge_angle_data_event_callback,
	};
	const struct senss_sensor_info *info[MAX_SENSOR_COUNT];
	const struct senss_sensor_info *tmp_info;
	struct senss_sensor_config base_acc_config;
	struct senss_sensor_config lid_acc_config;
	struct senss_sensor_config hingle_angle_config;
	int base_acc;
	int lid_acc;
	int hinge_angle;
	int ret, i, num = 0;

	ret = senss_get_sensors(&num, info);
	if (ret) {
		LOG_ERR("senss_get_sensors error");
		return;
	}

	for (i = 0; i < num; ++i) {
		LOG_INF("Sensor %d: name: %s friendly_name: %s type: %d index: %d",
			 i,
			 info[i]->name,
			 info[i]->friendly_name,
			 info[i]->type,
			 info[i]->sensor_index);
	}

	LOG_INF("senss run successfully");

	ret = senss_open_sensor_by_type(SENSS_SENSOR_TYPE_MOTION_ACCELEROMETER_3D,
				0,
				&base_acc_cb_list,
				&base_acc);
	if (ret) {
		LOG_ERR("senss_open_sensor_by_type, type:0x%x index:0 error:%d",
			SENSS_SENSOR_TYPE_MOTION_ACCELEROMETER_3D, ret);
	} else {
		base_acc_config.attri = SENSS_SENSOR_ATTRIBUTE_INTERVAL;
		base_acc_config.interval = 100 * USEC_PER_MSEC;
		ret = senss_set_config(base_acc, &base_acc_config, 1);
		if (ret) {
			LOG_ERR("base_acc senss_set_interval error:%d\n", ret);
		}
	}

	ret = senss_open_sensor_by_type(SENSS_SENSOR_TYPE_MOTION_ACCELEROMETER_3D,
				1,
				&lid_acc_cb_list,
				&lid_acc);
	if (ret) {
		LOG_ERR("senss_open_sensor_by_type, type:0x%x index:1 error:%d",
			SENSS_SENSOR_TYPE_MOTION_ACCELEROMETER_3D, ret);
	} else {
		lid_acc_config.attri = SENSS_SENSOR_ATTRIBUTE_INTERVAL;
		lid_acc_config.interval = 100 * USEC_PER_MSEC;
		ret = senss_set_config(base_acc, &lid_acc_config, 1);
		if (ret) {
			LOG_ERR("lid_acc senss_set_interval error:%d\n", ret);
		}
	}

	ret = senss_open_sensor(1,
				&hinge_angle_cb_list,
				&hinge_angle);
	if (ret) {
		LOG_ERR("senss_open_sensor_by_type, type:0x%x index:0 error:%d",
			SENSS_SENSOR_TYPE_MOTION_HINGE_ANGLE, ret);
	} else {
		tmp_info = senss_get_sensor_info(hinge_angle);
		hingle_angle_config.attri = SENSS_SENSOR_ATTRIBUTE_INTERVAL;
		hingle_angle_config.interval = tmp_info->minimal_interval;
		ret = senss_set_config(hinge_angle, &hingle_angle_config, 1);
		if (ret) {
			LOG_ERR("hinge_angle senss_set_interval error:%d\n", ret);
		}
	}

	ret = senss_close_sensor(base_acc);
	if (ret) {
		LOG_ERR("senss_close_sensor:%d error:%d", base_acc, ret);
	}

	ret = senss_close_sensor(lid_acc);
	if (ret) {
		LOG_ERR("senss_close_sensor:%d error:%d", lid_acc, ret);
	}

	ret = senss_close_sensor(hinge_angle);
	if (ret) {
		LOG_ERR("senss_close_sensor:%d error:%d", hinge_angle, ret);
	}
}
