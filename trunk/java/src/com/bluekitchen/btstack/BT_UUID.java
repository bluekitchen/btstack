package com.bluekitchen.btstack;

import java.util.Arrays;

// UUID stored in big endian format
// TODO store UUID internally as LE

public class BT_UUID {
	
	private static final byte BluetoothBaseUUID[] = { 0x00, 0x00, 0x00, 0x00, /* - */ 0x00, 0x00, /* - */ 0x10, 0x00, /* - */
	    (byte)0x80, 0x00, /* - */ 0x00, (byte)0x80, 0x5F, (byte)0x9B, 0x34, (byte)0xFB };
	
	byte [] uuid = new byte[16];
	
	public BT_UUID(long uuid32){
		System.arraycopy(BluetoothBaseUUID, 0, uuid, 0, 16);
		Util.storeNet32(uuid, 0, uuid32);
	}

	public BT_UUID(byte[] uuid128LE){
		Util.flipX(uuid128LE, uuid);
	}
	
	public BT_UUID(String uuidString){
		String parts[] = uuidString.split("-");
		if (parts.length != 5) return;
		Util.storeNet32(uuid, 0, Long.parseLong(parts[0], 16));
		Util.storeNet32(uuid, 4, Long.parseLong(parts[1]+parts[2], 16));
		Util.storeNet32(uuid, 8, Long.parseLong((parts[3]+parts[4]).substring(0, 8), 16));
		Util.storeNet32(uuid, 12, Long.parseLong((parts[3]+parts[4]).substring(8, 16), 16));
	}

	public String toString(){
	    return String.format("%02x%02x%02x%02x-%02x%02x-%02x%02x-%02x%02x-%02x%02x%02x%02x%02x%02x",
	            uuid[0], uuid[1], uuid[2], uuid[3], uuid[4], uuid[5], uuid[6], uuid[7],
	            uuid[8], uuid[9], uuid[10], uuid[11], uuid[12], uuid[13], uuid[14], uuid[15]);
	}
		
	public long getUUID32(){
		for (int i = 4; i < 16 ; i++){
			if (uuid[i] != BluetoothBaseUUID[i]) return 0;
		}
		return Util.readNet32(uuid, 0);
	}
	
	public byte[] getBytes(){
		byte result[] = new byte[16];
		Util.flipX(uuid, result);
		return result;
	}
	
	@Override
	public int hashCode() {
		final int prime = 31;
		int result = 1;
		result = prime * result + Arrays.hashCode(uuid);
		return result;
	}
	
	@Override
	public boolean equals(Object obj) {
		if (this == obj) {
			return true;
		}
		if (obj == null) {
			return false;
		}
		if (!(obj instanceof BT_UUID)) {
			return false;
		}
		BT_UUID other = (BT_UUID) obj;
		if (!Arrays.equals(uuid, other.uuid)) {
			return false;
		}
		return true;
	}	
	
	public static void main (String arg[]){
		BT_UUID uuid = new BT_UUID(0x2800);
		System.out.println("0x2800 = " + uuid);
		System.out.println("32bit UUID = " + uuid.getUUID32());
		String uuidString = uuid.toString();
		BT_UUID uuid2 = new BT_UUID(uuidString);
		System.out.println("equal " + uuid.equals(uuid2));
	}
}
