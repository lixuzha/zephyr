/*
 * Copyright (c) 2022-2023 Intel Corporation.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef ZEPHYR_INCLUDE_SENSS_H_
#define ZEPHYR_INCLUDE_SENSS_H_

/**
 * @defgroup senss Sensor Subsystem APIs
 * @defgroup senss_api API
 * @ingroup senss
 * @defgroup senss_sensor_types  Sensor Types
 * @ingroup senss
 * @defgroup senss_datatypes Data Types
 * @ingroup senss
 */

#include <zephyr/senss/senss_datatypes.h>
#include <zephyr/senss/senss_sensor_types.h>

/**
 * @brief Sensor Subsystem API
 * @addtogroup senss_api
 * @{
 */

#ifdef __cplusplus
extern "C" {
#endif


/**
 * @struct senss_sensor_version
 * @brief Sensor Version
 */
struct senss_sensor_version {
	union {
		uint32_t value;
		struct {
			uint8_t major;
			uint8_t minor;
			uint8_t hotfix;
			uint8_t build;
		};
	};
};

#define SENSS_SENSOR_INVALID_HANDLE (-1)
#define SENSS_INDEX_ALL (-1)

#define SENSS_SENSOR_VERSION(_major, _minor, _hotfix, _build)           \
	((_major) << 24 | (_minor) << 16 | (_hotfix) << 8 | (_build))


/**
 * @brief Sensor flag indicating if this sensor is on event reporting data.
 *
 * Reporting sensor data when the sensor event occurs, such as a motion detect sensor reporting
 * a motion or motionless detected event.
 */
#define SENSS_SENSOR_FLAG_REPORT_ON_EVENT			BIT(0)

/**
 * @brief Sensor flag indicating if this sensor is on change reporting data.
 *
 * Reporting sensor data when the sensor data changes.
 *
 * Exclusive with  \ref SENSS_SENSOR_FLAG_REPORT_ON_EVENT
 */
#define SENSS_SENSOR_FLAG_REPORT_ON_CHANGE			BIT(1)


/**
 * @brief Sensor subsytem sensor state.
 *
 */
enum senss_sensor_state {
	SENSS_SENSOR_STATE_NOT_READY = 1,
	SENSS_SENSOR_STATE_READY = 2,
};

/**
 * @brief Sensor subsystem sensor config attribute
 *
 */
enum senss_sensor_attribute {
	SENSS_SENSOR_ATTRIBUTE_INTERVAL = 0,
	SENSS_SENSOR_ATTRIBUTE_SENSITIVITY = 1,
	SENSS_SENSOR_ATTRIBUTE_LATENCY = 2,
	SENSS_SENSOR_ATTRIBUTE_MAX,
};


/**
 * @brief Sensor data event receive callback.
 *
 * @param handle The sensor instance handle.
 *
 * @param buf The data buffer with sensor data.
 *
 * @param size The buffer size in bytes.
 */
typedef void (*senss_data_event_t)(
		int handle,
		void *buf, int size);

/**
 * @brief Sensor bias after calibartion event receive callback.
 *
 * @param handle The sensor instance handle.
 *
 * @param buf The data buffer with bias data.
 */
typedef void (*senss_bias_event_t)(
		int handle,
		void *buf);

/**
 * @brief Sensor batch data flush complete event receive callback.
 *
 * @param handle The sensor instance handle.

 */
typedef void (*senss_flush_complete_t)(
		int handle);


/**
 * @struct senss_sensor_info
 * @brief Sensor basic constant information
 *
 */
struct senss_sensor_info {
	/** Name of the sensor instance */
	const char *name;

	/** Friendly name of the sensor instance */
	const char *friendly_name;

	/** Vendor name of the sensor instance */
	const char *vendor;

	/** Model name of the sensor instance */
	const char *model;

	/** Sensor type */
	int32_t type;

	/** Sensor index in the sensor info array returned from senss_get_sensors() */
	int32_t sensor_index;

	/** Sensor flags */
	uint32_t flags;

	/** Minimal report interval in micro seconds */
	uint32_t minimal_interval;

	/** Sensor version */
	struct senss_sensor_version version;
};

/**
 * @struct senss_callback_list
 * @brief Sensor subsystem event callback list
 *
 */
struct senss_callback_list {
	senss_data_event_t on_data_event;
	senss_bias_event_t on_bios_event;
	senss_flush_complete_t on_flush_complete;
};

/**
 * @struct senss_sensor_config
 * @brief Sensor subsystem sensor configure, including interval, sensitivity, latency
 *
 */
struct senss_sensor_config {
	enum senss_sensor_attribute attri;
	int8_t data_field;
	union {
		uint32_t interval;
		uint32_t sensitivity;
		uint64_t latency;
	};
};


 /**
  * @brief Get all supported sensor instances' information.
  *
  * This API just returns read only information of sensor instances,
  * no side effect to sensor instances.
  *
  * @param num_sensors Get number of sensor instances.
  *
  * @param info For receiving sensor instances' information array pointer.
  *
  * @return 0 on success or negative error value on failure.
  */
int senss_get_sensors(int *num_sensors, const struct senss_sensor_info **info);

/**
 * @brief Open sensor instance by index.
 *
 * Application clients use it to open a sensor instance and get its handle.
 * Support multiple Application clients for open same sensor instance,
 * in this case, the returned handle will different for different clients.
 * meanwhile, also register senss callback list.
 *
 * @param index The sensor index in sensor info array which get from /ref int senss_get_sensors().
 *
 * @param cb_list callback list to be registered to senss.
 *
 * @param *handle The opened instance handle, if failed will be set to -1.
 *
 * @return 0 on success or negative error value on failure.
 */
int senss_open_sensor(
		int index, const struct senss_callback_list *cb_list,
		int *handle);

/**
 * @brief Open sensor instance by sensor type and sensor index.
 *
 * Application clients use it to open a sensor instance and get its handle.
 * Support multiple Application clients for open same sensor instance,
 * in this case, the returned handle will different for different clients.
 * meanwhile, also register senss callback list
 *
 * @param type The sensor type which need to open.
 *
 * @param instance The id for multiple instances with same sensor /ref type, 0 means the default
 *                 instance for the /ref type.
 *
 * @param cb_list callback list to be registered to senss.
 *
 * @param *handle The opened instance handle, if failed will be set to -1.
 *
 * @return 0 on success or negative error value on failure.
 */
int senss_open_sensor_by_type(
		int type, int instance, const struct senss_callback_list *cb_list,
		int *handle);

/**
 * @brief Close sensor instance.
 *
 * @param handle The sensor instance handle need to close.
 *
 * @return 0 on success or negative error value on failure.
 */
int senss_close_sensor(
		int handle);

/**
 * @brief Set current config items to Sensor subsystem.
 *
 * @param handle The sensor instance handle.
 *
 * @param configs The configs to be set according to config attribute.
 *
 * @param count count of configs.
 *
 * @return 0 on success or negative error value on failure, not support etc.
 */
int senss_set_config(
		int handle,
		struct senss_sensor_config *configs, int count);

/**
 * @brief Get current config items from Sensor subsystem.
 *
 * @param handle The sensor instance handle.
 *
 * @param configs The configs to be get according to config attribute.
 *
 * @param count count of configs.
 *
 * @return 0 on success or negative error value on failure, not support etc.
 */
int senss_get_config(
		int handle,
		struct senss_sensor_config *configs, int count);

/**
 * @brief Read a data sample.
 *
 * This API will trigger target sensor read a sample each time.
 *
 * @param handle The sensor instance handle.
 *
 * @param buf The data buffer to receive sensor data sample.
 *
 * @param size The buffer size in bytes.
 *
 * @return 0 on success or negative error value on failure.
 */
int senss_read_sample(
		int handle,
		void *buf, int size);

/**
 * @brief Flush batching buffer for a sensor instance.
 *
 * @param handle The sensor instance handle.
 *
 * If \p handle is -1, will flush all buffer.
 *
 * @return 0 on success or negative error value on failure.
 */
int senss_batching_flush(
		int handle);

/**
 * @brief Get sensor information from sensor instance handle.
 *
 * @param handle The sensor instance hande.
 *
 * @return a const pointer to \ref senss_sensor_info on success or NULL on failure.
 */
const struct senss_sensor_info *senss_get_sensor_info(
		int handle);

/**
 * @brief Get sensor instance's state.
 *
 * @param handle The sensor instance hande.
 *
 * @param state Returned sensor state value.
 *
 * @return 0 on success or negative error value on failure.
 */
int senss_get_sensor_state(
		int handle,
		enum senss_sensor_state *state);

#ifdef __cplusplus
}
#endif

/**
 * @}
 */


#endif /*ZEPHYR_INCLUDE_SENSS_H_*/
