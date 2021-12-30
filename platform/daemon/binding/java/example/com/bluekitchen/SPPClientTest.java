package com.bluekitchen;

import java.util.ArrayList;
import java.util.List;

import com.bluekitchen.btstack.BD_ADDR;
import com.bluekitchen.btstack.BTstack;
import com.bluekitchen.btstack.Packet;
import com.bluekitchen.btstack.PacketHandler;
import com.bluekitchen.btstack.RFCOMMDataPacket;
import com.bluekitchen.btstack.Util;
import com.bluekitchen.btstack.event.BTstackEventState;
import com.bluekitchen.btstack.event.HCIEventDisconnectionComplete;
import com.bluekitchen.btstack.event.RFCOMMEventChannelOpened;
import com.bluekitchen.btstack.event.SDPEventQueryComplete;
import com.bluekitchen.btstack.event.SDPEventQueryRFCOMMService;

public class SPPClientTest implements PacketHandler {

	private enum STATE {
		w4_btstack_working, w4_query_result, w4_connected, active
	};

	private BTstack btstack;
	private STATE state;
	private int testHandle;
	
	private BD_ADDR remote = new BD_ADDR("00:1A:7D:DA:71:01");
	private int sppUUID = 0x1101;

	private int outgoing_channel_nr = -1;
	private int rfcommChannelID = 0;
	private int mtu = 0;
	 
	private int counter;

	private void startSDPQuery(){
        state = STATE.w4_query_result;
        byte[] serviceSearchPattern = Util.serviceSearchPatternForUUID16(sppUUID);
        btstack.SDPClientQueryRFCOMMServices(remote, serviceSearchPattern);
	}

	public void handlePacket(Packet packet){
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
					System.out.println("BTstack working. Start SDP inquiry.");
					startSDPQuery();
				}
			}
			break;

		case w4_query_result:
			if (packet instanceof SDPEventQueryRFCOMMService){
				SDPEventQueryRFCOMMService service = (SDPEventQueryRFCOMMService) packet;
                System.out.println("Found RFCOMM channel " + service.getName() + ", channel nr: " + service.getRFCOMMChannel());
                outgoing_channel_nr = service.getRFCOMMChannel();
			}
			if (packet instanceof SDPEventQueryComplete){
			    SDPEventQueryComplete complete = (SDPEventQueryComplete) packet;
			    if (complete.getStatus() != 0){
			        System.out.println(String.format("SDP Query failed with status 0x%02x, retry SDP query.", complete.getStatus()));
                    startSDPQuery();
                    break;
			    }
				if (outgoing_channel_nr >= 0){
                    state = STATE.w4_connected;
                    System.out.println("Connect to channel nr " + outgoing_channel_nr);
                    btstack.RFCOMMCreateChannel(remote, outgoing_channel_nr);
                }
			}
			break;
			
		case w4_connected:
			if (packet instanceof RFCOMMEventChannelOpened){
				RFCOMMEventChannelOpened e = (RFCOMMEventChannelOpened) packet;
				System.out.println("RFCOMMEventChannelOpened with status " + e.getStatus());
				if (e.getStatus() != 0) {
					System.out.println("RFCOMM channel open failed, status " + e.getStatus());
				} else {
					state = STATE.active;
					rfcommChannelID = e.getRFCOMMCid();
					mtu = e.getMaxFrameSize();
					System.out.println(String.format("RFCOMM channel open succeeded. New RFCOMM Channel ID %d, max frame size %d", rfcommChannelID, mtu));
					
					counter = 0;
					new Thread(new Runnable(){
						@Override
						public void run() {
							try {
								while(state == STATE.active){
									Thread.sleep(1000);
									byte [] data = String.format("BTstack SPP Client Test Counter %d\n", counter++).getBytes();
									btstack.RFCOMMSendData(rfcommChannelID, data);
								}
							} catch (InterruptedException e) {}
						}
					}).start();
				}
			}
			break;
		case active:
			if (packet instanceof RFCOMMDataPacket){	
				System.out.println("Received RFCOMM data packet: " + packet.toString());
			}
		default:
			break;
		}
	}
	
	void test(){
		System.out.println("SPP Test Application");

		// connect to BTstack Daemon via default port on localhost
		// start: src/BTdaemon --tcp
		
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
		new SPPClientTest().test();
	}
}
