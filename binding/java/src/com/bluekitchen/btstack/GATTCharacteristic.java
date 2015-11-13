package com.bluekitchen.btstack;

import java.util.Arrays;

public class GATTCharacteristic {

	/**
    uint16_t start_handle;
    uint16_t value_handle;
    uint16_t end_handle;
    uint16_t properties;
    uint8_t  uuid128[16];
    */
	
	public static final int LEN = 24;

	byte data[] = new byte[LEN];
	
	public byte[] getBytes(){
		return data;
	}
	
	public GATTCharacteristic(byte address[]){
		if (address.length != LEN) return;
		System.arraycopy(address, 0, this.data, 0, LEN);
	}
	
	public int getStartHandle(){
		return Util.readBt16(data, 0);
	}

	public int getValueHandle(){
		return Util.readBt16(data, 2);
	}

	public int getEndHandle(){
		return Util.readBt16(data, 4);
	}

	public int getProperties(){
		return Util.readBt16(data, 6);
	}
	
	public BT_UUID getUUID(){
		return new BT_UUID(Arrays.copyOfRange(data, 8, 8 + 16));
	}

	@Override
	public String toString() {
		return "GattCharacteristic [getStartHandle()=" + getStartHandle()
				+ ", getValueHandle()=" + getValueHandle()
				+ ", getEndHandle()=" + getEndHandle() + ", getProperties()="
				+ getProperties() + ", getUUID()=" + getUUID() + "]";
	}
}
