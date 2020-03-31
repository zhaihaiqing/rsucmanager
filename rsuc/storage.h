 #ifndef _STORAGE_H_
 #define _STORAGE_H_


#define RSUC_CFG_FN ("/rsuc.cfg")


#define RSUC_Eq_manag_CFG_FN     ("/eqma.cfg")         //定义设备管理表
#define RSUC_In_manag_CFG_FN     ("/inma.cfg")         //定义指令管理表


#define RSUC_CFG_READ_WAIT_TIME (1000)

#define RSUC_BUS_MAX_DEVICE     247             //总线上支持的从机设备数量

/*  定义单个节点属性列表，单台设备共有4种属性    */
typedef struct __attribute__ ((__packed__))
{
	unsigned char eq_addr;						    //节点地址
	unsigned char eq_type;						    //节点类型
    unsigned char eq_par;						    //节点属性
    unsigned char eq_group;						    //节点分组
}signel_eq_manag_type;


/*  定义总线上所有的从设备    */
typedef struct __attribute__ ((__packed__))
{
	unsigned int  flag;                             //标志位，用于判断配置表是否初始化过
    unsigned char eq_num;						    //总线从机数量
	signel_eq_manag_type eq[RSUC_BUS_MAX_DEVICE];	//为每个从设备分配一个属性
}eq_manag_type;




void RSUC_Eq_manag_writeread2(void);


int Check_eq_CFG(void);             //检查并初始化设备表     
int manager_eq(unsigned char oper，unsigned char addr,unsigned char type,unsigned char par,unsigned char group);//设备管理

 #endif