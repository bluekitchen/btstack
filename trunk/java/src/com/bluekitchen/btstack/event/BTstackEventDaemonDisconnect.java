package com.bluekitchen.btstack.event;

import com.bluekitchen.btstack.Event;

public class BTstackEventDaemonDisconnect extends Event {
    public BTstackEventDaemonDisconnect() {
        super(new byte[]{0x00, 0x00}, 0);
    }
}
