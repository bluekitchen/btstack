/*
 * Copyright (C) 2019 BlueKitchen GmbH
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

/*
 *  btstack_run_loop_qt.h
 *  Functionality special to the Qt event loop
 */

#ifndef btstack_run_loop_QT_H
#define btstack_run_loop_QT_H

#include "btstack_run_loop.h"

// BTstack Run Loop Object for Qt integration
#if defined __cplusplus
#include <QObject>
#include <QTimer>
#ifdef Q_OS_WIN
#include <windows.h>
#include <QWinEventNotifier>
#else
#include <QSocketNotifier>
#endif
class BTstackRunLoopQt : public QObject {
    Q_OBJECT
public:
    BTstackRunLoopQt(void);
public slots:
    void processTimers(void);
    void processCallbacks(void);
    void pollDataSources(void);
#ifdef Q_OS_WIN
    void processDataSource(HANDLE handle);
#else
    void processDataSourceRead(int fd);
    void processDataSourceWrite(int fd);
#endif
signals:
    void callbackAdded(void);
    void dataSourcesPollRequested(void);
};
#endif

#if defined __cplusplus
extern "C" {
#endif
	
/**
 * Provide btstack_run_loop_qt instance
 */
const btstack_run_loop_t * btstack_run_loop_qt_get_instance(void);

/* API_END */

#if defined __cplusplus
}
#endif

#endif // btstack_run_loop_QT_H
