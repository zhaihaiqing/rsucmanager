#include "rsucmanager.h"

#define LOG_TAG     "SPRS_MANA"    
#define LOG_LVL     LOG_LVL_DBG
#include <ulog.h>

//===========================================================
static struct rt_messagequeue rsuc_pipe; //CPNAME组件管道
unsigned char rsuc_msg_pool[RSUC_POOL_SIZE]={0}; //CPNAME组件管道缓冲
//===========================================================
CP_CTL_STRU rsuc_ctl={0,RT_NULL,RSUC_CPID,0}; //CPNAME组件控制体
CP_REG_STRU rsuc_reg={RT_NULL,RSUC_CPID}; //用于CPNAME组件向主管道注册

struct rt_thread rthread_msg_pro; //CPNAME消息处理进程结构体
unsigned char thread_msg_pro_stack[3072];
struct rt_thread rthread_register; //CPNAME组件注册进程结构体
unsigned char thread_register_stack[THREAD_STACK_SIZE];

DL_FUNCS_STRU funcs;

uint8_t is_eq_table_presence=0;
uint8_t is_in_table_presence=0;



//第一层：RSUC接收源组件发送的消息


//第二层：消息线程向任务线程发送消息队列
struct rt_messagequeue rsuc_input_dat_mq;  //定义主线程与数据处理函数的消息队列
rt_uint8_t rsuc_input_dat_mq_pool[512];
//struct rt_semaphore sem_rsuc_sample_pro;    //rsuc采样务处理信号量,主任务与采样任务确认信号量


//第三层：向源组件返回消息
rsuc_output_dat_type rsuc_output_dat;   //要发送数据的结构体
struct rt_semaphore sem_rsuc;           //发送消息时的信号量，用于判断源组件是否接收到消息
RESP_STRU rsuc_resp_data;               //发送消息时返回的结果
uint8_t   rsuc_output_eq_buf[256]={0};  //传感器返回的数据存在在该全局变量中，rsuc_output_dat中的数据指针指向该buf
                                        //该buf最长256字节



/********************************************************************
*
*   RSUC线程，在该进程中完成主要业务功能
*
********************************************************************/

void RSUC_msg_pro_entry(void *p) //CPNAME组件消息处理进程
{
    //DM_GMS_STRU rsuc_dat_dmgms;//主管道多维消息,发送数据用
    GMS_STRU rsuc_gms;
    uint8_t i=0;
    static rsuc_inside_dat_type rsuc_input_dat={0};

    rt_thread_mdelay(100);

    //初始化硬件、进行相关任务建立
    LOG_D("msg pro thread start");
    LOG_D("Hello world,this is a test info!");


    rt_mq_init(&rsuc_input_dat_mq,                 /* 创建消息队列，组件消息接收线程与任务处理线程通信 */
                "rsuc_input_dat_mq",
                &rsuc_input_dat_mq_pool,           /* 内存池指向 msg_pool */
                18,                                /* 每个消息的大小是 18 字节 */
                sizeof(rsuc_input_dat_mq_pool),    /* 内存池的大小是 msg_pool 的大小 */
                RT_IPC_FLAG_FIFO);                  /* 如果有多个线程等待，按照先来先得到的方法分配消息 */


    //硬件初始化
    rsuc_GPIO_init();               //初始化GPIO
    down_usart_init();              //初始化串口
    timer_sample();                 //初始化软件定时器



    // RSUC_Eq_manag_writeread2();
    //Init_in_CFG();




    is_eq_table_presence=Check_eq_CFG();     //检查设备表，正常=1，异常=0；
    is_in_table_presence=Check_in_CFG();     //检查指令表，正常=1，异常=0；


    while(1)
    {
        if(rt_mq_recv(&rsuc_pipe,&rsuc_gms,sizeof(GMS_STRU),RT_WAITING_FOREVER)==RT_EOK) //从cpname_pipe获取消息，等待模式
        {
            LOG_D("SPRS:msg recved");

            if(rsuc_gms.d_cmd.is_src_cmd == 1)//使用源的指令解析
            {
                mb_resp_msg(&rsuc_gms,MB_STATN_RSUC,0);  //释放信号量并返回结果
            }
            else    //使用目标的指令解析
            {
                rt_memset(&rsuc_input_dat,0,sizeof(rsuc_input_dat));    //清空buf
                //##rt_sem_init(&sem_rsuc_sample_pro,"sem_rsuc_sample_pro",0,RT_IPC_FLAG_FIFO);//第二层：初始化任务线程信号量

                rsuc_input_dat.d_src=rsuc_gms.d_src;                //获取消息源组件号
                rsuc_input_dat.d_len=rsuc_gms.d_dl;                 //获取数据（纯数据区）长度
                rsuc_input_dat.dat[0]=rsuc_gms.d_cmd.cmd;           //获取指令码
                rt_memcpy(&rsuc_input_dat.dat[1],(uint8_t *)rsuc_gms.d_p+1,rsuc_input_dat.d_len-1);//获取数据
                
                LOG_D("SPRS:d_src:%d,d_len:%d,mq_type:%d,dat[1]:%d,dat[2]:%d,dat[3]:%d",rsuc_input_dat.d_src,rsuc_input_dat.d_len,rsuc_input_dat.dat[0],rsuc_input_dat.dat[1],rsuc_input_dat.dat[2],rsuc_input_dat.dat[3]);

                rt_mq_send(&rsuc_input_dat_mq, &rsuc_input_dat, sizeof(rsuc_input_dat)); //第二层：向任务处理线程发送消息队列

                mb_resp_msg(&rsuc_gms,MB_STATN_RSUC,0);  //第一层：数据copy并向任务线程发送完成后，释放信号量，通知源组件
/*
                if(RT_EOK==rt_sem_take(&sem_rsuc_sample_pro,RSUC_SAMPLE_WAIT_TIME))//等待任务任务线程释放信号量，保证任务线程接收到消息
                {
                     LOG_D("send single sample suc!");  
                }
                else 
                {
                     LOG_E("send single sample err!");  
                }
                rt_sem_detach(&sem_rsuc_sample_pro);//脱离信号量
                */
            }
        }
    }
}








/*
RSUC注册线程，向主管道中注册
*/
struct rt_semaphore rsuc_sem_reg;
void RSUC_register_entry(void *p)
{
 unsigned char i=0; 
 static DM_GMS_STRU tmp_dmgms;
 GMS_STRU tmp_gms;
 RESP_STRU resp_reg;

 {
  rsuc_reg.p_pipe=&rsuc_pipe;
  
  { 
   rt_sem_init(&rsuc_sem_reg,"rsuc_reg",0,RT_IPC_FLAG_FIFO); //初始化用于消息同步的信号量，接受者在完成消息处理后，必要时需要对信号量进行释放

   rt_memset(&tmp_dmgms,0,sizeof(DM_GMS_STRU)); //清空多维消息体
   mb_make_dmgms(&tmp_dmgms,0,&rsuc_sem_reg,CP_CMD_DST(MAINPIPE_CMD_CPREG),MB_STATN_MAIN,MB_STATN_RSUC,&rsuc_reg,sizeof(CP_REG_STRU),&resp_reg); //向多维消息体中装入消息

   rt_mq_send(rsuc_ctl.p_mainpipe,&tmp_dmgms,sizeof(DM_GMS_STRU));//向主管道发送注册命令

   if(RT_EOK!=rt_sem_take(&rsuc_sem_reg,RSUC_REG_SEM_WAIT_TIME)) //等待接收者释放信号量，超时将造成组件注册失败
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
