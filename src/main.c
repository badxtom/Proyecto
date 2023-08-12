//Author Luz Collado

#include "server.h"
#include "code.h"

int main()
{

    stdio_init_all();
    gpio_init(DHT_PIN);
#ifdef LED_PIN
    gpio_init(LED_PIN);
    gpio_set_dir(LED_PIN, GPIO_OUT);
#endif

    gpio_init(BUTTON_PIN);
    gpio_set_dir(BUTTON_PIN, GPIO_IN);
    gpio_pull_up(BUTTON_PIN);

     gpio_set_irq_enabled_with_callback(BUTTON_PIN, GPIO_IRQ_EDGE_FALL, true, &button_isr_handler);

    // This example will use I2C0 on the default SDA and SCL pins (4, 5 on a Pico)

    i2c_init(i2c_default, 100 * 1000);
    gpio_set_function(PICO_DEFAULT_I2C_SDA_PIN, GPIO_FUNC_I2C);
    gpio_set_function(PICO_DEFAULT_I2C_SCL_PIN, GPIO_FUNC_I2C);
    gpio_pull_up(PICO_DEFAULT_I2C_SDA_PIN);
    gpio_pull_up(PICO_DEFAULT_I2C_SCL_PIN);
    // Make the I2C pins available to picotool
    bi_decl(bi_2pins_with_func(PICO_DEFAULT_I2C_SDA_PIN, PICO_DEFAULT_I2C_SCL_PIN, GPIO_FUNC_I2C));

    TimerHandle_t xTimer = xTimerCreate("SensorTimer", pdMS_TO_TICKS(1000), pdTRUE, NULL, timerCallback);

    if (xTimer == NULL)
    {
        printf("Failed to create timer\n");
        return 1;
    }

    // Start timer
    BaseType_t xTimerStarted = xTimerStart(xTimer, 0);

    if (xTimerStarted != pdPASS)
    {
        printf("Failed to start timer\n");
        return 1;
    }

    lcd_init();

        if (cyw43_arch_init()) {
        printf("failed to initialise\n");
        return 1;
    }

    cyw43_arch_enable_sta_mode();

    printf("Connecting to WiFi...\n");
    if (cyw43_arch_wifi_connect_timeout_ms(WIFI_SSID, WIFI_PASSWORD, CYW43_AUTH_WPA2_AES_PSK, 30000)) {
        printf("failed to connect.\n");
        return 1;
    } else {
        printf("Connected.\n");
    }

    
         vLaunch();


    return 0;
}  