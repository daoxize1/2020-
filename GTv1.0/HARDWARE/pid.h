#ifndef __PID_H
#define __PID_H	 
#include "sys.h"

void PID_init(void);
u16 PID_realize(u16 PWMValue,u16 rpm,u16 Setrpm);

typedef struct {
    u16 PWMValue;         //定义设定值
    u16 ActualPWMValue;      //定义实际值
    u16 err;              //定义偏差值
    u16 err_last;         //定义上一个偏差值
    float Kp,Ki,Kd;         //定义比例、积分、微分系数
    u16 Setrpm;              //定义设定值
    u16 Actualrpm;
    float integral;         //定义积分值
}pidvalue;

#endif
