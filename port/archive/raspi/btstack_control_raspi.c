#include "btstack_control.h"
#include "btstack_debug.h"

#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/ioctl.h>

// constansts for firmware interface
#define RPI_FIRMWARE_DEV "/dev/vcio"
#define IOCTL_RPI_FIRMWARE_PROPERTY _IOWR(100, 0, char*)

// firmware request messages
#define RPI_FIRMWARE_STATUS_REQUEST (0)
#define RPI_FIRMWARE_PROPERTY_END   (0)
#define RPI_FIRMWARE_SET_GPIO_STATE (0x00038041)

// results
#define RPI_FIRMWARE_STATUS_SUCCESS (0x80000000)
#define RPI_FIRMWARE_STATUS_ERROR   (0x80000001)

static uint8_t bt_reg_en;

void btstack_control_raspi_set_bt_reg_en_pin(uint8_t bt_reg_en_pin){
	bt_reg_en = bt_reg_en_pin;
}

// fd for firmware interface
static int fd = -1;

static void raspi_gpio_set(int gpio, int state){
	uint32_t m[8];
	m[0] = sizeof(m); // total len in bytes
	m[1] = RPI_FIRMWARE_STATUS_REQUEST;
	m[2] = RPI_FIRMWARE_SET_GPIO_STATE;
	m[3] = 8;		  // request size in bytes
	m[4] = 0; 		  // request response size
	m[5] = gpio;
	m[6] = state ? 1 : 0;
	m[7] = RPI_FIRMWARE_PROPERTY_END;;

	int ioctl_result = ioctl(fd, IOCTL_RPI_FIRMWARE_PROPERTY, m);
	if (ioctl_result == -1) {
		log_error("ioctl: IOCTL_RPI_FIRMWARE_PROPERTY: %s", strerror(errno));
		return;
	}

	uint32_t result = m[1];
	if (result != RPI_FIRMWARE_STATUS_SUCCESS) {
		log_error("ioctl: firmware result 0x%08x\n", result);
	}
}

static void raspi_init (const void *config) {
	UNUSED(config);

	fd = open(RPI_FIRMWARE_DEV, O_NONBLOCK);
	if (fd == -1) {
		log_error("cannot open: %s: %s", RPI_FIRMWARE_DEV, strerror(errno));
		return;
	}
}

static int raspi_on (){
    log_info("raspi_on");
    raspi_gpio_set( bt_reg_en, 1 );
    return 0;
}

static int raspi_off(void){
    log_info("raspi_off");
    raspi_gpio_set( bt_reg_en, 0 );
    return 0;
}

static btstack_control_t control = {
    raspi_init,
    raspi_on,
    raspi_off,
    NULL,
    NULL,
    NULL
};

btstack_control_t *btstack_control_raspi_get_instance(){
    return &control; 
}
