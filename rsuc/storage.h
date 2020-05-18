#ifndef _STORAGE_H_
#define _STORAGE_H_

#include "rsucmanager.h"

#define RSUC_CFG_FN ("/rsuc.cfg")
#define RSUC_Eq_manag_CFG_FN     ("/eqma.cfg")         //定义设备管理表
#define RSUC_In_manag_CFG_FN     ("/inma.cfg")         //定义指令管理表


#define RSUC_CFG_READ_WAIT_TIME (1000)

#define RSUC_BUS_MAX_DEVICE     128             //总线上支持的从机设备数量
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
    unsigned char eq_num;						    //总线从机数量
	signel_eq_manag_type eq[RSUC_BUS_MAX_DEVICE];	//为每个从设备分配一个属性
}eq_manag_type;


/*************************************************************
*  定义单类传感器虚拟指令结构体   
*  每种传感器支持最多六类指令（获取配置信息、获取校准信息、获取设备信息、获取数据信息、设置初始值、采样指令）
*  每一类指令由1条或多条指令组成
*  索引功能，用于指定每条指令的长度、采样间隔
 ************************************************************/
//#define  RSUC_SUPPORT_MAX_DEVICE	16		//RSUC支持的最大设备类型数量
#define  RSUC_IN_INDEX_SIZE			16		//指令索引信息大小
#define  RSUC_ONE_IN_MAX_LEN		12		//单条指令最长字节
#define  RSUC_ONE_EQ_MAX_INCOUNT	4		//单台设备最多由多少条指令组成

typedef struct __attribute__ ((__packed__))		//单条指令的结构
{
	uint8_t 	version_H;									//版本号
	uint8_t 	version_L;									//版本号
	uint8_t 	support_eq_type_minID;
	uint8_t 	support_eq_type_maxID;
	uint8_t   	backup[RSUC_IN_INDEX_SIZE-4];               //备用
}eq_in_index_type;

typedef struct __attribute__ ((__packed__))		//单条指令的结构
{
	unsigned char 	in_len;										//指令的实际长度（0：不合法，1-19）
	unsigned char   in[RSUC_ONE_IN_MAX_LEN];                    //指令的缓存区域（0-10）
}signel_in_manag_type;

typedef struct __attribute__ ((__packed__))						//单个设备的指令构成()
{
	unsigned char 			eq_type;							//设备类型(1-256，0：不支持)
	unsigned char			check_type;							//设备校验类型，通常为modbuscrc16
	unsigned int			eq_res_time;						//设备响应时间（ms）
	unsigned char			eq_in_type;							//设备指令类型（0：单指令，1：双指令）
	signel_in_manag_type 	eq_in[RSUC_ONE_EQ_MAX_INCOUNT];		//实际的8条指令（透传指令不需要存储）
}in_manag_type;




extern eq_manag_type   eq_manag;   	//定义所有的总线设备
//extern in_manag_type   eq_in_block;	//设备指令表

void RSUC_Eq_manag_writeread2(void);
int Check_eq_CFG(void);             //检查并初始化设备表     
int manager_eq(uint8_t d_src,uint8_t mq_type,uint8_t addr,uint8_t type,uint8_t par,uint8_t group);//设备管理

int mk_eqin_dir(void);
int Check_in_CFG(void);

 #endif