/*
 * Copyright 2006, 2023, Haiku, Inc. All Rights Reserved.
 * Distributed under the terms of the MIT License.
 *
 * Authors:
 *		Oscar Lesta
 */

#ifndef _IT87_SENSORS_H_
#define _IT87_SENSORS_H_

#include <Drivers.h>

#ifdef __cplusplus
extern "C" {
#endif


enum {
	IT87_SENSORS_OP_BASE = B_DEVICE_OP_CODES_END + 'it87',
	IT87_SENSORS_READ = IT87_SENSORS_OP_BASE + 1,
};


typedef struct {
	int16	temps[3];		// °Celsius
	int16	fans[5];		// RPMs
	int16	voltages[9];	// mV
} it87_sensors_data;


#ifdef __cplusplus
}
#endif

#endif	// _IT87_SENSORS_H_
