// DriverNotifications.h
//
// This provides a common set of notifications between FTS & the generic
// interface, and between the generic interface and the COM interface.

#pragma once

#ifdef	__cplusplus
enum eNotificationTypes
#else
typedef enum
#endif
{
	eOpenSetIoDlg, eOpenHwDlg,eLiveModeStarted, eLiveModeStopped, eAppClosed,
	eStartCaptureToBuf, eStartCaptureToFile, ePauseResumeCapture, eClearBuffer, eCloseFile,
	eResync, eReset, 
	eCaptureStarted, eCaptureStopped, eCapturePaused, eHardwareCapturePaused, eCaptureResumed, eCaptureSuspended, eCaptureUnsuspended,
	eCanClose,
	eOkToClose, eNotOkToClose,
	eOpenSetupDlg,
	eCloseSetIoDlg,
	eCloseHwDlg, eCancelHwDlg,
	eCloseSetupDlg, eCancelSetupDlg,eOpenXmtDlg,eCloseXmtDlg,eCancelXmtDlg,
	eXmitOkToClose,eXmitNotOkToClose,
	eCloseXmitDlg, eOpenXmitDlg,
	eCanTransmit, eCannotTransmit,
	eOpenStatsView, eCloseStatsView, eDisableStartCapture, eEnableStartCapture,
	eDeleteConfig, eCanDeleteConfig, eOkToDeleteConfig, eNotOkToDeleteConfig,
	eBufferedDataOnHw, eSaveAndClearComplete, eLiveImportOperationComplete= eSaveAndClearComplete,
	eEnableNoExpertSystems, eEnableBtExpert, eEnableAudioExpert, eEnableAudioAndBTExpert,
	eOkToSaveAppData, eNotOkToSaveAppData, 
	eOkToSaveDriverData, eNotOkToSaveDriverData, 
	eSaveDriverData, eSaveDriverDataComplete, eSaveDriverDataError,
	ePauseCapture,
	eSaveCaptureFile, eOpenCaptureFile,
	eSniffingStarted, eSniffingStopped,
	eEnableUserInput, eDisableUserInput,
    eClearWithPrompt, 
    eMightPauseResume,
    eDriverCanEnableStartCaptureButton, eDriverCannotEnableStartCaptureButton,
    eAppCanEnableStartCaptureButton, eAppCannotEnableStartCaptureButton,
    eAutoRestartCapturingEnabled, eAutoRestartCapturingDisabled, 
	eSpectrumPathName, eCloseSpectrumFile,
	eRecordStarted, eRecordStopped,
    eDriverCanEnableStartRecord, eDriverCannotEnableStartRecord,
    eAppCanEnableStartRecord, eAppCannotEnableStartRecord
}
#ifdef	__cplusplus
 ;
#else
 eNotificationTypes;
#endif

typedef void (*NotificationType) (void* pThis);
typedef void (*NotificationType2) (void* pThis, int iDatasourceId);



#ifdef	__cplusplus
enum eKeepAliveEventTypes
#else
typedef enum
#endif
{
    eNoopWithoutTimestamp,
    eNoopWithTimestamp,
}
#ifdef	__cplusplus
 ;
#else
 eKeepAliveEventTypes;
#endif
