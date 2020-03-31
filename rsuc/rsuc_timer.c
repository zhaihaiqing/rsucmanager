#include "rtthread.h"
#include "rsuc_pro.h"
#include "board.h"
#include <ulog.h>
#include "downstream.h"

/* 定时器的控制块*/
rt_timer_t timer1;
unsigned int cnt = 0;

/* 定时器1 超时函数,每2个OS节拍中断一次*/
void timeout1(void *parameter)
{
    if(down_serial_rx_timeout) down_serial_rx_timeout--;
	else
	{
		if(down_rx_buff.DataLen && !down_rx_buff.DataFlag)
	 	down_rx_buff.DataFlag = 1;
	 }

    // rt_kprintf("periodic timer is timeout %d\n", cnt);
    // /* 运行第10 次， 停止周期定时器*/
    // if (cnt++>= 9)
    // {
    //     rt_timer_stop(timer1);
    //     rt_kprintf("periodic timer was stopped! \n");
    // }
}

int timer_sample(void)
{
    /* 创建定时器1 周期定时器*/
    timer1 = rt_timer_create("timer1",                      /* 定时器名字是timer1 */
                                timeout1,                   /* 超时时回调的处理函数*/ 
                                RT_NULL,                    /* 超时函数的入口参数*/
                                2,                        /* 定时长度， 以OS Tick 为单位， 即10 个OS Tick */
                                RT_TIMER_FLAG_PERIODIC);    /* 周期性定时器*/
    /* 启动定时器1 */
    if (timer1 != RT_NULL) rt_timer_start(timer1);
    return 0;
}





