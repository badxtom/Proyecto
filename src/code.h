// Author Luz Collado

#include <stdio.h>
#include "pico/stdlib.h"
#include <string.h>
#include "hardware/i2c.h"
#include "pico/binary_info.h"
#include "hardware/gpio.h"
#include <math.h>
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "timers.h"
#include "semphr.h"



#define TASK_PRIORITY (tskIDLE_PRIORITY + 1UL)
#define DELAY 1000


const int LCD_CLEARDISPLAY = 0x01;
const int LCD_RETURNHOME = 0x02;
const int LCD_ENTRYMODESET = 0x04;
const int LCD_DISPLAYCONTROL = 0x08;
const int LCD_CURSORSHIFT = 0x10;
const int LCD_FUNCTIONSET = 0x20;
const int LCD_SETCGRAMADDR = 0x40;
const int LCD_SETDDRAMADDR = 0x80;

// flags for display entry mode
const int LCD_ENTRYSHIFTINCREMENT = 0x01;
const int LCD_ENTRYLEFT = 0x02;

// flags for display and cursor control
const int LCD_BLINKON = 0x01;
const int LCD_CURSORON = 0x02;
const int LCD_DISPLAYON = 0x04;

// flags for display and cursor shift
const int LCD_MOVERIGHT = 0x04;
const int LCD_DISPLAYMOVE = 0x08;

// flags for function set
const int LCD_5x10DOTS = 0x04;
const int LCD_2LINE = 0x08;
const int LCD_8BITMODE = 0x10;

// flag for backlight control
const int LCD_BACKLIGHT = 0x08;

const int LCD_ENABLE_BIT = 0x04;

// By default these LCD display drivers are on bus address 0x27
static int addr = 0x27;

// Modes for lcd_send_byte
#define LCD_CHARACTER 1
#define LCD_COMMAND 0

#define MAX_LINES 2
#define MAX_CHARS 16

#define  BUTTON_PIN 16


QueueHandle_t colaSend;

float fahrenheit;

TaskHandle_t sensorTaskHandle;
TaskHandle_t lcdTaskHandle;
TaskHandle_t serverTaskHandle;

static volatile bool displayEnabled = false;


void sensorTask(void *pvParameters);
void lcdTask(void *pvParameters);
void run_tcp_server_testTask(void *pvParameters);
void button_isr_handler(uint gpio, uint32_t events);

/* Quick helper function for single byte transfers */
void i2c_write_byte(uint8_t val)
{
#ifdef i2c_default
    i2c_write_blocking(i2c_default, addr, &val, 1, false);
#endif
}

void lcd_toggle_enable(uint8_t val)
{
    // Toggle enable pin on LCD display
    // We cannot do this too quickly or things don't work
#define DELAY_US 600
    sleep_us(DELAY_US);
    i2c_write_byte(val | LCD_ENABLE_BIT);
    sleep_us(DELAY_US);
    i2c_write_byte(val & ~LCD_ENABLE_BIT);
    sleep_us(DELAY_US);
}

// The display is sent a byte as two separate nibble transfers
void lcd_send_byte(uint8_t val, int mode)
{
    uint8_t high = mode | (val & 0xF0) | LCD_BACKLIGHT;
    uint8_t low = mode | ((val << 4) & 0xF0) | LCD_BACKLIGHT;

    i2c_write_byte(high);
    lcd_toggle_enable(high);
    i2c_write_byte(low);
    lcd_toggle_enable(low);
}
 
void lcd_clear(void)
{
    lcd_send_byte(LCD_CLEARDISPLAY, LCD_COMMAND);
}

// go to location on LCD
void lcd_set_cursor(int line, int position)
{
    int val = (line == 0) ? 0x80 + position : 0xC0 + position;
    lcd_send_byte(val, LCD_COMMAND);
}

static void inline lcd_char(char val)
{
    lcd_send_byte(val, LCD_CHARACTER);
}

void lcd_string(const char *s)
{
    while (*s)
    {
        lcd_char(*s++);
    }
}

void lcd_init()
{
    lcd_send_byte(0x03, LCD_COMMAND);
    lcd_send_byte(0x03, LCD_COMMAND);
    lcd_send_byte(0x03, LCD_COMMAND);
    lcd_send_byte(0x02, LCD_COMMAND);

    lcd_send_byte(LCD_ENTRYMODESET | LCD_ENTRYLEFT, LCD_COMMAND);
    lcd_send_byte(LCD_FUNCTIONSET | LCD_2LINE, LCD_COMMAND);
    lcd_send_byte(LCD_DISPLAYCONTROL | LCD_DISPLAYON, LCD_COMMAND);
    lcd_clear();
}


void button_isr_handler(uint gpio, uint32_t events) {
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    displayEnabled = !displayEnabled;

    if (displayEnabled) {
        vTaskNotifyGiveFromISR(sensorTaskHandle, &xHigherPriorityTaskWoken);
    } else {
        xQueueReset(colaSend);
    }

    portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
}

void timerCallback(TimerHandle_t xTimer)
{
    xTaskNotifyGive(sensorTaskHandle); 
}

void sensorTask(void *pvParameters)
{

    dht_reading reading;
    while (1)
    {
        read_from_dht(&reading);
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
        xQueueSend(colaSend, &reading, portMAX_DELAY);

    }
}


void lcdTask(void *pvParameters)
{

    dht_reading reading;
    char message[MAX_LINES][MAX_CHARS + 1] = {0};

    while (1)
    {

        if (xQueueReceive(colaSend, &reading, portMAX_DELAY)&& displayEnabled)
        {

            fahrenheit = (reading.temp_celsius * 9 / 5) + 32;
            snprintf(message[0], MAX_CHARS + 1, "Humidity: %.1f%%", reading.humidity);
            snprintf(message[1], MAX_CHARS + 1, "Temp:%.1fC/%.1fF", reading.temp_celsius, fahrenheit);

            for (int m = 0; m < sizeof(message) / sizeof(message[0]); m += MAX_LINES)
            {
                for (int line = 0; line < MAX_LINES; line++)
                {
                    lcd_set_cursor(line, (MAX_CHARS / 2) - strlen(message[m + line]) / 2);
                    lcd_string(message[m + line]);
                }
                vTaskDelay(DELAY);
                lcd_clear();
            }
        }
    }
}


void update_and_send_dht_data(void *arg, struct tcp_pcb *tpcb) {
    TCP_SERVER_T *state = (TCP_SERVER_T *)arg;
    dht_reading reading;
    xQueueReceive(colaSend, &reading, portMAX_DELAY);
    read_from_dht(&reading);
    char data[50];
    fahrenheit = (reading.temp_celsius * 9 / 5) + 32;
    snprintf(data, sizeof(data), "Humidity = %.1f%%, Temperature = %.1fC/%.1fF\n", reading.humidity, reading.temp_celsius, fahrenheit);
    printf("Enviando data: %s\n", data);
    tcp_server_send_data(arg, tpcb, data);
 
}

void vLaunch(void)
{

    colaSend = xQueueCreate(10, sizeof(dht_reading));
    xTaskCreate(sensorTask, "sensorTask", configMINIMAL_STACK_SIZE, NULL, TASK_PRIORITY + 2, &sensorTaskHandle);
    xTaskCreate(run_tcp_server_testTask, "serverTask", configMINIMAL_STACK_SIZE, NULL, TASK_PRIORITY , &serverTaskHandle);
    xTaskCreate(lcdTask, "lcdTask", configMINIMAL_STACK_SIZE, NULL, TASK_PRIORITY, &lcdTaskHandle);

    vTaskStartScheduler();

}
