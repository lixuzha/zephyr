/*
 * Copyright (c) 2023 Intel Corporation.
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include <zephyr/sys/__assert.h>
#include <zephyr/devicetree.h>
#include <zephyr/device.h>
#include <zephyr/kernel.h>
#include <zephyr/senss/senss.h>
#include <zephyr/senss/senss_sensor.h>
#include <zephyr/logging/log.h>
#include <stdlib.h>
#include "sensor_mgmt.h"

#define DT_DRV_COMPAT zephyr_senss

LOG_MODULE_REGISTER(senss, CONFIG_SENSS_LOG_LEVEL);

/* Enumerated sensors from device tree */
static const struct senss_sensor_dt_info sensors_dt[] = {
	DT_FOREACH_CHILD_STATUS_OKAY(DT_DRV_INST(0), SENSS_SENSOR_INFO)
};

static struct senss_mgmt_context senss_ctx = {0};

struct senss_mgmt_context *get_senss_ctx(void)
{
	return &senss_ctx;
}

static int cmp_sensor(const void *a, const void *b)
{
	struct senss_sensor *sensor_a = *(struct senss_sensor **)a;
	struct senss_sensor *sensor_b = *(struct senss_sensor **)b;

	return sensor_a->dt.ord - sensor_b->dt.ord;
}

static void sort_sensors(struct senss_mgmt_context *ctx)
{
	qsort(ctx->sensor_db, ctx->sensor_num, sizeof(struct senss_sensor *), cmp_sensor);
}

static int set_sensor_state(struct senss_sensor *sensor, enum senss_sensor_state state)
{
	__ASSERT(sensor, "set sensor state, senss_sensor is NULL");

	sensor->state = state;

	return 0;
}

static int init_each_connection(struct senss_mgmt_context *ctx,
				struct senss_connection *conn,
				struct senss_sensor *source,
				struct senss_sensor *sink)
{
	__ASSERT(conn, "init each connection, invalid connection");
	conn->source = source;
	conn->sink = sink;
	conn->interval = 0;
	memset(conn->sensitivity, 0x00, sizeof(conn->sensitivity));

	return 0;
}

static int init_sensor(struct senss_mgmt_context *ctx, struct senss_sensor *sensor)
{
	const struct senss_sensor_api *sensor_api;
	struct senss_sensor *reporter;
	struct senss_connection *conn;
	int conn_idx[CONFIG_SENSS_MAX_REPORTER_COUNT] = {0};
	int i, ret = 0;

	__ASSERT(sensor && sensor->dev, "init sensor, sensor or sensor device is NULL");
	sensor_api = sensor->dev->api;
	__ASSERT(sensor_api, "init sensor, sensor device sensor_api is NULL");

	sensor->conns_num = sensor->dt.reporter_num;

	/* physical sensor has no reporter, conns_num is 0 */
	if (sensor->conns_num == 0) {
		sensor->conns = NULL;
	}

	__ASSERT(sensor->conns_num <= CONFIG_SENSS_MAX_REPORTER_COUNT,
		"connection number:%d exceed max number:%d",
		 sensor->conns_num, CONFIG_SENSS_MAX_REPORTER_COUNT);

	for (i = 0; i < sensor->conns_num; i++) {
		conn = &sensor->conns[i];
		reporter = get_reporter_sensor(ctx, sensor, i);
		__ASSERT(reporter, "sensor's reporter should not be NULL");
		ret |= init_each_connection(ctx, conn, reporter, sensor);
		if (ret) {
			LOG_ERR("init_sensor, init_each_connection error:%d", ret);
			return ret;
		}

		/* device tree required sensor connection between reporter and client cannot be
		 * opened or closed any more, so it is called fixed connection.
		 */
		conn->index = ctx->fixed_connection_count++;
		LOG_DBG("init_sensor, reporter:%s, client:%s, connection:%d",
			reporter->dev->name, sensor->dev->name, conn->index);

		__ASSERT(conn->index < CONFIG_SENSS_MAX_CONNECTION_COUNT,
			 "sensor connection number:%d exceed %d",
			 conn->index, CONFIG_SENSS_MAX_CONNECTION_COUNT);

		sensor->conns = conn;
		ctx->conns[conn->index] = conn;
		/* link connection to its repoter's client_list */
		sys_slist_append(&reporter->client_list, &conn->snode);

		conn_idx[i] = conn->index;
	}

	/* physical sensor is working at polling mode by default,
	 * virtual sensor working mode is inherited from its reporter
	 */
	if (is_phy_sensor(sensor)) {
		sensor->mode = SENSOR_TRIGGER_MODE_POLLING;
	}

	return sensor_api->init(sensor->dev, &sensor->dt.info, conn_idx, sensor->conns_num);
}

/* create struct senss_sensor *sensor according to sensor device tree */
static struct senss_sensor *create_sensor(const struct senss_sensor_dt_info *dt)
{
	struct senss_sensor_ctx *sensor_ctx;
	struct senss_sensor *sensor;
	uint16_t sample_size, total_size;
	uint16_t conn_sample_size = 0;
	uint16_t i = 0;
	void *tmp_data;

	__ASSERT(dt && dt->dev, "device tree info or senss dev is invalid");
	sensor_ctx = dt->dev->data;
	__ASSERT(sensor_ctx, "senss sensor context is invalid");

	sample_size = sensor_ctx->register_info->sample_size;
	for (i = 0; i < dt->reporter_num; i++) {
		conn_sample_size += get_sensor_sample_size_from_dt(dt, i);
	}

	/* total memory to be alloctead for a sensor according to sensor device tree:
	 * 1) size of struct senss_sensor *
	 * 2) sample data point to struct senss_sensor->data_buf
	 * 3) size of struct senss_connection* for sensor connection to its reporter
	 * 4) reporter sample size to be stored in connection data
	 */
	total_size = sizeof(*sensor) + sample_size +
		     dt->reporter_num * sizeof(*sensor->conns) + conn_sample_size;

	/* total size for different sensor maybe different, for example:
	 * there's no reporeter for physical sensor, so no connection memory is needed
	 * reporter num of each virtual sensor may also different, so connection memory is also
	 * varied, so here malloc is a must for different sensor.
	 */
	sensor = malloc(total_size);
	if (!sensor) {
		LOG_ERR("malloc memory for struct senss_sensor error");
		return NULL;
	}
	sensor->sample_size = sample_size;
	sensor->data_buf = (uint8_t *)sensor + sizeof(*sensor);
	sensor->conns = (struct senss_connection *)((uint8_t *)sensor->data_buf + sample_size);

	tmp_data = sensor->conns + dt->reporter_num;
	for (i = 0; i < dt->reporter_num; i++) {
		sensor->conns[i].data = tmp_data;
		tmp_data = (uint8_t *)tmp_data + get_sensor_sample_size_from_dt(dt, i);
	}

	__ASSERT(tmp_data == ((uint8_t *)sensor + total_size), "sensor memory assign error");

	sensor->dev = dt->dev;
	memcpy(&sensor->dt, dt, sizeof(*dt));
	sensor->dt.info.flags = sensor_ctx->register_info->flags;
	sensor->dt.info.version = sensor_ctx->register_info->version;

#ifdef CONFIG_SENSOR_INFO
	STRUCT_SECTION_FOREACH(sensor_info, tmp) {
		if (!strcmp(sensor->dev->name, tmp->dev->name)) {
			sensor->dt.info.vendor = tmp->vendor;
			sensor->dt.info.model = tmp->model;
			break;
		}
	}
#endif

	LOG_INF("create_sensor, sensor:%s, min_ri:%d(us)",
		sensor->dev->name, sensor->dt.info.minimal_interval);

	sensor->interval = 0;
	sensor->sensitivity_count = sensor_ctx->register_info->sensitivity_count;
	__ASSERT(sensor->sensitivity_count <= CONFIG_SENSS_MAX_SENSITIVITY_COUNT,
		 "sensitivity count:%d should not exceed MAX_SENSITIVITY_COUNT",
		 sensor->sensitivity_count);
	memset(sensor->sensitivity, 0x00, sizeof(sensor->sensitivity));

	sensor->state = SENSS_SENSOR_STATE_NOT_READY;
	sys_slist_init(&sensor->client_list);

	sensor_ctx->priv_ptr = sensor;

	return sensor;
}

static int senss_bind_conn(struct senss_mgmt_context *ctx, struct senss_connection *conn)
{
	int i = 0;

	__ASSERT(conn && conn->source, "senss_bind_conn, connection or reporter not be NULL");
	/* multi applications could bind connection simultaneously, set mutex protect */
	k_mutex_lock(&ctx->conn_mutex, K_FOREVER);
	/* find the first free connection */
	for (i = ctx->fixed_connection_count; i < CONFIG_SENSS_MAX_CONNECTION_COUNT; i++) {
		if (!ctx->conns[i]) {
			break;
		}
	}

	if (i >= CONFIG_SENSS_MAX_CONNECTION_COUNT) {
		LOG_ERR("connection index:%d should less than CONFIG_SENSS_MAX_CONNECTION_COUNT:%d",
			i, CONFIG_SENSS_MAX_CONNECTION_COUNT);
		k_mutex_unlock(&ctx->conn_mutex);
		return -EINVAL;
	}

	conn->index = i;

	ctx->conns[i] = conn;
	/* conn->source is pointer to reporter, add conn to repoter sensor client list */
	sys_slist_append(&conn->source->client_list, &conn->snode);
	k_mutex_unlock(&ctx->conn_mutex);

	return 0;
}

static int senss_unbind_conn(struct senss_mgmt_context *ctx, struct senss_connection *conn)
{
	struct senss_sensor *reporter;

	/* multi applications could unbind connection simultaneously, set mutex protect */
	k_mutex_lock(&ctx->conn_mutex, K_FOREVER);
	__ASSERT(conn && conn->source, "close sensor, connection or reporter not be NULL");
	reporter = conn->source;
	__ASSERT(conn->index < CONFIG_SENSS_MAX_CONNECTION_COUNT,
		"sensor connection number:%d exceed MAX_SENSOR_COUNT", conn->index);

	sys_slist_find_and_remove(&reporter->client_list, &conn->snode);
	LOG_DBG("senss_unbind_conn, %s connection:%d complete", reporter->dev->name, conn->index);

	ctx->conns[conn->index] = NULL;
	k_mutex_unlock(&ctx->conn_mutex);

	return 0;
}


static int senss_init(void)
{
	struct senss_mgmt_context *ctx = get_senss_ctx();
	const struct senss_sensor_dt_info *dt_info = &sensors_dt[0];
	struct senss_sensor *sensor;
	enum senss_sensor_state state;
	int ret = 0;
	int i;

	LOG_INF("senss_init begin...");

	if (ctx->senss_initialized) {
		LOG_INF("senss is already initialized");
		return 0;
	}

	ctx->sensor_num = ARRAY_SIZE(sensors_dt);
	if (ctx->sensor_num == 0) {
		LOG_WRN("no sensor created yet");
		return 0;
	}

	for (i = 0; i < ctx->sensor_num; i++) {
		sensor = create_sensor(dt_info);
		if (!sensor) {
			LOG_ERR("senss_init, create_sensor error");
			return -EINVAL;
		}
		ctx->sensor_db[i] = sensor;
		dt_info++;
	}

	sort_sensors(ctx);

	for_each_sensor(ctx, i, sensor) {
		ret = init_sensor(ctx, sensor);
		if (ret) {
			LOG_ERR("sensor:%s initial error", sensor->dev->name);
		}
		state = (ret ? SENSS_SENSOR_STATE_NOT_READY : SENSS_SENSOR_STATE_READY);
		ret = set_sensor_state(sensor, state);
		if (ret) {
			LOG_ERR("set sensor:%s state:%d error", sensor->dev->name, state);
		}
		LOG_INF("senss_init, sensor:%s ret:%d", sensor->dev->name, ret);
	}

	/* initial events */
	k_mutex_init(&ctx->conn_mutex);

	return ret;
}

int open_sensor(int type, int instance)
{
	struct senss_mgmt_context *ctx = get_senss_ctx();
	struct senss_sensor *reporter;
	struct senss_connection *conn;
	int ret;

	/* get the reporter sensor to be opened */
	reporter = get_sensor_by_type_and_instance(ctx, type, instance);
	if (!reporter) {
		LOG_ERR("no sensor match to type:0x%x, index:%d", type, instance);
		return SENSS_SENSOR_INVALID_HANDLE;
	}
	/* allocate struct senss_connection *conn and conn data for application client */
	conn = malloc(sizeof(*conn) + reporter->sample_size);
	if (!conn) {
		LOG_ERR("malloc memory for struct senss_connection error");
		return SENSS_SENSOR_INVALID_HANDLE;
	}
	conn->data = (uint8_t *)conn + sizeof(*conn);

	/* create connection from reporter to application(client = NULL) */
	ret = init_each_connection(ctx, conn, reporter, NULL);
	if (ret) {
		LOG_ERR("open_sensor, init_each_connection error:%d", ret);
		free(conn);
		return SENSS_SENSOR_INVALID_HANDLE;
	}

	ret = senss_bind_conn(ctx, conn);
	if (ret) {
		LOG_ERR("open_sensor, senss_bind_conn error:%d", ret);
		free(conn);
		return SENSS_SENSOR_INVALID_HANDLE;
	}

	return conn->index;
}

int close_sensor(struct senss_connection *conn)
{
	struct senss_mgmt_context *ctx = get_senss_ctx();
	int ret;

	__ASSERT(!conn->sink, "sensor derived from device tree cannot be closed");

	ret = senss_unbind_conn(ctx, conn);
	if (ret) {
		LOG_ERR("close_sensor, senss_unbind_conn error:%d", ret);
		return SENSS_SENSOR_INVALID_HANDLE;
	}

	free(conn);

	return 0;
}

int senss_register_callback(struct senss_connection *conn,
			    const struct senss_callback_list *cb_list)
{
	__ASSERT(conn, "register senss callback list, connection not be NULL");

	__ASSERT(cb_list, "callback should not be NULL");

	conn->data_evt_cb = cb_list->on_data_event;

	return 0;
}

int set_interval(struct senss_connection *conn, uint32_t interval)
{
	return -ENOTSUP;
}

int get_interval(struct senss_connection *conn, uint32_t *interval)
{
	return -ENOTSUP;
}

int set_sensitivity(struct senss_connection *conn, int8_t index, uint32_t sensitivity)
{
	return -ENOTSUP;
}

int get_sensitivity(struct senss_connection *conn, int8_t index, uint32_t *sensitivity)
{
	return -ENOTSUP;
}

int senss_get_sensors(int *sensor_nums, const struct senss_sensor_info **info)
{
	struct senss_mgmt_context *ctx = (struct senss_mgmt_context *)get_senss_ctx();
	const struct senss_sensor *sensor;
	int i = 0;

	__ASSERT(sensor_nums, "sensor nums pointer is NULL");
	__ASSERT(info, "senss sensor info pointer is NULL");

	*sensor_nums = ctx->sensor_num;
	if (*sensor_nums == 0) {
		LOG_WRN("no valid sensor to get");
		return -EINVAL;
	}

	for_each_sensor(ctx, i, sensor) {
		*info++ = &sensor->dt.info;
	}

	return 0;
}

SYS_INIT(senss_init, APPLICATION, CONFIG_APPLICATION_INIT_PRIORITY);
