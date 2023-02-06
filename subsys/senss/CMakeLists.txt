# SPDX-License-Identifier: Apache-2.0

zephyr_library()
zephyr_library_include_directories(include)

zephyr_library_sources(
	sensor_mgmt.c
	senss.c
	senss_sensor.c
)
