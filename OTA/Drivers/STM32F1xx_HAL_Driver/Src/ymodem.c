#include "ymodem.h"
#include "main.h"
#include "stdio.h"

/*********************************************************
**跳转函数
*********************************************************/
typedef void (*pFunction)(void);//函数指针
//#define APPLICATION_START_ADDRESS	((uint32_t)0x0800a000)
uint32_t jumpaddress = Application_Addr;
void JumpToApp()
{
	printf("Start App!\n");
	__disable_irq();//停止中断
	jumpaddress = *(__IO uint32_t*) (Application_Addr + 4);
	pFunction Jump_To_Application = (pFunction) jumpaddress;
	__set_MSP(*(__IO uint32_t*) Application_Addr);
	Jump_To_Application();
	while(1);
}

/* 发送指令 */
void send_command(unsigned char command)
{
	HAL_UART_Transmit(&huart1, (uint8_t *)&command,1, 0xFFFF);
	HAL_Delay(10);
}



/**
 * @bieaf 擦除页
 *
 * @param pageaddr  页起始地址	
 * @param num       擦除的页数
 * @return 1
 */
static int Erase_page(uint32_t pageaddr, uint32_t num)
{
	HAL_FLASH_Unlock();
	
	/* 擦除FLASH*/
	FLASH_EraseInitTypeDef FlashSet;
	FlashSet.TypeErase = FLASH_TYPEERASE_PAGES;
	FlashSet.PageAddress = pageaddr;
	FlashSet.NbPages = num;
	
	/*设置PageError，调用擦除函数*/
	uint32_t PageError = 0;
	HAL_FLASHEx_Erase(&FlashSet, &PageError);
	
	HAL_FLASH_Lock();
	return 1;
}



/**
 * @bieaf 写若干个数据
 *
 * @param addr       写入的地址
 * @param buff       写入数据的数组指针
 * @param word_size  长度
 * @return 
 */
static void WriteFlash(uint32_t addr, uint32_t * buff, int word_size)
{	
	/* 1/4解锁FLASH*/
	HAL_FLASH_Unlock();
	
	for(int i = 0; i < word_size; i++)	
	{
		/* 3/4对FLASH烧写*/
		HAL_FLASH_Program(FLASH_TYPEPROGRAM_WORD, addr + 4 * i, buff[i]);	
	}

	/* 4/4锁住FLASH*/
	HAL_FLASH_Lock();
}

/**
 * @bieaf CRC-16 校验
 *
 * @param addr 开始地址
 * @param num   长度
 * @param num   CRC
 * @return crc  返回CRC的值
 */
#define POLY        0x1021  
uint16_t crc16(unsigned char *addr, int num, uint16_t crc)  
{  
    int i;  
    for (; num > 0; num--)					/* Step through bytes in memory */  
    {  
        crc = crc ^ (*addr++ << 8);			/* Fetch byte from memory, XOR into CRC top byte*/  
        for (i = 0; i < 8; i++)				/* Prepare to rotate 8 bits */  
        {
            if (crc & 0x8000)				/* b15 is set... */  
                crc = (crc << 1) ^ POLY;  	/* rotate and XOR with polynomic */  
            else                          	/* b15 is clear... */  
                crc <<= 1;					/* just rotate */  
        }									/* Loop for 8 bits */  
        crc &= 0xFFFF;						/* Ensure CRC remains 16-bit value */  
    }										/* Loop until num=0 */  
    return(crc);							/* Return updated CRC */  
}



/**
 * @bieaf 获取数据包的类型, 顺便进行校验
 *
 * @param buf 开始地址
 * @param len 长度
 * @return 
 */
unsigned char Check_CRC(unsigned char* buf, int len)
{
	unsigned short crc = 0;
	//return 1;
	/* 进行CRC校验 */
	if((buf[0]==0x01)&&(len >= 133))
	{
		crc = crc16(buf+3, 128, crc);
		if(crc != (buf[131]<<8|buf[132]))
		{
			return 0;///< 没通过校验
		}
		
		/* 通过校验 */
		return 1;
	}
	else if(buf[0]==0x02&&len>=1029)
	{
		crc = crc16(buf+3, 1024, crc);
		if(crc != (buf[1027]<<8|buf[1028]))
		{
			return 0;///< 没通过校验
		}
		
		/* 通过校验 */
		return 1;
	}
	else
		return 0;
}



/* 设置升级的步骤 */
static enum UPDATE_STATE update_state = TO_START;
void Set_state(enum UPDATE_STATE state)
{
	update_state = state;
}


/* 查询升级的步骤 */
unsigned char Get_state(void)
{
	return update_state;
}



unsigned char temp_buf[2048] = {0};
uint16_t temp_len = 0;

int setSendPacket(uint8_t commend)
{
	switch(commend)
	{
		case 0xf1://上报设备版本
		{
			sendPacket.version=version;
			sendPacket.commend=commend;
			sendPacket.dataLen[1]=0x02;
			//先清空数据位
			for(int i=0;i<256;i++)
			{
				sendPacket.data[i]=0x00;
			}
			//需要注意版本只有8位，这个帧申请了2B表示版本，故这里写了data[1]
			sendPacket.data[1]=version;
			//尚未生成校验
			//sendPacket.hCrc=;
			//sendPacket.lCrc=;
			
			//组帧
			int index=0x00;
			for(int i=0;i<3;i++)
			{
				sendBuffer[index]=sendPacket.head[i];
				index++;
			}
			for(int i=0;i<1;i++)
			{
				sendBuffer[index]=sendPacket.version;
				index++;
			}
			for(int i=0;i<1;i++)
			{
				sendBuffer[index]=sendPacket.commend;
				index++;
			}
			for(int i=0;i<2;i++)
			{
				sendBuffer[index]=sendPacket.dataLen[i];
				index++;
			}
			int temp=(int)((sendPacket.dataLen[0]<<8)|sendPacket.dataLen[1]);
			for(int i=0;i<temp;i++)
			{
				sendBuffer[index]=sendPacket.data[i];
				index++;
			}
			for(int i=0;i<1;i++)
			{
				sendBuffer[index]=sendPacket.hCrc;
				index++;
			}
			for(int i=0;i<1;i++)
			{
				sendBuffer[index]=sendPacket.lCrc;
				index++;
			}
			for(int i=0;i<3;i++)
			{
				sendBuffer[index]=sendPacket.tail[i];
				index++;
			}
			return index;
		}break;
		case 0xf2://返回是否更新
		{
			sendPacket.version=version;
			sendPacket.commend=commend;
			sendPacket.dataLen[1]=0x01;
			//先清空数据位
			for(int i=0;i<256;i++)
			{
				sendPacket.data[i]=0x00;
			}
			if((uint32_t)((temp_buf[7]<<4)|temp_buf[8])>version)//版本低时更新
			{
				update=1;
				sendPacket.data[0]=0x01;//表示更新
			}
			else if((uint32_t)((temp_buf[9] << 24)|(temp_buf[10]<<16)|(temp_buf[11]<<8)| temp_buf[12])<40*1024)//大小符合，可以更新
			{
				update=1;
				sendPacket.data[0]=0x01;//表示更新
			}
			else//有问题，不更新
			{
				sendPacket.data[0]=0x02;//设备忙
			}
			//sendPacket.data[0]=0x03;//Flash容量不足
			//sendPacket.data[0]=0x04;//剩余电量不足
			//sendPacket.data[0]=0x05;//信号强度不足
			//sendPacket.data[0]=0x06;//已经是最新版本
			//sendPacket.data[0]=0x07;//出现FLASH错误
			//sendPacket.data[0]=0x08;//出现未知错误
			//尚未生成校验
			//sendPacket.hCrc=;
			//sendPacket.lCrc=;
			
			//组帧
			int index=0x00;
			for(int i=0;i<3;i++)
			{
				sendBuffer[index]=sendPacket.head[i];
				index++;
			}
			for(int i=0;i<1;i++)
			{
				sendBuffer[index]=sendPacket.version;
				index++;
			}
			for(int i=0;i<1;i++)
			{
				sendBuffer[index]=sendPacket.commend;
				index++;
			}
			for(int i=0;i<2;i++)
			{
				sendBuffer[index]=sendPacket.dataLen[i];
				index++;
			}
			int temp=(int)((sendPacket.dataLen[0]<<8)|sendPacket.dataLen[1]);
			for(int i=0;i<temp;i++)
			{
				sendBuffer[index]=sendPacket.data[i];
				index++;
			}
			for(int i=0;i<1;i++)
			{
				sendBuffer[index]=sendPacket.hCrc;
				index++;
			}
			for(int i=0;i<1;i++)
			{
				sendBuffer[index]=sendPacket.lCrc;
				index++;
			}
			for(int i=0;i<3;i++)
			{
				sendBuffer[index]=sendPacket.tail[i];
				index++;
			}
			return index;
		}break;
		case 0xf3://收到更新命令，请平台下发更新包
		{
			sendPacket.version=version;
			sendPacket.commend=commend;
			sendPacket.dataLen[1]=0x00;
			//先清空数据位
			for(int i=0;i<256;i++)
			{
				sendPacket.data[i]=0x00;
			}
			//尚未生成校验
			//sendPacket.hCrc=;
			//sendPacket.lCrc=;
			
			//组帧
			int index=0x00;
			for(int i=0;i<3;i++)
			{
				sendBuffer[index]=sendPacket.head[i];
				index++;
			}
			for(int i=0;i<1;i++)
			{
				sendBuffer[index]=sendPacket.version;
				index++;
			}
			for(int i=0;i<1;i++)
			{
				sendBuffer[index]=sendPacket.commend;
				index++;
			}
			for(int i=0;i<2;i++)
			{
				sendBuffer[index]=sendPacket.dataLen[i];
				index++;
			}
			//帧长为0，这里就不写数据位了
			for(int i=0;i<1;i++)
			{
				sendBuffer[index]=sendPacket.hCrc;
				index++;
			}
			for(int i=0;i<1;i++)
			{
				sendBuffer[index]=sendPacket.lCrc;
				index++;
			}
			for(int i=0;i<3;i++)
			{
				sendBuffer[index]=sendPacket.tail[i];
				index++;
			}
			return index;
		}break;
		case 0xf4://下载固件成功
		{
			sendPacket.version=version;
			sendPacket.commend=commend;
			sendPacket.dataLen[1]=0x00;
			//先清空数据位
			for(int i=0;i<256;i++)
			{
				sendPacket.data[i]=0x00;
			}
			//尚未生成校验
			//sendPacket.hCrc=;
			//sendPacket.lCrc=;
			
			//组帧
			int index=0x00;
			for(int i=0;i<3;i++)
			{
				sendBuffer[index]=sendPacket.head[i];
				index++;
			}
			for(int i=0;i<1;i++)
			{
				sendBuffer[index]=sendPacket.version;
				index++;
			}
			for(int i=0;i<1;i++)
			{
				sendBuffer[index]=sendPacket.commend;
				index++;
			}
			for(int i=0;i<2;i++)
			{
				sendBuffer[index]=sendPacket.dataLen[i];
				index++;
			}
			//数据位是空
			for(int i=0;i<1;i++)
			{
				sendBuffer[index]=sendPacket.hCrc;
				index++;
			}
			for(int i=0;i<1;i++)
			{
				sendBuffer[index]=sendPacket.lCrc;
				index++;
			}
			for(int i=0;i<3;i++)
			{
				sendBuffer[index]=sendPacket.tail[i];
				index++;
			}
			return index;
		}break;
		case 0xf5://升级成功
		{
			sendPacket.version=version;
			sendPacket.commend=commend;
			sendPacket.dataLen[1]=0x02;
			//先清空数据位
			for(int i=0;i<256;i++)
			{
				sendPacket.data[i]=0x00;
			}
			//需要注意版本只有8位，这个帧申请了2B表示版本，故这里写了data[1]
			sendPacket.data[1]=version;
			//尚未生成校验
			//sendPacket.hCrc=;
			//sendPacket.lCrc=;
			
			//组帧
			int index=0x00;
			for(int i=0;i<3;i++)
			{
				sendBuffer[index]=sendPacket.head[i];
				index++;
			}
			for(int i=0;i<1;i++)
			{
				sendBuffer[index]=sendPacket.version;
				index++;
			}
			for(int i=0;i<1;i++)
			{
				sendBuffer[index]=sendPacket.commend;
				index++;
			}
			for(int i=0;i<2;i++)
			{
				sendBuffer[index]=sendPacket.dataLen[i];
				index++;
			}
			int temp=(int)((sendPacket.dataLen[0]<<8)|sendPacket.dataLen[1]);
			for(int i=0;i<temp;i++)
			{
				sendBuffer[index]=sendPacket.data[i];
				index++;
			}
			for(int i=0;i<1;i++)
			{
				sendBuffer[index]=sendPacket.hCrc;
				index++;
			}
			for(int i=0;i<1;i++)
			{
				sendBuffer[index]=sendPacket.lCrc;
				index++;
			}
			for(int i=0;i<3;i++)
			{
				sendBuffer[index]=sendPacket.tail[i];
				index++;
			}
			return index;
		}break;
		case 0xA1://crc校验出错
		{
			sendPacket.version=version;
			sendPacket.commend=commend;
			sendPacket.dataLen[1]=0x00;
			//先清空数据位
			for(int i=0;i<256;i++)
			{
				sendPacket.data[i]=0x00;
			}
			//尚未生成校验
			//sendPacket.hCrc=;
			//sendPacket.lCrc=;
			
			//组帧
			int index=0x00;
			for(int i=0;i<3;i++)
			{
				sendBuffer[index]=sendPacket.head[i];
				index++;
			}
			for(int i=0;i<1;i++)
			{
				sendBuffer[index]=sendPacket.version;
				index++;
			}
			for(int i=0;i<1;i++)
			{
				sendBuffer[index]=sendPacket.commend;
				index++;
			}
			for(int i=0;i<2;i++)
			{
				sendBuffer[index]=sendPacket.dataLen[i];
				index++;
			}
			//空数据区
			for(int i=0;i<1;i++)
			{
				sendBuffer[index]=sendPacket.hCrc;
				index++;
			}
			for(int i=0;i<1;i++)
			{
				sendBuffer[index]=sendPacket.lCrc;
				index++;
			}
			for(int i=0;i<3;i++)
			{
				sendBuffer[index]=sendPacket.tail[i];
				index++;
			}
			return index;
		}break;
	}
	return -1;
}

/**
 * @bieaf YModem升级
 *
 * @param none
 * @return none
 */
void ymodem_fun(void)
{
	int i;
	if(updating)//让更新了就发CCC
	{
		if(Get_state()==TO_START)
		{
			send_command(CCC);
			HAL_Delay(1000);
		}
	}
	if(Rx_Flag)    	// Receive flag
	{
		Rx_Flag=0;	// clean flag
				
		/* 拷贝 */
		temp_len = Rx_Len;
		for(i = 0; i < temp_len; i++)
		{
			temp_buf[i] = Rx_Buf[i];
		}
		for(i = 0; i < temp_len; i++)
		{
			printf("%d/",temp_buf[i]);
		}
		printf("\r\n");
		
		//非更新状态
		if(updating==0)
		{
			switch(temp_buf[4])
			{
				case 0x01://查询版本
				{
					int temp=setSendPacket(0xf1);
					if(temp!=-1)//成功组帧
					{
						HAL_UART_Transmit(&huart1, (uint8_t *)&sendBuffer,temp, 0xFFFF);
					}
				}break;
				case 0x02://查询是否可以更新，现在是版本低、小于40K就可以更新，不考虑其他情况
				{
					int temp=setSendPacket(0xf2);
					if(temp!=-1)//成功组帧
					{
						HAL_UART_Transmit(&huart1, (uint8_t *)&sendBuffer,temp, 0xFFFF);
					}
				}break;
				case 0x03://发出更新命令
				{
					if(update)//让更新，返回F3让平台下发更新包
					{
						int temp=setSendPacket(0xf3);
						if(temp!=-1)//成功组帧
						{
							HAL_UART_Transmit(&huart1, (uint8_t *)&sendBuffer,temp, 0xFFFF);
							updating=1;
						}
					}
					else//不让更新，返回0xF2不让更新命令
					{
						int temp=setSendPacket(0xf2);
						if(temp!=-1)//成功组帧
						{
							HAL_UART_Transmit(&huart1, (uint8_t *)&sendBuffer,temp, 0xFFFF);
						}
					}
					
				}break;
				/*case 0xA1://crc出错，请求重发
				{
					HAL_UART_Transmit(&huart1, (uint8_t *)&sendBuffer,temp, 0xFFFF);
				}break;*/
			}
			
		}
		else//更新状态
		{
			switch(temp_buf[0])
			{
				static unsigned char data_state = 0;
				case SOH:///<数据包开始
				{
					if(Check_CRC(temp_buf, temp_len)==1)///< 通过CRC16校验
					{					
						if((Get_state()==TO_START)&&(temp_buf[1] == 0x00)&&(temp_buf[2] == (unsigned char)(~temp_buf[1])))///< 开始
						{
							printf("> Receive start...\r\n");

							Set_state(TO_RECEIVE_DATA);
							data_state = 0x01;						
							send_command(ACK);
							send_command(CCC);

							/* 擦除App2 */							
							Erase_page(Application_Addr, Application_Pages);
						}
						else if((Get_state()==TO_RECEIVE_END)&&(temp_buf[1] == 0x00)&&(temp_buf[2] == (unsigned char)(~temp_buf[1])))///< 结束
						{
							printf("> Receive end...\r\n");					
							Set_state(TO_START);
							send_command(ACK);
							JumpToApp();
							//HAL_NVIC_SystemReset();
						}					
						else if((Get_state()==TO_RECEIVE_DATA)&&(temp_buf[1] == data_state)&&(temp_buf[2] == (unsigned char)(~temp_buf[1])))///< 接收数据
						{
							printf("> Receive data bag:%d byte\r\n",data_state * 128);

							/* 烧录程序 */
							WriteFlash((Application_Addr + (data_state-1) * 128), (uint32_t *)(&temp_buf[3]), 32);
							data_state++;
							
							send_command(ACK);		
						}
					}
					else
					{
						printf("> Notpass crc\r\n");
					}
					
				}break;
				case STX:
				{
					//static unsigned char data_state = 0;
					//static unsigned int app2_size = 0;
					if(Check_CRC(temp_buf, temp_len)==1)///< 通过CRC16校验
					{					
						if((Get_state()==TO_START)&&(temp_buf[1] == 0x00)&&(temp_buf[2] == (unsigned char)(~temp_buf[1])))///< 开始
						{
							printf("> Receive start...\r\n");

							Set_state(TO_RECEIVE_DATA);
							data_state = 0x01;						
							send_command(ACK);
							send_command(CCC);
							//printf("%d,%d\r\n",temp_buf[1],temp_buf[2]);

							/* 擦除App2 */							
							Erase_page(Application_Addr, Application_Pages);
						}
						else if((Get_state()==TO_RECEIVE_END)&&(temp_buf[1] == 0x00)&&(temp_buf[2] == (unsigned char)(~temp_buf[1])))///< 结束
						{
							printf("> Receive end...\r\n");

							//Set_Update_Down();						
							Set_state(TO_START);
							send_command(ACK);
							//HAL_NVIC_SystemReset();
							JumpToApp();
						}					
						else if((Get_state()==TO_RECEIVE_DATA)&&(temp_buf[1] == data_state)&&(temp_buf[2] == (unsigned char)(~temp_buf[1])))///< 接收数据
						{
							printf("> Receive data bag:%d byte\r\n",data_state * 1024);
							
							/* 烧录程序 */
							WriteFlash((Application_Addr + (data_state-1) * 1024), (uint32_t *)(&temp_buf[3]), 256);
							data_state++;
							//printf("%d,%d\r\n",temp_buf[1],temp_buf[2]);
							send_command(ACK);		
						}
					}
					else
					{
						printf("> Notpass crc\r\n");
					}
					
				}break;
				case EOT://结束数据包
				{
					if(Get_state()==TO_RECEIVE_DATA)
					{
						printf("> Receive EOT1...\r\n");
						
						Set_state(TO_RECEIVE_EOT2);					
						send_command(NACK);
					}
					else if(Get_state()==TO_RECEIVE_EOT2)
					{
						printf("> Receive EOT2...\r\n");
						
						Set_state(TO_RECEIVE_END);					
						send_command(ACK);
						send_command(CCC);
					}
					else
					{
						printf("> Receive EOT, But error...\r\n");
					}
				}break;	
			}
		}
		
	}
}



