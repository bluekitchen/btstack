package com.bluekitchen.btstack;

public class RFCOMMDataPacket extends Packet {

	public RFCOMMDataPacket(int channel, byte data[]){
		super(RFCOMM_DATA_PACKET, channel, data, data.length);
	}
	
	public RFCOMMDataPacket(Packet packet){
		super(RFCOMM_DATA_PACKET, packet.getChannel(), packet.getBuffer(), packet.getPayloadLen());
	}
}

