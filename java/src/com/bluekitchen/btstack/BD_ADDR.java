package com.bluekitchen.btstack;

import java.util.Arrays;

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

	@Override
	public int hashCode() {
	  return Arrays.hashCode(address);
	}

	@Override
	public boolean equals(Object obj) {
		if (this == obj) return true;
		if (obj == null) return false;
		if (getClass() != obj.getClass()) return false;
		BD_ADDR other = (BD_ADDR) obj;
		return Arrays.equals(address, other.address);
	}

	public static void main(String args[]){
		BD_ADDR a = new BD_ADDR("11:22:33:44:55:66");
		BD_ADDR b = new BD_ADDR("11:22:33:44:55:66");
		// Util.hexdump(addr.getBytes());
		System.out.println( "a = " + a.toString());
		System.out.println( "b = " + b.toString());
		System.out.println("a.equals(b) == " + a.equals(b));
	}
}
