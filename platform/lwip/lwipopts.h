#ifndef __LWIPOPTS_H__
#define __LWIPOPTS_H__

/**
 * NO_SYS==1: Bare metal lwIP
 */
#define NO_SYS 1
/**
 * LWIP_NETCONN==0: Disable Netconn API (require to use api_lib.c)
 */
#define LWIP_NETCONN 0
/**
 * LWIP_SOCKET==0: Disable Socket API (require to use sockets.c)
 */
#define LWIP_SOCKET 0

/**
 * SYS_LIGHTWEIGHT_PROT==1: enable inter-task protection (and task-vs-interrupt
 * protection) for certain critical regions during buffer allocation, deallocation
 * and memory allocation and deallocation.
 * ATTENTION: This is required when using lwIP from more than one context! If
 * you disable this, you must be sure what you are doing!
 */
/**
 * SYS_LIGHTWEIGHT_PROT==0: 
 */
#define SYS_LIGHTWEIGHT_PROT 0

/* ---------- Memory options ---------- */
/**
 * MEM_ALIGNMENT: should be set to the alignment of the CPU
 *    4 byte alignment -> #define MEM_ALIGNMENT 4
 *    2 byte alignment -> #define MEM_ALIGNMENT 2
 */
#ifndef MEM_ALIGNMENT
#define MEM_ALIGNMENT 4
#endif

/**
 * MEM_SIZE: the size of the heap memory. If the application will send
 * a lot of data that needs to be copied, this should be set high.
 */
#ifndef MEM_SIZE
#define MEM_SIZE (22 * 1024)
#endif

/* MEMP_NUM_PBUF: the number of memp struct pbufs. If the application
   sends a lot of data out of ROM (or other static memory), this
   should be set high. */
#ifndef MEMP_NUM_PBUF
#define MEMP_NUM_PBUF 15
#endif
/* MEMP_NUM_UDP_PCB: the number of UDP protocol control blocks. One
   per active UDP "connection". */
#ifndef MEMP_NUM_UDP_PCB
#define MEMP_NUM_UDP_PCB 6
#endif
/* MEMP_NUM_TCP_PCB: the number of simulatenously active TCP
   connections. */
#ifndef MEMP_NUM_TCP_PCB
#define MEMP_NUM_TCP_PCB 10
#endif
/* MEMP_NUM_TCP_PCB_LISTEN: the number of listening TCP
   connections. */
#ifndef MEMP_NUM_TCP_PCB_LISTEN
#define MEMP_NUM_TCP_PCB_LISTEN 6
#endif
/* MEMP_NUM_TCP_SEG: the number of simultaneously queued TCP
   segments. */
#ifndef MEMP_NUM_TCP_SEG
#define MEMP_NUM_TCP_SEG 22
#endif
/* MEMP_NUM_SYS_TIMEOUT: the number of simulateously active
   timeouts. */
#ifndef MEMP_NUM_SYS_TIMEOUT
#define MEMP_NUM_SYS_TIMEOUT 10
#endif

/* ---------- Pbuf options ---------- */
/* PBUF_POOL_SIZE: the number of buffers in the pbuf pool. */
#ifndef PBUF_POOL_SIZE
#define PBUF_POOL_SIZE 9
#endif

/* PBUF_POOL_BUFSIZE: the size of each pbuf in the pbuf pool. */
/* Default value is defined in lwip\src\include\lwip\opt.h as
 * LWIP_MEM_ALIGN_SIZE(TCP_MSS+40+PBUF_LINK_ENCAPSULATION_HLEN+PBUF_LINK_HLEN)*/

/* ---------- TCP options ---------- */
#ifndef LWIP_TCP
#define LWIP_TCP 1
#endif

#ifndef TCP_TTL
#define TCP_TTL 255
#endif

/* Controls if TCP should queue segments that arrive out of
   order. Define to 0 if your device is low on memory. */
#ifndef TCP_QUEUE_OOSEQ
#define TCP_QUEUE_OOSEQ 0
#endif

/* TCP Maximum segment size. */
#ifndef TCP_MSS
#define TCP_MSS (1500 - 40) /* TCP_MSS = (Ethernet MTU - IP header size - TCP header size) */
#endif

/* TCP sender buffer space (bytes). */
#ifndef TCP_SND_BUF
#define TCP_SND_BUF (6 * TCP_MSS) // 2
#endif

/* TCP sender buffer space (pbufs). This must be at least = 2 *
   TCP_SND_BUF/TCP_MSS for things to work. */
#ifndef TCP_SND_QUEUELEN
#define TCP_SND_QUEUELEN (3 * TCP_SND_BUF) / TCP_MSS // 6
#endif

/* TCP receive window. */
#ifndef TCP_WND
#define TCP_WND (2 * TCP_MSS)
#endif

/* Enable backlog*/
#ifndef TCP_LISTEN_BACKLOG
#define TCP_LISTEN_BACKLOG 1
#endif

/* ---------- Network Interfaces options ---------- */
/* Support netif api (in netifapi.c). */
#ifndef LWIP_NETIF_API
#define LWIP_NETIF_API 0
#endif

/* ---------- ICMP options ---------- */
#ifndef LWIP_ICMP
#define LWIP_ICMP 1
#endif

/* ---------- DHCP options ---------- */
/* Define LWIP_DHCP to 1 if you want DHCP configuration of
   interfaces. DHCP is not implemented in lwIP 0.5.1, however, so
   turning this on does currently not work. */
#ifndef LWIP_DHCP
#define LWIP_DHCP 1
#endif

/* ---------- UDP options ---------- */
#ifndef LWIP_UDP
#define LWIP_UDP 1
#endif
#ifndef UDP_TTL
#define UDP_TTL 255
#endif

/* ---------- Statistics options ---------- */
#ifndef LWIP_STATS
#define LWIP_STATS 0
#endif
#ifndef LWIP_PROVIDE_ERRNO
#define LWIP_PROVIDE_ERRNO 1
#endif

/*
   --------------------------------------
   ---------- Checksum options ----------
   --------------------------------------
*/

/*
Some MCU allow computing and verifying the IP, UDP, TCP and ICMP checksums by hardware:
 - To use this feature let the following define uncommented.
 - To disable it and process by CPU comment the  the checksum.
*/
//#define CHECKSUM_BY_HARDWARE

#ifdef CHECKSUM_BY_HARDWARE
/* CHECKSUM_GEN_IP==0: Generate checksums by hardware for outgoing IP packets.*/
#define CHECKSUM_GEN_IP 0
/* CHECKSUM_GEN_UDP==0: Generate checksums by hardware for outgoing UDP packets.*/
#define CHECKSUM_GEN_UDP 0
/* CHECKSUM_GEN_TCP==0: Generate checksums by hardware for outgoing TCP packets.*/
#define CHECKSUM_GEN_TCP 0
/* CHECKSUM_CHECK_IP==0: Check checksums by hardware for incoming IP packets.*/
#define CHECKSUM_CHECK_IP 0
/* CHECKSUM_CHECK_UDP==0: Check checksums by hardware for incoming UDP packets.*/
#define CHECKSUM_CHECK_UDP 0
/* CHECKSUM_CHECK_TCP==0: Check checksums by hardware for incoming TCP packets.*/
#define CHECKSUM_CHECK_TCP 0
#else
/* CHECKSUM_GEN_IP==1: Generate checksums in software for outgoing IP packets.*/
#define CHECKSUM_GEN_IP 1
/* CHECKSUM_GEN_UDP==1: Generate checksums in software for outgoing UDP packets.*/
#define CHECKSUM_GEN_UDP 1
/* CHECKSUM_GEN_TCP==1: Generate checksums in software for outgoing TCP packets.*/
#define CHECKSUM_GEN_TCP 1
/* CHECKSUM_CHECK_IP==1: Check checksums in software for incoming IP packets.*/
#define CHECKSUM_CHECK_IP 1
/* CHECKSUM_CHECK_UDP==1: Check checksums in software for incoming UDP packets.*/
#define CHECKSUM_CHECK_UDP 1
/* CHECKSUM_CHECK_TCP==1: Check checksums in software for incoming TCP packets.*/
#define CHECKSUM_CHECK_TCP 1
#endif

/*
   ------------------------------------
   ---------- Debugging options ----------
   ------------------------------------
*/

// #define LWIP_DEBUG

// TODO: map these to <stdint.h>

#ifdef LWIP_DEBUG
#define U8_F "c"
#define S8_F "c"
#define X8_F "02x"
#define U16_F "u"
#define S16_F "d"
#define X16_F "x"
#define U32_F "u"
#define S32_F "d"
#define X32_F "x"
#define SZT_F "u"
#endif

#if (LWIP_DNS || LWIP_IGMP || LWIP_IPV6) && !defined(LWIP_RAND)
/* When using IGMP or IPv6, LWIP_RAND() needs to be defined to a random-function returning an u32_t random value*/
#include "lwip/arch.h"
u32_t lwip_rand(void);
#define LWIP_RAND() lwip_rand()
#endif

#endif /* __LWIPOPTS_H__ */

/*****END OF FILE****/
