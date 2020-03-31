﻿#ifndef _DOWNSTREAM_H_
#define _DOWNSTREAM_H_

#define DOWNSTREAM_UART_NAME  "uart2"
#define DATA_CMD_END      '\r' /* 结束位设置为\r， 即回车符*/
#define ONE_DATA_MAXLEN    128 /* 不定长数据的最大长度*/
#define DE_DOWN_SERIAL_RX_TIMEOUT   2

typedef struct uart1_rbuf_st
{
	unsigned int in;							//输入
	unsigned int out;							//输出
	unsigned char  buf [ONE_DATA_MAXLEN];		//缓冲区空间
}UART1_RBUF_ST;

typedef struct __attribute__ ((__packed__))
{
	unsigned char DataFlag;						//数据标记,是否有数据
	unsigned char DataLen;						//数据长度
	unsigned char dat[ONE_DATA_MAXLEN];	    	//数据缓存
	
}Down_RX_type;


extern unsigned char down_serial_rx_timeout;
extern Down_RX_type down_rx_buff;





int down_usart_init(void);
void down_data_parsing(void);
void down_USART_ClearBuf_Flag(void);
uint16_t down_USART_GetChar(void);
uint16_t down_USART_GetCharBlock(uint16_t timeout);
rt_err_t down_uart_rx_ind(rt_device_t dev, rt_size_t size);

#endif