/**
* \file  p2p_demo.c
*
* \brief Demo Application for MiWi P2P Implementation
*
* Copyright (c) 2018 - 2019 Microchip Technology Inc. and its subsidiaries.
*
* \asf_license_start
*
* \page License
*
* Subject to your compliance with these terms, you may use Microchip
* software and any derivatives exclusively with Microchip products.
* It is your responsibility to comply with third party license terms applicable
* to your use of third party software (including open source software) that
* may accompany Microchip software.
*
* THIS SOFTWARE IS SUPPLIED BY MICROCHIP "AS IS".  NO WARRANTIES,
* WHETHER EXPRESS, IMPLIED OR STATUTORY, APPLY TO THIS SOFTWARE,
* INCLUDING ANY IMPLIED WARRANTIES OF NON-INFRINGEMENT, MERCHANTABILITY,
* AND FITNESS FOR A PARTICULAR PURPOSE. IN NO EVENT WILL MICROCHIP BE
* LIABLE FOR ANY INDIRECT, SPECIAL, PUNITIVE, INCIDENTAL OR CONSEQUENTIAL
* LOSS, DAMAGE, COST OR EXPENSE OF ANY KIND WHATSOEVER RELATED TO THE
* SOFTWARE, HOWEVER CAUSED, EVEN IF MICROCHIP HAS BEEN ADVISED OF THE
* POSSIBILITY OR THE DAMAGES ARE FORESEEABLE.  TO THE FULLEST EXTENT
* ALLOWED BY LAW, MICROCHIP'S TOTAL LIABILITY ON ALL CLAIMS IN ANY WAY
* RELATED TO THIS SOFTWARE WILL NOT EXCEED THE AMOUNT OF FEES, IF ANY,
* THAT YOU HAVE PAID DIRECTLY TO MICROCHIP FOR THIS SOFTWARE.
*
* \asf_license_stop
*
*/
/*
* Support and FAQ: visit <a href="https://www.microchip.com/support/">Microchip Support</a>
*/

/************************ HEADERS ****************************************/
#include "miwi_api.h"
#include "miwi_p2p_star.h"
#include "task.h"
#include "p2p_demo.h"
#include "mimem.h"
#include "asf.h"
#if defined(ENABLE_SLEEP_FEATURE)
#include "sleep_mgr.h"
#endif
#if defined (ENABLE_CONSOLE)
#include "sio2host.h"
#endif

#ifdef MIWI_AT_CMD
#include "atcmd.h"
#endif

#if defined(ENABLE_NETWORK_FREEZER)
#include "pdsMemIds.h"
#include "pdsDataServer.h"
#include "wlPdsTaskManager.h"
#endif

#if defined(PROTOCOL_P2P)
/************************ LOCAL VARIABLES ****************************************/
uint8_t i;
uint8_t TxSynCount = 0;
uint8_t TxSynCount2 = 0;
uint8_t TxNum = 0;
uint8_t RxNum = 0;
bool chk_sel_status = true;
uint8_t NumOfActiveScanResponse;
bool update_ed;
uint8_t select_ed;
uint8_t msghandledemo = 0;
/* Connection Table Memory */
extern CONNECTION_ENTRY connectionTable[CONNECTION_SIZE];

uint8_t cb_data[MAX_PAYLOAD];
uint8_t cb_data_fixed[MAX_PAYLOAD] = {
	0x31, 0x31, 0x31, 0x31, 0x31, 0x31, 0x31, 0x31, 0x31, 0x31,
	0x32, 0x32, 0x32, 0x32, 0x32, 0x32, 0x32, 0x32, 0x32, 0x32,
	0x33, 0x33, 0x33, 0x33, 0x33, 0x33, 0x33, 0x33, 0x33, 0x33,
	0x34, 0x34, 0x34, 0x34, 0x34, 0x34, 0x34, 0x34, 0x34, 0x34,
	0x35, 0x35, 0x35, 0x35, 0x35, 0x35, 0x35, 0x35, 0x35, 0x35,
	0x36, 0x36, 0x36, 0x36, 0x36, 0x36, 0x36, 0x36, 0x36, 0x36,
	0x37, 0x37, 0x37, 0x37, 0x37, 0x37, 0x37, 0x37, 0x37, 0x37,
	0x38, 0x38, 0x38, 0x38, 0x38, 0x38, 0x38, 0x38, 0x38, 0x38,
	0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39,
	0x30, 0x30, 0x30, 0x30, 0x30, 0x30, 0x30, 0x30, 0x30
};
bool cb_role = false;
bool test_back = false;
volatile uint8_t test_back_len = 0;

/************************ FUNCTION DEFINITIONS ****************************************/
/*********************************************************************
* Function: static void dataConfcb(uint8_t handle, miwi_status_t status)
*
* Overview: Confirmation Callback for MiApp_SendData
*
* Parameters:  handle - message handle, miwi_status_t status of data send
****************************************************************************/
#ifdef MIWI_AT_CMD
void dataConfcb(uint8_t handle, miwi_status_t status, uint8_t* msgPointer)
#else
static void dataConfcb(uint8_t handle, miwi_status_t status, uint8_t* msgPointer)
#endif
{

	++TxNum;

}

/*********************************************************************
* Function: void run_p2p_demo(void)
*
* Overview: runs the demo based on input
*
* Parameters: None
*********************************************************************/
void run_p2p_demo(void)
{
#ifdef PINGPONG_TEST
	if (test_back) {
		LED_Toggle(LED1);
		test_back = 0;
		uint16_t broadcastAddress = 0xFFFF;
		MiApp_SendData(SHORT_ADDR_LEN, (uint8_t *)&broadcastAddress, test_back_len, cb_data_fixed, msghandledemo++, true, dataConfcb);
		return;
	}
#endif

    uint8_t PressedButton = ButtonPressed();
    switch( PressedButton )
    {
        case 1:
        {
            /*******************************************************************/
            // Button 1 pressed. We need to send out the bitmap of word "MiWi".
            /*******************************************************************/
            uint16_t broadcastAddress = 0xFFFF;
            bool mac_ack_status;
			cb_data[0] = 0x00;
			cb_role = true;
			LED_Toggle(LED0);
            /* Function MiApp_SendData is used to broadcast a message with address as 0xFFFF */
            mac_ack_status = MiApp_SendData(SHORT_ADDR_LEN, (uint8_t *)&broadcastAddress, MAX_PAYLOAD, cb_data, msghandledemo++, false, dataConfcb);
            if (mac_ack_status)
            {
                /* Update the bitmap count */
                TxSynCount++;
            }
        }
        break;

        default:
            break;
    }

}

/*********************************************************************
* Function: void ReceivedDataIndication (RECEIVED_MESSAGE *ind)
*
* Overview: Process a Received Message
*
* PreCondition: MiApp_ProtocolInit
*
* Input:  RECEIVED_MESSAGE *ind - Indication structure
********************************************************************/
void ReceivedDataIndication (RECEIVED_MESSAGE *ind)
{
	/* Toggle LED2 to indicate receiving a packet */
	LED_Toggle(LED0);
#ifdef PINGPONG_TEST
	if (rxMessage.Payload[0] == 0x30) {
		LED_Toggle(LED1);
		test_back = true;
		test_back_len = rxMessage.PayloadSize;
	} else if (rxMessage.Payload[0] == 0x31) {
		LED_Toggle(LED1);
	}
#endif

#ifdef MIWI_AT_CMD
	ATCmd_SendReceiveData();
#endif
}
#endif