package com.bluekitchen.btstack;

public abstract class SocketConnection {

	public abstract boolean connect();

	public abstract boolean sendPacket(Packet packet);

	public abstract Packet receivePacket();

	public abstract void disconnect();

	public void setUnixDomainSocketPath(String path) {
	}

	public void setTcpPort(int port) {
	}
}