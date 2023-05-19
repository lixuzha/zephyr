/*
 * Copyright (c) 2023 Intel Corporation.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/sensing/sensing.h>
#include <zephyr/sensing/sensing_sensor.h>
#include <stdlib.h>
#include "sensor_mgmt.h"

#include <zephyr/logging/log.h>
LOG_MODULE_DECLARE(sensing, CONFIG_SENSING_LOG_LEVEL);

static struct sensing_dt_info *sensing_sensor_info_to_dt_info(
				const struct sensing_sensor_info *sensor_info)
{
	struct sensing_dt_info *dt_info;
	int index = 0;
	int number;

	STRUCT_SECTION_COUNT(sensing_sensor_info, &number);

	STRUCT_SECTION_FOREACH(sensing_sensor_info, tmp_sensor) {
		if (sensor_info != tmp_sensor) {
			index++;
			continue;
		} else {
			break;
		}
	}
	if (index >= number)
		return NULL;

	STRUCT_SECTION_GET(sensing_dt_info, index, &dt_info);

	return dt_info;
}

/* sensing_open_sensor is normally called by applications: hid, chre, zephyr main, etc */
int sensing_open_sensor(const struct sensing_sensor_info *sensor_info,
			const struct sensing_callback_list *cb_list,
			sensing_sensor_handle_t *handle)
{
	int ret = 0;
	struct sensing_sensor *sensor;
	struct sensing_dt_info *dt_info = sensing_sensor_info_to_dt_info(sensor_info);

	__ASSERT(handle, "invalid handle");

	__ASSERT(dt_info, "invalid sensing_dt_info");

	sensor = get_sensor_by_dev(dt_info->dev);

	ret = open_sensor(sensor, handle);
	if (ret) {
		return -EINVAL;
	}

	if (*handle == NULL) {
		return -ENODEV;
	}

	return sensing_register_callback(*handle, cb_list);
}

int sensing_open_sensor_by_dt(const struct device *dev,
			      const struct sensing_callback_list *cb_list,
			      sensing_sensor_handle_t *handle)
{
	int ret = 0;
	struct sensing_sensor *sensor = get_sensor_by_dev(dev);

	__ASSERT(sensor, "sensor get from dev:%p is NULL", dev);

	__ASSERT(handle, "invalid handle");

	ret = open_sensor(sensor, handle);
	if (ret) {
		return -EINVAL;
	}

	if (*handle == NULL) {
		return -ENODEV;
	}

	return sensing_register_callback(*handle, cb_list);
}

/* sensing_close_sensor is normally called by applications: hid, chre, zephyr main, etc */
int sensing_close_sensor(sensing_sensor_handle_t handle)
{
	return close_sensor(handle);
}

int sensing_set_config(sensing_sensor_handle_t handle,
		       struct sensing_sensor_config *configs,
		       int count)
{
	struct sensing_sensor_config *cfg;
	int i, ret = 0;

	__ASSERT(count > 0 && count < SENSING_SENSOR_ATTRIBUTE_MAX,
		"invalid config count:%d", count);

	for (i = 0; i < count; i++) {
		cfg = &configs[i];
		switch (cfg->attri) {
		case SENSING_SENSOR_ATTRIBUTE_INTERVAL:
			ret |= set_interval(handle, cfg->interval);
			break;

		case SENSING_SENSOR_ATTRIBUTE_SENSITIVITY:
			ret |= set_sensitivity(handle, cfg->data_field, cfg->sensitivity);
			break;

		case SENSING_SENSOR_ATTRIBUTE_LATENCY:
			break;

		default:
			ret = -EINVAL;
			LOG_ERR("invalid config attribute:%d\n", cfg->attri);
			break;
		}
	}

	return ret;
}

int sensing_get_config(sensing_sensor_handle_t handle,
		       struct sensing_sensor_config *configs,
		       int count)
{
	struct sensing_sensor_config *cfg;
	int i, ret = 0;

	__ASSERT(count > 0 && count < SENSING_SENSOR_ATTRIBUTE_MAX,
		"invalid config count:%d", count);

	for (i = 0; i < count; i++) {
		cfg = &configs[i];
		switch (cfg->attri) {
		case SENSING_SENSOR_ATTRIBUTE_INTERVAL:
			ret |= get_interval(handle, &cfg->interval);
			break;

		case SENSING_SENSOR_ATTRIBUTE_SENSITIVITY:
			ret |= get_sensitivity(handle, cfg->data_field, &cfg->sensitivity);
			break;

		case SENSING_SENSOR_ATTRIBUTE_LATENCY:
			break;

		default:
			ret = -EINVAL;
			LOG_ERR("invalid config attribute:%d\n", cfg->attri);
			break;
		}
	}

	return ret;
}

const struct sensing_sensor_info *sensing_get_sensor_info(sensing_sensor_handle_t handle)
{
	return get_sensor_info(handle);
}
