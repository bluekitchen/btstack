/***
 *** CSample.C
 ***
 *** An example program showing the use of LiveImport from purely straight C
 ***
 *** ----------
 ***
 *** Copyright 2009, Frontline Test Equipment, Inc
 ***
 *** ----------
 ***
 *** Tom Allebrandi (tallebrandi@fte.com/tom@ytram.com)
 *** Frontline Test Equipment, Inc
 ***/

/*******************************************************************************
 **
 ** Includes
 **/
#include "serial_port.h"
#pragma	warning(disable : 4115)
#include "LiveImportAPI.h"  // Definitions of function pointers.
#include "fifo.h"

#define COM_PORT_NUM_MAX	15
#define SERIAL_PORT_MUTEX_NAME "serial_port"
#define SERIAL_PORT_NUMBER_PREINDEX "\\\\.\\"

typedef struct {
	const char *port;
	bool is_available;
} com_port_t;

com_port_t COM_PORT[COM_PORT_NUM_MAX] = {
	{"COM1", false},
    {"COM2", false},
    {"COM3", false},
    {"COM4", false},
    {"COM5", false},
    {"COM6", false},
    {"COM7", false},
    {"COM8", false},
    {"COM9", false},
    {"COM10", false},
	{"COM11", false},
	{"COM12", false},
	{"COM13", false},
	{"COM14", false},
	{"COM15", false}
};

static DWORD serial_port_threadID = 0;
static HANDLE serial_port_Thread = NULL;
static HANDLE serial_port_mutex = NULL;
static SYSTEMTIME		systemtime;
static __int64			timestamp;
extern DWORD WINAPI serial_port_read_thread(LPVOID lpParam);
void serial_port_data_handler(uint8_t data);

BOOL mutex_lock()
{
    return OpenMutex(NULL, serial_port_mutex, SERIAL_PORT_MUTEX_NAME);
}

BOOL mutex_unlock()
{
    return ReleaseMutex(serial_port_mutex);
}

BOOL available_serial_port_check()
{
	uint8_t available_com_port_num = 0;
	BOOL ret;
	while (available_com_port_num == 0) {
		for (unsigned int i = 0; i < COM_PORT_NUM_MAX; i++) {
			if (i >= 9) { //can not open port that number is equal or greater than "COM10", add "\\\\.\\" in the front
				char buf[100] = { 0 };
				strncpy(buf, SERIAL_PORT_NUMBER_PREINDEX, strlen(SERIAL_PORT_NUMBER_PREINDEX));
				strncat(buf, COM_PORT[i].port, strlen(COM_PORT[i].port));
				ret = serial_port_open(buf, 115200);
			}
			else {
				ret = serial_port_open(COM_PORT[i].port, 115200);
			}
			if (ret == TRUE) {
				COM_PORT[i].is_available = true;
				serial_port_close();
				available_com_port_num++;
			}
		}
		if (available_com_port_num == 0) {
			printf("no available serial port!\r\n");
			Sleep(1000);
		}
		else {
			printf("available serial port: ");
			for (unsigned int i = 0; i < COM_PORT_NUM_MAX; i++) {
				if (COM_PORT[i].is_available) {
					printf("%s ", COM_PORT[i].port);
				}
			}
		}
	}
}
/*******************************************************************************
 **
 ** Forward Declarations
 **/
bool checkLiveImportConnection(void);

/*******************************************************************************
 **
 ** Local Data
 **/
/*
 * This example program sends one Bluetooth HCI frame, an HCI_READ_BD_ADDR
 */
BYTE sampleFrame[] = {0x09, 0x010, 0x00};
bool g_boolSendCannedMessage = false;

/*******************************************************************************
 **
 ** Program
 **/

int	main(
    int				argc,
    char **argv)
{
    int				i;
    char			*pszConfiguration;
    int				status = ERROR_INVALID_FUNCTION;
    //SYSTEMTIME		systemtime;
    char			szConfiguration[1024];
    char			szConnectionString[1024];
    char			szIniFile[_MAX_PATH];
    //__int64			timestamp;
    char buffer[81];
    int ch;
    bool boolQuit = false;
    int iFrameSize = sizeof(sampleFrame);
    char *psz = NULL;
    bool boolSuccess = false;
    HRESULT hr = S_OK;
    char szError[1024];
    char cPath[_MAX_PATH + 1];
    char cDir[_MAX_PATH];
    char cDrive[_MAX_DRIVE];
    int ret = 0;
    printf("Initializing...\n");

    //	__asm int 3;

    for (i = 1; i < argc; i++) {
        if (_strnicmp(argv[i], "/mem=", 5) == 0)
            psz = &argv[i][5];
        else if (_strnicmp(argv[i], "/canned", 7) == 0)
            g_boolSendCannedMessage = true;
    }

    /*
     * Set the name of the .INI file that is shared between FTS and a Live Import
     * data source
     */
    //_tcscpy_s(szIniFile, _countof(szIniFile), "..\\LiveImport.Ini");
    _tcscpy_s(szIniFile, _countof(szIniFile), ".\\LiveImport.Ini");

    // Get the connection string from the shared .INI file
    if (NULL == psz)
        GetPrivateProfileString("General", "ConnectionString", "", szConnectionString, sizeof(szConnectionString), szIniFile);
    else
        _tcscpy_s(szConnectionString, _countof(szConnectionString), psz);

    if (_tcslen(szConnectionString) == 0) {
        fprintf(stderr, "[General]ConnectionString not found in %s.\n", szIniFile);
        MessageBox(NULL, "[General]ConnectionString not found !", "Error", MB_OK);
        return (status);
    }

    // Get the configuration data from the shared .INI file
    GetPrivateProfileSection("Configuration", szConfiguration, sizeof(szConfiguration), szIniFile);

    if (strlen(szConfiguration) == 0) {
        fprintf(stderr, "[Configuration] section not found in %s.\n", szIniFile);
        MessageBox(NULL, "[Configuration] section not found !", "Error", MB_OK);
        return (status);
    }

    /*
     * The return was a buffer of null terminated strings. Live Import wants a
     * single string in which each line terminated by a new line
     */
    pszConfiguration = szConfiguration;
    while ((i = strlen(pszConfiguration)) != 0) {
        pszConfiguration[i] = '\n';	// Change null character to new line
        pszConfiguration += (i + 1);	// Move to the next string
    }

    // Initialize the FTS LiveImport Server
    GetModuleFileName(GetModuleHandle(NULL), cPath, _MAX_PATH);

    _tsplitpath_s(cPath, cDrive, _countof(cDrive), cDir, _countof(cDir), NULL, 0, NULL, 0);
    _stprintf_s(cPath, _countof(cPath), _T("%s%sLiveImportAPI.dll"), cDrive, cDir);  // Reuse cPath.
    g_pszLibraryName = cPath;

    if (!LoadLiveImportAPIFunctions())
        return (ERROR_INVALID_FUNCTION);

    hr = g_pInitializeLiveImport(szConnectionString, szConfiguration, &boolSuccess);

    if (FAILED(hr)) {			// Status flag from the server itself
        _stprintf_s(szError, _countof(szError), "LiveImport.Initialize() failed with status 0x%x.\n", hr);
        MessageBox(NULL, szError, "Error", MB_OK);
        return (status);
    } else if (!boolSuccess) {
        _tcscpy_s(szError, _countof(szError), "LiveImport.Initialize() failed to initialize.\n");
        MessageBox(NULL, szError, "Error", MB_OK);
        return (status);
    }

    // Send one frame of data
    if (checkLiveImportConnection()) {
        if (g_boolSendCannedMessage) {
            GetLocalTime(&systemtime);
            SystemTimeToFileTime(&systemtime, (FILETIME *) &timestamp);
            if (FAILED(status = g_pSendFrame(
                                    iFrameSize,
                                    iFrameSize,
                                    sampleFrame,
                                    1,				// Bluetooth HCI Command
                                    0,				// Bluetooth Host
                                    //  See BT Virtual.Dec and the [Configuration] in
                                    //  the LiveImport.Ini file
                                    timestamp))) {
                MessageBox(NULL, "LiveImport_SendFrame failed!", "Error", MB_OK);
                return (status);
            }

            boolQuit = true;
        }
		/* list available serial port*/
		BOOL check_ret = FALSE;
		do {
			check_ret = available_serial_port_check();
		} while (!check_ret);
		printf("\r\n");
		/****************************/

        while (!boolQuit) {
            printf("Enter a command (\"send\" or \"quit\" or \"serial port\"): \r\n");

            /* Read in single line from "stdin": */
            for (i = 0; (i < 80) && ((ch = getchar()) != EOF) && (ch != '\n'); i++)
                buffer[i] = (char)ch;

            /* Terminate string with null character: */
            buffer[i] = '\0';

            if (0 == _tcsicmp(buffer, "send")) {
                // Generate a timestamp
                GetLocalTime(&systemtime);
                SystemTimeToFileTime(&systemtime, (FILETIME *) &timestamp);

                // Send the frame
                if (FAILED(status = g_pSendFrame(
                                        iFrameSize,
                                        iFrameSize,
                                        sampleFrame,
                                        1,				// Bluetooth HCI Command
                                        0,				// Bluetooth Host
                                        //  See BT Virtual.Dec and the [Configuration] in
                                        //  the LiveImport.Ini file
                                        timestamp))) {
                    fprintf(stderr, "LiveImport_SendFrame() failed to send data: Error %d.", status);
                    MessageBox(NULL, "LiveImport_SendFrame failed!", "Error", MB_OK);
                    return (status);
                }

            } else if (0 == _tcsicmp(buffer, "quit")) {
                boolQuit = true;
            } else {
                for (unsigned int i = 0; i < COM_PORT_NUM_MAX; i++) {
                    if (0 == _tcsicmp(buffer, COM_PORT[i].port)) {
						printf("Opening serial port...\n");
						if (i >= 9) {
							char buf[100] = { 0 };
							strncpy(buf, SERIAL_PORT_NUMBER_PREINDEX, strlen(SERIAL_PORT_NUMBER_PREINDEX));
							strncat(buf, COM_PORT[i].port, strlen(COM_PORT[i].port));
							ret = serial_port_open(buf, 921600);
						} else {
							ret = serial_port_open(COM_PORT[i].port, 921600);
						}
                        if (!ret) {
                            printf("open serial port fail\r\n");
                            return -1;
                        }
						printf("open serial port success\r\n");
						serial_port_mutex = CreateMutex(NULL, TRUE, SERIAL_PORT_MUTEX_NAME);
						assert(serial_port_mutex);

                        serial_port_threadID = CreateThread(NULL, 0, serial_port_read_thread, NULL, 0, &serial_port_Thread); // 创建线程
                        assert(serial_port_threadID);
                        boolQuit = true;
                        break;
                    }
                }
            }
        }  // End while(!boolQuit)
    }
    while (1) {
        uint8_t pop_data = 0;
        uint8_t pop_result = 0;
        mutex_lock();
        pop_result = fifo_pop(&pop_data);
        mutex_unlock();
        if (pop_result) {
            serial_port_data_handler(pop_data);
        } else {
            //printf("fifo pop fail \r\n");
			Sleep(5);
        }
    }
    if (NULL != g_pReleaseLiveImport) {
        g_pReleaseLiveImport();
        NullLiveImportFunctionPointers();
    }

    FreeLibrary(g_hLiveImportAPI);
    serial_port_close();
    printf("主线程退出");
    return (status);
}

bool checkLiveImportConnection(void)
{
    bool boolConnectionIsRunning = false;
    char *pszMessage = NULL;
    int iCount = 0;
    DWORD dwStatus = 0;

    // Walk through the variables and return the state
    //	__asm int 3;
    while (!boolConnectionIsRunning && (iCount < 100)) {	// Wait no more than 100 seconds
        dwStatus = g_pIsAppReady(&boolConnectionIsRunning);
        if (FAILED(dwStatus)) {
            pszMessage = "Unspecified internal error in Live Import interface.";
            break;
        }

        if (!boolConnectionIsRunning) {
            Sleep(1000);
            iCount++;
        }
    }

    if ((100 == iCount) && !boolConnectionIsRunning)
        pszMessage = "FTS is not ready to receive data via Live Import.";

    if (boolConnectionIsRunning && g_boolSendCannedMessage) {
        pszMessage = "FTS is ready to receive data via Live Import.  Start capture now.";
        MessageBox(NULL, pszMessage, "Success", MB_OK);
        pszMessage = NULL;
    }

    // Display a message if there is a problem.
    if (pszMessage != NULL) {
        fprintf(stderr, "%s\n", pszMessage);
        MessageBox(NULL, pszMessage, "Error", MB_OK);
    }

    return ((bool)((NULL == pszMessage) ? true : false));	// Return true for "all is well"
}

static uint8_t hci_log_pre_buffer[HCI_LOG_HEADER_LENGTH] = { 0 }; // header(2 byte) + direction(1 byte) + length(2 byte) + log(n byte)
static hci_log_parse_state_t state = HCI_LOG_PARSE_STATE_W4_HEADER1;
static uint16_t index = 0;
static hci_log_struct_t hci_log;
static uint16_t log_index = 0;
void bt_hci_log_handle(hci_log_struct_t *hci_log);

void serial_port_data_handler(uint8_t data)
{
    switch (state) {
        case HCI_LOG_PARSE_STATE_W4_HEADER1: {
            if (data == 0xF5) {
                hci_log_pre_buffer[index++] = data;
                state = HCI_LOG_PARSE_STATE_W4_HEADER2;
            }
            break;
        }
        case HCI_LOG_PARSE_STATE_W4_HEADER2: {
            if (data == 0x5A) {
                hci_log_pre_buffer[index++] = data;
                state = HCI_LOG_PARSE_STATE_W4_TYPE;
            }
            break;
        }
        case HCI_LOG_PARSE_STATE_W4_TYPE: {
            hci_log_pre_buffer[index++] = data;
            state = HCI_LOG_PARSE_STATE_W4_LENGTH;
            break;
        }
        case HCI_LOG_PARSE_STATE_W4_LENGTH: {
            hci_log_pre_buffer[index++] = data;
            if (index == HCI_LOG_HEADER_LENGTH) {
                state = HCI_LOG_PARSE_STATE_W4_LOG;
                memcpy(&hci_log, hci_log_pre_buffer, sizeof(hci_log_pre_buffer));
                hci_log.log = (uint8_t *)malloc(hci_log.log_length);
                assert(hci_log.log != NULL);
            }
            break;
        }
        case HCI_LOG_PARSE_STATE_W4_LOG: {
            hci_log.log[log_index++] = data;
            if (log_index == hci_log.log_length) {
                state = HCI_LOG_PARSE_STATE_W4_CHECK_SUM;
            }
            break;
        }
        case HCI_LOG_PARSE_STATE_W4_CHECK_SUM: {
            uint8_t check_sum = 0;
            check_sum += GET_LOW_BYTE(hci_log.header);
            check_sum += GET_HIGH_BYTE(hci_log.header);
            check_sum += hci_log.type;
            check_sum += GET_LOW_BYTE(hci_log.log_length);
            check_sum += GET_HIGH_BYTE(hci_log.log_length);
            for (uint16_t i = 0; i < hci_log.log_length; i++) {
                check_sum += hci_log.log[i];
            }
            if (data == check_sum) {
                bt_hci_log_handle(&hci_log);
            } else {
                printf("check sum error!\r\n");
            }
            state = HCI_LOG_PARSE_STATE_W4_HEADER1;
            free(hci_log.log);
            memset(&hci_log, 0, sizeof(hci_log_struct_t));
            memset(hci_log_pre_buffer, 0, sizeof(hci_log_pre_buffer));
            index = 0;
            log_index = 0;
            break;
        }
        default: {
            break;
        }
    }
}

void bt_hci_log_handle(hci_log_struct_t *hci_log)
{
    bt_hci_data_type_t hci_data_type;
    bt_hci_log_direction_t hci_log_direction;

    GetLocalTime(&systemtime);
    SystemTimeToFileTime(&systemtime, (FILETIME *)&timestamp);
    if (hci_log->type == BT_HCI_LOG_CMD) {
        hci_data_type = BT_HCI_CMD;
        hci_log_direction = HOST_TO_CONTROLLER;
    } else if (hci_log->type == BT_HCI_LOG_ACL_OUT) {
        hci_data_type = BT_HCI_ACL;
        hci_log_direction = HOST_TO_CONTROLLER;
    } else if (hci_log->type == BT_HCI_LOG_ACL_IN) {
        hci_data_type = BT_HCI_ACL;
        hci_log_direction = CONTROLLER_TO_HOST;
    } else if (hci_log->type == BT_HCI_LOG_EVT) {
        hci_data_type = BT_HCI_EVT;
        hci_log_direction = CONTROLLER_TO_HOST;
    }
    g_pSendFrame(
        hci_log->log_length,
        hci_log->log_length,
        hci_log->log,
        hci_data_type,	   // Bluetooth HCI Command
        hci_log_direction, // Bluetooth Host
        //  See BT Virtual.Dec and the [Configuration] in
        //  the LiveImport.Ini file
        timestamp);
}