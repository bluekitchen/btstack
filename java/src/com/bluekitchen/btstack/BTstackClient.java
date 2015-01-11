package com.bluekitchen.btstack;

import com.bluekitchen.btstack.event.BTstackEventDaemonDisconnect;

public class BTstackClient {

	/**
	 * BTstack Server Client
	 * uses background receive thread
	 */
	
	public static final int DEFAULT_TCP_PORT = 13333;
	public static final String DEFAULT_UNIX_SOCKET = "/tmp/BTstack";

	private volatile SocketConnection socketConnection;
	private PacketHandler packetHandler;
	private boolean connected;
	private Thread rxThread;
	private String unixDomainSocketPath;
	private int tcpPort;
	
	public BTstackClient(){
		connected = false;
		socketConnection = null;
		rxThread = null;
	}

	public void setUnixDomainSocketPath(String path){
		this.unixDomainSocketPath = path;
	}

	public void setTcpPort(int port){
		this.tcpPort = port;
	}
	
	public void registerPacketHandler(PacketHandler packetHandler){
		this.packetHandler = packetHandler;
	}
	
	public boolean connect(){
		
		if (tcpPort == 0){
			try {
				Class<?> clazz = Class.forName("com.bluekitchen.btstack.SocketConnectionUnix");
				socketConnection = (SocketConnection) clazz.newInstance();
				if (unixDomainSocketPath != null){
					socketConnection.setUnixDomainSocketPath(unixDomainSocketPath);
				}
			} catch (ClassNotFoundException e) {
				e.printStackTrace();
				return false;
			} catch (InstantiationException e) {
				e.printStackTrace();
				return false;
			} catch (IllegalAccessException e) {
				e.printStackTrace();
				return false;
			}
			
		} else {
			// TODO implement SocketConnectionTcp
			socketConnection = new SocketConnectionTCP();
			socketConnection.setTcpPort(tcpPort);
		}
		
		connected = socketConnection.connect();
		if (!connected) return false;
		
		rxThread = new Thread(new Runnable(){
			@Override
			public void run() {
				while (socketConnection != null){
					Packet packet = socketConnection.receivePacket();
					if (Thread.currentThread().isInterrupted()){
						return;
					}
					if (packet == null) {
						// server disconnected
						System.out.println("Rx Thread: Daemon Disconnected");
						packetHandler.handlePacket(new BTstackEventDaemonDisconnect());
						return;
					}
					switch (packet.getPacketType()){
						case Packet.HCI_EVENT_PACKET:
							packetHandler.handlePacket(EventFactory.eventForPacket(packet));
							break;
						case Packet.L2CAP_DATA_PACKET:
							packetHandler.handlePacket(new L2CAPDataPacket(packet));
							break;
						case Packet.RFCOMM_DATA_PACKET:
							packetHandler.handlePacket(new RFCOMMDataPacket(packet));
							break;
						default:
							packetHandler.handlePacket(packet);
							break;
					}
				}
				System.out.println("Rx Thread: Interrupted");
			}
		});
		rxThread.start();
		
		return true;
	}
	
	public boolean sendPacket(Packet packet){
		if (socketConnection == null) return false;
		return socketConnection.sendPacket(packet);
	}
	
	public boolean L2CAPSendData(int l2capChannelID, byte[] data){
		return sendPacket(new L2CAPDataPacket(l2capChannelID, data));
	}

	public boolean RFCOMMSendData(int rfcommChannelID, byte[] data){
		return sendPacket(new RFCOMMDataPacket(rfcommChannelID, data));
	}

	public void disconnect(){
		if (socketConnection == null) return;
		if (rxThread == null) return;
		// signal rx thread to stop
		rxThread.interrupt();
		// wait for thread stopped
		try {
			rxThread.join();
		} catch (InterruptedException e){
			System.out.println("Unexpected interrupted execption waiting for receive thread to terminate");
			e.printStackTrace();
		}
		// disconnect socket
		socketConnection.disconnect();
		// done
		socketConnection = null;
	}
}