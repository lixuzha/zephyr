/*
 * Copyright (c) 2023 Intel Corporation.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/senss/senss.h>
#include <zephyr/senss/senss_sensor.h>
#include <stdlib.h>
#include "sensor_mgmt.h"

#include <zephyr/logging/log.h>
LOG_MODULE_DECLARE(senss, CONFIG_SENSS_LOG_LEVEL);


/* senss_open_sensor and senss_open_sensor_by_type are normally called by application:
 * hid, chre, zephyr main, etc
 */
int senss_open_sensor(int index,
		      const struct senss_callback_list *cb_list,
		      int *handle)
{
	struct senss_connection *conn;
	struct senss_mgmt_context *ctx = get_senss_ctx();
	struct senss_sensor *sensor = get_sensor_by_index(ctx, index);
	if (!sensor) {
		return -EINVAL;
	}

	/* set connection index directly to handle */
	*handle = open_sensor(sensor->dt.info.type, sensor->dt.info.sensor_index);
	if (*handle < 0) {
		return -EINVAL;
	}

	conn = get_connection_by_handle(ctx, *handle);

	__ASSERT(conn, "handle:%d get connection error", *handle);

	__ASSERT(!conn->sink, "only connection to application could register data event callback");

	LOG_INF("senss_open_sensor ready: %s, state:0x%x, conn:%d",
			conn->source->dev->name, conn->source->state, conn->index);

	return senss_register_callback(conn, cb_list);
}

int senss_open_sensor_by_type(int type,
			      int instance,
			      const struct senss_callback_list *cb_list,
			      int *handle)
{
	struct senss_connection *conn;
	struct senss_mgmt_context *ctx = get_senss_ctx();

	if (!handle) {
		return -ENODEV;
	}
	/* set connection index directly to handle */
	*handle = open_sensor(type, instance);

	if (*handle < 0) {
		return -EINVAL;
	}

	conn = get_connection_by_handle(ctx, *handle);

	__ASSERT(conn, "handle:%d get connection error", *handle);

	__ASSERT(!conn->sink, "only connection to application could register data event callback");

	LOG_INF("senss_open_sensor_by_type ready: %s, state:0x%x, conn:%d",
			conn->source->dev->name, conn->source->state, conn->index);

	return senss_register_callback(conn, cb_list);
}

/* senss_close_sensor is normally called by applcation: hid, chre, zephyr main, etc */
int senss_close_sensor(int handle)
{
	struct senss_mgmt_context *ctx = get_senss_ctx();
	struct senss_connection *conn = get_connection_by_handle(ctx, handle);
	struct senss_sensor *reporter;
	int ret;

	__ASSERT(conn, "handle:%d get connection error", handle);

	__ASSERT(!conn->sink, "only sensor that connects to application could be closed");

	reporter = conn->source;

	ret = close_sensor(conn);
	if (ret) {
		LOG_ERR("close_sensor:%d error, ret:%d", handle, ret);
		return ret;
	}

	LOG_INF("sensor:%s closed successfully", reporter->dev->name);

	return 0;
}

int senss_read_sample(int handle, void *buf, int size)
{
	return -ENOTSUP;
}

int senss_set_config(int handle, struct senss_sensor_config *configs, int count)
{
	struct senss_mgmt_context *ctx = get_senss_ctx();
	struct senss_connection *conn = get_connection_by_handle(ctx, handle);
	struct senss_sensor_config *cfg;
	int i, ret = 0;

	__ASSERT(conn, "handle:%d get connection error", handle);

	__ASSERT(count > 0 && count < SENSS_SENSOR_ATTRIBUTE_MAX, "invalid config count:%d", count);

	for (i = 0; i < count; i++) {
		cfg = &configs[i];
		switch (cfg->attri) {
		case SENSS_SENSOR_ATTRIBUTE_INTERVAL:
			ret |= set_interval(conn, cfg->interval);
			break;
		case SENSS_SENSOR_ATTRIBUTE_SENSITIVITY:
			ret |= set_sensitivity(conn, cfg->data_field, cfg->sensitivity);
			break;
		case SENSS_SENSOR_ATTRIBUTE_LATENCY:
			break;
		default:
			ret = -EINVAL;
			LOG_ERR("invalid config attribute:%d\n", cfg->attri);
			break;
		}
	}

	return ret;
}

int senss_get_config(int handle, struct senss_sensor_config *configs, int count)
{
	struct senss_mgmt_context *ctx = get_senss_ctx();
	struct senss_connection *conn = get_connection_by_handle(ctx, handle);
	struct senss_sensor_config *cfg;
	int i, ret = 0;

	__ASSERT(conn, "handle:%d get connection error", handle);

	__ASSERT(count > 0 && count < SENSS_SENSOR_ATTRIBUTE_MAX, "invalid config count:%d", count);

	for (i = 0; i < count; i++) {
		cfg = &configs[i];
		switch (cfg->attri) {
		case SENSS_SENSOR_ATTRIBUTE_INTERVAL:
			ret |= get_interval(conn, &cfg->interval);
			break;
		case SENSS_SENSOR_ATTRIBUTE_SENSITIVITY:
			ret |= get_sensitivity(conn, cfg->data_field, &cfg->sensitivity);
			break;
		case SENSS_SENSOR_ATTRIBUTE_LATENCY:
			break;
		default:
			ret = -EINVAL;
			LOG_ERR("invalid config attribute:%d\n", cfg->attri);
			break;
		}
	}

	return ret;
}

const struct senss_sensor_info *senss_get_sensor_info(int handle)
{
	struct senss_mgmt_context *ctx = get_senss_ctx();
	struct senss_connection *conn = get_connection_by_handle(ctx, handle);

	if (!conn) {
		LOG_ERR("senss_get_sensor_info, handle:%d get connection error", handle);
		return NULL;
	}

	return get_sensor_info(conn->source);
}

int senss_get_sensor_state(int handle, enum senss_sensor_state *state)
{
	return -ENOTSUP;
}
