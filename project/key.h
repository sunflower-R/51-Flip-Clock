#ifndef __KEY_H
#define __KEY_H

#include <reg52.h>
#include <intrins.h>

// 矩阵键盘引脚定义（行：P17~P14，列：P13~P10）
sbit KEY_ROW1 = P1^7;  // 行1
sbit KEY_ROW2 = P1^6;  // 行2
sbit KEY_ROW3 = P1^5;  // 行3
sbit KEY_ROW4 = P1^4;  // 行4
sbit KEY_COL1 = P3^3;  // 列1
sbit KEY_COL2 = P3^2;  // 列2
sbit KEY_COL3 = P3^1;  // 列3
sbit KEY_COL4 = P3^0;  // 列4

// 蜂鸣器接口（P37）
sbit BEEP = P3^4;

// 按键编号宏（对应4×4矩阵）
#define KEY_NONE           0   // 无按键
#define KEY_SET_MODE       1   // 进入/退出设置模式
#define KEY_SWITCH         2   // 切换设置项
#define KEY_ADD            3   // 数值+1
#define KEY_SUB            4   // 数值-1
#define KEY_SAVE           5   // 保存设置
#define KEY_MINUTE_FORWARD 6   // 分钟电机前进（按住旋转）
#define KEY_MINUTE_BACKWARD 7  // 分钟电机后退（按住旋转）
#define KEY_HOUR_FORWARD   8   // 时钟电机前进（按住旋转）
#define KEY_HOUR_BACKWARD  9   // 时钟电机后退（按住旋转）
// 闹钟功能按键
#define KEY_ALARM_SET      10  // 进入/退出闹钟设置
#define KEY_ALARM_HOUR     11  // 闹钟小时调节
#define KEY_ALARM_MINUTE   12  // 闹钟分钟调节
#define KEY_ALARM_TOGGLE   13  // 闹钟开关/停止响铃
// 预留按键
#define KEY_RESERVE14      14
#define KEY_RESERVE15      15
#define KEY_RESERVE16      16

// 函数声明
unsigned char Key_Scan(void);  // 矩阵键盘扫描（返回按键编号，无按键返回0）
void Key_Delay(unsigned int ms); // 消抖延时函数
void BEEP_Alert(void);        // 蜂鸣器提示（短鸣一声）

// 新增：持续按键检测函数
unsigned char Key_Hold_Scan(void);  // 检测按键是否被持续按住

#endif