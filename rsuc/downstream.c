#include "rsucmanager.h"

/* 用于接收消息的信号量*/
//struct rt_semaphore down_rx_sem;
rt_device_t down_serial;
// UART2_RBUF_ST uart2_rbuf = {
//     0,
//     0,
// };
Down_RX_type down_rx_buff = {0};
unsigned char down_rx_device_Len = 0;

//unsigned char down_serial_rx_timeout = DE_DOWN_SERIAL_RX_TIMEOUT;

/*******************************************************************************
* Function Name  : CRC16_Check
* Description    : CRC校验
* Input          : None
* Output         : None
* Return         : None
*******************************************************************************/
uint16_t Modbus_CRC16_Check(uint8_t *Pushdata, uint8_t length)
{
    uint16_t Reg_CRC = 0xffff;
    uint8_t i, j;
    for (i = 0; i < length; i++)
    {
        Reg_CRC ^= *Pushdata++;
        for (j = 0; j < 8; j++)
        {
            if (Reg_CRC & 0x0001)

                Reg_CRC = Reg_CRC >> 1 ^ 0xA001;
            else
                Reg_CRC >>= 1;
        }
    }
    return Reg_CRC;
}

void down_uart_send_dat(unsigned char *dat, unsigned short len)
{
    uint8_t i = 0;
    rsuc_RS485_TX();
    rt_kprintf("down_uart_send_dat:\r\n");
    for (i = 0; i < len; i++)
        rt_kprintf("0x%x ", *(dat + i));
    rt_kprintf("\r\n");
    rt_enter_critical();
    rt_device_write(down_serial, 0, dat, len);
    rt_exit_critical();
    rsuc_RS485_RX();
}

/**********************************************************************************
* 串口1接收字符函数，阻塞模式（接收缓冲区中提取）
**********************************************************************************/
// uint16_t down_USART_GetCharBlock(uint16_t timeout)
// {
//     UART2_RBUF_ST *p = &uart2_rbuf;
//     uint16_t to = timeout;
//     while (p->out == p->in)
//     {
//         if (!(--to))
//         {
//             return 0xffff;
//         }
//     }
//     return (p->buf[(p->out++) & (ONE_DATA_MAXLEN - 1)]);
// }

/**********************************************************************************
* 串口1接收字符函数，非阻塞模式（接收缓冲区中提取）
**********************************************************************************/
// uint16_t down_USART_GetChar(void)
// {
//     UART2_RBUF_ST *p = &uart2_rbuf;
//     if (p->out == p->in) //缓冲区空条件
//     {
//         return 0xffff;
//     }
//     return down_USART_GetCharBlock(1000);
// }

/**********************************************************************************
清除串口1接收标志位，按帧进行
**********************************************************************************/
// void down_USART_ClearBuf_Flag(void)
// {
//     UART2_RBUF_ST *p = &uart2_rbuf;
//     p->out = 0;
//     p->in = 0;

//     down_rx_buff.DataFlag = 0;
//     down_rx_buff.DataLen = 0;
// }

/* 接收数据回调函数*/
// rt_err_t down_uart_rx_ind(rt_device_t dev, rt_size_t size)
// {
//     uint8_t ch = 0;
//     UART2_RBUF_ST *p = &uart2_rbuf;

//     if (size > 0)
//     {
//         rt_device_read(down_serial, 0, &ch, 1); //获取数据

//         if (!down_rx_buff.DataFlag)
//         {
//             if ((((p->out - p->in) & (ONE_DATA_MAXLEN - 1)) == 0) || down_serial_rx_timeout)
//             {
//                 down_serial_rx_timeout = DE_DOWN_SERIAL_RX_TIMEOUT;
//                 if ((p->in - p->out) < ONE_DATA_MAXLEN)
//                 {
//                     p->buf[p->in & (ONE_DATA_MAXLEN - 1)] = ch;
//                     p->in++;
//                 }
//                 down_rx_buff.DataLen = (p->in - p->out) & (ONE_DATA_MAXLEN - 1);
//             }
//         }
//     }
//     return RT_EOK;
// }

/**************************************************
 * 
 * 创建串口数据接收线程，高优先级
 * 静态创建
 * ***********************************************/
// void down_data_parsing_thread_entry(void *p)
// {
//     int8_t ch;
//     uint8_t i = 0;
//     //unsigned int rx_count=0;

//     LOG_D("down_data_parsing_thread_entry startup");
//     while (1)
//     {
//         if (!down_rx_buff.DataFlag) //帧结束标志位为0，则等待完成信号量
//         {
//             //LOG_D("wait down_rx_sem");
//             rt_sem_take(&down_rx_sem, RT_WAITING_FOREVER); //获取信号量
//             down_rx_buff.DataFlag = 1;
//             //LOG_D("get down_rx_sem");
//         }
//         if (down_rx_buff.DataFlag)
//         {
//             if (down_rx_buff.DataLen != 0)
//             {
//                 rt_memset(&down_rx_buff.dat[0], 0, ONE_DATA_MAXLEN);
//                 for (i = 0; i < down_rx_buff.DataLen; i++)
//                 {
//                     down_rx_buff.dat[i] = down_USART_GetChar();
//                 }

//                 // for(i=0;i<down_rx_buff.DataLen;i++)
//                 // {
//                 //     rt_kprintf("0x%02x ", down_rx_buff.dat[i]);
//                 // }
//                 // rt_kprintf("rx_len:%d\r\n",down_rx_buff.DataLen);
//                 down_rx_device_Len = down_rx_buff.DataLen;

//                 //LOG_D("1111111111rx_len:%d,%d",down_rx_buff.DataLen,down_rx_device_Len);

//                 rt_sem_release(&down_frame_sem); //处理完成后发送信号量
//             }
//             down_USART_ClearBuf_Flag();
//             //rt_thread_mdelay(5);
//         }
//     }
// }




/* 接收数据回调函数*/
rt_err_t down_uart_input(rt_device_t dev, rt_size_t size)
{
    down_rx_msg msg;
    rt_err_t result;
    msg.dev = dev;
    msg.size = size;

    result = rt_mq_send(&down_serial_rx_mq, &msg, sizeof(msg));
    if (result == -RT_EFULL)
    {
        /* 消息队列满*/
        LOG_D("message queue full !");
    }
    return result;
}

void serial_thread_entry(void *parameter)
{
    down_rx_msg msg;
    rt_err_t result;
    rt_uint32_t rx_length;
    //static char rx_buffer[RT_SERIAL_RB_BUFSZ + 1];

    unsigned char i=0;
    while (1)
    {
        rt_memset(&msg, 0, sizeof(msg));
        /* 从消息队列中读取消息*/
        result = rt_mq_recv(&down_serial_rx_mq, &msg, sizeof(msg), RT_WAITING_FOREVER);
        
        //LOG_D("messag1111111111    result:%d RT_EOK:%d",result,RT_EOK);

        if (result == RT_EOK)
        {
            /* 从串口读取数据*/
            //rt_memset(&down_rx_buff.dat[0], 0, ONE_DATA_MAXLEN);
            rx_length = rt_device_read(msg.dev, 0, &down_rx_buff.dat[0], msg.size);

            down_rx_buff.DataLen=rx_length;
            down_rx_device_Len = rx_length;
            
            rt_sem_release(&down_frame_sem); //处理完成后发送信号量

            

            // for(i=0;i<rx_length;i++)
            // {
            //     rt_kprintf("0x%x ",rx_buffer[i]);
            // }
            // rt_kprintf("\r\n");

            
        }
    }
}