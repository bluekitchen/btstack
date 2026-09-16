#!/usr/bin/env python3
# BlueKitchen GmbH (c) 2025
import sys
import binascii

sampling_frequency_map = {
    0x1: 8000,
    0x2: 11250,
    0x3: 16000,
    0x4: 22050,
    0x5: 24000,
    0x6: 32000,
    0x7: 44100,
    0x8: 48000,
    0x9: 88200,
    0xA: 96000,
    0xB: 176400,
    0xC: 192000,
    0xD: 384000
}

def parse_config_tlv(config_data):
    offset = 0
    while offset < len(config_data):
        length = config_data[offset]
        type   = config_data[offset + 1]
        array = config_data[offset + 2:offset + 1 + length]
        value = int.from_bytes(array, 'little')
        offset += 1 + length
        if type == 0x01:
            print(f"    Sampling Frequency: {sampling_frequency_map[value]} Hz")
        elif type == 0x02:
            if value == 0:
                print(f"    Frame Duration: 7.5 ms")
            else:
                print(f"    Frame Duration: 10 ms")
        elif type == 0x03:
            print(f"    Audio Channel Allocation: {value}")
        elif type == 0x04:
            print(f"    Octets per Frame: {value}")
        elif type == 0x05:
            print(f"    Codec Frame Bocks per SDU: {value}")
        else:
            print(f"    Unknown Type {type}: {array.hex().upper()}")

def parse_meta_tlv(meta_data):
    offset = 0
    while offset < len(meta_data):
        length = meta_data[offset]
        type   = meta_data[offset + 1]
        array = meta_data[offset + 2:offset + 1 + length]
        value = int.from_bytes(array, 'little')
        offset += 1 + length
        if type == 0x01:
            print(f"    Preferred Audio Contexts: {value:x}")
        elif type == 0x02:
            print(f"    Streaming Audio Contexts: {value:x}")
        elif type == 0x03:
            print(f"    Program Info: {array.decode('utf-8')}")
        elif type == 0x04:
            print(f"    Language: {['%c' % c for c in array]}")
        elif type == 0x05:
            print(f"    CID: {array.hex().upper()}")
        elif type == 0x06:
            print(f"    Parental Rating: {value:x}")
        elif type == 0x07:
            print(f"    Program Info URI: {array.decode('utf-8')}")
        elif type == 0x08:
            print(f"    Extended Metadata: {array.hex().upper()}")
        else:
            print(f"    Unknown Type {type}: {array.hex().upper()}")


def parse_codec_config(num_ases, parameters):
    offset = 0
    for i in range(num_ases):
        ase_id =          parameters[offset]
        target_latency =  parameters[offset + 1]
        target_phy =      parameters[offset + 2]
        coding_format =   parameters[offset + 3]
        company_id =      parameters[offset + 4] | (parameters[offset + 5] << 8)
        vendor_codec_id = parameters[offset + 6] | (parameters[offset + 7] << 8)
        config_len      = parameters[offset + 8]
        config_data     = parameters[offset + 9:offset + 9 + config_len]
        offset += 9 + config_len

        print(f"ASE {ase_id}:")
        print(f"  Target Latency: {target_latency}")
        print(f"  Target PHY:     {target_phy}")
        print(f"  Coding Format:  {coding_format}")
        print(f"  Company ID:     {company_id}")
        print(f"  Codec Specific: {vendor_codec_id:04X}")
        print(f"  Codec Config:   {config_data.hex().upper()}")
        parse_config_tlv(config_data)

def parse_qos_config(num_ases, parameters):
    offset = 0
    for i in range(num_ases):
        ase_id =          parameters[offset]
        cig_id =          parameters[offset + 1]
        cis_id =          parameters[offset + 2]
        sdu_interval =    int.from_bytes(parameters[offset+3:offset+6], 'little')
        framing  =        parameters[offset + 6]
        phy =             parameters[offset + 7]
        max_sdu =         int.from_bytes(parameters[offset+8:offset+10], 'little')
        retransmission_number = parameters[offset+10]
        max_transport_latency = int.from_bytes(parameters[offset+11:offset+13], 'little')
        presentation_delay = int.from_bytes(parameters[offset+13:offset+16], 'little')
        offset += 16

        print(f"ASE {ase_id}:")
        print(f"  CIG ID: {cig_id}")
        print(f"  CIS ID: {cis_id}")
        print(f"  SDU Interval: {sdu_interval} us")
        print(f"  Framing: {framing}")
        print(f"  PHY: {phy}")
        print(f"  Max SDU: {max_sdu}")
        print(f"  Retransmission Number: {retransmission_number}")
        print(f"  Max Transport Latency: {max_transport_latency} us")
        print(f"  Presentation Delay: {presentation_delay} us")

def parse_enable(num_ases, parameters):
    offset = 0
    for i in range(num_ases):
        ase_id =         parameters[offset]
        meta_length =    parameters[offset + 1]
        meta_data =      parameters[offset + 2:offset + 2 + meta_length]
        offset += 2 + meta_length
        print(f"ASE {ase_id}:")
        print(f"  Meta:   {meta_data.hex().upper()}")
        parse_meta_tlv(meta_data)


def parse_ascs_command(hexstream: str):
    print('ASCS Operation:', hexstream)
    try:
        data = binascii.unhexlify(hexstream)
    except binascii.Error:
        print("Invalid hex stream format.")
        return

    if len(data) < 2:
        print("Data is too short to be a valid ASE Control Point write.")
        return

    opcode   = data[0]  # First byte: Opcode
    num_ases = data[1]  # Second byte: number of ASEs
    parameters = data[2:]  # Remaining bytes: Parameters

    opcode_dict = {
        0x01: "Codec Configure",
        0x02: "QoS Configure",
        0x03: "Enable",
        0x04: "Receiver Start Ready",
        0x05: "Disable",
        0x06: "Receiver Stop Ready",
        0x07: "Update Metadata",
        0x08: "Release",
    }

    print(f"Opcode: {opcode_dict.get(opcode, 'Unknown')} (0x{opcode:02X}), Num ASEs: {num_ases}")

    if opcode == 0x01:  # Codec Configure
        parse_codec_config(num_ases, parameters)
    elif opcode == 0x02:  # QoS Configure
        parse_qos_config(num_ases, parameters)
    elif opcode == 0x03:  # Enable
        parse_enable(num_ases, parameters)
    elif opcode == 0x04:  # Receiver Start Ready
        pass
    elif opcode == 0x05:  # Disable
        pass
    elif opcode == 0x06:  # Receiver Stop Ready
        pass
    elif opcode == 0x07:  # Update Metadata
        parse_enable(num_ases, parameters)
        pass
    elif opcode == 0x08:  # Release
        pass
    else:
        pass

    print()

if __name__ == "__main__":
    if len(sys.argv) < 2:
        print("Dump ASE Control Point Write command e.g. from Wireshark (copy as hexstream).")
        print('Usage: ', sys.argv[0], '<hexstream> ...')
        sys.exit(1)

    print()
    for hexstream in sys.argv[1:]:
        parse_ascs_command(hexstream)
