#include "exti.h"

/***********************
��Ŀ�����ж���������
���ߣ������ĵ�����--����
����ţ�tb43915564
�޸����ڣ�2025/3/20
�������ã�
************************/   

//�ⲿ�ж�PPB1/PB12���жϷ�������ʼ��
void EXTIX_Init(void)
{
		//GPIO_InitTypeDef GPIO_InitStructure;
		EXTI_InitTypeDef EXTI_InitStructure;
		NVIC_InitTypeDef NVIC_InitStructure;

		/*����ʱ��*/
		RCC_APB2PeriphClockCmd(RCC_APB2Periph_AFIO,ENABLE);	//ʹ�ܸ��ù���ʱ��
	
		/*�˴���Ҫ���ж�io������Ϊ���뷽ʽ��������PB1��PB12�ⲿ�����������裬��stm32�У�
		��û��ָ�����뷽ʽ��Ĭ��Ϊ�������룬��ʱ���ⲿ��·������������Ҫ�������뷽ʽ--�����ĵ�����ע*/

	   //PB1���½��ش�����key2
		GPIO_EXTILineConfig(GPIO_PortSourceGPIOB,GPIO_PinSource1);
		EXTI_InitStructure.EXTI_Line=EXTI_Line1;	
		EXTI_InitStructure.EXTI_Mode = EXTI_Mode_Interrupt;	
		EXTI_InitStructure.EXTI_Trigger = EXTI_Trigger_Falling;
		EXTI_InitStructure.EXTI_LineCmd = ENABLE;
		EXTI_Init(&EXTI_InitStructure);	 	//����EXTI_InitStruct��ָ���Ĳ�����ʼ������EXTI�Ĵ���

		//PB12���½��ش���,key1
		GPIO_EXTILineConfig(GPIO_PortSourceGPIOB,GPIO_PinSource12); 
		EXTI_InitStructure.EXTI_Line=EXTI_Line12;
		EXTI_InitStructure.EXTI_Mode = EXTI_Mode_Interrupt;
		EXTI_InitStructure.EXTI_Trigger = EXTI_Trigger_Falling;
		EXTI_InitStructure.EXTI_LineCmd = ENABLE;
		EXTI_Init(&EXTI_InitStructure);		//����EXTI_InitStruct��ָ���Ĳ�����ʼ������EXTI�Ĵ���

		//����PB12�ж����ȼ�
		NVIC_InitStructure.NVIC_IRQChannel = EXTI15_10_IRQn;			//ʹ��PB12���ڵ��ⲿ�ж�ͨ��
		NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 6;		//��ռ���ȼ� 
		NVIC_InitStructure.NVIC_IRQChannelSubPriority = 0;				//�����ȼ�0
		NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;					//ʹ���ⲿ�ж�ͨ��
		NVIC_Init(&NVIC_InitStructure); 
		
		//����PB1�ж����ȼ�
		NVIC_InitStructure.NVIC_IRQChannel = EXTI1_IRQn;				//ʹ�ܰ���PB1���ڵ��ⲿ�ж�ͨ��
		NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 7;		//��ռ���ȼ�
		NVIC_InitStructure.NVIC_IRQChannelSubPriority = 0;				//�����ȼ� 
		NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;					//ʹ���ⲿ�ж�ͨ��
		NVIC_Init(&NVIC_InitStructure);  	  							//����NVIC_InitStruct��ָ���Ĳ�����ʼ������NVIC�Ĵ���
}

//�ⲿ�ж�0������� 
//void EXTI0_IRQHandler(void)
//{
//	delay_ms(50);//����
//	if(WK_UP==1)	 	 //WK_UP����
//	{				 
//		LED1=!LED1;	
//	}
//	EXTI_ClearITPendingBit(EXTI_Line0); //���LINE0�ϵ��жϱ�־λ  
//}
 
//�ⲿ�ж�4�������
//void EXTI1_IRQHandler(void)
//{
//	delay_ms(50);//����
//	LED0=!LED0;
//	EXTI_ClearITPendingBit(EXTI_Line9);
//	if(KEY0==1)	 //����KEY0
//	{
//		LED0=!LED0;
//	}		 
//	EXTI_ClearITPendingBit(EXTI_Line4);  //���LINE4�ϵ��жϱ�־λ  
//}

//void EXTI1_IRQHandler(void)//�ⲿ�ж�1
//{
//		delay_ms(50);
//		if(KEY0==0)
//			{
//				flag=1;
//			}
//		EXTI_ClearITPendingBit(EXTI_Line1);
//}


//void EXTI15_10_IRQHandler(void)//�ⲿ�ж�PB12
//{
//		delay_ms(50);
//	  if(KEY1==0)
//	    {
//				mark=1;
//			}
//				
//		EXTI_ClearITPendingBit(EXTI_Line12);
//}

 
