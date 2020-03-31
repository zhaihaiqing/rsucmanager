#include <string.h>
#include "rtthread.h"
#include "rsuc_pro.h"
#include "board.h"
#include <ulog.h>
#include "downstream.h"
#include "rsuc_timer.h"


/* 用于接收消息的信号量*/
//struct rt_semaphore down_rx_sem;
rt_device_t down_serial;
UART1_RBUF_ST	uart2_rbuf	=	{ 0, 0, };
Down_RX_type down_rx_buff;

unsigned char down_serial_rx_timeout=DE_DOWN_SERIAL_RX_TIMEOUT;




/* 接收数据回调函数*/
rt_err_t down_uart_rx_ind(rt_device_t dev, rt_size_t size)
{
    uint8_t ch=0;
    UART1_RBUF_ST *p = &uart2_rbuf;

    rt_device_read(down_serial, 0, &ch, 1);//获取数据

    if(!down_rx_buff.DataFlag)
    {
        if((((p->out - p->in) & (ONE_DATA_MAXLEN - 1)) == 0) || down_serial_rx_timeout)
        {
            down_serial_rx_timeout=DE_DOWN_SERIAL_RX_TIMEOUT;
            if((p->in - p->out)<ONE_DATA_MAXLEN)
            {
                p->buf [p->in & (ONE_DATA_MAXLEN-1)] = ch;	
				p->in++;
            }
            down_rx_buff.DataLen  = (p->in - p->out) & (ONE_DATA_MAXLEN - 1);
        }
    }
    return RT_EOK;
}


/**********************************************************************************
* 串口1接收字符函数，阻塞模式（接收缓冲区中提取）
**********************************************************************************/
uint16_t down_USART_GetCharBlock(uint16_t timeout)
{
	UART1_RBUF_ST *p = &uart2_rbuf;
	uint16_t to = timeout;	
	while(p->out == p->in)if(!(--to))return 0xffff;
	return (p->buf [(p->out++) & (ONE_DATA_MAXLEN - 1)]);
}

/**********************************************************************************
* 串口1接收字符函数，非阻塞模式（接收缓冲区中提取）
**********************************************************************************/
uint16_t down_USART_GetChar(void)
{
	UART1_RBUF_ST *p = &uart2_rbuf;			
	if(p->out == p->in) //缓冲区空条件
		return 0xffff;
	return down_USART_GetCharBlock(1000);
}

/**********************************************************************************
清除串口1接收标志位，按帧进行
**********************************************************************************/
void down_USART_ClearBuf_Flag(void)
{
	UART1_RBUF_ST *p = &uart2_rbuf;
	p->out = 0;
	p->in = 0;
	
	down_rx_buff.DataFlag = 0;
	down_rx_buff.DataLen = 0;
}

/*up_usart线程处理函数*/
void down_data_parsing(void)
{
    int8_t ch;
    //char data[ONE_DATA_MAXLEN];
    uint8_t i=0;

    while(1)
    {
        if(down_rx_buff.DataFlag)
        {
            //发送接收完成信号量
            rt_kprintf("rx_len:%d\r\n", down_rx_buff.DataLen);

            for(i=0;i<down_rx_buff.DataLen;i++)
                down_rx_buff.dat[i]=down_USART_GetChar();


            for(i=0;i<down_rx_buff.DataLen;i++)
                rt_kprintf("0x%x ", down_rx_buff.dat[i]);
            rt_kprintf("\r\n");
            
            down_USART_ClearBuf_Flag();
            continue;
        }

         rt_thread_mdelay(20);
    }
}


int down_usart_init(void)
{
    rt_err_t ret=RT_EOK;
    char uart_name[RT_NAME_MAX];
    char str[]="hello RT_Thread!\r\n";

    rt_strncpy(uart_name,DOWNSTREAM_UART_NAME,RT_NAME_MAX);
    /*查找系统中的串口设备*/

    down_serial = rt_device_find(uart_name);
    if(!down_serial)
    {
        rt_kprintf("find %s failed!\n",uart_name);
        return RT_ERROR;
    }

    /*初始化信号量*/
    //rt_sem_init(&down_rx_sem,"down_rx_sem",0,RT_IPC_FLAG_FIFO);

    /*以中断接收及轮询发送模式打开串口设备*/
    rt_device_open(down_serial,RT_DEVICE_FLAG_INT_RX);

    /*设置回调函数*/
    rt_device_set_rx_indicate(down_serial,down_uart_rx_ind);

    /*发送字符*/
    rt_device_write(down_serial,0,str,(sizeof(str)-1));

    /*创建线程*/
    rt_thread_t down_usart_thread = rt_thread_create("down_serial",(void (*)(void *parameter))down_data_parsing,RT_NULL,1024,25,10 );

    /*启动创建的线程*/
    if(down_usart_thread!=RT_NULL)
    {
        rt_thread_startup(down_usart_thread);
    }
    else
    {
        ret = RT_ERROR;
    }

    return ret;
}











