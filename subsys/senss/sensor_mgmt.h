/*
 * Copyright (c) 2023 Intel Corporation.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef SENSOR_MGMT_H_
#define SENSOR_MGMT_H_

#include <zephyr/kernel.h>
#include <zephyr/senss/senss_datatypes.h>
#include <zephyr/senss/senss_sensor.h>
#include <zephyr/sys/slist.h>


#ifdef __cplusplus
extern "C" {
#endif

#define PHANDLE_DEVICE_BY_IDX(idx, node, prop)				\
	DEVICE_DT_GET(DT_PHANDLE_BY_IDX(node, prop, idx))

#define PHANDLE_DEVICE_LIST(node, prop)					\
{									\
	LISTIFY(DT_PROP_LEN_OR(node, prop, 0),				\
			PHANDLE_DEVICE_BY_IDX,				\
			(,),						\
			node,						\
			prop)						\
}

#define SENSS_SENSOR_INFO(node)						\
{									\
	.dev = DEVICE_DT_GET(node),					\
	.ord = DT_DEP_ORD(node),					\
	.info.type = DT_PROP(node, sensor_type),			\
	.info.sensor_index = DT_PROP_OR(node, sensor_index, 0),		\
	.info.name = DT_NODE_FULL_NAME(node),				\
	.info.friendly_name = DT_PROP(node, friendly_name),		\
	.info.minimal_interval = DT_PROP(node, minimal_interval),	\
	.reporter_num = DT_PROP_LEN_OR(node, reporters, 0),		\
	.reporters = PHANDLE_DEVICE_LIST(node, reporters),		\
},

#define for_each_sensor(ctx, i, sensor)					\
	for (i = 0; i < ctx->sensor_num && (sensor = ctx->sensor_db[i]) != NULL; i++)

enum sensor_trigger_mode {
	SENSOR_TRIGGER_MODE_POLLING = 1,
	SENSOR_TRIGGER_MODE_DATA_READY = 2,
};

/**
 * @struct senss_sensor_dt_info
 * @brief Sensor device tree data structure
 */
struct senss_sensor_dt_info {
	const struct device *dev;
	const int ord;
	struct senss_sensor_info info;
	uint16_t reporter_num;
	const struct device *reporters[CONFIG_SENSS_MAX_REPORTER_COUNT];
};

/**
 * @struct senss_connection information
 * @brief senss_connection indicates connection from reporter sensor(source) to client sensor(sink)
 */
struct senss_connection {
	int index;
	struct senss_sensor *source;
	struct senss_sensor *sink;
	/* interval and sensitivity set from client(sink) to reporter(source) */
	uint32_t interval;
	int sensitivity[CONFIG_SENSS_MAX_SENSITIVITY_COUNT];
	/* copy sensor data to connection data buf from reporter */
	void *data;
	/* client(sink) next consume time */
	sys_snode_t snode;
	/* post data to application */
	senss_data_event_t data_evt_cb;
};

/**
 * @struct senss_sensor
 * @brief Internal sensor instance data structure.
 *
 * Each sensor instance will have its unique data structure for storing all
 * it's related information.
 *
 * Sensor management will enumerate all these instance data structures,
 * build report relationship model base on them, etc.
 */
struct senss_sensor {
	/* device binding to sensor device tree */
	const struct device *dev;
	/* Common Register info */
	struct senss_sensor_dt_info dt;
	int conns_num;
	struct senss_connection *conns;
	sys_slist_t client_list;
	uint32_t interval;
	uint8_t sensitivity_count;
	int sensitivity[CONFIG_SENSS_MAX_SENSITIVITY_COUNT];
	enum senss_sensor_state state;
	enum sensor_trigger_mode mode;
	/* runtime info */
	uint16_t sample_size;
	void *data_buf;
};

struct senss_mgmt_context {
	bool senss_initialized;
	int sensor_num;
	int fixed_connection_count;
	struct senss_sensor_info *info;
	struct senss_sensor *sensor_db[CONFIG_SENSS_MAX_SENSOR_COUNT];
	struct senss_connection *conns[CONFIG_SENSS_MAX_CONNECTION_COUNT];
	struct k_mutex conn_mutex;
};

struct senss_mgmt_context *get_senss_ctx(void);
int open_sensor(int type, int instance);
int close_sensor(struct senss_connection *conn);
int senss_register_callback(struct senss_connection *conn,
			    const struct senss_callback_list *cb_list);
int set_interval(struct senss_connection *conn, uint32_t interval);
int get_interval(struct senss_connection *con, uint32_t *sensitivity);
int set_sensitivity(struct senss_connection *conn, int8_t index, uint32_t interval);
int get_sensitivity(struct senss_connection *con, int8_t index, uint32_t *sensitivity);

static inline bool is_phy_sensor(struct senss_sensor *sensor)
{
	return sensor->dt.reporter_num == 0;
}

static inline uint16_t get_sensor_sample_size_from_dt(const struct senss_sensor_dt_info *dt, int i)
{
	__ASSERT(dt, "device tree info is invalid");
	__ASSERT(i < dt->reporter_num, "dt index should less than reporter num");

	return ((struct senss_sensor_ctx *)dt->reporters[i]->data)->register_info->sample_size;
}

static inline struct senss_sensor *get_sensor_by_dev(const struct device *dev)
{
	return dev ? (struct senss_sensor *)((struct senss_sensor_ctx *)dev->data)->priv_ptr : NULL;
}

static inline struct senss_sensor *get_reporter_sensor(struct senss_mgmt_context *ctx,
						       struct senss_sensor *sensor,
						       int index)
{
	if (!sensor || index >= sensor->conns_num) {
		return NULL;
	}

	return get_sensor_by_dev(sensor->dt.reporters[index]);
}

static inline struct senss_connection *get_connection_by_handle(struct senss_mgmt_context *ctx,
							  int handle)
{
	if (handle >= CONFIG_SENSS_MAX_CONNECTION_COUNT) {
		return NULL;
	}

	return ctx->conns[handle];
}

static inline struct senss_sensor *get_sensor_by_index(struct senss_mgmt_context *ctx, int index)
{
	if (index > CONFIG_SENSS_MAX_SENSOR_COUNT) {
		return NULL;
	}

	return ctx->sensor_db[index];
}

static inline struct senss_sensor *get_sensor_by_type_and_instance(struct senss_mgmt_context *ctx,
								   int type,
								   int sensor_index)
{
	struct senss_sensor *sensor;
	int i = 0;

	for_each_sensor(ctx, i, sensor) {
		if (sensor->dt.info.type != type ||
		    sensor->dt.info.sensor_index != sensor_index) {
			continue;
		}
		return sensor;
	}

	return NULL;
}

static inline const struct senss_sensor_info *get_sensor_info(struct senss_sensor *sensor)
{
	__ASSERT(sensor, "get sensor info, senss_sensor is NULL");

	return &sensor->dt.info;
}

/**
 * @}
 */

#ifdef __cplusplus
}
#endif


#endif /* SENSOR_MGMT_H_ */
