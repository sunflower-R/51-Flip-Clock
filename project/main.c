#include <reg52.h>
#include <intrins.h>

// 先引入i2c.h避免类型重定义
#include "i2c.h"
#include "ds1302.h"
#include "oledchar.h"
#include "key.h"
#include "motor.h"

// 声明ds1302的地址数组
extern uchar code WRITE_RTC_ADDR[7];
extern uchar code READ_RTC_ADDR[7];

// 时钟全局变量
bit SET_MODE = 0;          // 设置模式：0=正常，1=设置
uchar SET_ITEM = 0;        // 设置项：0=年，1=月，2=日，3=时，4=分，5=秒
bit BLINK_FLAG = 0;        // 闪烁标志
uint BLINK_CNT = 0;        // 闪烁计数
uchar singletime[13];      // 时间拆分数组

// 闹钟全局变量
bit ALARM_SET_MODE = 0;    // 闹钟设置模式：0=正常，1=设置
uchar ALARM_SET_ITEM = 0;  // 闹钟设置项：0=时，1=分
bit ALARM_ENABLED = 0;     // 闹钟启用标志：0=关闭，1=开启
uchar ALARM_HOUR = 0;      // 闹钟小时（默认0点）
uchar ALARM_MINUTE = 0;    // 闹钟分钟（默认0分）
bit ALARM_RINGING = 0;     // 闹钟响铃中标志
uint ALARM_BUZZER_CNT = 0; // 闹钟蜂鸣器计数器
uchar ALARM_STOPPED = 1;   // 闹钟响铃已停止（防止重复响铃）

// 步进延迟计数器
unsigned int delay_counter1 = 0;
unsigned int delay_counter2 = 0;

// 按键状态变量
unsigned char last_key_state = 0;     // 上次按键状态
unsigned char current_key_state = 0;  // 当前按键状态

// 函数声明
void display(void);
void Timer0_Init(void);
void Time_Set_Handle(uchar key);
void Motor_Continuous_Handle(void);  // 电机连续控制处理
void Alarm_Handle(uchar key);        // 闹钟处理函数
void Alarm_Display(void);            // 闹钟显示函数
void Alarm_Check(void);              // 闹钟检查函数

// ---------------------- 定时器0初始化（1ms定时，11.0592MHz晶振） ----------------------
void Timer0_Init(void)
{
    TMOD |= 0x01;         
    TH0 = 0xFC;     // 高8位 (11.0592MHz, 1ms定时)
    TL0 = 0x66;     // 低8位
    ET0 = 1;              
    EA = 1;               
    TR0 = 1;              
}

// ---------------------- 定时器0中断 ----------------------
void Timer0_ISR(void) interrupt 1
{
    // 重装初值
    TH0 = 0xFC;
    TL0 = 0x66;
    
    // 1. 处理时钟闪烁
    BLINK_CNT++;
    if(BLINK_CNT >= 1000)  // 1秒/次
    {
        BLINK_CNT = 0;
        BLINK_FLAG = !BLINK_FLAG;
    }
    
    // 2. 处理闹钟蜂鸣器音乐
    if(ALARM_RINGING)
    {
        ALARM_BUZZER_CNT++;
        
        // 播放简单的旋律（如"叮咚叮咚"）
        // 500Hz 250ms，然后750Hz 250ms，循环
        if((ALARM_BUZZER_CNT / 250) % 2 == 0)
        {
            // 500Hz：每1ms翻转一次（500Hz周期为2ms）
            if((ALARM_BUZZER_CNT % 2) == 0)
                BEEP = ~BEEP;
        }
        else
        {
            // 750Hz：每0.666ms翻转一次，近似处理
            if((ALARM_BUZZER_CNT % 1) == 0)  // 简化处理，实际需要更精确的计时
                BEEP = ~BEEP;
        }
    }
    else
    {
        BEEP = 1;  // 确保蜂鸣器停止
    }
    
    // 3. 处理电机自动控制（使用DS1302的实际时间触发）
    motor_auto_control();
    
    // 4. 处理电机连续旋转
    motor_process_continuous();
    
    // 5. 处理电机1步进（非连续模式）
    if(motor1_busy && !motor1_continuous_mode) {
        if(++delay_counter1 >= motor1_step_delay) {
            delay_counter1 = 0;
            motor1_step();
        }
    }
    
    // 6. 处理电机2步进（非连续模式）
    if(motor2_busy && !motor2_continuous_mode) {
        if(++delay_counter2 >= motor2_step_delay) {
            delay_counter2 = 0;
            motor2_step();
        }
    }
}

// ---------------------- 时间拆分 ----------------------
void display()
{     
    // 实时拆分TIME的BCD值到显示数组
    singletime[0] = TIME[6]/16;    // 年十位
    singletime[1] = TIME[6]%16;    // 年个位
    singletime[2] = TIME[4]/16;    // 月十位
    singletime[3] = TIME[4]%16;    // 月个位
    singletime[4] = TIME[3]/16;    // 日十位
    singletime[5] = TIME[3]%16;    // 日个位
    singletime[6] = TIME[2]/16;    // 时十位
    singletime[7] = TIME[2]%16;    // 时个位
    singletime[8] = TIME[1]/16;    // 分十位
    singletime[9] = TIME[1]%16;    // 分个位
    singletime[10] = TIME[0]/16;   // 秒十位
    singletime[11] = TIME[0]%16;   // 秒个位
}

// ---------------------- 时间设置处理 ----------------------
void Time_Set_Handle(uchar key)
{
    uchar n;
    if(!SET_MODE) return;
    
    switch(key)
    {
        case KEY_SWITCH:  
            SET_ITEM = (SET_ITEM + 1) % 6;  // 切换设置项
            break;
            
        case KEY_ADD:  
            switch(SET_ITEM)
            {
                case 0: // 年
                    TIME[6] = ((TIME[6] & 0xF0) + (((TIME[6] & 0x0F) + 1) % 10)) | 
                              ((TIME[6] & 0x0F) == 9 ? (TIME[6] + 0x10) & 0xF0 : TIME[6] & 0xF0);
                    break;
                case 1: // 月
                    TIME[4]++;
                    if((TIME[4] & 0x0F) > 9) TIME[4] += 0x07;
                    if(TIME[4] > 0x12) TIME[4] = 0x01;
                    break;
                case 2: // 日
                    TIME[3]++;
                    if((TIME[3] & 0x0F) > 9) TIME[3] += 0x07;
                    if(TIME[3] > 0x31) TIME[3] = 0x01;
                    break;
                case 3: // 时
                    TIME[2]++;
                    if((TIME[2] & 0x0F) > 9) TIME[2] += 0x07;
                    if(TIME[2] > 0x23) TIME[2] = 0x00;
                    break;
                case 4: // 分钟
                    TIME[1]++;
                    if((TIME[1] & 0x0F) > 9)
                        TIME[1] = (TIME[1] & 0xF0) + 0x10;
                    if(TIME[1] > 0x59)
                        TIME[1] = 0x00;
                    break;
                case 5: // 秒
                    TIME[0]++;
                    if((TIME[0] & 0x0F) > 9) TIME[0] = (TIME[0] & 0xF0) + 0x10;
                    if(TIME[0] > 0x59) TIME[0] = 0x00;
                    break;
            }
            display();
            break;
            
        case KEY_SUB:  
            switch(SET_ITEM)
            {
                case 0: // 年
                    TIME[6] = ((TIME[6] & 0xF0) + (((TIME[6] & 0x0F) - 1 + 10) % 10)) | 
                              ((TIME[6] & 0x0F) == 0 ? (TIME[6] - 0x10) & 0xF0 : TIME[6] & 0xF0);
                    break;
                case 1: // 月
                    TIME[4]--;
                    if((TIME[4] & 0x0F) < 0) TIME[4] = (TIME[4] & 0xF0) + 0x09;
                    if(TIME[4] < 0x01) TIME[4] = 0x12;
                    break;
                case 2: // 日
                    TIME[3]--;
                    if((TIME[3] & 0x0F) < 0) TIME[3] = (TIME[3] & 0xF0) + 0x09;
                    if(TIME[3] < 0x01) TIME[3] = 0x31;
                    break;
                case 3: // 时
                    TIME[2]--;
                    if((TIME[2] & 0x0F) < 0) TIME[2] = (TIME[2] & 0xF0) + 0x09;
                    if(TIME[2] > 0x23) TIME[2] = 0x23;
                    break;
                case 4: // 分钟
                    TIME[1]--;
                    if((TIME[1] & 0x0F) > 9 || (TIME[1] & 0x0F) < 0)
                        TIME[1] = (TIME[1] & 0xF0) - 0x10 + 0x09;
                    if(TIME[1] > 0x59 || TIME[1] < 0x00)
                        TIME[1] = 0x59;
                    break;
                case 5: // 秒
                    TIME[0]--;
                    if((TIME[0] & 0x0F) > 9 || (TIME[0] & 0x0F) < 0)
                        TIME[0] = (TIME[0] & 0xF0) - 0x10 + 0x09;
                    if(TIME[0] > 0x59 || TIME[0] < 0x00)
                        TIME[0] = 0x59;
                    break;
            }
            display();
            break;
            
        case KEY_SAVE:  
            Ds1302Write(0x8E, 0X00);  
            for(n=0; n<7; n++)
            {
                Ds1302Write(WRITE_RTC_ADDR[n], TIME[n]);
            }
            Ds1302Write(0x8E, 0x80);  
            SET_MODE = 0;
            Ds1302ReadTime();
            display();
            break;
    }
}

// ---------------------- 闹钟处理函数 ----------------------
void Alarm_Handle(uchar key)
{
    if(key == KEY_ALARM_SET)
    {
        ALARM_SET_MODE = !ALARM_SET_MODE;
        if(ALARM_SET_MODE)
        {
            // 进入闹钟设置模式时，重置设置项
            ALARM_SET_ITEM = 0;
        }
    }
    
    // 处理闹钟开关/停止响铃
    if(key == KEY_ALARM_TOGGLE)
    {
        if(ALARM_RINGING)
        {
            // 停止响铃
            ALARM_RINGING = 0;
            ALARM_STOPPED = 1;
            BEEP = 1;  // 确保蜂鸣器停止
        }
        else
        {
            // 切换闹钟开关状态
            ALARM_ENABLED = !ALARM_ENABLED;
            ALARM_STOPPED = 0;  // 重置停止标志
        }
    }
    
    if(!ALARM_SET_MODE) return;
    
    switch(key)
    {
        case KEY_SWITCH:  // 使用切换键在闹钟设置项间切换
            ALARM_SET_ITEM = (ALARM_SET_ITEM + 1) % 2;
            break;
            
        case KEY_ALARM_HOUR:  // 增加小时或分钟
            if(ALARM_SET_ITEM == 0)
            {
                ALARM_HOUR++;
                if(ALARM_HOUR > 23) ALARM_HOUR = 0;
            }
            else if(ALARM_SET_ITEM == 1)  // 增加分钟
            {
                ALARM_MINUTE++;
                if(ALARM_MINUTE > 59) ALARM_MINUTE = 0;
            }
            break;
            
        case KEY_ALARM_MINUTE:  // 减小小时或分钟
            if(ALARM_SET_ITEM == 0)
            {
                if(ALARM_HOUR == 0)
                    ALARM_HOUR = 23;
                else
                    ALARM_HOUR--;
            }
            else if(ALARM_SET_ITEM == 1)
            {
                if(ALARM_MINUTE == 0)
                    ALARM_MINUTE = 59;
                else
                    ALARM_MINUTE--;
            }
            break;
    }
}

// ---------------------- 闹钟检查函数 ----------------------
void Alarm_Check(void)
{
    // 获取当前时间（从TIME数组中，注意是BCD码）
    uchar current_hour = (TIME[2] >> 4) * 10 + (TIME[2] & 0x0F);
    uchar current_minute = (TIME[1] >> 4) * 10 + (TIME[1] & 0x0F);
    
    // 检查闹钟是否启用且未停止且未在响铃
    if(ALARM_ENABLED && !ALARM_STOPPED && !ALARM_RINGING)
    {
        // 检查当前时间是否与闹钟时间匹配
        if(current_hour == ALARM_HOUR && current_minute == ALARM_MINUTE)
        {
            ALARM_RINGING = 1;
            ALARM_BUZZER_CNT = 0;
        }
    }
}

// ---------------------- 闹钟显示函数 ----------------------
void Alarm_Display(void)
{
    uchar i, j;
    uchar alarm_hour_tens, alarm_hour_ones;
    uchar alarm_minute_tens, alarm_minute_ones;
    
    // 计算闹钟时间的十位和个位
    alarm_hour_tens = ALARM_HOUR / 10;
    alarm_hour_ones = ALARM_HOUR % 10;
    alarm_minute_tens = ALARM_MINUTE / 10;
    alarm_minute_ones = ALARM_MINUTE % 10;
    
    // 显示闹钟开关状态 (0或1)
    oled_put_char_16x16(0, 3, 0);  // 第4行，第0列，8x16字符
    if(ALARM_ENABLED)
    {
        // 显示"1"
        for(j=0; j<16; j++) oled_Write_data(number[16][j]);
    }
    else
    {
        // 显示"0"
        for(j=0; j<16; j++) oled_Write_data(number[15][j]);
    }
    
    // 显示空格
    oled_put_char_16x16(3, 3, 0);
    for(j=0; j<16; j++) oled_Write_data(0x00);
    
    // 显示闹钟时间 "HH:MM"
    if(ALARM_SET_MODE && BLINK_FLAG && ALARM_SET_ITEM == 0)
    {
        // 小时闪烁时清空小时显示
        oled_put_char_16x16(2, 3, 0);
        for(j=0; j<16; j++) oled_Write_data(0x00);
        oled_put_char_16x16(3, 3, 0);
        for(j=0; j<16; j++) oled_Write_data(0x00);
    }
    else
    {
        // 显示小时
        oled_put_char_16x16(2, 3, 0);
        for(j=0; j<16; j++) oled_Write_data(number[15+alarm_hour_tens][j]);
        oled_put_char_16x16(3, 3, 0);
        for(j=0; j<16; j++) oled_Write_data(number[15+alarm_hour_ones][j]);
    }
    
    // 显示冒号
    oled_put_char_16x16(4, 3, 0);
    for(j=0; j<16; j++) oled_Write_data(number[26][j]);  // ':'
    
    if(ALARM_SET_MODE && BLINK_FLAG && ALARM_SET_ITEM == 1)
    {
        // 分钟闪烁时清空分钟显示
        oled_put_char_16x16(5, 3, 0);
        for(j=0; j<16; j++) oled_Write_data(0x00);
        oled_put_char_16x16(6, 3, 0);
        for(j=0; j<16; j++) oled_Write_data(0x00);
    }
    else
    {
        // 显示分钟
        oled_put_char_16x16(5, 3, 0);
        for(j=0; j<16; j++) oled_Write_data(number[15+alarm_minute_tens][j]);
        oled_put_char_16x16(6, 3, 0);
        for(j=0; j<16; j++) oled_Write_data(number[15+alarm_minute_ones][j]);
    }
    
 
}

// ---------------------- 电机连续控制处理（按住旋转） ----------------------
void Motor_Continuous_Handle(void)
{
    if(SET_MODE || ALARM_SET_MODE) {
        // 设置模式下，停止所有连续旋转
        motor1_stop_continuous();
        motor2_stop_continuous();
        return;
    }
    
    // 检测当前按键状态（电机控制键）
    current_key_state = Key_Hold_Scan();
    
    // 处理按键状态变化
    if(current_key_state != last_key_state) {
        // 按键状态变化，处理按键释放
        switch(last_key_state) {
            case KEY_MINUTE_FORWARD:
            case KEY_MINUTE_BACKWARD:
                motor2_stop_continuous();  // 停止分钟电机
                break;
            case KEY_HOUR_FORWARD:
            case KEY_HOUR_BACKWARD:
                motor1_stop_continuous();  // 停止时钟电机
                break;
        }
        
        // 处理新按键按下
        switch(current_key_state) {
            case KEY_MINUTE_FORWARD:  // 分钟前进
                motor2_start_continuous(1);  // 正转
                break;
            case KEY_MINUTE_BACKWARD:  // 分钟后退
                motor2_start_continuous(0);  // 反转
                break;
            case KEY_HOUR_FORWARD:  // 时钟前进
                motor1_start_continuous(1);  // 反转前进
                break;
            case KEY_HOUR_BACKWARD:  // 时钟后退
                motor1_start_continuous(0);  // 正转后退
                break;
        }
        
        // 更新按键状态
        last_key_state = current_key_state;
    }
    else if(current_key_state == 0) {
        // 没有按键被按住，确保电机停止
        if(motor1_continuous_mode) motor1_stop_continuous();
        if(motor2_continuous_mode) motor2_stop_continuous();
        last_key_state = 0;
    }
}

// ---------------------- 主函数 ----------------------
void main(void)
{
    uchar i, j, k;
    uchar key_num;
    
    // 初始化
    oled_initial();     
    Ds1302Init();       
    Timer0_Init();
    motor_init();       // 电机初始化
    
    // 初始拆分时间
    Ds1302ReadTime();
    display();

    // 静态显示框架
    // 1. 简易时钟标题
    for(i=2; i<6; i++)
    {
        oled_put_char_16x16(i, 0, 1);
        for(j=0; j<32; j++)
        {
            oled_Write_data(zifu[i-2][j]);
        }
    }
     
    // 2. 年月日标题
    for(i=0; i<3; i++)
    {
        oled_put_char_16x16(2+2*i, 1, 1);
        for(j=0; j<32; j++)
        {
            oled_Write_data(zifu[i+4][j]);
        }
    }

    // 3. 冒号（时分、分秒）
    for(i=0; i<2; i++)
    {
        oled_put_char_16x16(6+3*i, 2, 0);
        for(j=0; j<16; j++)
        {
            oled_Write_data(number[25][j]);
        }
    }

    // 4. 年份前两位（20）
    for(i=0; i<2; i++)
    {
        oled_put_char_16x16(i, 1, 0);
        for(j=0; j<16; j++)
        {
            oled_Write_data(number[17-2*i][j]);
        }
    }

    // 主循环
    while(1)
    {
        // 1. 实时扫描键盘
        key_num = Key_Scan();
        
        // 2. 进入/退出时钟设置模式
        if(key_num == KEY_SET_MODE)
        {
            SET_MODE = !SET_MODE;
            SET_ITEM = 0;
            BEEP_Alert();
            display();
        }
        
        // 3. 处理时钟设置逻辑
        Time_Set_Handle(key_num);
        
        // 4. 处理闹钟逻辑
        Alarm_Handle(key_num);
        
        // 5. 检查闹钟
        Alarm_Check();
        
        // 6. 处理电机连续控制（按住旋转）
        Motor_Continuous_Handle();
        
        // 7. 非时钟设置模式：实时读取DS1302时间
        if(!SET_MODE)
        {
            Ds1302ReadTime();
            display();
        }

        // 8. 动态显示
        if(SET_MODE && BLINK_FLAG)
        {
            // 闪烁时清空当前项
            switch(SET_ITEM)
            {
                case 0: // 年
                    oled_put_char_16x16(2,1,0);oled_put_char_16x16(3,1,0);
                    for(j=0;j<16;j++){oled_Write_data(0x00);}
                    for(j=0;j<16;j++){oled_Write_data(0x00);}
                    break;
                case 1: // 月
                    oled_put_char_16x16(6,1,0);oled_put_char_16x16(7,1,0);
                    for(j=0;j<16;j++){oled_Write_data(0x00);}
                    for(j=0;j<16;j++){oled_Write_data(0x00);}
                    break;
                case 2: // 日
                    oled_put_char_16x16(10,1,0);oled_put_char_16x16(11,1,0);
                    for(j=0;j<16;j++){oled_Write_data(0x00);}
                    for(j=0;j<16;j++){oled_Write_data(0x00);}
                    break;
                case 3: // 时
                    oled_put_char_16x16(4,2,0);oled_put_char_16x16(5,2,0);
                    for(j=0;j<16;j++){oled_Write_data(0x00);}
                    for(j=0;j<16;j++){oled_Write_data(0x00);}
                    break;
                case 4: // 分
                    oled_put_char_16x16(10,2,0);oled_put_char_16x16(11,2,0);
                    for(j=0;j<16;j++){oled_Write_data(0x00);}
                    for(j=0;j<16;j++){oled_Write_data(0x00);}
                    break;
                case 5: // 秒
                    oled_put_char_16x16(16,2,0);oled_put_char_16x16(17,2,0);
                    for(j=0;j<16;j++){oled_Write_data(0x00);}
                    for(j=0;j<16;j++){oled_Write_data(0x00);}
                    break;
            }
        }
        else
        {
            // 正常显示：实时刷新所有时间项
            // 年月日
            for(k=0;k<3;k++)
            {
                for(i=0;i<2;i++)
                {
                    oled_put_char_16x16(i+2+4*k,1,0);
                    for(j=0;j<16;j++)
                    { 
                        oled_Write_data(number[15+singletime[i+2*k]][j]);
                    }
                }
            }
            // 时分秒
            for(k=0;k<3;k++)
            {
                for(i=0;i<2;i++)
                {
                    oled_put_char_16x16(i+4+3*k,2,0);
                    for(j=0;j<16;j++)
                    { 
                        oled_Write_data(number[15+singletime[i+2*k+6]][j]);
                    }
                }
            }
        }
        
        // 9. 显示闹钟
        Alarm_Display();
        
        // 10. 空闲时进入省电模式
        PCON |= 0x01;  // 进入IDLE模式
        _nop_();
        _nop_();
    }
}