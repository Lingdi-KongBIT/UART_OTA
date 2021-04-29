#ifndef __YMODEM_H
#define __YMODEM_H
#ifdef __cplusplus
extern "C" {
#endif



/*=====用户配置(根据自己的分区进行配置)=====*/
//大内存512K，共256页，每页2KB
#define BootLoader_Size 		0x14000U			//< BootLoader的大小 80K，40页
#define Application_Size		0x14000U			//< 应用程序的大小 80K

#define Application_Pages		40U						//每页大小
#define Application_Addr		0x08014000U		//< 应用程序的首地址
/*==========================================*/



#define SOH		0x01
#define STX		0x02
#define ACK		0x06
#define NACK	0x15
#define EOT		0x04
#define CCC		0x43



/* 升级的步骤 */
enum UPDATE_STATE
{
	TO_START = 0x01,
	TO_RECEIVE_DATA = 0x02,
	TO_RECEIVE_EOT1 = 0x03,
	TO_RECEIVE_EOT2 = 0x04,
	TO_RECEIVE_END = 0x05
};



void ymodem_fun(void);



#ifdef __cplusplus
}
#endif

#endif /* __YMODEM_H */
