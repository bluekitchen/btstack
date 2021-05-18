
BTstack allows to implement and use GATT Services in a modular way.

To use an already implemented GATT Service Server, you only have to add it to your application's
GATT file with:

  - `#import <service_name.gatt>` for .gatt files located in *src/ble/gatt-service* 
  - `#import "service_name.gatt"` for .gatt files located in the same folder as your application's .gatt file.

Each service will have an API at *src/ble/gatt-service/service_name_server.h. To activate it, you need
to call *service_name_init(..)*. Please see the .h file for details.

### Battery Service Server {#sec:BatteryServiceServer}}

The Battery Service allows to query your device's battery level in a standardized way.

To use with your application, add `#import <batery_service.gatt>` to your .gatt file. 
After adding it to your .gatt file, you call *battery_service_server_init(value)* with the
current value of your battery. The valid range for the battery level is 0-100.

If the battery level changes, you can call *battery_service_server_set_battery_value(value)*. The service supports sending Notifications if the client enables them.

See [Battery Service Server API](appendix/apis/#battery-service-server-api).

### Cycling Power Service Server {#sec:CyclingPowerServiceServer}

The Cycling Power Service allows to query device's power- and force-related data and optionally speed- and cadence-related data for use in sports and fitness applications.

To use with your application, add `#import <cycling_power_service.gatt>` to your .gatt file. 

See [Cycling Power Service Server API](appendix/apis/#cycling-power-service-server-api).

### Cycling Speed and Cadence Service Server {#sec:CyclingSpeedAndCadenceServiceServer}

The Cycling Speed and Cadence Service allows to query device's speed- and cadence-related data for use in sports and fitness applications.

To use with your application, add `#import <cycling_speed_and_cadence_service.gatt>` to your .gatt file. 

See [Cycling Speed and Cadence Service Server API](appendix/apis/#cycling-speed-and-cadence-service-server-api).

### Device Information Service Server {#sec:DeviceInformationServiceServer}

TheDevice Information Service allows to query manufacturer and/or vendor information about a device.

To use with your application, add `#import <device_information_service.gatt>` to your .gatt file. 

See [Device Information Service Server API](appendix/apis/#device-information-service-server-api).


### Heart Rate Service Server {#sec:HeartRateServiceServer}}
 
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

See [Heart Rate Service Server API](appendix/apis/#heart-rate-service-server-api).


### Mesh Provisioning Service Server {#sec:MeshProvisioningServiceServer}

The Mesh Provisioning Service allows a Provisioning Client to provision a device to participate in the mesh network.

To use with your application, add `#import <mesh_provisioning_service.gatt>` to your .gatt file. 

See [Mesh Provisioning Service Server API](appendix/apis/#mesh-provisioning-service-server-api).

### Mesh Proxy Service Server {#sec:MeshProxyServiceServer}

The Mesh Proxy Service is used to enable a server to send and receive Proxy PDUs with a client.

To use with your application, add `#import <mesh_proxy_service.gatt>` to your .gatt file. 

See [Mesh Proxy Service Server API](appendix/apis/#mesh-proxy-service-server-api).

### Nordic SPP Service Server {#sec:NordicSPPServiceServer}

The Nordic SPP Service is implementation of the Nordic SPP-like profile.

To use with your application, add `#import <nordic_spp_service.gatt>` to your .gatt file. 

See [Nordic SPP Service Server API](appendix/apis/#nordic-spp-service-server-api).

### Scan Parameters Service Server {#sec:ScanParametersServiceServer}

The Scan Parameters Service enables a remote GATT Client to store the LE scan parameters it is using locally. These parameters can be utilized by the application to optimize power consumption and/or reconnection latency.

To use with your application, add `#import <scan_parameters_service.gatt>` to your .gatt file. 

See [Scan Parameters Service Server API](appendix/apis/#scan-parameters-server-api).

### u-blox SPP Service Server {#sec:ubloxSPPServiceServer}

The u-blox SPP Service is implementation of the u-Blox SPP-like profile.

To use with your application, add `#import <ublox_spp_service.gatt>` to your .gatt file. 

See [u-blox SPP Service Server API](appendix/apis/#ublox-spp-service-server-api).