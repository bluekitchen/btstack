/**
  ******************************************************************************
  * @file    pdm_filter.h
  * @author  MCD Application Team
  * @version V2.1.0
  * @date    31-March-2015
  * @brief   Header file for PDM audio software decoding Library.   
  *          This Library is used to decode and reconstruct the audio signal
  *          produced by ST MEMS microphone (MP45Dxxx, MP34Dxxx).
  *          For more details about this Library, please refer to document
  *          "PDM audio software decoding on STM32 microcontrollers (AN3998)".  
 ******************************************************************************
  * @attention
  *
  * <h2><center>&copy; COPYRIGHT 2015 STMicroelectronics</center></h2>
  *
  * Licensed under MCD-ST Image SW License Agreement V2, (the "License");
  * You may not use this file except in compliance with the License.
  * You may obtain a copy of the License at:
  *
  *        http://www.st.com/software_license_agreement_image_v2
  *
  * Unless required by applicable law or agreed to in writing, software 
  * distributed under the License is distributed on an "AS IS" BASIS, 
  * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
  * See the License for the specific language governing permissions and
  * limitations under the License.
  *
  ******************************************************************************
  */

/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef __PDM_FILTER_H
#define __PDM_FILTER_H

#ifdef __cplusplus
 extern "C" {
#endif

/* Includes ------------------------------------------------------------------*/
#include <stdint.h>


/* Exported types ------------------------------------------------------------*/
typedef struct {
  uint16_t Fs;
  float LP_HZ;
  float HP_HZ;
  uint16_t In_MicChannels;
  uint16_t Out_MicChannels;
  char InternalFilter[34];
} PDMFilter_InitStruct;

/* Exported constants --------------------------------------------------------*/
/* Exported macros -----------------------------------------------------------*/
#define HTONS(A)  ((((uint16_t)(A) & 0xff00) >> 8) | \
                   (((uint16_t)(A) & 0x00ff) << 8))

/* Exported functions ------------------------------------------------------- */
void PDM_Filter_Init(PDMFilter_InitStruct * Filter);

int32_t PDM_Filter_64_MSB(uint8_t* data, uint16_t* dataOut, uint16_t MicGain, PDMFilter_InitStruct * Filter);
int32_t PDM_Filter_80_MSB(uint8_t* data, uint16_t* dataOut, uint16_t MicGain, PDMFilter_InitStruct * Filter);
int32_t PDM_Filter_128_MSB(uint8_t* data, uint16_t* dataOut, uint16_t MicGain, PDMFilter_InitStruct * Filter);
int32_t PDM_Filter_64_LSB(uint8_t* data, uint16_t* dataOut, uint16_t MicGain, PDMFilter_InitStruct * Filter);
int32_t PDM_Filter_80_LSB(uint8_t* data, uint16_t* dataOut, uint16_t MicGain, PDMFilter_InitStruct * Filter);
int32_t PDM_Filter_128_LSB(uint8_t* data, uint16_t* dataOut, uint16_t MicGain, PDMFilter_InitStruct * Filter);

#ifdef __cplusplus
}
#endif

#endif /* __PDM_FILTER_H */

/************************ (C) COPYRIGHT STMicroelectronics *****END OF FILE****/

