package com.bluekitchen.btstack;

public class L2CAPDataPacket extends Packet {

	public L2CAPDataPacket(int channel, byte data[]){
		super(L2CAP_DATA_PACKET, channel, data, data.length);
	}
	
	public L2CAPDataPacket(Packet packet){
		super(L2CAP_DATA_PACKET, packet.getChannel(), packet.getBuffer(), packet.getPayloadLen());
	}
}