// stub library to allow linking against libbluetoothdrv.so without having a copy of it
#include "bluetoothdrv.h"

// @returns fd for Bluetooth UART if successfull
int mtk_bt_enable(void){
	return 0;	
}

// disables Bluetooth
void mtk_bt_disable(int fd){
}

//
int mtk_bt_write(int fd, uint8_t * buffer, size_t len){
	return 0;
}

// @returns number of bytes read
int mtk_bt_read(int fd, uint8_t * buffer, size_t len){
	return 0;
}