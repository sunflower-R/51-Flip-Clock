#include "motor.h"
#include "ds1302.h"  // 添加头文件以使用TIME数组

// 8拍（半步）驱动序列（低4位给电机1，高4位给电机2）
// 序列：A-AB-B-BC-C-CD-D-DA
unsigned char code phase_table[8] = {
    0x08,  // 00001000: A相通电 (1000)
    0x0C,  // 00001100: A+B相通电 (1100)
    0x04,  // 00000100: B相通电 (0100)
    0x06,  // 00000110: B+C相通电 (0110)
    0x02,  // 00000010: C相通电 (0010)
    0x03,  // 00000011: C+D相通电 (0011)
    0x01,  // 00000001: D相通电 (0001)
    0x09   // 00001001: D+A相通电 (1001)
};

// 电机控制变量
unsigned char motor1_index = 0;
unsigned char motor2_index = 0;
unsigned int motor1_current_step = 0;
unsigned int motor2_current_step = 0;
unsigned int motor1_target_steps = 0;
unsigned int motor2_target_steps = 0;
bit motor1_busy = 0;
bit motor2_busy = 0;
bit motor1_direction = 1;  // 0=正转，1=反转（小时电机默认反转）
bit motor2_direction = 0;  // 0=正转，1=反转（分钟电机默认正转）

// 连续旋转模式标志
bit motor1_continuous_mode = 0;
bit motor2_continuous_mode = 0;

// 步进延迟（控制速度）
unsigned int motor1_step_delay = 10;  // 10ms/步（自动控制速度）
unsigned int motor2_step_delay = 10;  // 10ms/步（自动控制速度）
unsigned int motor1_continuous_delay = 20;  // 20ms/步（连续旋转速度，适中）
unsigned int motor2_continuous_delay = 20;  // 20ms/步（连续旋转速度，适中）

// 手动调节累积值
int motor1_manual_offset = 0;
int motor2_manual_offset = 0;

// 连续旋转计数器
unsigned int motor1_continuous_steps = 0;
unsigned int motor2_continuous_steps = 0;

// 连续旋转延迟计数器
static unsigned int motor1_continuous_counter = 0;
static unsigned int motor2_continuous_counter = 0;

// 用于时间触发的静态变量
static unsigned char last_minute = 0xFF;
static unsigned char last_hour = 0xFF;

// 零点校正相关变量
static unsigned char minute_zero_correcting = 0;  // 分钟电机零点校正标志
static unsigned char hour_zero_correcting = 0;    // 时钟电机零点校正标志（新增）
static unsigned int zero_correction_steps = 0;    // 零点校正已走步数
static unsigned int hour_zero_correction_steps = 0; // 时钟电机零点校正步数（新增）

// 特殊分钟补偿相关变量
static unsigned char processed_20min = 0;  // 标记第20分钟是否已处理

// 补偿配置
MotorCompConfig motor2_comp_config = {
    68,     // base_steps: 基础步数68
    73,     // compensation_steps: 补偿步数69
    3,     // compensation_minutes: 每3分钟补偿一次
    0,      // current_count: 当前计数从0开始
    1       // enabled: 启用补偿 (1=启用)
};

// ---------------------- 补偿配置函数 ----------------------
void motor_set_compensation(unsigned char base_steps, 
                           unsigned char comp_steps, 
                           unsigned char interval)
{
    motor2_comp_config.base_steps = base_steps;
    motor2_comp_config.compensation_steps = comp_steps;
    motor2_comp_config.compensation_minutes = interval;
    motor2_comp_config.current_count = 0;
}

void motor_enable_compensation(unsigned char enable)
{
    motor2_comp_config.enabled = enable;
    if (!enable) {
        motor2_comp_config.current_count = 0;
    }
}

// 获取当前分钟应走步数（带补偿）
unsigned char motor_get_minute_steps(void)
{
    unsigned char steps;
    
    // 如果补偿未启用或间隔为0，返回基础步数
    if (!motor2_comp_config.enabled || motor2_comp_config.compensation_minutes == 0) {
        return motor2_comp_config.base_steps;
    }
    
    // 增加计数
    motor2_comp_config.current_count++;
    
    // 如果达到补偿间隔，返回补偿步数并重置计数
    if (motor2_comp_config.current_count >= motor2_comp_config.compensation_minutes) {
        motor2_comp_config.current_count = 0;
        steps = motor2_comp_config.compensation_steps;
    } else {
        // 否则返回基础步数
        steps = motor2_comp_config.base_steps;
    }
    
    return steps;
}

// ---------------------- 电机初始化 ----------------------
void motor_init(void)
{
    // 初始化端口（两个电机都停止）
    MOTOR_PORT = 0x00;
    
    // 重置控制状态
    motor1_busy = 0;
    motor2_busy = 0;
    motor1_continuous_mode = 0;
    motor2_continuous_mode = 0;
    motor1_current_step = 0;
    motor2_current_step = 0;
    motor1_target_steps = 0;
    motor2_target_steps = 0;
    motor1_index = 0;
    motor2_index = 0;
    
    // 重置手动偏移
    motor1_manual_offset = 0;
    motor2_manual_offset = 0;
    
    // 重置连续旋转计数器
    motor1_continuous_steps = 0;
    motor2_continuous_steps = 0;
    
    // 初始化时间记录变量
    last_minute = 0xFF;
    last_hour = 0xFF;
    
    // 初始化零点校正标志
    minute_zero_correcting = 0;
    hour_zero_correcting = 0;  // 新增
    zero_correction_steps = 0;
    hour_zero_correction_steps = 0;  // 新增
    
    // 初始化特殊分钟补偿标记
    processed_20min = 0;
    
    // 初始化补偿计数
    motor2_comp_config.current_count = 0;
}

// ---------------------- 电机1启动移动（固定步数） ----------------------
void motor1_start_move(unsigned int steps, bit direction)
{
    if(motor1_busy && !motor1_continuous_mode) return;
    
    motor1_target_steps = steps;
    motor1_current_step = 0;
    motor1_busy = 1;
    motor1_direction = direction;
    motor1_index = direction ? 7 : 0;  // 8拍模式，起始相位0或7
}

// ---------------------- 电机2启动移动（固定步数） ----------------------
void motor2_start_move(unsigned int steps, bit direction)
{
    if(motor2_busy && !motor2_continuous_mode) return;
    
    motor2_target_steps = steps;
    motor2_current_step = 0;
    motor2_busy = 1;
    motor2_direction = direction;
    motor2_index = direction ? 7 : 0;  // 8拍模式，起始相位0或7
}

// ---------------------- 电机1单步驱动 ----------------------
void motor1_step(void)
{
    unsigned char phase_index;
    unsigned char current_port;
    
    if(!motor1_busy) return;
    
    // 检查是否完成（非连续模式）
    if(!motor1_continuous_mode && motor1_current_step >= motor1_target_steps)
    {
        motor1_busy = 0;
        // 停止电机1（只清除低4位）
        current_port = MOTOR_PORT;
        current_port &= 0xF0;
        MOTOR_PORT = current_port;
        return;
    }
    
    // 计算相位（8拍模式）
    if(motor1_direction == 0) {
        // 正转
        phase_index = motor1_index;
        motor1_index = (motor1_index + 1) % 8;
    } else {
        // 反转
        phase_index = motor1_index;
        if(motor1_index == 0) {
            motor1_index = 7;
        } else {
            motor1_index--;
        }
    }
    
    // 应用相位到电机1（低4位）
    current_port = MOTOR_PORT;
    current_port = (current_port & 0xF0) | (phase_table[phase_index] & 0x0F);
    MOTOR_PORT = current_port;
    
    motor1_current_step++;
    
    // 如果是连续模式，累积偏移
    if(motor1_continuous_mode) {
        if(motor1_direction == 1) {  // 反转（前进）
            motor1_manual_offset -= 1;
        } else {  // 正转（后退）
            motor1_manual_offset += 1;
        }
        motor1_continuous_steps++;
    }
}

// ---------------------- 电机2单步驱动 ----------------------
void motor2_step(void)
{
    unsigned char phase_index;
    unsigned char current_port;
    
    if(!motor2_busy) return;
    
    // 检查是否完成（非连续模式）
    if(!motor2_continuous_mode && motor2_current_step >= motor2_target_steps)
    {
        motor2_busy = 0;
        // 停止电机2（只清除高4位）
        current_port = MOTOR_PORT;
        current_port &= 0x0F;
        MOTOR_PORT = current_port;
        return;
    }
    
    // 计算相位（8拍模式）
    if(motor2_direction == 0) {
        // 正转
        phase_index = motor2_index;
        motor2_index = (motor2_index + 1) % 8;
    } else {
        // 反转
        phase_index = motor2_index;
        if(motor2_index == 0) {
            motor2_index = 7;
        } else {
            motor2_index--;
        }
    }
    
    // 应用相位到电机2（高4位）
    current_port = MOTOR_PORT;
    current_port = (current_port & 0x0F) | (phase_table[phase_index] << 4);
    MOTOR_PORT = current_port;
    
    motor2_current_step++;
    
    // 如果是连续模式，累积偏移
    if(motor2_continuous_mode) {
        if(motor2_direction == 0) {  // 正转（前进）
            motor2_manual_offset += 1;
        } else {  // 反转（后退）
            motor2_manual_offset -= 1;
        }
        motor2_continuous_steps++;
    }
}

// ---------------------- 自动控制处理（带补偿） ----------------------
void motor_auto_control(void)
{
    unsigned char current_minute, current_hour;
    unsigned char steps;
    
    // 获取当前分钟和小时（从TIME数组中，BCD码格式）
    current_minute = (TIME[1] >> 4) * 10 + (TIME[1] & 0x0F);
    current_hour = (TIME[2] >> 4) * 10 + (TIME[2] & 0x0F);
    
    // 初始化记录值
    if(last_minute == 0xFF) {
        last_minute = current_minute;
        last_hour = current_hour;
        processed_20min = 0;  // 重置第20分钟标记
        motor2_comp_config.current_count = current_minute % motor2_comp_config.compensation_minutes;
        return;
    }
    
    // 检查是否是第20分钟且还未处理
    if(current_minute == 20 && !processed_20min && !motor2_busy && !motor2_continuous_mode) {
        // 第20分钟额外转68步
        motor2_start_move(120, 0);  // 正转68步
        motor2_manual_offset += 68;
        processed_20min = 1;  // 标记已处理
    }
    
    // 分钟电机控制（每分钟转动一次）
    if(!motor2_busy && !motor2_continuous_mode && current_minute != 20) {
        // 检测分钟是否变化
        if(current_minute != last_minute) {
            // 获取当前分钟应该走的步数（带补偿）
            steps = motor_get_minute_steps();
            
            // 如果是00分，进行零点校正
            if(current_minute == 0) {
                processed_20min = 0;  // 重置第20分钟标记
                
                // 先执行正常的转动
                motor2_start_move(steps, 0);
                motor2_manual_offset += steps;
                
                // 设置零点校正标志
                minute_zero_correcting = 1;
                zero_correction_steps = 0;
            } else {
                // 非00分：正常转动
                motor2_start_move(steps, 0);
                motor2_manual_offset += steps;
            }
            last_minute = current_minute;
        }
    }
    
    // 时钟电机控制（每小时转动一次）
    if(!motor1_busy && !motor1_continuous_mode) {
        // 检测小时是否变化
        if(current_hour != last_hour) {
            // 如果小时为0，进行时钟电机零点校正
            if(current_hour == 0) {
                hour_zero_correcting = 1;
                hour_zero_correction_steps = 0;
                last_hour = current_hour;
            } else {
                // 8拍模式步数翻倍：84步 × 2 = 168步
                motor1_start_move(170, 1);  // 反转168步（8拍模式）
                motor1_manual_offset -= 170;
                last_hour = current_hour;
            }
        }
    }
}

// ---------------------- 处理零点校正 ----------------------
void motor_process_zero_correction(void)
{
    static unsigned int zero_correction_counter = 0;
    static unsigned int hour_zero_correction_counter = 0;
    unsigned char current_port;
    
    // 处理分钟电机零点校正
    if(minute_zero_correcting && !motor2_busy && !motor2_continuous_mode) {
        // 每20ms执行一步（不要太快）
        if(++zero_correction_counter >= 20) {
            zero_correction_counter = 0;
            
            // 检查霍尔传感器是否已经在零点位置
            if(HALL_SENSOR == 0) {
                // 零点已找到，停止校正
                minute_zero_correcting = 0;
                zero_correction_steps = 0;
                
                // 停止电机2（只清除高4位）
                current_port = MOTOR_PORT;
                current_port &= 0x0F;
                MOTOR_PORT = current_port;
            } else {
                // 零点未找到，继续正转寻找
                if(zero_correction_steps < 2000) {
                    // 手动控制电机2正转一步
                    motor2_index = (motor2_index + 1) % 8;
                    current_port = MOTOR_PORT;
                    current_port = (current_port & 0x0F) | (phase_table[motor2_index] << 4);
                    MOTOR_PORT = current_port;
                    zero_correction_steps++;
                    
                    // 更新偏移（正转，偏移加1）
                    motor2_manual_offset += 1;
                } else {
                    // 超过2000步还没找到零点，放弃校正
                    minute_zero_correcting = 0;
                    zero_correction_steps = 0;
                    
                    // 停止电机2（只清除高4位）
                    current_port = MOTOR_PORT;
                    current_port &= 0x0F;
                    MOTOR_PORT = current_port;
                }
            }
        }
    }
    
     // 处理时钟电机零点校正
    if(hour_zero_correcting && !motor1_busy && !motor1_continuous_mode) {
        // 先检查霍尔传感器状态
        if(HOUR_HALL_SENSOR == 0) {
            // 零点已找到，立即停止校正
            hour_zero_correcting = 0;
            hour_zero_correction_steps = 0;
            
            // 立即停止电机1（只清除低4位）
            current_port = MOTOR_PORT;
            current_port &= 0xF0;
            MOTOR_PORT = current_port;
            
            // 零点位置调整：由于检测到零点时已经到达零点位置，不需要再走额外步数
            // 将当前偏移归零，表示现在是准确的零点位置
            motor1_manual_offset = 0;
            
            // 重置计时器
            hour_zero_correction_counter = 0;
            
            // 校正完成后，正常转动到12点位置（反转170步）
            // 注意：这里需要等当前校正完全停止后再启动
        } else {
            // 每30ms执行一步（时钟电机速度可以稍慢）
            if(++hour_zero_correction_counter >= 30) {
                hour_zero_correction_counter = 0;
                
                // 在走步之前再次检查霍尔传感器
                if(HOUR_HALL_SENSOR == 0) {
                    // 发现零点，立即停止
                    hour_zero_correcting = 0;
                    hour_zero_correction_steps = 0;
                    
                    // 立即停止电机1
                    current_port = MOTOR_PORT;
                    current_port &= 0xF0;
                    MOTOR_PORT = current_port;
                    
                    // 零点位置调整
                    motor1_manual_offset = 0;
                    
        
                } else {
                    // 零点未找到，继续反转寻找（时钟电机反转方向）
                    if(hour_zero_correction_steps < 3000) {  // 最多转3000步（约1.5圈）
                        // 手动控制电机1反转一步
                        if(motor1_index == 0) {
                            motor1_index = 7;
                        } else {
                            motor1_index--;
                        }
                        current_port = MOTOR_PORT;
                        current_port = (current_port & 0xF0) | (phase_table[motor1_index] & 0x0F);
                        MOTOR_PORT = current_port;
                        hour_zero_correction_steps++;
                        
                        // 更新偏移（反转，偏移减1）
                        motor1_manual_offset -= 1;
                    } else {
                        // 超过3000步还没找到零点，放弃校正
                        hour_zero_correcting = 0;
                        hour_zero_correction_steps = 0;
                        
                        // 停止电机1（只清除低4位）
                        current_port = MOTOR_PORT;
                        current_port &= 0xF0;
                        MOTOR_PORT = current_port;
                        
          
                    }
                }
            }
        }
    }
}

// ---------------------- 连续旋转控制函数 ----------------------
void motor1_start_continuous(bit direction)
{
    if(motor1_continuous_mode && motor1_direction == direction) {
        return;
    }
    
    motor1_continuous_mode = 1;
    motor1_direction = direction;
    motor1_busy = 1;
    motor1_index = direction ? 7 : 0;
    motor1_continuous_steps = 0;
    motor1_continuous_counter = 0;
}

void motor2_start_continuous(bit direction)
{
    if(motor2_continuous_mode && motor2_direction == direction) {
        return;
    }
    
    motor2_continuous_mode = 1;
    motor2_direction = direction;
    motor2_busy = 1;
    motor2_index = direction ? 7 : 0;
    motor2_continuous_steps = 0;
    motor2_continuous_counter = 0;
}

void motor1_stop_continuous(void)
{
    unsigned char current_port;
    
    if(motor1_continuous_mode) {
        motor1_continuous_mode = 0;
        motor1_busy = 0;
        // 停止电机1（只清除低4位）
        current_port = MOTOR_PORT;
        current_port &= 0xF0;
        MOTOR_PORT = current_port;
    }
}

void motor2_stop_continuous(void)
{
    unsigned char current_port;
    
    if(motor2_continuous_mode) {
        motor2_continuous_mode = 0;
        motor2_busy = 0;
        // 停止电机2（只清除高4位）
        current_port = MOTOR_PORT;
        current_port &= 0x0F;
        MOTOR_PORT = current_port;
    }
}

// ---------------------- 处理连续旋转 ----------------------
void motor_process_continuous(void)
{
    // 处理电机1连续旋转
    if(motor1_continuous_mode) {
        if(++motor1_continuous_counter >= motor1_continuous_delay) {
            motor1_continuous_counter = 0;
            motor1_step();
        }
    }
    
    // 处理电机2连续旋转
    if(motor2_continuous_mode) {
        if(++motor2_continuous_counter >= motor2_continuous_delay) {
            motor2_continuous_counter = 0;
            motor2_step();
        }
    }
    
    // 处理零点校正
    motor_process_zero_correction();
}