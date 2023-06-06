/*
 * Copyright (c) 2023 Intel Corporation.
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include <zephyr/sys/__assert.h>
#include <zephyr/devicetree.h>
#include <zephyr/device.h>
#include <zephyr/kernel.h>
#include <zephyr/sensing/sensing.h>
#include <zephyr/sensing/sensing_sensor.h>
#include <zephyr/logging/log.h>
#include <stdlib.h>
#include "sensor_mgmt.h"

#define DT_DRV_COMPAT zephyr_sensing

#define SENSING_SENSOR_NUM (sizeof((int []){ DT_FOREACH_CHILD_STATUS_OKAY_SEP(		\
			    DT_DRV_INST(0), DT_NODE_EXISTS, (,))}) / sizeof(int))

LOG_MODULE_REGISTER(sensing, CONFIG_SENSING_LOG_LEVEL);

DT_FOREACH_CHILD_STATUS_OKAY(DT_DRV_INST(0), SENSING_SENSOR_INFO_DEFINE)
DT_FOREACH_CHILD_STATUS_OKAY(DT_DRV_INST(0), SENSING_SENSOR_DEFINE)


/**
 * @struct sensing_context
 * @brief sensing subsystem context to include global variables
 */
struct sensing_context {
	bool sensing_initialized;
	int sensor_num;
	struct sensing_sensor *sensors[SENSING_SENSOR_NUM];
};

static struct sensing_context sensing_ctx = {
	.sensor_num = SENSING_SENSOR_NUM,
};


static int set_sensor_state(struct sensing_sensor *sensor, enum sensing_sensor_state state)
{
	__ASSERT(sensor, "set sensor state, sensing_sensor is NULL");

	sensor->state = state;

	return 0;
}

static void init_connection(struct sensing_connection *conn,
			    struct sensing_sensor *source,
			    struct sensing_sensor *sink)
{
	__ASSERT(conn, "init each connection, invalid connection");
	conn->source = source;
	conn->sink = sink;
	conn->interval = 0;
	memset(conn->sensitivity, 0x00, sizeof(conn->sensitivity));
	/* link connection to its reporter's client_list */
	sys_slist_append(&source->client_list, &conn->snode);
}

static int init_sensor(struct sensing_sensor *sensor, int conns_num)
{
	const struct sensing_sensor_api *sensor_api;
	struct sensing_sensor *reporter;
	struct sensing_connection *conn;
	void *tmp_conns[conns_num];
	int i;

	__ASSERT(sensor && sensor->dev, "init sensor, sensor or sensor device is NULL");
	sensor_api = sensor->dev->api;
	__ASSERT(sensor_api, "init sensor, sensor device sensor_api is NULL");

	/* physical sensor has no reporters, conns_num is 0 */
	if (conns_num == 0) {
		sensor->conns = NULL;
	}

	for (i = 0; i < conns_num; i++) {
		conn = &sensor->conns[i];
		reporter = get_reporter_sensor(sensor, i);
		__ASSERT(reporter, "sensor's reporter should not be NULL");

		init_connection(conn, reporter, sensor);

		LOG_DBG("init sensor, reporter:%s, client:%s, connection:%d",
			reporter->dev->name, sensor->dev->name, i);

		tmp_conns[i] = conn;
	}

	/* physical sensor is working at polling mode by default,
	 * virtual sensor working mode is inherited from its reporter
	 */
	if (is_phy_sensor(sensor)) {
		sensor->mode = SENSOR_TRIGGER_MODE_POLLING;
	}

	return sensor_api->init(sensor->dev, sensor->info, tmp_conns, conns_num);
}

/* create struct sensing_sensor *sensor according to sensor device tree */
static int pre_init_sensor(struct sensing_sensor *sensor)
{
	struct sensing_sensor_ctx *sensor_ctx;
	uint16_t sample_size, total_size;
	uint16_t conn_sample_size = 0;
	uint16_t i = 0;
	void *tmp_data;

	sensor_ctx = sensor->dev->data;
	__ASSERT(sensor_ctx, "sensing sensor context is invalid");

	sample_size = sensor_ctx->register_info->sample_size;
	for (i = 0; i < sensor->reporter_num; i++) {
		conn_sample_size += get_reporter_sample_size(sensor, i);
	}

	/* total memory to be allocated for a sensor according to sensor device tree:
	 * 1) sample data point to struct sensing_sensor->data_buf
	 * 2) size of struct sensing_connection* for sensor connection to its reporter
	 * 3) reporter sample size to be stored in connection data
	 */
	total_size = sample_size + sensor->reporter_num * sizeof(*sensor->conns) +
		conn_sample_size;

	/* total size for different sensor maybe different, for example:
	 * there's no reporter for physical sensor, so no connection memory is needed
	 * reporter num of each virtual sensor may also different, so connection memory is also
	 * varied, so here malloc is a must for different sensor.
	 */
	tmp_data = malloc(total_size);
	if (!tmp_data) {
		LOG_ERR("malloc memory for sensing_sensor error");
		return -ENOMEM;
	}
	sensor->sample_size = sample_size;
	sensor->data_buf = tmp_data;
	sensor->conns = (struct sensing_connection *)((uint8_t *)sensor->data_buf + sample_size);

	tmp_data = sensor->conns + sensor->reporter_num;
	for (i = 0; i < sensor->reporter_num; i++) {
		sensor->conns[i].data = tmp_data;
		tmp_data = (uint8_t *)tmp_data + get_reporter_sample_size(sensor, i);
	}

	__ASSERT(tmp_data == ((uint8_t *)sensor->data_buf + total_size), "sensor memory assign error");

	LOG_INF("create sensor, sensor:%s, min_ri:%d(us)",
			sensor->dev->name, sensor->info->minimal_interval);

	sensor->interval = 0;
	sensor->sensitivity_count = sensor_ctx->register_info->sensitivity_count;
	__ASSERT(sensor->sensitivity_count <= CONFIG_SENSING_MAX_SENSITIVITY_COUNT,
			"sensitivity count:%d should not exceed MAX_SENSITIVITY_COUNT",
			sensor->sensitivity_count);
	memset(sensor->sensitivity, 0x00, sizeof(sensor->sensitivity));

	sensor->state = SENSING_SENSOR_STATE_OFFLINE;
	sys_slist_init(&sensor->client_list);

	sensor_ctx->priv_ptr = sensor;

	return 0;
}

static int sensing_init(void)
{
	struct sensing_context *ctx = &sensing_ctx;
	struct sensing_sensor *sensor;
	enum sensing_sensor_state state;
	int ret = 0;
	int i = 0;

	LOG_INF("sensing init begin...");

	if (ctx->sensing_initialized) {
		LOG_INF("sensing is already initialized");
		return 0;
	}

	if (ctx->sensor_num == 0) {
		LOG_WRN("no sensor created by device tree yet");
		return 0;
	}

	STRUCT_SECTION_FOREACH(sensing_sensor, snr) {
		ret = pre_init_sensor(snr);
		if (ret) {
			LOG_ERR("sensing init, create sensor error");
			return -EINVAL;
		}
		ctx->sensors[i++] = snr;
	}

	for_each_sensor(ctx, i, sensor) {
		ret = init_sensor(sensor, sensor->reporter_num);
		if (ret) {
			LOG_ERR("sensor:%s initial error", sensor->dev->name);
		}
		state = (ret ? SENSING_SENSOR_STATE_OFFLINE : SENSING_SENSOR_STATE_READY);
		ret = set_sensor_state(sensor, state);
		if (ret) {
			LOG_ERR("set sensor:%s state:%d error", sensor->dev->name, state);
		}
		LOG_INF("sensing init, sensor:%s ret:%d", sensor->dev->name, ret);
	}

	return ret;
}

int open_sensor(struct sensing_sensor *sensor, sensing_sensor_handle_t *handle)
{
	struct sensing_connection *conn;

	*handle = NULL;
	/* allocate struct sensing_connection *conn and conn data for application client */
	conn = malloc(sizeof(*conn) + sensor->sample_size);
	if (!conn) {
		LOG_ERR("malloc memory for struct sensing_connection error");
		return -ENOMEM;
	}
	conn->data = (uint8_t *)conn + sizeof(*conn);

	/* create connection from sensor to application(client = NULL) */
	init_connection(conn, sensor, NULL);

	*handle = conn;

	return 0;
}

int close_sensor(struct sensing_connection *conn)
{
	__ASSERT(conn && conn->source, "connection or reporter not be NULL");

	__ASSERT(!conn->sink, "sensor derived from device tree cannot be closed");

	sys_slist_find_and_remove(&conn->source->client_list, &conn->snode);

	free(conn);

	return 0;
}

int sensing_register_callback(struct sensing_connection *conn,
			      const struct sensing_callback_list *cb_list)
{
	__ASSERT(conn, "register sensing callback list, connection not be NULL");

	__ASSERT(!conn->sink, "only connection to application could register sensing callback");

	__ASSERT(cb_list, "callback should not be NULL");

	conn->data_evt_cb = cb_list->on_data_event;

	return 0;
}

int set_interval(struct sensing_connection *conn, uint32_t interval)
{
	return -ENOTSUP;
}

int get_interval(struct sensing_connection *conn, uint32_t *interval)
{
	return -ENOTSUP;
}

int set_sensitivity(struct sensing_connection *conn, int8_t index, uint32_t sensitivity)
{
	return -ENOTSUP;
}

int get_sensitivity(struct sensing_connection *conn, int8_t index, uint32_t *sensitivity)
{
	return -ENOTSUP;
}

int sensing_get_sensors(int *sensor_nums, const struct sensing_sensor_info **info)
{
	*sensor_nums = SENSING_SENSOR_NUM;

	STRUCT_SECTION_GET(sensing_sensor_info, 0, info);

	return 0;
}


SYS_INIT(sensing_init, APPLICATION, CONFIG_APPLICATION_INIT_PRIORITY);
