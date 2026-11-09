package com.bluekitchen.sppclient;

import java.io.UnsupportedEncodingException;
import java.util.ArrayList;
import java.util.List;

import android.annotation.SuppressLint;
import android.app.Activity;
import android.os.Bundle;
import android.util.Log;
import android.view.Menu;
import android.widget.TextView;

import com.bluekitchen.btstack.RFCOMMDataPacket;
import com.bluekitchen.btstack.BD_ADDR;
import com.bluekitchen.btstack.BTstack;
import com.bluekitchen.btstack.Packet;
import com.bluekitchen.btstack.PacketHandler;
import com.bluekitchen.btstack.Util;
import com.bluekitchen.btstack.event.BTstackEventState;
import com.bluekitchen.btstack.event.HCIEventCommandComplete;
import com.bluekitchen.btstack.event.HCIEventDisconnectionComplete;
import com.bluekitchen.btstack.event.HCIEventHardwareError;
import com.bluekitchen.btstack.event.HCIEventInquiryComplete;
import com.bluekitchen.btstack.event.HCIEventInquiryResultWithRssi;
import com.bluekitchen.btstack.event.HCIEventRemoteNameRequestComplete;
import com.bluekitchen.btstack.event.RFCOMMEventChannelOpened;
import com.bluekitchen.btstack.event.SDPEventQueryComplete;
import com.bluekitchen.btstack.event.SDPEventQueryRFCOMMService;


public class MainActivity extends Activity implements PacketHandler {
	
	// minimal Android text UI

	private static final String BTSTACK_TAG = "BTstack";

	private TextView tv;
	private String onScreenMessage = "";

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
	

	// helper class to store inquiry results
	private static class RemoteDevice{
		private enum NAME_STATE {
			REMOTE_NAME_REQUEST, REMOTE_NAME_INQUIRED, REMOTE_NAME_FETCHED
		};
		
		private BD_ADDR bdAddr;
		private int pageScanRepetitionMode;
		private int clockOffset;
		private String name;
		private NAME_STATE state;
		
		public RemoteDevice(BD_ADDR bdAddr, int pageScanRepetitionMode, int clockOffset) {
			this.bdAddr = bdAddr;
			this.state = NAME_STATE.REMOTE_NAME_REQUEST;
		}

		public boolean nameRequest() {
			return this.state == NAME_STATE.REMOTE_NAME_REQUEST;
		}

		public void inquireName(BTstack btstack) {
			this.state = NAME_STATE.REMOTE_NAME_INQUIRED;
			btstack.HCIRemoteNameRequest(bdAddr, pageScanRepetitionMode, 0, clockOffset);
		}

		public BD_ADDR getBDAddress() {
			return this.bdAddr;
		}

		private String getName() {
			return name;
		}

		private void setName(String name) {
			this.state = NAME_STATE.REMOTE_NAME_FETCHED;
			this.name = name;
		}
	}

	// constants
	
	private static final int HCI_INQUIRY_LAP = 0x9E8B33;  // 0x9E8B33: General/Unlimited Inquiry Access Code (GIAC)
	private static final int INQUIRY_INTERVAL = 5;
	private static final int SPP_UUID = 0x1002;
	
	private enum STATE {
		w4_btstack_working, w4_write_inquiry_mode, w4_scan_result, w4_remote_name, w4_sdp_query_result, w4_connected, active
	};

	private static final String REMOTE_DEVICE_NAME_PREFIX = "BTstack SPP";
	private static final String RFCOMM_SERVICE_PREFIX = "SPP";
	
	// state 

	private BTstack btstack;
	private STATE state;
	private int testHandle;
	
	private int rfcommChannelID = 0;
	private int mtu = 0;
	private BD_ADDR remoteBDAddr;
	
	List<SDPEventQueryRFCOMMService> services = new ArrayList<SDPEventQueryRFCOMMService>(10);
	List<RemoteDevice> devices = new ArrayList<RemoteDevice>(10);

	private int counter;


	private boolean hasMoreRemoteNameRequests() {
		for (RemoteDevice device:devices){
			if (device.nameRequest()){
				return true;
			}
		}
		return false;
	}

	private void doNextRemoteNameRequest() {
		for (RemoteDevice device:devices){
			if (device.nameRequest()){
				addMessage("Get remote name of " + device.getBDAddress());
	            device.inquireName(btstack);
	            return;
			}
		}
	}

	private void restartInquiry(){
		clearMessages();
		addMessage("Restart Inquiry");
		state = STATE.w4_scan_result;
		services.clear();
		devices.clear();
		counter = 0;
		btstack.HCIInquiry(HCI_INQUIRY_LAP, INQUIRY_INTERVAL, 0);
	}
	
	@SuppressLint("DefaultLocale")
	public void handlePacket(Packet packet){
		if (packet instanceof HCIEventHardwareError){
			clearMessages();
			addMessage("Received HCIEventHardwareError, \nhandle power cycle of the Bluetooth \nchip of the device.");
		}
		
		if (packet instanceof HCIEventDisconnectionComplete){
			HCIEventDisconnectionComplete event = (HCIEventDisconnectionComplete) packet;
			testHandle = event.getConnectionHandle();
			addMessage(String.format("Received disconnect, status %d, handle %x", event.getStatus(), testHandle));
			restartInquiry();
			return;
		}
		
		switch (state){
		case w4_btstack_working:
			if (packet instanceof BTstackEventState){
				BTstackEventState event = (BTstackEventState) packet;
				if (event.getState() == 2)	{
					addMessage("BTstack working. Set write inquiry mode.");
					state = STATE.w4_write_inquiry_mode;
					btstack.HCIWriteInquiryMode(1); // with RSSI
				}
			}
			break;

		case w4_write_inquiry_mode:
			if (packet instanceof HCIEventCommandComplete){
				state = STATE.w4_scan_result;
				btstack.HCIInquiry(HCI_INQUIRY_LAP, INQUIRY_INTERVAL, 0);
			}
			break;
		
		case w4_scan_result:
			if (packet instanceof HCIEventInquiryResultWithRssi){
				HCIEventInquiryResultWithRssi result = (HCIEventInquiryResultWithRssi) packet;
				devices.add(new RemoteDevice(result.getBdAddr(), result.getPageScanRepetitionMode(), result.getClockOffset()));
				addMessage("Found device: " + result.getBdAddr());
			}
			
			if (packet instanceof HCIEventInquiryComplete){
				state = STATE.w4_remote_name;
				if (hasMoreRemoteNameRequests()){
			        doNextRemoteNameRequest();
			        break;
			    }
			}
			break;
		
		case w4_remote_name:
			if (packet instanceof HCIEventRemoteNameRequestComplete){
				HCIEventRemoteNameRequestComplete result = (HCIEventRemoteNameRequestComplete) packet;
				if (result.getStatus() == 0){
					// store name on success
					setNameForDeviceWithBDAddr(result.getRemoteName(), result.getBdAddr());
				}
				
				if (hasMoreRemoteNameRequests()){
			        doNextRemoteNameRequest();
			        break;
			    }
				
				// discovery done, connect to device with remote name prefix
				
				RemoteDevice remoteDevice = null;
				for (RemoteDevice device : devices){
					if (device.getName() != null && device.getName().startsWith(REMOTE_DEVICE_NAME_PREFIX)){
						remoteDevice = device;
					}
				}
				
				// try first one otherwise
				if (remoteDevice == null && devices.size() > 0){
					remoteDevice = devices.get(0);
				}

				// no device, restart inquiry
				if (remoteDevice == null){
					restartInquiry();
					break;
				}

				// start SDP query for RFCOMMM services
				remoteBDAddr = remoteDevice.getBDAddress();
				if (remoteDevice.getName() == null){
					addMessage("Start SDP Query of " + remoteDevice.getBDAddress());
				} else {
					addMessage("Start SDP Query of " + remoteDevice.getName());
				}
				state = STATE.w4_sdp_query_result;
				byte[] serviceSearchPattern = Util.serviceSearchPatternForUUID16(SPP_UUID);
				btstack.SDPClientQueryRFCOMMServices(remoteBDAddr, serviceSearchPattern);	
				break;
			}
			break;
			
		case w4_sdp_query_result:
			if (packet instanceof SDPEventQueryRFCOMMService){
				SDPEventQueryRFCOMMService service = (SDPEventQueryRFCOMMService) packet;
				services.add(service);
				addMessage(String.format("Found \"%s\", channel %d", service.getName(), service.getRFCOMMChannel()));
			}
			if (packet instanceof SDPEventQueryComplete){
				// find service with "SPP" prefix
				SDPEventQueryRFCOMMService selectedService = null;
				for  (SDPEventQueryRFCOMMService service : services){
					if (service.getName().startsWith(RFCOMM_SERVICE_PREFIX)){
						selectedService = service;
						break;
					}
				}
				// restart demo, if no service with prefix found
				if (selectedService == null){
					restartInquiry();
					break;
				}
				
				// connect
				state = STATE.w4_connected;
				clearMessages();
				addMessage("SPP Test Application / Part 2");
				addMessage("Connect to channel nr " + selectedService.getRFCOMMChannel());
				btstack.RFCOMMCreateChannel(remoteBDAddr, selectedService.getRFCOMMChannel());
			}
			
			break;
			
		case w4_connected:
			if (packet instanceof RFCOMMEventChannelOpened){
				RFCOMMEventChannelOpened e = (RFCOMMEventChannelOpened) packet;
				if (e.getStatus() != 0) {
					addMessage("RFCOMM channel open failed, status " + e.getStatus());
				} else {
					state = STATE.active;
					rfcommChannelID = e.getRFCOMMCid();
					mtu = e.getMaxFrameSize();
					addMessage(String.format("RFCOMM channel open succeeded. \nNew RFCOMM Channel ID %d,\n max frame size %d", rfcommChannelID, mtu));
					
					counter = 0;
					new Thread(new Runnable(){
						@Override
						public void run() {
							try {
								while(state == STATE.active){
									Thread.sleep(1000);
									byte[] data;
									try {
										data = String.format("BTstack SPP Counter %d\n", counter).getBytes("utf8");
										if (counter < Integer.MAX_VALUE){
											counter++;
										} else {
											counter = 0;
										}
										btstack.RFCOMMSendData(rfcommChannelID, data);
									} catch (UnsupportedEncodingException e) {
										// TODO Auto-generated catch block
										e.printStackTrace();
									}
								}
							} catch (InterruptedException e) {}
						}
					}).start();
				}
			}
			break;
			
		case active:
			if (packet instanceof RFCOMMDataPacket){	
				addTempMessage("Received RFCOMM data packet: \n" + packet.toString());
			}
			break;
			
		default:
			break;
		}
	}

	private RemoteDevice deviceForBDAddr(BD_ADDR bdAddr){
		for (RemoteDevice device:devices){
			if (device.getBDAddress().equals(bdAddr) )
				return device;
		}
		return null;
	}
		
	private void setNameForDeviceWithBDAddr(String remoteName, BD_ADDR bdAddr) {
		RemoteDevice device = deviceForBDAddr(bdAddr);
		if (device != null){
			addMessage("Found " + remoteName);
			device.setName(remoteName);
		}
	}
	
	void test(){
		counter = 0;
		addMessage("SPP Test Application");

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

