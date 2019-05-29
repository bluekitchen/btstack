/**
 ******************************************************************************
 * @file    hci_tl.c
 * @author  MCD Application Team
 * @brief   Function for managing HCI interface.
 ******************************************************************************
  * @attention
  *
  * <h2><center>&copy; Copyright (c) 2019 STMicroelectronics. 
  * All rights reserved.</center></h2>
  *
  * This software component is licensed by ST under BSD 3-Clause license,
  * the "License"; You may not use this file except in compliance with the 
  * License. You may obtain a copy of the License at:
  *                        opensource.org/licenses/BSD-3-Clause
  *
  ******************************************************************************
 */


/* Includes ------------------------------------------------------------------*/
#include "ble_common.h"
#include "ble_const.h"

#include "stm_list.h"
#include "tl.h"
#include "hci_tl.h"


/* Private typedef -----------------------------------------------------------*/
/* Private defines -----------------------------------------------------------*/

/**
 * The default HCI layer timeout is set to 33s
 */
#define HCI_TL_DEFAULT_TIMEOUT (33000)

/* Private macros ------------------------------------------------------------*/
/* Public variables ---------------------------------------------------------*/
/* Private variables ---------------------------------------------------------*/
/**
 * START of Section BLE_DRIVER_CONTEXT
 */
PLACE_IN_SECTION("BLE_DRIVER_CONTEXT") static volatile uint8_t hci_timer_id;
PLACE_IN_SECTION("BLE_DRIVER_CONTEXT") static tListNode HciAsynchEventQueue;
PLACE_IN_SECTION("BLE_DRIVER_CONTEXT") static TL_CmdPacket_t *pCmdBuffer;
PLACE_IN_SECTION("BLE_DRIVER_CONTEXT") HCI_TL_UserEventFlowStatus_t UserEventFlow;
/**
 * END of Section BLE_DRIVER_CONTEXT
 */

static tHciContext hciContext;
static tListNode HciCmdEventQueue;
static void (* StatusNotCallBackFunction) (HCI_TL_CmdStatus_t status);

/* Private function prototypes -----------------------------------------------*/
static void NotifyCmdStatus(HCI_TL_CmdStatus_t hcicmdstatus);
static void SendCmd(uint16_t opcode, uint8_t plen, void *param);
static void TlEvtReceived(TL_EvtPacket_t *hcievt);
static void TlInit( TL_CmdPacket_t * p_cmdbuffer );

/* Interface ------- ---------------------------------------------------------*/
void hci_init(void(* UserEvtRx)(void* pData), void* pConf)
{
  StatusNotCallBackFunction = ((HCI_TL_HciInitConf_t *)pConf)->StatusNotCallBack;
  hciContext.UserEvtRx = UserEvtRx;

  hci_register_io_bus (&hciContext.io);

  TlInit((TL_CmdPacket_t *)(((HCI_TL_HciInitConf_t *)pConf)->p_cmdbuffer));

  return;
}

void hci_user_evt_proc(void)
{
  TL_EvtPacket_t *phcievtbuffer;
  tHCI_UserEvtRxParam UserEvtRxParam;

  /**
   * Up to release version v1.2.0, a while loop was implemented to read out events from the queue as long as
   * it is not empty. However, in a bare metal implementation, this leads to calling in a "blocking" mode
   * hci_user_evt_proc() as long as events are received without giving the opportunity to run other tasks
   * in the background.
   * From now, the events are reported one by one. When it is checked there is still an event pending in the queue,
   * a request to the user is made to call again hci_user_evt_proc().
   * This gives the opportunity to the application to run other background tasks between each event.
   */

  /**
   * It is more secure to use LST_remove_head()/LST_insert_head() compare to LST_get_next_node()/LST_remove_node()
   * in case the user overwrite the header where the next/prev pointers are located
   */

  if((LST_is_empty(&HciAsynchEventQueue) == FALSE) && (UserEventFlow != HCI_TL_UserEventFlow_Disable))
  {
    LST_remove_head ( &HciAsynchEventQueue, (tListNode **)&phcievtbuffer );

    UserEventFlow = HCI_TL_UserEventFlow_Enable;

    if (hciContext.UserEvtRx != NULL)
    {
      UserEvtRxParam.pckt = phcievtbuffer;
      hciContext.UserEvtRx((void *)&UserEvtRxParam);
      UserEventFlow = UserEvtRxParam.status;
    }

    if(UserEventFlow != HCI_TL_UserEventFlow_Disable)
    {
      TL_MM_EvtDone( phcievtbuffer );
    }
    else
    {
      /**
       * put back the event in the queue
       */
      LST_insert_head ( &HciAsynchEventQueue, (tListNode *)phcievtbuffer );
    }
  }

  if((LST_is_empty(&HciAsynchEventQueue) == FALSE) && (UserEventFlow != HCI_TL_UserEventFlow_Disable))
  {
    hci_notify_asynch_evt((void*) &HciAsynchEventQueue);
  }


  return;
}

void hci_resume_flow( void )
{
  UserEventFlow = HCI_TL_UserEventFlow_Enable;

  /**
   * It is better to go through the background process as it is not sure from which context this API may
   * be called
   */
  hci_notify_asynch_evt((void*) &HciAsynchEventQueue);

  return;
}

int hci_send_req(struct hci_request *p_cmd, uint8_t async)
{
  uint16_t opcode;
  TL_CcEvt_t  *pcommand_complete_event;
  TL_CsEvt_t    *pcommand_status_event;
  TL_EvtPacket_t *pevtpacket;
  uint8_t hci_cmd_complete_return_parameters_length;
  HCI_TL_CmdStatus_t local_cmd_status;

  NotifyCmdStatus(HCI_TL_CmdBusy);
  local_cmd_status = HCI_TL_CmdBusy;
  opcode = ((p_cmd->ocf) & 0x03ff) | ((p_cmd->ogf) << 10);
  SendCmd(opcode, p_cmd->clen, p_cmd->cparam);

  while(local_cmd_status == HCI_TL_CmdBusy)
  {
    hci_cmd_resp_wait(HCI_TL_DEFAULT_TIMEOUT);

    /**
     * Process Cmd Event
     */
    while(LST_is_empty(&HciCmdEventQueue) == FALSE)
    {
      LST_remove_head (&HciCmdEventQueue, (tListNode **)&pevtpacket);

      if(pevtpacket->evtserial.evt.evtcode == TL_BLEEVT_CS_OPCODE)
      {
        pcommand_status_event = (TL_CsEvt_t*)pevtpacket->evtserial.evt.payload;
        if(pcommand_status_event->cmdcode == opcode)
        {
          *(uint8_t *)(p_cmd->rparam) = pcommand_status_event->status;
        }

        if(pcommand_status_event->numcmd != 0)
        {
          local_cmd_status = HCI_TL_CmdAvailable;
        }
      }
      else
      {
        pcommand_complete_event = (TL_CcEvt_t*)pevtpacket->evtserial.evt.payload;

        if(pcommand_complete_event->cmdcode == opcode)
        {
          hci_cmd_complete_return_parameters_length = pevtpacket->evtserial.evt.plen - TL_EVT_HDR_SIZE;
          p_cmd->rlen = MIN(hci_cmd_complete_return_parameters_length, p_cmd->rlen);
          memcpy(p_cmd->rparam, pcommand_complete_event->payload, p_cmd->rlen);
        }

        if(pcommand_complete_event->numcmd != 0)
        {
          local_cmd_status = HCI_TL_CmdAvailable;
        }
      }
    }
  }

  NotifyCmdStatus(HCI_TL_CmdAvailable);

  return 0;
}

/* Private functions ---------------------------------------------------------*/
static void TlInit( TL_CmdPacket_t * p_cmdbuffer )
{
  TL_BLE_InitConf_t Conf;

  /**
   * Always initialize the command event queue
   */
  LST_init_head (&HciCmdEventQueue);

  pCmdBuffer = p_cmdbuffer;

  LST_init_head (&HciAsynchEventQueue);

  UserEventFlow = HCI_TL_UserEventFlow_Enable;

  /* Initialize low level driver */
  if (hciContext.io.Init)
  {

    Conf.p_cmdbuffer = (uint8_t *)p_cmdbuffer;
    Conf.IoBusEvtCallBack = TlEvtReceived;
    hciContext.io.Init(&Conf);
  }

  return;
}

static void SendCmd(uint16_t opcode, uint8_t plen, void *param)
{
  pCmdBuffer->cmdserial.cmd.cmdcode = opcode;
  pCmdBuffer->cmdserial.cmd.plen = plen;
  memcpy( pCmdBuffer->cmdserial.cmd.payload, param, plen );

  hciContext.io.Send(0,0);

  return;
}

static void NotifyCmdStatus(HCI_TL_CmdStatus_t hcicmdstatus)
{
  if(hcicmdstatus == HCI_TL_CmdBusy)
  {
    if(StatusNotCallBackFunction != 0)
    {
      StatusNotCallBackFunction(HCI_TL_CmdBusy);
    }
  }
  else
  {
    if(StatusNotCallBackFunction != 0)
    {
      StatusNotCallBackFunction(HCI_TL_CmdAvailable);
    }
  }

  return;
}

static void TlEvtReceived(TL_EvtPacket_t *hcievt)
{
  if ( ((hcievt->evtserial.evt.evtcode) == TL_BLEEVT_CS_OPCODE) || ((hcievt->evtserial.evt.evtcode) == TL_BLEEVT_CC_OPCODE ) )
  {
    LST_insert_tail(&HciCmdEventQueue, (tListNode *)hcievt);
    hci_cmd_resp_release(0); /**< Notify the application a full Cmd Event has been received */
  }
  else
  {
    LST_insert_tail(&HciAsynchEventQueue, (tListNode *)hcievt);
    hci_notify_asynch_evt((void*) &HciAsynchEventQueue); /**< Notify the application a full HCI event has been received */
  }

  return;
}
