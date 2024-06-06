#include "driver/gpio.h"
#include <esp_log.h>
#include "freertos/FreeRTOS.h"
#include "driver/ledc.h"
#include "freertos/task.h"

#define int1 9                                                                                                                                                        
#define int2 10
#define int3 11
#define int4 12                                             

#define SERVO_PIN_1 13
#define SERVO_PIN_2 14
#define SERVO_PIN_3 39
#define SERVO_PIN_4 47

#define GPIO_OUTPUT_PIN_SEL ((1ULL << int1) | \
                             (1ULL << int2) | \
                             (1ULL << int3) | \
                             (1ULL << int4))




#define SERVO_OUTPUT_PIN_SEL ((1ULL << SERVO_PIN_1) | \
                              (1ULL << SERVO_PIN_2) | \
                              (1ULL << SERVO_PIN_3) | \
                              (1ULL << SERVO_PIN_4))

#define SERVO_MIN_PULSEWIDTH_US 500  
#define SERVO_MAX_PULSEWIDTH_US 2500 
#define SERVO_MAX_DEGREE 180

int servo_pins[] = { SERVO_PIN_1, SERVO_PIN_2, SERVO_PIN_3, SERVO_PIN_4 };

static const char *TAG = "motor";

typedef struct {
    int gpio_num;
    ledc_channel_t channel;
    ledc_timer_t timer;
} servo_t;

int ch_io[2][2] = {
    {int1, int2},
    {int3, int4},
};

int channel_status[2] = {0, 0};
servo_t *servos;
int servo_num;

void GPIO_INIT() {
    gpio_config_t io_conf;
    io_conf.intr_type = GPIO_INTR_DISABLE;
    io_conf.mode = GPIO_MODE_OUTPUT;
    io_conf.pin_bit_mask = GPIO_OUTPUT_PIN_SEL;
    io_conf.pull_down_en = 1;
    io_conf.pull_up_en = 0;
    gpio_config(&io_conf);
}

void init_servo(servo_t *servo)
{
    ledc_timer_config_t ledc_timer = {
        .duty_resolution = LEDC_TIMER_14_BIT,
        .freq_hz = 50,
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .timer_num = servo->timer
    };
    ledc_timer_config(&ledc_timer);

    ledc_channel_config_t ledc_channel = {
        .channel    = servo->channel,
        .duty       = 0,
        .gpio_num   = servo->gpio_num,
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .hpoint     = 0,
        .timer_sel  = servo->timer
    };
    ledc_channel_config(&ledc_channel);
}

void SERVO_INIT() {
    gpio_config_t io_conf;
    io_conf.intr_type = GPIO_INTR_DISABLE;
    io_conf.mode = GPIO_MODE_OUTPUT;
    io_conf.pin_bit_mask = SERVO_OUTPUT_PIN_SEL;
    io_conf.pull_down_en = 0;
    io_conf.pull_up_en = 0;
    gpio_config(&io_conf);
    servo_num = sizeof(servo_pins) / sizeof(servo_pins[0]);
    servos = malloc(servo_num * sizeof(servo_t));
    for (int i = 0; i < servo_num; i++) {
        servos[i].gpio_num = servo_pins[i];        
        servos[i].channel = LEDC_CHANNEL_0 + i;
        servos[i].timer = LEDC_TIMER_0 + i;     
        init_servo(&servos[i]);            
    }
}

uint32_t angle_to_duty_cycle(int angle)
{
    uint32_t duty_cycle = ((SERVO_MIN_PULSEWIDTH_US + ((SERVO_MAX_PULSEWIDTH_US - SERVO_MIN_PULSEWIDTH_US) * angle) / SERVO_MAX_DEGREE) * (1 << LEDC_TIMER_14_BIT)) / 20000;
    return duty_cycle;
}

void set_servo_angle(int index, int angle)
{
    ESP_LOGE(TAG, "index %d , angle %d", index, angle);
    if (index >= servo_num) return;

    if (angle > SERVO_MAX_DEGREE) {
        angle = SERVO_MAX_DEGREE;
    }

    uint32_t duty_cycle = angle_to_duty_cycle(angle);
    ledc_set_duty(LEDC_LOW_SPEED_MODE, servos[index].channel, duty_cycle);
    ledc_update_duty(LEDC_LOW_SPEED_MODE, servos[index].channel);
}

void step_motor_task() {
        gpio_set_level(int1, 1);
        gpio_set_level(int2, 0);
        gpio_set_level(int3, 1);
        gpio_set_level(int4, 0);
        vTaskDelay(pdMS_TO_TICKS(10)); 
        gpio_set_level(int1, 0);
        gpio_set_level(int2, 1);
        gpio_set_level(int3, 1);
        gpio_set_level(int4, 0);            
        vTaskDelay(pdMS_TO_TICKS(10)); 
        gpio_set_level(int1, 0);
        gpio_set_level(int2, 1);
        gpio_set_level(int3, 0);
        gpio_set_level(int4, 1);  
        vTaskDelay(pdMS_TO_TICKS(10)); 
        gpio_set_level(int1, 1);
        gpio_set_level(int2, 0);
        gpio_set_level(int3, 0);
        gpio_set_level(int4, 1);
        vTaskDelay(pdMS_TO_TICKS(10)); 
}

void loop() {
  while (1) {
    step_motor_task();
    vTaskDelay(pdMS_TO_TICKS(5)); 
  }
}

void motor_control(int id, int fx)
{
    ESP_LOGE(TAG, "index %d , state %d", id, fx);
    if (fx == 2) {
        ESP_LOGE(TAG, "2");
        channel_status[id] = 2;
        gpio_set_level(ch_io[id][0], 1);
        gpio_set_level(ch_io[id][1], 0);
    } else if (fx == 1){
        ESP_LOGE(TAG, "1");
        channel_status[id] = 1;
        gpio_set_level(ch_io[id][0], 0);
        gpio_set_level(ch_io[id][1], 1);
    } else {
        ESP_LOGE(TAG, "0");
        channel_status[id] = 0;
        gpio_set_level(ch_io[id][0], 0);
        gpio_set_level(ch_io[id][1], 0);
    }
}
