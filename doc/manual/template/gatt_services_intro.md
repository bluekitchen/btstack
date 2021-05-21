#

BTstack allows to implement and use GATT Services in a modular way.

To use an already implemented GATT Service Server, you only have to add it to your application's GATT file with:
  - `#import <service_name.gatt>` for .gatt files located in *src/ble/gatt-service* 
  - `#import "service_name.gatt"` for .gatt files located in the same folder as your application's .gatt file.

Each service will have an API at *src/ble/gatt-service/service_name_server.h*. To activate it, you need to call *service_name_init(..)*.

Please see the .h file for details.
