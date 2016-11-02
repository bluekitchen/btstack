
BTstack allows to implement and use GATT Services in a modular way.

To use an already implemented GATT Service, you only have to add it to your application's
GATT file with:

  - *#import <service_name.gatt>* for .gatt files located in *src/ble/gatt-service* 
  - *#import "service_name.gatt"* for .gatt files located in the same folder as your application's .gatt file.

Each service will have an API at *src/ble/gatt-service/service_name_server.h. To activate it, you need
to call *service_name_init(..)*. Please see the .h file for details.

### Battery Service {#sec:BatteryService}}

The Battery Service allows to query your device's battery level in a standardized way.

After adding it to your .gatt file, you call *battery_service_server_init(value)* with the
current value of your battery. The valid range for the battery level is 0-100.

If the battery level changes, you can call *battery_service_server_set_battery_value(value)*. The service supports sending Notifications if the client enables them.
