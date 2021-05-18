
BTstack allows to implement and use GATT Clients in a modular way.

### Battery Service Client {#sec:BatteryServiceClient}}

The Battery Service Client connects to the Battery Services of a remote device and queries its battery level values. Level updates are either received via notifications (if supported by the remote Battery Service), or by manual polling. 

See [Battery Service Client API](appendix/apis/#battery-service-client-api).

### Device Information Service Client {#sec:DeviceInformationServiceClient}}

The Device Information Service Client retrieves the following information from a remote device:
- manufacturer name
- model number     
- serial number    
- hardware revision
- firmware revision
- software revision
- system ID        
- IEEE regulatory certification
- PNP ID  

See [Device Information Service Client API](appendix/apis/#device-information-service-client-api).

### Scan Parameters Service Client {#sec:ScanParametersServiceClient}}

The Scan Parameters Service Client allows to store its LE scan parameters on a remote device such that the remote device can utilize this information to optimize power consumption and/or reconnection latency.

See [Scan Parameters Service Client API](appendix/apis/#scan-parameters-service-client-api).