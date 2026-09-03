#include "bsp_pwm.h"


/**
 * @函数说明        LEDC功能初始化
 * @传入参数        无
 * @函数返回        无
 * @备    注        PWM频率越高，可用的占空比分辨率越低
 */
void LedcInitConfig(void)
{
    // 准备并应用led PWM定时器配置
    ledc_timer_config_t ledc_timer = {
        .speed_mode       = LEDC_MODE,          //LED模式 低速模式
        .timer_num        = LEDC_TIMER,         //通道的定时器源    定时器0
        .duty_resolution  = LEDC_DUTY_RES,      //将占空比分辨率设置为13位
        .freq_hz          = LEDC_FREQUENCY,     // 设置输出频率为5 kHz
        .clk_cfg          = LEDC_AUTO_CLK       //设置LEDPWM的时钟来源 为自动
        //LEDC_AUTO_CLK = 启动定时器时，将根据给定的分辨率和占空率参数自动选择led源时钟
    };
    ledc_timer_config(&ledc_timer);

    // 准备并应用LEDC PWM通道配置
    ledc_channel_config_t ledc_channel = {
        .speed_mode     = LEDC_MODE,            //LED模式 低速模式
        .channel        = LEDC_CHANNEL,         //通道0
        .timer_sel      = LEDC_TIMER,           //定时器源 定时器0
        .intr_type      = LEDC_INTR_DISABLE,    //关闭中断
        .gpio_num       = LEDC_OUTPUT_IO,       //输出引脚  GPIO5
        .duty           = 0,                    // 设置占空比为0
        .hpoint         = 0
    };
    ledc_channel_config(&ledc_channel);
}