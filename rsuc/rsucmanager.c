#include "rtthread.h"
#include "common.h"
#include "rsucmanager.h"
#include "rsuc_pro.h"
#include "downstream.h"
#include "rsuc_timer.h"
#include "storage.h"

#define LOG_TAG     "MB_CPNAME"    
#define LOG_LVL     LOG_LVL_DBG
#include <ulog.h>

//===========================================================
static struct rt_messagequeue rsuc_pipe; //CPNAME组件管道
unsigned char rsuc_msg_pool[RSUC_POOL_SIZE]={0}; //CPNAME组件管道缓冲
//===========================================================
CP_CTL_STRU rsuc_ctl={0,RT_NULL,RSUC_CPID,0}; //CPNAME组件控制体
CP_REG_STRU rsuc_reg={RT_NULL,RSUC_CPID}; //用于CPNAME组件向主管道注册

struct rt_thread rthread_msg_pro; //CPNAME消息处理进程结构体
unsigned char thread_msg_pro_stack[THREAD_STACK_SIZE];
struct rt_thread rthread_register; //CPNAME组件注册进程结构体
unsigned char thread_register_stack[THREAD_STACK_SIZE];

DL_FUNCS_STRU funcs;

/*
RSUC业务进程，在该进程中完成主要业务功能
*/
void RSUC_msg_pro_entry(void *p) //CPNAME组件消息处理进程
{
 GMS_STRU tmp_gms;

//进行相关任务建立

 {
  
 }

     testprintf();testprintf();testprintf();
     testprintf();testprintf();testprintf();
     LOG_D("msg pro thread start");
     LOG_D("this is a test info! Hello world");
     LOG_D("this is a test info! Hello world");
     LOG_D("this is a test info! Hello world");
     LOG_D("this is a test info! Hello world");

pin_led1_init();
//初始化串口

down_usart_init();
timer_sample();

RSUC_Eq_manag_writeread2();

 while(1)
 {
  // if(rt_mq_recv(&rsuc_pipe,&tmp_gms,sizeof(GMS_STRU),RT_WAITING_FOREVER)==RT_EOK) //从cpname_pipe获取消息，等待模式
  // {
      
  //  LOG_D("msg recved");
  // }


  LED1_OFF();
  rt_thread_mdelay(450);
  LED1_ON();
  rt_thread_mdelay(50);

 }
}
/*
RSUC注册线程，向主管道中注册
*/
struct rt_semaphore sem_reg;
void RSUC_register_entry(void *p)
{
 unsigned char i=0; 
 static DM_GMS_STRU tmp_dmgms;
 GMS_STRU tmp_gms;
 RESP_STRU resp_reg;

 {
  rsuc_reg.p_pipe=&rsuc_pipe;
  
  { 
   rt_sem_init(&sem_reg,"rsuc_reg",0,RT_IPC_FLAG_FIFO); //初始化用于消息同步的信号量，接受者在完成消息处理后，必要时需要对信号量进行释放

   rt_memset(&tmp_dmgms,0,sizeof(DM_GMS_STRU)); //清空多维消息体
   mb_make_dmgms(&tmp_dmgms,0,&sem_reg,CP_CMD_DST(MAINPIPE_CMD_CPREG),MB_STATN_MAIN,MB_STATN_CP_NAME,&rsuc_reg,sizeof(CP_REG_STRU),&resp_reg); //向多维消息体中装入消息

   rt_mq_send(rsuc_ctl.p_mainpipe,&tmp_dmgms,sizeof(DM_GMS_STRU));//向主管道发送注册命令

   if(RT_EOK!=rt_sem_take(&sem_reg,RSUC_REG_SEM_WAIT_TIME)) //等待接收者释放信号量，超时将造成组件注册失败
   {
    LOG_E("cp-reg ot!");
    return ;
   } 

   if(MB_STATN_MAIN==resp_reg.r_src && MB_RESP_SUC==resp_reg.r_res) //注册成功
   {
    rsuc_ctl.is_registered=1;
    LOG_D("cp-reg suc!");
   }
   else
   {
    LOG_E("cp-reg fail!");
    return ;
   } 
  }
 }

 {
  if(rt_mq_recv(&rsuc_pipe,&tmp_gms,sizeof(GMS_STRU),RT_WAITING_FOREVER)==RT_EOK) //从CPNAME组件管道获取消息
  {
   if(tmp_gms.d_cmd.is_src_cmd) //是源头命令
   {
    if(MB_STATN_MAIN==tmp_gms.d_src) //来自主管道
    {
     if(MAINPIPE_CMD_STARTUP==tmp_gms.d_cmd.cmd) //业务启动命令
     {
      rt_thread_init(&rthread_msg_pro, "rsuc_msg_pro",RSUC_msg_pro_entry,RT_NULL,thread_msg_pro_stack,sizeof(thread_msg_pro_stack),THREAD_PRIORITY,THREAD_TIMESLICE);//创建业务线程
      rt_thread_startup(&rthread_msg_pro);
     } else return ;
    } else return ;
   } else return ;
  }
 }

 return ;
}

void main(void *p) //主入口
{
 rt_err_t res=0;
 GMS_STRU tmp_gms;

 LOG_D("main thread start");

//初始化组件消息队列
 res=rt_mq_init(&rsuc_pipe,"rsuc_pipe",rsuc_msg_pool,sizeof(GMS_STRU),sizeof(rsuc_msg_pool),RT_IPC_FLAG_FIFO); //建立CPNAME管道,等待主管道消息队列
 if(res!=RT_EOK)
 {
  LOG_E("pipe init fail!");
  return;
 }

 {
  rsuc_ctl.p_mainpipe=((CP_ENTRY_ARGS_STRU *)p)->p_mq;
  rsuc_ctl.p_lib=((CP_ENTRY_ARGS_STRU *)p)->p_lib;

  //将动态库中的函数导出
  //这样的作法是为了在函数层面解耦
  {
   //funcs.pfun_=dlsym(comnb_ctl.p_lib,"xxx"); //从动态库(即so中找到名为xxx的符号，即函数名)
  }
 }

 rt_thread_init(&rthread_register, "rsuc_reg",RSUC_register_entry,RT_NULL,thread_register_stack,sizeof(thread_register_stack),THREAD_PRIORITY,THREAD_TIMESLICE);//创建RSUC注册线程
 rt_thread_startup(&rthread_register);  

 return; 
}
