#include "rsucmanager.h"


/* 定时器的控制块*/
static struct rt_timer timer1;
static struct rt_timer timer2;

/* 定时器1 超时函数,每2个OS节拍中断一次*/
void timeout1(void *parameter)
{
    if(down_serial_rx_timeout) 
    {
        down_serial_rx_timeout--;
    }
	else
	{
		if(down_rx_buff.DataLen && !down_rx_buff.DataFlag)
        {
            rt_sem_release(&down_rx_sem);
        }
	 }
}


/* 定时器2 超时函数,每10个OS节拍中断一次*/
static unsigned int timeout2_count;
void timeout2(void *parameter)
{
    timeout2_count++;
    if(timeout2_count==130)
    {
        LED1_ON();
    }
    else if(timeout2_count>=135)
    {
        timeout2_count=0;
        LED1_OFF();
    }
}


int timer_sample(void)
{
    /* 初始化定时器1 周期定时器*/
    rt_timer_init(  &timer1,"timer1",             /* 定时器名字是timer1 */
                    timeout1,                   /* 超时时回调的处理函数*/ 
                    RT_NULL,                    /* 超时函数的入口参数*/
                    2,                          /* 定时长度， 以OS Tick 为单位， 即2 个OS Tick */
                    RT_TIMER_FLAG_PERIODIC);    /* 周期性定时器*/
    
    /* 初始化定时器2 周期定时器*/
    rt_timer_init(  &timer2,"timer2",             /* 定时器名字是timer1 */
                    timeout2,                   /* 超时时回调的处理函数*/ 
                    RT_NULL,                    /* 超时函数的入口参数*/
                    10,                         /* 定时长度， 以OS Tick 为单位， 即10 个OS Tick */
                    RT_TIMER_FLAG_PERIODIC);    /* 周期性定时器*/

    
    /* 启动定时器1、2 */
    rt_timer_start(&timer1);
    rt_timer_start(&timer2);
    return 0;
}





