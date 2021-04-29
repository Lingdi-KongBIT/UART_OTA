#include "ymodem.h"
#include "main.h"
#include "stdio.h"

/*********************************************************
**��ת����
*********************************************************/
typedef void (*pFunction)(void);//����ָ��
//#define APPLICATION_START_ADDRESS	((uint32_t)0x0800a000)
uint32_t jumpaddress = Application_Addr;
void JumpToApp()
{
	printf("Start App!\n");
	__disable_irq();//ֹͣ�ж�
	jumpaddress = *(__IO uint32_t*) (Application_Addr + 4);
	pFunction Jump_To_Application = (pFunction) jumpaddress;
	__set_MSP(*(__IO uint32_t*) Application_Addr);
	Jump_To_Application();
	while(1);
}

/* ����ָ�� */
void send_command(unsigned char command)
{
	HAL_UART_Transmit(&huart1, (uint8_t *)&command,1, 0xFFFF);
	HAL_Delay(10);
}



/**
 * @bieaf ����ҳ
 *
 * @param pageaddr  ҳ��ʼ��ַ	
 * @param num       ������ҳ��
 * @return 1
 */
static int Erase_page(uint32_t pageaddr, uint32_t num)
{
	HAL_FLASH_Unlock();
	
	/* ����FLASH*/
	FLASH_EraseInitTypeDef FlashSet;
	FlashSet.TypeErase = FLASH_TYPEERASE_PAGES;
	FlashSet.PageAddress = pageaddr;
	FlashSet.NbPages = num;
	
	/*����PageError�����ò�������*/
	uint32_t PageError = 0;
	HAL_FLASHEx_Erase(&FlashSet, &PageError);
	
	HAL_FLASH_Lock();
	return 1;
}



/**
 * @bieaf д���ɸ�����
 *
 * @param addr       д��ĵ�ַ
 * @param buff       д�����ݵ�����ָ��
 * @param word_size  ����
 * @return 
 */
static void WriteFlash(uint32_t addr, uint32_t * buff, int word_size)
{	
	/* 1/4����FLASH*/
	HAL_FLASH_Unlock();
	
	for(int i = 0; i < word_size; i++)	
	{
		/* 3/4��FLASH��д*/
		HAL_FLASH_Program(FLASH_TYPEPROGRAM_WORD, addr + 4 * i, buff[i]);	
	}

	/* 4/4��סFLASH*/
	HAL_FLASH_Lock();
}

/**
 * @bieaf CRC-16 У��
 *
 * @param addr ��ʼ��ַ
 * @param num   ����
 * @param num   CRC
 * @return crc  ����CRC��ֵ
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
 * @bieaf ��ȡ���ݰ�������, ˳�����У��
 *
 * @param buf ��ʼ��ַ
 * @param len ����
 * @return 
 */
unsigned char Check_CRC(unsigned char* buf, int len)
{
	unsigned short crc = 0;
	//return 1;
	/* ����CRCУ�� */
	if((buf[0]==0x01)&&(len >= 133))
	{
		crc = crc16(buf+3, 128, crc);
		if(crc != (buf[131]<<8|buf[132]))
		{
			return 0;///< ûͨ��У��
		}
		
		/* ͨ��У�� */
		return 1;
	}
	else if(buf[0]==0x02&&len>=1029)
	{
		crc = crc16(buf+3, 1024, crc);
		if(crc != (buf[1027]<<8|buf[1028]))
		{
			return 0;///< ûͨ��У��
		}
		
		/* ͨ��У�� */
		return 1;
	}
	else
		return 0;
}



/* ���������Ĳ��� */
static enum UPDATE_STATE update_state = TO_START;
void Set_state(enum UPDATE_STATE state)
{
	update_state = state;
}


/* ��ѯ�����Ĳ��� */
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
		case 0xf1://�ϱ��豸�汾
		{
			sendPacket.version=version;
			sendPacket.commend=commend;
			sendPacket.dataLen[1]=0x02;
			//���������λ
			for(int i=0;i<256;i++)
			{
				sendPacket.data[i]=0x00;
			}
			//��Ҫע��汾ֻ��8λ�����֡������2B��ʾ�汾��������д��data[1]
			sendPacket.data[1]=version;
			//��δ����У��
			//sendPacket.hCrc=;
			//sendPacket.lCrc=;
			
			//��֡
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
		case 0xf2://�����Ƿ����
		{
			sendPacket.version=version;
			sendPacket.commend=commend;
			sendPacket.dataLen[1]=0x01;
			//���������λ
			for(int i=0;i<256;i++)
			{
				sendPacket.data[i]=0x00;
			}
			if((uint32_t)((temp_buf[7]<<4)|temp_buf[8])>version)//�汾��ʱ����
			{
				update=1;
				sendPacket.data[0]=0x01;//��ʾ����
			}
			else if((uint32_t)((temp_buf[9] << 24)|(temp_buf[10]<<16)|(temp_buf[11]<<8)| temp_buf[12])<40*1024)//��С���ϣ����Ը���
			{
				update=1;
				sendPacket.data[0]=0x01;//��ʾ����
			}
			else//�����⣬������
			{
				sendPacket.data[0]=0x02;//�豸æ
			}
			//sendPacket.data[0]=0x03;//Flash��������
			//sendPacket.data[0]=0x04;//ʣ���������
			//sendPacket.data[0]=0x05;//�ź�ǿ�Ȳ���
			//sendPacket.data[0]=0x06;//�Ѿ������°汾
			//sendPacket.data[0]=0x07;//����FLASH����
			//sendPacket.data[0]=0x08;//����δ֪����
			//��δ����У��
			//sendPacket.hCrc=;
			//sendPacket.lCrc=;
			
			//��֡
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
		case 0xf3://�յ����������ƽ̨�·����°�
		{
			sendPacket.version=version;
			sendPacket.commend=commend;
			sendPacket.dataLen[1]=0x00;
			//���������λ
			for(int i=0;i<256;i++)
			{
				sendPacket.data[i]=0x00;
			}
			//��δ����У��
			//sendPacket.hCrc=;
			//sendPacket.lCrc=;
			
			//��֡
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
			//֡��Ϊ0������Ͳ�д����λ��
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
		case 0xf4://���ع̼��ɹ�
		{
			sendPacket.version=version;
			sendPacket.commend=commend;
			sendPacket.dataLen[1]=0x00;
			//���������λ
			for(int i=0;i<256;i++)
			{
				sendPacket.data[i]=0x00;
			}
			//��δ����У��
			//sendPacket.hCrc=;
			//sendPacket.lCrc=;
			
			//��֡
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
			//����λ�ǿ�
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
		case 0xf5://�����ɹ�
		{
			sendPacket.version=version;
			sendPacket.commend=commend;
			sendPacket.dataLen[1]=0x02;
			//���������λ
			for(int i=0;i<256;i++)
			{
				sendPacket.data[i]=0x00;
			}
			//��Ҫע��汾ֻ��8λ�����֡������2B��ʾ�汾��������д��data[1]
			sendPacket.data[1]=version;
			//��δ����У��
			//sendPacket.hCrc=;
			//sendPacket.lCrc=;
			
			//��֡
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
		case 0xA1://crcУ�����
		{
			sendPacket.version=version;
			sendPacket.commend=commend;
			sendPacket.dataLen[1]=0x00;
			//���������λ
			for(int i=0;i<256;i++)
			{
				sendPacket.data[i]=0x00;
			}
			//��δ����У��
			//sendPacket.hCrc=;
			//sendPacket.lCrc=;
			
			//��֡
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
			//��������
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
 * @bieaf YModem����
 *
 * @param none
 * @return none
 */
void ymodem_fun(void)
{
	int i;
	if(updating)//�ø����˾ͷ�CCC
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
				
		/* ���� */
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
		
		//�Ǹ���״̬
		if(updating==0)
		{
			switch(temp_buf[4])
			{
				case 0x01://��ѯ�汾
				{
					int temp=setSendPacket(0xf1);
					if(temp!=-1)//�ɹ���֡
					{
						HAL_UART_Transmit(&huart1, (uint8_t *)&sendBuffer,temp, 0xFFFF);
					}
				}break;
				case 0x02://��ѯ�Ƿ���Ը��£������ǰ汾�͡�С��40K�Ϳ��Ը��£��������������
				{
					int temp=setSendPacket(0xf2);
					if(temp!=-1)//�ɹ���֡
					{
						HAL_UART_Transmit(&huart1, (uint8_t *)&sendBuffer,temp, 0xFFFF);
					}
				}break;
				case 0x03://������������
				{
					if(update)//�ø��£�����F3��ƽ̨�·����°�
					{
						int temp=setSendPacket(0xf3);
						if(temp!=-1)//�ɹ���֡
						{
							HAL_UART_Transmit(&huart1, (uint8_t *)&sendBuffer,temp, 0xFFFF);
							updating=1;
						}
					}
					else//���ø��£�����0xF2���ø�������
					{
						int temp=setSendPacket(0xf2);
						if(temp!=-1)//�ɹ���֡
						{
							HAL_UART_Transmit(&huart1, (uint8_t *)&sendBuffer,temp, 0xFFFF);
						}
					}
					
				}break;
				/*case 0xA1://crc���������ط�
				{
					HAL_UART_Transmit(&huart1, (uint8_t *)&sendBuffer,temp, 0xFFFF);
				}break;*/
			}
			
		}
		else//����״̬
		{
			switch(temp_buf[0])
			{
				static unsigned char data_state = 0;
				case SOH:///<���ݰ���ʼ
				{
					if(Check_CRC(temp_buf, temp_len)==1)///< ͨ��CRC16У��
					{					
						if((Get_state()==TO_START)&&(temp_buf[1] == 0x00)&&(temp_buf[2] == (unsigned char)(~temp_buf[1])))///< ��ʼ
						{
							printf("> Receive start...\r\n");

							Set_state(TO_RECEIVE_DATA);
							data_state = 0x01;						
							send_command(ACK);
							send_command(CCC);

							/* ����App2 */							
							Erase_page(Application_Addr, Application_Pages);
						}
						else if((Get_state()==TO_RECEIVE_END)&&(temp_buf[1] == 0x00)&&(temp_buf[2] == (unsigned char)(~temp_buf[1])))///< ����
						{
							printf("> Receive end...\r\n");					
							Set_state(TO_START);
							send_command(ACK);
							JumpToApp();
							//HAL_NVIC_SystemReset();
						}					
						else if((Get_state()==TO_RECEIVE_DATA)&&(temp_buf[1] == data_state)&&(temp_buf[2] == (unsigned char)(~temp_buf[1])))///< ��������
						{
							printf("> Receive data bag:%d byte\r\n",data_state * 128);

							/* ��¼���� */
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
					if(Check_CRC(temp_buf, temp_len)==1)///< ͨ��CRC16У��
					{					
						if((Get_state()==TO_START)&&(temp_buf[1] == 0x00)&&(temp_buf[2] == (unsigned char)(~temp_buf[1])))///< ��ʼ
						{
							printf("> Receive start...\r\n");

							Set_state(TO_RECEIVE_DATA);
							data_state = 0x01;						
							send_command(ACK);
							send_command(CCC);
							//printf("%d,%d\r\n",temp_buf[1],temp_buf[2]);

							/* ����App2 */							
							Erase_page(Application_Addr, Application_Pages);
						}
						else if((Get_state()==TO_RECEIVE_END)&&(temp_buf[1] == 0x00)&&(temp_buf[2] == (unsigned char)(~temp_buf[1])))///< ����
						{
							printf("> Receive end...\r\n");

							//Set_Update_Down();						
							Set_state(TO_START);
							send_command(ACK);
							//HAL_NVIC_SystemReset();
							JumpToApp();
						}					
						else if((Get_state()==TO_RECEIVE_DATA)&&(temp_buf[1] == data_state)&&(temp_buf[2] == (unsigned char)(~temp_buf[1])))///< ��������
						{
							printf("> Receive data bag:%d byte\r\n",data_state * 1024);
							
							/* ��¼���� */
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
				case EOT://�������ݰ�
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



