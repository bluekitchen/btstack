# lwIP Support for BTstack

`bnep_lwip` connects BNEP to lwIP. `port` provides a `NO_SYS == 1`
configuration that runs both stacks on the BTstack thread.

## Threading and startup

BTstack's run loop owns BNEP operations and channel state. Call the adapter's
registration and connection APIs from that context. lwIP's core context owns
its netif, protocol state, and raw APIs; the pointer returned by
`bnep_lwip_get_interface()` does not make netif access thread-safe.

With `NO_SYS == 1`, run both stacks and their callbacks on the same thread; the
BTstack run loop drives lwIP timeouts. With `NO_SYS == 0`, the TCP/IP thread
owns the lwIP core. Use the port's supported callback or core-locking mechanism
to access it, and dispatch BTstack work to its run loop. The RTOS port must
provide the synchronization required to allocate `PBUF_POOL` buffers from the
BTstack context. Neither thread may wait for work that requires the other to
make progress.

Initialize lwIP and wait until its core is ready before calling
`bnep_lwip_init_with_callback()` from the BTstack context. The callback reports
completion and status in that context and may run before the call returns. In
RTOS mode, service registration and outgoing connections fail until successful
completion. The legacy `bnep_lwip_init()` does not report completion. If the
HTTP example uses platform-provided lwIP (`HAVE_LWIP`), the platform must have
started its TCP/IP thread before calling `btstack_main`.

In RTOS mode, netif changes run in the lwIP core context. Intermediate link
changes may be coalesced, but a reconnect takes the previous link down before
bringing the new one up, even if the BNEP CID is reused.

## Packet ownership

Incoming BNEP data is copied into an lwIP pbuf before the packet callback
returns. In RTOS mode, `tcpip_input` takes ownership on success. If posting a
pbuf release fails, the adapter retries and drops new incoming frames until the
release can be posted.

For outgoing traffic:

- With `NO_SYS == 0`, the adapter copies each frame into a bounded pool before
  returning from `linkoutput`; it does not retain the pbuf. The pool has
  `BNEP_LWIP_OUTGOING_QUEUE_SIZE` slots (default: `TCP_SND_QUEUELEN`), each
  reserving `HCI_ACL_PAYLOAD_SIZE` bytes plus metadata. A full pool returns
  `ERR_MEM` without waiting for Bluetooth.
- With `NO_SYS == 1`, the adapter queues a pbuf reference and releases it after
  sending or discarding the packet. It flattens chained pbufs into the BNEP
  output buffer immediately before sending. The reference does not protect
  volatile external payload data from modification; that data must remain valid
  and unchanged until transmission or discard.

In RTOS mode, disconnect disables enqueue before queued frames are discarded.
Transmission resumes only after lwIP applies the new connection's netif state.
