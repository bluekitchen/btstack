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
import com.bluekitchen.btstack.event.BTstackEventDaemonDisconnect;
import com.bluekitchen.btstack.event.BTstackEventState;
import com.bluekitchen.btstack.event.GAPEventAdvertisingReport;
import com.bluekitchen.btstack.event.GATTEventCharacteristicQueryResult;
import com.bluekitchen.btstack.event.GATTEventCharacteristicValueQueryResult;
import com.bluekitchen.btstack.event.GATTEventNotification;
import com.bluekitchen.btstack.event.GATTEventQueryComplete;
import com.bluekitchen.btstack.event.GATTEventServiceQueryResult;
import com.bluekitchen.btstack.event.HCIEventDisconnectionComplete;
import com.bluekitchen.btstack.event.HCIEventHardwareError;
import com.bluekitchen.btstack.event.HCIEventLEConnectionComplete;

public class MainActivity extends Activity implements PacketHandler {

	private static final String BTSTACK_TAG = "BTstack";

	private enum STATE {
		w4_btstack_working, w4_scan_result, w4_connected, w4_services_complete, w4_characteristic_complete, w4_characteristic_read
		, w4_characteristic_write, w4_acc_service_result, w4_acc_enable_characteristic_result, w4_write_acc_enable_result, w4_acc_client_config_characteristic_result, w4_acc_client_config_result,
		w4_acc_data, w4_connected_acc, w4_disconnect
	};

	private TextView tv;
	private BTstack btstack;
	private STATE state;
	private int testAddrType;
	private BD_ADDR testAddr;
	private int testHandle;
	private GATTService testService;
	private GATTCharacteristic testCharacteristic;
	private int service_count = 0;
	private int characteristic_count = 0;
	private int test_run_count = 0;
	
	private byte[] acc_service_uuid =           new byte[] {(byte)0xf0, 0, (byte)0xaa, (byte)0x10, 4, (byte)0x51, (byte)0x40, 0, (byte)0xb0, 0, 0, 0, 0, 0, 0, 0};
	private byte[] acc_chr_client_config_uuid = new byte[] {(byte)0xf0, 0, (byte)0xaa, (byte)0x11, 4, (byte)0x51, (byte)0x40, 0, (byte)0xb0, 0, 0, 0, 0, 0, 0, 0};
	private byte[] acc_chr_enable_uuid =        new byte[] {(byte)0xf0, 0, (byte)0xaa, (byte)0x12, 4, (byte)0x51, (byte)0x40, 0, (byte)0xb0, 0, 0, 0, 0, 0, 0, 0};
	private byte[] acc_enable = new byte[] {1};
	private byte acc_notification = 1; 
	private GATTService accService;
	private GATTCharacteristic enableCharacteristic;
	private GATTCharacteristic configCharacteristic;


	@Override
	protected void onCreate(Bundle savedInstanceState) {
		super.onCreate(savedInstanceState);
		setContentView(R.layout.activity_main);

		tv = new TextView(this);
		setContentView(tv);

		test();
	}

	void addMessage(final String message){
		Log.d(BTSTACK_TAG, message);
		runOnUiThread(new Runnable(){
			public void run(){
				tv.append("\n");
				tv.append(message);
			}
		});
	}
	void clearMessages(){
		runOnUiThread(new Runnable(){
			public void run(){
				tv.setText("");
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
			if (packet instanceof GAPEventAdvertisingReport){
				GAPEventAdvertisingReport report = (GAPEventAdvertisingReport) packet;
				testAddrType = report.getAddressType();
				testAddr = report.getAddress();
				addMessage(String.format("Adv: type %d, addr %s", testAddrType, testAddr));
				addMessage(String.format("Data: %s", Util.asHexdump(report.getData())));
				addMessage("GAPLEScanStop()");
				btstack.GAPLEScanStop();
				addMessage("GAPLEConnect(...)");
				state = STATE.w4_connected;
				btstack.GAPLEConnect(testAddrType, testAddr);
			}
			break;
		case w4_connected:
			if (packet instanceof HCIEventLEConnectionComplete){
				HCIEventLEConnectionComplete event = (HCIEventLEConnectionComplete) packet;
				testHandle = event.getConnectionHandle();
				addMessage(String.format("Connection complete, status %d, handle %x", event.getStatus(), testHandle));
				state = STATE.w4_services_complete;
				addMessage("GATTDiscoverPrimaryServices(...)");
				btstack.GATTDiscoverPrimaryServices(testHandle);
			}
			break;
		case w4_services_complete:
			if (packet instanceof GATTEventServiceQueryResult){
				GATTEventServiceQueryResult event = (GATTEventServiceQueryResult) packet;
				if (testService == null){
					addMessage(String.format("First service UUID %s", event.getService().getUUID()));
					testService = event.getService();
				}
				Log.d(BTSTACK_TAG, "Service: " + event.getService());
				service_count++;
			}
			if (packet instanceof GATTEventQueryComplete){
				addMessage(String.format("Service query complete, total %d services", service_count));
				state = STATE.w4_characteristic_complete;
				btstack.GATTDiscoverCharacteristicsForService(testHandle, testService);
			}
			break;

		case w4_characteristic_complete:
			if (packet instanceof GATTEventCharacteristicQueryResult){
				GATTEventCharacteristicQueryResult event = (GATTEventCharacteristicQueryResult) packet;
				if (testCharacteristic == null){
					addMessage(String.format("First characteristic UUID %s", event.getCharacteristic().getUUID()));
					testCharacteristic = event.getCharacteristic();
				}
				Log.d(BTSTACK_TAG, "Characteristic: " + event.getCharacteristic());
				characteristic_count++;
			}
			if (packet instanceof GATTEventQueryComplete){
				addMessage(String.format("Characteristic query complete, total %d characteristics", characteristic_count));
				state = STATE.w4_characteristic_read;
				btstack.GATTReadValueOfCharacteristic(testHandle, testCharacteristic);
			}			
			break;

		case w4_characteristic_read:
			if (packet instanceof GATTEventCharacteristicValueQueryResult){
				addMessage("Read complete");
				Log.d(BTSTACK_TAG, packet.toString());
				state = STATE.w4_characteristic_write;
				byte [] data = { 'B', 'T', 's', 't', 'a', 'c', 'k'};
				btstack.GATTWriteValueOfCharacteristic(testHandle, testCharacteristic, data.length, data);
			}
			break;
		case w4_characteristic_write:
			if (packet instanceof GATTEventQueryComplete){
				addMessage("Write complete, search for ACC service");
				state = STATE.w4_btstack_working;
				btstack.GAPDisconnect(testHandle);
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
			if (packet instanceof GAPEventAdvertisingReport){
				GAPEventAdvertisingReport report = (GAPEventAdvertisingReport) packet;
				testAddrType = report.getAddressType();
				testAddr = report.getAddress();
				addMessage(String.format("Adv: type %d, addr %s", testAddrType, testAddr));
				addMessage(String.format("Data: %s", Util.asHexdump(report.getData())));
				addMessage("GAPLEScanStop()");
				btstack.GAPLEScanStop();
				addMessage("GAPLEConnect(...)");
				state = STATE.w4_connected_acc;
				btstack.GAPLEConnect(testAddrType, testAddr);
			}
			break;



		case w4_connected_acc:
			if (packet instanceof HCIEventLEConnectionComplete){
				HCIEventLEConnectionComplete event = (HCIEventLEConnectionComplete) packet;
				testHandle = event.getConnectionHandle();
				addMessage(String.format("Connection complete, status %d, handle %x", event.getStatus(), testHandle));
				addMessage("Search for ACC service");
				state = STATE.w4_acc_service_result;
				byte [] uuid = new byte[16];
				Util.flipX(this.acc_service_uuid, uuid);	// works
				btstack.GATTDiscoverPrimaryServicesByUUID128(testHandle, new BT_UUID(uuid));
			}
			break;

		case w4_acc_service_result:
			addMessage(String.format("w4_acc_service_result state"));
			if (packet instanceof GATTEventServiceQueryResult){
				GATTEventServiceQueryResult event = (GATTEventServiceQueryResult) packet;
				addMessage(String.format("ACC service found %s", event.getService().getUUID()));
				accService = event.getService();
				break;
			}
			if (packet instanceof GATTEventQueryComplete){
				if (accService == null) {
					addMessage("No acc service found");
					break;
				}
				addMessage("ACC Service found, searching for acc enable characteristic");
				state = STATE.w4_acc_enable_characteristic_result;
				byte [] uuid = new byte[16];
				Util.flipX(this.acc_chr_enable_uuid, uuid);
				btstack.GATTDiscoverCharacteristicsForServiceByUUID128(testHandle, accService, new BT_UUID(uuid));
			}
			break;

		case w4_acc_enable_characteristic_result:
			if (packet instanceof GATTEventCharacteristicQueryResult){
				GATTEventCharacteristicQueryResult event = (GATTEventCharacteristicQueryResult) packet;
				enableCharacteristic = event.getCharacteristic();
				addMessage("Enable ACC Characteristic found ");
			}
			if (packet instanceof GATTEventQueryComplete){
				if (enableCharacteristic == null) {
					addMessage("No acc enable chr found");
					break;
				}
				addMessage("Write enable acc characteristic");
				state = STATE.w4_write_acc_enable_result;
				btstack.GATTWriteValueOfCharacteristic(testHandle, enableCharacteristic, 1, this.acc_enable);
			}	
			break;
		case w4_write_acc_enable_result:
			if (packet instanceof GATTEventQueryComplete){
				addMessage("Acc enabled,searching for acc client config characteristic");
				byte [] uuid = new byte[16];
				Util.flipX(this.acc_chr_client_config_uuid, uuid);
				btstack.GATTDiscoverCharacteristicsForServiceByUUID128(testHandle, accService, new BT_UUID(uuid));
				state = STATE.w4_acc_client_config_characteristic_result;
			}
			break;

		case w4_acc_client_config_characteristic_result:
			if (packet instanceof GATTEventCharacteristicQueryResult){
				GATTEventCharacteristicQueryResult event = (GATTEventCharacteristicQueryResult) packet;
				configCharacteristic = event.getCharacteristic();
				addMessage("ACC Client Config Characteristic found");
			}
			if (packet instanceof GATTEventQueryComplete){
				if (configCharacteristic == null) {
					addMessage("No acc config chr found");
					break;
				}
				addMessage("Write ACC Client Config Characteristic");
				state = STATE.w4_acc_data;
				btstack.GATTWriteClientCharacteristicConfiguration(testHandle, configCharacteristic, this.acc_notification);
			}	
			break;

		case w4_acc_data:
			if (packet instanceof GATTEventQueryComplete){
				addMessage("Acc configured for notification");
				break;
			}

			if (packet instanceof GATTEventNotification){
				addMessage("Acc Value");
				Log.d(BTSTACK_TAG, packet.toString());
				state = STATE.w4_btstack_working;
				btstack.GAPDisconnect(testHandle);
			}

		default:
			break;
		}
	}

	public void testConnectDisconnect(Packet packet){
		
		if (packet instanceof BTstackEventDaemonDisconnect){
			addMessage("Daemon disconnected, restarting connection");
			
			// wait a bit for BTstack to restart
			try {
				Thread.sleep(500);
			} catch (InterruptedException e) {
			}
			
			// start all over
			test();
			return;
		}
		
		if (packet instanceof HCIEventHardwareError){
			addMessage("Bluetooth module crashed, restarting app and waiting for working");
			state = STATE.w4_btstack_working;
			return;
		}
		
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
					addMessage("GAPLEScanStart()");
					state = STATE.w4_scan_result;
					btstack.GAPLEScanStart();
				}
			}
			break;
		case w4_scan_result:
			if (packet instanceof GAPEventAdvertisingReport){
				
				BD_ADDR sensor_tag_addr = new BD_ADDR("1C:BA:8C:20:C7:F6");
				BD_ADDR pts_dongle = new BD_ADDR("00:1B:DC:07:32:EF");
				BD_ADDR asus_dongle = new BD_ADDR("5c:f3:70:60:7b:87");

				GAPEventAdvertisingReport report = (GAPEventAdvertisingReport) packet;
				BD_ADDR reportAddr = report.getAddress();
				if (reportAddr.toString().equalsIgnoreCase(asus_dongle.toString())){
					testAddrType = report.getAddressType();
					testAddr = report.getAddress();
					addMessage(String.format("Adv: type %d, addr %s", testAddrType, testAddr));
					addMessage(String.format("Data: %s", Util.asHexdump(report.getData())));

					addMessage("GAPLEScanStop()");
					btstack.GAPLEScanStop();
					addMessage("GAPLEConnect(...)");
					state = STATE.w4_connected;
					
					btstack.GAPLEConnect(testAddrType, testAddr);
				}
			}
			break;
		case w4_connected:
			if (packet instanceof HCIEventLEConnectionComplete){
				HCIEventLEConnectionComplete event = (HCIEventLEConnectionComplete) packet;
				
				testHandle = event.getConnectionHandle();
				addMessage(String.format("Connection complete, status %d, handle %x", event.getStatus(), testHandle));
				state = STATE.w4_services_complete;
				addMessage("GATTDiscoverPrimaryServices(...)");
				btstack.GATTDiscoverPrimaryServices(testHandle);
				
			}
			break;
		case w4_services_complete:
			if (packet instanceof GATTEventServiceQueryResult){
				GATTEventServiceQueryResult event = (GATTEventServiceQueryResult) packet;
				if (testService == null){
					addMessage(String.format("First service UUID %s", event.getService().getUUID()));
					testService = event.getService();
				}
				Log.d(BTSTACK_TAG, "Service: " + event.getService());
				service_count++;
			}
			if (packet instanceof GATTEventQueryComplete){
				addMessage(String.format("Service query complete, total %d services", service_count));
				state = STATE.w4_disconnect;
				test_run_count++;
				btstack.GAPDisconnect(testHandle);
			}
			break;
		case w4_disconnect:
			Log.d(BTSTACK_TAG, packet.toString());
			if (packet instanceof HCIEventDisconnectionComplete){
				clearMessages();
				addMessage(String.format("Test run number %d started.", test_run_count));
				if (test_run_count%10 == 0){
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
				}
			}
			break;
		default:
			break;
		}
	}

	
	public void handlePacket(Packet packet){
		// testCharacteristics(packet);
		// testAccelerometer(packet);
		testConnectDisconnect(packet);
	}

	
	void test(){

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
