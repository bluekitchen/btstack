#include <stdio.h>
#include <stdint.h>
#include "btstack_util.h"
#include "aes_cmac.h"
#include <errno.h>
#include "aes_ccm.h"

// degbugging
// #define LOG_XN

// CCM Encrypt & Decrypt from Zephyr Project

typedef uint8_t  u8_t;
typedef uint16_t u16_t;
typedef uint64_t u64_t;

static void sys_put_be16(uint16_t value, uint8_t * buffer) {
	big_endian_store_16(buffer, 0, value);
}
static int bt_encrypt_be(const uint8_t * key, const uint8_t * plain, uint8_t * cipher) {
	aes128_calc_cyphertext(key, plain, cipher);
	return 0;
}

int bt_mesh_ccm_decrypt(const u8_t key[16], u8_t nonce[13],
			       const u8_t *enc_msg, size_t msg_len,
			       const u8_t *aad, size_t aad_len,
			       u8_t *out_msg, size_t mic_size)
{
	u8_t msg[16], pmsg[16], cmic[16], cmsg[16], Xn[16], mic[16];
	u16_t last_blk, blk_cnt;
	size_t i, j;
	int err;

	if (msg_len < 1 || aad_len >= 0xff00) {
		return -EINVAL;
	}

	/* C_mic = e(AppKey, 0x01 || nonce || 0x0000) */
	pmsg[0] = 0x01;
	memcpy(pmsg + 1, nonce, 13);
	sys_put_be16(0x0000, pmsg + 14);

#ifdef LOG_XN
	printf("%16s: ", "A0");
	printf_hexdump(pmsg, 16);
#endif

	err = bt_encrypt_be(key, pmsg, cmic);
	if (err) {
		return err;
	}

#ifdef LOG_XN
	printf("%16s: ", "S0");
	printf_hexdump(cmic, 16);
#endif


	/* X_0 = e(AppKey, 0x09 || nonce || length) */
	if (mic_size == sizeof(u64_t)) {
		pmsg[0] = 0x19 | (aad_len ? 0x40 : 0x00);
	} else {
		pmsg[0] = 0x09 | (aad_len ? 0x40 : 0x00);
	}

	memcpy(pmsg + 1, nonce, 13);
	sys_put_be16(msg_len, pmsg + 14);

#ifdef LOG_XN
	printf("%16s: ", "B0");
	printf_hexdump(pmsg, 16);
#endif

	err = bt_encrypt_be(key, pmsg, Xn);
	if (err) {
		return err;
	}

#ifdef LOG_XN
	printf("%16s: ", "Xn");
	printf_hexdump(Xn, 16);
#endif

	/* If AAD is being used to authenticate, include it here */
	if (aad_len) {
		sys_put_be16(aad_len, pmsg);

		for (i = 0; i < sizeof(u16_t); i++) {
			pmsg[i] = Xn[i] ^ pmsg[i];
		}

		j = 0;
		aad_len += sizeof(u16_t);
		while (aad_len > 16) {
			do {
				pmsg[i] = Xn[i] ^ aad[j];
				i++, j++;
			} while (i < 16);

			aad_len -= 16;
			i = 0;

			err = bt_encrypt_be(key, pmsg, Xn);
			if (err) {
				return err;
			}
		}

		for (i = 0; i < aad_len; i++, j++) {
			pmsg[i] = Xn[i] ^ aad[j];
		}

		for (i = aad_len; i < 16; i++) {
			pmsg[i] = Xn[i];
		}

#ifdef LOG_XN
    	printf("%16s: ", "Xn XOR bn");
	    printf_hexdump(pmsg, 16);
#endif

		err = bt_encrypt_be(key, pmsg, Xn);
		if (err) {
			return err;
		}
	}

	last_blk = msg_len % 16;
	blk_cnt = (msg_len + 15) / 16;
	if (!last_blk) {
		last_blk = 16;
	}

	for (j = 0; j < blk_cnt; j++) {
		if (j + 1 == blk_cnt) {
			/* C_1 = e(AppKey, 0x01 || nonce || 0x0001) */
			pmsg[0] = 0x01;
			memcpy(pmsg + 1, nonce, 13);
			sys_put_be16(j + 1, pmsg + 14);

			err = bt_encrypt_be(key, pmsg, cmsg);
			if (err) {
				return err;
			}

			/* Encrypted = Payload[0-15] ^ C_1 */
			for (i = 0; i < last_blk; i++) {
				msg[i] = enc_msg[(j * 16) + i] ^ cmsg[i];
			}

			memcpy(out_msg + (j * 16), msg, last_blk);

			/* X_1 = e(AppKey, X_0 ^ Payload[0-15]) */
			for (i = 0; i < last_blk; i++) {
				pmsg[i] = Xn[i] ^ msg[i];
			}

			for (i = last_blk; i < 16; i++) {
				pmsg[i] = Xn[i] ^ 0x00;
			}

#ifdef LOG_XN
            printf("%16s: ", "Xn XOR bn");
            printf_hexdump(pmsg, 16);
#endif

			err = bt_encrypt_be(key, pmsg, Xn);
			if (err) {
				return err;
			}

#ifdef LOG_XN
            printf("%16s: ", "Xn XOR bn");
            printf_hexdump(pmsg, 16);
#endif

			/* MIC = C_mic ^ X_1 */
			for (i = 0; i < sizeof(mic); i++) {
				mic[i] = cmic[i] ^ Xn[i];
			}

#ifdef LOG_XN
            printf("%16s: ", "mic");
            printf_hexdump(mic, 16);
#endif

		} else {
			/* C_1 = e(AppKey, 0x01 || nonce || 0x0001) */
			pmsg[0] = 0x01;
			memcpy(pmsg + 1, nonce, 13);
			sys_put_be16(j + 1, pmsg + 14);

#ifdef LOG_XN
            printf("%16s: ", "Ai");
            printf_hexdump(mic, 16);
#endif

			err = bt_encrypt_be(key, pmsg, cmsg);
			if (err) {
				return err;
			}

#ifdef LOG_XN
            printf("%16s: ", "Si");
            printf_hexdump(mic, 16);
#endif

			/* Encrypted = Payload[0-15] ^ C_1 */
			for (i = 0; i < 16; i++) {
				msg[i] = enc_msg[(j * 16) + i] ^ cmsg[i];
			}

			memcpy(out_msg + (j * 16), msg, 16);

#ifdef LOG_XN
            printf("%16s: ", "bn");
            printf_hexdump(msg, 16);
#endif

			/* X_1 = e(AppKey, X_0 ^ Payload[0-15]) */
			for (i = 0; i < 16; i++) {
				pmsg[i] = Xn[i] ^ msg[i];
			}

			err = bt_encrypt_be(key, pmsg, Xn);
			if (err) {
				return err;
			}

#ifdef LOG_XN
            printf("%16s: ", "Xn");
            printf_hexdump(mic, 16);
#endif

		}
	}
	
	return 0;
}

int bt_mesh_ccm_encrypt(const u8_t key[16], u8_t nonce[13],
			       const u8_t *msg, size_t msg_len,
			       const u8_t *aad, size_t aad_len,
			       u8_t *out_msg, size_t mic_size)
{
	u8_t pmsg[16], cmic[16], cmsg[16], mic[16], Xn[16];
	u16_t blk_cnt, last_blk;
	size_t i, j;
	int err;

	// BT_DBG("key %s", bt_hex(key, 16));
	// BT_DBG("nonce %s", bt_hex(nonce, 13));
	// BT_DBG("msg (len %zu) %s", msg_len, bt_hex(msg, msg_len));
	// BT_DBG("aad_len %zu mic_size %zu", aad_len, mic_size);

	/* Unsupported AAD size */
	if (aad_len >= 0xff00) {
		return -EINVAL;
	}

	/* C_mic = e(AppKey, 0x01 || nonce || 0x0000) */
	pmsg[0] = 0x01;
	memcpy(pmsg + 1, nonce, 13);
	sys_put_be16(0x0000, pmsg + 14);

#ifdef LOG_XN
	printf("%16s: ", "A0");
	printf_hexdump(pmsg, 16);
#endif

	err = bt_encrypt_be(key, pmsg, cmic);
	if (err) {
		return err;
	}

#ifdef LOG_XN
	printf("%16s: ", "S0");
	printf_hexdump(cmic, 16);
#endif

	/* X_0 = e(AppKey, 0x09 || nonce || length) */
	if (mic_size == sizeof(u64_t)) {
		pmsg[0] = 0x19 | (aad_len ? 0x40 : 0x00);
	} else {
		pmsg[0] = 0x09 | (aad_len ? 0x40 : 0x00);
	}

	memcpy(pmsg + 1, nonce, 13);
	sys_put_be16(msg_len, pmsg + 14);

#ifdef LOG_XN
	printf("%16s: ", "B0");
	printf_hexdump(pmsg, 16);
#endif

	err = bt_encrypt_be(key, pmsg, Xn);
	if (err) {
		return err;
	}

#ifdef LOG_XN
	printf("%16s: ", "X1");
	printf_hexdump(Xn, 16);
#endif

	/* If AAD is being used to authenticate, include it here */
	if (aad_len) {
		sys_put_be16(aad_len, pmsg);

		for (i = 0; i < sizeof(u16_t); i++) {
			pmsg[i] = Xn[i] ^ pmsg[i];
		}

		j = 0;
		aad_len += sizeof(u16_t);
		while (aad_len > 16) {
			do {
				pmsg[i] = Xn[i] ^ aad[j];
				i++, j++;
			} while (i < 16);

			aad_len -= 16;
			i = 0;

#ifdef LOG_XN
            printf("%16s: ", "Xn XOR bn (aad)");
            printf_hexdump(pmsg, 16);
#endif

			err = bt_encrypt_be(key, pmsg, Xn);
			if (err) {
				return err;
			}

#ifdef LOG_XN
            printf("%16s: ", "Xn+1 AAD");
            printf_hexdump(Xn, 16);
#endif

		}

		for (i = 0; i < aad_len; i++, j++) {
			pmsg[i] = Xn[i] ^ aad[j];
		}

		for (i = aad_len; i < 16; i++) {
			pmsg[i] = Xn[i];
		}

#ifdef LOG_XN
        printf("%16s: ", "Xn XOR bn (aad)");
        printf_hexdump(pmsg, 16);
#endif

		err = bt_encrypt_be(key, pmsg, Xn);
		if (err) {
			return err;
		}
#ifdef LOG_XN
        printf("%16s: ", "Xn+1 AAD");
        printf_hexdump(Xn, 16);
#endif

	}

	last_blk = msg_len % 16;
	blk_cnt = (msg_len + 15) / 16;
	if (!last_blk) {
		last_blk = 16;
	}

	for (j = 0; j < blk_cnt; j++) {
		if (j + 1 == blk_cnt) {
			/* X_1 = e(AppKey, X_0 ^ Payload[0-15]) */
			for (i = 0; i < last_blk; i++) {
				pmsg[i] = Xn[i] ^ msg[(j * 16) + i];
			}
			for (i = last_blk; i < 16; i++) {
				pmsg[i] = Xn[i] ^ 0x00;
			}

#ifdef LOG_XN
            printf("%16s: ", "Xn XOR Bn");
            printf_hexdump(pmsg, 16);
#endif

			err = bt_encrypt_be(key, pmsg, Xn);
			if (err) {
				return err;
			}

#ifdef LOG_XN
            printf("%16s: ", "Xn+1");
            printf_hexdump(Xn, 16);
#endif

			/* MIC = C_mic ^ X_1 */
			for (i = 0; i < sizeof(mic); i++) {
				mic[i] = cmic[i] ^ Xn[i];
			}

#ifdef LOG_XN
            printf("%16s: ", "mic");
            printf_hexdump(mic, 16);
#endif

			/* C_1 = e(AppKey, 0x01 || nonce || 0x0001) */
			pmsg[0] = 0x01;
			memcpy(pmsg + 1, nonce, 13);
			sys_put_be16(j + 1, pmsg + 14);

			err = bt_encrypt_be(key, pmsg, cmsg);
			if (err) {
				return err;
			}

			/* Encrypted = Payload[0-15] ^ C_1 */
			for (i = 0; i < last_blk; i++) {
				out_msg[(j * 16) + i] =
					msg[(j * 16) + i] ^ cmsg[i];
			}
		} else {

#ifdef LOG_XN
            printf("%16s: ", "bn");
            printf_hexdump(msg, 16);
#endif

			/* X_1 = e(AppKey, X_0 ^ Payload[0-15]) */
			for (i = 0; i < 16; i++) {
				pmsg[i] = Xn[i] ^ msg[(j * 16) + i];
			}

#ifdef LOG_XN
            printf("%16s: ", "Xn XOR Bn");
            printf_hexdump(pmsg, 16);
#endif

			err = bt_encrypt_be(key, pmsg, Xn);
			if (err) {
				return err;
			}

#ifdef LOG_XN
            printf("%16s: ", "Xn+1");
            printf_hexdump(Xn, 16);
#endif

			/* C_1 = e(AppKey, 0x01 || nonce || 0x0001) */
			pmsg[0] = 0x01;
			memcpy(pmsg + 1, nonce, 13);
			sys_put_be16(j + 1, pmsg + 14);

			err = bt_encrypt_be(key, pmsg, cmsg);
			if (err) {
				return err;
			}

			/* Encrypted = Payload[0-15] ^ C_N */
			for (i = 0; i < 16; i++) {
				out_msg[(j * 16) + i] =
					msg[(j * 16) + i] ^ cmsg[i];
			}

		}
	}

	memcpy(out_msg + msg_len, mic, mic_size);

	return 0;
}
