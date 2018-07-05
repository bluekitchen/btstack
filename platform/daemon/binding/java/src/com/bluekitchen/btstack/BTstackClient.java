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
	private int logicTime = 1;
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
		
		rxThread = null;
		
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
		
		logicTime++;
		final int rxThreadId = logicTime;
		final SocketConnection threadSocketConnection = socketConnection;
		rxThread = new Thread(new Runnable(){
			@Override
			public void run() {
				while (logicTime == rxThreadId){
					Packet packet = threadSocketConnection.receivePacket();
					if (Thread.currentThread().isInterrupted()){
						System.out.println("Rx Thread: exit via interrupt, thread id " + rxThreadId);
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
				System.out.println("Rx Thread: exit via logic time change, thread id " + rxThreadId);
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

		logicTime++;
		
		// check if we're called on rx thread
		if (Thread.currentThread() != rxThread){
			
			// signal rx thread to stop
			rxThread.interrupt();

			// unblock read by sending an arbitrary command
			if (this instanceof BTstack){
				BTstack btstack = (BTstack) this;
				btstack.BTstackGetState();
			}

			// wait for thread to stop
			try {
				rxThread.join();
			} catch (InterruptedException e){
				System.out.println("Unexpected interrupted execption waiting for receive thread to terminate");
				e.printStackTrace();
			}
		}
		
		// disconnect socket -> triggers IOException on read
		socketConnection.disconnect();

		// done
		socketConnection = null;
	}
}
