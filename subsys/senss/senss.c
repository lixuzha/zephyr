/*
 * Copyright (c) 2023 Intel Corporation.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/senss/senss.h>
#include <zephyr/senss/senss_sensor.h>

#include <zephyr/logging/log.h>
LOG_MODULE_DECLARE(senss, CONFIG_SENSS_LOG_LEVEL);

/* senss_open_sensor is normally called by application: hid, chre, zephyr main, etc */
int senss_open_sensor(int type,
		      int index,
		      const struct senss_callback_list *cb_list,
		      int *handle)
{
	return -ENOTSUP;
}

int senss_open_sensor_by_type(int type,
			      int instance,
			      const struct senss_callback_list *cb_list,
			      int *handle)
{
	return -ENOTSUP;
}

/* senss_close_sensor is normally called by applcation: hid, chre, zephyr main, etc */
int senss_close_sensor(int handle)
{
	return -ENOTSUP;
}

int senss_read_sample(int handle, void *buf, int size)
{
	return -ENOTSUP;
}

int senss_set_config(int handle, struct senss_sensor_config *configs, int count)
{
	return -ENOTSUP;
}

int senss_get_config(int handle, struct senss_sensor_config *configs, int count)
{
	return -ENOTSUP;
}

const struct senss_sensor_info *senss_get_sensor_info(int handle)
{
	return NULL;
}

int senss_get_sensor_state(int handle, enum senss_sensor_state *state)
{
	return -ENOTSUP;
}
