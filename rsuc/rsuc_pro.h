#ifndef _RSUC_PRO_H_
#define _RSUC_PRO_H_
#include "rsucmanager.h"


//#define _ _
#define RSUC_SEM_WAIT_TIME (100)


#ifndef LED1_PIN_NUM
    #define LED1_PIN_NUM  GET_PIN(A, 13)    //PA13
#endif 

#ifndef RS485_TRX_PIN_NUM
    #define RS485_TRX_PIN_NUM  GET_PIN(A, 1)    //PA1
#endif 

#ifndef M_EN_PIN_NUM
    #define M_EN_PIN_NUM  GET_PIN(C, 4)    //PC4
#endif 


#define LED1_ON()           rt_pin_write(LED1_PIN_NUM,PIN_LOW)
#define LED1_OFF()          rt_pin_write(LED1_PIN_NUM,PIN_HIGH)
#define rsuc_MPOW_EN_ON()   rt_pin_write(M_EN_PIN_NUM,PIN_LOW)
#define rsuc_MPOW_EN_OFF()  rt_pin_write(M_EN_PIN_NUM,PIN_HIGH)
#define rsuc_RS485_RX()     rt_pin_write(RS485_TRX_PIN_NUM,PIN_LOW)
#define rsuc_RS485_TX()     rt_pin_write(RS485_TRX_PIN_NUM,PIN_HIGH)


typedef struct
{
 //动态库函数指针
 //void *(pfun_)(void); 
 //将动态库中的函数逐一装入到这些函数指针中，以备使用
} DL_FUNCS_STRU;


/**********    定义信号量     ************/
extern struct rt_semaphore down_rx_sem;		/* 串口接收字节信号量*/
extern struct rt_semaphore down_frame_sem;		/* 串口帧信号量*/


extern uint8_t is_eq_table_presence;
extern uint8_t is_in_table_presence;


int down_usart_init(void);
void rsuc_GPIO_init(void);

int rsuc_eq_mang_thread_entry(void *p);
int rsuc_eq_access_thread_entry(void *p);
int Rsuc_Send_Msg(DM_GMS_STRU *dat);

#endif