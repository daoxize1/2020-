#ifndef __PID_H
#define __PID_H	 
#include "sys.h"

void PID_init(void);
u16 PID_realize(u16 PWMValue,u16 rpm,u16 Setrpm);

typedef struct {
    u16 PWMValue;         //�����趨ֵ
    u16 ActualPWMValue;      //����ʵ��ֵ
    u16 err;              //����ƫ��ֵ
    u16 err_last;         //������һ��ƫ��ֵ
    float Kp,Ki,Kd;         //������������֡�΢��ϵ��
    u16 Setrpm;              //�����趨ֵ
    u16 Actualrpm;
    float integral;         //�������ֵ
}pidvalue;

#endif
