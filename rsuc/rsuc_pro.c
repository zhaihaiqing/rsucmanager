#include "rtthread.h"
#include "rsuc_pro.h"
#include "board.h"


#define LOG_TAG     "RSUC_PRO"    
#define LOG_LVL     LOG_LVL_DBG
#include <ulog.h>

//此文件主要是组件的具体内部实现
//mapp_template中的消息数据处理尽可能简短(防止msgpro进程长时间阻塞)
//将消息对应的具体工作抛出到单独的进程中进行处理

void  testprintf(void)
{
    rt_kprintf("hello world\r\n");
}

#ifndef LED1_PIN_NUM
    #define LED1_PIN_NUM  GET_PIN(A, 13)    //PA13
#endif 


void LED1_ON(void *args)
{
    //rt_kprintf("turn on LED1!\n");
    rt_pin_write(LED1_PIN_NUM,PIN_LOW);
} 

void LED1_OFF(void *args)
{
    //rt_kprintf("turn on LED1!\n");
    rt_pin_write(LED1_PIN_NUM,PIN_HIGH);
} 

void pin_led1_init(void)
{
    rt_pin_mode(LED1_PIN_NUM,PIN_MODE_OUTPUT);
    rt_pin_write(LED1_PIN_NUM,PIN_HIGH);

    
    //LED1_ON();
}

//MSH_CMD_EXPORT(pin_led1_sample,pin led1 sample)









