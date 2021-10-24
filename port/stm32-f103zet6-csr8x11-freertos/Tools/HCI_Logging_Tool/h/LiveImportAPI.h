// LiveImportAPI.h : Defines the initialization routines for the DLL.

#include "tchar.h"
#include "DriverNotifications.h"
#include "QueueMode.h"

// Type/value definitions:
#ifndef	__cplusplus
#define	bool BYTE
#define	true ((bool) 1)
#define	false ((bool) 0)
#endif

// API function pointer definitions:
typedef HRESULT (GetDllVersion)(TCHAR** pszDllVersion, int* piSize);
typedef HRESULT (InitializeLiveImport)(const TCHAR* szMemoryName, const TCHAR* szConfiguration, bool* pboolSuccess);
typedef HRESULT (InitializeLiveImportEx)(const TCHAR* szMemoryName, const TCHAR* szConfiguration, bool* pboolSuccess, EQueueMode eQueueMode);
typedef HRESULT (SetEnqueueMode)(EQueueMode eQueueMode);
typedef HRESULT (ReleaseLiveImport)(void);
typedef HRESULT (StillAlive)(bool* pboolIsAppAlive);
typedef HRESULT (IsAppReady)(bool* pboolIsAppReady);

typedef HRESULT (SendFrame)(
		int iOriginalLength,				// The "real" length of a frame. Some frames may be truncated, so this may not be the same as included length
		int iIncludedLength,				// The size of the data passed in this call
		const BYTE* pbytFrame,						// The actual bytes of the frame
		int iDrf,							// any errors or other data related flag
		int iStream,						// Which side this data comes from
		__int64 i64Timestamp);

typedef HRESULT (SendFrame2)(
		int iDatasourceId,
		int iOriginalLength,				// The "real" length of a frame. Some frames may be truncated, so this may not be the same as included length
		int iIncludedLength,				// The size of the data passed in this call
		const BYTE* pbytFrame,						// The actual bytes of the frame
		int iDrf,							// any errors or other data related flag
		int iStream,						// Which side this data comes from
		__int64 i64Timestamp);

typedef HRESULT (SendFrameWithComment)(
		int iOriginalLength,				// The "real" length of a frame. Some frames may be truncated, so this may not be the same as included length
		int iIncludedLength,				// The size of the data passed in this call
		const BYTE* pbytFrame,						// The actual bytes of the frame
		int iDrf,							// any errors or other data related flag
		int iStream,						// Which side this data comes from
		__int64 i64Timestamp,
		const TCHAR* ptcComment,  // Comment
		unsigned int uiCommentLength  // Length of comment
		);

typedef HRESULT (SendFrameWithCommentFromDatasource)(
		int iDatasourceId,
		int iOriginalLength,				// The "real" length of a frame. Some frames may be truncated, so this may not be the same as included length
		int iIncludedLength,				// The size of the data passed in this call
		const BYTE* pbytFrame,						// The actual bytes of the frame
		int iDrf,							// any errors or other data related flag
		int iStream,						// Which side this data comes from
		__int64 i64Timestamp,
		const TCHAR* ptcComment,  // Comment
		unsigned int uiCommentLength  // Length of comment
		);

typedef HRESULT (SendKeepAliveEvent)(int iDatasourceId, eKeepAliveEventTypes eKaeType, __int64 i64Timestamp);
typedef HRESULT (SendEvent)(BYTE bytData, int iDrf, int iStream, __int64 i64Timestamp);
typedef HRESULT (SendControlSignalChange)(int iNdrf, __int64 i64Timestamp);
typedef HRESULT (SendNondataFlagChange)(int iNdrf, __int64 i64Timestamp);
typedef HRESULT (SendBreak)(int iStream, __int64 i64Timestamp);
typedef HRESULT (SendFlowControl)(bool boolFlowControlIsOn, __int64 i64Timestamp);
typedef HRESULT (SendPower)(int iLevel, __int64 i64Timestamp);
typedef HRESULT (SendConnectionStatus)(bool boolIsConnected, __int64 i64Timestamp);
typedef HRESULT (SendConfigurationString)(const TCHAR* szConfiguration);
typedef HRESULT (SendConfigurationString2)(int iDatasourceId, const TCHAR* szConfiguration);
typedef HRESULT (SendSetIODialogHwnd) (const HWND hwnd);
typedef HRESULT (SendXmitDialogHwnd) (const HWND hwnd);
typedef HRESULT (SendSpecialEvent)(int iStream, int iEventNumber, __int64 i64Timestamp);
typedef HRESULT (SendStartOfFrame)(int iStream, __int64 i64Timestamp);
typedef HRESULT (SendEndOfFrame)(int iStream, __int64 i64Timestamp);
typedef HRESULT (SendAbortedFrame)(int iStream, __int64 i64Timestamp);
typedef HRESULT (SendByteSimple)(BYTE byData, __int64 i64Timestamp);
typedef HRESULT (CheckForMessages)();
typedef HRESULT (GetAppVersionNumber)(TCHAR** pszAppVersionNumber, int* piSize);
typedef HRESULT (GetAppSerialNumber)(TCHAR** pszAppSerialNumber, int* piSize);
typedef HRESULT (GetAppDisplayedConfigurationName)(TCHAR** pszAppConfigName, int* piSize);
typedef HRESULT (GetSerialNumberSectionKeyValuePairs)(TCHAR** pszKeyValuePairs, int* piSize);
typedef HRESULT (GetDriverSavePath)(TCHAR** pszDriverSavePath, int* piSize);
typedef HRESULT (GetDriverSaveName)(TCHAR** pszDriverSaveName, int* piSize);
typedef HRESULT (RegisterNotification)(eNotificationTypes eType, NotificationType pNotification, void* pThis);
typedef HRESULT (RegisterNotification2)(eNotificationTypes eType, NotificationType2 pNotification, void* pThis);
typedef HRESULT (SendNotification)(eNotificationTypes eType);
typedef HRESULT (SendNotification2)(int iDatasourceId, eNotificationTypes eType);
typedef HRESULT (SendArraySimple)(BYTE* pbytData, int iLength, __int64 i64Timestamp);
typedef HRESULT (SendStringSimple)(TCHAR* szData, __int64 i64Timestamp);
typedef HRESULT (SendNumberOfLostMessages)(const int iNumberOfLostMessages);
typedef HRESULT (UpdateStat)(int iStream, int iStatNumber, __int64 i64IncrementAmount);
typedef HRESULT (FramesLost)(int iFramesLost);
typedef HRESULT (FramesLost2)(int iDatasourceId, int iFramesLost);
typedef HRESULT (SetExePath)(TCHAR* szServerPath);
typedef HRESULT (SendPostNotify)(UINT uiId, WPARAM wParam, LPARAM lParam);
typedef HRESULT (SendComment)(const TCHAR* szComment);
typedef HRESULT (AddNamedData)(const TCHAR* szName, const TCHAR* szValue);
typedef HRESULT (SetDataSourceFilterName)(const TCHAR* szFilterName);
typedef HRESULT (SendQueueStats)(unsigned int uiProcessId, __int64 i64NumQueued, const TCHAR* szDataSrcName);
typedef HRESULT (SaveAndClear)(const TCHAR* szSavePath);
typedef HRESULT (SaveCapture)(const TCHAR* szSavePath);
typedef HRESULT (ClearCapture)(void);
typedef HRESULT (ExportHtmlToPath)(const TCHAR* htmlPath);
typedef HRESULT (GetLiveImportInitializationStatus)(long* pliveImportInitializationStatus);
typedef HRESULT (SendSpectrumPathName)(const TCHAR* szSpectrumPathName);
typedef HRESULT (SendStartingTimestamp)(__int64 i64Timestamp);
typedef HRESULT (GetNumFreeFrameBytes)(int *pAvailFrameBytes);
typedef HRESULT (GetNumAppEventEnqueueBusyWaits)(int *pNumBusyWaits);

#ifndef	SIMPLIFIED_LIVE_IMPORT
// API function pointer variable definitions:
GetDllVersion* g_pGetDllVersion= NULL;
InitializeLiveImport* g_pInitializeLiveImport= NULL;
InitializeLiveImportEx* g_pInitializeLiveImportEx= NULL;
SetEnqueueMode* g_pSetEnqueueMode= NULL;
ReleaseLiveImport* g_pReleaseLiveImport= NULL;
StillAlive* g_pStillAlive= NULL;
IsAppReady* g_pIsAppReady= NULL;
SendFrame* g_pSendFrame= NULL;
SendFrame2* g_pSendFrame2= NULL;
SendFrameWithComment* g_pSendFrameWithComment= NULL;
SendFrameWithCommentFromDatasource* g_pSendFrameWithCommentFromDatasource= NULL;
SendKeepAliveEvent* g_pSendKeepAliveEvent= NULL;
SendEvent* g_pSendEvent= NULL;
SendControlSignalChange* g_pSendControlSignalChange= NULL;
SendNondataFlagChange* g_pSendNondataFlagChange= NULL;
SendBreak* g_pSendBreak= NULL;
SendFlowControl* g_pSendFlowControl= NULL;
SendConnectionStatus* g_pSendConnectionStatus= NULL;
SendConfigurationString* g_pSendConfigurationString= NULL;
SendConfigurationString2* g_pSendConfigurationString2= NULL;
SendSetIODialogHwnd* g_pSendSetIODialogHwnd= NULL;
SendXmitDialogHwnd* g_pSendXmitDialogHwnd= NULL;
SendSpecialEvent* g_pSendSpecialEvent= NULL;
SendStartOfFrame* g_pSendStartOfFrame= NULL;
SendEndOfFrame* g_pSendEndOfFrame= NULL;
SendAbortedFrame* g_pSendAbortedFrame= NULL;
SendByteSimple* g_pSendByteSimple= NULL;
CheckForMessages* g_pCheckForMessages= NULL;
GetAppVersionNumber* g_pGetAppVersionNumber= NULL;
GetAppSerialNumber* g_pGetAppSerialNumber= NULL;
GetAppDisplayedConfigurationName* g_pGetAppDisplayedConfigurationName= NULL;
GetSerialNumberSectionKeyValuePairs* g_pGetSerialNumberSectionKeyValuePairs= NULL;
GetDriverSavePath* g_pGetDriverSavePath= NULL;
GetDriverSaveName* g_pGetDriverSaveName= NULL;
RegisterNotification* g_pRegisterNotification= NULL;
RegisterNotification2* g_pRegisterNotification2= NULL;
SendNotification* g_pSendNotification= NULL;
SendNotification2* g_pSendNotification2= NULL;
SendArraySimple* g_pSendArraySimple= NULL;
SendStringSimple* g_pSendStringSimple= NULL;
SendNumberOfLostMessages* g_pSendNumberOfLostMessages= NULL;
UpdateStat* g_pUpdateStat= NULL;
FramesLost* g_pFramesLost= NULL;
FramesLost2* g_pFramesLost2= NULL;
SetExePath* g_pSetExePath= NULL;
SendPostNotify* g_pSendPostNotify= NULL;
SendComment* g_pSendComment= NULL;
AddNamedData* g_pAddNamedData= NULL;
SetDataSourceFilterName* g_pSetDataSourceFilterName= NULL;;
SendQueueStats* g_pSendQueueStats= NULL;
SaveAndClear* g_pSaveAndClear= NULL;
SaveCapture* g_pSaveCapture= NULL;
ClearCapture* g_pClearCapture= NULL;
ExportHtmlToPath* g_pExportHtmlToPath= NULL;
GetLiveImportInitializationStatus* g_pGetLiveImportInitializationStatus= NULL;
SendSpectrumPathName* g_pSendSpectrumPathName= NULL;
SendStartingTimestamp* g_pSendStartingTimestamp= NULL;
GetNumFreeFrameBytes* g_pGetNumFreeFrameBytes= NULL;
GetNumAppEventEnqueueBusyWaits* g_pGetNumAppEventEnqueueBusyWaits= NULL;

// Library definitions:
TCHAR* g_pszLibraryName= _T(".\\LiveImportAPI.dll");
TCHAR* g_pszLibraryName_x64= _T(".\\LiveImportAPI_x64.dll");
HMODULE g_hLiveImportAPI= NULL;

bool ShowFailMessage(char* pszProcName)
{
	TCHAR szError[1024]= {0};
	_stprintf_s(szError, _countof(szError), _T("Failed to get address of function \"%s\"."), (TCHAR*)pszProcName);
	MessageBox(NULL, szError, _T("Error"), MB_OK);
	FreeLibrary(g_hLiveImportAPI);
	return (false);
}

bool LoadAPIFunctions(void)
{
	char* pszProcName= NULL;
	TCHAR szError[1024]= {0};
	
	if(NULL == g_hLiveImportAPI)
	{
		DWORD lastError= GetLastError();
		_stprintf_s(szError, _countof(szError), _T("Failed to load module \"%s\" - error %d"), g_pszLibraryName, lastError);
		MessageBox(NULL, szError, _T("Error"), MB_OK);
		return (false);
	}
	
	pszProcName= "InitializeLiveImport";
	g_pInitializeLiveImport= (InitializeLiveImport*)GetProcAddress(g_hLiveImportAPI, pszProcName);
	if(NULL == g_pInitializeLiveImport)
		return (ShowFailMessage(pszProcName));
	
	pszProcName= "InitializeLiveImportEx";
	g_pInitializeLiveImportEx= (InitializeLiveImportEx*)GetProcAddress(g_hLiveImportAPI, pszProcName);
	if(NULL == g_pInitializeLiveImportEx)
		return (ShowFailMessage(pszProcName));
	
	pszProcName= "SetEnqueueMode";
	g_pSetEnqueueMode= (SetEnqueueMode*)GetProcAddress(g_hLiveImportAPI, pszProcName);
	if(NULL == g_pSetEnqueueMode)
		return (ShowFailMessage(pszProcName));
	
	pszProcName= "ReleaseLiveImport";
	g_pReleaseLiveImport= (ReleaseLiveImport*)GetProcAddress(g_hLiveImportAPI, pszProcName);
	if(NULL == g_pReleaseLiveImport)
		return (ShowFailMessage(pszProcName));
	
	pszProcName= "StillAlive";
	g_pStillAlive= (StillAlive*)GetProcAddress(g_hLiveImportAPI, pszProcName);
	if(NULL == g_pStillAlive)
		return (ShowFailMessage(pszProcName));

	pszProcName= "IsAppReady";
	g_pIsAppReady= (IsAppReady*)GetProcAddress(g_hLiveImportAPI, pszProcName);
	if(NULL == g_pIsAppReady)
		return (ShowFailMessage(pszProcName));
	
	pszProcName= "SendFrame";
	g_pSendFrame= (SendFrame*)GetProcAddress(g_hLiveImportAPI, pszProcName);
	if(NULL == g_pSendFrame)
		return (ShowFailMessage(pszProcName));

	pszProcName= "SendFrame2";
	g_pSendFrame2= (SendFrame2*)GetProcAddress(g_hLiveImportAPI, pszProcName);
	if(NULL == g_pSendFrame2)
		return (ShowFailMessage(pszProcName));
	
	pszProcName= "SendFrameWithComment";
	g_pSendFrameWithComment= (SendFrameWithComment*)GetProcAddress(g_hLiveImportAPI, pszProcName);
	if(NULL == g_pSendFrameWithComment)
		return (ShowFailMessage(pszProcName));
	
	pszProcName= "SendFrameWithCommentFromDatasource";
	g_pSendFrameWithCommentFromDatasource= (SendFrameWithCommentFromDatasource*)GetProcAddress(g_hLiveImportAPI, pszProcName);
	if(NULL == g_pSendFrameWithCommentFromDatasource)
		return (ShowFailMessage(pszProcName));

	pszProcName= "SendKeepAliveEvent";
	g_pSendKeepAliveEvent= (SendKeepAliveEvent*)GetProcAddress(g_hLiveImportAPI, pszProcName);
	if(NULL == g_pSendKeepAliveEvent)
		return (ShowFailMessage(pszProcName));
	
	pszProcName= "SendEvent";
	g_pSendEvent= (SendEvent*)GetProcAddress(g_hLiveImportAPI, pszProcName);
	if(NULL == g_pSendEvent)
		return (ShowFailMessage(pszProcName));
	
	pszProcName= "SendControlSignalChange";
	g_pSendControlSignalChange= (SendControlSignalChange*)GetProcAddress(g_hLiveImportAPI, pszProcName);
	if(NULL == g_pSendControlSignalChange)
		return (ShowFailMessage(pszProcName));
	
	pszProcName= "SendNondataFlagChange";
	g_pSendNondataFlagChange= (SendNondataFlagChange*)GetProcAddress(g_hLiveImportAPI, pszProcName);
	if(NULL == g_pSendNondataFlagChange)
		return (ShowFailMessage(pszProcName));
	
	pszProcName= "SendBreak";
	g_pSendBreak= (SendBreak*)GetProcAddress(g_hLiveImportAPI, pszProcName);
	if(NULL == g_pSendBreak)
		return (ShowFailMessage(pszProcName));
	
	pszProcName= "SendFlowControl";
	g_pSendFlowControl= (SendFlowControl*)GetProcAddress(g_hLiveImportAPI, pszProcName);
	if(NULL == g_pSendFlowControl)
		return (ShowFailMessage(pszProcName));
	
	pszProcName= "SendConnectionStatus";
	g_pSendConnectionStatus= (SendConnectionStatus*)GetProcAddress(g_hLiveImportAPI, pszProcName);
	if(NULL == g_pSendConnectionStatus)
		return (ShowFailMessage(pszProcName));
	
	pszProcName= "SendConfigurationString";
	g_pSendConfigurationString= (SendConfigurationString*)GetProcAddress(g_hLiveImportAPI, pszProcName);
	if(NULL == g_pSendConfigurationString)
		return (ShowFailMessage(pszProcName));

	pszProcName= "SendConfigurationString2";
	g_pSendConfigurationString2= (SendConfigurationString2*)GetProcAddress(g_hLiveImportAPI, pszProcName);
	if(NULL == g_pSendConfigurationString2)
		return (ShowFailMessage(pszProcName));
	
	pszProcName= "SendSetIODialogHwnd";
	g_pSendSetIODialogHwnd= (SendSetIODialogHwnd*)GetProcAddress(g_hLiveImportAPI, pszProcName);
	if(NULL == g_pSendSetIODialogHwnd)
		return (ShowFailMessage(pszProcName));

	pszProcName= "SendXmitDialogHwnd";
	g_pSendXmitDialogHwnd= (SendXmitDialogHwnd*)GetProcAddress(g_hLiveImportAPI, pszProcName);
	if(NULL == g_pSendXmitDialogHwnd)
		return (ShowFailMessage(pszProcName));
	
	pszProcName= "SendSpecialEvent";
	g_pSendSpecialEvent= (SendSpecialEvent*)GetProcAddress(g_hLiveImportAPI, pszProcName);
	if(NULL == g_pSendSpecialEvent)
		return (ShowFailMessage(pszProcName));
	
	pszProcName= "SendControlSignalChange";
	g_pSendControlSignalChange= (SendControlSignalChange*)GetProcAddress(g_hLiveImportAPI, pszProcName);
	if(NULL == g_pSendControlSignalChange)
		return (ShowFailMessage(pszProcName));
	
	pszProcName= "SendStartOfFrame";
	g_pSendStartOfFrame= (SendStartOfFrame*)GetProcAddress(g_hLiveImportAPI, pszProcName);
	if(NULL == g_pSendStartOfFrame)
		return (ShowFailMessage(pszProcName));
	
	pszProcName= "SendEndOfFrame";
	g_pSendEndOfFrame= (SendEndOfFrame*)GetProcAddress(g_hLiveImportAPI, pszProcName);
	if(NULL == g_pSendEndOfFrame)
		return (ShowFailMessage(pszProcName));
	
	pszProcName= "SendAbortedFrame";
	g_pSendAbortedFrame= (SendAbortedFrame*)GetProcAddress(g_hLiveImportAPI, pszProcName);
	if(NULL == g_pSendAbortedFrame)
		return (ShowFailMessage(pszProcName));
	
	pszProcName= "SendByteSimple";
	g_pSendByteSimple= (SendByteSimple*)GetProcAddress(g_hLiveImportAPI, pszProcName);
	if(NULL == g_pSendByteSimple)
		return (ShowFailMessage(pszProcName));
	
	pszProcName= "CheckForMessages";
	g_pCheckForMessages= (CheckForMessages*)GetProcAddress(g_hLiveImportAPI, pszProcName);
	if(NULL == g_pCheckForMessages)
		return (ShowFailMessage(pszProcName));
	
	pszProcName= "GetAppVersionNumber";
	g_pGetAppVersionNumber= (GetAppVersionNumber*)GetProcAddress(g_hLiveImportAPI, pszProcName);
	if(NULL == g_pGetAppVersionNumber)
		return (ShowFailMessage(pszProcName));
	
	pszProcName= "GetAppSerialNumber";
	g_pGetAppSerialNumber= (GetAppSerialNumber*)GetProcAddress(g_hLiveImportAPI, pszProcName);
	if(NULL == g_pGetAppSerialNumber)
		return (ShowFailMessage(pszProcName));
	
	pszProcName= "GetAppDisplayedConfigurationName";
	g_pGetAppDisplayedConfigurationName= (GetAppDisplayedConfigurationName*)GetProcAddress(g_hLiveImportAPI, pszProcName);
	if(NULL == g_pGetAppDisplayedConfigurationName)
		return (ShowFailMessage(pszProcName));
	
	pszProcName= "GetSerialNumberSectionKeyValuePairs";
	g_pGetSerialNumberSectionKeyValuePairs= (GetSerialNumberSectionKeyValuePairs*)GetProcAddress(g_hLiveImportAPI, pszProcName);
	if(NULL == g_pGetSerialNumberSectionKeyValuePairs)
		return (ShowFailMessage(pszProcName));

    pszProcName= "GetDriverSavePath";
    g_pGetDriverSavePath= (GetDriverSavePath*)GetProcAddress(g_hLiveImportAPI, pszProcName);
	if(NULL == g_pGetDriverSavePath)
		return (ShowFailMessage(pszProcName));

    pszProcName= "GetDriverSaveName";
    g_pGetDriverSaveName= (GetDriverSaveName*)GetProcAddress(g_hLiveImportAPI, pszProcName);
	if(NULL == g_pGetDriverSaveName)
		return (ShowFailMessage(pszProcName));

	pszProcName= "RegisterNotification";
	g_pRegisterNotification= (RegisterNotification*)GetProcAddress(g_hLiveImportAPI, pszProcName);
	if(NULL == g_pRegisterNotification)
		return (ShowFailMessage(pszProcName));

	pszProcName= "RegisterNotification2";
	g_pRegisterNotification2= (RegisterNotification2*)GetProcAddress(g_hLiveImportAPI, pszProcName);
	if(NULL == g_pRegisterNotification2)
		return (ShowFailMessage(pszProcName));
	
	pszProcName= "SendNotification";
	g_pSendNotification= (SendNotification*)GetProcAddress(g_hLiveImportAPI, pszProcName);
	if(NULL == g_pSendNotification)
		return (ShowFailMessage(pszProcName));
	
	pszProcName= "SendArraySimple";
	g_pSendArraySimple= (SendArraySimple*)GetProcAddress(g_hLiveImportAPI, pszProcName);
	if(NULL == g_pSendArraySimple)
		return (ShowFailMessage(pszProcName));
	
	pszProcName= "SendStringSimple";
	g_pSendStringSimple= (SendStringSimple*)GetProcAddress(g_hLiveImportAPI, pszProcName);
	if(NULL == g_pSendStringSimple)
		return (ShowFailMessage(pszProcName));
	
	pszProcName= "SendNumberOfLostMessages";
	g_pSendNumberOfLostMessages= (SendNumberOfLostMessages*)GetProcAddress(g_hLiveImportAPI, pszProcName);
	if(NULL == g_pSendNumberOfLostMessages)
		return (ShowFailMessage(pszProcName));
	
	pszProcName= "UpdateStat";
	g_pUpdateStat= (UpdateStat*)GetProcAddress(g_hLiveImportAPI, pszProcName);
	if(NULL == g_pUpdateStat)
		return (ShowFailMessage(pszProcName));
	
	pszProcName= "FramesLost";
	g_pFramesLost= (FramesLost*)GetProcAddress(g_hLiveImportAPI, pszProcName);
	if(NULL == g_pFramesLost)
		return (ShowFailMessage(pszProcName));
	
	pszProcName= "SetExePath";
	g_pSetExePath= (SetExePath*)GetProcAddress(g_hLiveImportAPI, pszProcName);
	if(NULL == g_pSetExePath)
		return (ShowFailMessage(pszProcName));

	pszProcName= "SendPostNotify";
	g_pSendPostNotify= (SendPostNotify*)GetProcAddress(g_hLiveImportAPI, pszProcName);
	if(NULL == g_pSendPostNotify)
		return (ShowFailMessage(pszProcName));

	pszProcName= "SendComment";
	g_pSendComment= (SendComment*)GetProcAddress(g_hLiveImportAPI, pszProcName);
	if(NULL == g_pSendComment)
		return (ShowFailMessage(pszProcName));
	
	pszProcName= "AddNamedData";
	g_pAddNamedData= (AddNamedData*)GetProcAddress(g_hLiveImportAPI, pszProcName);
	if(NULL == g_pAddNamedData)
		return (ShowFailMessage(pszProcName));
	
	pszProcName= "SetDataSourceFilterName";
	g_pSetDataSourceFilterName= (SetDataSourceFilterName*)GetProcAddress(g_hLiveImportAPI, pszProcName);
	if(NULL == g_pSetDataSourceFilterName)
		return (ShowFailMessage(pszProcName));
	
	pszProcName= "SendQueueStats";
	g_pSendQueueStats= (SendQueueStats*)GetProcAddress(g_hLiveImportAPI, pszProcName);
	if(NULL == g_pSendQueueStats)
		return (ShowFailMessage(pszProcName));
	
	pszProcName= "SaveAndClear";
	g_pSaveAndClear= (SaveAndClear*)GetProcAddress(g_hLiveImportAPI, pszProcName);
	if(NULL == g_pSaveAndClear)
		return (ShowFailMessage(pszProcName));
	
	pszProcName= "SaveCapture";
	g_pSaveCapture= (SaveCapture*)GetProcAddress(g_hLiveImportAPI, pszProcName);
	if(NULL == g_pSaveCapture)
		return (ShowFailMessage(pszProcName));
	
	pszProcName= "ClearCapture";
	g_pClearCapture= (ClearCapture*)GetProcAddress(g_hLiveImportAPI, pszProcName);
	if(NULL == g_pClearCapture)
		return (ShowFailMessage(pszProcName));
	
	pszProcName= "ExportHtmlToPath";
	g_pExportHtmlToPath= (ExportHtmlToPath*)GetProcAddress(g_hLiveImportAPI, pszProcName);
	if(NULL == g_pExportHtmlToPath)
		return (ShowFailMessage(pszProcName));
	
	pszProcName= "GetLiveImportInitializationStatus";
	g_pGetLiveImportInitializationStatus= (GetLiveImportInitializationStatus*)GetProcAddress(g_hLiveImportAPI, pszProcName);
	if(NULL == g_pGetLiveImportInitializationStatus)
		return (ShowFailMessage(pszProcName));
	
	pszProcName= "SendSpectrumPathName";
	g_pSendSpectrumPathName= (SendSpectrumPathName*)GetProcAddress(g_hLiveImportAPI, pszProcName);
	if(NULL == g_pSendSpectrumPathName)
		return (ShowFailMessage(pszProcName));

	pszProcName= "SendStartingTimestamp";
	g_pSendStartingTimestamp= (SendStartingTimestamp*)GetProcAddress(g_hLiveImportAPI, pszProcName);
	if(NULL == g_pSendStartingTimestamp)
		return (ShowFailMessage(pszProcName));
    
  	pszProcName= "GetNumFreeFrameBytes";
	g_pGetNumFreeFrameBytes= (GetNumFreeFrameBytes*)GetProcAddress(g_hLiveImportAPI, pszProcName);
	if(NULL == g_pGetNumFreeFrameBytes)
		return (ShowFailMessage(pszProcName));

   	pszProcName= "GetNumAppEventEnqueueBusyWaits";
	g_pGetNumAppEventEnqueueBusyWaits= (GetNumAppEventEnqueueBusyWaits*)GetProcAddress(g_hLiveImportAPI, pszProcName);
	if(NULL == g_pGetNumAppEventEnqueueBusyWaits)
		return (ShowFailMessage(pszProcName));

	return (true);
}

bool LoadLiveImportAPIFunctions(void)
{
	DWORD lastError= ERROR_SUCCESS;
	TCHAR szError[1024]= {0};
	TCHAR tcFullPath[_MAX_PATH + 1]= {0};
	g_hLiveImportAPI= LoadLibrary(g_pszLibraryName);
	if(NULL == g_hLiveImportAPI)
	{
		if(0 == GetFullPathName(g_pszLibraryName, _MAX_PATH, tcFullPath, NULL))  // Failed
		{
			_tcscpy_s(tcFullPath, _countof(tcFullPath), g_pszLibraryName);
		}
		lastError= GetLastError();
		_stprintf_s(szError, _countof(szError), _T("Failed to load module \"%s\" - error %d"), tcFullPath, lastError);
		MessageBox(NULL, szError, _T("Error"), MB_OK);
		return (false);
	}
	
	return (LoadAPIFunctions());
}

bool LoadLiveImportAPIFunctions_x64(void)
{
	DWORD lastError= ERROR_SUCCESS;
	TCHAR szError[1024]= {0};
	TCHAR tcFullPath[_MAX_PATH + 1]= {0};
	g_hLiveImportAPI= LoadLibrary(g_pszLibraryName_x64);
	if(NULL == g_hLiveImportAPI)
	{
		if(0 == GetFullPathName(g_pszLibraryName, _MAX_PATH, tcFullPath, NULL))  // Failed
		{
			_tcscpy_s(tcFullPath, _countof(tcFullPath), g_pszLibraryName_x64);
		}
		_stprintf_s(szError, _countof(szError), _T("Failed to load module \"%s\" - error %d"), tcFullPath, lastError);
		MessageBox(NULL, szError, _T("Error"), MB_OK);
		return (false);
	}
	
	return (LoadAPIFunctions());
}

void NullLiveImportFunctionPointers(void)
{
	g_pGetDllVersion= NULL;
	g_pInitializeLiveImport= NULL;
	g_pReleaseLiveImport= NULL;
	g_pStillAlive= NULL;
	g_pIsAppReady= NULL;
	g_pSendFrame= NULL;
	g_pSendEvent= NULL;
	g_pSendControlSignalChange= NULL;
	g_pSendNondataFlagChange= NULL;
	g_pSendBreak= NULL;
	g_pSendFlowControl= NULL;
	g_pSendConnectionStatus= NULL;
	g_pSendConfigurationString= NULL;
	g_pSendSetIODialogHwnd= NULL;
	g_pSendXmitDialogHwnd= NULL;
	g_pSendSpecialEvent= NULL;
	g_pSendStartOfFrame= NULL;
	g_pSendEndOfFrame= NULL;
	g_pSendAbortedFrame= NULL;
	g_pSendByteSimple= NULL;
	g_pCheckForMessages= NULL;
	g_pGetAppVersionNumber= NULL;
	g_pGetAppSerialNumber= NULL;
	g_pGetAppDisplayedConfigurationName= NULL;
	g_pGetSerialNumberSectionKeyValuePairs= NULL;
	g_pGetDriverSavePath= NULL;
	g_pGetDriverSaveName= NULL;
	g_pRegisterNotification= NULL;
	g_pSendNotification= NULL;
	g_pSendArraySimple= NULL;
	g_pSendStringSimple= NULL;
	g_pSendNumberOfLostMessages= NULL;
	g_pUpdateStat= NULL;
	g_pFramesLost= NULL;
	g_pSetExePath= NULL;
	g_pSendPostNotify = NULL;

	g_pSendFrame2 = NULL;
	g_pSendFrameWithComment= NULL;
	g_pSendFrameWithCommentFromDatasource= NULL;
	g_pSendKeepAliveEvent= NULL;
	g_pFramesLost2= NULL;
	g_pSendConfigurationString2= NULL;
	g_pSendNotification2= NULL;
	g_pRegisterNotification2 = NULL;
	g_pSendQueueStats = NULL;
	g_pSaveAndClear = NULL;
	g_pSaveCapture= NULL;
	g_pClearCapture= NULL;
	g_pExportHtmlToPath= NULL;
	g_pGetLiveImportInitializationStatus= NULL;
	g_pSendSpectrumPathName= NULL;
	g_pSendStartingTimestamp= NULL;
	g_pGetNumFreeFrameBytes= NULL;
	g_pGetNumAppEventEnqueueBusyWaits= NULL;
}
#endif
