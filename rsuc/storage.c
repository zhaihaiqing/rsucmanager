 #include "rsucmanager.h"
#include <unistd.h>

unsigned char storage_buff[128]={0};    //定义测试buf

eq_manag_type   eq_manag;       //定义所有的总线设备管理表，组件启动后，读取该表到内存中
//in_manag_type   eq_in_block;    //定义单台设备的指令块，RSUC接收到业务组件消息后，根据访问设备的类型，将该设备的指令块读入到内存中（指令表可能比较大，无法完全读入到内存中）

#define   EQ_IN_BLOCK_SIZE    sizeof(eq_in_block);  //设备指令块大小，指针移动

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

    eq_manag.version=RSUC_VERSION;
    eq_manag.flag=0x1234;
    eq_manag.eq_num=5;

    for(i=0;i<10;i++)
    {
        eq_manag.eq[i].eq_addr=i+1;
    }

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
    int fd1=0,fd2=0,fd3=0,size=0;
    int res=0,status=0;
    unsigned char tem[6]={0};
    unsigned short version=0;
    unsigned int flag=0;
    eq_manag_type   eq_manag_temp;  
    struct stat stat_buf;
    int i=0;

    res=stat("/EQMA.CFG",&stat_buf);//获取文件信息
    if(res==0)//获取文件信息成功，说明文件存在
    {
        LOG_D("EQMA.CFG read info OK,size:%d",stat_buf.st_size);

        fd1 = open("/EQMA.CFG", O_RDWR | O_CREAT);
        size = read(fd1, tem, 4);
        for(i=0;i<6;i++)LOG_D("tem[%d]:0x%x",i,tem[i]);
        version=tem[1]<<8 | tem[0];
        flag = tem[3]<<8 | tem[2];
        LOG_D("get flag success,size:%d,version:0x%x,flag:0x%x ",size,version,flag);

        if( (version!=RSUC_VERSION) || (flag != 0x1234) )  //如果标志位和版本号不满足，则进行设备表初始化
        {
            LOG_D("Version or flag is not correat!");
            close(fd1);
            fd2=unlink("/EQMA.CFG");
            if(fd2==0)
            {
                LOG_D("unlink EQMA.CFG OK!");
                fd3=open("/EQMA.CFG", O_RDWR | O_CREAT);
                if(fd3>0)
                {
                    memset(&eq_manag_temp, 0, sizeof(eq_manag_temp));
                    eq_manag_temp.version=RSUC_VERSION;
                    eq_manag_temp.flag=0x1234;

                    write(fd3, &eq_manag_temp, sizeof(eq_manag_temp));
                    LOG_D("Creat and init EQMA.CFG Ok");
                    close(fd3);

                    status=1;
                }
                else
                {
                    LOG_D("Creat EQMA.CFG failure!");
                    status=0;
                }
            }
            if(fd2==-1)
            {
                LOG_D("unlink EQMA.CFG failure!");
                status=0;
            }
        }
        else
        {
            LOG_D("Version or flag is correat! Init OK");
            status=1;
        }
    }

    if(res==-1)//获取文件信息失败，说明文件不存在
    {
        LOG_D("EQMA.CFG read info failure!");
        fd2=open("/EQMA.CFG", O_RDWR | O_CREAT);
        if(fd2>0)
        {
            memset(&eq_manag_temp, 0, sizeof(eq_manag_temp));
            eq_manag_temp.version=RSUC_VERSION;
            eq_manag_temp.flag=0x1234;

            write(fd2, &eq_manag_temp, sizeof(eq_manag_temp));
            LOG_D("Creat and init EQMA.CFG Ok");
            close(fd2);
            status=1;
        }
        else
        {
            LOG_D("Creat EQMA.CFG failure!");
            status=0;
         }
    }

    //读测试
    rt_thread_mdelay(20);
    memset(storage_buff, 0, 128);
    
    fd1 = open("/EQMA.CFG", O_RDONLY);   /* 以只读模式打开 /text.txt 文件 */
    if(fd1>0)        //读文件成功
    {
        lseek(fd1,4,SEEK_SET);
        size = read(fd1, storage_buff, 32);
        close(fd1);
    }
    LOG_D("fopen size:%d",size);

    for(i=0;i<size;i++)
     LOG_D("buff[%d]:%d ",i,storage_buff[i]);

    return status; //如果成功，返回0，如果不成功，返回对应的标志位
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
int manager_eq(uint8_t d_src,uint8_t mq_type,uint8_t addr,uint8_t type,uint8_t par,uint8_t group)
{
    int fd=0,res=0,size=0;
    uint8_t temp[4]={0};
    uint8_t err=0;

    DM_GMS_STRU rsuc_tmp_dmgms;//主管道多维消息,发送数据用
    GMS_STRU rsuc_gms;

    //参数合法性检查
    if(!is_eq_table_presence)err= 0xE1;                   //指令表不存在，返回0xfc
    if((addr<1) || (addr>RSUC_BUS_MAX_DEVICE))err= 0xE2;  //地址异常，返回0xf8

    fd = open("/EQMA.CFG", O_RDWR);    //以读写方式打开设备表
    if(fd>0)     //如果成功
    {
        size = read(fd, &eq_manag, sizeof(eq_manag));   //读出数据
        
        if(mq_type==1)//增加设备
        {
            //首先判断新增的设备地址是否存在，否则返回错误
            if( eq_manag.eq[addr-1].eq_addr != 0){close(fd);err= 0xE3;}          //  如果新增的地址已经存在，返回错误（地址根据对应的号放入），数组下标从0开始
            if( eq_manag.eq_num == RSUC_BUS_MAX_DEVICE){close(fd);err= 0xE4;}    //  如果总线当前设备数量已满，返回错误（其实和地址判断是重复的）
            eq_manag.eq_num++;
            eq_manag.eq[addr-1].eq_addr=addr;
            eq_manag.eq[addr-1].eq_type=type;
            eq_manag.eq[addr-1].eq_par=par;
            eq_manag.eq[addr-1].eq_group=group;

            write(fd, &eq_manag, sizeof(eq_manag));
            LOG_D("Add eq Ok,add:%d",addr);
            
        }
        else if(mq_type==2)//删除设备
        {
            //首先判断新增的设备地址是否存在，否则返回错误
            if( eq_manag.eq[addr-1].eq_addr == 0){close(fd);err= 0xE5;}
            if( eq_manag.eq_num == 0){close(fd);err= 0XE6;}    //  如果总线当前设备数量已空，返回错误（其实和地址判断是重复的）
            eq_manag.eq_num--;
            eq_manag.eq[addr-1].eq_addr=0;
            eq_manag.eq[addr-1].eq_type=0;
            eq_manag.eq[addr-1].eq_par=0;
            eq_manag.eq[addr-1].eq_group=0;

            write(fd, &eq_manag, sizeof(eq_manag));
            LOG_D("Delte eq Ok,add:%d",addr);
            
        }
        else if(mq_type==3)//修改设备
        {
            //首先判断新增的设备地址是否存在，否则返回错误
            if( eq_manag.eq[addr-1].eq_addr == 0){close(fd);err= 0xE7;}          //  如果新增的地址已经存在，返回错误（地址根据对应的号放入），数组下标从0开始
            eq_manag.eq[addr-1].eq_addr=addr;
            eq_manag.eq[addr-1].eq_type=type;
            eq_manag.eq[addr-1].eq_par=par;
            eq_manag.eq[addr-1].eq_group=group;

            write(fd, &eq_manag, sizeof(eq_manag));
            LOG_D("Modify eq Ok,add:%d",addr);
            
        }
        else    //查询设备
        {
            if( eq_manag.eq[addr-1].eq_addr == 0){close(fd);err= 0xE8;} 
            temp[0]=eq_manag.eq[addr-1].eq_addr;
            temp[1]=eq_manag.eq[addr-1].eq_type;
            temp[2]=eq_manag.eq[addr-1].eq_par;
            temp[3]=eq_manag.eq[addr-1].eq_group;

            //向业务组件发送消息
        }
        close(fd);
        err= 0;//返回成功
    }
    else 
    {
       close(fd);
       err= 0xE9;
    } 

    // if(err==0)
    // {
    //     rsuc_output_dat.src=d_src;
    //     rsuc_output_dat.mq_type=mq_type;
    //     rsuc_output_dat.is_mq_success=err;
    //     rsuc_output_dat.d_len=0;
    //     rsuc_output_dat.dp  = &rsuc_output_eq_buf[0];
    //     //rt_memcpy(&rsuc_output_dat.dat[0],&rx_temp[0],rsuc_output_dat.d_len);
    //     //发送数据
    //     mb_make_dmgms(&rsuc_tmp_dmgms,0,&sem_rsuc,CP_CMD_DST(SEN_EQ_PAS),d_src,RSUC_CPID,(uint8_t *)&rsuc_output_dat,sizeof(rsuc_output_dat),&rsuc_resp_data); //向多维消息体中装入消息
    //     if(RT_EOK == Rsuc_Send_Msg(&rsuc_tmp_dmgms))
    //     {
    //         LOG_D("return param suc!");
    //     }   
    //     else
    //     {
    //         LOG_E("return param err!");
    //     }
    // }
    // else
    // {
        rsuc_output_dat.src=d_src;
        rsuc_output_dat.mq_type=mq_type;
        rsuc_output_dat.is_mq_success=err;
        rsuc_output_dat.d_len=0;   
        rsuc_output_dat.dp  = &rsuc_output_eq_buf[0];
        //rt_memcpy(&rsuc_output_dat.dat[0],&rx_temp[0],rsuc_output_dat.d_len);
        //发送数据
        mb_make_dmgms(&rsuc_tmp_dmgms,0,&sem_rsuc,CP_CMD_DST(SEN_EQ_PAS),d_src,RSUC_CPID,(uint8_t *)&rsuc_output_dat,sizeof(rsuc_output_dat),&rsuc_resp_data); //向多维消息体中装入消息
        if(RT_EOK == Rsuc_Send_Msg(&rsuc_tmp_dmgms))
        {
            LOG_D("return param suc!");
        }   
        else
        {
            LOG_E("return param err!");
        }
    //}
    


}



/*****************************************************
 * 初始化指令表，检查指令表是否存在，不存在则返回错误
 * 
 * 
 * 
 ****************************************************/
int Check_in_CFG(void)
{
    int fd=0,size=0;
    int res=0,status=0;
    struct stat stat_buf;
    in_manag_type   eq_in_blocktest={0};

    res=stat("/INMA.CFG",&stat_buf);//获取文件信息
    if(res==0)//获取文件信息成功
    {
        LOG_D("INMA.CFG read info OK,ALL_IN_size:%d,eq_in_size",stat_buf.st_size,sizeof(eq_in_blocktest));
        status=1;
    }
    else
    {
       LOG_D("INMA.CFG read failure");
       status=0;
    }

    return status; //如果成功，返回0，如果不成功，返回对应的标志位
}


int Init_in_CFG(void)
{
    int fd=0,size=0;
    int res=0,status=0;
    int i=0;
    struct stat stat_buf;

    eq_in_index_type eq_in_index={0};
    in_manag_type   eq_in_blocktest={0};

    //创建指令表，仅供测试
    fd = open("/INMA.CFG", O_RDWR | O_CREAT);
    if(fd>0)     //如果打开或创建文件成功，则初始化配置表
    {
        LOG_D("Open INMA.CFG");

        rt_memset(&eq_in_index,0,sizeof(eq_in_index));
        eq_in_index.version_H=0x04;
        eq_in_index.version_L=0x00;
        eq_in_index.support_eq_type_minID=0x01;
        eq_in_index.support_eq_type_maxID=0x03;
        lseek(fd,0,SEEK_SET); 
        write(fd, &eq_in_index, sizeof(eq_in_index));

        rt_memset(&eq_in_blocktest,0,sizeof(eq_in_blocktest));
        eq_in_blocktest.eq_type=1;
        eq_in_blocktest.check_type=1;
        eq_in_blocktest.eq_res_time=300;
        eq_in_blocktest.eq_in_type=1;
        eq_in_blocktest.eq_in[2].in_len=8;
        eq_in_blocktest.eq_in[2].in[0]=1;
        eq_in_blocktest.eq_in[2].in[1]=0x04;
        eq_in_blocktest.eq_in[2].in[2]=0x00;
        eq_in_blocktest.eq_in[2].in[3]=0x10;
        eq_in_blocktest.eq_in[2].in[4]=0x00;
        eq_in_blocktest.eq_in[2].in[5]=0x13;
        lseek(fd,(eq_in_blocktest.eq_type-1)*sizeof(eq_in_blocktest)+RSUC_IN_INDEX_SIZE,SEEK_SET); 
        write(fd, &eq_in_blocktest, sizeof(eq_in_blocktest));

        rt_memset(&eq_in_blocktest,0,sizeof(eq_in_blocktest));
        eq_in_blocktest.eq_type=2;
        eq_in_blocktest.check_type=1;
        eq_in_blocktest.eq_res_time=300;
        eq_in_blocktest.eq_in_type=0;
        eq_in_blocktest.eq_in[0].in_len=8;
        eq_in_blocktest.eq_in[0].in[0]=1;
        eq_in_blocktest.eq_in[0].in[1]=0x03;
        eq_in_blocktest.eq_in[0].in[2]=0x00;
        eq_in_blocktest.eq_in[0].in[3]=0x00;
        eq_in_blocktest.eq_in[0].in[4]=0x00;
        eq_in_blocktest.eq_in[0].in[5]=0x10;

        eq_in_blocktest.eq_in[2].in_len=8;
        eq_in_blocktest.eq_in[2].in[0]=1;
        eq_in_blocktest.eq_in[2].in[1]=0x04;
        eq_in_blocktest.eq_in[2].in[2]=0x00;
        eq_in_blocktest.eq_in[2].in[3]=0x10;
        eq_in_blocktest.eq_in[2].in[4]=0x00;
        eq_in_blocktest.eq_in[2].in[5]=0x13;

        eq_in_blocktest.eq_in[3].in_len=8;
        eq_in_blocktest.eq_in[3].in[0]=1;
        eq_in_blocktest.eq_in[3].in[1]=0x03;
        eq_in_blocktest.eq_in[3].in[2]=0x00;
        eq_in_blocktest.eq_in[3].in[3]=0x56;
        eq_in_blocktest.eq_in[3].in[4]=0x00;
        eq_in_blocktest.eq_in[3].in[5]=0x02;
        lseek(fd,(eq_in_blocktest.eq_type-1)*sizeof(eq_in_blocktest)+RSUC_IN_INDEX_SIZE,SEEK_SET); 
        write(fd, &eq_in_blocktest, sizeof(eq_in_blocktest));

        rt_memset(&eq_in_blocktest,0,sizeof(eq_in_blocktest));
        eq_in_blocktest.eq_type=3;
        eq_in_blocktest.check_type=1;
        eq_in_blocktest.eq_res_time=300;
        eq_in_blocktest.eq_in_type=0;
        eq_in_blocktest.eq_in[2].in_len=8;
        eq_in_blocktest.eq_in[2].in[0]=1;
        eq_in_blocktest.eq_in[2].in[1]=0x04;
        eq_in_blocktest.eq_in[2].in[2]=0x00;
        eq_in_blocktest.eq_in[2].in[3]=0x10;
        eq_in_blocktest.eq_in[2].in[4]=0x00;
        eq_in_blocktest.eq_in[2].in[5]=0x13;
        lseek(fd,(eq_in_blocktest.eq_type-1)*sizeof(eq_in_blocktest)+RSUC_IN_INDEX_SIZE,SEEK_SET); 
        write(fd, &eq_in_blocktest, sizeof(eq_in_blocktest));

        LOG_D("INMA.CFG creat or fopen OK,fd:%d",fd);
        stat("/INMA.CFG",&stat_buf);
        LOG_D("INMA.CFG size:%d,eq_in_size:%d",stat_buf.st_size,sizeof(eq_in_blocktest));
        close(fd);
    }
    else  
    {
        LOG_D("Creat INMA.CFG failure");
    }


    return status; //如果成功，返回0，如果不成功，返回对应的标志位
}



















