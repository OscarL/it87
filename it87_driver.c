//
// Copyright 2003-2006, 2022-2023, Oscar Lesta <oscar.lesta@gmail.com>. All right reserved.
// Distributed under the terms of the MIT License.
//
// A simple device driver for the ITE IT8705 (and compatibles) sensor chips.
//

#include "it87xx.h"

#include <KernelExport.h>	// for spin(bigtime_t µsecs)
#include <Drivers.h>
#include <Errors.h>
#include <ISA.h>
#include <stdio.h>
#include <string.h>

////////////////////////////////////////////////////////////////////////////////
// Globals

#define HW_SENSORS_DEVICE_PATH	"misc/"

static const char* gDevNames[] = {
	HW_SENSORS_DEVICE_PATH IT87_SENSOR_DEVICE_NAME,
	NULL
};

static isa_module_info* isa;

static int32 gOpenCount = 0;
static uint8 gChipID = 0;
static uint16 gBaseAddress = 0;	// default ISA base address 0x290

#define IT87_ADDRESS_REG (gBaseAddress + IT87_ADDR_PORT_OFFSET)
#define IT87_DATA_REG (gBaseAddress + IT87_DATA_PORT_OFFSET)

////////////////////////////////////////////////////////////////////////////////
//	#pragma mark - Hardware I/O

static uint8
read_indexed(uint16 port, uint8 reg)
{
	isa->write_io_8(port, reg);
	return isa->read_io_8(port + 1);
}


static void
write_indexed(uint16 port, uint8 reg, uint8 value)
{
	isa->write_io_8(port, reg);
	isa->write_io_8(port + 1, value);
}


static inline void
enter_mb_pnp_mode(void)
{
	// Write 0x87, 0x01, 0x55, 0x55 to register 0x2E to enter MB PnP Mode.
	isa->write_io_8(0x2E, 0x87);
	isa->write_io_8(0x2E, 0x01);
	isa->write_io_8(0x2E, 0x55);
	isa->write_io_8(0x2E, 0x55);
}


static inline void
exit_mb_pnp_mode(void)
{
	//---- Set bit 1 to 1 in register at index 0x2 to leave MB PnP Mode.
	// Return to the "Wait for Key" state after we're done with the config.
	write_indexed(0x2E, 0x02, read_indexed(0x2E, 0x02) | (1 << 1));
}


static bool
is_it87xx_present(void)
{
	uint8 chip_id = 0x0;
	bool chip_found = false;

	enter_mb_pnp_mode();

	chip_id = read_indexed(0x2E, 0x20);
	if (chip_id == 0x87) {
		chip_id = read_indexed(0x2E, 0x21);
		if (chip_id == 0x05 || chip_id == 0x12 || chip_id == 0x16 || chip_id == 0x18) {
			chip_found = true;	// an ITE IT87xx was found.
			gChipID = chip_id;	// Save the Chip ID for future use.
		}
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


////////////////////////////////////////////////////////////////////////////////
//	#pragma mark - Misc

static inline uint8
ITESensorRead(int regNum)
{
	isa->write_io_8(IT87_ADDRESS_REG, regNum);
	return isa->read_io_8(IT87_DATA_REG);
}


static inline void
ITESensorWrite(int regNum, uint8 value)
{
	isa->write_io_8(IT87_ADDRESS_REG, regNum);
	isa->write_io_8(IT87_DATA_REG, value);
}


static inline uint8
ITESensorReadValue(int regNum)
{
	while (isa->read_io_8(IT87_ADDRESS_REG) & IT87_BUSY) {
		spin(IT87_WAIT);
	}
    return ITESensorRead(regNum);
}


////////////////////////////////////////////////////////////////////////////////
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
OutInt(void* buff, size_t* len, char format[], int value)
{
	sprintf((char*) buff + *len, format, value);
	*len = strlen((char*) buff);
}

/*
static void
OutFloat(void* buff, size_t* len, char format[], float value)
{
	sprintf((char*) buff + *len, format, value);
	*len = strlen((char*) buff);
}
*/

static void myprintf(void* buff, size_t* len, char format[], uint8 val1, uint8 val2)
{
	sprintf((char*) buff + *len, format, val1, val2);
	*len = strlen((char*) buff);
}


////////////////////////////////////////////////////////////////////////////////
//	#pragma mark - Device Hooks

static status_t
it87_open(const char name[], uint32 flags, void** cookie)
{
	if (strncmp(gDevNames[0], name, B_OS_NAME_LENGTH))
		return B_BAD_VALUE;

	if (atomic_or(&gOpenCount, 1) & 1)	// There can be only one...
		return B_BUSY;

	return B_OK;
}


// Text Interface.
static status_t
it87_read(void* cookie, off_t pos, void* data, size_t* num_bytes)
{
	int v;
	*num_bytes = 0;

	if (pos > 175)	return B_OK;

	enter_mb_pnp_mode();
	it87_config(true);
/*
	OutInt(data, num_bytes, "CHIP_ID: IT87%2x", gChipID);
	OutInt(data, num_bytes, " - VENDOR_ID: 0x%2x", ITESensorReadValue(IT87_REG_ITE_VENDOR_ID));
	OutInt(data, num_bytes, " - CORE_ID: 0x%2x", ITESensorReadValue(IT87_REG_CORE_ID));
	OutInt(data, num_bytes, " - Base Address: 0x%04X\n", gBaseAddress);
*/
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

/*
    OutFloat(data, num_bytes, "VIN1 : %d\n", ITESensorReadValue(IT87_REG_VIN1) * 0.016);
    OutFloat(data, num_bytes, "VBAT : %d\n", ITESensorReadValue(IT87_REG_VBAT) * 0.016);
*/
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
it87_write(void* cookie, off_t pos, const void* data, size_t* num_bytes)
{
    *num_bytes = 0;
    return B_NOT_ALLOWED;
}


static status_t
it87_control(void* cookie, uint32 operation, void* args, size_t length)
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
				case 1:	ioctl->value = isa->read_io_8(ioctl->port);		break;
				case 2:	ioctl->value = isa->read_io_16(ioctl->port);	break;
				case 4:	ioctl->value = isa->read_io_32(ioctl->port);	break;
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
				case 1:	isa->write_io_8(ioctl->port, ioctl->value);		break;
				case 2:	isa->write_io_16(ioctl->port, ioctl->value);	break;
				case 4:	isa->write_io_32(ioctl->port, ioctl->value);	break;
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


static status_t
it87_close(void* cookie)
{
    return B_OK;
}


static status_t
it87_free(void* cookie)
{
    atomic_and(&gOpenCount, ~1);
    return B_OK;
}


////////////////////////////////////////////////////////////////////////////////
//	#pragma mark - Driver Hooks

status_t
init_hardware(void)
{
	if (get_module(B_ISA_MODULE_NAME, (module_info**) &isa) < 0)
		return ENOSYS;

	if (!is_it87xx_present()) {
		return ENODEV;	// Why there's no B_DEVICE_NOT_FOUND ?
	}

	put_module(B_ISA_MODULE_NAME);
	return B_OK;
}


status_t
init_driver(void)
{
	if (get_module(B_ISA_MODULE_NAME, (module_info**) &isa) < 0)
		return ENOSYS;

	// Find out the proper ISA port address to talk to the EC.
	gBaseAddress = find_isa_port_address();

	if (gBaseAddress == 0)
		return ENOSYS;

	// Put the Fan Divisor into a known state. Only affects FAN_TAC1 and FAN_TAC2
//	ITESensorWrite(IT87_REG_FAN_DIV, IT87_FANDIV);

	gOpenCount = 0;
	return B_OK;
}


void
uninit_driver(void)
{
	put_module(B_ISA_MODULE_NAME);
}


static device_hooks it87_hooks = {
	it87_open,		// -> open entry point
	it87_close,		// -> close entry point
	it87_free,		// -> free cookie
	it87_control,	// -> control entry point
	it87_read,		// -> read entry point
	it87_write,		// -> write entry point
	NULL,			// -> readv
	NULL,			// -> writev
	NULL,			// -> select
	NULL,			// -> deselect
//	NULL,			// -> wakeup
//	NULL			// -> suspend
};


const char** publish_devices()
{
	return gDevNames;
}


device_hooks* find_device(const char name[])
{
	if (!strcmp(gDevNames[0], name))
		return &it87_hooks;

	return NULL;	// Device not found
}


int32 api_version = B_CUR_DRIVER_API_VERSION;
