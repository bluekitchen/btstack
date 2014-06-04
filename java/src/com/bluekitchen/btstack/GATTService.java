package com.bluekitchen.btstack;

import java.util.Arrays;

public class GATTService {
	
	/**
    uint16_t start_group_handle;
    uint16_t end_group_handle;
    uint8_t  uuid128[16];
	 */

	public static final int LEN = 20;

	byte data[] = new byte[LEN];
	
	public byte[] getBytes(){
		return data;
	}
	
	public GATTService(byte data[]){
		if (data.length != LEN) return;
		System.arraycopy(data, 0, this.data, 0, LEN);
	}
	
	public int getStartGroupHandle(){
		return Util.readBt16(data, 0);
	}

	public int getEndGroupHandle(){
		return Util.readBt16(data, 2);
	}

	public BT_UUID getUUID(){
		return new BT_UUID(Arrays.copyOfRange(data, 4, 4 + 16));
	}
	
	@Override
	public String toString() {
		return "GattService [getStartGroupHandle()=" + getStartGroupHandle()
				+ ", getEndGroupHandle()=" + getEndGroupHandle()
				+ ", getUUID()=" + getUUID() + "]";
	}
}
