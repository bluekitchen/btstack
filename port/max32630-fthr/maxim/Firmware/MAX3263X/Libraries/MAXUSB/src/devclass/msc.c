/*******************************************************************************
 * Copyright (C) 2017 Maxim Integrated Products, Inc., All Rights Reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 * IN NO EVENT SHALL MAXIM INTEGRATED BE LIABLE FOR ANY CLAIM, DAMAGES
 * OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
 *
 * Except as contained in this notice, the name of Maxim Integrated
 * Products, Inc. shall not be used except as stated in the Maxim Integrated
 * Products, Inc. Branding Policy.
 *
 * The mere transfer of this software does not imply any licenses
 * of trade secrets, proprietary technology, copyrights, patents,
 * trademarks, maskwork rights, or any other form of intellectual
 * property whatsoever. Maxim Integrated Products, Inc. retains all
 * ownership rights.
 *
 * $Date: 2017-10-17 15:36:13 -0500 (Tue, 17 Oct 2017) $ 
 * $Revision: 31422 $
 *
 ******************************************************************************/

#include "usb.h"
#include "usb_event.h"
#include "enumerate.h"
#include "msc.h"
#include "string.h"

/***** Definitions *****/

/* USB Mass-Storage class status values */
#define CSW_OK                  0
#define CSW_FAILED              1

/* USB Mass-storage class requests */
#define MSC_MASS_STORAGE_RESET          0xff
#define MSC_GET_MAX_LUN                 0xfe

#define CBW_LEN                         31
#define CSW_LEN                         13
#define REQSENSE_LEN                    18
#define MODESENSE_LEN                   4
#define INQUIRY_LEN                     36
#define RECVDIAGRES_LEN                 32
#define FORMATCAPACITY_LEN              12
#define CAPACITY_LEN                    8
#define CAPACITY16_LEN                  32

#define FORMATTED_MEDIA                 0x02000000

#define CMD_TEST_UNIT_READY             0x00
#define CMD_REQUEST_SENSE               0x03
#define CMD_INQUIRY                     0x12
#define CMD_MODE_SENSE_6                0x1A
#define CMD_START_STOP                  0x1B
#define CMD_RECEIVE_DIAGNOSTIC_RESULT   0x1C
#define CMD_MEDIUM_REMOVAL              0x1E
#define CMD_READ_FORMAT_CAPACITY        0x23
#define CMD_READ_CAPACITY               0x25
#define CMD_READ_10                     0x28
#define CMD_WRITE_10                    0x2A
#define CMD_MODE_SENSE_10               0x5A
#define CMD_READ_CAPACITY_16            0x9E

#define REQSENSE_OK                     0
#define REQSENSE_CHECK_CONDITION        2

#define CSW_SIGNATURE_IDX               0
#define CSW_TAG_IDX                     4
#define CSW_DATARESIDUE_IDX             8
#define CSW_STATUS_IDX                  12

#define EP_CONTROL                      0

#define CBW_SIGNATURE_IDX               0
#define CBW_TAG_IDX                     4
#define CBW_DATA_TRANSFER_LENGTH_IDX    8
#define CBW_FLAGS_IDX                   12
#define CBW_LUN_IDX                     13
#define CBW_CBLENGTH_IDX                14
#define CBW_CB_IDX                      15

/* Function pointer definition.  Used to call a funtion after a read or write completes. */
typedef void callbackFunc();

/***** File Scope Data *****/
static usb_req_t req;
static uint8_t req_data[MAX_PACKET_SIZE];
static uint8_t csw_data[CSW_LEN];

/* Storage for application specific data */
static msc_idstrings_t id_strings;
static msc_mem_t mem;

/* Endpoint numbers */
static unsigned int ep_from_host;
static unsigned int ep_to_host;

static uint32_t numBlocks;
static uint32_t blockAddr;

/***** Function Prototypes *****/
static int msc_classReq(usb_setup_pkt *sud, void *cbdata);
static void msc_write(int ep, int len, void* cbdata);
static void msc_writeComplete(void* cbdata);
static void msc_read(int ep, int len, void (*callback)(void *));
static void msc_waitForCBW(void);
static void msc_cbwReceived(void* cbdata);
static void msc_sendCSW(void);
static void msc_sendRequestSense(void);
static void msc_sendInquiry(void);
static void msc_sendModeSense(void);
static void msc_sendReceiveDiagResult(void);
static void msc_sendFormatCapacity(void);
static void msc_sendCapacity(void);
static void msc_sendCapacity16(void);
static void msc_readMem(void);
static void msc_writeMem(void);
static void msc_writeMemBlock(void* cbdata);

/******************************************************************************/

int msc_init(const msc_idstrings_t* ids, const msc_mem_t* funcs)
{
    int err;
    
    ep_from_host = 0;
    ep_to_host = 0;
    
    memcpy(&id_strings, ids, sizeof(msc_idstrings_t));
    memcpy(&mem, funcs, sizeof(msc_mem_t));
    
    /* Prep "disk" memory */
    err = mem.init();
  
    /* Handle class-specific SETUP requests */
    enum_register_callback(ENUM_CLASS_REQ, msc_classReq, NULL);
    
    /* Return zero for success or non-zero for error. */
    return err;
}

/******************************************************************************/

int msc_configure(const msc_cfg_t *cfg)
{
    int err;
    
    /* This driver is limited to handling blocks no bigger than MAX_PACKET_SIZE. */
    if((cfg->out_maxpacket > MAX_PACKET_SIZE) || (cfg->in_maxpacket > MAX_PACKET_SIZE))
    {
        return 1;
    }

    /* Setup endpoint used to receive communication from the host. */
    ep_from_host = cfg->out_ep;
    if ((err = usb_config_ep(ep_from_host, MAXUSB_EP_TYPE_OUT, cfg->out_maxpacket)) != 0) {
        msc_deconfigure();
        return err;
    }

    /* Setup endpoint used to send communication to the host. */
    ep_to_host = cfg->in_ep;
    if ((err = usb_config_ep(ep_to_host, MAXUSB_EP_TYPE_IN, cfg->in_maxpacket)) != 0) {
        msc_deconfigure();
        return err;
    }
    
    /* Device has been configured, prep the memory for transactions. */
    return mem.start();
}

/******************************************************************************/

int msc_deconfigure()
{
    /* Release endpoints if they have been configured */
    if (ep_from_host != 0) {
        usb_reset_ep(ep_from_host);
        ep_from_host = 0;
    }

    if (ep_to_host != 0) {
        usb_reset_ep(ep_to_host);
        ep_to_host = 0;
    }

    return 0;
}

/******************************************************************************/

static int msc_classReq(usb_setup_pkt *sud, void *cbdata)
{
    int result = -1;

    switch (sud->bRequest) {
        case MSC_GET_MAX_LUN:
            if (sud->bmRequestType == (RT_DEV_TO_HOST | RT_TYPE_CLASS | RT_RECIP_IFACE)) {
                /* Get Max LUN */
                req_data[0] = 0;
                msc_write(EP_CONTROL, 1, (void *)msc_waitForCBW);
                result = 0;
            }
            break;
        case MSC_MASS_STORAGE_RESET:
            /* Reset the control endpoint */
            usb_reset_ep(EP_CONTROL);
            break;
        default:
            /* Unexpected message received */
            usb_stall(EP_CONTROL);
            break;
    }

    return result;
}

/******************************************************************************/

/* Send bytes to the host */
/* cbdata contains a function pointer that will be called once the write completes. */
static void msc_write(int ep, int len, void* cbdata)
{
    memset(&req, 0, sizeof(usb_req_t));
    req.ep = ep;
    req.data = req_data;
    req.reqlen = len;
    req.callback = msc_writeComplete;
    req.cbdata = (void*)cbdata;
    req.type = MAXUSB_TYPE_TRANS;
    usb_write_endpoint(&req);
}

/******************************************************************************/

static void msc_writeComplete(void* cbdata)
{
    /* A write to the host has completed.  Check if the correct number of bytes were transferred. */
    if(req.reqlen == req.actlen) {
        usb_ackstat(req.ep);
        /* Call the requested function (if any) */
        if(cbdata) {
            ((callbackFunc*)cbdata)();
        }
    } else {
        /* Wrote an unexpected number of bytes. */
        usb_stall(req.ep);
    }
}

/******************************************************************************/

/* Read bytes from the host */
/* callback contains a function pointer to be called when the read completes. */
static void msc_read(int ep, int len, void (*callback)(void *))
{
    memset(&req, 0, sizeof(usb_req_t));
    req.ep = ep;
    req.data = req_data;
    req.reqlen = len;
    req.callback = callback;
    req.cbdata = NULL;
    req.type = MAXUSB_TYPE_TRANS;
    usb_read_endpoint(&req);
}

/******************************************************************************/

static void msc_waitForCBW()
{
    /* Submit a read request to the USB stack. */
    /* Have it call cbwReceived when the read completes. */
    msc_read(ep_from_host, CBW_LEN, msc_cbwReceived);
}

/******************************************************************************/

static void msc_cbwReceived(void* cbdata)
{
    /* Verify this is a valid CBW */
    if((req.actlen == CBW_LEN) && (strncmp((const char *)(req_data + CBW_SIGNATURE_IDX), "USBC", 4) == 0)) {
        /* Construct the response CSW */
        memset(csw_data, 0, CSW_LEN);
        memcpy(csw_data + CSW_SIGNATURE_IDX, "USBS", 4);
        memcpy(csw_data + CSW_TAG_IDX, req_data + CBW_TAG_IDX, 4);

        /* Assume a good response for now.  Commands can overwrite this as needed */
        csw_data[CSW_STATUS_IDX] = CSW_OK;

        /* More CBW validation. */
        if((req_data[CBW_CBLENGTH_IDX] & 0x1F) > 16) {
            usb_stall(EP_CONTROL);
        }

        /* Act on the command received.  Most commands will process some data
           from the host, or send data to the host.  Once all the required 
           data has been transmitted, a CSW must be sent.  Those commands that
           have no data can send the CSW immediately.  Others will need to rely
           on callbacks to send the CSW after the data is transmitted. */

        switch(req_data[CBW_CB_IDX]) {
            case CMD_TEST_UNIT_READY:
                /* This command only requires a CSW to be sent back. */
                /* Prep the response and send it along. */
                csw_data[CSW_STATUS_IDX] = mem.ready() ? CSW_OK : CSW_FAILED;
                msc_sendCSW();
                break;
            case CMD_REQUEST_SENSE:
                msc_sendRequestSense();
                break;
            case CMD_INQUIRY:
                msc_sendInquiry();
                break;
            case CMD_START_STOP:
                /* This command only requires a CSW to be sent back. */
                /* Prep the response and send it along. */
                if(req_data[19] & 2) {
                    /* STOP */
                    /* Flush any pending transactions and report the results. */
                    csw_data[CSW_STATUS_IDX] = mem.stop() ? CSW_FAILED : CSW_OK;
                } else {
                    /* START */
                    mem.start();
                    csw_data[CSW_STATUS_IDX] = mem.ready() ? CSW_OK : CSW_FAILED;
                }
                msc_sendCSW();
                break;
            case CMD_MODE_SENSE_6:
            case CMD_MODE_SENSE_10:
                msc_sendModeSense();
                break;
            case CMD_MEDIUM_REMOVAL:
                /* This command only requires a CSW to be sent back. */
                msc_sendCSW();
                break;
            case CMD_RECEIVE_DIAGNOSTIC_RESULT:
                msc_sendReceiveDiagResult();
                break;
            case CMD_READ_FORMAT_CAPACITY:
                msc_sendFormatCapacity();
                break;
            case CMD_READ_CAPACITY:
                msc_sendCapacity();
                break;
            case CMD_READ_CAPACITY_16:
                msc_sendCapacity16();
                break;
            case CMD_READ_10:
                /* The read command may consist of multiple blocks.  The readMem function will 
                   handle sending all of the blocks followed by sending the CSW. */
                numBlocks = req_data[22] << 8 | req_data[23];
                blockAddr = req_data[17] << 24 | req_data[18] << 16 | req_data[19] << 8 | req_data[20];
                msc_readMem();
                break;
            case CMD_WRITE_10:
                /* The write command may consist of multiple blocks.  The writeMem function will 
                   handle receiving all of the blocks followed by sending the CSW. */
                numBlocks = req_data[22] << 8 | req_data[23];
                blockAddr = req_data[17] << 24 | req_data[18] << 16 | req_data[19] << 8 | req_data[20];
                msc_writeMem();
                break;
            default:
                /* This command wasn't recognized.  Send an error CSW. */
                csw_data[CSW_STATUS_IDX] = CSW_FAILED;
                msc_sendCSW();
                break;
        }
    } else {
        /* Packet received with bad length - stall endpoint */
        usb_stall(req.ep);
    }
}

/******************************************************************************/

static void msc_sendRequestSense()
{
    /* Prep the response data */
    req_data[0]  = 0x70;     /* Error Code */
    req_data[1]  = 0x00;     /* Segment Number */
    req_data[2]  = mem.ready() ? REQSENSE_OK : REQSENSE_CHECK_CONDITION; /* Sense Key */
    req_data[3]  = 0x00;     /* Info - 4 Bytes */
    req_data[4]  = 0x00;     
    req_data[5]  = 0x00;     
    req_data[6]  = 0x00;     
    req_data[7]  = 0x0A;     /* Additional Sense Length */
    req_data[8]  = 0x00;     /* Command Specific Info - 4 Bytes */
    req_data[9]  = 0x00;     
    req_data[10] = 0x00;     
    req_data[11] = 0x00;     
    req_data[12] = 0x3A;     /* Additional Sense Code */
    req_data[13] = 0x00;     /* Additional Send Qualifier */
    req_data[14] = 0x00;     /* Reserved - 4 Bytes */
    req_data[15] = 0x00;     
    req_data[16] = 0x00;     
    req_data[17] = 0x00;     

    /* Send the data to the host and have the USB stack call
       send CSW once complete. */
    msc_write(ep_to_host, REQSENSE_LEN, (void *)msc_sendCSW);
}

/******************************************************************************/

static void msc_sendInquiry()
{
    req_data[0]  = 0x00;               // Peripheral Device Type (PDT) - SBC Direct-access device
    req_data[1]  = 0x80;               // Removable Medium Bit is Set
    req_data[2]  = 0x02;               // Version
    req_data[3]  = 0x02;               // Obsolete[7:6],NORMACA[5],HISUP[4],Response Data Format[3:0]
    req_data[4]  = 0x1f;               // Additional Length
    req_data[5]  = 0x73;               // SCCS[7],ACC[6],TPGS[5:4],3PC[3],Reserved[2:1],PROTECT[0]
    req_data[6]  = 0x6d;               // BQUE[7],ENCSERV[6],VS[5],MULTIP[4],MCHNGR[3],Obsolete[2:1],ADDR16[0]
    req_data[7]  = 0x69;               // Obsolete[7:6],WBUS116[5],SYNC[4],LINKED[3],Obsolete[2],CMDQUE[1],VS[0]

    memcpy(&(req_data[8]), id_strings.vendor, VENDOR_ID_MAX_LEN);        /* Copy application specific data in */
    memcpy(&(req_data[16]), id_strings.product, PRODUCT_ID_MAX_LEN);
    memcpy(&(req_data[32]), id_strings.revision, PRODUCT_REV_MAX_LEN);

    msc_write(ep_to_host, INQUIRY_LEN, (void *)msc_sendCSW);
}

/******************************************************************************/

static void msc_sendCSW()
{
    /* Previously received command has stored its status data in the csw_data array
       Copy that data to the outgoing buffer, send it along, and have the USB stack call
       waitForCBW once all of the data has been transmitted.  This will arm the driver
       for receiving the next command. */
    memcpy(req_data, csw_data, CSW_LEN);
    msc_write(ep_to_host, CSW_LEN, (void *)msc_waitForCBW);
}

/******************************************************************************/

static void msc_sendModeSense()
{
    /* Prep the response and send it followed by sending the CSW */
    req_data[0] = 0x04;
    req_data[1] = 0x00;
    req_data[2] = (mem.write == NULL) ? 0x90 : 0x10;
    req_data[3] = 0x00;
    msc_write(ep_to_host, MODESENSE_LEN, (void *)msc_sendCSW);
}

/******************************************************************************/

static void msc_sendReceiveDiagResult()
{
    /* Prep the response and send it followed by sending the CSW */
    memset(req_data, 0, RECVDIAGRES_LEN);
    msc_write(ep_to_host, RECVDIAGRES_LEN, (void *)msc_sendCSW);
}

/******************************************************************************/

static void msc_sendFormatCapacity()
{
    /* Get the total number of blocks on the "disk". */
    uint32_t size = mem.size();

    /* Prep the response and send it followed by sending the CSW */
    req_data[0] = 0x00;
    req_data[1] = 0x00;
    req_data[2] = 0x00;
    req_data[3] = 0x08;

    req_data[4] = (uint8_t)(size >> 24);
    req_data[5] = (uint8_t)(size >> 16);
    req_data[6] = (uint8_t)(size >> 8);
    req_data[7] = (uint8_t)(size);

    req_data[8]  = (uint8_t)((MAX_PACKET_SIZE | FORMATTED_MEDIA) >> 24);
    req_data[9]  = (uint8_t)((MAX_PACKET_SIZE | FORMATTED_MEDIA) >> 16);
    req_data[10] = (uint8_t)((MAX_PACKET_SIZE | FORMATTED_MEDIA) >> 8);
    req_data[11] = (uint8_t)(MAX_PACKET_SIZE | FORMATTED_MEDIA);

    msc_write(ep_to_host, FORMATCAPACITY_LEN, (void *)msc_sendCSW);
}

/******************************************************************************/

static void msc_sendCapacity()
{
    /* Get the total number of data blocks on the "disk". */
    uint32_t size = mem.size() - 1;

    /* Prep the response and send it followed by sending the CSW */
    req_data[0] = (uint8_t)(size >> 24);
    req_data[1] = (uint8_t)(size >> 16);
    req_data[2] = (uint8_t)(size >> 8);
    req_data[3] = (uint8_t)(size);

    req_data[4] = (uint8_t)((MAX_PACKET_SIZE) >> 24);
    req_data[5] = (uint8_t)((MAX_PACKET_SIZE) >> 16);
    req_data[6] = (uint8_t)((MAX_PACKET_SIZE) >> 8);
    req_data[7] = (uint8_t)(MAX_PACKET_SIZE);

    msc_write(ep_to_host, CAPACITY_LEN, (void *)msc_sendCSW);
}

/******************************************************************************/

static void msc_sendCapacity16()
{
    /* Get the total number of data blocks on the "disk". */
    uint32_t size = mem.size() - 1;

    /* Prep the response and send it followed by sending the CSW */
    memset(req_data, 0, CAPACITY16_LEN);

    req_data[4] = (uint8_t)(size >> 24);
    req_data[5] = (uint8_t)(size >> 16);
    req_data[6] = (uint8_t)(size >> 8);
    req_data[7] = (uint8_t)(size);

    req_data[8]  = (uint8_t)((MAX_PACKET_SIZE) >> 24);
    req_data[9]  = (uint8_t)((MAX_PACKET_SIZE) >> 16);
    req_data[10] = (uint8_t)((MAX_PACKET_SIZE) >> 8);
    req_data[11] = (uint8_t)(MAX_PACKET_SIZE);

    msc_write(ep_to_host, CAPACITY16_LEN, (void *)msc_sendCSW);
}

/******************************************************************************/

static void msc_readMem()
{
    /* Are there more blocks to send to the host? */
    if(numBlocks) {
        /* Read from the "disk" and place the results in the outgoing buffer. */
        /* Log any errors in the appropriate location of the CSW response. */
        csw_data[CSW_STATUS_IDX] |= mem.read(blockAddr, req_data);
        blockAddr++; 
        numBlocks--;
        /* Send the data to the host and have the USB stack call the function
           again once the data has been sent. */
        msc_write(ep_to_host, MAX_PACKET_SIZE, (void *)msc_readMem);
    } else {
        /* All the requested data has been sent, it is time to send the CSW. */
        msc_sendCSW();
    }
}

/******************************************************************************/

static void msc_writeMem()
{
    /* Are there more blocks to receive from the host? */
    if(numBlocks) {
        /* Setup the read from host transaction and have the USB stack call
           writeMemBlock once all of the data has been received. */
        msc_read(ep_from_host, MAX_PACKET_SIZE, msc_writeMemBlock);
    }
    else {
        /* All the requested data has been received, it is time to send the CSW. */
        msc_sendCSW();
    }
}

/******************************************************************************/

static void msc_writeMemBlock(void* cbdata)
{
    /* Write the received data to the "disk". */
    csw_data[CSW_STATUS_IDX] |= mem.write(blockAddr, req_data);
    blockAddr++;
    numBlocks--;

    /* Go receive the next block of data (in any) */
    msc_writeMem();
}
