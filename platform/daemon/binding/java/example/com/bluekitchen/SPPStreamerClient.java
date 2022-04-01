package com.bluekitchen;

import com.bluekitchen.btstack.BD_ADDR;
import com.bluekitchen.btstack.BTstack;
import com.bluekitchen.btstack.Packet;
import com.bluekitchen.btstack.PacketHandler;
import com.bluekitchen.btstack.RFCOMMDataPacket;
import com.bluekitchen.btstack.Util;
import com.bluekitchen.btstack.event.*;

/**
 * Simple demonstration of the Java binding by streaming data over SPP.
 *
 * This is the same example as the: example/spp_streamer_client.c example.
 *
 * It can be run against the 'spp_streamer' example.
 *
 * To run, two dongles are needed, then:
 *
 * 1. In a first terminal run:
 *
 * cd port/libusb
 * ./spp_streamer
 *
 * 2. In a second terminal, run the daemon:
 *
 * cd port/daemon
 * ./src/BTdaemon --tcp
 *
 * 3. In a third terminal, run the java application:
 *
 * cd platform/daemon/binding/java
 * ant run
 */
public class SPPStreamerClient implements PacketHandler {

    private final static int NUM_ROWS = 25;
    private final static int NUM_COLS = 40;
    public static final int TEST_COD = 0x1234;

    private enum STATE {
        BTSTACK_WORKING, INQUIRY_RESULT, SDP_QUERY_RESULT, CONNECTED, SENDING
    }

    private BTstack btstack;
    private STATE state;

    private BD_ADDR remote;

    private int outgoing_channel_nr = -1;
    private int rfcommChannelID = 0;

    private final static int REPORT_INTERVAL_MS = 3000;
    private long test_data_transferred;
    private long test_data_start;
    private byte[] sppTestData;


    private void createSppTestData(final int mtu) {
        byte[] temp = new byte[NUM_ROWS * NUM_COLS];
        int x, y;
        for (y = 0; y < NUM_ROWS; y++) {
            for (x = 0; x < NUM_COLS - 2; x++) {
                temp[y * NUM_COLS + x] = (byte) ('0' + (x % 10));
            }
            temp[y * NUM_COLS + NUM_COLS - 2] = '\n';
            temp[y * NUM_COLS + NUM_COLS - 1] = '\r';
        }

        // cut to MTU
        sppTestData = new byte[Math.min(mtu, temp.length)];
        System.arraycopy(temp, 0, sppTestData, 0, sppTestData.length);
    }

    void testReset() {
        test_data_start = System.currentTimeMillis();
        test_data_transferred = 0;
    }

    private void calculateSpeedAndLog(int bytesSent){
        test_data_transferred += bytesSent;
        // evaluate
        long now = System.currentTimeMillis();
        long timePassed = now - test_data_start;
        if (timePassed < REPORT_INTERVAL_MS) return;

        // print speed
        long bytesPerSecond = test_data_transferred * 1000 / timePassed;
        System.out.printf("%d bytes -> %d.%03d kB/s\n", (int) test_data_transferred, (int) bytesPerSecond / 1000, bytesPerSecond % 1000);

        // restart
        test_data_start = now;
        test_data_transferred  = 0;
    }

    private void startSDPQuery() {
        state = STATE.SDP_QUERY_RESULT;
        int sppUUID = 0x1101;
        byte[] serviceSearchPattern = Util.serviceSearchPatternForUUID16(sppUUID);
        btstack.SDPClientQueryRFCOMMServices(remote, serviceSearchPattern);
    }

    public void handlePacket(Packet packet) {
        if (packet instanceof HCIEventDisconnectionComplete) {
            final HCIEventDisconnectionComplete event = (HCIEventDisconnectionComplete) packet;
            System.out.printf("Received disconnect, status %d, handle %x%n", event.getStatus(), event.getConnectionHandle());
            btstack.disconnect();
            return;
        }

        switch (state) {
            case BTSTACK_WORKING:
                if (packet instanceof BTstackEventState) {
                    final BTstackEventState event = (BTstackEventState) packet;
                    if (event.getState() == 2) {
                        System.out.println("BTstack working. Start inquiry..");
                        state = STATE.INQUIRY_RESULT;
                        btstack.GAPInquiryStart(5);
                    }
                }
                break;

            case INQUIRY_RESULT:
                if (packet instanceof GAPEventInquiryResult) {
                    final GAPEventInquiryResult event = ((GAPEventInquiryResult) packet);
                    System.out.printf("Found device with COD: %d with address: %s\n", event.getClassOfDevice(), event.getBdAddr());
                    if (event.getClassOfDevice() == TEST_COD) {
                        remote = event.getBdAddr();
                        btstack.GAPInquiryStop();
                        System.out.println("Start SDP query on: " + remote);
                        startSDPQuery();
                    }
                }
                if (packet instanceof GAPEventInquiryComplete) {
                    if (remote == null) {
                        System.out.println("No device with COD: %d found -> scan again.");
                        btstack.GAPInquiryStart(5);
                    }
                }
                break;

            case SDP_QUERY_RESULT:
                if (packet instanceof SDPEventQueryRFCOMMService) {
                    final SDPEventQueryRFCOMMService service = (SDPEventQueryRFCOMMService) packet;
                    System.out.println("Found RFCOMM channel " + service.getName() + ", channel nr: " + service.getRFCOMMChannel());
                    outgoing_channel_nr = service.getRFCOMMChannel();
                }
                if (packet instanceof SDPEventQueryComplete) {
                    SDPEventQueryComplete complete = (SDPEventQueryComplete) packet;
                    if (complete.getStatus() != 0) {
                        System.out.printf("SDP Query failed with status 0x%02x, retry SDP query.%n", complete.getStatus());
                        startSDPQuery();
                        break;
                    }
                    if (outgoing_channel_nr >= 0) {
                        state = STATE.CONNECTED;
                        System.out.println("Connect to channel nr " + outgoing_channel_nr);
                        btstack.RFCOMMCreateChannel(remote, outgoing_channel_nr);
                    }
                }
                break;

            case CONNECTED:
                if (packet instanceof RFCOMMEventChannelOpened) {
                    RFCOMMEventChannelOpened e = (RFCOMMEventChannelOpened) packet;
                    System.out.println("RFCOMMEventChannelOpened with status " + e.getStatus());
                    if (e.getStatus() != 0) {
                        System.out.println("RFCOMM channel open failed, status " + e.getStatus());
                    } else {
                        state = STATE.SENDING;
                        rfcommChannelID = e.getRFCOMMCid();
                        System.out.printf("RFCOMM channel open succeeded. New RFCOMM Channel ID %d, max frame size %d%n", rfcommChannelID, e.getMaxFrameSize());

                        createSppTestData(e.getMaxFrameSize());
                        testReset();

                        btstack.RFCOMMRequestCanSendNow(e.getRFCOMMCid());
                    }
                }
                break;

            case SENDING:
                if (packet instanceof RFCOMMEventCanSendNow) {
                    btstack.RFCOMMSendData(rfcommChannelID, sppTestData);
                    calculateSpeedAndLog(sppTestData.length);
                    btstack.RFCOMMRequestCanSendNow(rfcommChannelID);
                }

                if (packet instanceof RFCOMMDataPacket) {
                    // duplex
                    calculateSpeedAndLog(sppTestData.length);
                }
            default:
                break;
        }
    }

    void stream() {
        System.out.println("SPP Streamer Client");

        // connect to BTstack Daemon via default port on localhost
        // start: src/BTdaemon --tcp

        btstack = new BTstack();
        btstack.setTcpPort(BTstack.DEFAULT_TCP_PORT);
        btstack.registerPacketHandler(this);
        boolean ok = btstack.connect();
        if (!ok) {
            System.out.println("Failed to connect to BTstack Server");
            return;
        }

        System.out.println("BTstackSetPowerMode(1)");

        state = STATE.BTSTACK_WORKING;
        btstack.BTstackSetPowerMode(1);
    }

    public static void main(String[] args) {
        new SPPStreamerClient().stream();
    }
}

