/**
 ******************************************************************************
  * File Name          : ble_conf.h
  * Description        : Configuration file for BLE 
  *                      middleWare.
  ******************************************************************************
  */

/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef __BLE_CONF_H
#define __BLE_CONF_H

#include "app_conf.h"

/******************************************************************************
 *
 * BLE SERVICES CONFIGURATION
 * blesvc
 *
 ******************************************************************************/

 /**
 * This setting shall be set to '1' if the device needs to support the Peripheral Role
 * In the MS configuration, both BLE_CFG_PERIPHERAL and BLE_CFG_CENTRAL shall be set to '1'
 */
#define BLE_CFG_PERIPHERAL                                                     1

/**
 * This setting shall be set to '1' if the device needs to support the Central Role
 * In the MS configuration, both BLE_CFG_PERIPHERAL and BLE_CFG_CENTRAL shall be set to '1'
 */
#define BLE_CFG_CENTRAL                                                        0
#define BLE_CFG_DATA_ROLE_MODE                                                    4
/**
 * There is one handler per service enabled
 * Note: There is no handler for the Device Information Service
 *
 * This shall take into account all registered handlers
 * (from either the provided services or the custom services)
 */
#define BLE_CFG_SVC_MAX_NBR_CB                                                 2

#define BLE_CFG_CLT_MAX_NBR_CB                                                 0

/******************************************************************************
 * Device Information Service (DIS)
 ******************************************************************************/
/**< Options: Supported(1) or Not Supported(0) */
#define BLE_CFG_DIS_MANUFACTURER_NAME_STRING                                   
#define BLE_CFG_DIS_MODEL_NUMBER_STRING                                        
#define BLE_CFG_DIS_SERIAL_NUMBER_STRING                                       
#define BLE_CFG_DIS_HARDWARE_REVISION_STRING                                   
#define BLE_CFG_DIS_FIRMWARE_REVISION_STRING                                   
#define BLE_CFG_DIS_SOFTWARE_REVISION_STRING                                   
#define BLE_CFG_DIS_SYSTEM_ID                                                  
#define BLE_CFG_DIS_IEEE_CERTIFICATION                                         
#define BLE_CFG_DIS_PNP_ID                                                     

/**
 * device information service characteristic lengths
 */
#define BLE_CFG_DIS_SYSTEM_ID_LEN_MAX                                        (8)
#define BLE_CFG_DIS_MODEL_NUMBER_STRING_LEN_MAX                              (32)
#define BLE_CFG_DIS_SERIAL_NUMBER_STRING_LEN_MAX                             (32)
#define BLE_CFG_DIS_FIRMWARE_REVISION_STRING_LEN_MAX                         (32)
#define BLE_CFG_DIS_HARDWARE_REVISION_STRING_LEN_MAX                         (32)
#define BLE_CFG_DIS_SOFTWARE_REVISION_STRING_LEN_MAX                         (32)
#define BLE_CFG_DIS_MANUFACTURER_NAME_STRING_LEN_MAX                         (32)
#define BLE_CFG_DIS_IEEE_CERTIFICATION_LEN_MAX                               (32)
#define BLE_CFG_DIS_PNP_ID_LEN_MAX                                           (7)

/**
 * Generic Access Appearance
 */
#define BLE_CFG_UNKNOWN_APPEARANCE                  (0)
#define BLE_CFG_HR_SENSOR_APPEARANCE                (832)
#define BLE_CFG_GAP_APPEARANCE                      (BLE_CFG_UNKNOWN_APPEARANCE)

#endif /*__BLE_CONF_H */
/************************ (C) COPYRIGHT STMicroelectronics *****END OF FILE****/
