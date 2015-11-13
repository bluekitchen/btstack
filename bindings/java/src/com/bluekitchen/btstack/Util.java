package com.bluekitchen.btstack;

import java.io.IOException;
import java.io.InputStream;
import java.io.UnsupportedEncodingException;
import java.util.Arrays;

public class Util {
	public static int opcode(int ogf, int ocf){
		return ocf | (ogf << 10);
	}
	
	public static int readByte(byte[] buffer, int offset){
		int data = buffer[offset];
		if (data >= 0) return data;
		return data + 256;
	}
	
	public static int readBt16(byte[] buffer, int offset){
		return readByte(buffer, offset) | (readByte(buffer, offset + 1) << 8);
	}
	
	public static int readBt24(byte[] buffer, int offset){
		return readByte(buffer, offset) | (readByte(buffer, offset + 1) << 8) | (readByte(buffer, offset + 2) << 16) ;
	}

	public static long readBt32(byte[] buffer, int offset){
		return (((long) readByte(buffer, offset+3)) << 24) | readByte(buffer, offset) | (readByte(buffer, offset + 1) << 8) | (readByte(buffer, offset + 2) << 16) ;
	}

	public static BD_ADDR readBdAddr(byte[] buffer, int offset){
		return new BD_ADDR(Arrays.copyOfRange(buffer, offset, offset + BD_ADDR.LEN));
	}
	
	public static GATTService readGattService(byte[] buffer, int offset){
		return new GATTService(Arrays.copyOfRange(buffer, offset, offset + GATTService.LEN));
	}
	
	public static GATTCharacteristic readGattCharacteristic(byte[] buffer, int offset){
		return new GATTCharacteristic(Arrays.copyOfRange(buffer, offset, offset + GATTCharacteristic.LEN));
	}
	
	public static GATTCharacteristicDescriptor readGattCharacteristicDescriptor(byte[] buffer, int offset){
		return new GATTCharacteristicDescriptor(Arrays.copyOfRange(buffer, offset, offset + GATTCharacteristicDescriptor.LEN));
	}

	public static int readExactly(InputStream in, byte[] buffer, int offset, int len){
		int readTotal = 0;
		try {
			while (len > 0){
				int read;
					read = in.read(buffer, offset, len);
				if (read < 0) break;
				len -= read;
				offset += read;
				readTotal += read;
			}
		} catch (IOException e) {
		}
		return readTotal;
	}
	
	public static void storeByte(byte[] buffer, int offset, int value){
		buffer[offset] = (byte) value;
	}

	public static void storeBt16(byte[] buffer, int offset, int value){
		storeByte(buffer, offset, value & 0xff);
		storeByte(buffer, offset+1, value >> 8);
	}
	
	public static void storeBt24(byte[] buffer, int offset, int value){
		storeByte(buffer, offset, value & 0xff);
		storeByte(buffer, offset+1, value >> 8);
		storeByte(buffer, offset+2, value >> 16);
	}

	public static void storeBt32(byte[] buffer, int offset, long value){
		storeByte(buffer, offset, (int) (value & 0xff));
		storeByte(buffer, offset+1, (int) (value >> 8));
		storeByte(buffer, offset+2, (int) (value >> 16));
		storeByte(buffer, offset+3, (int) (value >> 24));
	}

	public static void storeBytes(byte[] buffer, int offset, byte[] value) {
		System.arraycopy(value, 0, buffer, offset, value.length);
	}

	public static void storeBytes(byte[] buffer, int offset, byte[] value, int len) {
		int bytes_to_copy = Math.min(value.length, len);
		System.arraycopy(value, 0, buffer, offset, bytes_to_copy);
		for (int i = bytes_to_copy; i < len ; i++){
			buffer[i] = 0;
		}
	}

	public static void storeString(byte[] buffer, int offset, String value, int len) {
		byte data[] = new byte[0];
		try {
			data = value.getBytes("UTF-8");
		} catch (UnsupportedEncodingException e) {
			e.printStackTrace();
		}
		storeBytes(buffer, offset, data, len);
	}

	public static String asHexdump(byte[] buffer, int len){
		StringBuffer t = new StringBuffer();
		for (int i = 0; i < len ; i++){
			t.append(String.format("0x%02x, ", readByte(buffer, i)));
		}
		return t.toString();
	}

	public static String asHexdump(byte[] buffer){
		return asHexdump(buffer, buffer.length);
	}
	
	public static void hexdump(byte[] buffer, int len){
		System.out.println(asHexdump(buffer, len));
		System.out.println();
	}
	
	public static void hexdump(byte[] buffer){
		hexdump(buffer, buffer.length);
	}

	public static void flipX(byte src[], byte dst[]){
		int len = src.length;
		if (len != dst.length) return;
	    for (int i = 0; i < len ; i++) {
	        dst[len - 1 - i] = src[i];
		}
	}

	public static void storeNet32(byte[] buffer, int offset, long value) {
			storeByte(buffer, offset+3, (int) (value & 0xff));
			storeByte(buffer, offset+2, (int) (value >> 8));
			storeByte(buffer, offset+1, (int) (value >> 16));
			storeByte(buffer, offset,   (int) (value >> 24));
	}

	public static long readNet32(byte[] buffer, int offset) {
		return (((long) readByte(buffer, offset)) << 24) | (readByte(buffer, offset + 1) << 16) | (readByte(buffer, offset + 2) << 8)  | readByte(buffer, offset + 3);
	}

	public static byte[] serviceSearchPatternForUUID16(int uuid){
		return new byte[] {(byte)0x35, (byte)0x03, (byte)0x19, (byte) (uuid >> 8), (byte) (uuid & 0xff)};
	}

	public static byte[] getBytes(byte[] buffer, int offset, int length){
		return Arrays.copyOfRange(buffer, offset, offset + length);
	}

	public static String getText(byte[] buffer, int offset, int length){
		byte [] byteData = getBytes(buffer, offset, length);
		try {
			return new String(byteData, "UTF-8");
		} catch (UnsupportedEncodingException e) {
			e.printStackTrace();
		}
		return "";
	}
}
