#ifndef __MOTOR_H_
#define __MOTOR_H_

#include <reg52.h>

// 电机引脚定义
#define MOTOR_PORT P2  // 电机控制端口

// 霍尔传感器引脚定义
sbit HALL_SENSOR = P1^2;  // 分钟电机霍尔传感器接在P1^2，磁铁靠近时输出低电平
sbit HOUR_HALL_SENSOR = P1^3;  // 时钟电机霍尔传感器接在P1^3，磁铁靠近时输出低电平

// 8拍（半步）驱动序列
extern unsigned char code phase_table[8];

// 电机控制变量（仅声明在data区频繁访问的变量）
extern unsigned char motor1_index;
extern unsigned char motor2_index;
extern unsigned int motor1_current_step;
extern unsigned int motor2_current_step;
extern unsigned int motor1_target_steps;
extern unsigned int motor2_target_steps;
extern bit motor1_busy;
extern bit motor2_busy;
extern bit motor1_direction;
extern bit motor2_direction;

// 连续旋转模式标志
extern bit motor1_continuous_mode;
extern bit motor2_continuous_mode;

// 步进延迟（控制速度） - 定时器中断中频繁访问，放在data区
extern unsigned int motor1_step_delay;
extern unsigned int motor2_step_delay;
extern unsigned int motor1_continuous_delay;
extern unsigned int motor2_continuous_delay;

// 连续旋转计数器
extern unsigned int motor1_continuous_steps;
extern unsigned int motor2_continuous_steps;

// 手动调节累积值
extern int motor1_manual_offset;
extern int motor2_manual_offset;

// 补偿配置结构
typedef struct {
    unsigned char base_steps;           // 基础步数（通常68）
    unsigned char compensation_steps;   // 补偿步数（69或70）
    unsigned char compensation_minutes; // 补偿间隔（分钟数）
    unsigned char current_count;        // 当前分钟计数
    unsigned char enabled;              // 补偿是否启用 (0=禁用, 1=启用)
} MotorCompConfig;

// 补偿配置
extern MotorCompConfig motor2_comp_config;

// 函数声明
void motor_init(void);
void motor1_start_move(unsigned int steps, bit direction);
void motor2_start_move(unsigned int steps, bit direction);
void motor1_step(void);
void motor2_step(void);
void motor_auto_control(void);
void motor_process_continuous(void);
void motor_process_zero_correction(void);
void motor1_start_continuous(bit direction);
void motor2_start_continuous(bit direction);
void motor1_stop_continuous(void);
void motor2_stop_continuous(void);

// 补偿配置函数
void motor_set_compensation(unsigned char base_steps, 
                           unsigned char comp_steps, 
                           unsigned char interval);
void motor_enable_compensation(unsigned char enable);
unsigned char motor_get_minute_steps(void);

#endif