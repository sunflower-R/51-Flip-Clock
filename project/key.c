#include "key.h"

// 延时函数（消抖用，1ms@11.0592MHz）
void Key_Delay(unsigned int ms)
{
    unsigned int i, j;
    for(i = ms; i > 0; i--)
        for(j = 110; j > 0; j--);
}

// 蜂鸣器短鸣提示
void BEEP_Alert(void)
{
    BEEP = 0;    // 蜂鸣器响（低电平有效）
    Key_Delay(100);
    BEEP = 1;    // 蜂鸣器停
    Key_Delay(100);
}

// 4×4矩阵键盘扫描（返回键值，支持持续按住检测）
unsigned char Key_Scan(void)
{
    unsigned char key_val = KEY_NONE;
    static unsigned char last_key = KEY_NONE;
    
    // 第一步：空闲时所有行置高
    KEY_ROW1 = 1;
    KEY_ROW2 = 1;
    KEY_ROW3 = 1;
    KEY_ROW4 = 1;
    
    // ---------------------- 扫描行1（P17） ----------------------
    KEY_ROW1 = 0;  // 拉低行1
    Key_Delay(1);  // 稳定电平
    
    if(KEY_COL1 == 0)  // 行1列1 → 按键1
    {
        Key_Delay(10);
        if(KEY_COL1 == 0)
        {
            while(KEY_COL1 == 0);  // 等待释放
            Key_Delay(10);
            BEEP_Alert();
            key_val = KEY_SET_MODE;
        }
    }
    if(KEY_COL2 == 0)  // 行1列2 → 按键2
    {
        Key_Delay(10);
        if(KEY_COL2 == 0)
        {
            while(KEY_COL2 == 0);
            Key_Delay(10);
            BEEP_Alert();
            key_val = KEY_SWITCH;
        }
    }
    if(KEY_COL3 == 0)  // 行1列3 → 按键3
    {
        Key_Delay(10);
        if(KEY_COL3 == 0)
        {
            while(KEY_COL3 == 0);
            Key_Delay(10);
            BEEP_Alert();
            key_val = KEY_ADD;
        }
    }
    if(KEY_COL4 == 0)  // 行1列4 → 按键4
    {
        Key_Delay(10);
        if(KEY_COL4 == 0)
        {
            while(KEY_COL4 == 0);
            Key_Delay(10);
            BEEP_Alert();
            key_val = KEY_SUB;
        }
    }
    KEY_ROW1 = 1;
    
    // ---------------------- 扫描行2（P16） ----------------------
    KEY_ROW2 = 0;
    Key_Delay(1);
    
    if(KEY_COL1 == 0)  // 行2列1 → 按键5
    {
        Key_Delay(10);
        if(KEY_COL1 == 0)
        {
            while(KEY_COL1 == 0);
            Key_Delay(10);
            BEEP_Alert();
            key_val = KEY_SAVE;
        }
    }
    if(KEY_COL2 == 0)  // 行2列2 → 按键6
    {
        Key_Delay(10);
        if(KEY_COL2 == 0)
        {
            // 这里不等待释放，改为直接返回
            Key_Delay(10);
            BEEP_Alert();
            key_val = KEY_MINUTE_FORWARD;
        }
    }
    if(KEY_COL3 == 0)  // 行2列3 → 按键7
    {
        Key_Delay(10);
        if(KEY_COL3 == 0)
        {
            Key_Delay(10);
            BEEP_Alert();
            key_val = KEY_MINUTE_BACKWARD;
        }
    }
    if(KEY_COL4 == 0)  // 行2列4 → 按键8
    {
        Key_Delay(10);
        if(KEY_COL4 == 0)
        {
            Key_Delay(10);
            BEEP_Alert();
            key_val = KEY_HOUR_FORWARD;
        }
    }
    KEY_ROW2 = 1;
    
    // ---------------------- 扫描行3（P15） ----------------------
    KEY_ROW3 = 0;
    Key_Delay(1);
    
    if(KEY_COL1 == 0)  // 行3列1 → 按键9
    {
        Key_Delay(10);
        if(KEY_COL1 == 0)
        {

            Key_Delay(10);
            BEEP_Alert();
            key_val = KEY_HOUR_BACKWARD;
        }
    }
    if(KEY_COL2 == 0)  // 行3列2 → 按键10（闹钟设置）
    {
        Key_Delay(10);
        if(KEY_COL2 == 0)
        {
            while(KEY_COL2 == 0);
            Key_Delay(10);
            BEEP_Alert();
            key_val = KEY_ALARM_SET;
        }
    }
    if(KEY_COL3 == 0)  // 行3列3 → 按键11（闹钟小时调节）
    {
        Key_Delay(10);
        if(KEY_COL3 == 0)
        {
            while(KEY_COL3 == 0);
            Key_Delay(10);
            BEEP_Alert();
            key_val = KEY_ALARM_HOUR;
        }
    }
    if(KEY_COL4 == 0)  // 行3列4 → 按键12（闹钟分钟调节）
    {
        Key_Delay(10);
        if(KEY_COL4 == 0)
        {
            while(KEY_COL4 == 0);
            Key_Delay(10);
            BEEP_Alert();
            key_val = KEY_ALARM_MINUTE;
        }
    }
    KEY_ROW3 = 1;
    
    // ---------------------- 扫描行4（P14） ----------------------
    KEY_ROW4 = 0;
    Key_Delay(1);
    
    if(KEY_COL1 == 0)  // 行4列1 → 按键13（闹钟开关/停止响铃）
    {
        Key_Delay(10);
        if(KEY_COL1 == 0)
        {
            while(KEY_COL1 == 0);
            Key_Delay(10);
            BEEP_Alert();
            key_val = KEY_ALARM_TOGGLE;
        }
    }
    // 按键14-16预留
    KEY_ROW4 = 1;
    
    // 记录最后一次按键
    if(key_val != KEY_NONE) {
        last_key = key_val;
    }
    
    return key_val;
}

// 新增：检测按键是否被持续按住（用于电机控制）
unsigned char Key_Hold_Scan(void)
{
    unsigned char key_val = KEY_NONE;
    
    // 只扫描电机控制按键（行2和行3）
    
    // ---------------------- 扫描行2（P16） ----------------------
    KEY_ROW1 = 1;
    KEY_ROW2 = 0;  // 只拉低行2
    KEY_ROW3 = 1;
    KEY_ROW4 = 1;
    
    Key_Delay(1);  // 稳定电平
    
    if(KEY_COL2 == 0)  // 行2列2 → 按键6
    {
        Key_Delay(5);
        if(KEY_COL2 == 0)
        {
            key_val = KEY_MINUTE_FORWARD;
        }
    }
    
    if(KEY_COL3 == 0)  // 行2列3 → 按键7
    {
        Key_Delay(5);
        if(KEY_COL3 == 0)
        {
            key_val = KEY_MINUTE_BACKWARD;
        }
    }
    
    if(KEY_COL4 == 0)  // 行2列4 → 按键8
    {
        Key_Delay(5);
        if(KEY_COL4 == 0)
        {
            key_val = KEY_HOUR_FORWARD;
        }
    }
    
    KEY_ROW2 = 1;
    
    // ---------------------- 扫描行3（P15） ----------------------
    KEY_ROW1 = 1;
    KEY_ROW2 = 1;
    KEY_ROW3 = 0;  // 只拉低行3
    KEY_ROW4 = 1;
    
    Key_Delay(1);
    
    if(KEY_COL1 == 0)  // 行3列1 → 按键9
    {
        Key_Delay(5);
        if(KEY_COL1 == 0)
        {
            key_val = KEY_HOUR_BACKWARD;
        }
    }
    
    KEY_ROW3 = 1;
    
    return key_val;
}