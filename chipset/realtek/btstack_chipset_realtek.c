/*
 * Copyright (C) 2022 BlueKitchen GmbH
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the copyright holders nor the names of
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 * 4. Any redistribution, use, or modification is done solely for
 *    personal benefit and not for any commercial purpose or for
 *    monetary gain.
 *
 * THIS SOFTWARE IS PROVIDED BY BLUEKITCHEN GMBH AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL BLUEKITCHEN
 * GMBH OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS
 * OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
 * AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF
 * THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * Please inquire about commercial licensing options at
 * contact@bluekitchen-gmbh.com
 *
 */

#define BTSTACK_FILE__ "btstack_chipset_realtek.c"

/*
 *  btstack_chipset_realtek.c
 *
 *  Adapter to use REALTEK-based chipsets with BTstack
 */

#include "btstack_chipset_realtek.h"

#include <stddef.h> /* NULL */
#include <stdio.h>
#include <string.h> /* memcpy */

#include "btstack_control.h"
#include "btstack_debug.h"
#include "btstack_event.h"
#include "btstack_linked_list.h"
#include "btstack_util.h"
#include "hci.h"
#include "hci_transport.h"

#ifdef _MSC_VER
// ignore deprecated warning for fopen
#pragma warning(disable : 4996)
#endif

#define ROM_LMP_NONE 0x0000
#define ROM_LMP_8723a 0x1200
#define ROM_LMP_8723b 0x8723
#define ROM_LMP_8821a 0x8821
#define ROM_LMP_8761a 0x8761
#define ROM_LMP_8822b 0x8822
#define ROM_LMP_8852a 0x8852
#define ROM_LMP_8851b 0x8851

#define HCI_OPCODE_HCI_RTK_DOWNLOAD_FW 0xFC20
#define HCI_OPCODE_HCI_RTK_READ_ROM_VERSION 0xFC6D

#define READ_SEC_PROJ 4

#define HCI_CMD_SET_OPCODE(buf, opcode) little_endian_store_16(buf, 0, opcode)
#define HCI_CMD_SET_LENGTH(buf, length) buf[2] = length
#define HCI_CMD_DOWNLOAD_SET_INDEX(buf, index) buf[3] = index
#define HCI_CMD_DOWNLOAD_COPY_FW_DATA(buf, firmware, ptr, len) memcpy(buf + 4, firmware + ptr, len)

#define PATCH_SNIPPETS		0x01
#define PATCH_DUMMY_HEADER	0x02
#define PATCH_SECURITY_HEADER	0x03
#define PATCH_OTA_FLAG		0x04
#define SECTION_HEADER_SIZE	8

/* software id */
#define RTLPREVIOUS	0x00
#define RTL8822BU	0x70
#define RTL8723DU	0x71
#define RTL8821CU	0x72
#define RTL8822CU	0x73
#define RTL8761BU	0x74
#define RTL8852AU	0x75
#define RTL8723FU	0x76
#define RTL8852BU	0x77
#define RTL8852CU	0x78
#define RTL8822EU	0x79
#define RTL8851BU	0x7A

#pragma pack(push, 1)
struct rtk_epatch_entry {
    uint16_t chipID;
    uint16_t patch_length;
    uint32_t start_offset;
};

struct rtk_epatch {
    uint8_t signature[8];
    uint32_t fw_version;
    uint16_t number_of_total_patch;
    struct rtk_epatch_entry entry[0];
};

struct rtk_extension_entry {
    uint8_t opcode;
    uint8_t length;
    uint8_t *data;
};

struct rtb_section_hdr {
    uint32_t opcode;
    uint32_t section_len;
    uint32_t soffset;
};

struct rtb_new_patch_hdr {
    uint8_t signature[8];
    uint8_t fw_version[8];
    uint32_t number_of_section;
};
#pragma pack(pop)


enum {
    // Pre-Init: runs before HCI Reset
    STATE_PHASE_1_READ_LMP_SUBVERSION,
    STATE_PHASE_1_W4_READ_LMP_SUBVERSION,
    STATE_PHASE_1_READ_HCI_REVISION,
    STATE_PHASE_1_W4_READ_HCI_REVISION,
    STATE_PHASE_1_DONE,
    // Custom Init: runs after HCI Reset
    STATE_PHASE_2_READ_ROM_VERSION,
    STATE_PHASE_2_READ_SEC_PROJ,
    STATE_PHASE_2_W4_SEC_PROJ,
    STATE_PHASE_2_LOAD_FIRMWARE,
    STATE_PHASE_2_RESET,
    STATE_PHASE_2_DONE,
};
enum { FW_DONE, FW_MORE_TO_DO };

typedef struct {
    uint16_t prod_id;
    uint16_t lmp_sub;
    char *   mp_patch_name;
    char *   patch_name;
    char *   config_name;

    uint8_t *fw_cache1;
    int      fw_len1;
    uint8_t chip_type;
} patch_info;

static const patch_info fw_patch_table[] = {
/* { pid, lmp_sub, mp_fw_name, fw_name, config_name, chip_type } */
    {0x1724, 0x1200, "mp_rtl8723a_fw", "rtl8723a_fw", "rtl8723a_config", NULL, 0, RTLPREVIOUS},	/* RTL8723A */
    {0x8723, 0x1200, "mp_rtl8723a_fw", "rtl8723a_fw", "rtl8723a_config", NULL, 0, RTLPREVIOUS},	/* 8723AE */
    {0xA723, 0x1200, "mp_rtl8723a_fw", "rtl8723a_fw", "rtl8723a_config", NULL, 0, RTLPREVIOUS},	/* 8723AE for LI */
    {0x0723, 0x1200, "mp_rtl8723a_fw", "rtl8723a_fw", "rtl8723a_config", NULL, 0, RTLPREVIOUS},	/* 8723AE */
    {0x3394, 0x1200, "mp_rtl8723a_fw", "rtl8723a_fw", "rtl8723a_config", NULL, 0, RTLPREVIOUS},	/* 8723AE for Azurewave */

    {0x0724, 0x1200, "mp_rtl8723a_fw", "rtl8723a_fw", "rtl8723a_config", NULL, 0, RTLPREVIOUS},	/* 8723AU */
    {0x8725, 0x1200, "mp_rtl8723a_fw", "rtl8723a_fw", "rtl8723a_config", NULL, 0, RTLPREVIOUS},	/* 8723AU */
    {0x872A, 0x1200, "mp_rtl8723a_fw", "rtl8723a_fw", "rtl8723a_config", NULL, 0, RTLPREVIOUS},	/* 8723AU */
    {0x872B, 0x1200, "mp_rtl8723a_fw", "rtl8723a_fw", "rtl8723a_config", NULL, 0, RTLPREVIOUS},	/* 8723AU */

    {0xb720, 0x8723, "mp_rtl8723b_fw", "rtl8723b_fw", "rtl8723b_config", NULL, 0, RTLPREVIOUS},	/* RTL8723BU */
    {0xb72A, 0x8723, "mp_rtl8723b_fw", "rtl8723b_fw", "rtl8723b_config", NULL, 0, RTLPREVIOUS},	/* RTL8723BU */
    {0xb728, 0x8723, "mp_rtl8723b_fw", "rtl8723b_fw", "rtl8723b_config", NULL, 0, RTLPREVIOUS},	/* RTL8723BE for LC */
    {0xb723, 0x8723, "mp_rtl8723b_fw", "rtl8723b_fw", "rtl8723b_config", NULL, 0, RTLPREVIOUS},	/* RTL8723BE */
    {0xb72B, 0x8723, "mp_rtl8723b_fw", "rtl8723b_fw", "rtl8723b_config", NULL, 0, RTLPREVIOUS},	/* RTL8723BE */
    {0xb001, 0x8723, "mp_rtl8723b_fw", "rtl8723b_fw", "rtl8723b_config", NULL, 0, RTLPREVIOUS},	/* RTL8723BE for HP */
    {0xb002, 0x8723, "mp_rtl8723b_fw", "rtl8723b_fw", "rtl8723b_config", NULL, 0, RTLPREVIOUS},	/* RTL8723BE */
    {0xb003, 0x8723, "mp_rtl8723b_fw", "rtl8723b_fw", "rtl8723b_config", NULL, 0, RTLPREVIOUS},	/* RTL8723BE */
    {0xb004, 0x8723, "mp_rtl8723b_fw", "rtl8723b_fw", "rtl8723b_config", NULL, 0, RTLPREVIOUS},	/* RTL8723BE */
    {0xb005, 0x8723, "mp_rtl8723b_fw", "rtl8723b_fw", "rtl8723b_config", NULL, 0, RTLPREVIOUS},	/* RTL8723BE */

    {0x3410, 0x8723, "mp_rtl8723b_fw", "rtl8723b_fw", "rtl8723b_config", NULL, 0, RTLPREVIOUS},	/* RTL8723BE for Azurewave */
    {0x3416, 0x8723, "mp_rtl8723b_fw", "rtl8723b_fw", "rtl8723b_config", NULL, 0, RTLPREVIOUS},	/* RTL8723BE for Azurewave */
    {0x3459, 0x8723, "mp_rtl8723b_fw", "rtl8723b_fw", "rtl8723b_config", NULL, 0, RTLPREVIOUS},	/* RTL8723BE for Azurewave */
    {0xE085, 0x8723, "mp_rtl8723b_fw", "rtl8723b_fw", "rtl8723b_config", NULL, 0, RTLPREVIOUS},	/* RTL8723BE for Foxconn */
    {0xE08B, 0x8723, "mp_rtl8723b_fw", "rtl8723b_fw", "rtl8723b_config", NULL, 0, RTLPREVIOUS},	/* RTL8723BE for Foxconn */
    {0xE09E, 0x8723, "mp_rtl8723b_fw", "rtl8723b_fw", "rtl8723b_config", NULL, 0, RTLPREVIOUS},	/* RTL8723BE for Foxconn */

    {0xA761, 0x8761, "mp_rtl8761a_fw", "rtl8761au_fw", "rtl8761a_config", NULL, 0, RTLPREVIOUS},	/* RTL8761AU only */
    {0x818B, 0x8761, "mp_rtl8761a_fw", "rtl8761aw_fw", "rtl8761aw_config", NULL, 0, RTLPREVIOUS},	/* RTL8761AW + 8192EU */
    {0x818C, 0x8761, "mp_rtl8761a_fw", "rtl8761aw_fw", "rtl8761aw_config", NULL, 0, RTLPREVIOUS},	/* RTL8761AW + 8192EU */
    {0x8760, 0x8761, "mp_rtl8761a_fw", "rtl8761au8192ee_fw", "rtl8761a_config", NULL, 0, RTLPREVIOUS},	/* RTL8761AU + 8192EE */
    {0xB761, 0x8761, "mp_rtl8761a_fw", "rtl8761au_fw", "rtl8761a_config", NULL, 0, RTLPREVIOUS},	/* RTL8761AUV only */
    {0x8761, 0x8761, "mp_rtl8761a_fw", "rtl8761au8192ee_fw", "rtl8761a_config", NULL, 0, RTLPREVIOUS},	/* RTL8761AU + 8192EE for LI */
    {0x8A60, 0x8761, "mp_rtl8761a_fw", "rtl8761au8812ae_fw", "rtl8761a_config", NULL, 0, RTLPREVIOUS},	/* RTL8761AU + 8812AE */
    {0x3527, 0x8761, "mp_rtl8761a_fw", "rtl8761au8192ee_fw", "rtl8761a_config", NULL, 0, RTLPREVIOUS},	/* RTL8761AU + 8814AE */

    {0x8821, 0x8821, "mp_rtl8821a_fw", "rtl8821a_fw", "rtl8821a_config", NULL, 0, RTLPREVIOUS},	/* RTL8821AE */
    {0x0821, 0x8821, "mp_rtl8821a_fw", "rtl8821a_fw", "rtl8821a_config", NULL, 0, RTLPREVIOUS},	/* RTL8821AE */
    {0x0823, 0x8821, "mp_rtl8821a_fw", "rtl8821a_fw", "rtl8821a_config", NULL, 0, RTLPREVIOUS},	/* RTL8821AU */
    {0x3414, 0x8821, "mp_rtl8821a_fw", "rtl8821a_fw", "rtl8821a_config", NULL, 0, RTLPREVIOUS},	/* RTL8821AE */
    {0x3458, 0x8821, "mp_rtl8821a_fw", "rtl8821a_fw", "rtl8821a_config", NULL, 0, RTLPREVIOUS},	/* RTL8821AE */
    {0x3461, 0x8821, "mp_rtl8821a_fw", "rtl8821a_fw", "rtl8821a_config", NULL, 0, RTLPREVIOUS},	/* RTL8821AE */
    {0x3462, 0x8821, "mp_rtl8821a_fw", "rtl8821a_fw", "rtl8821a_config", NULL, 0, RTLPREVIOUS},	/* RTL8821AE */

    {0xb82c, 0x8822, "mp_rtl8822bu_fw", "rtl8822bu_fw", "rtl8822bu_config", NULL, 0, RTL8822BU}, /* RTL8822BU */

    {0xd720, 0x8723, "mp_rtl8723du_fw", "rtl8723du_fw", "rtl8723du_config", NULL, 0, RTL8723DU}, /* RTL8723DU */
    {0xd723, 0x8723, "mp_rtl8723du_fw", "rtl8723du_fw", "rtl8723du_config", NULL, 0, RTL8723DU}, /* RTL8723DU */
    {0xd739, 0x8723, "mp_rtl8723du_fw", "rtl8723du_fw", "rtl8723du_config", NULL, 0, RTL8723DU}, /* RTL8723DU */
    {0xb009, 0x8723, "mp_rtl8723du_fw", "rtl8723du_fw", "rtl8723du_config", NULL, 0, RTL8723DU}, /* RTL8723DU */
    {0x0231, 0x8723, "mp_rtl8723du_fw", "rtl8723du_fw", "rtl8723du_config", NULL, 0, RTL8723DU}, /* RTL8723DU for LiteOn */

    {0xb820, 0x8821, "mp_rtl8821cu_fw", "rtl8821cu_fw", "rtl8821cu_config", NULL, 0, RTL8821CU}, /* RTL8821CU */
    {0xc820, 0x8821, "mp_rtl8821cu_fw", "rtl8821cu_fw", "rtl8821cu_config", NULL, 0, RTL8821CU}, /* RTL8821CU */
    {0xc821, 0x8821, "mp_rtl8821cu_fw", "rtl8821cu_fw", "rtl8821cu_config", NULL, 0, RTL8821CU}, /* RTL8821CE */
    {0xc823, 0x8821, "mp_rtl8821cu_fw", "rtl8821cu_fw", "rtl8821cu_config", NULL, 0, RTL8821CU}, /* RTL8821CE */
    {0xc824, 0x8821, "mp_rtl8821cu_fw", "rtl8821cu_fw", "rtl8821cu_config", NULL, 0, RTL8821CU}, /* RTL8821CE */
    {0xc825, 0x8821, "mp_rtl8821cu_fw", "rtl8821cu_fw", "rtl8821cu_config", NULL, 0, RTL8821CU}, /* RTL8821CE */
    {0xc827, 0x8821, "mp_rtl8821cu_fw", "rtl8821cu_fw", "rtl8821cu_config", NULL, 0, RTL8821CU}, /* RTL8821CE */
    {0xc025, 0x8821, "mp_rtl8821cu_fw", "rtl8821cu_fw", "rtl8821cu_config", NULL, 0, RTL8821CU}, /* RTL8821CE */
    {0xc024, 0x8821, "mp_rtl8821cu_fw", "rtl8821cu_fw", "rtl8821cu_config", NULL, 0, RTL8821CU}, /* RTL8821CE */
    {0xc030, 0x8821, "mp_rtl8821cu_fw", "rtl8821cu_fw", "rtl8821cu_config", NULL, 0, RTL8821CU}, /* RTL8821CE */
    {0xb00a, 0x8821, "mp_rtl8821cu_fw", "rtl8821cu_fw", "rtl8821cu_config", NULL, 0, RTL8821CU}, /* RTL8821CE */
    {0xb00e, 0x8821, "mp_rtl8821cu_fw", "rtl8821cu_fw", "rtl8821cu_config", NULL, 0, RTL8821CU}, /* RTL8821CE */
    {0xc032, 0x8821, "mp_rtl8821cu_fw", "rtl8821cu_fw", "rtl8821cu_config", NULL, 0, RTL8821CU}, /* RTL8821CE */
    {0x4000, 0x8821, "mp_rtl8821cu_fw", "rtl8821cu_fw", "rtl8821cu_config", NULL, 0, RTL8821CU}, /* RTL8821CE for LiteOn */
    {0x4001, 0x8821, "mp_rtl8821cu_fw", "rtl8821cu_fw", "rtl8821cu_config", NULL, 0, RTL8821CU}, /* RTL8821CE for LiteOn */
    {0x3529, 0x8821, "mp_rtl8821cu_fw", "rtl8821cu_fw", "rtl8821cu_config", NULL, 0, RTL8821CU}, /* RTL8821CE for Azurewave */
    {0x3530, 0x8821, "mp_rtl8821cu_fw", "rtl8821cu_fw", "rtl8821cu_config", NULL, 0, RTL8821CU}, /* RTL8821CE for Azurewave */
    {0x3532, 0x8821, "mp_rtl8821cu_fw", "rtl8821cu_fw", "rtl8821cu_config", NULL, 0, RTL8821CU}, /* RTL8821CE for Azurewave */
    {0x3533, 0x8821, "mp_rtl8821cu_fw", "rtl8821cu_fw", "rtl8821cu_config", NULL, 0, RTL8821CU}, /* RTL8821CE for Azurewave */
    {0x3538, 0x8821, "mp_rtl8821cu_fw", "rtl8821cu_fw", "rtl8821cu_config", NULL, 0, RTL8821CU}, /* RTL8821CE for Azurewave */
    {0x3539, 0x8821, "mp_rtl8821cu_fw", "rtl8821cu_fw", "rtl8821cu_config", NULL, 0, RTL8821CU}, /* RTL8821CE for Azurewave */
    {0x3558, 0x8821, "mp_rtl8821cu_fw", "rtl8821cu_fw", "rtl8821cu_config", NULL, 0, RTL8821CU}, /* RTL8821CE for Azurewave */
    {0x3559, 0x8821, "mp_rtl8821cu_fw", "rtl8821cu_fw", "rtl8821cu_config", NULL, 0, RTL8821CU}, /* RTL8821CE for Azurewave */
    {0x3581, 0x8821, "mp_rtl8821cu_fw", "rtl8821cu_fw", "rtl8821cu_config", NULL, 0, RTL8821CU}, /* RTL8821CE for Azurewave */
    {0x3540, 0x8821, "mp_rtl8821cu_fw", "rtl8821cu_fw", "rtl8821cu_config", NULL, 0, RTL8821CU}, /* RTL8821CE */
    {0x3541, 0x8821, "mp_rtl8821cu_fw", "rtl8821cu_fw", "rtl8821cu_config", NULL, 0, RTL8821CU}, /* RTL8821CE for GSD */
    {0x3543, 0x8821, "mp_rtl8821cu_fw", "rtl8821cu_fw", "rtl8821cu_config", NULL, 0, RTL8821CU}, /* RTL8821CE for GSD */
    {0xc80c, 0x8821, "mp_rtl8821cu_fw", "rtl8821cu_fw", "rtl8821cu_config", NULL, 0, RTL8821CU}, /* RTL8821CUH */

    {0xc82c, 0x8822, "mp_rtl8822cu_fw", "rtl8822cu_fw", "rtl8822cu_config", NULL, 0, RTL8822CU}, /* RTL8822CU */
    {0xc82e, 0x8822, "mp_rtl8822cu_fw", "rtl8822cu_fw", "rtl8822cu_config", NULL, 0, RTL8822CU}, /* RTL8822CU */
    {0xc81d, 0x8822, "mp_rtl8822cu_fw", "rtl8822cu_fw", "rtl8822cu_config", NULL, 0, RTL8822CU}, /* RTL8822CU */
    {0xd820, 0x8822, "mp_rtl8821du_fw", "rtl8821du_fw", "rtl8821du_config", NULL, 0, RTL8822CU}, /* RTL8821DU */

    {0xc822, 0x8822, "mp_rtl8822cu_fw", "rtl8822cu_fw", "rtl8822cu_config", NULL, 0, RTL8822CU}, /* RTL8822CE */
    {0xc82b, 0x8822, "mp_rtl8822cu_fw", "rtl8822cu_fw", "rtl8822cu_config", NULL, 0, RTL8822CU}, /* RTL8822CE */
    {0xb00c, 0x8822, "mp_rtl8822cu_fw", "rtl8822cu_fw", "rtl8822cu_config", NULL, 0, RTL8822CU}, /* RTL8822CE */
    {0xb00d, 0x8822, "mp_rtl8822cu_fw", "rtl8822cu_fw", "rtl8822cu_config", NULL, 0, RTL8822CU}, /* RTL8822CE */
    {0xc123, 0x8822, "mp_rtl8822cu_fw", "rtl8822cu_fw", "rtl8822cu_config", NULL, 0, RTL8822CU}, /* RTL8822CE */
    {0xc126, 0x8822, "mp_rtl8822cu_fw", "rtl8822cu_fw", "rtl8822cu_config", NULL, 0, RTL8822CU}, /* RTL8822CE */
    {0xc127, 0x8822, "mp_rtl8822cu_fw", "rtl8822cu_fw", "rtl8822cu_config", NULL, 0, RTL8822CU}, /* RTL8822CE */
    {0xc128, 0x8822, "mp_rtl8822cu_fw", "rtl8822cu_fw", "rtl8822cu_config", NULL, 0, RTL8822CU}, /* RTL8822CE */
    {0xc129, 0x8822, "mp_rtl8822cu_fw", "rtl8822cu_fw", "rtl8822cu_config", NULL, 0, RTL8822CU}, /* RTL8822CE */
    {0xc131, 0x8822, "mp_rtl8822cu_fw", "rtl8822cu_fw", "rtl8822cu_config", NULL, 0, RTL8822CU}, /* RTL8822CE */
    {0xc136, 0x8822, "mp_rtl8822cu_fw", "rtl8822cu_fw", "rtl8822cu_config", NULL, 0, RTL8822CU}, /* RTL8822CE */
    {0x3549, 0x8822, "mp_rtl8822cu_fw", "rtl8822cu_fw", "rtl8822cu_config", NULL, 0, RTL8822CU}, /* RTL8822CE for Azurewave */
    {0x3548, 0x8822, "mp_rtl8822cu_fw", "rtl8822cu_fw", "rtl8822cu_config", NULL, 0, RTL8822CU}, /* RTL8822CE for Azurewave */
    {0xc125, 0x8822, "mp_rtl8822cu_fw", "rtl8822cu_fw", "rtl8822cu_config", NULL, 0, RTL8822CU}, /* RTL8822CE */
    {0x4005, 0x8822, "mp_rtl8822cu_fw", "rtl8822cu_fw", "rtl8822cu_config", NULL, 0, RTL8822CU}, /* RTL8822CE for LiteOn */
    {0x3051, 0x8822, "mp_rtl8822cu_fw", "rtl8822cu_fw", "rtl8822cu_config", NULL, 0, RTL8822CU}, /* RTL8822CE for LiteOn */
    {0x18ef, 0x8822, "mp_rtl8822cu_fw", "rtl8822cu_fw", "rtl8822cu_config", NULL, 0, RTL8822CU}, /* RTL8822CE */
    {0x161f, 0x8822, "mp_rtl8822cu_fw", "rtl8822cu_fw", "rtl8822cu_config", NULL, 0, RTL8822CU}, /* RTL8822CE */
    {0x3053, 0x8822, "mp_rtl8822cu_fw", "rtl8822cu_fw", "rtl8822cu_config", NULL, 0, RTL8822CU}, /* RTL8822CE */
    {0xc547, 0x8822, "mp_rtl8822cu_fw", "rtl8822cu_fw", "rtl8822cu_config", NULL, 0, RTL8822CU}, /* RTL8822CE */
    {0x3553, 0x8822, "mp_rtl8822cu_fw", "rtl8822cu_fw", "rtl8822cu_config", NULL, 0, RTL8822CU}, /* RTL8822CE */
    {0x3555, 0x8822, "mp_rtl8822cu_fw", "rtl8822cu_fw", "rtl8822cu_config", NULL, 0, RTL8822CU}, /* RTL8822CE */
    {0xc82f, 0x8822, "mp_rtl8822cu_fw", "rtl8822cu_fw", "rtl8822cu_config", NULL, 0, RTL8822CU}, /* RTL8822CE-VS */
    {0xc02f, 0x8822, "mp_rtl8822cu_fw", "rtl8822cu_fw", "rtl8822cu_config", NULL, 0, RTL8822CU}, /* RTL8822CE-VS */
    {0xc03f, 0x8822, "mp_rtl8822cu_fw", "rtl8822cu_fw", "rtl8822cu_config", NULL, 0, RTL8822CU}, /* RTL8822CE-VS */

    {0x8771, 0x8761, "mp_rtl8761b_fw", "rtl8761bu_fw", "rtl8761bu_config", NULL, 0, RTL8761BU}, /* RTL8761BU only */
    {0xa725, 0x8761, "mp_rtl8761b_fw", "rtl8725au_fw", "rtl8725au_config", NULL, 0, RTL8761BU}, /* RTL8725AU */
    {0xa72A, 0x8761, "mp_rtl8761b_fw", "rtl8725au_fw", "rtl8725au_config", NULL, 0, RTL8761BU}, /* RTL8725AU BT only */

    {0x885a, 0x8852, "mp_rtl8852au_fw", "rtl8852au_fw", "rtl8852au_config", NULL, 0, RTL8852AU}, /* RTL8852AU */
    {0x8852, 0x8852, "mp_rtl8852au_fw", "rtl8852au_fw", "rtl8852au_config", NULL, 0, RTL8852AU}, /* RTL8852AE */
    {0xa852, 0x8852, "mp_rtl8852au_fw", "rtl8852au_fw", "rtl8852au_config", NULL, 0, RTL8852AU}, /* RTL8852AE */
    {0x2852, 0x8852, "mp_rtl8852au_fw", "rtl8852au_fw", "rtl8852au_config", NULL, 0, RTL8852AU}, /* RTL8852AE */
    {0x385a, 0x8852, "mp_rtl8852au_fw", "rtl8852au_fw", "rtl8852au_config", NULL, 0, RTL8852AU}, /* RTL8852AE */
    {0x3852, 0x8852, "mp_rtl8852au_fw", "rtl8852au_fw", "rtl8852au_config", NULL, 0, RTL8852AU}, /* RTL8852AE */
    {0x1852, 0x8852, "mp_rtl8852au_fw", "rtl8852au_fw", "rtl8852au_config", NULL, 0, RTL8852AU}, /* RTL8852AE */
    {0x4852, 0x8852, "mp_rtl8852au_fw", "rtl8852au_fw", "rtl8852au_config", NULL, 0, RTL8852AU}, /* RTL8852AE */
    {0x4006, 0x8852, "mp_rtl8852au_fw", "rtl8852au_fw", "rtl8852au_config", NULL, 0, RTL8852AU}, /* RTL8852AE */
    {0x3561, 0x8852, "mp_rtl8852au_fw", "rtl8852au_fw", "rtl8852au_config", NULL, 0, RTL8852AU}, /* RTL8852AE */
    {0x3562, 0x8852, "mp_rtl8852au_fw", "rtl8852au_fw", "rtl8852au_config", NULL, 0, RTL8852AU}, /* RTL8852AE */
    {0x588a, 0x8852, "mp_rtl8852au_fw", "rtl8852au_fw", "rtl8852au_config", NULL, 0, RTL8852AU}, /* RTL8852AE */
    {0x589a, 0x8852, "mp_rtl8852au_fw", "rtl8852au_fw", "rtl8852au_config", NULL, 0, RTL8852AU}, /* RTL8852AE */
    {0x590a, 0x8852, "mp_rtl8852au_fw", "rtl8852au_fw", "rtl8852au_config", NULL, 0, RTL8852AU}, /* RTL8852AE */
    {0xc125, 0x8852, "mp_rtl8852au_fw", "rtl8852au_fw", "rtl8852au_config", NULL, 0, RTL8852AU}, /* RTL8852AE */
    {0xe852, 0x8852, "mp_rtl8852au_fw", "rtl8852au_fw", "rtl8852au_config", NULL, 0, RTL8852AU}, /* RTL8852AE */
    {0xb852, 0x8852, "mp_rtl8852au_fw", "rtl8852au_fw", "rtl8852au_config", NULL, 0, RTL8852AU}, /* RTL8852AE */
    {0xc852, 0x8852, "mp_rtl8852au_fw", "rtl8852au_fw", "rtl8852au_config", NULL, 0, RTL8852AU}, /* RTL8852AE */
    {0xc549, 0x8852, "mp_rtl8852au_fw", "rtl8852au_fw", "rtl8852au_config", NULL, 0, RTL8852AU}, /* RTL8852AE */
    {0xc127, 0x8852, "mp_rtl8852au_fw", "rtl8852au_fw", "rtl8852au_config", NULL, 0, RTL8852AU}, /* RTL8852AE */
    {0x3565, 0x8852, "mp_rtl8852au_fw", "rtl8852au_fw", "rtl8852au_config", NULL, 0, RTL8852AU}, /* RTL8852AE */

    {0xb733, 0x8723, "mp_rtl8723fu_fw", "rtl8723fu_fw", "rtl8723fu_config", NULL, 0, RTL8723FU}, /* RTL8723FU */
    {0xb73a, 0x8723, "mp_rtl8723fu_fw", "rtl8723fu_fw", "rtl8723fu_config", NULL, 0, RTL8723FU}, /* RTL8723FU */
    {0xf72b, 0x8723, "mp_rtl8723fu_fw", "rtl8723fu_fw", "rtl8723fu_config", NULL, 0, RTL8723FU}, /* RTL8723FU */

    {0x8851, 0x8852, "mp_rtl8851au_fw", "rtl8851au_fw", "rtl8851au_config", NULL, 0, RTL8852BU}, /* RTL8851AU */
    {0xa85b, 0x8852, "mp_rtl8852bu_fw", "rtl8852bu_fw", "rtl8852bu_config", NULL, 0, RTL8852BU}, /* RTL8852BU */
    {0xb85b, 0x8852, "mp_rtl8852bu_fw", "rtl8852bu_fw", "rtl8852bu_config", NULL, 0, RTL8852BU}, /* RTL8852BE */
    {0xb85c, 0x8852, "mp_rtl8852bu_fw", "rtl8852bu_fw", "rtl8852bu_config", NULL, 0, RTL8852BU}, /* RTL8852BE */
    {0x3571, 0x8852, "mp_rtl8852bu_fw", "rtl8852bu_fw", "rtl8852bu_config", NULL, 0, RTL8852BU}, /* RTL8852BE */
    {0x3570, 0x8852, "mp_rtl8852bu_fw", "rtl8852bu_fw", "rtl8852bu_config", NULL, 0, RTL8852BU}, /* RTL8852BE */
    {0x3572, 0x8852, "mp_rtl8852bu_fw", "rtl8852bu_fw", "rtl8852bu_config", NULL, 0, RTL8852BU}, /* RTL8852BE */
    {0x4b06, 0x8852, "mp_rtl8852bu_fw", "rtl8852bu_fw", "rtl8852bu_config", NULL, 0, RTL8852BU}, /* RTL8852BE */
    {0x885b, 0x8852, "mp_rtl8852bu_fw", "rtl8852bu_fw", "rtl8852bu_config", NULL, 0, RTL8852BU}, /* RTL8852BE */
    {0x886b, 0x8852, "mp_rtl8852bu_fw", "rtl8852bu_fw", "rtl8852bu_config", NULL, 0, RTL8852BU}, /* RTL8852BE */
    {0x887b, 0x8852, "mp_rtl8852bu_fw", "rtl8852bu_fw", "rtl8852bu_config", NULL, 0, RTL8852BU}, /* RTL8852BE */
    {0xc559, 0x8852, "mp_rtl8852bu_fw", "rtl8852bu_fw", "rtl8852bu_config", NULL, 0, RTL8852BU}, /* RTL8852BE */
    {0xb052, 0x8852, "mp_rtl8852bu_fw", "rtl8852bu_fw", "rtl8852bu_config", NULL, 0, RTL8852BU}, /* RTL8852BE */
    {0xb152, 0x8852, "mp_rtl8852bu_fw", "rtl8852bu_fw", "rtl8852bu_config", NULL, 0, RTL8852BU}, /* RTL8852BE */
    {0xb252, 0x8852, "mp_rtl8852bu_fw", "rtl8852bu_fw", "rtl8852bu_config", NULL, 0, RTL8852BU}, /* RTL8852BE */
    {0x4853, 0x8852, "mp_rtl8852bu_fw", "rtl8852bu_fw", "rtl8852bu_config", NULL, 0, RTL8852BU}, /* RTL8852BE */
    {0x1670, 0x8852, "mp_rtl8852bu_fw", "rtl8852bu_fw", "rtl8852bu_config", NULL, 0, RTL8852BU}, /* RTL8852BE */

    {0xc85a, 0x8852, "mp_rtl8852cu_fw", "rtl8852cu_fw", "rtl8852cu_config", NULL, 0, RTL8852CU}, /* RTL8852CU */
    {0x0852, 0x8852, "mp_rtl8852cu_fw", "rtl8852cu_fw", "rtl8852cu_config", NULL, 0, RTL8852CU}, /* RTL8852CE */
    {0x5852, 0x8852, "mp_rtl8852cu_fw", "rtl8852cu_fw", "rtl8852cu_config", NULL, 0, RTL8852CU}, /* RTL8852CE */
    {0xc85c, 0x8852, "mp_rtl8852cu_fw", "rtl8852cu_fw", "rtl8852cu_config", NULL, 0, RTL8852CU}, /* RTL8852CE */
    {0x885c, 0x8852, "mp_rtl8852cu_fw", "rtl8852cu_fw", "rtl8852cu_config", NULL, 0, RTL8852CU}, /* RTL8852CE */
    {0x886c, 0x8852, "mp_rtl8852cu_fw", "rtl8852cu_fw", "rtl8852cu_config", NULL, 0, RTL8852CU}, /* RTL8852CE */
    {0x887c, 0x8852, "mp_rtl8852cu_fw", "rtl8852cu_fw", "rtl8852cu_config", NULL, 0, RTL8852CU}, /* RTL8852CE */
    {0x4007, 0x8852, "mp_rtl8852cu_fw", "rtl8852cu_fw", "rtl8852cu_config", NULL, 0, RTL8852CU}, /* RTL8852CE */

    {0xe822, 0x8822, "mp_rtl8822eu_fw", "rtl8822eu_fw", "rtl8822eu_config", NULL, 0, RTL8822EU}, /* RTL8822EU */
    {0xa82a, 0x8822, "mp_rtl8822eu_fw", "rtl8822eu_fw", "rtl8822eu_config", NULL, 0, RTL8822EU}, /* RTL8822EU */

    {0xb851, 0x8851, "mp_rtl8851bu_fw", "rtl8851bu_fw", "rtl8851bu_config", NULL, 0, RTL8851BU}, /* RTL8851BU */

/* NOTE: must append patch entries above the null entry */
    {0, 0, NULL, NULL, NULL, NULL, 0, 0}
};

static uint16_t project_id[] = {
    ROM_LMP_8723a, ROM_LMP_8723b, ROM_LMP_8821a, ROM_LMP_8761a, ROM_LMP_NONE,
    ROM_LMP_NONE,  ROM_LMP_NONE,  ROM_LMP_NONE,  ROM_LMP_8822b, ROM_LMP_8723b, /* RTL8723DU */
    ROM_LMP_8821a,                                                             /* RTL8821CU */
    ROM_LMP_NONE,  ROM_LMP_NONE,  ROM_LMP_8822b,                               /* RTL8822CU */
    ROM_LMP_8761a,                                                             /* index 14 for 8761BU */
    ROM_LMP_NONE, ROM_LMP_NONE, ROM_LMP_NONE, ROM_LMP_8852a,                   /* index 18 for 8852AU */
    ROM_LMP_8723b,                                                             /* index 19 for 8723FU */
    ROM_LMP_8852a,                                                             /* index 20 for 8852BU */
    ROM_LMP_NONE, ROM_LMP_NONE, ROM_LMP_NONE, ROM_LMP_NONE, ROM_LMP_8852a,     /* index 25 for 8852CU */
    ROM_LMP_NONE, ROM_LMP_NONE, ROM_LMP_NONE, ROM_LMP_NONE, ROM_LMP_NONE,
    ROM_LMP_NONE, ROM_LMP_NONE, ROM_LMP_8822b,                                 /* index 33 for 8822EU */
    ROM_LMP_NONE, ROM_LMP_NONE, ROM_LMP_8851b,                                 /* index 36 for 8851BU */
};

static btstack_packet_callback_registration_t hci_event_callback_registration;
static uint8_t                                state;
static uint8_t                                rom_version;
static uint16_t                               lmp_subversion;
static uint16_t                               product_id;
static const patch_info *                     patch;
static uint8_t                                g_key_id = 0;

#ifdef HAVE_POSIX_FILE_IO
static const char *firmware_folder_path = ".";
static const char *firmware_file_path;
static const char *config_folder_path = ".";
static const char *config_file_path;
static char        firmware_file[1000];
static char        config_file[1000];
#endif

static const uint8_t FW_SIGNATURE[8]        = {0x52, 0x65, 0x61, 0x6C, 0x74, 0x65, 0x63, 0x68};
static const uint8_t FW_SIGNATURE_NEW[8]    = {0x52, 0x54, 0x42, 0x54, 0x43, 0x6F, 0x72, 0x65};
static const uint8_t EXTENSION_SIGNATURE[4] = {0x51, 0x04, 0xFD, 0x77};

static void hci_packet_handler(uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size) {
    UNUSED(channel);
    UNUSED(size);
    if (packet_type != HCI_EVENT_PACKET) {
        return;
    }
    if (hci_event_packet_get_type(packet) != HCI_EVENT_COMMAND_COMPLETE) {
        return;
    }

    uint16_t opcode = hci_event_command_complete_get_command_opcode(packet);
    const uint8_t * return_para = hci_event_command_complete_get_return_parameters(packet);
    switch (opcode) {
        case HCI_OPCODE_HCI_READ_LOCAL_VERSION_INFORMATION:
            lmp_subversion = little_endian_read_16(packet, 12);
            break;
        case HCI_OPCODE_HCI_RTK_READ_ROM_VERSION:
            rom_version = return_para[1];
            log_info("Received ROM version 0x%02x", rom_version);
            printf("Realtek: Received ROM version 0x%02x\n", rom_version);
            if (patch->lmp_sub != lmp_subversion) {
                printf("Realtek: Firmware already exists\n");
                state = STATE_PHASE_2_DONE;
            }
            break;
        case HCI_OPCODE_HCI_RTK_READ_CARD_INFO:
            switch (state){
                case STATE_PHASE_1_W4_READ_LMP_SUBVERSION:
                    log_info("Read Card: LMP Subversion");
                    if (little_endian_read_16(hci_event_command_complete_get_return_parameters(packet), 1) == 0x8822){
                        state = STATE_PHASE_1_READ_HCI_REVISION;
                    } else {
                        state = STATE_PHASE_1_DONE;
                    }
                    break;
                case STATE_PHASE_1_W4_READ_HCI_REVISION:
                    log_info("Read Card: HCI Revision");
                    if (little_endian_read_16(hci_event_command_complete_get_return_parameters(packet), 1) == 0x000e){
                        state = STATE_PHASE_2_READ_ROM_VERSION;
                    } else {
                        state = STATE_PHASE_1_DONE;
                    }
                    break;
                case STATE_PHASE_2_W4_SEC_PROJ:
                    g_key_id = return_para[1];
                    printf("Realtek: Received key id 0x%02x\n", g_key_id);
                    state = STATE_PHASE_2_LOAD_FIRMWARE;
                    break;
                default:
                    btstack_assert(false);
                    break;
            }
            break;
        default:
            break;
    }
}

static void chipset_init(const void *config) {
    UNUSED(config);

    // pre-set lmp subversion: HCI starts custom download only if HCI Version = 0x00e, and LMP Subversion = 0x8822
    lmp_subversion = 0x8822;

#ifdef HAVE_POSIX_FILE_IO
    // determine file path
    if (firmware_file_path == NULL || config_file_path == NULL) {
        log_info("firmware or config file path is empty. Using product id 0x%04x!", product_id);
        patch = NULL;
        for (uint16_t i = 0; i < sizeof(fw_patch_table) / sizeof(patch_info); i++) {
            if (fw_patch_table[i].prod_id == product_id) {
                patch = &fw_patch_table[i];
                break;
            }
        }
        if (patch == NULL) {
            log_info("Product id 0x%04x is unknown", product_id);
            state = STATE_PHASE_2_DONE;
            return;
        }
        btstack_snprintf_assert_complete(firmware_file, sizeof(firmware_file), "%s/%s", firmware_folder_path, patch->patch_name);
        btstack_snprintf_assert_complete(config_file, sizeof(config_file), "%s/%s", config_folder_path, patch->config_name);
        firmware_file_path = &firmware_file[0];
        config_file_path   = &config_file[0];
        //lmp_subversion     = patch->lmp_sub;
    }
    log_info("Using firmware '%s' and config '%s'", firmware_file_path, config_file_path);
    printf("Realtek: Using firmware '%s' and config '%s'\n", firmware_file_path, config_file_path);

    // activate hci callback
    hci_event_callback_registration.callback = &hci_packet_handler;
    hci_add_event_handler(&hci_event_callback_registration);
    state = STATE_PHASE_1_READ_LMP_SUBVERSION;
#endif
}

#ifdef HAVE_POSIX_FILE_IO

/**
 * @brief Opens the specified file and stores content to an allocated buffer
 *
 * @param file
 * @param buf
 * @param name
 * @return uint32_t Length of file
 */
static uint32_t read_file(FILE **file, uint8_t **buf, const char *name) {
    uint32_t size;

    // open file
    *file = fopen(name, "rb");
    if (*file == NULL) {
        log_info("Failed to open file %s", name);
        return 0;
    }

    // determine length of file
    fseek(*file, 0, SEEK_END);
    size = ftell(*file);
    fseek(*file, 0, SEEK_SET);
    if (size <= 0) {
        return 0;
    }

    // allocate memory
    *buf = malloc(size);
    if (*buf == NULL) {
        fclose(*file);
        *file = NULL;
        log_info("Failed to allocate %u bytes for file %s", size, name);
        return 0;
    }

    // read file
    size_t ret = fread(*buf, size, 1, *file);
    if (ret != 1) {
        log_info("Failed to read %u bytes from file %s (ret = %d)", size, name, (int) ret);
        fclose(*file);
        free(*buf);
        *file = NULL;
        *buf  = NULL;
        return 0;
    }

    log_info("Opened file %s and read %u bytes", name, size);
    return size;
}

static void finalize_file_and_buffer(FILE **file, uint8_t **buffer) {
    fclose(*file);
    free(*buffer);
    *buffer = NULL;
    *file   = NULL;
}

static uint8_t rtk_get_fw_project_id(uint8_t * p_buf)
{
    uint8_t opcode;
    uint8_t len;
    uint8_t data = 0;

    do {
        opcode = *p_buf;
        len = *(p_buf - 1);
        if (opcode == 0x00) {
            if (len == 1) {
                data = *(p_buf - 2);
                log_info
                    ("rtk_get_fw_project_id: opcode %d, len %d, data %d",
                     opcode, len, data);
                break;
            } else {
                log_error
                    ("rtk_get_fw_project_id: invalid len %d",
                     len);
            }
        }
        p_buf -= len + 2;
    } while (*p_buf != 0xFF);

    return data;
}

struct rtb_ota_flag {
    uint8_t eco;
    uint8_t enable;
    uint16_t reserve;
};

struct patch_node {
    btstack_linked_item_t item;
    uint8_t eco;
    uint8_t pri;
    uint8_t key_id;
    uint8_t reserve;
    uint32_t len;
    uint8_t *payload;
};

/* Add a node to alist that is in ascending order. */
static void insert_queue_sort(btstack_linked_list_t * list, struct patch_node *node)
{
    btstack_assert(list != NULL);
    btstack_assert(node != NULL);

    struct patch_node *next;
    btstack_linked_item_t *it;

    for (it = (btstack_linked_item_t *) list; it->next ; it = it->next){
        next = (struct patch_node *) it->next;
        if(next->pri >= node->pri) {
            break;
        }
    }
    node->item.next = it->next;
    it->next = (btstack_linked_item_t *) node;
}

static int insert_patch(btstack_linked_list_t * patch_list, uint8_t *section_pos,
        uint32_t opcode, uint32_t *patch_len, uint8_t *sec_flag)
{
    struct patch_node *tmp;
    uint32_t i;
    uint32_t numbers;
    uint32_t section_len = 0;
    uint8_t eco = 0;
    uint8_t *pos = section_pos + 8;

    numbers = little_endian_read_16(pos, 0);
    log_info("number 0x%04x", numbers);

    pos += 4;
    for (i = 0; i < numbers; i++) {
        eco = (uint8_t)*(pos);
        log_info("eco 0x%02x, Eversion:%02x", eco, rom_version);
        if (eco == rom_version + 1) {
            //tmp = (struct patch_node*)kzalloc(sizeof(struct patch_node), GFP_KERNEL);
            tmp = (struct patch_node*)malloc(sizeof(struct patch_node));
            tmp->pri = (uint8_t)*(pos + 1);
            if(opcode == PATCH_SECURITY_HEADER)
                tmp->key_id = (uint8_t)*(pos + 1);

            section_len = little_endian_read_32(pos, 4);
            tmp->len =  section_len;
            *patch_len += section_len;
            log_info("Pri:%d, Patch length 0x%04x", tmp->pri, tmp->len);
            tmp->payload = pos + 8;
            if(opcode != PATCH_SECURITY_HEADER) {
                insert_queue_sort(patch_list, tmp);
            } else {
                if((g_key_id == tmp->key_id) && (g_key_id > 0)) {
                    insert_queue_sort(patch_list, tmp);
                    *sec_flag = 1;
                } else {
                    pos += (8 + section_len);
                    free(tmp);
                    continue;
                }
            }
        } else {
            section_len =  little_endian_read_32(pos, 4);
            log_info("Patch length 0x%04x", section_len);
        }
        pos += (8 + section_len);
    }
    return 0;
}
static uint8_t *rtb_get_patch_header(uint32_t *len,
                                     btstack_linked_list_t * patch_list, uint8_t * epatch_buf,
                                     uint8_t key_id)
{
    UNUSED(key_id);
    uint16_t i, j;
    struct rtb_new_patch_hdr *new_patch;
    uint8_t sec_flag = 0;
    uint32_t number_of_ota_flag;
    uint32_t patch_len = 0;
    uint8_t *section_pos;
    uint8_t *ota_flag_pos;
    uint32_t number_of_section;

    struct rtb_section_hdr section_hdr;
    struct rtb_ota_flag ota_flag;

    new_patch = (struct rtb_new_patch_hdr *)epatch_buf;
    number_of_section = new_patch->number_of_section;

    log_info("FW version 0x%02x,%02x,%02x,%02x,%02x,%02x,%02x,%02x",
                *(epatch_buf + 8), *(epatch_buf + 9), *(epatch_buf + 10),
                *(epatch_buf + 11),*(epatch_buf + 12), *(epatch_buf + 13),
                *(epatch_buf + 14), *(epatch_buf + 15));

    section_pos = epatch_buf + 20;

    for (i = 0; i < number_of_section; i++) {
        section_hdr.opcode = little_endian_read_32(section_pos, 0);
        section_hdr.section_len = little_endian_read_32(section_pos, 4);
        log_info("opcode 0x%04x", section_hdr.opcode);
        switch (section_hdr.opcode) {
        case PATCH_SNIPPETS:
            insert_patch(patch_list, section_pos, PATCH_SNIPPETS, &patch_len, NULL);
            printf("Realtek: patch len is %d\n",patch_len);
            break;
        case PATCH_SECURITY_HEADER:
            if(!g_key_id)
                break;

            sec_flag = 0;
            insert_patch(patch_list, section_pos, PATCH_SECURITY_HEADER, &patch_len, &sec_flag);
            if(sec_flag)
                break;

            for (i = 0; i < number_of_section; i++) {
                section_hdr.opcode = little_endian_read_32(section_pos, 0);
                section_hdr.section_len = little_endian_read_32(section_pos, 4);
                if(section_hdr.opcode == PATCH_DUMMY_HEADER) {
                    insert_patch(patch_list, section_pos, PATCH_DUMMY_HEADER, &patch_len, NULL);
                }
                section_pos += (SECTION_HEADER_SIZE + section_hdr.section_len);
            }
            break;
        case PATCH_DUMMY_HEADER:
            if(g_key_id) {
                break;
            }
            insert_patch(patch_list, section_pos, PATCH_DUMMY_HEADER, &patch_len, NULL);
            break;
        case PATCH_OTA_FLAG:
            ota_flag_pos = section_pos + 4;
            number_of_ota_flag = little_endian_read_32(ota_flag_pos, 0);
            ota_flag.eco = (uint8_t)*(ota_flag_pos + 1);
            if (ota_flag.eco == rom_version + 1) {
                for (j = 0; j < number_of_ota_flag; j++) {
                    if (ota_flag.eco == rom_version + 1) {
                        ota_flag.enable = little_endian_read_32(ota_flag_pos, 4);
                    }
                }
            }
            break;
        default:
            log_error("Unknown Opcode");
            break;
        }
        section_pos += (SECTION_HEADER_SIZE + section_hdr.section_len);
    }
    *len = patch_len;

    return NULL;
}

static inline int get_max_patch_size(uint8_t chip_type)
{
    int max_patch_size = 0;

    switch (chip_type) {
    case RTLPREVIOUS:
        max_patch_size = 24 * 1024;
        break;
    case RTL8822BU:
        max_patch_size = 25 * 1024;
        break;
    case RTL8723DU:
    case RTL8822CU:
    case RTL8761BU:
    case RTL8821CU:
        max_patch_size = 40 * 1024;
        break;
    case RTL8852AU:
        max_patch_size = 0x114D0 + 529; /* 69.2KB */
        break;
    case RTL8723FU:
        max_patch_size = 0xC4Cf + 529; /* 49.2KB */
        break;
    case RTL8852BU:
    case RTL8851BU:
        max_patch_size = 0x104D0 + 529;  /* 65KB */
        break;
    case RTL8852CU:
        max_patch_size = 0x130D0 + 529; /* 76.2KB */
        break;
    case RTL8822EU:
        max_patch_size = 0x24620 + 529;    /* 145KB */
        break;
    default:
        max_patch_size = 40 * 1024;
        break;
    }

    return max_patch_size;
}

static uint8_t update_firmware(const char *firmware, const char *config, uint8_t *hci_cmd_buffer) {
    static uint8_t *patch_buf = NULL;
    static uint32_t fw_total_len;
    static uint32_t fw_ptr;
    static uint8_t  index;

    // read firmware and config
    if (patch_buf == NULL) {
        uint16_t patch_length = 0;
        uint32_t offset = 0;
        FILE *   fw = NULL;
        uint32_t fw_size;
        uint8_t *fw_buf = NULL;

        FILE *   conf = NULL;
        uint32_t conf_size;
        uint8_t *conf_buf = NULL;

        uint32_t fw_version;
        uint16_t fw_num_patches;

        struct patch_node *tmp;
        unsigned max_patch_size = 0;

        if (firmware == NULL || config == NULL) {
            log_info("Please specify realtek firmware and config file paths");
            return FW_DONE;
        }
        // read config
        conf_size = read_file(&conf, &conf_buf, config);
        if (conf_size == 0) {
            log_info("Config size is 0, using efuse settings!");
        }
        // read firmware
        fw_size = read_file(&fw, &fw_buf, firmware);
        if (fw_size == 0) {
            log_info("Firmware size is 0. Quit!");
            if (conf_size != 0){
                finalize_file_and_buffer(&conf, &conf_buf);
            }
            return FW_DONE;
        }
        // check signature
        if (((memcmp(fw_buf, FW_SIGNATURE, 8) != 0) && (memcmp(fw_buf, FW_SIGNATURE_NEW, 8) != 0))
              || memcmp(fw_buf + fw_size - 4, EXTENSION_SIGNATURE, 4) != 0) {
            log_info("Wrong signature. Quit!");
            finalize_file_and_buffer(&fw, &fw_buf);
            finalize_file_and_buffer(&conf, &conf_buf);
            return FW_DONE;
        }
        // check project id
        if (lmp_subversion != project_id[rtk_get_fw_project_id(fw_buf + fw_size - 5)]) {
            log_info("Wrong project id. Quit!");
            finalize_file_and_buffer(&fw, &fw_buf);
            finalize_file_and_buffer(&conf, &conf_buf);
            return FW_DONE;
        }
        // init ordered list for new firmware signature
        btstack_linked_list_t patch_list = NULL;
        bool have_new_firmware_signature = memcmp(fw_buf, FW_SIGNATURE_NEW, 8) == 0;
        if (have_new_firmware_signature){
            printf("Realtek: Using new signature\n");
            uint8_t key_id = g_key_id;
            if (key_id == 0) {
                log_info("Wrong key id. Quit!");
                finalize_file_and_buffer(&fw, &fw_buf);
                finalize_file_and_buffer(&conf, &conf_buf);
                return FW_DONE;
            }

            rtb_get_patch_header(&fw_total_len, &patch_list, fw_buf, key_id);
            if (fw_total_len == 0) {
                finalize_file_and_buffer(&fw, &fw_buf);
                finalize_file_and_buffer(&conf, &conf_buf);
                return FW_DONE;
            }
            fw_total_len += conf_size;
        } else {
            printf("Realtek: Using old signature\n");
            // read firmware version
            fw_version = little_endian_read_32(fw_buf, 8);
            log_info("Firmware version: 0x%x", fw_version);

            // read number of patches
            fw_num_patches = little_endian_read_16(fw_buf, 12);
            log_info("Number of patches: %d", fw_num_patches);

        // find correct entry
            for (uint16_t i = 0; i < fw_num_patches; i++) {
                if (little_endian_read_16(fw_buf, 14 + 2 * i) == rom_version + 1) {
                    patch_length = little_endian_read_16(fw_buf, 14 + 2 * fw_num_patches + 2 * i);
                    offset       = little_endian_read_32(fw_buf, 14 + 4 * fw_num_patches + 4 * i);
                    log_info("patch_length %u, offset %u", patch_length, offset);
                    break;
                }
            }
            if (patch_length == 0) {
                log_debug("Failed to find valid patch");
                finalize_file_and_buffer(&fw, &fw_buf);
                finalize_file_and_buffer(&conf, &conf_buf);
                return FW_DONE;
            }
            fw_total_len = patch_length + conf_size;
        }

        max_patch_size = get_max_patch_size(patch->chip_type);
        printf("Realtek: FW/CONFIG total length is %d, max patch size id %d\n", fw_total_len, max_patch_size);
        if (fw_total_len > max_patch_size) {
            printf("FRealtek: W/CONFIG total length larger than allowed %d\n", max_patch_size);
            finalize_file_and_buffer(&fw, &fw_buf);
            finalize_file_and_buffer(&conf, &conf_buf);
            return FW_DONE;
        }
        // allocate patch buffer
        patch_buf = malloc(fw_total_len);
        if (patch_buf == NULL) {
            log_debug("Failed to allocate %u bytes for patch buffer", fw_total_len);
            finalize_file_and_buffer(&fw, &fw_buf);
            finalize_file_and_buffer(&conf, &conf_buf);
            return FW_DONE;
        }
        if (have_new_firmware_signature) {
            int tmp_len = 0;
            // append patches based on priority and free
            while (patch_list) {
                tmp = (struct patch_node *) patch_list;
                log_info("len = 0x%x", tmp->len);
                memcpy(patch_buf + tmp_len, tmp->payload, tmp->len);
                tmp_len += tmp->len;
                patch_list = patch_list->next;
                free(tmp);
            }
            if (conf_size) {
                memcpy(&patch_buf[fw_total_len - conf_size], conf_buf, conf_size);
            }
        } else {
            // copy patch
            memcpy(patch_buf, fw_buf + offset, patch_length);
            memcpy(patch_buf + patch_length - 4, &fw_version, 4);
            memcpy(patch_buf + patch_length, conf_buf, conf_size);
        }
        fw_ptr = 0;
        index  = 0;

        // close files
        finalize_file_and_buffer(&fw, &fw_buf);
        finalize_file_and_buffer(&conf, &conf_buf);
    }

    uint8_t len;
    if (fw_total_len - fw_ptr > 252) {
        len = 252;
    } else {
        len = fw_total_len - fw_ptr;
        index |= 0x80;  // end
    }

    if (len) {
        little_endian_store_16(hci_cmd_buffer, 0, HCI_OPCODE_HCI_RTK_DOWNLOAD_FW);
        HCI_CMD_SET_LENGTH(hci_cmd_buffer, len + 1);
        HCI_CMD_DOWNLOAD_SET_INDEX(hci_cmd_buffer, index);
        HCI_CMD_DOWNLOAD_COPY_FW_DATA(hci_cmd_buffer, patch_buf, fw_ptr, len);
        index++;
        if (index > 0x7f) {
            index = (index & 0x7f) +1;
        }
        fw_ptr += len;
        return FW_MORE_TO_DO;
    }

    // cleanup and return
    free(patch_buf);
    patch_buf = NULL;
    printf("Realtek: Init process finished\n");
    return FW_DONE;
}

#endif  // HAVE_POSIX_FILE_IO

static const uint8_t hci_realtek_read_sec_proj[]       = {0x61, 0xfc, 0x05, 0x10, 0xA4, 0x0D, 0x00, 0xb0 };
static const uint8_t hci_realtek_read_lmp_subversion[] = {0x61, 0xfc, 0x05, 0x10, 0x38, 0x04, 0x28, 0x80 };
static const uint8_t hci_realtek_read_hci_revision[]   = {0x61, 0xfc, 0x05, 0x10, 0x3A, 0x04, 0x28, 0x80 };

static btstack_chipset_result_t chipset_next_command(uint8_t *hci_cmd_buffer) {
#ifdef HAVE_POSIX_FILE_IO
    uint8_t ret;
    while (true) {
        switch (state) {
            case STATE_PHASE_1_READ_LMP_SUBVERSION:
                memcpy(hci_cmd_buffer, hci_realtek_read_lmp_subversion, sizeof(hci_realtek_read_lmp_subversion));
                state = STATE_PHASE_1_W4_READ_LMP_SUBVERSION;
                break;
            case STATE_PHASE_1_READ_HCI_REVISION:
                memcpy(hci_cmd_buffer, hci_realtek_read_hci_revision, sizeof(hci_realtek_read_hci_revision));
                state = STATE_PHASE_1_W4_READ_HCI_REVISION;
                break;
            case STATE_PHASE_1_DONE:
                // custom pre-init done, continue with read ROM version in main custom init
                state = STATE_PHASE_2_READ_ROM_VERSION;
                return BTSTACK_CHIPSET_DONE;
            case STATE_PHASE_2_READ_ROM_VERSION:
                HCI_CMD_SET_OPCODE(hci_cmd_buffer, HCI_OPCODE_HCI_RTK_READ_ROM_VERSION);
                HCI_CMD_SET_LENGTH(hci_cmd_buffer, 0);
                state = STATE_PHASE_2_READ_SEC_PROJ;
                break;
            case STATE_PHASE_2_READ_SEC_PROJ:
                memcpy(hci_cmd_buffer, hci_realtek_read_sec_proj, sizeof(hci_realtek_read_sec_proj));
                state = STATE_PHASE_2_W4_SEC_PROJ;
                break;
            case STATE_PHASE_2_LOAD_FIRMWARE:
                if (lmp_subversion != ROM_LMP_8723a) {
                    ret = update_firmware(firmware_file_path, config_file_path, hci_cmd_buffer);
                } else {
                    log_info("Realtek firmware for old patch style not implemented");
                    ret = FW_DONE;
                }
                if (ret != FW_DONE) {
                    break;
                }
                // we are done
                state = STATE_PHASE_2_RESET;

                /* fall through */

            case STATE_PHASE_2_RESET:
                HCI_CMD_SET_OPCODE(hci_cmd_buffer, HCI_OPCODE_HCI_RESET);
                HCI_CMD_SET_LENGTH(hci_cmd_buffer, 0);
                state = STATE_PHASE_2_DONE;
                break;
            case STATE_PHASE_2_DONE:
                hci_remove_event_handler(&hci_event_callback_registration);
                return BTSTACK_CHIPSET_DONE;
            default:
                log_info("Invalid state %d", state);
                return BTSTACK_CHIPSET_DONE;
        }
        return BTSTACK_CHIPSET_VALID_COMMAND;
    }
#else   // HAVE_POSIX_FILE_IO
    log_info("Realtek without File IO is not implemented yet");
    return BTSTACK_CHIPSET_NO_INIT_SCRIPT;
#endif  // HAVE_POSIX_FILE_IO
}

void btstack_chipset_realtek_set_firmware_file_path(const char *path) {
#ifdef HAVE_POSIX_FILE_IO
    firmware_file_path = path;
#endif
}

void btstack_chipset_realtek_set_firmware_folder_path(const char *path) {
#ifdef HAVE_POSIX_FILE_IO
    firmware_folder_path = path;
#endif
}

void btstack_chipset_realtek_set_config_file_path(const char *path) {
#ifdef HAVE_POSIX_FILE_IO
    config_file_path = path;
#endif
}

void btstack_chipset_realtek_set_config_folder_path(const char *path) {
#ifdef HAVE_POSIX_FILE_IO
    config_folder_path = path;
#endif
}

void btstack_chipset_realtek_set_product_id(uint16_t id) {
    product_id = id;
}

uint16_t btstack_chipset_realtek_get_num_usb_controllers(void){
    return (sizeof(fw_patch_table) / sizeof(patch_info)) - 1; // sentinel
}

void btstack_chipset_realtek_get_vendor_product_id(uint16_t index, uint16_t * out_vendor_id, uint16_t * out_product_id){
    btstack_assert(index < ((sizeof(fw_patch_table) / sizeof(patch_info)) - 1));
    *out_vendor_id = 0xbda;
    *out_product_id = fw_patch_table[index].prod_id;
}

static const btstack_chipset_t btstack_chipset_realtek = {
    "REALTEK", chipset_init, chipset_next_command,
    NULL,  // chipset_set_baudrate_command,
    NULL,  // chipset_set_bd_addr_command not supported or implemented
};

const btstack_chipset_t *btstack_chipset_realtek_instance(void) { return &btstack_chipset_realtek; }
