#ifdef MIWI_AT_CMD
#ifndef AT_CMD_H
#define AT_CMD_H

#define AT_RX_BUF_SIZE         64//100
//#define AT_TX_BUF_SIZE			100
#define APP_RX_CMD_SIZE         150//200
#define APP_TX_CMD_SIZE			64//200

void ATCmd_CmdInit( void );
void ATCmdTask(void);
uint8_t ATCmd_IsCfgMode(void);
void ATCmd_RendReceiveData( void );

#endif
#endif
