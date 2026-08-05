# Synchronous HCI Transport

## Status

First option implemented.

## Context

For asynchronous HCI transport interfaces, we send an HCI packet and receive a HCI_EVENT_TRANSPORT_PACKET_SENT
on a later run loop iteration.
When using a synchronous transport, e.g. the virtual HCI interface of our Zephyr port, the original implementation
simulates the HCI_EVENT_TRANSPORT_PACKET_SENT immediately, which could trigger another send operation, which will
repeat. This multiplies stack usage roughly by the number of packets the Bluetooth Controller can buffer.
-
## Decision

To avoid that, we can use the 'execute on main thread' run loop functionality to avoid the recursion.
There are several places where this could be introduced:
1. HCI transport interface: right after the send HCI packet, we could schedule a callback to simulate the
  HCI_EVENT_TRANSPORT_PACKET_SENT.
2. L2CAP: the 'can send now' is emitted by l2cap_notify_channel_can_send. Instead of calling it directly, it could
  be called indirectly
3. L2CAP: the 'can send now' is emitted by l2cap_notify_channel_can_send. Instead of emitting the 'can send now' directly
  this could be done indirectly

The proposed options can be implemented in any combination.

As a first step, we'll implement the HCI transport version as it reduces the differences between asynchronous and 
synchronous APIs, which might reduce complexity / variations, too.

## Consequences

Simulating the 'HCI_EVENT_TRANSPORT_PACKET_SENT' via execute on main thread will break the deep recursion for 
synchronous transports, while also reducing complexity in HCI.
