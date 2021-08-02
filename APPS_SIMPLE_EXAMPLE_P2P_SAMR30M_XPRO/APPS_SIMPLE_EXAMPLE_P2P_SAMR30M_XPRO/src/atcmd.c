#ifdef MIWI_AT_CMD
#include "string.h"
#include "asf.h"
#include "sio2host.h"
#include "atcmd.h"
#include "miwi_api.h"

/*****************************************************************************
*****************************************************************************/
extern void dataConfcb(uint8_t handle, miwi_status_t status, uint8_t* msgPointer);	//implemented in p2p_demo.c
extern void Connection_Confirm(miwi_status_t status);	//implemented in task.c
extern uint8_t msghandledemo;
extern CONNECTION_ENTRY connectionTable[CONNECTION_SIZE];
extern uint8_t myChannel;
#if defined(PROTOCOL_P2P)	
extern bool startNetwork;
#endif
uint16_t myPAN_ID = MY_PAN_ID;
bool freezer_enable = false;
bool manual_establish_network;
uint8_t enable_echo = 0;	//0: disable rx echo
uint8_t at_cfg_mode = 1;	//1: configure mode, 0: command mode
uint8_t reboot_reported = 0;	//1: reboot reported

static uint8_t at_rx_data[AT_RX_BUF_SIZE];	//buffer receiving rx data input
uint8_t rx_cmd[APP_RX_CMD_SIZE];	//rx command, single command buffer, SAMR30 will reply AOK, host must wait for AOK before sending next command
uint8_t tx_cmd[APP_TX_CMD_SIZE];	//tx command, single command buffer
uint8_t *prx_cmd, *ptag1, *ptag2, *ptag3, *ptag4;
//uint8_t rx_cmd_last_byte;
/*****************************************************************************
*****************************************************************************/
const char StrECHO[] = "echo";
const char StrExitECHO[] = "~echo";
const char StrCFG[] = "cfg";
const char StrExitCFG[] = "~cfg";
const char StrGET[] = "get";
const char StrSET[] = "set";
const char StrPAN[] = "pan";
const char StrCHANNEL[] = "channel";
const char StrRECONN[] = "reconn";

const char StrADDR[] = "addr";
const char StrVER[] = "ver";
const char StrSTART[] = "start";
const char StrJOIN[] = "join";
const char StrREMOVE[] = "remove";
const char StrCONN[] = "conn";
const char StrCONSIZE[] = "consize";
const char StrROLE[] = "role";
const char StrSEND[] = "send";
const char StrRECEIVE[] = "recv";
const char StrRESET[] = "reset";
const char StrEDSIZE[] = "edsize";
const char StrMYINDEX[] = "myindex";
const char StrEDS[] = "eds";
const char StrEVENT[] = "event";
const char StrERROR[] = "error";

const char StrAOK[] = "\nAOK\n\r";
const char StrAOK2[] = "AOK\r";
const char StrERR[] = "\nERR\n\r";
const char StrERR2[] = "ERR\r";
const char StrREBOOT[] = "Reboot\n\r";

#if defined(PROTOCOL_STAR)
 #if defined(PHY_AT86RF233)
 const char StrRET_VERSION[] = "\nver cmd06fw01d\n\r";	//SAMR21 Star
 const char StrRET_VERSION2[] = "ver cmd06fw01d\r";
 #else
 const char StrRET_VERSION[] = "\nver cmd06fw01b\n\r";	//SAMR30 Star
 const char StrRET_VERSION2[] = "ver cmd06fw01b\r";
 #endif
#else
 #if defined(PHY_AT86RF233)
 const char StrRET_VERSION[] = "\nver cmd06fw01c\n\r";	//SAMR21 P2P
 const char StrRET_VERSION2[] = "ver cmd06fw01c\r";
 #else
 const char StrRET_VERSION[] = "\nver cmd06fw01a\n\r";	//SAMR30 P2P
 const char StrRET_VERSION2[] = "ver cmd06fw01a\r";
 #endif
#endif


/*****************************************************************************
*****************************************************************************/
void ATCmd_ByteReceived(uint8_t byte);
void ATCmd_RxCmdInit( void );
void ATCmd_TxCmdInit( void );
/*****************************************************************************
*****************************************************************************/
static void channel2BCDStr(uint8_t channel, uint8_t *pTxt, uint8_t* txtSize)
{
	if(!pTxt)
		return;
	if(channel > 26)	//channel range: 0~10(RF212B), 11~26(RF233)
		return;
		
	if(channel < 10)
	{
		pTxt[0] = channel + 48;
		*txtSize = 1;
	}
	else if(channel < 20)
	{
		pTxt[0] = '1';
		pTxt[1] = channel + 38;
		*txtSize = 2;
	}
	else
	{
		pTxt[0] = '2';
		pTxt[1] = channel + 28;
		*txtSize = 2;
	}
}
static void channel2HexStr(uint8_t channel, uint8_t *pTxt, uint8_t* txtSize)
/*similar to num2HexStr(), but will remove first 0 character if number is < 16*/
{
	if(!pTxt)
	return;
	if(channel > 26)	//channel range: 0~10(RF212B), 11~26(RF233)
	return;
	
	if(channel < 10)
	{
		pTxt[0] = channel + 48;
		*txtSize = 1;
	}
	else if(channel < 16)
	{
		pTxt[0] = channel + 87;
		*txtSize = 1;
	}
	else if(channel < 26)
	{
		pTxt[0] = '1';
		pTxt[1] = channel + 32;
		*txtSize = 2;
	}
	else
	{
		pTxt[0] = '1';
		pTxt[1] = channel + 71;
		*txtSize = 2;
	}
}
static void num2HexStr(uint8_t *pHex, uint8_t hexSize, uint8_t *pTxt, uint8_t txtSize)
{
	uint8_t i, datah, datal;
	
	if(!pTxt || !pHex)
		return;
	if(hexSize == 0 || hexSize > 8)	//hex number need to be equal or smaller than 8
		return;
	if(txtSize < 2*hexSize)	//text buffer size must be greater than 2 times of hex size
		return;
	
	for(i=0; i<hexSize; i++)
	{
		datah = *pHex++;
		datal = datah&0x0f;
		datah = datah>>4;
		if(datah>=0x0a)
			datah += 87;//55;
		else
			datah += 0x30;
		*pTxt++ = datah;
		if(datal>=0x0a)
			datal += 87;//55;
		else
			datal += 0x30;
		*pTxt++ = datal;
	}
}
static void num2Hex(uint8_t num, uint8_t *pTxt, uint8_t* txtSize)
{
	if(!pTxt)
		return;
		
	if(num < 10)
	{
		pTxt[0] = num + 48;
		*txtSize = 1;
	}
	else if(num < 16)
	{
		pTxt[0] = num + 87;
		*txtSize = 1;
	}
	else if(num < 0xa0)
	{
		if(num%16 < 10)
		{
			pTxt[0] = num/16 + 48;
			pTxt[1] = num%16 + 48;
			*txtSize = 2;
		}
		else
		{
			pTxt[0] = num/16 + 48;
			pTxt[1] = num%16 + 87;
			*txtSize = 2;
		}
	}
	else
	{
		if(num%16 < 10)
		{
			pTxt[0] = num/16 + 87;
			pTxt[1] = num%16 + 48;
			*txtSize = 2;
		}
		else
		{
			pTxt[0] = num/16 + 87;
			pTxt[1] = num%16 + 87;
			*txtSize = 2;
		}
	}
}
static uint8_t char2byte(uint8_t charb)	//only used by str2byte()
{
	uint8_t a;
	if(charb >= 0x41 && charb <= 0x46 )
		a = charb - 0x41 + 10;
	else if(charb >= 0x61 && charb <= 0x66 )
		a = charb - 0x61 + 10;
	else if(charb >= 0x30 && charb <= 0x39 )
		a = charb - 0x30;
	else
		a = 0;
	return a;
}
static uint8_t str2byte(uint8_t* str)
{
	uint8_t strb[3];
	uint8_t bb;
	strb[0] = *str++;
	strb[1] = *str;
	strb[2] = 0;
	bb = char2byte(strb[0]);
	if(strb[1])
	{
		bb <<= 4;
		bb += char2byte(strb[1]);
	}
	return bb;
}

/*****************************************************************************
*****************************************************************************/
static void ATCmd_ResponseAOK( void )
{
	if(enable_echo)
		sio2host_tx((uint8_t *)StrAOK, sizeof(StrAOK));
	else
		sio2host_tx((uint8_t *)StrAOK2, sizeof(StrAOK2));
}
static void ATCmd_ResponseERR( void )
{
	if(enable_echo)
		sio2host_tx((uint8_t *)StrERR, sizeof(StrERR));
	else
		sio2host_tx((uint8_t *)StrERR2, sizeof(StrERR2));
}

/*
error r1
r1: 0: connection failure
	1: reconnection failure
	2: link failure, BUSY, NO ACK, ...
*/
void ATCmd_SendErrorCode( uint8_t error_code )
{
	uint8_t tx_data[10];
	uint8_t* ptx_data = tx_data;
	uint8_t tx_data_len = 0;
	uint8_t hex[2];
	uint8_t hex_len;
	
	//beginning is adding '\n' good for PC terminal display
	if(enable_echo)
	{
		*ptx_data++ = '\n';
		tx_data_len++;
	}
	
	//add command tag
	*ptx_data++ = 'e';
	*ptx_data++ = 'v';
	*ptx_data++ = 'e';
	*ptx_data++ = 'n';
	*ptx_data++ = 't';
	*ptx_data++ = ' ';
	tx_data_len += 6;
	
	//add r1
	num2Hex(error_code, hex, &hex_len);
	if(hex_len == 1)
	{
		*ptx_data++ = hex[0];
		tx_data_len ++;
	}
	else if(hex_len == 2)
	{
		*ptx_data++ = hex[0];
		*ptx_data++ = hex[1];
		tx_data_len += 2;
	}
	
	//tail is adding 'r'
	if(enable_echo)
	{
		*ptx_data++ = '\n';
		*ptx_data++ = '\r';
		tx_data_len += 2;
	}
	else
	{
		*ptx_data++ = '\r';	//0x13, ENTER
		tx_data_len += 1;
	}
	
	//send data out and initialize
	sio2host_tx(tx_data, tx_data_len);
	ATCmd_TxCmdInit();
}
/*
event r1
r1: 0: connection table change, for Star PAN or P2P all devices
    1: shared connection table change, for Star End Devices(reserved now)
*/
void ATCmd_SendStatusChange( uint8_t event_code )
{
	uint8_t tx_data[10];
	uint8_t* ptx_data = tx_data;
	uint8_t tx_data_len = 0;
	uint8_t hex[2];
	uint8_t hex_len;
	
	//beginning is adding '\n' good for PC terminal display
	if(enable_echo)
	{
		*ptx_data++ = '\n';
		tx_data_len++;
	}
	
	//add command tag
	*ptx_data++ = 'e';
	*ptx_data++ = 'v';
	*ptx_data++ = 'e';
	*ptx_data++ = 'n';
	*ptx_data++ = 't';
	*ptx_data++ = ' ';
	tx_data_len += 6;
	
	//add r1
	num2Hex(event_code, hex, &hex_len);
	if(hex_len == 1)
	{
		*ptx_data++ = hex[0];
		tx_data_len ++;
	}
	else if(hex_len == 2)
	{
		*ptx_data++ = hex[0];
		*ptx_data++ = hex[1];
		tx_data_len += 2;
	}
	
	//tail is adding 'r'
	if(enable_echo)
	{
		*ptx_data++ = '\n';
		*ptx_data++ = '\r';
		tx_data_len += 2;
	}
	else
	{
		*ptx_data++ = '\r';	//0x13, ENTER
		tx_data_len += 1;
	}
	
	//send data out and initialize
	sio2host_tx(tx_data, tx_data_len);
	ATCmd_TxCmdInit();
}

/*
conn r1 r2 r3
r1: index, valid from 0
r2: 1: valid connection. 0: invalid connection
r3: 8bytes long address
*/
void ATCmd_SendConnectionChange( uint8_t index )
{
	uint8_t* pStr1 = tx_cmd;
	uint8_t str1Len = 0;
	uint8_t hex[2];
	uint8_t hex_len;
	uint8_t conn_sts;

	if(enable_echo)
	{
		*pStr1++ = '\n';
		str1Len++;
	}
	*pStr1++ = 'c';	//add "conn"
	*pStr1++ = 'o';
	*pStr1++ = 'n';
	*pStr1++ = 'n';
	*pStr1++ = ' ';
	str1Len += 5;
	
	//r1: connection index
	num2Hex(index, hex, &hex_len);
	if(hex_len == 1)
	{
		*pStr1++ = hex[0];
		*pStr1++ = ' ';
		str1Len += 2;
	}
	else if(hex_len == 2)
	{
		*pStr1++ = hex[0];
		*pStr1++ = hex[1];
		*pStr1++ = ' ';
		str1Len += 3;
	}
	
	//r2: connection valid/invalid, link status
#if 0	//add link status
	conn_sts = 0;
 #if defined(PROTOCOL_STAR)
 #if defined(ENABLE_LINK_STATUS)
	conn_sts = connectionTable[index].link_status;
 #endif
 #endif
	if( connectionTable[index].status.bits.isValid )
		conn_sts |= 0x80;
	num2Hex(conn_sts, hex, &hex_len);
	if(hex_len == 1)
	{
		*pStr1++ = hex[0];
		*pStr1++ = ' ';
		str1Len += 2;
	}
	else
	{
		*pStr1++ = hex[0];
		*pStr1++ = hex[1];
		*pStr1++ = ' ';
		str1Len += 3;
	}
#else
	if( connectionTable[index].status.bits.isValid )
	{
		*pStr1++ = '1';	//'1' means valid
		*pStr1++ = ' ';
		str1Len += 2;
	}
	else
	{
		*pStr1++ = '0';	//'0' means invalid
		*pStr1++ = ' ';
		str1Len += 2;
	}
#endif	
	
	//r3: connection long address
	num2HexStr(connectionTable[index].Address, MY_ADDRESS_LENGTH, pStr1, 2*MY_ADDRESS_LENGTH+1);
	pStr1 += 2*MY_ADDRESS_LENGTH;
	str1Len += 2*MY_ADDRESS_LENGTH;
	
	if(enable_echo)
	{
		*pStr1++ = '\n';
		*pStr1 = '\r';
		str1Len += 2;
	}
	else
	{
		*pStr1++ = '\r';	//0x13, ENTER
		str1Len += 1;
	}
	sio2host_tx(tx_cmd, str1Len);
	ATCmd_TxCmdInit();
}
/*
recv r1 r2 r3 r4
r1: 0: unicast&unsecured, 1: unicast&secured, 2: broadcast&unsecured, 3: broadcast&secured
r2: rssi value
r3: address(8bytes long address or 2bytes short address
r4: received data
** there is no connection index or received data size as parameters **
*/
void ATCmd_SendReceiveData( void )
{
	uint8_t* ptx_cmd = tx_cmd;
	uint16_t tx_cmd_len = 0;
	uint8_t temp;
	
	if(enable_echo)
	{
		*ptx_cmd++ = '\n';
		tx_cmd_len++;
	}
	
	*ptx_cmd++ = 'r';	//add "recv"
	*ptx_cmd++ = 'e';
	*ptx_cmd++ = 'c';
	*ptx_cmd++ = 'v';
	*ptx_cmd++ = ' ';
	tx_cmd_len += 5;
	
	//r1: security&broadcast(unicast)
	temp = 0;
	if( rxMessage.flags.bits.secEn )
		temp |= 0x02;
	if( rxMessage.flags.bits.broadcast )
		temp |= 0x01;
	num2HexStr(&temp, 1, ptx_cmd, 2);
	ptx_cmd += 2; 
	tx_cmd_len += 2;
	*ptx_cmd++ = ' ';
	tx_cmd_len ++;
	
	//r2: rssi
	temp = rxMessage.PacketRSSI;
	num2HexStr(&temp, 1, ptx_cmd, 2);
	ptx_cmd += 2;
	tx_cmd_len += 2;
	*ptx_cmd++ = ' ';
	tx_cmd_len ++;
	
	//r3: address
	if( rxMessage.flags.bits.srcPrsnt )
	{
		if( rxMessage.flags.bits.altSrcAddr )
		{
			num2HexStr(rxMessage.SourceAddress, 2, ptx_cmd, 4);
			ptx_cmd += 4;
			tx_cmd_len += 4;
		}
		else
		{
			num2HexStr(rxMessage.SourceAddress, MY_ADDRESS_LENGTH, ptx_cmd, 2*MY_ADDRESS_LENGTH);
			ptx_cmd += 2*MY_ADDRESS_LENGTH;
			tx_cmd_len += 2*MY_ADDRESS_LENGTH;
		}
	}
	else
	{
		//if no source address presented, use "ffff" to indicate this situation
		*ptx_cmd++ = 'f';
		*ptx_cmd++ = 'f';
		*ptx_cmd++ = 'f';
		*ptx_cmd++ = 'f';
		tx_cmd_len += 4;
	}
	*ptx_cmd++ = ' ';
	tx_cmd_len ++;
	
	for(temp = 0; temp < rxMessage.PayloadSize; temp++)
	{
		*ptx_cmd++ = rxMessage.Payload[temp];
		tx_cmd_len++;
	}
	
	if(enable_echo)
	{
		*ptx_cmd++ = '\n';
		*ptx_cmd++ = '\r';
		tx_cmd_len += 2;
	}
	else
	{
		*ptx_cmd++ = '\r';	//0x13, ENTER
		tx_cmd_len += 1;
	}
	
	sio2host_tx(tx_cmd, tx_cmd_len);
	ATCmd_TxCmdInit();
}

void ATCmd_ProcessCommand( void )
{
	uint8_t temp, index, data_size;
	uint8_t str1[APP_TX_CMD_SIZE];	//can be replaced by tx_cmd[] global array
	uint8_t* pStr1;
	uint8_t str1Len;
	uint8_t pana[2];
	uint8_t conn_sts;

//process this command
	switch(ptag1[0])
	{
		case 'c':
		//case 'C':
		if(strcmp(StrCFG, (const char*)ptag1) == 0)
		{
			if(!at_cfg_mode)	//not in configure mode
			ATCmd_ResponseERR();
			else if(strcmp(StrPAN, (const char*)ptag2) == 0)		//command: cfg pan r1
			{
				ATCmd_ResponseAOK();
				myPAN_ID = (uint16_t)str2byte(ptag3);
				ptag3+=2;
				myPAN_ID <<= 8;
				myPAN_ID += (uint16_t)str2byte(ptag3);
				//printf("%x", myPAN_ID);	//debug
			}
			else if(strcmp(StrRECONN, (const char*)ptag2) == 0)		//command: cfg reconn r1(0/1/2)
			{
				temp = str2byte(ptag3);
				if(temp == 0 || temp == 1 || temp == 2)
				{
					//0: new network with auto running.
					//1: network reconnection, if no reconnection available, then auto run to establish network.
					//2: wait for "start" and "join" to establish new network.
					ATCmd_ResponseAOK();
					if(temp == 0 || temp == 2)
					freezer_enable = false;
					else
					freezer_enable = true;
					if(temp == 2)
					manual_establish_network = true;
					else
					manual_establish_network = false;
				}
				else
				{
					ATCmd_ResponseERR();
					freezer_enable = 0;
					manual_establish_network = false;
				}
				//printf("%d", freezer_enable);	//debug
			}
			else if(strcmp(StrCHANNEL, (const char*)ptag2) == 0)	//command: cfg channel r1
			{
				temp = str2byte(ptag3);
#if defined(PHY_AT86RF233)
				if(temp > 26 || temp < 11)		//SAMR21
#else
				if(temp > 10)					//SAMR30
#endif				
				
					ATCmd_ResponseERR();
				else
				{
					ATCmd_ResponseAOK();
					myChannel = str2byte(ptag3);
				}
				//printf("%d", myChannel);	//debug
			}
			else
			{
				ATCmd_ResponseERR();
			}
		}
		else
		{
			ATCmd_ResponseERR();
		}
		break;
		
		case 'g':
		//case 'G':
		if(strcmp(StrGET, (const char*)ptag1) == 0)
		{
			if(strcmp(StrADDR, (const char*)ptag2) == 0)	//command: get addr
			{
				memset(str1, 0, sizeof(str1));
				pStr1 = str1;
				str1Len = 0;
				if(enable_echo)
				{
				*pStr1++ = '\n';
				str1Len++;
				}
			*pStr1++ = 'a';	//add "addr"
			*pStr1++ = 'd';
			*pStr1++ = 'd';
			*pStr1++ = 'r';
			*pStr1++ = ' ';
			str1Len += 5;
			num2HexStr(myLongAddress, MY_ADDRESS_LENGTH, pStr1, 2*MY_ADDRESS_LENGTH+1);
			pStr1 += 2*MY_ADDRESS_LENGTH;
			str1Len += 2*MY_ADDRESS_LENGTH;
				if(enable_echo)
				{
				*pStr1++ = '\n';
				*pStr1 = '\r';
				str1Len += 2;
				}
				else
				{
				*pStr1++ = '\r';	//0x13, ENTER
				str1Len += 1;
				}
				sio2host_tx(str1, str1Len);
			}
			else if(strcmp(StrCHANNEL, (const char*)ptag2) == 0)	//command: get channel
			{
				memset(str1, 0, sizeof(str1));
				pStr1 = str1;
				str1Len = 0;
				if(enable_echo)
				{
				*pStr1++ = '\n';
				str1Len++;
				}
				*pStr1++ = 'c';	//add "channel"
				*pStr1++ = 'h';
				*pStr1++ = 'a';
				*pStr1++ = 'n';
				*pStr1++ = 'n';
				*pStr1++ = 'e';
				*pStr1++ = 'l';
				*pStr1++ = ' ';
				str1Len += 8;
			
				if(!at_cfg_mode)	//afer exiting from config mode, use API to read channel; in config mode, just use variable myChannel
					MiApp_Get(CHANNEL, &myChannel);
				//channel2BCDStr(myChannel, pStr1, &temp);
				channel2HexStr(myChannel, pStr1, &temp);
				pStr1 += temp;
				str1Len += temp;
			
				if(enable_echo)
				{
				*pStr1++ = '\n';
				*pStr1 = '\r';
				str1Len += 2;
				}
				else
				{
				*pStr1++ = '\r';	//0x13, ENTER
				str1Len += 1;
				}
				sio2host_tx(str1, str1Len);
			}
			else if(strcmp(StrPAN, (const char*)ptag2) == 0)	//command: get pan
			{
				memset(str1, 0, sizeof(str1));
				pStr1 = str1;
				str1Len = 0;
				if(enable_echo)
				{
				*pStr1++ = '\n';
				str1Len++;
				}
				*pStr1++ = 'p';	//add "pan"
				*pStr1++ = 'a';
				*pStr1++ = 'n';
				*pStr1++ = ' ';
				str1Len += 4;
				
				if(!at_cfg_mode)	//afer exiting from config mode, use API to read PAN ID; in config mode, just use variable myPAN_ID
					MiApp_Get(PANID, &myPAN_ID);
				pana[0] = (uint8_t)(myPAN_ID>>8);
				pana[1] = (uint8_t)myPAN_ID;
				num2HexStr(pana, 2, pStr1, 5);
				pStr1 += 4;
				str1Len += 4;
				
				if(enable_echo)
				{
				*pStr1++ = '\n';
				*pStr1 = '\r';
				str1Len += 2;
				}
				else
				{
					*pStr1++ = '\r';	//0x13, ENTER
					str1Len += 1;
				}
				sio2host_tx(str1, str1Len);
			}
			else if(!at_cfg_mode && (strcmp(StrROLE, (const char*)ptag2) == 0))	//command: get role
			{
				memset(str1, 0, sizeof(str1));
				pStr1 = str1;
				str1Len = 0;
				if(enable_echo)
				{
					*pStr1++ = '\n';
					str1Len++;
				}
				*pStr1++ = 'r';	//add "role"
				*pStr1++ = 'o';
				*pStr1++ = 'l';
				*pStr1++ = 'e';
				*pStr1++ = ' ';
				str1Len += 5;
			
				temp = 0;
#if defined(PROTOCOL_P2P)				
				if(startNetwork)
					temp |= 0x01;
#endif					
#if defined(PROTOCOL_STAR)
				if(role == PAN_COORD)
					temp |= 0x02;
#endif		
				num2HexStr(&temp, 1, pStr1, 2);
				pStr1 += 2;
				str1Len += 2;		

				if(enable_echo)
				{
					*pStr1++ = '\n';
					*pStr1 = '\r';
					str1Len += 2;
				}
				else
				{
					*pStr1++ = '\r';	//0x13, ENTER
					str1Len += 1;
				}
				sio2host_tx(str1, str1Len);
			}
			else if(!at_cfg_mode && (strcmp(StrCONSIZE, (const char*)ptag2) == 0))	//command: get consize
			{
				memset(str1, 0, sizeof(str1));
				pStr1 = str1;
				str1Len = 0;
				if(enable_echo)
				{
					*pStr1++ = '\n';
					str1Len++;
				}
				*pStr1++ = 'c';	//add "consize"
				*pStr1++ = 'o';
				*pStr1++ = 'n';
				*pStr1++ = 's';
				*pStr1++ = 'i';
				*pStr1++ = 'z';
				*pStr1++ = 'e';
				*pStr1++ = ' ';
				str1Len += 8;
			
				temp = Total_Connections();
				num2HexStr(&temp, 1, pStr1, 2);
				pStr1 += 2;
				str1Len += 2;
			
				if(enable_echo)
				{
				*pStr1++ = '\n';
				*pStr1 = '\r';
				str1Len += 2;
				}
				else
				{
				*pStr1++ = '\r';	//0x13, ENTER
				str1Len += 1;
				}
				sio2host_tx(str1, str1Len);
			}
			else if(!at_cfg_mode && (strcmp(StrCONN, (const char*)ptag2) == 0))	//command: get conn r1
			{
				temp = str2byte(ptag3);
				if(temp >= Total_Connections())
					ATCmd_ResponseERR();
				else
				{
					memset(str1, 0, sizeof(str1));
					pStr1 = str1;
					str1Len = 0;
					if(enable_echo)
					{
					*pStr1++ = '\n';
					str1Len++;
					}
					*pStr1++ = 'c';	//add "conn"
					*pStr1++ = 'o';
					*pStr1++ = 'n';
					*pStr1++ = 'n';
					*pStr1++ = ' ';
					str1Len += 5;
			
					//r1: connection index
					num2Hex(temp, pana, &data_size);
					if(data_size == 1)
					{
						*pStr1++ = pana[0];
						*pStr1++ = ' ';
						str1Len += 2;
					}
					else if(data_size == 2)
					{
						*pStr1++ = pana[0];
						*pStr1++ = pana[1];
						*pStr1++ = ' ';
						str1Len += 3;
					}
			
					//r2: connection valid/invalid, link status
#if 0	//add link status					
					conn_sts = 0;
 #if defined(PROTOCOL_STAR)
 #if defined(ENABLE_LINK_STATUS)					
					conn_sts = connectionTable[temp].link_status;
 #endif
 #endif
					if( connectionTable[temp].status.bits.isValid )
						conn_sts |= 0x80;
					num2Hex(conn_sts, pana, &data_size);
					if(data_size == 1)
					{
						*pStr1++ = pana[0];
						*pStr1++ = ' ';
						str1Len += 2;
					}
					else
					{
						*pStr1++ = pana[0];
						*pStr1++ = pana[1];
						*pStr1++ = ' ';
						str1Len += 3;
					}
#else
					if( connectionTable[temp].status.bits.isValid )
					{
						*pStr1++ = '1';	//'1' means valid
						*pStr1++ = ' ';
						str1Len += 2;
					}
					else
					{
						*pStr1++ = '0';	//'0' means invalid
						*pStr1++ = ' ';
						str1Len += 2;
					}
#endif					
				
					//r3: connection IEEE long address
					num2HexStr(connectionTable[temp].Address, MY_ADDRESS_LENGTH, pStr1, 2*MY_ADDRESS_LENGTH+1);
					pStr1 += 2*MY_ADDRESS_LENGTH;
					str1Len += 2*MY_ADDRESS_LENGTH;
			
					if(enable_echo)
					{
						*pStr1++ = '\n';
						*pStr1 = '\r';
						str1Len += 2;
					}
					else
					{
						*pStr1++ = '\r';	//0x13, ENTER
						str1Len += 1;
					}
					sio2host_tx(str1, str1Len);
				}
			}
			else if(strcmp(StrVER, (const char*)ptag2) == 0)	//command: get ver
			{
				if(enable_echo)
				sio2host_tx((uint8_t *)StrRET_VERSION, sizeof(StrRET_VERSION));
				else
				sio2host_tx((uint8_t *)StrRET_VERSION2, sizeof(StrRET_VERSION2));
			}
			else if(!at_cfg_mode && (strcmp(StrEDSIZE, (const char*)ptag2) == 0))	//command: get edsize
			{
#if defined(PROTOCOL_STAR)
				if(role == END_DEVICE)
				{
					memset(str1, 0, sizeof(str1));
					pStr1 = str1;
					str1Len = 0;
					if(enable_echo)
					{
						*pStr1++ = '\n';
						str1Len++;
					}
					*pStr1++ = 'e';	//add "edsize"
					*pStr1++ = 'd';
					*pStr1++ = 's';
					*pStr1++ = 'i';
					*pStr1++ = 'z';
					*pStr1++ = 'e';
					*pStr1++ = ' ';
					str1Len += 7;
					
					num2Hex(end_nodes, pStr1, &temp);
					if(temp == 1)
					{
						pStr1++;
						str1Len ++;
					}
					else
					{
						pStr1 += 2;
						str1Len += 2;
					}
					if(enable_echo)
					{
						*pStr1++ = '\n';
						*pStr1 = '\r';
						str1Len += 2;
					}
					else
					{
						*pStr1++ = '\r';	//0x13, ENTER
						str1Len += 1;
					}
					sio2host_tx(str1, str1Len);
				}
				else
				{
					ATCmd_ResponseERR();
				}
#else
				ATCmd_ResponseERR();
#endif				
			}
			else if(!at_cfg_mode && (strcmp(StrMYINDEX, (const char*)ptag2) == 0))	//command: get myindex
			{
#if defined(PROTOCOL_STAR)
				if(role == END_DEVICE)
				{
					memset(str1, 0, sizeof(str1));
					pStr1 = str1;
					str1Len = 0;
					if(enable_echo)
					{
						*pStr1++ = '\n';
						str1Len++;
					}
					*pStr1++ = 'm';	//add "edsize"
					*pStr1++ = 'y';
					*pStr1++ = 'i';
					*pStr1++ = 'n';
					*pStr1++ = 'd';
					*pStr1++ = 'e';
					*pStr1++ = 'x';
					*pStr1++ = ' ';
					str1Len += 8;
					
					num2Hex(myConnectionIndex_in_PanCo, pStr1, &temp);
					if(temp == 1)
					{
						pStr1++;
						str1Len ++;
					}
					else
					{
						pStr1 += 2;
						str1Len += 2;
					}
					if(enable_echo)
					{
						*pStr1++ = '\n';
						*pStr1 = '\r';
						str1Len += 2;
					}
					else
					{
						*pStr1++ = '\r';	//0x13, ENTER
						str1Len += 1;
					}
					sio2host_tx(str1, str1Len);
				}
				else
				{
					ATCmd_ResponseERR();
				}
#else
				ATCmd_ResponseERR();
#endif
			}
			else if(!at_cfg_mode && (strcmp(StrEDS, (const char*)ptag2) == 0))	//command: get eds r1 r2
			//r1 is start index of eds reading, r2 is end index of eds reading.
			{
#if defined(PROTOCOL_STAR)
				if(role == END_DEVICE)
				{
					if(ptag3 && ptag4)
					{
						temp = str2byte(ptag3);	//get start index of eds reading
						data_size = str2byte(ptag4);	//get end index of eds reading
						if(temp > data_size || temp >= end_nodes || data_size >= end_nodes)
							ATCmd_ResponseERR();
						else
						{
						memset(str1, 0, sizeof(str1));
						pStr1 = str1;
						str1Len = 0;
						if(enable_echo)
						{
							*pStr1++ = '\n';
							str1Len++;
						}
						*pStr1++ = 'e';	//add "eds"
						*pStr1++ = 'd';
						*pStr1++ = 's';
						*pStr1++ = ' ';
						str1Len += 4;
						
						for(index=temp; index<=data_size; index++)
						{
							num2HexStr(&END_DEVICES_Short_Address[index].Address[0], 3, pStr1, 6);
							pStr1+=6;
							str1Len += 6;
							num2HexStr(&END_DEVICES_Short_Address[index].connection_slot, 1, pStr1, 2);
							pStr1+=2;
							str1Len += 2;
						}
						
						if(enable_echo)
						{
							*pStr1++ = '\n';
							*pStr1 = '\r';
							str1Len += 2;
						}
						else
						{
							*pStr1++ = '\r';	//0x13, ENTER
							str1Len += 1;
						}
						sio2host_tx(str1, str1Len);
						}	//start index, end index, end_nodes value check
					}
					else //r1, r2 parameters is incomplete
					{
						ATCmd_ResponseERR();
					}
				}
				else
				{
					ATCmd_ResponseERR();
				}
#else
				ATCmd_ResponseERR();
#endif				
			}
			else
			{
				ATCmd_ResponseERR();
			}
		}
		else
		{
			ATCmd_ResponseERR();
		}
		break;
		
		case 's':
		//case 'S':
		if(!at_cfg_mode && (strcmp(StrSTART, (const char*)ptag1) == 0))	//command: start
		{
			ATCmd_ResponseAOK();
#if defined(PROTOCOL_P2P)				
			startNetwork = true;
#endif			
			MiApp_StartConnection(START_CONN_DIRECT, 10, (1L << myChannel), Connection_Confirm);			
		}
		else if(!at_cfg_mode && (strcmp(StrSEND, (const char*)ptag1) == 0))	//command: send r1 r2 r3
		{
			if(ptag2 && ptag3 && ptag4)
			{
				temp = strlen(ptag2);
				data_size = str2byte(ptag3);
				if(temp == 1 || temp == 2)	//unicast, by peer device index
				{
					index = str2byte(ptag2);
					if(index >= Total_Connections())
					ATCmd_ResponseERR();
					else
					{
						ATCmd_ResponseAOK();
						if(!data_size)	//if r2=0, count r3 bytes and use counted number
						MiApp_SendData(LONG_ADDR_LEN, connectionTable[index].Address, strlen(ptag4), ptag4, msghandledemo++, true, dataConfcb);
						else
						MiApp_SendData(LONG_ADDR_LEN, connectionTable[index].Address, data_size, ptag4, msghandledemo++, true, dataConfcb);
					}
				}
				else if(temp == 4)	//broadcast, by 0xFFFF broadcast address
				{
					uint16_t broadcastAddress = 0xFFFF;
					if((strcmp(ptag2, "ffff") == 0) || strcmp(ptag2, "FFFF") == 0)
					{
						ATCmd_ResponseAOK();
						if(!data_size)	//if r2=0, count r3 bytes and use counted number
						MiApp_SendData(SHORT_ADDR_LEN, (uint8_t *)&broadcastAddress, strlen(ptag4), ptag4, msghandledemo++, false, dataConfcb);
						else
						MiApp_SendData(SHORT_ADDR_LEN, (uint8_t *)&broadcastAddress, data_size, ptag4, msghandledemo++, false, dataConfcb);
					}
					else
					{
						ATCmd_ResponseERR();
					}
				}
				else if(temp == 6)	//unicast, Star edx -> edy only
				{
#if defined(PROTOCOL_STAR)
					if(role == END_DEVICE)
					{
						uint8_t desShortAddress[3];
						ATCmd_ResponseAOK();
						pStr1 = ptag2;
						desShortAddress[0] = str2byte(pStr1);
						pStr1+=2;
						desShortAddress[1] = str2byte(pStr1);
						pStr1+=2;
						desShortAddress[2] = str2byte(pStr1);
						if(!data_size)	//if r2=0, count r3 bytes and use counted number
							MiApp_SendData(3, desShortAddress, strlen(ptag4), ptag4, msghandledemo++, true, dataConfcb);
						else
							MiApp_SendData(3, desShortAddress, data_size, ptag4, msghandledemo++, true, dataConfcb);
					}
					else
					{
						ATCmd_ResponseERR();
					}
#else
					ATCmd_ResponseERR();
#endif
				}
				else if(temp == 16)	//unicast, by 8bytes IEEE long address
				{
					uint8_t destLongAddress[8];
					ATCmd_ResponseAOK();
					pStr1 = ptag2;
					destLongAddress[0] = str2byte(pStr1);
					pStr1+=2;
					destLongAddress[1] = str2byte(pStr1);
					pStr1+=2;
					destLongAddress[2] = str2byte(pStr1);
					pStr1+=2;
					destLongAddress[3] = str2byte(pStr1);
					pStr1+=2;
					destLongAddress[4] = str2byte(pStr1);
					pStr1+=2;
					destLongAddress[5] = str2byte(pStr1);
					pStr1+=2;
					destLongAddress[6] = str2byte(pStr1);
					pStr1+=2;
					destLongAddress[7] = str2byte(pStr1);
					if(!data_size)	//if r2=0, count r3 bytes and use counted number
					MiApp_SendData(LONG_ADDR_LEN, destLongAddress, strlen(ptag4), ptag4, msghandledemo++, true, dataConfcb);
					else
					MiApp_SendData(LONG_ADDR_LEN, destLongAddress, data_size, ptag4, msghandledemo++, true, dataConfcb);
				}
				else
				{
					ATCmd_ResponseERR();
				}
			}
			else
			{
				ATCmd_ResponseERR();
			}
		}
		else
		{
			ATCmd_ResponseERR();
		}
		break;
		
		case 'j':
		//case 'J':
		if(!at_cfg_mode && (strcmp(StrJOIN, (const char*)ptag1) == 0))	//command: join
		{
			uint16_t broadcastAddr = 0xFFFF;
			ATCmd_ResponseAOK();
#if defined(PROTOCOL_P2P)				
			startNetwork = false;
#endif			
			MiApp_EstablishConnection(myChannel, 2, (uint8_t*)&broadcastAddr, 0, Connection_Confirm);
		}
		else
		{
			ATCmd_ResponseERR();
		}
		break;
		
		case 'r':
		//case 'R':
		if(strcmp(StrRESET, (const char*)ptag1) == 0)		//command: reset
		{
			ATCmd_ResponseAOK();
			MiApp_ResetToFactoryNew();
		}
		else if(!at_cfg_mode &&  (strcmp(StrREMOVE, (const char*)ptag1) == 0))		//command: remove r1
		{
			temp = str2byte(ptag2);
			if(temp >= Total_Connections())
				ATCmd_ResponseERR();
			else
			{
				ATCmd_ResponseAOK();
				MiApp_RemoveConnection(temp);
			}
		}
		else
		{
			ATCmd_ResponseERR();
		}
		break;
		
		case 'e':
		//case 'E':
		if(strcmp(StrECHO, (const char*)ptag1) == 0)		//command: echo
		{
			ATCmd_ResponseAOK();
			enable_echo = 1;
		}
		else
		{
			ATCmd_ResponseERR();
		}
		break;
		
		case '~':
		if(strcmp(StrExitCFG, (const char*)ptag1) == 0)	//command: ~cfg
		{
			ATCmd_ResponseAOK();
			at_cfg_mode = 0;	//go to action mode
		}
		else if(strcmp(StrExitECHO, (const char*)ptag1) == 0)	//command: ~echo
		{
			ATCmd_ResponseAOK();
			enable_echo = 0;
		}
		else
		{
			ATCmd_ResponseERR();
		}
		break;
		
		default:
		break;
	}

	//initialize rx command and its pointers
	ATCmd_RxCmdInit();
}
/*****************************************************************************
*****************************************************************************/
void ATCmd_ByteReceived(uint8_t byte)
{
	if(byte == 0x0D)	//ENTER character
	{
		//get to command end
		ATCmd_ProcessCommand();		
		return;
	}
	//rx_cmd_last_byte = byte;
	if((byte == 0x20) && (!ptag2 || !ptag3 || !ptag4))	//SPACE check
	{
		byte = 0;		//replace SPACE with NULL
		*prx_cmd++ = byte;
		if(!ptag2)
			ptag2 = prx_cmd;
		else if(!ptag3)
			ptag3 = prx_cmd;
		else if(!ptag4)
			ptag4 = prx_cmd;
	}
	else
		*prx_cmd++ = byte;
}
/*****************************************************************************
*****************************************************************************/
void ATCmd_RxCmdInit( void )
{
	prx_cmd = rx_cmd;
	ptag1 = rx_cmd;
	ptag2 = 0;
	ptag3 = 0;
	ptag4 = 0;
	memset(rx_cmd, 0, APP_RX_CMD_SIZE);
	//rx_cmd_last_byte = 0;
}
/*****************************************************************************
*****************************************************************************/
void ATCmd_TxCmdInit( void )
{
	memset(tx_cmd, 0, APP_TX_CMD_SIZE);
}
/*****************************************************************************
*****************************************************************************/
void ATCmd_CmdInit( void )
{
	reboot_reported = 0;
	at_cfg_mode = 1;
	enable_echo = 0;
	manual_establish_network = false;
	ATCmd_RxCmdInit();
	ATCmd_TxCmdInit();
}
/*****************************************************************************
*****************************************************************************/
void ATCmdTask(void)
{
	uint16_t bytes;
	if(reboot_reported == 0)
	{
		reboot_reported = 1;
		sio2host_tx((uint8_t *)StrREBOOT, sizeof(StrAOK));
	}
	if ((bytes = sio2host_rx(at_rx_data, AT_RX_BUF_SIZE)) > 0) {
		if(enable_echo)
			sio2host_tx(at_rx_data, bytes);		//echo back
		for (uint16_t i = 0; i < bytes; i++) {
			ATCmd_ByteReceived(at_rx_data[i]);
		}
	}
}

uint8_t ATCmd_IsCfgMode(void)
{
	return at_cfg_mode;
}
#endif
