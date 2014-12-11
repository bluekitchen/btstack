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
import com.bluekitchen.btstack.event.RFCOMMEventOpenChannelComplete;
import com.bluekitchen.btstack.event.SDPQueryComplete;
import com.bluekitchen.btstack.event.SDPQueryRFCOMMService;

public class SPPClientTest implements PacketHandler {

	private enum STATE {
		w4_btstack_working, w4_query_result, w4_connected, active
	};

	private BTstack btstack;
	private STATE state;
	private int testHandle;
	
	private BD_ADDR remote = new BD_ADDR("84:38:35:65:D1:15");
	private int rfcommServiceUUID = 0x1002;
	private int btIncomingChannelNr = 3;
	
	private int rfcommChannelID = 0;
	private int mtu = 0;
	 
	List<Integer> services = new ArrayList<Integer>(10);
	private int counter;
	
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
					state = STATE.w4_query_result;
					byte[] serviceSearchPattern = Util.serviceSearchPatternForUUID16(rfcommServiceUUID);
					btstack.SDPClientQueryRFCOMMServices(remote, serviceSearchPattern);	
				}
			}
			
			break;

		case w4_query_result:
			if (packet instanceof SDPQueryRFCOMMService){
				SDPQueryRFCOMMService service = (SDPQueryRFCOMMService) packet;
				services.add(service.getRFCOMMChannel());
			}
			if (packet instanceof SDPQueryComplete){
				for  (Integer channel_nr : services){
					System.out.println("Found rfcomm channel nr: " + channel_nr);
					if (channel_nr == btIncomingChannelNr){
						state = STATE.w4_connected;
						System.out.println("Connect to channel nr " + channel_nr);
						btstack.RFCOMMCreateChannel(remote, 3);
					}
				}
				
			}
			break;
			
		case w4_connected:
			if (packet instanceof RFCOMMEventOpenChannelComplete){
				RFCOMMEventOpenChannelComplete e = (RFCOMMEventOpenChannelComplete) packet;
				System.out.println("RFCOMMEventOpenChannelComplete with status " + e.getStatus());
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
									byte [] data = String.format("BTstack SPP Counter %d\n", counter).getBytes();
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
