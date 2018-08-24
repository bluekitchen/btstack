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
import com.bluekitchen.btstack.event.GAPEventAdvertisingReport;
import com.bluekitchen.btstack.event.GATTEventCharacteristicQueryResult;
import com.bluekitchen.btstack.event.GATTEventCharacteristicValueQueryResult;
import com.bluekitchen.btstack.event.GATTEventNotification;
import com.bluekitchen.btstack.event.GATTEventQueryComplete;
import com.bluekitchen.btstack.event.GATTEventServiceQueryResult;
import com.bluekitchen.btstack.event.HCIEventDisconnectionComplete;
import com.bluekitchen.btstack.event.HCIEventLEConnectionComplete;
import com.bluekitchen.btstack.event.SMEventJustWorksRequest;

import java.nio.charset.StandardCharsets;

public class LEStreamerClient implements PacketHandler {

	private enum STATE {
		w4_btstack_working, w4_scan_result, w4_connected, w4_services_complete, w4_characteristic_complete,
		w4_write_client_config_characteristic_complete, active
	};

	private BTstack btstack;
	private STATE state;
	private int testAddrType;
	private BD_ADDR testAddr = new BD_ADDR("00:1A:7D:DA:71:01");
	private int connectionHandle;

	private BT_UUID testServiceUUID        = new BT_UUID("0000FF10-0000-1000-8000-00805F9B34FB");
	private BT_UUID testCharacteristicUUID = new BT_UUID("0000FF11-0000-1000-8000-00805F9B34FB");
	private byte    testNotification = 1; 

	private GATTService testService;
	private GATTCharacteristic testCharacteristic;
		
	public void handlePacket(Packet packet){

		System.out.println("Event " + packet);

		if (packet instanceof SMEventJustWorksRequest){
			SMEventJustWorksRequest event = (SMEventJustWorksRequest) packet;
			System.out.println("Received Just Works pairing request from " + event.getAddress() + " -> auto-accept");
			btstack.SMJustWorksConfirm(event.getHandle());
			return;
		}

		if (packet instanceof HCIEventDisconnectionComplete){
			System.out.println("Received dissconnect, restart scannning.");
			state = STATE.w4_scan_result;
			btstack.GAPLEScanStart();
			return;
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
				if (packet instanceof GAPEventAdvertisingReport){
					// Advertisement received. Connect to the found BT address.
					GAPEventAdvertisingReport report = (GAPEventAdvertisingReport) packet;
					System.out.println(String.format("Adv: type %d, addr %s\ndata: %s\n", report.getAddressType(), report.getAddress(), Util.asHexdump(report.getData())));
					// hack to find 'LE Streamer'
					if (new String(report.getData(), StandardCharsets.UTF_8).indexOf("LE Streamer") > 0){
						testAddrType = report.getAddressType();
						testAddr = report.getAddress();
						System.out.println(String.format("LE Streamer found, connect to %s\n", testAddr));
						btstack.GAPLEScanStop();
						state = STATE.w4_connected;
						btstack.GAPLEConnect(testAddrType, testAddr);
					}
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
					
					// Query test service.
					state = STATE.w4_services_complete;
					connectionHandle = event.getConnectionHandle();
					System.out.println(testAddr + String.format(" - connected %x.\nQuery streamer service.", connectionHandle));
					btstack.GATTDiscoverPrimaryServicesByUUID128(connectionHandle, testServiceUUID);
				}
				break;
			case w4_services_complete:
				if (packet instanceof GATTEventServiceQueryResult){
					// Store streamer service. Wait for GATTEventQueryComplete event to send next GATT command.
					GATTEventServiceQueryResult event = (GATTEventServiceQueryResult) packet;
					System.out.println(testAddr + String.format(" - streamer service %s", event.getService().getUUID()));
					testService = event.getService();
					break;
				}
				if (packet instanceof GATTEventQueryComplete){
					// Check if streamer service is found.
					if (testService == null) {
						System.out.println(testAddr + " - no streamer service. \nRestart scanning.");
						state = STATE.w4_scan_result;
						btstack.GAPLEScanStart(); 
						break;
					}
					System.out.println(testAddr + " - query streamer characteristic.");
					state = STATE.w4_characteristic_complete;
					btstack.GATTDiscoverCharacteristicsForServiceByUUID128(connectionHandle, testService, testCharacteristicUUID);	
				}
				break;
			case w4_characteristic_complete:
				if (packet instanceof GATTEventCharacteristicQueryResult){
					// Store streamer characteristic. Wait for GATTEventQueryComplete event to send next GATT command.
					GATTEventCharacteristicQueryResult event = (GATTEventCharacteristicQueryResult) packet;
					testCharacteristic = event.getCharacteristic();
					System.out.println(testAddr + " - streamer characteristic found.");
					break;
				}
				
				if (!(packet instanceof GATTEventQueryComplete)) break;

				if (testCharacteristic == null) {
					System.out.println("No streamer characteristic found");
					break;
				}
				System.out.println(testAddr + " - enable notifications.");
				state = STATE.w4_write_client_config_characteristic_complete;
				btstack.GATTWriteClientCharacteristicConfiguration(connectionHandle, testCharacteristic, this.testNotification);
				break;

			case w4_write_client_config_characteristic_complete:
				if (packet instanceof GATTEventQueryComplete){
					System.out.println(testAddr + " - notification enabled.");
					state = STATE.active;
				}
				break;

			case active:
				if (packet instanceof GATTEventNotification){
					GATTEventNotification event = (GATTEventNotification) packet;
					System.out.println("Data:");
					Util.hexdump(event.getValue());
				}
				break;

			default:
				break;
		}
	}

	void test(){
		
		System.out.println("LE Streamer Client");

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
		
		// btstack.SMSetAuthenticationRequirements(8 | 4);
		// btstack.SMSetIoCapabilities(4);

		state = STATE.w4_btstack_working;
		btstack.BTstackSetPowerMode(1);
	}
	
	public static void main(String args[]){
		new LEStreamerClient().test();
	}
}
