#ifndef _RSUCMANAGER_H_
#define _RSUCMANAGER_H_

#include <string.h>
#include <ulog.h>
#include "board.h"
#include "rtthread.h"
#include "common.h"
#include "rsuc_pro.h"
#include "downstream.h"
#include "rsuc_timer.h"
#include "storage.h"
#include "sub_sample.h"
#include "rsuc_timer.h"

//#define RSUC_DEBUG

//#define sprs_led_debug

//注:将CPNAME替换为实际mapp名，形如UTCM STOM

#define RSUC_VERSION    0x400

#define RSUC_CPID  (8)

#define RSUC_PIPE_NMSG  (20)
#define RSUC_POOL_SIZE  (sizeof(GMS_STRU)*RSUC_PIPE_NMSG)
#define RSUC_REG_SEM_WAIT_TIME  (1000)

#define RSUC_SAMPLE_WAIT_TIME  (10*1000)

/* 定义结构体，组件消息接收线程与任务处理线程通信 */
typedef struct __attribute__ ((__packed__))
{
	uint8_t  d_src;							//消息源
	uint8_t  d_len;                         //数据长度
    uint8_t  dat[16];						//每个消息最长16字节
}rsuc_inside_dat_type;

// /* 定义结构体，组件返回消息 */
// typedef struct __attribute__ ((__packed__))
// {
// 	uint8_t  src;							//消息源
// 	uint8_t	 mq_type;						//指令类型
// 	uint8_t  is_mq_success;					//0成功，错误代码请查看错误规范
// 	uint8_t  d_len;                         //返回的数据长度
// 	uint8_t	 *dp;							//返回的数据指针
// }rsuc_output_dat_type;


//第三层：向源组件返回消息
extern rsuc_output_dat_type rsuc_output_dat;
extern struct rt_semaphore sem_rsuc;
extern RESP_STRU rsuc_resp_data; 
extern uint8_t   rsuc_output_eq_buf[256];


extern CP_CTL_STRU rsuc_ctl; //CPNAME组件控制体
extern CP_REG_STRU rsuc_reg; //用于CPNAME组件向主管道注册




extern DM_GMS_STRU rsuc_tmp_dmgms;
extern GMS_STRU rsuc_gms; //主管道信息,用于解析



extern struct rt_messagequeue rsuc_input_dat_mq;  //定义主线程与数据处理函数的消息队列
extern struct rt_semaphore sem_rsuc_sample_pro; 

extern	uint8_t is_cfg_table_presence; 
extern	uint8_t is_eq_table_presence; 
extern	uint8_t is_in_table_presence; 


#endif