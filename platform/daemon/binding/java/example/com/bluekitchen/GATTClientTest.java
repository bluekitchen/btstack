package com.bluekitchen;

import com.bluekitchen.btstack.BD_ADDR;
import com.bluekitchen.btstack.BT_UUID;
import com.bluekitchen.btstack.BTstack;
import com.bluekitchen.btstack.GATTCharacteristic;
import com.bluekitchen.btstack.GATTService;
import com.bluekitchen.btstack.Packet;
import com.bluekitchen.btstack.PacketHandler;
import com.bluekitchen.btstack.Util;
import com.bluekitchen.btstack.event.BTstackEventState;
import com.bluekitchen.btstack.event.GAPLEAdvertisingReport;
import com.bluekitchen.btstack.event.GATTCharacteristicQueryResult;
import com.bluekitchen.btstack.event.GATTCharacteristicValueQueryResult;
import com.bluekitchen.btstack.event.GATTNotification;
import com.bluekitchen.btstack.event.GATTQueryComplete;
import com.bluekitchen.btstack.event.GATTServiceQueryResult;
import com.bluekitchen.btstack.event.HCIEventDisconnectionComplete;
import com.bluekitchen.btstack.event.HCIEventLEConnectionComplete;

public class GATTClientTest implements PacketHandler {

	private enum STATE {
		w4_btstack_working, w4_scan_result, w4_connected, w4_services_complete, w4_characteristic_complete, w4_characteristic_read
	, w4_characteristic_write, w4_acc_service_result, w4_acc_enable_characteristic_result, w4_write_acc_enable_result, w4_acc_client_config_characteristic_result, w4_acc_client_config_result,
	w4_acc_data, w4_connected_acc, battery_data
	};

	private BTstack btstack;
	private STATE state;
	private int testAddrType;
	private BD_ADDR testAddr;
	private int testHandle;
	private GATTService testService;
	private GATTCharacteristic testCharacteristic;
	private int service_count = 0;
	private int characteristic_count = 0;
	private int connectionHandle;
	private int counter = 0;

	private byte[] acc_service_uuid =           new byte[] {(byte)0xf0, 0, (byte)0xaa, (byte)0x10, 4, (byte)0x51, (byte)0x40, 0, (byte)0xb0, 0, 0, 0, 0, 0, 0, 0};
	private byte[] acc_chr_client_config_uuid = new byte[] {(byte)0xf0, 0, (byte)0xaa, (byte)0x11, 4, (byte)0x51, (byte)0x40, 0, (byte)0xb0, 0, 0, 0, 0, 0, 0, 0};
	private byte[] acc_chr_enable_uuid =        new byte[] {(byte)0xf0, 0, (byte)0xaa, (byte)0x12, 4, (byte)0x51, (byte)0x40, 0, (byte)0xb0, 0, 0, 0, 0, 0, 0, 0};
	private byte[] acc_enable = new byte[] {1};
	private byte acc_notification = 1; 
	private GATTService accService;
	private GATTCharacteristic enableCharacteristic;
	private GATTCharacteristic configCharacteristic;

	private GATTService batteryService;
	private GATTCharacteristic batteryLevelCharacteristic;
	private byte[] battery_level_chr_uuid = new byte[] {0, 0, (byte)0x2a, (byte)0x19, 0, 0, (byte)0x10, 0, (byte)0x80, 0, 0, (byte)0x80, (byte)0x5f, (byte)0x9b, (byte)0x34, (byte)0xfb}; 
	GATTCharacteristicValueQueryResult battery;
	
	private BT_UUID uuid128(byte[] att_uuid) {
		byte [] uuid = new byte[16];
		Util.flipX(att_uuid, uuid);	
		return new BT_UUID(uuid);
	}
	
	public void handlePacket(Packet packet){
		if (packet instanceof HCIEventDisconnectionComplete){
			System.out.println(String.format("Received dissconnect, restart scannning."));
			state = STATE.w4_scan_result;
			btstack.GAPLEScanStart();
			return;
		}
		
		if (packet instanceof GATTQueryComplete){
			GATTQueryComplete event = (GATTQueryComplete) packet;
			System.out.println(testAddr + " battery data");
			if (event.getStatus() != 0){
				System.out.println("Battery data could not be read.\nRestart scanning.");
				state = STATE.w4_scan_result;
				btstack.GAPLEScanStart(); 
				return;
			}
		}

		switch (state){
			case w4_btstack_working:
				if (packet instanceof BTstackEventState){
					BTstackEventState event = (BTstackEventState) packet;
					if (event.getState() == 2)	{
						System.out.println("BTstack working, start scanning.");
						state = STATE.w4_scan_result;
						btstack.GAPLEScanStart();
					}
				}
				break;
			case w4_scan_result:
				if (packet instanceof GAPLEAdvertisingReport){
					// Advertisement received. Connect to the found BT address.
					GAPLEAdvertisingReport report = (GAPLEAdvertisingReport) packet;
					testAddrType = report.getAddressType();
					testAddr = report.getAddress();
					System.out.println(String.format("Adv: type %d, addr %s\ndata: %s \n Stop scan, initiate connect.", testAddrType, testAddr, Util.asHexdump(report.getData())));
					btstack.GAPLEScanStop();
					state = STATE.w4_connected;
					btstack.GAPLEConnect(testAddrType, testAddr);
				}
				break;
			case w4_connected:
				if (packet instanceof HCIEventLEConnectionComplete){
					HCIEventLEConnectionComplete event = (HCIEventLEConnectionComplete) packet;
					if (event.getStatus() != 0) {
						System.out.println(testAddr + String.format(" - connection failed, status %d.\nRestart scanning.", event.getStatus()));
						state = STATE.w4_scan_result;
						btstack.GAPLEScanStart();
						break;
					}
					
					// Query battery service.
					state = STATE.w4_services_complete;
					connectionHandle = event.getConnectionHandle();
					System.out.println(testAddr + String.format(" - connected %x.\nQuery battery service.", connectionHandle));
					btstack.GATTDiscoverPrimaryServicesByUUID16(connectionHandle, 0x180f);
				}
				break;
			case w4_services_complete:
				if (packet instanceof GATTServiceQueryResult){
					// Store battery service. Wait for GATTQueryComplete event to send next GATT command.
					GATTServiceQueryResult event = (GATTServiceQueryResult) packet;
					System.out.println(testAddr + String.format(" - battery service %s", event.getService().getUUID()));
					batteryService = event.getService();
					break;
				}
				if (packet instanceof GATTQueryComplete){
					// Check if battery service is found.
					if (batteryService == null) {
						System.out.println(testAddr + " - no battery service. \nRestart scanning.");
						state = STATE.w4_scan_result;
						btstack.GAPLEScanStart(); 
						break;
					}
					System.out.println(testAddr + " - query battery level.");
					state = STATE.w4_characteristic_complete;
					btstack.GATTDiscoverCharacteristicsForServiceByUUID128(connectionHandle, batteryService, uuid128(this.battery_level_chr_uuid));	
				}
				break;
			case w4_characteristic_complete:
				if (packet instanceof GATTCharacteristicQueryResult){
					// Store battery level characteristic. Wait for GATTQueryComplete event to send next GATT command.
					GATTCharacteristicQueryResult event = (GATTCharacteristicQueryResult) packet;
					batteryLevelCharacteristic = event.getCharacteristic();
					System.out.println(testAddr + " - battery level found.");
					break;
				}
				
				if (!(packet instanceof GATTQueryComplete)) break;
				if (batteryLevelCharacteristic == null) {
					System.out.println("No battery level characteristic found");
					break;
				}
				System.out.println(testAddr + " - polling battery.");
				counter = 0;
				state = STATE.battery_data;
				new Thread(new Runnable(){
					@Override
					public void run() {
						try {
							while(state == STATE.battery_data){
								Thread.sleep(5000);
								btstack.GATTReadValueOfCharacteristic(connectionHandle, batteryLevelCharacteristic);
							}
						} catch (InterruptedException e) {}
					}
				}).start();
				break;
			case battery_data:
				if (packet instanceof GATTCharacteristicValueQueryResult){
					GATTCharacteristicValueQueryResult battery = (GATTCharacteristicValueQueryResult) packet;
					
					if (battery.getValueLength() != 1) break;
					byte[] data = battery.getValue();
					counter = counter + 1;
					System.out.println(String.format("Counter %d, battery level: %d", counter, data[0]) + "%");
					break;
					
				}
				break;
			default:
				break;
		}
	}

	public void handlePacketAcc(Packet packet){
		
//		System.out.println(packet.toString());
		if (packet instanceof HCIEventDisconnectionComplete){
			HCIEventDisconnectionComplete event = (HCIEventDisconnectionComplete) packet;
			testHandle = event.getConnectionHandle();
			System.out.println(String.format("Received disconnect, status %d, handle %x", event.getStatus(), testHandle));
			return;
		}
		
		switch (state){
		case w4_btstack_working:
			if (packet instanceof BTstackEventState){
				BTstackEventState event = (BTstackEventState) packet;
				if (event.getState() == 2)	{
					
					System.out.println("GAPLEScanStart()");
					state = STATE.w4_scan_result;
					btstack.GAPLEScanStart();
				}
			}
			break;
		case w4_scan_result:
			if (packet instanceof GAPLEAdvertisingReport){
				GAPLEAdvertisingReport report = (GAPLEAdvertisingReport) packet;
				testAddrType = report.getAddressType();
				testAddr = report.getAddress();
				System.out.println(String.format("Adv: type %d, addr %s", testAddrType, testAddr));
				System.out.println(String.format("Data: %s", Util.asHexdump(report.getData())));
				System.out.println("GAPLEScanStop()");
				btstack.GAPLEScanStop();
				System.out.println("GAPLEConnect(...)");
				state = STATE.w4_connected_acc;
				btstack.GAPLEConnect(testAddrType, testAddr);
			}
			break;
			
		case w4_connected:
			if (packet instanceof HCIEventLEConnectionComplete){
				HCIEventLEConnectionComplete event = (HCIEventLEConnectionComplete) packet;
				testHandle = event.getConnectionHandle();
				System.out.println(String.format("Connection complete, status %d, handle %x", event.getStatus(), testHandle));
				state = STATE.w4_services_complete;
				System.out.println("GATTDiscoverPrimaryServices(...)");
				btstack.GATTDiscoverPrimaryServices(testHandle);
			}
			break;
		case w4_services_complete:
			if (packet instanceof GATTServiceQueryResult){
				GATTServiceQueryResult event = (GATTServiceQueryResult) packet;
				if (testService == null){
					System.out.println(String.format("First service UUID %s", event.getService().getUUID()));
					testService = event.getService();
				}
				System.out.println("Service: " + event.getService());
				service_count++;
			}
			if (packet instanceof GATTQueryComplete){
				System.out.println(String.format("Service query complete, total %d services", service_count));
				state = STATE.w4_characteristic_complete;
				btstack.GATTDiscoverCharacteristicsForService(testHandle, testService);
			}
			break;
				
		case w4_characteristic_complete:
			if (packet instanceof GATTCharacteristicQueryResult){
				GATTCharacteristicQueryResult event = (GATTCharacteristicQueryResult) packet;
				if (testCharacteristic == null){
					System.out.println(String.format("First characteristic UUID %s", event.getCharacteristic().getUUID()));
					testCharacteristic = event.getCharacteristic();
				}
				System.out.println("Characteristic: " + event.getCharacteristic());
				characteristic_count++;
			}
			if (packet instanceof GATTQueryComplete){
				System.out.println(String.format("Characteristic query complete, total %d characteristics", characteristic_count));
				state = STATE.w4_characteristic_read;
				btstack.GATTReadValueOfCharacteristic(testHandle, testCharacteristic);
			}			
			break;

		case w4_characteristic_read:
			if (packet instanceof GATTCharacteristicValueQueryResult){
				System.out.println("Read complete");
				System.out.println( packet.toString());
				state = STATE.w4_characteristic_write;
				byte [] data = { 'B', 'T', 's', 't', 'a', 'c', 'k'};
				btstack.GATTWriteValueOfCharacteristic(testHandle, testCharacteristic, data.length, data);
			}
			break;
		case w4_characteristic_write:
			if (packet instanceof GATTQueryComplete){
				System.out.println("Write complete, search for ACC service");
				state = STATE.w4_acc_service_result;
				btstack.GATTDiscoverPrimaryServicesByUUID128(testHandle, new BT_UUID(this.acc_service_uuid)); // not working
			}
			break;
		
		case w4_connected_acc:
			if (packet instanceof HCIEventLEConnectionComplete){
				HCIEventLEConnectionComplete event = (HCIEventLEConnectionComplete) packet;
				testHandle = event.getConnectionHandle();
				System.out.println(String.format("Connection complete, status %d, handle %x", event.getStatus(), testHandle));
				System.out.println("Search for ACC service");
				state = STATE.w4_acc_service_result;
				byte [] uuid = new byte[16];
				Util.flipX(this.acc_service_uuid, uuid);	// works
				btstack.GATTDiscoverPrimaryServicesByUUID128(testHandle, new BT_UUID(uuid));
			}
			break;
		
		 case w4_acc_service_result:
			System.out.println(String.format("w4_acc_service_result state"));
			if (packet instanceof GATTServiceQueryResult){
				GATTServiceQueryResult event = (GATTServiceQueryResult) packet;
				System.out.println(String.format("ACC service found %s", event.getService().getUUID()));
				accService = event.getService();
				break;
			}
			if (packet instanceof GATTQueryComplete){
				if (accService == null) {
					System.out.println("No acc service found");
					break;
				}
				System.out.println("ACC Service found, searching for acc enable characteristic");
				state = STATE.w4_acc_enable_characteristic_result;
				byte [] uuid = new byte[16];
				Util.flipX(this.acc_chr_enable_uuid, uuid);
				btstack.GATTDiscoverCharacteristicsForServiceByUUID128(testHandle, accService, new BT_UUID(uuid));
			}
			break;
		
		case w4_acc_enable_characteristic_result:
			if (packet instanceof GATTCharacteristicQueryResult){
				GATTCharacteristicQueryResult event = (GATTCharacteristicQueryResult) packet;
				enableCharacteristic = event.getCharacteristic();
				System.out.println("Enable ACC Characteristic found ");
			}
			if (packet instanceof GATTQueryComplete){
				if (enableCharacteristic == null) {
					System.out.println("No acc enable chr found");
					break;
				}
				System.out.println("Write enable acc characteristic");
				state = STATE.w4_write_acc_enable_result;
				btstack.GATTWriteValueOfCharacteristic(testHandle, enableCharacteristic, 1, this.acc_enable);
			}	
			break;
		case w4_write_acc_enable_result:
			if (packet instanceof GATTQueryComplete){
				System.out.println("Acc enabled,searching for acc client config characteristic");
				byte [] uuid = new byte[16];
				Util.flipX(this.acc_chr_client_config_uuid, uuid);
				btstack.GATTDiscoverCharacteristicsForServiceByUUID128(testHandle, accService, new BT_UUID(uuid));
				state = STATE.w4_acc_client_config_characteristic_result;
			}
			break;
			
		case w4_acc_client_config_characteristic_result:
			if (packet instanceof GATTCharacteristicQueryResult){
				GATTCharacteristicQueryResult event = (GATTCharacteristicQueryResult) packet;
				configCharacteristic = event.getCharacteristic();
				System.out.println("ACC Client Config Characteristic found");
			}
			if (packet instanceof GATTQueryComplete){
				if (configCharacteristic == null) {
					System.out.println("No acc config chr found");
					break;
				}
				System.out.println("Write ACC Client Config Characteristic");
				state = STATE.w4_acc_data;
				btstack.GATTWriteClientCharacteristicConfiguration(testHandle, configCharacteristic, this.acc_notification);
			}	
			break;
		
		case w4_acc_data:
			if (packet instanceof GATTQueryComplete){
				System.out.println("Acc configured for notification");
				break;
			}
			
			if (packet instanceof GATTNotification){
				System.out.println("Acc Value");
				System.out.println(packet.toString());
				btstack.GAPDisconnect(testHandle);
			}
		
		default:
			break;
		}
	}
	
	void test(){
		
		System.out.println("LE Test Application");

		// connect to BTstack Daemon via default port on localhost
		btstack = new BTstack();
		btstack.setTcpPort(BTstack.DEFAULT_TCP_PORT);
		btstack.registerPacketHandler(this);
		boolean ok = btstack.connect();
		if (!ok) {
			System.out.println("Failed to connect to BTstack Server");
			return;
		}
					
		System.out.println("BTstackSetPowerMode(1)");
		
		state = STATE.w4_btstack_working;
		btstack.BTstackSetPowerMode(1);
	}
	
	public static void main(String args[]){
		new GATTClientTest().test();
	}
}
