#include "stm32f10x.h"                  // Device header
#include "Delay.h"
#include "Serial.h"
#include "LED.h"
#include "string.h"

int main(void)
{
	/*模块初始化*/
	LED_Init();			//LED初始化
	Serial_Init();		//串口初始化
	
	
	for (uint8_t i = 0; i < 3; i++)
	{
		Delay_ms(500);
		LED1_ON();
		Delay_ms(500);
		LED1_OFF();
	}
	
	
	while (1)
	{
		// Serial_SendString("@TEST\r\n");
		if (Serial_RxFlag == 1)		//如果接收到数据包
		{
			
			/*将收到的数据包与预设的指令对比，以此决定将要执行的操作*/
			if (strcmp(Serial_RxPacket, "LED_ON") == 0)			//如果收到LED_ON指令
			{
				LED1_ON();										//点亮LED
				Serial_SendString("@LED_ON_OK\r\n");				//串口回传一个字符串LED_ON_OK
			}
			else if (strcmp(Serial_RxPacket, "LED_OFF") == 0)	//如果收到LED_OFF指令
			{
				LED1_OFF();										//熄灭LED
				Serial_SendString("@LED_OFF_OK\r\n");			//串口回传一个字符串LED_OFF_OK
			}
			else						//上述所有条件均不满足，即收到了未知指令
			{
				Serial_SendString("@ERROR_COMMAND\r\n");			//串口回传一个字符串ERROR_COMMAND
			}
			
			Serial_RxFlag = 0;			//处理完成后，需要将接收数据包标志位清零，否则将无法接收后续数据包
		}
	}
}
