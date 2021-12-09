package com.bluekitchen;

import com.bluekitchen.btstack.BD_ADDR;
import com.bluekitchen.btstack.BTstack;
import com.bluekitchen.btstack.Packet;
import com.bluekitchen.btstack.PacketHandler;
import com.bluekitchen.btstack.Util;
import com.bluekitchen.btstack.event.BTstackEventState;
import com.bluekitchen.btstack.event.GAPEventInquiryResult;
import com.bluekitchen.btstack.event.GAPEventInquiryComplete;

import java.nio.charset.StandardCharsets;

public class GAPInquiry implements PacketHandler {

	private enum STATE {
		w4_btstack_working, w4_query_result,
	};

	private BTstack btstack;
	private STATE state;

	private void startInquiry(){
        state = STATE.w4_query_result;
        int duration_in_1_28s = 5;
        btstack.GAPInquiryStart(duration_in_1_28s);
	}

	public void handlePacket(Packet packet){
		switch (state){
		case w4_btstack_working:
			if (packet instanceof BTstackEventState){
				BTstackEventState event = (BTstackEventState) packet;
				if (event.getState() == 2)	{
					System.out.println("BTstack working. Start Inquiry");
					startInquiry();
				}
			}
			break;

		case w4_query_result:
			if (packet instanceof GAPEventInquiryResult){
				GAPEventInquiryResult result = (GAPEventInquiryResult) packet;
				String name = "";
				if (result.getNameAvailable() != 0){
				    name = Util.getText(result.getName(), 0, result.getNameLen());
    			    System.out.println( "Device found " + result.getBdAddr() + ", name: '" + name + "'");
				} else {
    			    System.out.println( "Device found " + result.getBdAddr());
                }
			}
			if (packet instanceof GAPEventInquiryComplete){
                // TODO: for devices without name, do a gap_remote_name_request
                System.out.println("Inquiry done, restart");
                startInquiry();
			}
			break;
		default:
			break;
		}
	}
	
	void test(){
		System.out.println("Inquiry Test Application");

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
		new GAPInquiry().test();
	}
}
