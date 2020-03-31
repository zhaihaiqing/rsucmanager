#ifndef _RSUC_PRO_H_
#define _RSUC_PRO_H_

//#define _ _
#define RSUC_SEM_WAIT_TIME (100)

typedef struct
{
 //动态库函数指针
 //void *(pfun_)(void); 
 //将动态库中的函数逐一装入到这些函数指针中，以备使用
} DL_FUNCS_STRU;


#define RSUC_MAX_NODE_NUM   247
typedef struct __attribute__ ((__packed__))
{
	unsigned char node_num;						        //总线上节点数量
	unsigned char node_id[RSUC_MAX_NODE_NUM];			//对应节点的地址
	unsigned char node_type[RSUC_MAX_NODE_NUM];	    	//节点类型
    unsigned char node_pro[RSUC_MAX_NODE_NUM];          //节点属性
    unsigned char node_group[RSUC_MAX_NODE_NUM];        //节点分组
	
}rxuc_node_type;





void  testprintf(void);

#endif