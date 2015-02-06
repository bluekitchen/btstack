// libbluetoothdrv.so wrapper around libbluetooth_mtk.so
#pragma once

#include <stdint.h>

// @returns fd for Bluetooth UART if successfull
int mtk_bt_enable(void);

// disables Bluetooth
void mtk_bt_disable(int fd);

//
int mtk_bt_write(int fd, uint8_t * buffer, size_t len);

// @returns number of bytes read
int mtk_bt_read(int fd, uint8_t * buffer, size_t len);