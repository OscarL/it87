//
// Copyright 2003-2006, 2022-2023, Oscar Lesta. All right reserved.
// Distributed under the terms of the MIT License.
//
// A simple device driver for the ITE IT8705 (and compatibles) sensor chips.
//

#include <Drivers.h>
#include <Errors.h>
#include <ISA.h>
#include <KernelExport.h>	// for spin(bigtime_t µsecs)

#include <stdio.h>
#include <string.h>

#include "it87.h"

//-----------------------------------------------------------------------------

#define TRACE_IT87
#ifdef TRACE_IT87
#	define TRACE(x...) dprintf("it87: " x)
#else
#	define TRACE(x...)
#endif
#define INFO(x...)	dprintf("it87: " x)
#define ERROR(x...)	dprintf("it87: " x)

#define IT87_ADDRESS_REG	(gBaseAddress + IT87_ADDR_PORT_OFFSET)
#define IT87_DATA_REG		(gBaseAddress + IT87_DATA_PORT_OFFSET)

//-----------------------------------------------------------------------------
// Globals

int32 api_version = B_CUR_DRIVER_API_VERSION;

static isa_module_info* gISA;

static uint16 gChipID = 0;
static uint16 gBaseAddress = 0;	// default ISA base address 0x290

//-----------------------------------------------------------------------------
//	#pragma mark - Hardware I/O

static uint8
read_indexed(uint16 port, uint8 reg)
{
	gISA->write_io_8(port, reg);
	return gISA->read_io_8(port + 1);
}


static void
write_indexed(uint16 port, uint8 reg, uint8 value)
{
	gISA->write_io_8(port, reg);
	gISA->write_io_8(port + 1, value);
}


static void
enter_mb_pnp_mode(void)
{
	// Write 0x87, 0x01, 0x55, 0x55 to register 0x2E to enter MB PnP Mode.
	gISA->write_io_8(0x2E, 0x87);
	gISA->write_io_8(0x2E, 0x01);
	gISA->write_io_8(0x2E, 0x55);
	gISA->write_io_8(0x2E, 0x55);
}


static void
exit_mb_pnp_mode(void)
{
	//---- Set bit 1 to 1 in register at index 0x2 to leave MB PnP Mode.
	// Return to the "Wait for Key" state after we're done with the config.
	write_indexed(0x2E, 0x02, read_indexed(0x2E, 0x02) | (1 << 1));
}


static bool
is_it87xx_present(void)
{
	uint16 chip_id = 0x0000;
	bool chip_found = false;

	enter_mb_pnp_mode();

	chip_id = (read_indexed(0x2E, 0x20) << 8) | read_indexed(0x2E, 0x21);
	switch (chip_id) {
		case 0x8705:
		case 0x8712:
		case 0x8718:
		case 0x8720:
		case 0x8721:
		case 0x8726:
		case 0x8728:
		case 0x8772:
			chip_found = true;	// an ITE IT87xx was found.
			gChipID = chip_id;	// Save the Chip ID for future use.
	}

	exit_mb_pnp_mode();

	return chip_found;
}


static uint16
find_isa_port_address(void)
{
	uint16 port = 0;

	enter_mb_pnp_mode();

	// Select the proper logical device: LDN 0x4 = Enviromental Controller (EC).
	write_indexed(0x2E, 0x07, 0x4);

	// Make sure the EC is active.
	write_indexed(0x2E, 0x30, 0x1);

	// Now fetch the base address port.
	port = read_indexed(0x2E, 0x60) << 8;
	port |= read_indexed(0x2E, 0x61);

	exit_mb_pnp_mode();
	return port;
}


int set_bit(int n, int k)
{
	return (n | (1 << k));
}


int clear_bit(int n, int k)
{
	return (n & (~(1 << k)));
}


int toggle_bit(int n, int k)
{
	return (n ^ (1 << k));
}


static inline void
it87_config(bool enable)
{
	uint8 value = read_indexed(gBaseAddress, IT87_REG_CONFIG);
	if (enable) {
		value |= (1 << 6); // Update VBAT
		value |= (1 << 0); // Start Monitoring Operations
	} else {
		value &= ~(1 << 6); // Don't update VBAT
		value &= ~(1 << 0); // Stop Monitoring Operations
	}
	write_indexed(gBaseAddress, IT87_REG_CONFIG, value);
}


//-----------------------------------------------------------------------------
//	#pragma mark - Misc

static inline uint8
ITESensorRead(int regNum)
{
	gISA->write_io_8(IT87_ADDRESS_REG, regNum);
	return gISA->read_io_8(IT87_DATA_REG);
}


static inline void
ITESensorWrite(int regNum, uint8 value)
{
	gISA->write_io_8(IT87_ADDRESS_REG, regNum);
	gISA->write_io_8(IT87_DATA_REG, value);
}


static inline uint8
ITESensorReadValue(int regNum)
{
	while (gISA->read_io_8(IT87_ADDRESS_REG) & IT87_BUSY) {
		spin(IT87_WAIT);
	}
	return ITESensorRead(regNum);
}


//-----------------------------------------------------------------------------
//	#pragma mark - utils funcs

static inline int
CountToRPM(uint8 count)
{
	if (count == 255)
		return 0;
	if (count < 2)
		count = 152;
	return 1350000 / (count * 2);
}


static void
OutInt(void* buff, size_t* len, const char format[], int value)
{
	sprintf((char*) buff + *len, format, value);
	*len = strlen((char*) buff);
}


static void
myprintf(void* buff, size_t* len, const char format[], uint8 val1, uint8 val2)
{
	sprintf((char*) buff + *len, format, val1, val2);
	*len = strlen((char*) buff);
}


//-----------------------------------------------------------------------------
//	#pragma mark - Device Hooks

static status_t
device_open(const char name[], uint32 flags, void** cookie)
{
	*cookie = NULL;
	return B_OK;
}


static status_t
device_close(void* cookie)
{
	return B_OK;
}


static status_t
device_free(void* cookie)
{
	return B_OK;
}


// Text Interface.
static status_t
device_read(void* cookie, off_t position, void* data, size_t* num_bytes)
{
	if (*num_bytes < 1)
		return B_IO_ERROR;

	if (position) {
		*num_bytes = 0;
		return B_OK;
	}

	int v;

	enter_mb_pnp_mode();
	it87_config(true);

	v = ITESensorReadValue(IT87_REG_VIN0) * 16;
	myprintf(data, num_bytes, "VIN0 : %3d.%03d\n", (v/1000), (v%1000));

	v = ITESensorReadValue(IT87_REG_VIN1) * 16;
	myprintf(data, num_bytes, "VIN1 : %3d.%03d\n", (v/1000), (v%1000));

	v = ITESensorReadValue(IT87_REG_VIN2) * 16;
	myprintf(data, num_bytes, "VIN2 : %3d.%03d\n", (v/1000), (v%1000));

	v = ITESensorReadValue(IT87_REG_VIN3) * 16;
	myprintf(data, num_bytes, "VIN3 : %3d.%03d\n", (v/1000), (v%1000));

	v = ITESensorReadValue(IT87_REG_VIN4) * 16 * 4; // +12 V
	myprintf(data, num_bytes, "VIN4 : %3d.%03d\n", (v/1000), (v%1000));

	v = ITESensorReadValue(IT87_REG_VIN5) * 16; // -12 V
	myprintf(data, num_bytes, "VIN5 : %2d.%03d\n", (v/1000), (v%1000));

	v = ITESensorReadValue(IT87_REG_VIN6) * 16 * 3; // SB 3V
	myprintf(data, num_bytes, "VIN6 : %3d.%03d\n", (v/1000), (v%1000));

	v = ITESensorReadValue(IT87_REG_VIN7) * 16; // SB 5V
	myprintf(data, num_bytes, "VIN7 : %3d.%03d\n", (v/1000), (v%1000));

	v = ITESensorReadValue(IT87_REG_VBAT) * 16;
	myprintf(data, num_bytes, "VBAT : %3d.%03d\n", (v/1000), (v%1000));

	OutInt(data, num_bytes, "TEMP0: %3d\n", ITESensorReadValue(IT87_REG_TEMP0));
	OutInt(data, num_bytes, "TEMP1: %3d\n", ITESensorReadValue(IT87_REG_TEMP1));
	OutInt(data, num_bytes, "TEMP2: %3d\n", ITESensorReadValue(IT87_REG_TEMP2));

	OutInt(data, num_bytes, "FAN1 : %4d\n", CountToRPM(ITESensorReadValue(IT87_REG_FAN_1)));
	OutInt(data, num_bytes, "FAN2 : %4d\n", CountToRPM(ITESensorReadValue(IT87_REG_FAN_2)));
	OutInt(data, num_bytes, "FAN3 : %4d\n", CountToRPM(ITESensorReadValue(IT87_REG_FAN_3)));

	it87_config(false);
	exit_mb_pnp_mode();
 
	return B_OK;
}


static status_t
device_write(void* cookie, off_t pos, const void* data, size_t* num_bytes)
{
	*num_bytes = 0;
	return B_NOT_ALLOWED;
}


static status_t
device_control(void* cookie, uint32 operation, void* args, size_t length)
{
	switch (operation)
	{
/*
		case HW_SENSOR_READ:
		{
			status_t result;
			hw_sensor_io_args* ioctl = (hw_sensor_io_args*) args;
			if (ioctl->signature != IT87_SENSOR_SIGNATURE)
				return B_BAD_VALUE;

			result = B_OK;
			switch (ioctl->size) {
				case 1:	ioctl->value = gISA->read_io_8(ioctl->port);		break;
				case 2:	ioctl->value = gISA->read_io_16(ioctl->port);	break;
				case 4:	ioctl->value = gISA->read_io_32(ioctl->port);	break;
				default:
					result = B_BAD_VALUE;
			}

			return result;
		}

		case HW_SENSOR_WRITE:
		{

			status_t result;
			hw_sensor_io_args* ioctl = (hw_sensor_io_args*) arg;
			if (ioctl->signature != IT87_SENSOR_SIGNATURE)
				return B_BAD_VALUE;

			result = B_OK;
			switch (ioctl->size) {
				case 1:	gISA->write_io_8(ioctl->port, ioctl->value);		break;
				case 2:	gISA->write_io_16(ioctl->port, ioctl->value);	break;
				case 4:	gISA->write_io_32(ioctl->port, ioctl->value);	break;
				default:
					result = B_BAD_VALUE;
			}

			return result;
		}

		case HW_SENSOR_GET_INFO:
		{

			hw_sensor_info_args* ioctl = (hw_sensor_info_args*) arg;
			if (ioctl->signature != IT87_SENSOR_SIGNATURE)
				return B_BAD_VALUE;

			ioctl->status = pci->get_nth_pci_info(ioctl->index, ioctl->info);

			return B_OK;
		}
*/
	}

	return B_BAD_VALUE;	// B_DEV_INVALID_IOCTL?
}


//-----------------------------------------------------------------------------
//	#pragma mark - Driver Hooks

status_t
init_hardware(void)
{
	if (get_module(B_ISA_MODULE_NAME, (module_info**) &gISA) < 0)
		return ENOSYS;

	if (!is_it87xx_present()) {
		TRACE("device not found.");
		return B_DEVICE_NOT_FOUND;	// ENODEV
	}

	put_module(B_ISA_MODULE_NAME);
	return B_OK;
}


status_t
init_driver(void)
{
	if (get_module(B_ISA_MODULE_NAME, (module_info**) &gISA) < 0)
		return ENOSYS;

	// Find out the proper ISA port address to talk to the EC.
	gBaseAddress = find_isa_port_address();

	if (gBaseAddress == 0)
		return ENOSYS;

	uint8 vendor_id = ITESensorReadValue(IT87_REG_ITE_VENDOR_ID);
	uint8 core_id = ITESensorReadValue(IT87_REG_CORE_ID);

	INFO("ITE%4x found at address = 0x%04x.", gChipID, gBaseAddress);
	INFO("\tVENDOR_ID: 0x%2x - CORE_ID: 0x%2x", vendor_id, core_id);

	// Put the Fan Divisor into a known state. Only affects FAN_TAC1 and FAN_TAC2
//	ITESensorWrite(IT87_REG_FAN_DIV, IT87_FANDIV);

	return B_OK;
}


void
uninit_driver(void)
{
	put_module(B_ISA_MODULE_NAME);
}


const char**
publish_devices()
{
	static const char* names[] = {
		"sensor/" IT87_SENSOR_DEVICE_NAME,
		NULL
	};
	return names;
}


device_hooks*
find_device(const char name[])
{
	static device_hooks hooks = {
		device_open,	// -> open entry point
		device_close,	// -> close entry point
		device_free,	// -> free cookie
		device_control,	// -> control entry point
		device_read,	// -> read entry point
		device_write,	// -> write entry point
		NULL,			// -> readv
		NULL,			// -> writev
		NULL,			// -> select
		NULL,			// -> deselect
	//	NULL,			// -> wakeup
	//	NULL			// -> suspend
	};

	return &hooks;
}
