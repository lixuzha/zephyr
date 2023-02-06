/*
 * Copyright (c) 2023 Intel Corporation.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef SENSOR_MGMT_H_
#define SENSOR_MGMT_H_

#include <zephyr/senss/senss_datatypes.h>
#include <zephyr/senss/senss_sensor.h>

#ifdef __cplusplus
extern "C" {
#endif

struct senss_mgmt_context {
	bool senss_initialized;
	int sensor_num;
	struct senss_sensor_info *info;
};

struct senss_mgmt_context *get_senss_ctx(void);

/**
 * @}
 */

#ifdef __cplusplus
}
#endif


#endif /* SENSOR_MGMT_H_ */
