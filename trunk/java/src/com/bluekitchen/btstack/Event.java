package com.bluekitchen.btstack;

public class Event extends Packet {
	
	public Event(byte data[], int payloadLen){
		super(HCI_EVENT_PACKET, 0, data, payloadLen);
	}
	
	public Event(Packet packet){
		super(HCI_EVENT_PACKET, 0, packet.getBuffer(), packet.getPayloadLen());
	}
	
	public final int getEventType(){
		return Util.readByte(data, 0);
	}
		
	public String toString(){
		StringBuffer t = new StringBuffer();
		t.append(String.format("Event type %d, len %d: ", getEventType(), getPayloadLen()));
		t.append(Util.asHexdump(data, payloadLen));
		return t.toString();
	}
}
