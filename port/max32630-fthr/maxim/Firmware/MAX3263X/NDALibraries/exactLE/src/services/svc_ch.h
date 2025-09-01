/*************************************************************************************************/
/*!
 *  \file   svc_ch.h
 *        
 *  \brief  Characteristic constants.
 *
 *          $Date: 2015-08-25 13:59:48 -0500 (Tue, 25 Aug 2015) $
 *          $Revision: 18761 $
 *  
 *  Copyright (c) 2011 Wicentric, Inc., all rights reserved.
 *  Wicentric confidential and proprietary.
 *
 *  IMPORTANT.  Your use of this file is governed by a Software License Agreement
 *  ("Agreement") that must be accepted in order to download or otherwise receive a
 *  copy of this file.  You may not use or copy this file for any purpose other than
 *  as described in the Agreement.  If you do not agree to all of the terms of the
 *  Agreement do not use this file and delete all copies in your possession or control;
 *  if you do not have a copy of the Agreement, you must contact Wicentric, Inc. prior
 *  to any use, copying or further distribution of this software.
 */
/*************************************************************************************************/

#ifndef SVC_CH_H
#define SVC_CH_H

#ifdef __cplusplus
extern "C" {
#endif

/**************************************************************************************************
  Macros
**************************************************************************************************/

/* Appearance */
#define CH_APPEAR_UNKNOWN                     0       /* Unknown */
#define CH_APPEAR_PHONE                       64      /* Generic Phone */
#define CH_APPEAR_COMPUTER                    128     /* Generic Computer */
#define CH_APPEAR_WATCH                       192     /* Generic Watch */
#define CH_APPEAR_WATCH_SPORTS                193     /* Watch: Sports Watch */
#define CH_APPEAR_CLOCK                       256     /* Generic Clock */
#define CH_APPEAR_DISPLAY                     320     /* Generic Display */
#define CH_APPEAR_REMOTE                      384     /* Generic Remote Control */
#define CH_APPEAR_GLASSES                     448     /* Generic Eye-glasses */
#define CH_APPEAR_TAG                         512     /* Generic Tag */
#define CH_APPEAR_KEYRING                     576     /* Generic Keyring */
#define CH_APPEAR_PLAYER                      640     /* Generic Media Player */
#define CH_APPEAR_BARCODE                     704     /* Generic Barcode Scanner */
#define CH_APPEAR_THERM                       768     /* Generic Thermometer */
#define CH_APPEAR_THERM_EAR                   769     /* Thermometer: Ear */
#define CH_APPEAR_HR                          832     /* Generic Heart rate Sensor */
#define CH_APPEAR_HR_BELT                     833     /* Heart Rate Sensor: Heart Rate Belt  */
#define CH_APPEAR_BP                          896     /* Generic Blood Pressure */
#define CH_APPEAR_BP_ARM                      897     /* Blood Pressure: Arm */
#define CH_APPEAR_BP_WRIST                    898     /* Blood Pressure: Wrist */
#define CH_APPEAR_HID                         960     /* Human Interface Device (HID) */
#define CH_APPEAR_HID_KEYBOARD                961     /* Keyboard */
#define CH_APPEAR_HID_MOUSE                   962     /* Mouse */
#define CH_APPEAR_HID_JOYSTICK                963     /* Joystick */
#define CH_APPEAR_HID_GAMEPAD                 964     /* Gamepad */
#define CH_APPEAR_HID_TABLET                  965     /* Digitizer Tablet */
#define CH_APPEAR_HID_READER                  966     /* Card Reader */
#define CH_APPEAR_HID_PEN                     967     /* Digital Pen */
#define CH_APPEAR_HID_BARCODE                 968     /* Barcode Scanner */
#define CH_APPEAR_GLUCOSE                     1024    /* Generic Glucose Meter */
#define CH_APPEAR_RUN                         1088    /* Generic: Running Walking Sensor */
#define CH_APPEAR_RUN_IN_SHOE                 1089    /* Running Walking Sensor: In-Shoe */
#define CH_APPEAR_RUN_ON_SHOE                 1090    /* Running Walking Sensor: On-Shoe */
#define CH_APPEAR_RUN_ON_HIP                  1091    /* Running Walking Sensor: On-Hip */
#define CH_APPEAR_CYCL                        1152    /* Generic: Cycling */
#define CH_APPEAR_CYCL_COMP                   1153    /* Cycling: Cycling Computer */
#define CH_APPEAR_CYCL_SPEED                  1154    /* Cycling: Speed Sensor */
#define CH_APPEAR_CYCL_CAD                    1155    /* Cycling: Cadence Sensor */
#define CH_APPEAR_CYCL_POWER                  1156    /* Cycling: Power Sensor */
#define CH_APPEAR_CYCL_SPEED_CAD              1157    /* Cycling: Speed and Cadence Sensor */

#define CH_APPEAR_LEN                         2

/* Alert Level */
#define CH_ALERT_LVL_NONE                     0       /* No Alert */
#define CH_ALERT_LVL_MILD                     1       /* Mild Alert */
#define CH_ALERT_LVL_HIGH                     2       /* High Alert */

#define CH_ALERT_LVL_LEN                      1

/* Heart Rate Measurement flags */
#define CH_HRM_FLAGS_VALUE_8BIT               0x00    /* Heart Rate Value Format is set to UINT8 */
#define CH_HRM_FLAGS_VALUE_16BIT              0x01    /* Heart Rate Value Format is set to UINT16 */
#define CH_HRM_FLAGS_SENSOR_NOT_SUP           0x00    /* Sensor Contact feature is not supported */
#define CH_HRM_FLAGS_SENSOR_NOT_DET           0x04    /* Sensor Contact feature is supported, but contact is not detected */
#define CH_HRM_FLAGS_SENSOR_DET               0x06    /* Sensor Contact feature is supported and contact is detected */
#define CH_HRM_FLAGS_ENERGY_EXP               0x08    /* Energy Expended field is present */
#define CH_HRM_FLAGS_RR_INTERVAL              0x10    /* One or more RR-Interval values are present */

/* Heart Rate Measurement field lengths */
#define CH_HRM_LEN_VALUE_8BIT                 1
#define CH_HRM_LEN_VALUE_16BIT                2
#define CH_HRM_LEN_ENERGY_EXP                 2
#define CH_HRM_LEN_RR_INTERVAL                2

/* Body Sensor Location */
#define CH_BSENSOR_LOC_OTHER                  0       /* Other */
#define CH_BSENSOR_LOC_CHEST                  1       /* Chest */
#define CH_BSENSOR_LOC_WRIST                  2       /* Wrist */
#define CH_BSENSOR_LOC_FINGER                 3       /* Finger */
#define CH_BSENSOR_LOC_HAND                   4       /* Hand */
#define CH_BSENSOR_LOC_EARLOBE                5       /* Ear Lobe */
#define CH_BSENSOR_LOC_FOOT                   6       /* Foot */

#define CH_BSENSOR_LOC_LEN                    1

/* Heart Rate Control Point */
#define CH_HRCP_RESET_ENERGY_EXP              1       /* Reset Energy Expended */

/* Alert Notifcation Control Point */
#define CH_ANCP_ENABLE_NEW                    0       /* Enable New Incoming Alert Notification */
#define CH_ANCP_ENABLE_UNREAD                 1       /* Enable Unread Category Status Notification */
#define CH_ANCP_DISABLE_NEW                   2       /* Disable New Incoming Alert Notification */
#define CH_ANCP_DISABLE_UNREAD                3       /* Disable Unread Category Status Notification */
#define CH_ANCP_NOTIFY_NEW                    4       /* Notify New Incoming Alert immediately */
#define CH_ANCP_NOTIFY_UNREAD                 5       /* Notify Unread Category Status immediately */

/* Alert Category ID */
#define CH_ALERT_CAT_ID_SIMPLE                0       /* Simple Alert: General text alert or non-text alert */
#define CH_ALERT_CAT_ID_EMAIL                 1       /* Email: Alert when Email messages arrives */
#define CH_ALERT_CAT_ID_NEWS                  2       /* News: News feeds such as RSS, Atom */
#define CH_ALERT_CAT_ID_CALL                  3       /* Call: Incoming call */
#define CH_ALERT_CAT_ID_MISSED                4       /* Missed call: Missed Call */
#define CH_ALERT_CAT_ID_SMS                   5       /* SMS/MMS: SMS/MMS message arrives */
#define CH_ALERT_CAT_ID_VMAIL                 6       /* Voice mail: Voice mail */
#define CH_ALERT_CAT_ID_SCHED                 7       /* Schedule: Alert occurred on calendar, planner */
#define CH_ALERT_CAT_ID_ALERT                 8       /* High Prioritized Alert: Alert that should be handled as high priority */
#define CH_ALERT_CAT_ID_IM                    9       /* Instant Message: Alert for incoming instant messages */
#define CH_ALERT_CAT_ID_ALL                   0xFF    /* All Categories */

/* Alert Category ID Bit Mask */
#define CH_ALERT_CAT_MASK_SIMPLE              0x0001  /* Simple Alert: General text alert or non-text alert */
#define CH_ALERT_CAT_MASK_EMAIL               0x0002  /* Email: Alert when Email messages arrives */
#define CH_ALERT_CAT_MASK_NEWS                0x0004  /* News: News feeds such as RSS, Atom */
#define CH_ALERT_CAT_MASK_CALL                0x0008  /* Call: Incoming call */
#define CH_ALERT_CAT_MASK_MISSED              0x0010  /* Missed call: Missed Call */
#define CH_ALERT_CAT_MASK_SMS                 0x0020  /* SMS/MMS: SMS/MMS message arrives */
#define CH_ALERT_CAT_MASK_VMAIL               0x0040  /* Voice mail: Voice mail */
#define CH_ALERT_CAT_MASK_SCHED               0x0080  /* Schedule: Alert occurred on calendar, planner */
#define CH_ALERT_CAT_MASK_ALERT               0x0100  /* High Prioritized Alert: Alert that should be handled as high priority */
#define CH_ALERT_CAT_MASK_IM                  0x0200  /* Instant Message: Alert for incoming instant messages */

/* Ringer Control Point */
#define CH_RCP_SILENT                         1       /* Silent Mode */
#define CH_RCP_MUTE_ONCE                      2       /* Mute Once */
#define CH_RCP_CANCEL_SILENT                  3       /* Cancel Silent Mode */

/* Ringer Setting */
#define CH_RINGER_SET_SILENT                  0       /* Ringer Silent */
#define CH_RINGER_SET_NORMAL                  1       /* Ringer Normal */

/* Alert Status */
#define CH_ALERT_STATUS_RINGER                0x01    /* Ringer State active */
#define CH_ALERT_STATUS_VIBRATE               0x02    /* Vibrate State active */
#define CH_ALERT_STATUS_DISPLAY               0x04    /* Display Alert Status State active */

/* System ID */
#define CH_SYSTEM_ID_LEN                      8

/* Battery Level */
#define CH_BATT_LEVEL_LEN                     1

/* Blood Pressure Measurement flags */
#define CH_BPM_FLAG_UNITS_MMHG                0x00    /* Blood pressure in units of mmHg */
#define CH_BPM_FLAG_UNITS_KPA                 0x01    /* Blood pressure in units of kPa */
#define CH_BPM_FLAG_TIMESTAMP                 0x02    /* Time Stamp Flag */
#define CH_BPM_FLAG_PULSE_RATE                0x04    /* Pulse Rate Flag */
#define CH_BPM_FLAG_USER_ID                   0x08    /* User ID Flag */
#define CH_BPM_FLAG_MEAS_STATUS               0x10    /* Measurement Status Flag */

/* Blood Pressure Measurement measurement status bitmasks */
#define CH_BPM_MS_BIT_MOVEMENT                0x0001  /* Body Movement Detection */
#define CH_BPM_MS_BIT_CUFF_FIT                0x0002  /* Cuff Fit Detection */
#define CH_BPM_MS_BIT_IRR_PULSE               0x0004  /* Irregular Pulse Detection */
#define CH_BPM_MS_BIT_PULSE_RANGE             0x0018  /* Pulse Rate Range Detection */
#define CH_BPM_MS_BIT_MEAS_POS                0x0020  /* Measurement Position Detection */

/* Blood Pressure Measurement measurement status flags */
#define CH_BPM_MS_FLAG_MOVEMENT_NONE          0x0000  /* No body movement */
#define CH_BPM_MS_FLAG_MOVEMENT               0x0001  /* Body movement */
#define CH_BPM_MS_FLAG_CUFF_FIT_OK            0x0000  /* Cuff fit ok */
#define CH_BPM_MS_FLAG_CUFF_FIT_LOOSE         0x0002  /* Cuff fit loose */
#define CH_BPM_MS_FLAG_IRR_PULSE_NONE         0x0000  /* No irregular pulse detected */
#define CH_BPM_MS_FLAG_IRR_PULSE              0x0004  /* Irregular pulse detected */
#define CH_BPM_MS_FLAG_PULSE_RANGE_OK         0x0000  /* Pulse rate is within the range */
#define CH_BPM_MS_FLAG_PULSE_RANGE_HIGH       0x0008  /* Pulse rate exceeds upper limit */
#define CH_BPM_MS_FLAG_PULSE_RANGE_LOW        0x0010  /* Pulse rate is less than lower limit */
#define CH_BPM_MS_FLAG_MEAS_POS               0x0000  /* Proper measurement position */
#define CH_BPM_MS_FLAG_MEAS_POS_ERR           0x0020  /* Improper measurement position */

/* Blood Pressure Measurement field lengths */
#define CH_BPM_FLAGS_LEN                      1
#define CH_BPM_MEAS_LEN                       6
#define CH_BPM_TIMESTAMP_LEN                  7
#define CH_BPM_PULSE_RATE_LEN                 2
#define CH_BPM_USER_ID_LEN                    1
#define CH_BPM_MEAS_STATUS_LEN                2

/* Blood Pressure Feature flags */
#define CH_BPF_FLAG_MOVEMENT                  0x0001  /* Body Movement Detection Support bit */
#define CH_BPF_FLAG_CUFF_FIT                  0x0002  /* Cuff Fit Detection Support bit */
#define CH_BPF_FLAG_IRR_PULSE                 0x0004  /* Irregular Pulse Detection Support bit */
#define CH_BPF_FLAG_PULSE_RANGE               0x0008  /* Pulse Rate Range Detection Support bit */
#define CH_BPF_FLAG_MEAS_POS                  0x0010  /* Measurement Position Detection Support bit */
#define CH_BPF_FLAG_MULTI_BOND                0x0020  /* Multiple bond support bit */

/* SFLOAT special values */
#define CH_SFLOAT_NAN                         0x07FF  /* Not a number */
#define CH_SFLOAT_NRES                        0x07FF  /* Not at this resolution */
#define CH_SFLOAT_POS_INF                     0x07FE  /* + Infinity */
#define CH_SFLOAT_NEG_INF                     0x0802  /* - Infinity */
#define CH_SFLOAT_RSVD                        0x0801  /* Not at this resolution */

/* Glucose feature */
#define CH_GLF_LOW_BATT                       0x0001  /* Low Battery Detection During Measurement Supported */
#define CH_GLF_MALFUNC                        0x0002  /* Sensor Malfunction Detection Supported */
#define CH_GLF_SAMPLE_SIZE                    0x0004  /* Sensor Sample Size Supported */
#define CH_GLF_INSERT_ERR                     0x0008  /* Sensor Strip Insertion Error Detection Supported */
#define CH_GLF_TYPE_ERR                       0x0010  /* Sensor Strip Type Error Detection Supported */
#define CH_GLF_RES_HIGH_LOW                   0x0020  /* Sensor Result High-Low Detection Supported */
#define CH_GLF_TEMP_HIGH_LOW                  0x0040  /* Sensor Temperature High-Low Detection Supported */
#define CH_GLF_READ_INT                       0x0080  /* Sensor Read Interrupt Detection Supported */
#define CH_GLF_GENERAL_FAULT                  0x0100  /* General Device Fault Supported */
#define CH_GLF_TIME_FAULT                     0x0200  /* Time Fault Supported */
#define CH_GLF_MULTI_BOND                     0x0400  /* Multiple Bond Supported */

/* Glucose measurement flags */
#define CH_GLM_FLAG_TIME_OFFSET               0x01    /* Time Offset Present */
#define CH_GLM_FLAG_CONC_TYPE_LOC             0x02    /* Glucose Concentration, Type, and Sample Location Present */
#define CH_GLM_FLAG_UNITS_KG_L                0x00    /* Glucose Concentration Units kg/L */
#define CH_GLM_FLAG_UNITS_MOL_L               0x04    /* Glucose Concentration Units mol/L */
#define CH_GLM_FLAG_SENSOR_STATUS             0x08    /* Sensor Status Annunciation Present */
#define CH_GLM_FLAG_CONTEXT_INFO              0x10    /* Context Information Follows */

/* Glucose measurement type */
#define CH_GLM_TYPE_CAP_BLOOD                 1       /* Capillary whole blood */
#define CH_GLM_TYPE_CAP_PLASMA                2       /* Capillary plasma */
#define CH_GLM_TYPE_VEN_BLOOD                 3       /* Venous whole blood */
#define CH_GLM_TYPE_VEN_PLASMA                4       /* Venous plasma */
#define CH_GLM_TYPE_ART_BLOOD                 5       /* Arterial whole blood */
#define CH_GLM_TYPE_ART_PLASMA                6       /* Arterial plasma */
#define CH_GLM_TYPE_UNDET_BLOOD               7       /* Undetermined whole blood */
#define CH_GLM_TYPE_UNDET_PLASMA              8       /* Undetermined plasma */
#define CH_GLM_TYPE_FLUID                     9       /* Interstitial fluid (ISF) */
#define CH_GLM_TYPE_CONTROL                   10      /* Control solution */

/* Glucose measurement location */
#define CH_GLM_LOC_FINGER                     1     /* Finger */
#define CH_GLM_LOC_AST                        2     /* Alternate Site Test (AST) */
#define CH_GLM_LOC_EAR                        3     /* Earlobe */
#define CH_GLM_LOC_CONTROL                    4     /* Control solution */
#define CH_GLM_LOC_NOT_AVAIL                  15    /* Sample Location value not available */

/* Glucose sensor status annunciation */
#define CH_GLM_STATUS_BATT_LOW                0x0001  /* Device battery low at time of measurement */
#define CH_GLM_STATUS_SENSOR_FAULT            0x0002  /* Sensor malfunction or faulting at time of measurement */
#define CH_GLM_STATUS_SAMPLE_SIZE             0x0004  /* Sample size for blood or control solution insufficient at time of measurement */
#define CH_GLM_STATUS_STRIP_INSERT            0x0008  /* Strip insertion error */
#define CH_GLM_STATUS_STRIP_TYPE              0x0010  /* Strip type incorrect for device */
#define CH_GLM_STATUS_RESULT_HIGH             0x0020  /* Sensor result higher than the device can process */
#define CH_GLM_STATUS_RESULT_LOW              0x0040  /* Sensor result lower than the device can process */
#define CH_GLM_STATUS_TEMP_HIGH               0x0080  /* Sensor temperature too high for valid test/result at time of measurement */
#define CH_GLM_STATUS_TEMP_LOW                0x0100  /* Sensor temperature too low for valid test/result at time of measurement */
#define CH_GLM_STATUS_STRIP_PULL              0x0200  /* Sensor read interrupted because strip was pulled too soon at time of measurement */
#define CH_GLM_STATUS_GENERAL_FAULT           0x0400  /* General device fault has occurred in the sensor */
#define CH_GLM_STATUS_TIME_FAULT              0x0800  /* Time fault has occurred in the sensor and time may be inaccurate */

/* Glucose measurement field lengths */
#define CH_GLM_FLAGS_LEN                      1
#define CH_GLM_SEQNUM_LEN                     2
#define CH_GLM_TIMESTAMP_LEN                  7
#define CH_GLM_TIME_OFFSET_LEN                2
#define CH_GLM_CONC_TYPE_LOC_LEN              3
#define CH_GLM_SENSOR_STATUS_LEN              2

/* Glucose measurement context flags */
#define CH_GLMC_FLAG_CARB                     0x01    /* Carbohydrate id and carbohydrate present */
#define CH_GLMC_FLAG_MEAL                     0x02    /* Meal present */
#define CH_GLMC_FLAG_TESTER                   0x04    /* Tester-health present */
#define CH_GLMC_FLAG_EXERCISE                 0x08    /* Exercise duration and exercise intensity present */
#define CH_GLMC_FLAG_MED                      0x10    /* Medication ID and medication present */
#define CH_GLMC_FLAG_MED_KG                   0x00    /* Medication value units, kilograms */
#define CH_GLMC_FLAG_MED_L                    0x20    /* Medication value units, liters */
#define CH_GLMC_FLAG_HBA1C                    0x40    /* Hba1c present */
#define CH_GLMC_FLAG_EXT                      0x80    /* Extended flags present */

/* Glucose measurement context field lenths */
#define CH_GLMC_FLAGS_LEN                     1
#define CH_GLMC_SEQNUM_LEN                    2
#define CH_GLMC_CARB_LEN                      3
#define CH_GLMC_MEAL_LEN                      1
#define CH_GLMC_TESTER_LEN                    1
#define CH_GLMC_EXERCISE_LEN                  3
#define CH_GLMC_MED_LEN                       3
#define CH_GLMC_HBA1C_LEN                     2
#define CH_GLMC_EXT_LEN                       1


/* Glucose measurement context carbohydrate ID */
#define CH_GLMC_CARB_BREAKFAST                1       /* Breakfast */
#define CH_GLMC_CARB_LUNCH                    2       /* Lunch */
#define CH_GLMC_CARB_DINNER                   3       /* Dinner */
#define CH_GLMC_CARB_SNACK                    4       /* Snack */
#define CH_GLMC_CARB_DRINK                    5       /* Drink */
#define CH_GLMC_CARB_SUPPER                   6       /* Supper */
#define CH_GLMC_CARB_BRUNCH                   7       /* Brunch */

/* Glucose measurement context meal */
#define CH_GLMC_MEAL_PREPRANDIAL              1       /* Preprandial (before meal) */
#define CH_GLMC_MEAL_POSTPRANDIAL             2       /* Postprandial (after meal) */
#define CH_GLMC_MEAL_FASTING                  3       /* Fasting */
#define CH_GLMC_MEAL_CASUAL                   4       /* Casual (snacks, drinks, etc.) */
#define CH_GLMC_MEAL_BEDTIME                  5       /* Bedtime */

/* Glucose measurement context tester */
#define CH_GLMC_TESTER_SELF                   1       /* Self */
#define CH_GLMC_TESTER_PRO                    2       /* Health care professional */
#define CH_GLMC_TESTER_LAB                    3       /* Lab test */
#define CH_GLMC_TESTER_NOT_AVAIL              15      /* Tester value not available */

/* Glucose measurement context health */
#define CH_GLMC_HEALTH_MINOR                  1       /* Minor health issues */
#define CH_GLMC_HEALTH_MAJOR                  2       /* Major health issues */
#define CH_GLMC_HEALTH_MENSES                 3       /* During menses */
#define CH_GLMC_HEALTH_STRESS                 4       /* Under stress */
#define CH_GLMC_HEALTH_NONE                   5       /* No health issues */
#define CH_GLMC_HEALTH_NOT_AVAIL              15      /* Health value not available */

/* Glucose measurement context medication ID */
#define CH_GLMC_MED_RAPID                     1       /* Rapid acting insulin */
#define CH_GLMC_MED_SHORT                     2       /* Short acting insulin */
#define CH_GLMC_MED_INTERMED                  3       /* Intermediate acting insulin */
#define CH_GLMC_MED_LONG                      4       /* Long acting insulin */
#define CH_GLMC_MED_PREMIX                    5       /* Pre-mixed insulin */

/* Record access control point opcode */
#define CH_RACP_OPCODE_REPORT                 1       /* Report stored records */
#define CH_RACP_OPCODE_DELETE                 2       /* Delete stored records */
#define CH_RACP_OPCODE_ABORT                  3       /* Abort operation */
#define CH_RACP_OPCODE_REPORT_NUM             4       /* Report number of stored records */
#define CH_RACP_OPCODE_NUM_RSP                5       /* Number of stored records response */
#define CH_RACP_OPCODE_RSP                    6       /* Response code */

/* Record access control point operator */
#define CH_RACP_OPERATOR_NULL                 0       /* Null operator */
#define CH_RACP_OPERATOR_ALL                  1       /* All records */
#define CH_RACP_OPERATOR_LTEQ                 2       /* Less than or equal to */
#define CH_RACP_OPERATOR_GTEQ                 3       /* Greater than or equal to */
#define CH_RACP_OPERATOR_RANGE                4       /* Within range of (inclusive) */
#define CH_RACP_OPERATOR_FIRST                5       /* First record(i.e. oldest record) */
#define CH_RACP_OPERATOR_LAST                 6       /* Last record (i.e. most recent record) */

/* Record access control point response code values */
#define CH_RACP_RSP_SUCCESS                   1       /* Success */
#define CH_RACP_RSP_OPCODE_NOT_SUP            2       /* Op code not supported */
#define CH_RACP_RSP_INV_OPERATOR              3       /* Invalid operator */
#define CH_RACP_RSP_OPERATOR_NOT_SUP          4       /* Operator not supported */
#define CH_RACP_RSP_INV_OPERAND               5       /* Invalid operand */
#define CH_RACP_RSP_NO_RECORDS                6       /* No records found */
#define CH_RACP_RSP_ABORT_FAILED              7       /* Abort unsuccessful */
#define CH_RACP_RSP_PROC_NOT_COMP             8       /* Procedure not completed */
#define CH_RACP_RSP_OPERAND_NOT_SUP           9       /* Operand not supported */

/* Glucose service operand filter types and field lengths */
#define CH_RACP_GLS_FILTER_SEQ                1       /* Sequence number */
#define CH_RACP_GLS_FILTER_TIME               2       /* User facing time */
#define CH_RACP_GLS_FILTER_SEQ_LEN            2       /* Sequence number filter length */
#define CH_RACP_GLS_FILTER_TIME_LEN           7       /* User facing time filter length */

/* Service changed */
#define CH_SC_LEN                             4       /* Length of service changed */

/* Temperature Measurement flags */
#define CH_TM_FLAG_UNITS_C                    0x00    /* Temperature in units of C */
#define CH_TM_FLAG_UNITS_F                    0x01    /* Temperature in units of F */
#define CH_TM_FLAG_TIMESTAMP                  0x02    /* Time Stamp Flag */
#define CH_TM_FLAG_TEMP_TYPE                  0x04    /* Temperature Type Flag */

/* Temperature Measurement field lengths */
#define CH_TM_FLAGS_LEN                       1
#define CH_TM_MEAS_LEN                        4
#define CH_TM_TIMESTAMP_LEN                   7
#define CH_TM_TEMP_TYPE_LEN                   1

/* Temperature type */
#define CH_TT_ARMPIT                          1       /* Armpit */
#define CH_TT_BODY                            2       /* Body (general) */
#define CH_TT_EAR                             3       /* Ear (usually ear lobe) */
#define CH_TT_FINGER                          4       /* Finger */
#define CH_TT_GI                              5       /* Gastro-intestinal Tract */
#define CH_TT_MOUTH                           6       /* Mouth */
#define CH_TT_RECTUM                          7       /* Rectum */
#define CH_TT_TOE                             8       /* Toe */
#define CH_TT_TYMPANUM                        9       /* Tympanum (ear drum) */


/* remove values below when adopted */

/* Weight Scale Feature flags */
#define CH_WSF_FLAG_TIMESTAMP                 0x0001  /* Time Stamp Supported bit */
#define CH_WSF_FLAG_MULTIUSER                 0x0002  /* Multiple Users Supported bit */
#define CH_WSF_FLAG_BMI                       0x0004  /* BMI Supported bit */

/* Weight Scale Measurement flags */
#define CH_WSM_FLAG_UNITS_KG                  0x00    /* Weight in units of kilograms */
#define CH_WSM_FLAG_UNITS_LBS                 0x01    /* Weight in units of pounds */
#define CH_WSM_FLAG_TIMESTAMP                 0x02    /* Time stamp present */
#define CH_WSM_FLAG_USER_ID                   0x04    /* User ID present */
#define CH_WSM_FLAG_BMI_HEIGHT                0x08    /* BMI and height present */

/* Weight Scale Measurement field lengths */
#define CH_WSM_FLAGS_LEN                      1
#define CH_WSM_MEAS_LEN                       2
#define CH_WSM_TIMESTAMP_LEN                  7
#define CH_WSM_USER_ID_LEN                    1
#define CH_WSM_BMI_HEIGHT_LEN                 4

#ifdef __cplusplus
};
#endif

#endif /* SVC_CH_H */

