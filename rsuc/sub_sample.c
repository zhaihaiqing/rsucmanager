#include "rsucmanager.h"
#include <unistd.h>

#define LOG_TAG "SPRS_SUB"
#define LOG_LVL LOG_LVL_DBG
#include <ulog.h>

//大小端转换
#define htons(n) ((((n)&0x00ff) << 8) | (((n)&0xff00) >> 8))
#define ntohs(n) htons(n)
#define htonl(n) ((((n)&0x000000ff) << 24) | \
                  (((n)&0x0000ff00) << 8) |  \
                  (((n)&0x00ff0000) >> 8) |  \
                  (((n)&0xff000000) >> 24))
#define ntohl(n) htonl(n)
#define htond(n) (htonl(n & 0xffffffff) << 32) | htonl(n >> 32)
#define ntohd(n) htond(n)

//自测试程序，读取固定地址的HCF2100
int rsuc_self_test(void)
{
    uint8_t tem[8] = {0};
    int res=0;
    uint8_t rx_len=0,err=0,i=0;
    uint16_t crc16=0;


    tem[0]=0x02;tem[1]=0x04;tem[2]=0x00;tem[3]=0x00;tem[4]=0x00;tem[5]=0x01;tem[6]=0x31;tem[7]=0xf9;

    down_uart_send_dat(&tem[0], 8); //发送指令

    rt_sem_control(&down_frame_sem, RT_IPC_CMD_RESET, RT_NULL); //等待前清零信号量，防止误操作
    res = rt_sem_take(&down_frame_sem,  300 );
    if (res == RT_EOK) //获取成功
    {
        rx_len = down_rx_device_Len;
        rt_memcpy(&rsuc_output_eq_buf[0], &down_rx_buff.dat[0], rx_len);
        crc16 = Modbus_CRC16_Check(rsuc_output_eq_buf, rx_len - 2);

        // LOG_D("sprs receive dat:");
        // for (i = 0; i < rx_len; i++)
        // {
        //     rt_kprintf("0x%02x ", rsuc_output_eq_buf[i]);
        // }
        // rt_kprintf(" rx_len:%d\r\n", rx_len);
        // rt_kprintf("crc16:0x%x\r\n", crc16);

        if ((rsuc_output_eq_buf[0] == tem[0]) && (crc16 == ((rsuc_output_eq_buf[rx_len - 1] << 8) | rsuc_output_eq_buf[rx_len - 2])) && (   ((rsuc_output_eq_buf[3]<<8) |  rsuc_output_eq_buf[4]) == 2100   ) ) //对返回的数据进行地址+CRC判定
        {
            err = 0x00;
        }
        else
        {
            err = 0x01;
        }
    }
    else
    {
        err = 0x01; //超时错误
    }

    return err;
}

//采集子函数
/**************************************************
 * 
 * rsuc_sub_sample采集子函数
 * Input：  mq_type：消息类型，RSUC组件接收到的消息类型（7-9）
 *          addr：要访问的子设备地址（1-247）
 *          *dat：透传指令的数据指针
 *          dat_len：透传指令的数据长度（不超过256）
 * 
 * Output：向业务组件返回接受到的传感器消息/或超时信息
 * 
 * ***********************************************/
int rsuc_sub_sample(uint8_t d_src, uint8_t mq_type, uint8_t addr, uint8_t *dat, uint8_t dat_len)
{
    uint8_t i = 0, rx_len = 0;

    DM_GMS_STRU rsuc_tmp_dmgms; //主管道多维消息,发送数据用
    GMS_STRU rsuc_gms;

    unsigned short crc16 = 0;
    int fd1 = 0, fd2 = 0, res = 0, size = 0;
    uint8_t eq_type = 0;
    uint8_t eq_par = 0;
    uint8_t eq_gro = 0;
    unsigned char err = 0;
    unsigned short eq_timeout = 300; //超时控制，默认为300

    eq_in_index_type eq_in_index = {0}; //索引块
    in_manag_type eq_in_block = {0};    //指令块

    //LOG_D("d_src=%d,mq_type=%d,addr=%d,dat=%d,dat_len=%d",d_src,mq_type,addr,*dat,dat_len);

    if (!is_in_table_presence)
        err = 0xC1; //指令表不存在，返回0xC1

    rt_memset(&rsuc_output_dat, 0, sizeof(rsuc_output_dat));
    if (err == 0)
    {
        if (mq_type == GET_EQ_INDEX_INFO) //如果是查询索引
        {
            fd1 = open("/INMA.CFG", O_RDWR);
            if (fd1 > 0)
            {
                fd2 = lseek(fd1, 0, SEEK_SET);
                size = read(fd1, &eq_in_index, sizeof(eq_in_index)); //读出该设备的指令
            }
            close(fd1);
        }
        else
        {
            //LOG_D("eq_manag.eq[addr - 1].eq_addr:%d",eq_manag.eq[addr - 1].eq_addr);
            if (eq_manag.eq[addr - 1].eq_addr == 0)
                err = 0xd1;                          //1：判断该设备是否在节点表中存在
            eq_type = eq_manag.eq[addr - 1].eq_type; //2：根据地址，在节点表中查找对应的设备类型
            eq_par = eq_manag.eq[addr - 1].eq_par;
            eq_gro = eq_manag.eq[addr - 1].eq_group;

            //LOG_D("22222222222err:%d",err);
            //eq_type = 2;                     //3：根据设备类型，在指令表中读取该设备的指令
            if (err == 0)
            {
                fd1 = open("/INMA.CFG", O_RDWR); //以读写方式打开指令表
                if (fd1 > 0)                     //如果成功
                {
                    fd2 = lseek(fd1, (eq_type - 1) * sizeof(eq_in_block) + RSUC_IN_INDEX_SIZE, SEEK_SET); //移动指针
                    size = read(fd1, &eq_in_block, sizeof(eq_in_block));                                  //读出该设备的指令
                    //LOG_D("Read in success");

                    eq_in_block.eq_res_time = htonl(eq_in_block.eq_res_time);
                    //LOG_D("fd1:%d,fd2:%d,size:%d", fd1, fd2, size);

                    //LOG_D("eq_type:%d,check_type:%d,res_time:%d,in_type:%d", eq_in_block.eq_type, eq_in_block.check_type, eq_in_block.eq_res_time, eq_in_block.eq_in_type);

                    if (mq_type == GET_EQ_CFG_INFO)
                        ;
                    //LOG_D("in0_len:%d,in0[0]:%d,in0[1]:%d,in0[2]:%d", eq_in_block.eq_in[0].in_len, eq_in_block.eq_in[0].in[0], eq_in_block.eq_in[0].in[1], eq_in_block.eq_in[0].in[2]);
                    else if (mq_type == GET_EQ_GET_SDAT)
                    {
                        if (eq_in_block.eq_in_type)
                            ;

                        //LOG_D("in1_len:%d,in1[0]:%d,in1[1]:%d,in1[2]:%d", eq_in_block.eq_in[1].in_len, eq_in_block.eq_in[1].in[0], eq_in_block.eq_in[1].in[1], eq_in_block.eq_in[1].in[2]);
                        //LOG_D("in2_len:%d,in2[0]:%d,in2[1]:%d,in2[2]:%d", eq_in_block.eq_in[2].in_len, eq_in_block.eq_in[2].in[0], eq_in_block.eq_in[2].in[1], eq_in_block.eq_in[2].in[2]);
                    }
                    else if (mq_type == EQ_CUSTOM_COMMAND)
                        ;
                    //LOG_D("in3_len:%d,in3[0]:%d,in3[1]:%d,in3[2]:%d", eq_in_block.eq_in[3].in_len, eq_in_block.eq_in[3].in[0], eq_in_block.eq_in[3].in[1], eq_in_block.eq_in[3].in[2]);
                }
                close(fd1);

                if ((fd1 <= 0) || (fd2 < 0))
                {
                    //文件操作错误，发送信号量
                    if ((size != sizeof(eq_in_index)) && (size != sizeof(eq_in_index)))
                    {
                        err = 0xc3;
                        LOG_D("SEN_EQ_PAS failure");
                    }
                }
            }
        } 
    }

    switch (mq_type) //判断消息类型，根据设备类型执行不同的采集指令
    {
    case GET_EQ_INDEX_INFO: //查询索引信息
                            //将索引块发送给业务组件
        rsuc_output_dat.src = d_src;
        rsuc_output_dat.eq_addr = addr;
        rsuc_output_dat.eq_type = eq_type;
        rsuc_output_dat.eq_par = eq_par;
        rsuc_output_dat.eq_gro = eq_gro;
        rsuc_output_dat.mq_type = mq_type;
        rsuc_output_dat.is_mq_success = 0;
        rsuc_output_dat.d_len = sizeof(eq_in_index);
        rt_memcpy(&rsuc_output_eq_buf[0], &eq_in_index.version_H, rsuc_output_dat.d_len);
        rsuc_output_dat.dp = &rsuc_output_eq_buf[0];
        //发送数据
        rt_memset(&rsuc_tmp_dmgms, 0, sizeof(DM_GMS_STRU));
        mb_make_dmgms(&rsuc_tmp_dmgms, 0, &sem_rsuc, CP_CMD_SRC(GET_EQ_INDEX_INFO), d_src, RSUC_CPID, (uint8_t *)&rsuc_output_dat, sizeof(rsuc_output_dat), &rsuc_resp_data); //向多维消息体中装入消息
        if (RT_EOK == Rsuc_Send_Msg(&rsuc_tmp_dmgms))
        {
            //LOG_D("return param suc!");
        }
        else
        {
            LOG_E("return param err!");
        }
        break;
    case GET_EQ_CFG_INFO: //获取配置信息
        eq_in_block.eq_in[0].in[0] = addr;
        //LOG_D("eq_in_block.eq_in[0].in[0]:%d,addr=%d",eq_in_block.eq_in[0].in[0],addr);
        crc16 = Modbus_CRC16_Check(&eq_in_block.eq_in[0].in[0], eq_in_block.eq_in[0].in_len - 2);
        eq_in_block.eq_in[0].in[eq_in_block.eq_in[0].in_len - 2] = crc16 & 0xff;
        eq_in_block.eq_in[0].in[eq_in_block.eq_in[0].in_len - 1] = crc16 >> 8;
        eq_timeout = eq_in_block.eq_res_time;

        // LOG_D("sprs send in0:");
        // for (i = 0; i < eq_in_block.eq_in[0].in_len; i++)
        // {
        //     rt_kprintf("0x%02x ", eq_in_block.eq_in[0].in[i]);
        // }
        // rt_kprintf("\r\n");

        if (err == 0)
        {
            down_uart_send_dat(&eq_in_block.eq_in[0].in[0], eq_in_block.eq_in[0].in_len); //发送指令

            rt_sem_control(&down_frame_sem, RT_IPC_CMD_RESET, RT_NULL); //等待前清零信号量，防止误操作
            res = rt_sem_take(&down_frame_sem, eq_timeout * 3);
            if (res == RT_EOK) //获取成功
            {
                rx_len = down_rx_device_Len;
                rt_memcpy(&rsuc_output_eq_buf[0], &down_rx_buff.dat[0], rx_len);
                crc16 = Modbus_CRC16_Check(rsuc_output_eq_buf, rx_len - 2);

                // LOG_D("sprs receive dat:");
                // for (i = 0; i < rx_len; i++)
                // {
                //     rt_kprintf("0x%02x ", rsuc_output_eq_buf[i]);
                // }
                // rt_kprintf(" rx_len:%d\r\n", rx_len);
                // rt_kprintf("crc16:0x%x\r\n", crc16);

                if ((rsuc_output_eq_buf[0] == addr) && (crc16 == ((rsuc_output_eq_buf[rx_len - 1] << 8) | rsuc_output_eq_buf[rx_len - 2]))) //对返回的数据进行地址+CRC判定
                {
                    err = 0x00;
                }
                else
                {
                    err = 0xD3;
                }
            }
            else
            {
                err = 0xD2; //超时错误
            }
        }

        //向业务组件发送消息
        rsuc_output_dat.src = d_src;
        rsuc_output_dat.eq_addr = addr;
        rsuc_output_dat.eq_type = eq_type;
        rsuc_output_dat.eq_par = eq_par;
        rsuc_output_dat.eq_gro = eq_gro;
        rsuc_output_dat.mq_type = mq_type;
        rsuc_output_dat.is_mq_success = err;
        rsuc_output_dat.d_len = rx_len;
        rsuc_output_dat.dp = &rsuc_output_eq_buf[0];
        //发送数据
        rt_memset(&rsuc_tmp_dmgms, 0, sizeof(DM_GMS_STRU));
        mb_make_dmgms(&rsuc_tmp_dmgms, 0, &sem_rsuc, CP_CMD_SRC(GET_EQ_CFG_INFO), d_src, RSUC_CPID, (uint8_t *)&rsuc_output_dat, sizeof(rsuc_output_dat), &rsuc_resp_data); //向多维消息体中装入消息
        if (RT_EOK == Rsuc_Send_Msg(&rsuc_tmp_dmgms))
        {
            //LOG_D("return param suc!");
        }
        else
        {
            LOG_E("return param err!");
        }

        break;
    case GET_EQ_GET_SDAT: //获取数据，判断单双指令
        //LOG_D("GET_EQ_GET_SDAT");
        if (eq_in_block.eq_in_type == 1) //双指令，先发采样指令
        {
            eq_in_block.eq_in[1].in[0] = addr;
            crc16 = Modbus_CRC16_Check(&eq_in_block.eq_in[1].in[0], eq_in_block.eq_in[1].in_len - 2);
            eq_in_block.eq_in[1].in[eq_in_block.eq_in[1].in_len - 2] = crc16 & 0xff;
            eq_in_block.eq_in[1].in[eq_in_block.eq_in[1].in_len - 1] = crc16 >> 8;
            eq_timeout = eq_in_block.eq_res_time;

            // LOG_D("sprs send in1:");
            // for (i = 0; i < eq_in_block.eq_in[1].in_len; i++)
            // {
            //     rt_kprintf("0x%02x ", eq_in_block.eq_in[1].in[i]);
            // }
            // rt_kprintf("\r\n");

            if (err == 0)
            {
                //LOG_D("eq_timeout:%d",eq_timeout);
                down_uart_send_dat(&eq_in_block.eq_in[1].in[0], eq_in_block.eq_in[1].in_len); //发送采样指令

                rt_sem_control(&down_frame_sem, RT_IPC_CMD_RESET, RT_NULL); //等待前清零信号量，防止误操作
                res = rt_sem_take(&down_frame_sem, eq_timeout);             //等待信号量
                if (res == RT_EOK)
                {
                    rx_len = down_rx_device_Len;
                    rt_memcpy(&rsuc_output_eq_buf[0], &down_rx_buff.dat[0], rx_len);
                    crc16 = Modbus_CRC16_Check(rsuc_output_eq_buf, rx_len - 2);

                    // LOG_D("sprs receive dat:", crc16);
                    // for (i = 0; i < rx_len; i++)
                    // {
                    //     rt_kprintf("0x%02x ", rsuc_output_eq_buf[i]);
                    // }
                    // rt_kprintf(" rx_len:%d\r\n", rx_len);
                    // rt_kprintf("crc16:0x%x\r\n", crc16);

                    if ((rsuc_output_eq_buf[0] == addr) && (crc16 == ((rsuc_output_eq_buf[rx_len - 1] << 8) | rsuc_output_eq_buf[rx_len - 2])))
                    {
                        err = 0x00;
                    }
                    else
                    {
                        err = 0xD3;
                    }
                }
                else
                {
                    err = 0xD2; //超时错误
                }
            }
            if (err != 0)
            {
                //向业务组件发送错误
                //LOG_D("In1 Nack,err:0x%x", err);
                rsuc_output_dat.src = d_src;
                rsuc_output_dat.eq_addr = addr;
                rsuc_output_dat.eq_type = eq_type;
                rsuc_output_dat.eq_par = eq_par;
                rsuc_output_dat.eq_gro = eq_gro;
                rsuc_output_dat.mq_type = mq_type;
                rsuc_output_dat.is_mq_success = err;
                rsuc_output_dat.d_len = rx_len;
                rsuc_output_dat.dp = &rsuc_output_eq_buf[0];
                //发送数据
                rt_memset(&rsuc_tmp_dmgms, 0, sizeof(DM_GMS_STRU));
                mb_make_dmgms(&rsuc_tmp_dmgms, 0, &sem_rsuc, CP_CMD_SRC(GET_EQ_GET_SDAT), d_src, RSUC_CPID, (uint8_t *)&rsuc_output_dat, sizeof(rsuc_output_dat), &rsuc_resp_data); //向多维消息体中装入消息
                if (RT_EOK == Rsuc_Send_Msg(&rsuc_tmp_dmgms))
                {
                    //LOG_D("return param suc!");
                }
                else
                {
                    LOG_E("return param err!");
                }
                return -1;
            }
        }

        rt_thread_mdelay(4);
        //发送数据获取指令
        eq_in_block.eq_in[2].in[0] = addr;
        crc16 = Modbus_CRC16_Check(&eq_in_block.eq_in[2].in[0], eq_in_block.eq_in[2].in_len - 2);
        eq_in_block.eq_in[2].in[eq_in_block.eq_in[2].in_len - 2] = crc16 & 0xff;
        eq_in_block.eq_in[2].in[eq_in_block.eq_in[2].in_len - 1] = crc16 >> 8;
        eq_timeout = eq_in_block.eq_res_time;

        // LOG_D("sprs send in2:");
        // for (i = 0; i < eq_in_block.eq_in[2].in_len; i++)
        // {
        //     rt_kprintf("0x%02x ", eq_in_block.eq_in[2].in[i]);
        // }
        // rt_kprintf("\r\n");

        if (err == 0)
        {
            down_uart_send_dat(&eq_in_block.eq_in[2].in[0], eq_in_block.eq_in[2].in_len); //发送数据获取指令

            rt_sem_control(&down_frame_sem, RT_IPC_CMD_RESET, RT_NULL); //等待前清零信号量，防止误操作
            res = rt_sem_take(&down_frame_sem, eq_timeout);

            if (res == RT_EOK) //获取成功
            {
                rx_len = down_rx_device_Len;
                rt_memset(&rsuc_output_eq_buf[0], 0, 256);
                rt_memcpy(&rsuc_output_eq_buf[0], &down_rx_buff.dat[0], rx_len);
                crc16 = Modbus_CRC16_Check(&rsuc_output_eq_buf[0], rx_len - 2);

                // LOG_D("sprs receive dat:", crc16);
                // for (i = 0; i < rx_len+2; i++)
                // {
                //     rt_kprintf("0x%02x ", rsuc_output_eq_buf[i]);
                // }
                // rt_kprintf(" rx_len:%d\r\n", rx_len);

                // rt_kprintf("crc16:0x%x\r\n", crc16);

                if ((rsuc_output_eq_buf[0] == addr) && (crc16 == ((rsuc_output_eq_buf[rx_len - 1] << 8) | rsuc_output_eq_buf[rx_len - 2]))) //对返回的数据进行地址+CRC判定
                {
                    err = 0x00;
                }
                else
                {
                    err = 0xD3;
                }
            }
            else
            {
                err = 0xD2; //超时错误
            }
        }

        //LOG_D("err_info:0x%x，%d", err, sizeof(rsuc_output_dat));

        //向业务组件发送消息
        rsuc_output_dat.src = d_src;
        rsuc_output_dat.eq_addr = addr;
        rsuc_output_dat.eq_type = eq_type;
        rsuc_output_dat.eq_par = eq_par;
        rsuc_output_dat.eq_gro = eq_gro;
        rsuc_output_dat.mq_type = mq_type;
        rsuc_output_dat.is_mq_success = err;
        rsuc_output_dat.d_len = rx_len;
        rsuc_output_dat.dp = &rsuc_output_eq_buf[0];
        //LOG_D("dp:0x%x", rsuc_output_dat.dp);
        //发送数据
        rt_memset(&rsuc_tmp_dmgms, 0, sizeof(DM_GMS_STRU));
        mb_make_dmgms(&rsuc_tmp_dmgms, 0, &sem_rsuc, CP_CMD_SRC(GET_EQ_GET_SDAT), d_src, RSUC_CPID, (uint8_t *)&rsuc_output_dat, sizeof(rsuc_output_dat), &rsuc_resp_data); //向多维消息体中装入消息
        if (RT_EOK == Rsuc_Send_Msg(&rsuc_tmp_dmgms))
        {
            //LOG_D("return param suc!");
        }
        else
        {
            LOG_E("return param err!");
        }
        break;
    case EQ_CUSTOM_COMMAND: //自定义指令
        eq_in_block.eq_in[3].in[0] = addr;
        crc16 = Modbus_CRC16_Check(&eq_in_block.eq_in[3].in[0], eq_in_block.eq_in[3].in_len - 2);
        eq_in_block.eq_in[3].in[eq_in_block.eq_in[3].in_len - 2] = crc16 & 0xff;
        eq_in_block.eq_in[3].in[eq_in_block.eq_in[3].in_len - 1] = crc16 >> 8;
        eq_timeout = eq_in_block.eq_res_time;

        if (err == 0)
        {
            down_uart_send_dat(&eq_in_block.eq_in[3].in[0], eq_in_block.eq_in[3].in_len); //发送指令

            rt_sem_control(&down_frame_sem, RT_IPC_CMD_RESET, RT_NULL); //等待前清零信号量，防止误操作
            res = rt_sem_take(&down_frame_sem, eq_timeout);
            if (res == RT_EOK) //获取成功
            {
                rx_len = down_rx_device_Len;
                //for(i=0;i<rx_len;i++)rsuc_output_eq_buf[i]=down_rx_buff.dat[i];//获取串口数据
                rt_memcpy(&rsuc_output_eq_buf[0], &down_rx_buff.dat[0], rx_len);
                crc16 = Modbus_CRC16_Check(rsuc_output_eq_buf, rx_len - 2);

                // rt_kprintf("sprs receive dat:\r\n", crc16);
                // for (i = 0; i < rx_len; i++)
                // {
                //     rt_kprintf("0x%02x ", rsuc_output_eq_buf[i]);
                // }
                // rt_kprintf(" rx_len:%d\r\n", rx_len);
                // rt_kprintf("crc16:0x%x\r\n", crc16);

                if ((rsuc_output_eq_buf[0] == addr) && (crc16 == ((rsuc_output_eq_buf[rx_len - 1] << 8) | rsuc_output_eq_buf[rx_len - 2]))) //对返回的数据进行地址+CRC判定
                {
                    err = 0x00;
                }
                else
                {
                    err = 0xD3;
                }
            }
            else
            {
                err = 0xD2; //超时错误
            }
        }

        //向业务组件发送消息
        rsuc_output_dat.src = d_src;
        rsuc_output_dat.eq_addr = addr;
        rsuc_output_dat.eq_type = eq_type;
        rsuc_output_dat.eq_par = eq_par;
        rsuc_output_dat.eq_gro = eq_gro;
        rsuc_output_dat.mq_type = mq_type;
        rsuc_output_dat.is_mq_success = err;
        rsuc_output_dat.d_len = rx_len;
        rsuc_output_dat.dp = &rsuc_output_eq_buf[0];
        //发送数据
        rt_memset(&rsuc_tmp_dmgms, 0, sizeof(DM_GMS_STRU));
        mb_make_dmgms(&rsuc_tmp_dmgms, 0, &sem_rsuc, CP_CMD_SRC(EQ_CUSTOM_COMMAND), d_src, RSUC_CPID, (uint8_t *)&rsuc_output_dat, sizeof(rsuc_output_dat), &rsuc_resp_data); //向多维消息体中装入消息
        if (RT_EOK == Rsuc_Send_Msg(&rsuc_tmp_dmgms))
        {
            //LOG_D("return param suc!");
        }
        else
        {
            LOG_E("return param err!");
        }
        break;
    case SEN_EQ_PAS:                      //透传指令
        down_uart_send_dat(dat, dat_len); //发送透传指令

        rt_sem_control(&down_frame_sem, RT_IPC_CMD_RESET, RT_NULL); //等待前清零信号量，防止误操作
        res = rt_sem_take(&down_frame_sem, 2000);                   //获取信号量   //发送完成后，延时300ms，做超时控制
        if (res == RT_EOK)                                          //获取成功
        {
            rx_len = down_rx_device_Len;
            //for(i=0;i<rx_len;i++)rsuc_output_eq_buf[i]=down_rx_buff.dat[i];//获取串口数据
            rt_memcpy(&rsuc_output_eq_buf[0], &down_rx_buff.dat[0], rx_len);

            // rt_kprintf("sprs receive dat:\r\n", crc16);
            // for (i = 0; i < rx_len; i++)
            // {
            //     rt_kprintf("0x%02x ", rsuc_output_eq_buf[i]);
            // }
            // rt_kprintf(" rx_len:%d\r\n", rx_len);
            // rt_kprintf("crc16:0x%x\r\n", crc16);
        }
        else
        {
            err = 0xD2; //超时错误
        }

        //向业务组件发送消息
        rsuc_output_dat.src = d_src;
        rsuc_output_dat.eq_addr = 0;
        rsuc_output_dat.eq_type = 0;
        rsuc_output_dat.eq_par = 0;
        rsuc_output_dat.eq_gro = 0;
        rsuc_output_dat.mq_type = mq_type;
        rsuc_output_dat.is_mq_success = err;
        rsuc_output_dat.d_len = rx_len;
        rsuc_output_dat.dp = &rsuc_output_eq_buf[0];
        //发送数据
        rt_memset(&rsuc_tmp_dmgms, 0, sizeof(DM_GMS_STRU));
        mb_make_dmgms(&rsuc_tmp_dmgms, 0, &sem_rsuc, CP_CMD_SRC(SEN_EQ_PAS), d_src, RSUC_CPID, (uint8_t *)&rsuc_output_dat, sizeof(rsuc_output_dat), &rsuc_resp_data); //向多维消息体中装入消息
        if (RT_EOK == Rsuc_Send_Msg(&rsuc_tmp_dmgms))
        {
            //LOG_D("return param suc!");
        }
        else
        {
            LOG_E("return param err!");
        }
        break;
    default:

        break;
    }
}
