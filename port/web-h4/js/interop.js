// Interop between C code and browser DOM

// custom log function
function log(msg) {
    console.log("[BTstack]", msg);
}

// convert hex to string
function prettyHex(
    buffer,
    offset = 0,
    length = buffer.length,
    bytesPerLine = 16,
) {
    bytes = buffer.subarray(offset, offset + length);
    return Array.from(bytes)
        .map((b, i) => {
            const hex = b.toString(16).padStart(2, "0");
            const sep = (i + 1) % bytesPerLine === 0 ? "\n" : " ";
            return hex + sep;
        })
        .join(" ");
}

//
// Async JS part of hci_transport_h4_js.c
//

// HCI H4 Packet Types
const HCI_H4 = Object.freeze({
    COMMAND: 0x01,
    ACL: 0x02,
    SCO: 0x03,
    EVENT: 0x04,
    ISO: 0x05,
    RESERVED: 0x06,
});

// global state
let serialPort = null;
let writer = null;
let bufferedReader = null;

async function hci_transport_h4_js_receiver() {
    // get shared h4 receive buffer
    bufferPtr = _hci_transport_h4_get_receive_buffer();
    bufferSize = _hci_transport_h4_get_receive_buffer_size();
    receiveBuffer = new Uint8Array(HEAPU8.buffer, bufferPtr, bufferSize);
    while (1) {
        await bufferedReader.readInto(receiveBuffer, 0, 1);
        packetType = receiveBuffer[0];
        offset = 1;
        switch (packetType) {
            case HCI_H4.EVENT:
                await bufferedReader.readInto(receiveBuffer, offset, 2);
                offset += 2;
                payloadLen = receiveBuffer[2];
                break;
            case HCI_H4.SCO:
                await bufferedReader.readInto(receiveBuffer, offset, 3);
                offset += 3;
                payloadLen = receiveBuffer[3];
                break;
            case HCI_H4.ACL:
            case HCI_H4.ISO:
                log("ACL/ISO packet");
                await bufferedReader.readInto(receiveBuffer, offset, 4);
                offset += 4;
                payloadLen = receiveBuffer[3] + (receiveBuffer[4] << 8);
                break;
            default:
                log("Invalid packet type " + packetType);
                continue;
        }
        // read payload
        await bufferedReader.readInto(receiveBuffer, offset, payloadLen);
        offset += payloadLen;
        // dispatch packet
        _hci_transport_h4_process_packet(offset);
    }
}

async function hci_transport_h4_js_send_data(data) {
    await writer.write(data);
    _hci_transport_h4_packet_sent();
}

// UI Helper
function btstack_tlv_js_updated(){
    console.log("[BTstack TLV] updated");
    const clearTLVBtn = document.getElementById("clearTLVButton");
    clearTLVBtn.hidden = false;
}

// Main Application

// open serial port triggered by user interaction, e.g. pressing an HTML button
Module.onStartBTstack = async (config) => {
    log("Start BTstack clicked");

    if ("serial" in navigator) {
        // The Web Serial API is supported.
        log("Web Serial API supported");

        // Prompt user to select any serial port.
        const port = await navigator.serial.requestPort();
        log("Port selected");

        // Wait for the serial port to open.
        flowControl = "hardware";
        await port.open({ baudRate: config.baud, flowControl: flowControl });
        log(
            "Port opened" + ` (baudrate: ${baudrate}, flowControl: ${flowControl})`,
        );

        // setup reader and writer
        bufferedReader = new BufferedReader(port);
        writer = port.writable.getWriter();

        // start example
        _btstack_main();
    }
};

if (!("serial" in navigator)) {
    log("Web Serial API --NOT-- supported");
}

// Emscripten module
if (typeof Module === "undefined") Module = {};

// post-js example
Module.onRuntimeInitialized = () => {
    log("WebAssembly is ready!");
};
