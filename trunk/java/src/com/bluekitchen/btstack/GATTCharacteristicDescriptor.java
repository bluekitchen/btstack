package com.bluekitchen.btstack;

import java.util.Arrays;

public class GATTCharacteristicDescriptor {

	/**
    uint16_t handle;
    uint8_t  uuid128[16];
     */
	
	public static final int LEN = 18;

	byte data[] = new byte[LEN];
	
	public byte[] getBytes(){
		return data;
	}
	
	public GATTCharacteristicDescriptor(byte address[]){
		if (address.length != LEN) return;
		System.arraycopy(address, 0, this.data, 0, LEN);
	}
	
	public int getHandle(){
		return Util.readBt16(data, 0);
	}
	
	public BT_UUID getUUID(){
		return new BT_UUID(Arrays.copyOfRange(data, 2, 2 + 16));
	}

	@Override
	public String toString() {
		return "GattCharacteristicDescriptor [getHandle()=" + getHandle()
				+ ", getUUID()=" + getUUID() + "]";
	}
}
