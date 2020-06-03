#ifndef _SUB_SAMPLE_H_
#define _SUB_SAMPLE_H_
#include "rsucmanager.h"

// enum SUB_FUNCTION_CODE
// {
// 	ADD_EQ_NODE=1,			//增加节点
// 	DEL_EQ_NODE=2,			//删除节点
// 	CHA_EQ_NODE=3,			//修改节点
// 	GET_EQ_NODE=4,			//查询节点

// 	GET_EQ_INDEX_INFO=9,	//获取指令表索引信息
// 	GET_EQ_CFG_INFO=10,		//获取配置信息
// 	GET_EQ_GET_SDAT=11,		//获取采样数据
// 	EQ_CUSTOM_COMMAND =12,	//自定义指令
// 	SEN_EQ_PAS	   =13,		//透传指令
// };





enum EQ_CODE
{
	EQ_HCF700=1,
	EQ_HCF710=2,
	EQ_HCF1100=3,
};


int rsuc_self_test(void);
int rsuc_sub_sample(uint8_t d_src,uint8_t mq_type,uint8_t addr,uint8_t *dat,uint8_t dat_len);


void    HCF710_Read_test(void);



 #endif