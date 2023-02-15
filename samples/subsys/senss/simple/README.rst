.. _senss-sample:

Sensor subsystem sample
#######################

Overview
********

A simple sample that shows how to use the sensors with senss APIs. It defines
four sensors, with the underlying device bmi160 emulator, and get the sensor
data in defined interval.

The program runs in the following sequence:

#. Define the sensor in the dts

#. Open the sensor

#. Register call back.

#. Set sample interval

#. Run forever and get the sensor data.

Building and Running
********************

This application can be built and executed on native_posix as follows:

.. zephyr-app-commands::
   :zephyr-app: samples/subsys/senss/simple
   :host-os: unix
   :board: native_posix
   :goals: run
   :compact:

To build for another board, change "native_posix" above to that board's name.
At the current stage, it only support native posix
