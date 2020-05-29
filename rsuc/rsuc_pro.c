#include "rsucmanager.h"

#define LOG_TAG "SPRS_PRO"
#define LOG_LVL LOG_LVL_DBG
#include <ulog.h>

//此文件主要是组件的具体内部实现
//完成硬件初始化、系统功能初始化（软件定时器除外）

/*******    串口接收线程    ******/
struct rt_thread rthread_downstream;
unsigned char rthread_downstream_stack[1024];
unsigned char downstream_THREAD_PRIORITY = 20;

/*******    总线设备访问线程    ******/
//1：等待业务组件发送的消息，对消息进行处理，访问下层设备
//2：等待串口接收线程的信号量，解析数据，发送给业务组件
struct rt_thread rthread_busdevice_access;
unsigned char rthread_busdevice_access_stack[3072];
unsigned char busdevice_access_THREAD_PRIORITY = 21;

struct rt_semaphore down_rx_sem;
struct rt_semaphore down_frame_sem;

sprs_uart_cfg_type  sprs_uart_cfg;

//向主管道发送消息
int Rsuc_Send_Msg(DM_GMS_STRU *dat)
{
    rt_sem_init(&sem_rsuc, "sem_rsuc", 0, RT_IPC_FLAG_FIFO); //初始化主任务信号量

    if (!mb_send_msg(rsuc_ctl.p_mainpipe, dat, sizeof(DM_GMS_STRU))) //向主管道发送配置数据
    {
        if (RT_EOK == rt_sem_take(&sem_rsuc, RSUC_REG_SEM_WAIT_TIME))
        {
            rt_sem_detach(&sem_rsuc); //脱离信号量
            //LOG_D("dm_gms:%d,r_src:%d,r_res:%d", dat->dm_gms[0].d_des, rsuc_resp_data.r_src, rsuc_resp_data.r_res);
            if (dat->dm_gms[0].d_des == rsuc_resp_data.r_src && 0 == rsuc_resp_data.r_res)
                return RT_EOK;
            else
                return RT_ERROR;
        }
        else
        {
            rt_sem_detach(&sem_rsuc); //脱离信号量
            return RT_ERROR;
        }
    }
}

int down_usart_init(void)
{
    rt_err_t ret = RT_EOK;
    char uart_name[RT_NAME_MAX];
    char str[] = "hello RT_Thread!\r\n";
    struct serial_configure config = RT_SERIAL_CONFIG_DEFAULT;

    down_USART_ClearBuf_Flag();
    rt_strncpy(uart_name, DOWNSTREAM_UART_NAME, RT_NAME_MAX);

    /* step1：查找串口设备 */
    down_serial = rt_device_find(uart_name);
    if (!down_serial)
    {
        LOG_D("find %s failed!", uart_name);
        return RT_ERROR;
    }

    /* step2：修改串口配置参数 */
    config.baud_rate = BAUD_RATE_9600; //修改波特率为 9600
    config.data_bits = DATA_BITS_8;    //数据位 8
    config.stop_bits = STOP_BITS_1;    //停止位 1
    config.bufsz = 256;                //修改缓冲区 buff size 为 128
    config.parity = PARITY_NONE;       //无奇偶校验位

    /* step3：控制串口设备。通过控制接口传入命令控制字，与控制参数 */
    rt_device_control(down_serial, RT_DEVICE_CTRL_CONFIG, &config);

    /*以中断接收及轮询发送模式打开串口设备*/
    rt_device_open(down_serial, RT_DEVICE_FLAG_INT_RX);

    //初始化信号量
    rt_sem_init(&down_rx_sem, "down_rx_sem", 0, RT_IPC_FLAG_FIFO);
    rt_sem_init(&down_frame_sem, "down_frame_sem", 0, RT_IPC_FLAG_FIFO); //创建信号量

    /*设置回调函数*/
    rt_device_set_rx_indicate(down_serial, down_uart_rx_ind);

    /*发送测试字符*/
    //down_uart_send_dat(str, (sizeof(str) - 1));

    //创建串口数据接收线程
    rt_thread_init(&rthread_downstream,
                   "rthread_downstream",
                   down_data_parsing_thread_entry,
                   RT_NULL,
                   rthread_downstream_stack,
                   sizeof(rthread_downstream_stack),
                   downstream_THREAD_PRIORITY,
                   THREAD_TIMESLICE);
    rt_thread_init(&rthread_busdevice_access,
                   "rthread_busdevice_access",
                   rsuc_eq_access_thread_entry,
                   RT_NULL,
                   rthread_busdevice_access_stack,
                   sizeof(rthread_busdevice_access_stack),
                   busdevice_access_THREAD_PRIORITY,
                   THREAD_TIMESLICE);

    rt_thread_startup(&rthread_downstream);       //启动串口接收线程
    rt_thread_startup(&rthread_busdevice_access); //启动总线设备访问线程

    return ret;
}

void rsuc_GPIO_init(void)
{
#ifdef  sprs_led_debug
    rt_pin_mode(LED1_PIN_NUM, PIN_MODE_OUTPUT);      //初始化LED灯
#endif
    rt_pin_mode(M_EN_PIN_NUM, PIN_MODE_OUTPUT);      //初始化模块电源
    rt_pin_mode(RS485_TRX_PIN_NUM, PIN_MODE_OUTPUT); //初始化RS485方向控制引脚
#ifdef  sprs_led_debug
    LED1_OFF();
#endif
    rsuc_MPOW_EN_ON();
    rsuc_RS485_RX();
}

/******创建总线设备访问线程*********/
//根据设备类型调用不同的索引函数

int rsuc_eq_access_thread_entry(void *p)
{
    int err = 0;
    uint8_t temp[8] = {0};
    uint8_t i=0;
    rsuc_inside_dat_type rsuc_dat_buf = {0};
    DM_GMS_STRU rsuc_dat_dmgms;
    GMS_STRU rsuc_gms; //主管道信息,用于解析

    LOG_D("sprs_eq_access_thread_entry startup");
    //#ifdef RSUC_DEBUG

    // rsuc_sub_sample(9,10,1,&temp,1);//查询配置-1
    // rt_thread_mdelay(1000);
    // rsuc_sub_sample(9,12,1,&temp,1);//查询配置-2
    // rt_thread_mdelay(1000);

    // rt_thread_mdelay(3000);
    // LOG_D("rsuc_eq_access_thread_entry");

    // temp[0]=0x01;
    // temp[1]=0x04;
    // temp[2]=0x00;
    // temp[3]=0x10;
    // temp[4]=0x00;
    // temp[5]=0x13;
    // temp[6]=0xb0;
    // temp[7]=0x02;
    // down_uart_send_dat(&temp[0],8);
    // rt_thread_mdelay(2000);
    // down_uart_send_dat(&temp[0],8);
    // rt_thread_mdelay(2000);

    // rsuc_sub_sample(253,11,1,&temp,1);//获取数据
    // rt_thread_mdelay(2000);
    // LOG_D("1111111111111111111111111111");
    // rsuc_sub_sample(253,11,1,&temp,1);//获取数据
    // rt_thread_mdelay(2000);

    //#endif

    while (1)
    {
        rt_memset(&rsuc_dat_buf, 0, sizeof(rsuc_dat_buf));                                                     //清零缓冲区
        if (rt_mq_recv(&rsuc_input_dat_mq, &rsuc_dat_buf, sizeof(rsuc_dat_buf), RT_WAITING_FOREVER) == RT_EOK) //等待消息队列
        {
            //LOG_D("rt_mq_recv");

            // LOG_D("rsuc_dat_buf.d_src：%d", rsuc_dat_buf.d_src);
            // LOG_D("rsuc_dat_buf.d_len：%d", rsuc_dat_buf.d_len);
            // LOG_D("rsuc_dat_buf.dat[0]：%d", rsuc_dat_buf.dat[0]);
            // LOG_D("rsuc_dat_buf.dat[1]：%d", rsuc_dat_buf.dat[1]);
            // LOG_D("rsuc_dat_buf.dat[2]：%d", rsuc_dat_buf.dat[2]);
            // LOG_D("rsuc_dat_buf.dat[3]：%d", rsuc_dat_buf.dat[3]);
            // LOG_D("rsuc_dat_buf.dat[4]：%d", rsuc_dat_buf.dat[4]);
            // LOG_D("rsuc_dat_buf.dat[5]：%d", rsuc_dat_buf.dat[5]);
            // LOG_D("rsuc_dat_buf.dat[6]：%d", rsuc_dat_buf.dat[6]);
            // LOG_D("rsuc_dat_buf.dat[7]：%d", rsuc_dat_buf.dat[7]);
            // LOG_D("rsuc_dat_buf.dat[8]：%d", rsuc_dat_buf.dat[8]);
            // LOG_D("rsuc_dat_buf.dat[9]：%d", rsuc_dat_buf.dat[9]);
            // LOG_D("rsuc_dat_buf.dat[10]：%d", rsuc_dat_buf.dat[10]);
            // LOG_D("rsuc_dat_buf.dat[11]：%d", rsuc_dat_buf.dat[11]);
            // LOG_D("rsuc_dat_buf.dat[12]：%d", rsuc_dat_buf.dat[12]);

            

            if ((rsuc_dat_buf.dat[0] >= 1) && (rsuc_dat_buf.dat[0] <= 4))
            {
                err = manager_eq(rsuc_dat_buf.d_src, rsuc_dat_buf.dat[0], rsuc_dat_buf.dat[2], rsuc_dat_buf.dat[3], rsuc_dat_buf.dat[4], rsuc_dat_buf.dat[5]); //如果成功，在函数内部直接向源组件发送消息，如果失败，则返回错误代码
            }
            else if ((rsuc_dat_buf.dat[0] >= 9) && (rsuc_dat_buf.dat[0] <= 13))
            {
                err = rsuc_sub_sample(rsuc_dat_buf.d_src, rsuc_dat_buf.dat[0], rsuc_dat_buf.dat[2], &rsuc_dat_buf.dat[2], rsuc_dat_buf.d_len - 2); //如果成功，在函数内部直接向源组件发送消息，如果失败，则返回错误代码
            }
            else
            {
                err = 0xf1;
            }
        }
    }
}
