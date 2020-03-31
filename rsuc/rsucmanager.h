#ifndef _RSUCMANAGER_H_
#define _RSUCMANAGER_H_

//注:将CPNAME替换为实际mapp名，形如UTCM STOM

#define RSUC_CPID  (8)

#define RSUC_PIPE_NMSG  (20)
#define RSUC_POOL_SIZE  (sizeof(GMS_STRU)*RSUC_PIPE_NMSG)
#define RSUC_REG_SEM_WAIT_TIME  (1000)


extern CP_CTL_STRU rsuc_ctl; //CPNAME组件控制体
extern CP_REG_STRU rsuc_reg; //用于CPNAME组件向主管道注册

#endif