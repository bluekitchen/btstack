#include "serial_port.h"
#include "fifo.h"
#define SERIAL_PORT_FIFO_DEPTH (5 * 1024)

static HANDLE hCom = NULL; //全局变量，串口句柄
//static OVERLAPPED *p_overlapped = NULL;
static HANDLE ghEvent = NULL;

DWORD WINAPI serial_port_read_thread(LPVOID lpParam)
{
    COMSTAT ComStat;
    DWORD dwErrorFlags;
    BOOL bReadStat;
    DWORD dwEventMask = 0;
    unsigned int read_length = 0;
    unsigned char Data = 0;
    printf("serial_port_read_thread started\r\n");
    BYTE status = fifo_init(SERIAL_PORT_FIFO_DEPTH);
    assert(status);

    while (1) {
        //BOOL status = WaitCommEvent(hCom, &dwEventMask, NULL);
        ClearCommError(hCom, &dwErrorFlags, &ComStat);
        bReadStat = ReadFile(hCom, &Data, sizeof(Data), &read_length, NULL);
        DWORD error = GetLastError();
        if (!bReadStat || read_length == 0) {
            //printf("read file failed, error = %d \r\n", error);
            Sleep(5);
            continue;
        }
        mutex_lock();
        status = fifo_push(Data);
        if (status == 0) {
            //printf("fifo push fail \r\n");
        }
        mutex_unlock();
    }
    fifo_deinit();
    printf("serial_port_read_thread finished\r\n");
    return 0;
}

/*
VOID WINAPI serial_port_read_done_callback(DWORD dwErrorCode, DWORD dwNumberOfBytesTransfered, LPOVERLAPPED lpOverlapped)
{
	if (dwErrorCode == 0 && dwNumberOfBytesTransfered) {
		printf("error_code = %d, number_of_byte_read = %d, event = %d \r\n", dwErrorCode, dwNumberOfBytesTransfered, lpOverlapped->hEvent);

		valid_rx_buffer_length = dwNumberOfBytesTransfered;
		unsigned char *hci_log_buf = get_hci_log_buffer();
		memcpy(hci_log_buf, rx_buffer, valid_rx_buffer_length);
		set_ready_to_show_hci_log(TRUE);
	}
	PurgeComm(hCom, PURGE_TXABORT | PURGE_RXABORT | PURGE_TXCLEAR | PURGE_RXCLEAR);
	SetEvent(ghEvent);

	memset(rx_buffer, 0, SERIAL_PORT_RX_BUF_LENGTH);
}
*/
/*
int serial_port_read_port_request()
{
	BOOL ret;
	DWORD err;
	unsigned int i = 0;
	COMSTAT ComStat;
	DWORD dwErrorFlags;
	ClearCommError(hCom, &dwErrorFlags, &ComStat);

	for (i = 0; i < 1; i++) {
		ret = ReadFileEx(hCom, rx_buffer, SERIAL_PORT_RX_BUF_LENGTH, p_overlapped, serial_port_read_done_callback);
		if (ret) {
			return TRUE;
		}
		err = GetLastError();
		// Handle recoverable error
		if (err == ERROR_INVALID_USER_BUFFER || err == ERROR_NOT_ENOUGH_QUOTA || err == ERROR_NOT_ENOUGH_MEMORY) {
			Sleep(50); // Wait around and try later
			continue;
		} else {
			break;
		}
	}
	printf("ReadFileEx failed.\n");
    return FALSE;
}
*/

int serial_port_open(LPCWSTR COMx, int BaudRate)
{
    //p_overlapped = (OVERLAPPED *)malloc(sizeof(OVERLAPPED));
    //if (p_overlapped == NULL) {
    //	return FALSE;
    //}
    //memset(p_overlapped, 0 ,sizeof(OVERLAPPED));

    ghEvent = CreateEvent(
                  NULL,// No security
                  TRUE,// Manual reset-extremely important!
                  FALSE,// Initially set Event to non-signaled state
                  NULL); // No name
    if (ghEvent == NULL) {
        return FALSE;
    }

    hCom = CreateFile(COMx, //COM1口
                      GENERIC_READ | GENERIC_WRITE, //允许读和写
                      0, //独占方式
                      NULL,
                      OPEN_EXISTING, //打开而不是创建
                      0,//FILE_FLAG_OVERLAPPED,//  (同步方式设置为0) FILE_ATTRIBUTE_NORMAL |
                      NULL);
    if (hCom == INVALID_HANDLE_VALUE) {
        return FALSE;
    }
    BOOL ret = SetupComm(hCom, 1024, 1024); //输入缓冲区和输出缓冲区的大小都是1024

    //设定读写超时
    COMMTIMEOUTS TimeOuts;
    TimeOuts.ReadIntervalTimeout = 50;
    TimeOuts.ReadTotalTimeoutMultiplier = 50;
    TimeOuts.ReadTotalTimeoutConstant = 10; //设定写超时
    TimeOuts.WriteTotalTimeoutMultiplier = 50;
    TimeOuts.WriteTotalTimeoutConstant = 10;
    SetCommTimeouts(hCom, &TimeOuts); //设置超时

    DCB dcb;
    GetCommState(hCom, &dcb);
    dcb.BaudRate = BaudRate;		//设置波特率为BaudRate
    dcb.ByteSize = 8;					//每个字节有8位
    dcb.Parity = NOPARITY;			//无奇偶校验位
    dcb.StopBits = ONESTOPBIT;		//一个停止位
    dcb.fOutX = FALSE;
    dcb.fInX = FALSE;
    ret = SetCommState(hCom, &dcb);		//设置参数到hCom
    ret = PurgeComm(hCom, PURGE_TXCLEAR | PURGE_RXCLEAR);//清空缓存区
    //PURGE_TXABORT 中断所有写操作并立即返回，即使写操作还没有完成。
    //PURGE_RXABORT 中断所有读操作并立即返回，即使读操作还没有完成。
    //PURGE_TXCLEAR 清除输出缓冲区
    //PURGE_RXCLEAR 清除输入缓冲区

    BOOL status = SetCommMask(hCom, EV_RXCHAR);
    assert(status);

    return TRUE;
}

int serial_port_write(char lpOutBuffer[])	//同步写串口
{
    DWORD dwBytesWrite = sizeof(lpOutBuffer);
    COMSTAT ComStat;
    DWORD dwErrorFlags;
    BOOL bWriteStat;
    ClearCommError(hCom, &dwErrorFlags, &ComStat);
    bWriteStat = WriteFile(hCom, lpOutBuffer, dwBytesWrite, &dwBytesWrite, NULL);
    if (!bWriteStat) {
        printf("写串口失败!\n");
        return FALSE;
    }
    PurgeComm(hCom, PURGE_TXABORT | PURGE_RXABORT | PURGE_TXCLEAR | PURGE_RXCLEAR);
    return TRUE;
}

int serial_port_read(char *ptr, unsigned int buffer_size, unsigned int *acture_read_length)
{
    COMSTAT ComStat;
    DWORD dwErrorFlags;
    BOOL bReadStat;
    DWORD dwEventMask = 0;

    BOOL status = WaitCommEvent(hCom, &dwEventMask, NULL);
    assert(status);
    ClearCommError(hCom, &dwErrorFlags, &ComStat);
    bReadStat = ReadFile(hCom, ptr, buffer_size, acture_read_length, NULL);
    DWORD error = GetLastError();
    if (!bReadStat) {
        printf("读串口失败！error = %d \r\n", error);
        return FALSE;
    }
    return TRUE;
}

void serial_port_close(void)
{
    //free(p_overlapped);
    fifo_deinit();
    CloseHandle(hCom);
}
