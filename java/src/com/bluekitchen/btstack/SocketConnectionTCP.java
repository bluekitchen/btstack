package com.bluekitchen.btstack;

import java.io.IOException;
import java.io.InputStream;
import java.io.OutputStream;
import java.net.Socket;


public class SocketConnectionTCP extends SocketConnection {

	private int port;
	private Socket socket;
	private InputStream in;
	private OutputStream out;
	private byte inHeader[]  = new byte[6];
	private byte inPayload[] = new byte[2000];

	public SocketConnectionTCP(){
		socket = null;
	}
	
	public void setTcpPort(int port) {
		this.port = port;
	}

	/* (non-Javadoc)
	 * @see com.bluekitchen.btstack.SocketConnection#connect()
	 */
	@Override
	public boolean connect() {
		try {
			socket = new Socket("localhost", port);
			in = socket.getInputStream();
			out = socket.getOutputStream();
			return true;
		} catch (IOException e) {
			e.printStackTrace();
			return false;
		}
	}

	/* (non-Javadoc)
	 * @see com.bluekitchen.btstack.SocketConnection#sendPacket(com.bluekitchen.btstack.Packet)
	 */
	@Override
	public synchronized boolean sendPacket(Packet packet) {
		
		if (out == null) return false;

		try {
			// System.out.println("Send "); Util.hexdump(packet.getBuffer(), packet.getPayloadLen());
			out.write(headerForPacket(packet));
			out.write(packet.getBuffer());
			out.flush();
			return true;
		} catch (IOException e) {
			e.printStackTrace();
			return false;
		}
	}

	/* (non-Javadoc)
	 * @see com.bluekitchen.btstack.SocketConnection#receivePacket()
	 */
	@Override
	public Packet receivePacket() {

		if (in == null) return null;
				
		int bytes_read = Util.readExactly(in, inHeader, 0, 6);
		if (bytes_read != 6) return null;

		int packetType = Util.readBt16(inHeader, 0);
		int channel    = Util.readBt16(inHeader, 2);
		int len        = Util.readBt16(inHeader, 4);
		
		bytes_read = Util.readExactly(in, inPayload, 0, len);
		if (bytes_read != len) return null;

		Packet packet = new Packet(packetType, channel ,inPayload, len);
		return packet;
	}
	
	/* (non-Javadoc)
	 * @see com.bluekitchen.btstack.SocketConnection#disconnect()
	 */
	@Override
	public void disconnect() {

		if (socket != null){
			try {
				socket.close();
			} catch (IOException e) {
			}
		}
	}

	private byte[] headerForPacket(Packet packet) {
		byte header[]  = new byte[6];
		Util.storeBt16(header, 0, packet.getPacketType());
		Util.storeBt16(header, 2, packet.getChannel());
		Util.storeBt16(header, 4, packet.getBuffer().length);
		return header;
	}
}
