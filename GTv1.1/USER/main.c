#include "led.h"
#include "key.h"
#include "delay.h"
#include "sys.h"
#include "timer.h"
#include "exti.h"
#include "usart.h"
#include "pid.h"
#include "lcd.h"
#include "24cxx.h"
#include "w25qxx.h"
#include "touch.h"
#include "string.h"
#include "math.h"
#include "stmflash.h"

#define FLASH_SAVE_ADDR  0X08070000

void LCD_ShowButton(u8 icon);
void LCD_Refresh(void);
void detect_touch(void);
void LCD_ShowData(void);
void Usart_Change_Parameter(void);
void Check_the_reset_button(void);
float string_to_float(u8 *str,u8 len);
extern u8 flag_10ms;
extern u8 flag_20ms;
extern u16 rpm;
extern u16 Setrpm;
extern float Kp_buff,Ki_buff,Kd_buff;
extern pidvalue pid;

u8 icon = 0;
u16 Setrpm_buff;
u8 PID_Parameters[4][4]={"Kp","Ki","Kd","rpm"};
u16 PWMValue = 0;
u8 touched = 0;
u8 n20ms = 0;
u8 x0=0;
u8 y0=150;
u8 y1=0;
u8 step = 0;
u8 parameter;
u16 u16Kp,u16Ki,u16Kd;
int main()
{
	
	u8 lcd_id[12];			//存放LCD ID字符串
	delay_init();
	NVIC_PriorityGroupConfig(NVIC_PriorityGroup_2);//设置NVIC中断分组2:2位抢占优先级，2位响应优先级
	LED_Init();
	EXTIX_Init();
	uart_init(115200);
	PID_init();
	KEY_Init();	
	//先初始化LCD再运行系统
	sprintf((char*)lcd_id,"LCD ID:%04X",lcddev.id);//将LCD ID打印到lcd_id数组。
	LCD_Init();
	LCD_Clear(WHITE);
	tp_dev.init();
	POINT_COLOR = RED;
	LCD_DrawLine(0, 151, 240, 151);   //画rpm=0时的参考线
	POINT_COLOR = BLACK;		
	LCD_SSD_BackLightSet(100);			//LCD背光100%
	LCD_ShowButton(icon);

	Setrpm_buff = Setrpm;
	TIM2_Int_Init(1000,71);// 定时1000us
	TIM3_PWM_Init(59999,0); //不分频。PWM频率=72000000/60000=1200hz
	TIM_SetCompare2(TIM3,PWMValue);

	LCD_Refresh();
	while(1)
	{
		if(flag_20ms == 1)
		{
			flag_20ms = 0;
			LCD_Refresh();
			LCD_ShowData();
			if(touched == 0)
			{
				detect_touch();
				n20ms = 0;
			}
			else
				n20ms++;
			if(n20ms == 20)
				touched = 0;		//类似消抖
			
		}
		if(USART_RX_STA&0x8000)
		{					   
			Usart_Change_Parameter();
		}
		Check_the_reset_button();	
	}
}

void Check_the_reset_button()		//恢复默认值（Kp=4,Ki=1,Kd=0.01,rpm=10000）
{
	u8 key;
	key=KEY_Scan(0);
	if(key==WKUP_PRES)	//KEY1按下,写入STM32 FLASH
	{
		Kp_buff=4.0;pid.Kp=Kp_buff;
		u16Kp=(u16)(Kp_buff*100);STMFLASH_Write(FLASH_SAVE_ADDR,&u16Kp,1);
		Ki_buff=1.0;pid.Ki=Ki_buff;
		u16Ki=(u16)(Ki_buff*100);STMFLASH_Write(FLASH_SAVE_ADDR+2,&u16Ki,1);
		Kd_buff=0.01;pid.Kd=Kd_buff;
		u16Kd=(u16)(Kd_buff*100);STMFLASH_Write(FLASH_SAVE_ADDR+4,&u16Kd,1);
		Setrpm_buff=10000;Setrpm=Setrpm_buff;
		STMFLASH_Write(FLASH_SAVE_ADDR+6,&Setrpm,1);
					
	}
}

void Usart_Change_Parameter()
{
	u8 len;
	len=USART_RX_STA&0x3fff;//得到此次接收到的数据长度
	if(step == 0)
	{
		printf("\r\n请选择需要更改的参数\r\n");
		printf("1.Kp 2.Ki 3.Kd 4.Setrpm\r\n");
		step++;
	}
	else if(step == 1)
	{
		switch(USART_RX_BUF[0])
		{
		case '1':parameter = 1;printf("请输入更改后的值\r\n");break;
		case '2':parameter = 2;printf("请输入更改后的值\r\n");break;
		case '3':parameter = 3;printf("请输入更改后的值\r\n");break;
		case '4':parameter = 4;printf("请输入更改后的值\r\n");break;
		}
		step++;
	}
	else if(step == 2)
	{
		switch (parameter)
		{
			case 1:Kp_buff=string_to_float(USART_RX_BUF,len);pid.Kp=Kp_buff;
					u16Kp=(u16)(Kp_buff*100);STMFLASH_Write(FLASH_SAVE_ADDR,&u16Kp,1);break;
			case 2:Ki_buff=string_to_float(USART_RX_BUF,len);pid.Ki=Ki_buff;
					u16Ki=(u16)(Ki_buff*100);STMFLASH_Write(FLASH_SAVE_ADDR+2,&u16Ki,1);break;
			case 3:Kd_buff=string_to_float(USART_RX_BUF,len);pid.Kd=Kd_buff;
					u16Kd=(u16)(Kd_buff*100);STMFLASH_Write(FLASH_SAVE_ADDR+4,&u16Kd,1);break;
			case 4:Setrpm_buff=(u16)string_to_float(USART_RX_BUF,len);Setrpm=Setrpm_buff;
					STMFLASH_Write(FLASH_SAVE_ADDR+6,&Setrpm,1);break;
		}
		printf("修改完成\r\n");
		step = 0;
	}
	printf("\r\n\r\n");//插入换行
	USART_RX_STA=0;
}

void LCD_Refresh()
{
	if(x0 >= 239)			//到屏幕边缘清屏
	{
		x0 = 0;
		LCD_Fill(0, 0, 240, 150, WHITE);
	}
	y1 = 150-rpm/100;
	if(y1>150)
		y1 = 150;
	LCD_DrawLine(x0, y0, x0+1, y1);
	x0 = x0 + 2;
	y0 = y1;			   		 
	
}


void LCD_ShowButton(u8 icon)
{
		LCD_ShowString(20,  280, 60,  60, 24,"+");
		LCD_ShowString(80, 280, 60,  60, 24,"-");
		LCD_ShowString(140, 280, 60,  60, 24,"OK");
		LCD_ShowString(200, 280, 60,  60, 24,PID_Parameters[icon]);

		LCD_DrawLine(0,260,240,260);
		LCD_DrawLine(60,260,60,320);
		LCD_DrawLine(120,260,120,320);
		LCD_DrawLine(180,260,180,320);

}
void LCD_ShowData()
{
	LCD_ShowString(10, 160, 30,  60, 16,"rpm=");
	LCD_ShowxNum(40,160,rpm,5,16,0x40);

	LCD_ShowString(10, 180, 60,  60, 16,"Setrpm=");
	LCD_ShowxNum(70,180,Setrpm_buff,5,16,0x40);

	LCD_ShowString(10,200,30,60,16,"Kp=");
	LCD_ShowxNum(40,200,(int)Kp_buff,2,16,0x40);
	LCD_ShowChar(60,200,'.',16,0);
	LCD_ShowxNum(62,200,(int)(Kp_buff*100)%100,2,16,0xc0);
	
	LCD_ShowString(10,220,30,60,16,"Ki=");
	LCD_ShowxNum(40,220,(int)Ki_buff,2,16,0x40);
	LCD_ShowChar(60,220,'.',16,0);
	LCD_ShowxNum(62,220,(int)(Ki_buff*100)%100,2,16,0xc0);



	LCD_ShowString(10,240,30,60,16,"Kd=");
	LCD_ShowxNum(40,240,(int)Kd_buff,2,16,0x40);
	LCD_ShowChar(60,240,'.',16,0);
	LCD_ShowxNum(62,240,(int)(Kd_buff*100)%100,2,16,0xc0);
}




void detect_touch()
{
	tp_dev.scan(0); 		 
	if(tp_dev.sta&TP_PRES_DOWN)			//触摸屏被按下
	{	
		if(tp_dev.x[0]<lcddev.width&&tp_dev.y[0]<lcddev.height)
		{	
			if(tp_dev.x[0]<60 && tp_dev.x[0]>0 && tp_dev.y[0]<320 && tp_dev.y[0]>260 )
			{
				switch (icon)
				{
				case 0:
					Kp_buff+=0.1;
					LCD_Fill(92 ,207,94,209,BLACK);
					break;
				case 1:
					Ki_buff+=0.01;
					LCD_Fill(92 ,227,94,229,BLACK);
					break;
				case 2:
					Kd_buff+=0.01;
					LCD_Fill(92 ,247,94,249,BLACK);
					break;
				case 3:
					if(Setrpm_buff<13000)
						Setrpm_buff+=1000;
					LCD_Fill(112 ,187,114,189,BLACK);			//这个点用于表示当前变量处于变更状态
					break;
				default:
					break;
				}
			}


			if(tp_dev.x[0]<120 && tp_dev.x[0]>60 && tp_dev.y[0]<320 && tp_dev.y[0]>260 )
			{
				switch (icon)
				{
				case 0:
					Kp_buff-=0.1;
					LCD_Fill(92 ,207,94,209,BLACK);
					break;
				case 1:
					Ki_buff-=0.01;
					LCD_Fill(92 ,227,94,229,BLACK);
					break;
				case 2:
					Kd_buff-=0.01;
					LCD_Fill(92 ,247,94,249,BLACK);
					break;
				case 3:
					if(Setrpm_buff>6000)
						Setrpm_buff-=1000;
					LCD_Fill(112 ,187,114,189,BLACK);
					break;
				default:
					break;
				}
			}

			if(tp_dev.x[0]<180 && tp_dev.x[0]>120 && tp_dev.y[0]<320 && tp_dev.y[0]>260)
			{

				pid.Kp = Kp_buff;
				pid.Kp = Kp_buff;
				pid.Kp = Kp_buff;
				Setrpm = Setrpm_buff;
				LCD_Fill(92 ,207,94,209,WHITE);				//清除变更标志
				LCD_Fill(92 ,227,94,229,WHITE);
				LCD_Fill(92 ,247,94,249,WHITE);
				LCD_Fill(112 ,187,114,189,WHITE);
				u16Kp=(u16)(Kp_buff*100);
				STMFLASH_Write(FLASH_SAVE_ADDR,&u16Kp,1);
				u16Ki=(u16)(Ki_buff*100);
				STMFLASH_Write(FLASH_SAVE_ADDR,&u16Ki,1);
				u16Kd=(u16)(Kd_buff*100);
				STMFLASH_Write(FLASH_SAVE_ADDR,&u16Kd,1);
				STMFLASH_Write(FLASH_SAVE_ADDR+6,&Setrpm,1);
				if(STMFLASH_ReadHalfWord(0X08070000+6)==Setrpm)
				LCD_ShowString(140, 170, 60,  60, 24,"OK");
				
			}

			
			if(tp_dev.x[0]<240 && tp_dev.x[0]>180 && tp_dev.y[0]<320 && tp_dev.y[0]>260)
			{
				icon++;
				if (icon >= 4)
				{
					icon = 0;
				}
				LCD_Fill(181,261,239,319,WHITE);
				LCD_ShowButton(icon);
			}
			touched = 1;
		}
	}
}

float string_to_float(u8 *str,u8 len)
{
    int i=0,sign;
    double integer=0;
    double decimal=0;
    int dot = len;
    if((unsigned)(str[i])==45)
    {
        sign=-1;
        i++;
    }
    else 
        sign=1;
    
    for(;i<len;i++)
    {
        unsigned one=(unsigned)(str[i]);
        if(one>=48&&one<=57)
            if(i<dot)
                integer=(one-48)+integer*10;
            else
                decimal=decimal+(one-48)/pow(10,i-dot);
        else
            if(one==46)
                dot=i;
    }
    return sign*(integer+decimal);
}


