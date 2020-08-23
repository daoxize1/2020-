#include "pid.h"
#include "stmflash.h"
float Kp_buff = 0;
float Ki_buff = 0;
float Kd_buff = 0;
u16 Setrpm = 10000;
pidvalue pid;


void PID_init(void)
{
    pid.PWMValue=0.0;
    pid.ActualPWMValue=0.0;
    pid.err=0.0;
    pid.err_last=0.0;
    pid.Setrpm=0.0;
    pid.Actualrpm=0.0;
    pid.integral=0.0;
    pid.Kp=STMFLASH_ReadHalfWord(0X08070000)*1.0/100;
    pid.Ki=STMFLASH_ReadHalfWord(0X08070000+2)*1.0/100;
    pid.Kd=STMFLASH_ReadHalfWord(0X08070000+4)*1.0/100;
    Setrpm = STMFLASH_ReadHalfWord(0X08070000+6);
    Kp_buff=pid.Kp;
    Ki_buff=pid.Ki;
    Kd_buff=pid.Kd;
    
}

u16 PID_realize(u16 PWMValue,u16 rpm,u16 Setrpm)
{
    pid.PWMValue = PWMValue;
    pid.err = Setrpm - rpm;
    pid.integral += pid.err;
    pid.PWMValue = (u16)(pid.Kp * pid.err + pid.Ki * pid.integral + pid.Kd *(pid.err - pid.err_last));
    pid.err_last = pid.err;
    return pid.PWMValue;
}
