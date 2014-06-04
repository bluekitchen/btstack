package com.bluekitchen.btstack;

public class BD_ADDR {
	
	public static final int LEN = 6;

	byte address[] = new byte[LEN];
	
	public byte[] getBytes(){
		return address;
	}
	
	public BD_ADDR(String text){
		String[] parts = text.split(":");
		if (parts.length != LEN) return;
		for (int i = 0; i < LEN ; i++){
			Util.storeByte(address,	LEN - 1 - i, Integer.parseInt(parts[i], 16));
		}
	}
	
	public BD_ADDR(byte address[]){
		if (address.length != LEN) return;
		System.arraycopy(address, 0, this.address, 0, LEN);
	}
	
	public String toString(){
		StringBuffer buffer = new StringBuffer();
		for (int i = LEN - 1 ; i >= 0 ; i--){
			if (i != LEN - 1){
				buffer.append(":");
			}
			buffer.append(String.format("%02X", Util.readByte(address, i)));
		}
		return buffer.toString();
	}
	
	public static void main(String args[]){
		BD_ADDR addr = new BD_ADDR("11:22:33:44:55:66");
		Util.hexdump(addr.getBytes());
		System.out.println( addr.toString());
	}
}
