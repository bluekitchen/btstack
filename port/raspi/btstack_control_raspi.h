#include "btstack_control.h"

btstack_control_t *btstack_control_raspi_get_instance();

// set GPIO for power cycle
void btstack_control_raspi_set_bt_reg_en_pin(uint8_t bt_reg_en_pin);
