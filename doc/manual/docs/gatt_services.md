
BTstack allows to implement and use GATT Services in a modular way.

To use an already implemented GATT Service, you only have to add it to your application's
GATT file with:

  - `#import <service_name.gatt>` for .gatt files located in *src/ble/gatt-service* 
  - `#import "service_name.gatt"` for .gatt files located in the same folder as your application's .gatt file.

Each service will have an API at *src/ble/gatt-service/service_name_server.h. To activate it, you need
to call *service_name_init(..)*. Please see the .h file for details.

### Battery Service {#sec:BatteryService}}

The Battery Service allows to query your device's battery level in a standardized way.

To use with your application, add `#import <batery_service.gatt>` to your .gatt file. 
After adding it to your .gatt file, you call *battery_service_server_init(value)* with the
current value of your battery. The valid range for the battery level is 0-100.

If the battery level changes, you can call *battery_service_server_set_battery_value(value)*. The service supports sending Notifications if the client enables them.


### Heart Rate Service {#sec:HeartRateService}}
 
The heart rate service server provides heart rate measurements via notifications.

Each notification reports the heart rate measurement in beats per minute, and if enabled, 
the total energy expended in kilo Joules, as well as RR-intervals in 1/1024 seconds resolution.

The Energy Expended field represents the accumulated energy expended
in kilo Joules since the last time it was reset. If the maximum value of 65535
kilo Joules is reached, it will remain at this value, until a reset command
from the client is received.
 
The RR-Interval represents the time between two consecutive R waves in 
an Electrocardiogram (ECG) waveform. If needed, the RR-Intervals are sent in
multiple notifications.

To use with your application, add `#import <heart_rate_service.gatt>` to your .gatt file.
After adding it to your .gatt file, you call *heart_rate_server_init(body_sensor_location, energy_expended_supported)*
with the intended sensor location, and a flag indicating if energy expanded is supported.

If heart rate measurement changes, you can call 
*heart_rate_service_server_update_heart_rate_values(heart_rate_bpm, service_sensor_contact_status, rr_interval_count, rr_intervals)*. 
This function will trigger sending Notifications if the client enables them.

If energy expanded is supported, you can call *heart_rate_service_add_energy_expended(energy_expended_kJ)* 
with the newly expanded energy. The accumulated energy expended value
will be emitted with the next heart rate measurement.

