package com.bluekitchen.lescan;

import android.app.Activity;
import android.os.Bundle;
import android.util.Log;
import android.view.Menu;
import android.widget.TextView;

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
import com.bluekitchen.btstack.event.HCIEventCommandComplete;
import com.bluekitchen.btstack.event.HCIEventDisconnectionComplete;
import com.bluekitchen.btstack.event.HCIEventLEConnectionComplete;

public class MainActivity extends Activity implements PacketHandler {

	private static final String BTSTACK_TAG = "BTstack";

	private enum STATE {
		w4_btstack_working, w4_scan_result, w4_connected, w4_services_complete, w4_characteristic_complete, w4_characteristic_read
		, w4_characteristic_write, w4_acc_service_result, w4_acc_enable_characteristic_result, w4_write_acc_enable_result, w4_acc_client_config_characteristic_result, w4_acc_client_config_result,
		w4_acc_data, w4_connected_acc, w4_disconnect, track_rssi, battery_data
	};

	private TextView tv;
	private BTstack btstack;
	private STATE state;
	private int testAddrType;
	private BD_ADDR deviceAddr;
	private int connectionHandle;
	private GATTService testService;
	private GATTCharacteristic testCharacteristic;
	private int service_count = 0;
	private int characteristic_count = 0;
	private int test_run_count = 0;
	private String onScreenMessage = "";
	
	BD_ADDR sensor_tag_addr = new BD_ADDR("1C:BA:8C:20:C7:F6");
	// Accelerometer
	private byte[] acc_service_uuid =           new byte[] {(byte)0xf0, 0, (byte)0xaa, (byte)0x10, 4, (byte)0x51, (byte)0x40, 0, (byte)0xb0, 0, 0, 0, 0, 0, 0, 0};
	private byte[] acc_chr_client_config_uuid = new byte[] {(byte)0xf0, 0, (byte)0xaa, (byte)0x11, 4, (byte)0x51, (byte)0x40, 0, (byte)0xb0, 0, 0, 0, 0, 0, 0, 0};
	private byte[] acc_chr_enable_uuid =        new byte[] {(byte)0xf0, 0, (byte)0xaa, (byte)0x12, 4, (byte)0x51, (byte)0x40, 0, (byte)0xb0, 0, 0, 0, 0, 0, 0, 0};
	private byte[] acc_enable = new byte[] {1};
	private byte acc_notification = 1; 
	private GATTService accService;
	private GATTCharacteristic enableCharacteristic;
	private GATTCharacteristic configCharacteristic;
	
	// Battery 
	private GATTService batteryService;
	private GATTCharacteristic batteryLevelCharacteristic;
	//private byte[] battery_service_uuid =   new byte[] {0, 0, (byte)0x18, (byte)0x0F, 0, 0, (byte)0x10, 0, (byte)0x80, 0, 0, (byte)0x80, (byte)0x5f, (byte)0x9b, (byte)0x34, (byte)0xfb};
	private byte[] battery_level_chr_uuid = new byte[] {0, 0, (byte)0x2a, (byte)0x1b, 0, 0, (byte)0x10, 0, (byte)0x80, 0, 0, (byte)0x80, (byte)0x5f, (byte)0x9b, (byte)0x34, (byte)0xfb}; 
	GATTCharacteristicValueQueryResult battery;
	private int batteryLevel = 0;
	private int counter;


	@Override
	protected void onCreate(Bundle savedInstanceState) {
		super.onCreate(savedInstanceState);
		setContentView(R.layout.activity_main);

		tv = new TextView(this);
		setContentView(tv);

		test();
	}

	void addMessage(final String message){
		onScreenMessage = onScreenMessage + "\n" + message;
		Log.d(BTSTACK_TAG, message);
		runOnUiThread(new Runnable(){
			public void run(){
				tv.setText(onScreenMessage);
			}
		});
	}
	
	
	void addTempMessage(final String message){
		Log.d(BTSTACK_TAG, message);
		runOnUiThread(new Runnable(){
			public void run(){
				tv.setText(onScreenMessage +"\n" + message);
			}
		});
	}
	
	void clearMessages(){
		onScreenMessage = "";
		runOnUiThread(new Runnable(){
			public void run(){
				tv.setText(onScreenMessage);
			}
		});
	}

	
	@Override
	public boolean onCreateOptionsMenu(Menu menu) {
		// Inflate the menu; this adds items to the action bar if it is present.
		getMenuInflater().inflate(R.menu.main, menu);
		return true;
	}

	
	public void testCharacteristics(Packet packet){
		
		switch (state){
		case w4_btstack_working:
			if (packet instanceof BTstackEventState){
				BTstackEventState event = (BTstackEventState) packet;
				if (event.getState() == 2)	{
					addMessage("GAPLEScanStart()");
					state = STATE.w4_scan_result;
					btstack.GAPLEScanStart();
				}
			}
			break;
		case w4_scan_result:
			if (packet instanceof GAPLEAdvertisingReport){
				GAPLEAdvertisingReport report = (GAPLEAdvertisingReport) packet;
				testAddrType = report.getAddressType();
				deviceAddr = report.getAddress();
				addMessage(String.format("Adv: type %d, addr %s", testAddrType, deviceAddr));
				addMessage(String.format("Data: %s", Util.asHexdump(report.getData())));
				addMessage("GAPLEScanStop()");
				btstack.GAPLEScanStop();
				addMessage("GAPLEConnect(...)");
				state = STATE.w4_connected;
				btstack.GAPLEConnect(testAddrType, deviceAddr);
			}
			break;
		case w4_connected:
			if (packet instanceof HCIEventLEConnectionComplete){
				HCIEventLEConnectionComplete event = (HCIEventLEConnectionComplete) packet;
				connectionHandle = event.getConnectionHandle();
				addMessage(String.format("Connection complete, status %d, handle %x", event.getStatus(), connectionHandle));
				state = STATE.w4_services_complete;
				addMessage("GATTDiscoverPrimaryServices(...)");
				btstack.GATTDiscoverPrimaryServices(connectionHandle);
			}
			break;
		case w4_services_complete:
			if (packet instanceof GATTServiceQueryResult){
				GATTServiceQueryResult event = (GATTServiceQueryResult) packet;
				if (testService == null){
					addMessage(String.format("First service UUID %s", event.getService().getUUID()));
					testService = event.getService();
				}
				Log.d(BTSTACK_TAG, "Service: " + event.getService());
				service_count++;
			}
			if (packet instanceof GATTQueryComplete){
				addMessage(String.format("Service query complete, total %d services", service_count));
				state = STATE.w4_characteristic_complete;
				btstack.GATTDiscoverCharacteristicsForService(connectionHandle, testService);
			}
			break;

		case w4_characteristic_complete:
			if (packet instanceof GATTCharacteristicQueryResult){
				GATTCharacteristicQueryResult event = (GATTCharacteristicQueryResult) packet;
				if (testCharacteristic == null){
					addMessage(String.format("First characteristic UUID %s", event.getCharacteristic().getUUID()));
					testCharacteristic = event.getCharacteristic();
				}
				Log.d(BTSTACK_TAG, "Characteristic: " + event.getCharacteristic());
				characteristic_count++;
			}
			if (packet instanceof GATTQueryComplete){
				addMessage(String.format("Characteristic query complete, total %d characteristics", characteristic_count));
				if (characteristic_count > 0){
					state = STATE.w4_characteristic_read;
					btstack.GATTReadValueOfCharacteristic(connectionHandle, testCharacteristic);
				} else {
					state = STATE.w4_disconnect;
					btstack.GAPDisconnect(connectionHandle);
				}
			}			
			break;

		case w4_characteristic_read:
			if (packet instanceof GATTCharacteristicValueQueryResult){
				addMessage("Read complete");
				Log.d(BTSTACK_TAG, packet.toString());
				state = STATE.w4_characteristic_write;
				byte [] data = { 'B', 'T', 's', 't', 'a', 'c', 'k'};
				btstack.GATTWriteValueOfCharacteristic(connectionHandle, testCharacteristic, data.length, data);
			}
			break;
		case w4_characteristic_write:
			if (packet instanceof GATTQueryComplete){
				addMessage("Write complete, disconnect now.");
				state = STATE.w4_disconnect;
				// btstack.GAPDisconnect(testHandle);
			}
			break;
		case w4_disconnect:
			if (packet instanceof HCIEventDisconnectionComplete){
				addMessage("Disconnected.");
			}
			break;
		default:
			break;
		}
	}

	public void testAccelerometer(Packet packet){
		switch (state){
		case w4_btstack_working:
			if (packet instanceof BTstackEventState){
				BTstackEventState event = (BTstackEventState) packet;
				if (event.getState() == 2)	{
					addMessage("GAPLEScanStart()");
					state = STATE.w4_scan_result;
					btstack.GAPLEScanStart();
				}
			}
			break;
		case w4_scan_result:
			if (packet instanceof GAPLEAdvertisingReport){
				GAPLEAdvertisingReport report = (GAPLEAdvertisingReport) packet;
				testAddrType = report.getAddressType();
				deviceAddr = report.getAddress();
				if (deviceAddr.toString().equalsIgnoreCase(sensor_tag_addr.toString())){
					addMessage(String.format("Adv: type %d, addr %s", testAddrType, deviceAddr));
					addMessage(String.format("Data: %s", Util.asHexdump(report.getData())));
					addMessage("GAPLEScanStop()");
					btstack.GAPLEScanStop();
					addMessage("GAPLEConnect(...)");
					state = STATE.w4_connected_acc;
					btstack.GAPLEConnect(testAddrType, deviceAddr);
				}
			}
			break;



		case w4_connected_acc:
			if (packet instanceof HCIEventLEConnectionComplete){
				HCIEventLEConnectionComplete event = (HCIEventLEConnectionComplete) packet;
				connectionHandle = event.getConnectionHandle();
				addMessage(String.format("Connection complete, status %d, handle %x", event.getStatus(), connectionHandle));
				addMessage("Search for ACC service");
				state = STATE.w4_acc_service_result;
				byte [] uuid = new byte[16];
				Util.flipX(this.acc_service_uuid, uuid);	
				btstack.GATTDiscoverPrimaryServicesByUUID128(connectionHandle, new BT_UUID(uuid));
			}
			break;

		case w4_acc_service_result:
			clearMessages();
			addMessage(String.format("w4_acc_service_result state"));
			if (packet instanceof GATTServiceQueryResult){
				GATTServiceQueryResult event = (GATTServiceQueryResult) packet;
				addMessage(String.format("ACC service found %s", event.getService().getUUID()));
				accService = event.getService();
				break;
			}
			if (packet instanceof GATTQueryComplete){
				if (accService == null) {
					addMessage("No acc service found");
					break;
				}
				addMessage("ACC Service found, searching for acc enable characteristic");
				state = STATE.w4_acc_enable_characteristic_result;
				byte [] uuid = new byte[16];
				Util.flipX(this.acc_chr_enable_uuid, uuid);
				btstack.GATTDiscoverCharacteristicsForServiceByUUID128(connectionHandle, accService, new BT_UUID(uuid));
			}
			break;

		case w4_acc_enable_characteristic_result:
			if (packet instanceof GATTCharacteristicQueryResult){
				GATTCharacteristicQueryResult event = (GATTCharacteristicQueryResult) packet;
				enableCharacteristic = event.getCharacteristic();
				addMessage("Enable ACC Characteristic found ");
			}
			if (packet instanceof GATTQueryComplete){
				if (enableCharacteristic == null) {
					addMessage("No acc enable chr found");
					break;
				}
				addMessage("Write enable acc characteristic");
				state = STATE.w4_write_acc_enable_result;
				btstack.GATTWriteValueOfCharacteristic(connectionHandle, enableCharacteristic, 1, this.acc_enable);
			}	
			break;
		case w4_write_acc_enable_result:
			if (packet instanceof GATTQueryComplete){
				addMessage("Acc enabled,searching for acc client config characteristic");
				byte [] uuid = new byte[16];
				Util.flipX(this.acc_chr_client_config_uuid, uuid);
				btstack.GATTDiscoverCharacteristicsForServiceByUUID128(connectionHandle, accService, new BT_UUID(uuid));
				state = STATE.w4_acc_client_config_characteristic_result;
			}
			break;

		case w4_acc_client_config_characteristic_result:
			if (packet instanceof GATTCharacteristicQueryResult){
				GATTCharacteristicQueryResult event = (GATTCharacteristicQueryResult) packet;
				configCharacteristic = event.getCharacteristic();
				addMessage("ACC Client Config Characteristic found");
			}
			if (packet instanceof GATTQueryComplete){
				if (configCharacteristic == null) {
					addMessage("No acc config chr found");
					break;
				}
				addMessage("Write ACC Client Config Characteristic");
				state = STATE.w4_acc_data;
				btstack.GATTWriteClientCharacteristicConfiguration(connectionHandle, configCharacteristic, this.acc_notification);
			}	
			break;

		case w4_acc_data:
			if (packet instanceof GATTQueryComplete){
				addMessage("Acc configured for notification");
				break;
			}

			if (packet instanceof GATTNotification){
				addTempMessage("Acc Value");
				Log.d(BTSTACK_TAG, packet.toString());
				//state = STATE.w4_btstack_working;
				//btstack.GAPDisconnect(connectionHandle);
			}
			break;
			
		default:
			break;
		}
	}

	public void testConnectDisconnect(Packet packet){
		if (packet instanceof HCIEventDisconnectionComplete){
			if (state != STATE.w4_disconnect) {
				state = STATE.w4_scan_result;
				btstack.GAPLEScanStart();
				clearMessages();
				HCIEventDisconnectionComplete event = (HCIEventDisconnectionComplete) packet;
				addMessage(String.format("Unexpected disconnect %x. Start scan.", event.getConnectionHandle()));
				return;
			}
		}
		
		switch (state){
		case w4_btstack_working:
			if (packet instanceof BTstackEventState){
				BTstackEventState event = (BTstackEventState) packet;
				if (event.getState() == 2)	{
//					addMessage("GAPLEScanStart()");
//					state = STATE.w4_scan_result;
					// btstack.GAPLEScanStart();
//					runOnUiThread(new Runnable(){
//						public void run(){
							addMessage("BTstack dissconnect.");
							btstack.disconnect();

//					runOnUiThread(new Runnable(){
//						public void run(){
							try {
								Thread.sleep(5000);
							} catch (InterruptedException e) {
								e.printStackTrace();
							}
							btstack.connect();
							addMessage("Power on BTstack.");
							state = STATE.w4_btstack_working;
							test_run_count = 0;
							btstack.BTstackSetPowerMode(1);
//						}
//					});
				}
			}
			break;
		case w4_scan_result:
			if (packet instanceof GAPLEAdvertisingReport){
				BD_ADDR sensor_tag_addr = new BD_ADDR("1C:BA:8C:20:C7:F6");
				//BD_ADDR pts_dongle = new BD_ADDR("00:1B:DC:07:32:EF");

				GAPLEAdvertisingReport report = (GAPLEAdvertisingReport) packet;
				BD_ADDR reportAddr = report.getAddress();
				if (reportAddr.toString().equalsIgnoreCase(sensor_tag_addr.toString())){
					testAddrType = report.getAddressType();
					deviceAddr = report.getAddress();
					addMessage(String.format("Adv: type %d, addr %s", testAddrType, deviceAddr));
					addMessage(String.format("Data: %s", Util.asHexdump(report.getData())));

					addMessage("GAPLEScanStop()");
					btstack.GAPLEScanStop();
					addMessage("GAPLEConnect(...)");
					state = STATE.w4_connected;
					
					btstack.GAPLEConnect(testAddrType, deviceAddr);
				}
			}
			break;
		case w4_connected:
			if (packet instanceof HCIEventLEConnectionComplete){
				HCIEventLEConnectionComplete event = (HCIEventLEConnectionComplete) packet;
				
				connectionHandle = event.getConnectionHandle();
				addMessage(String.format("Connection complete, status %d, handle %x", event.getStatus(), connectionHandle));
				state = STATE.w4_services_complete;
				addMessage("GATTDiscoverPrimaryServices(...)");
				btstack.GATTDiscoverPrimaryServices(connectionHandle);
				
			}
			break;
		case w4_services_complete:
			if (packet instanceof GATTServiceQueryResult){
				GATTServiceQueryResult event = (GATTServiceQueryResult) packet;
				if (testService == null){
					addMessage(String.format("First service UUID %s", event.getService().getUUID()));
					testService = event.getService();
				}
				Log.d(BTSTACK_TAG, "Service: " + event.getService());
				service_count++;
			}
			if (packet instanceof GATTQueryComplete){
				addMessage(String.format("Service query complete, total %d services", service_count));
				state = STATE.w4_disconnect;
				test_run_count++;
				btstack.GAPDisconnect(connectionHandle);
			}
			break;
		case w4_disconnect:
			Log.d(BTSTACK_TAG, packet.toString());
			if (packet instanceof HCIEventDisconnectionComplete){
				clearMessages();
				addMessage("BTstack dissconnect.");
				btstack.disconnect();
				try {
					Thread.sleep(15000);
				} catch (InterruptedException e) {
					e.printStackTrace();
				}
				
				btstack.connect();
				addMessage("Power on BTstack.");
				state = STATE.w4_btstack_working;
				test_run_count = 0;
				btstack.BTstackSetPowerMode(1);
				
				
				/*if (test_run_count%10 == 0){
					addMessage("Power off BTstack.");
					state = STATE.w4_btstack_working;
					btstack.BTstackSetPowerMode(0);
					try {
						Thread.sleep(15000);
					} catch (InterruptedException e) {
						e.printStackTrace();
					}
					addMessage("Power on BTstack.");
					btstack.BTstackSetPowerMode(1);
				} else {
					addMessage("GAPLEScanStart()");
					state = STATE.w4_scan_result;
					btstack.GAPLEScanStart();
				}*/
			}
			break;
		default:
			break;
		}
	}

	public void trackRSSI(Packet packet){
		switch (state){
		case w4_btstack_working:
			if (packet instanceof BTstackEventState){
				BTstackEventState event = (BTstackEventState) packet;
				if (event.getState() == 2)	{
					addMessage("GAPLEScanStart()");
					state = STATE.w4_scan_result;
					btstack.GAPLEScanStart();
				}
			}
			break;
		case w4_scan_result:
			if (packet instanceof GAPLEAdvertisingReport){
				clearMessages();
				GAPLEAdvertisingReport report = (GAPLEAdvertisingReport) packet;
				testAddrType = report.getAddressType();
				deviceAddr = report.getAddress();
				addMessage(String.format("Adv: type %d, addr %s", testAddrType, deviceAddr));
				addMessage(String.format("Data: %s", Util.asHexdump(report.getData())));
				addMessage("GAPLEScanStop()");
				btstack.GAPLEScanStop();
				addMessage("GAPLEConnect(...)");
				state = STATE.w4_connected;
				btstack.GAPLEConnect(testAddrType, deviceAddr);
			}
			break;
		case w4_connected:
			if (packet instanceof HCIEventLEConnectionComplete){
				HCIEventLEConnectionComplete event = (HCIEventLEConnectionComplete) packet;
				if (event.getStatus() != 0) {
					addMessage(String.format("Connection didn't succeed, status %d, restart scanning.", event.getStatus()));
					state = STATE.w4_scan_result;
					btstack.GAPLEScanStart();
					break;
				}
				
				state = STATE.track_rssi;
				connectionHandle = event.getConnectionHandle();
				addMessage(String.format("Connection complete, status %d, handle %x. Start RSSI query.", event.getStatus(), connectionHandle));
				counter = 0;
				new Thread(new Runnable(){
					@Override
					public void run() {
						try {
							while(state == STATE.track_rssi){
								Thread.sleep(1000);
								btstack.HCIReadRssi(connectionHandle);
							}
						} catch (InterruptedException e) {}
					}
				}).start();
			}
			break;
		case track_rssi:
			if (packet instanceof HCIEventDisconnectionComplete){
				clearMessages();
				addMessage(String.format("Received dissconnect. Start scannning."));
				state = STATE.w4_scan_result;
				btstack.GAPLEScanStart();
				break;
			}
			
			if (!(packet instanceof HCIEventCommandComplete)) break;
			HCIEventCommandComplete event = (HCIEventCommandComplete) packet;
			int opcode = event.getCommandOpcode();
			if (opcode != 0X1405) break;
			counter = counter + 1;
			byte data[] = event.getReturnParameters();
			addTempMessage(String.format("counter %d, status %d, RSSI: %ddBm", counter, Util.readByte(data, 0), data[3]));
			break;
		
		default:
			break;
		}
	}
	
	public void queryBatteryLevel(Packet packet){
		// Restart scanning on disconnect.
		if (packet instanceof HCIEventDisconnectionComplete){
			clearMessages();
			addMessage(String.format("Received dissconnect. Start scannning."));
			state = STATE.w4_scan_result;
			btstack.GAPLEScanStart();
			return;
		}
		
		switch (state){
		case w4_btstack_working:
			if (packet instanceof BTstackEventState){
				BTstackEventState event = (BTstackEventState) packet;
				if (event.getState() == 2)	{
					addMessage("BTstack working, start scanning.");
					state = STATE.w4_scan_result;
					btstack.GAPLEScanStart();
				}
			}
			break;
		case w4_scan_result:
			if (packet instanceof GAPLEAdvertisingReport){
				// Advertisement received. Connect to the found BT address.
				clearMessages();
				GAPLEAdvertisingReport report = (GAPLEAdvertisingReport) packet;
				testAddrType = report.getAddressType();
				deviceAddr = report.getAddress();
				addMessage(String.format("Adv: type %d, addr %s\ndata: %s \n Stop scan, initiate connect.", testAddrType, deviceAddr, Util.asHexdump(report.getData())));
				btstack.GAPLEScanStop();
				state = STATE.w4_connected;
				btstack.GAPLEConnect(testAddrType, deviceAddr);
			}
			break;
		case w4_connected:
			if (packet instanceof HCIEventLEConnectionComplete){
				HCIEventLEConnectionComplete event = (HCIEventLEConnectionComplete) packet;
				if (event.getStatus() != 0) {
					addMessage(String.format("Connection didn't succeed, status %d, restart scanning.", event.getStatus()));
					state = STATE.w4_scan_result;
					btstack.GAPLEScanStart();
					break;
				}
				
				// Query battery service.
				state = STATE.w4_services_complete;
				connectionHandle = event.getConnectionHandle();
				addMessage(String.format("Connection complete, status %d, handle %x.\nQuery battery service.", event.getStatus(), connectionHandle));
				
				//btstack.GATTDiscoverPrimaryServicesByUUID128(connectionHandle, uuid128(this.battery_service_uuid));
				btstack.GATTDiscoverPrimaryServicesByUUID16(connectionHandle, 0x180f);
			}
			break;
			
		case w4_services_complete:
			if (packet instanceof GATTServiceQueryResult){
				// Store battery service. Wait for GATTQueryComplete event to send next GATT command.
				GATTServiceQueryResult event = (GATTServiceQueryResult) packet;
				addMessage(String.format("Battery service found %s", event.getService().getUUID()));
				batteryService = event.getService();
				break;
			}
			if (packet instanceof GATTQueryComplete){
				// Check if battery service is found.
				if (batteryService == null) {
					addMessage("No battery service found, restart scanning.");
					state = STATE.w4_scan_result;
					btstack.GAPLEScanStart(); 
					break;
				}
				addMessage("Battery service is found. Query battery level.");
				state = STATE.w4_characteristic_complete;
				btstack.GATTDiscoverCharacteristicsForServiceByUUID128(connectionHandle, batteryService, uuid128(this.battery_level_chr_uuid));	
			}
			break;
		case w4_characteristic_complete:
			if (packet instanceof GATTCharacteristicQueryResult){
				// Store battery level characteristic. Wait for GATTQueryComplete event to send next GATT command.
				GATTCharacteristicQueryResult event = (GATTCharacteristicQueryResult) packet;
				batteryLevelCharacteristic = event.getCharacteristic();
				addMessage("Battery level characteristic found.");
				break;
			}
			
			if (!(packet instanceof GATTQueryComplete)) break;
			if (batteryLevelCharacteristic == null) {
				addMessage("No battery level characteristic found");
				break;
			}
			clearMessages();
			addMessage("Polling battery.");
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
			clearMessages();
			if (packet instanceof GATTCharacteristicValueQueryResult){
				GATTCharacteristicValueQueryResult battery = (GATTCharacteristicValueQueryResult) packet;
				
				if (battery.getValueLength() < 1) break;
				this.batteryLevel = battery.getValue()[0];
				addTempMessage(String.format("Counter %d, battery level: %d", counter, batteryLevel) + "%");
				counter = counter + 1;
				break;
				
			}
			if (packet instanceof GATTQueryComplete){
				GATTQueryComplete event = (GATTQueryComplete) packet;
				addMessage(String.format("Counter %d, battery level: %d", counter, batteryLevel) + "%");
				if (event.getStatus() != 0){
					addMessage("Battery data could not be read - status 0x%02x. Restart scanning." + String.valueOf(event.getStatus()));
					state = STATE.w4_scan_result;
					btstack.GAPLEScanStart(); 
					break;
				}
			}
			
			break;
		default:
			break;
		}
	}

	private BT_UUID uuid128(byte[] att_uuid) {
		byte [] uuid = new byte[16];
		Util.flipX(att_uuid, uuid);	
		return new BT_UUID(uuid);
	}
	
	public void handlePacket(Packet packet){
		// queryBatteryLevel(packet);
		// trackRSSI(packet);
		// testCharacteristics(packet);
		// testAccelerometer(packet);
		testConnectDisconnect(packet);
	}

	void test(){
		counter = 0;
		addMessage("LE Test Application");

		btstack = new BTstack();
		btstack.registerPacketHandler(this);

		boolean ok = btstack.connect();
		if (!ok) {
			addMessage("Failed to connect to BTstack Server");
			return;
		}

		addMessage("BTstackSetPowerMode(1)");

		state = STATE.w4_btstack_working;
		btstack.BTstackSetPowerMode(1);
	}
}
