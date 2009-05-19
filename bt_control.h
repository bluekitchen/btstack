/*
 *  bt_control.h
 *
 *  BT Control API -- allows BT Daemon to initialize and control differnt hardware
 *
 *  Created by Matthias Ringwald on 5/19/09.
 *
 */

typedef struct {
    int          (*on)(void *config);     // <-- turn BT module on and configure
    int          (*off)(void *config);    // <-- turn BT module off
    int          (*valid)(void *confif);  // <-- test if hardware can be supported
    const char * (*names)(void *config);  // <-- return hardware name
} bt_control_t;
