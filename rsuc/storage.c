 #include "rtthread.h"
 #include "common.h"
 #include "rsucmanager.h"
 #include "rsuc_pro.h"
 #include "board.h"
 #include <ulog.h>
 #include "rsuc_timer.h"
 #include "storage.h"
 //#include "ff.h"

//#define stsp_CPID (8)


unsigned char storage_buff[128]={0};

eq_manag_type   eq_manag;   //定义所有的总线设备

// CP_CTL_STRU stsp_ctl={0,RT_NULL,stsp_CPID,0}; //stsp组件控制体
// CP_REG_STRU stsp_reg={RT_NULL,stsp_CPID}; //用于stsp组件向主管道注册

static struct rt_semaphore sem_read_cfg;//读文件静态信号量
static struct rt_semaphore sem_write_cfg;//写文件静态信号量

static RESP_STRU stsp_save_data;
static RESP_STRU stsp_read_data;

DM_GMS_STRU dat_dmgms;//主管道多维消息,发送数据用
CP_STOM_RWDATA_STRU tmp_read_data;//读写文件变量结构体




void RSUC_Eq_manag_writeread(void)
{
    unsigned char i=0;
    int fd=0,size=0;

    eq_manag.eq_num=5;

    for(i=0;i<10;i++)
    {
        eq_manag.eq[i].eq_addr=i+1;
    }
    
    //写数据
    LOG_D("write_test\r\n");

    rt_sem_init(&sem_write_cfg,"sem_write_cfg",0,RT_IPC_FLAG_FIFO); //初始化信号量
    strcpy(tmp_read_data.fn,RSUC_Eq_manag_CFG_FN); //文件系统，将配置参数的文件名装入结构体内
    tmp_read_data.fom= O_CREAT | O_WRONLY | O_TRUNC; //文件操作方式 写，没有则创建文件
    tmp_read_data.d_p=&eq_manag; //STOM读取数据向这个指针里放
    tmp_read_data.d_len=128; //要读取的数据长度
    mb_make_dmgms(&dat_dmgms,0,&sem_write_cfg,CP_CMD_DST(STOM_CMD_SAVE),MB_STATN_STOM,MB_STATN_STSP,&tmp_read_data,sizeof(CP_STOM_RWDATA_STRU),&stsp_read_data);
                    //封装消息(主管道多维消息、0、信号量、指令类型、目的组件、源组件、数据指针、数据长度、返回数据指针)
    if(!mb_send_msg(rsuc_ctl.p_mainpipe,&dat_dmgms,sizeof(DM_GMS_STRU)))
    {
        if(RT_EOK==rt_sem_take(&sem_write_cfg,RSUC_CFG_READ_WAIT_TIME))
        {
           if(MB_STATN_STOM==stsp_read_data.r_src && 0==stsp_read_data.r_res)LOG_D("save cfg suc!");  
        }
        else
        {
            LOG_E("save cfg err!");
        }
    }
    rt_sem_detach(&sem_write_cfg);//脱离信号量


    rt_thread_mdelay(1000);


    LOG_D("read_test\r\n");
    rt_sem_init(&sem_read_cfg,"sem_read_cfg",0,RT_IPC_FLAG_FIFO);//初始化读文件信号量
    strcpy(tmp_read_data.fn,RSUC_Eq_manag_CFG_FN); //文件系统，将配置参数的文件名装入结构体内
    tmp_read_data.fom= O_RDONLY; //文件操作方式 读
    tmp_read_data.d_p=storage_buff; //STOM读取数据向这个指针里放
    tmp_read_data.d_len=32; //要读取的数据长度

    mb_make_dmgms(&dat_dmgms,0,&sem_read_cfg,CP_CMD_DST(STOM_CMD_READ),MB_STATN_STOM,MB_STATN_STSP,&tmp_read_data,sizeof(CP_STOM_RWDATA_STRU),&stsp_save_data); 
                //封装消息(主管道多维消息、0、信号量、指令类型、目的组件、源组件、数据指针、数据长度、返回数据指针)
    if(!mb_send_msg(rsuc_ctl.p_mainpipe,&dat_dmgms,sizeof(DM_GMS_STRU)))
    {
        if(RT_EOK==rt_sem_take(&sem_read_cfg,RSUC_CFG_READ_WAIT_TIME))
         {
             for(i=0;i<10;i++)
                LOG_D("%d ",storage_buff[i]);
            if(MB_STATN_STOM==stsp_save_data.r_src && 0==stsp_save_data.r_res) //读取成功
            {
               LOG_D("read cfg suc!");
              
               
            }
            else//失败则写入默认值
            {
               //is_def_cfg=1;
            }
         }
         else
         {
            //is_def_cfg=1;
            LOG_E("read cfg err!");
         }        
    }
    else
    {
        rt_sem_detach(&sem_read_cfg);//脱离信号量
        LOG_D("fatal error!!!");
        return;//如果向主管道发送消息失败则说明系统异常,则直接返回
    }
}


/********************************************************************************************  
*由于两个配置表数据量较大，不适宜使用向存储组件发送消息的方式存储数据，因此本部分使用文件操作的方式进行
*
*
*
********************************************************************************************/

void RSUC_Eq_manag_writeread2(void)
{
    unsigned char i=0;
    int fd=0,size=0;

    // eq_manag.eq_num=5;

    // for(i=0;i<10;i++)
    // {
    //     eq_manag.eq[i].eq_addr=i+1;
    // }

    //使用文件操作的方式进行数据读出
    LOG_D("Used Fopen to read file!!!");


    //在配置页中前4个字符，用于放置第一次配置信息


    //写测试
    fd = open("/EQMA.CFG", O_WRONLY | O_CREAT);   /* 以创建和读写模式打开 /text.txt 文件，如果该文件不存在则创建该文件 */
    if(fd>0)        //读文件成功
    {
        write(fd, &eq_manag, sizeof(eq_manag));
        close(fd);
        LOG_D("Used Fwrite to write %d bytes...",sizeof(eq_manag));
    }


    for(i=0;i<128;i++)storage_buff[i]=0;
    rt_thread_mdelay(2000);

    //读测试
    fd = open("/EQMA.CFG", O_RDONLY);   /* 以只读模式打开 /text.txt 文件 */
    if(fd>0)        //读文件成功
    {
        size = read(fd, storage_buff, 32);
        close(fd);
    }

    LOG_D("fopen size:%d",size);

    for(i=0;i<size;i++)
     LOG_D("%d ",storage_buff[i]);
}








/*****************************************************
 * 初始化配置表，检查设备表是否存在，不存在则创建
 * 
 * 
 * 
 ****************************************************/
int Check_eq_CFG(void)
{
    int fd=0,size=0;
    int res=0;
    unsigned char tem[4]={0};
    unsigned int flag=0;
    eq_manag_type   eq_manag_temp;  

    fd = open("/EQMA.CFG", O_RDWR | O_CREAT);
    if(fd>0)     //如果打开或创建文件成功，则初始化配置表
    {
        LOG_D("EQMA.CFG creat or fopen OK,fd:%d",fd);
        size = read(fd, tem, 4);
        flag = tem[0]<<24 | tem[1]<<16 | tem[2]<<8 | tem[3];
        LOG_D("get flag success,size:%d,flag:%d ",size,flag);
        if(flag != 0x12345678)  //如果标志位不满足，则进行设备表初始化
        {
            memset(&eq_manag_temp, 0, sizeof(eq_manag_temp));
            eq_manag_temp.flag=0x12345678;

            write(fd, &eq_manag_temp, sizeof(eq_manag_temp));
            LOG_D("first init EQMA.CFG Ok");
        }
        close(eq_fd);
    }
    else  res=res|0x01;

    return res; //如果成功，返回0，如果不成功，返回对应的标志位
}

/*****************************************************
 * 管理单个设备（增加、修改、删除）
 * Input：1：操作类型：1-增加，2-修改，3-删除
 *        2：所操作的设备地址（1-Max）
 *        3：该地址对应的设备类型
 *        4：该地址对应的设备属性
 *        5：该地址对应的分组号
 * 
 ****************************************************/
int manager_eq(unsigned char oper，unsigned char addr,unsigned char type,unsigned char par,unsigned char group)
{
    int eq_fd=0,res=0;

    //参数合法性检查
    if((oper<1) || (oper>3))return -1;                      //对操作进行合法性检查
    if((addr<1) || (addr>RSUC_BUS_MAX_DEVICE))return -1;    //对地址进行合法性检查

    eq_fd = open("/EQMA.CFG", O_RDWR);    //以读写方式打开设备表
    if(eq_fd>0)     //如果成功
    {
        size = read(fd, &eq_manag, sizeof(eq_manag));   //读出数据
        
        if(oper==1)//增加设备
        {
            //首先判断新增的设备地址是否存在，否则返回错误
            if( eq_manag.eq[addr-1].eq_addr != 0){close(eq_fd);return -1;}          //  如果新增的地址已经存在，返回错误（地址根据对应的号放入），数组下标从0开始
            if( eq_manag.eq_num == RSUC_BUS_MAX_DEVICE){close(eq_fd);return -1;}    //  如果总线当前设备数量已满，返回错误（其实和地址判断是重复的）
            eq_manag.eq_num++；
            eq_manag.eq[addr-1].eq_addr=addr;
            eq_manag.eq[addr-1].eq_type=type;
            eq_manag.eq[addr-1].eq_par=par;
            eq_manag.eq[addr-1].eq_group=group;

            write(fd, &eq_manag, sizeof(eq_manag));
            LOG_D("Add eq Ok,add:%d",addr);
        }
        else if(oper==2)//修改设备
        {
            //首先判断新增的设备地址是否存在，否则返回错误
            if( eq_manag.eq[addr-1].eq_addr == 0){close(eq_fd);return -1;}          //  如果新增的地址已经存在，返回错误（地址根据对应的号放入），数组下标从0开始
            eq_manag.eq[addr-1].eq_addr=addr;
            eq_manag.eq[addr-1].eq_type=type;
            eq_manag.eq[addr-1].eq_par=par;
            eq_manag.eq[addr-1].eq_group=group;

            write(fd, &eq_manag, sizeof(eq_manag));
            LOG_D("Modify eq Ok,add:%d",addr);
        }
        else//删除设备
        {
            //首先判断新增的设备地址是否存在，否则返回错误
            if( eq_manag.eq_num == 0){close(eq_fd);return -1;}    //  如果总线当前设备数量已空，返回错误（其实和地址判断是重复的）
            eq_manag.eq_num--；
            eq_manag.eq[addr-1].eq_addr=0;
            eq_manag.eq[addr-1].eq_type=0;
            eq_manag.eq[addr-1].eq_par=0;
            eq_manag.eq[addr-1].eq_group=0;

            write(fd, &eq_manag, sizeof(eq_manag));
            LOG_D("Delte eq Ok,add:%d",addr);
        }
        
        close(eq_fd);
    }
    else 
    {
       close(eq_fd)；
       return -2； 
    } 
}










