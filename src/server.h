//Author Luz Collado

#include <stdio.h>
#include "pico/stdlib.h"

#include <string.h>
#include <stdlib.h>

#include "pico/stdlib.h"
#include "pico/cyw43_arch.h"

#include "lwip/pbuf.h"
#include "lwip/tcp.h"

#include <math.h>
#include "hardware/gpio.h"

#define WIFI_SSID "Tom"
#define WIFI_PASSWORD "Lucero000"

#define TCP_PORT 4242
#define BUF_SIZE 2048
#define TEST_ITERATIONS 10
#define POLL_TIME_S 5
#define VAR_SIZE 10

#ifdef PICO_DEFAULT_LED_PIN
#define LED_PIN PICO_DEFAULT_LED_PIN
#endif

const uint DHT_PIN = 15;
const uint MAX_TIMINGS = 85;

typedef struct TCP_SERVER_T_
{
    struct tcp_pcb *server_pcb;
    struct tcp_pcb *client_pcb;
    bool complete;
    uint8_t buffer_sent[BUF_SIZE];
    uint8_t buffer_recv[BUF_SIZE];
    int sent_len;
    int recv_len;
    int run_count;
} TCP_SERVER_T;

static TCP_SERVER_T *tcp_server_init(void)
{
    TCP_SERVER_T *state = calloc(1, sizeof(TCP_SERVER_T));
    if (!state)
    {
        printf("failed to allocate state\n");
        return NULL;
    }
    return state;
}

typedef struct
{
    float humidity;
    float temp_celsius;
    float fahrenheit;
} dht_reading;

void read_from_dht(dht_reading *result);

static err_t tcp_server_close(void *arg)
{
    TCP_SERVER_T *state = (TCP_SERVER_T *)arg;
    err_t err = ERR_OK;
    if (state->client_pcb != NULL)
    {
        tcp_arg(state->client_pcb, NULL);
        tcp_poll(state->client_pcb, NULL, 0);
        tcp_sent(state->client_pcb, NULL);
        tcp_recv(state->client_pcb, NULL);
        tcp_err(state->client_pcb, NULL);
        err = tcp_close(state->client_pcb);
        if (err != ERR_OK)
        {
            printf("close failed %d, calling abort\n", err);
            tcp_abort(state->client_pcb);
            err = ERR_ABRT;
        }
        state->client_pcb = NULL;
    }
    if (state->server_pcb)
    {
        tcp_arg(state->server_pcb, NULL);
        tcp_close(state->server_pcb);
        state->server_pcb = NULL;
    }
    return err;
}

static err_t tcp_server_result(void *arg, int status)
{
    TCP_SERVER_T *state = (TCP_SERVER_T *)arg;
    if (status == 0)
    {
        printf("test success\n");
    }
    else
    {
        printf("test failed %d\n", status);
    }
    state->complete = true;
    return tcp_server_close(arg);
}

static err_t tcp_server_sent(void *arg, struct tcp_pcb *tpcb, u16_t len)
{

    TCP_SERVER_T *state = (TCP_SERVER_T *)arg;

    printf("tcp_server_sent %u\n", len);
    state->sent_len += len;

    if (state->sent_len >= BUF_SIZE)
    {

        // We should get the data back from the client
        state->recv_len = 0;
        printf("Waiting for buffer from client\n");
    }

    return ERR_OK;
}



err_t tcp_server_send_data(void *arg, struct tcp_pcb *tpcb, const char *data)
{
    TCP_SERVER_T *state = (TCP_SERVER_T *)arg;

     state->sent_len = snprintf(state->buffer_sent, BUF_SIZE, "%s", data);

    printf("Writing %d bytes to client\n", state->sent_len);

    cyw43_arch_lwip_check();
    err_t err = tcp_write(tpcb, state->buffer_sent, state->sent_len, TCP_WRITE_FLAG_COPY);
    if (err != ERR_OK)
    {
        printf("Failed to write data %d\n", err);
        return ERR_OK;
    }

    return ERR_OK;
}

err_t tcp_server_recv(void *arg, struct tcp_pcb *tpcb, struct pbuf *p, err_t err) {
    TCP_SERVER_T *state = (TCP_SERVER_T *)arg;
    if (!p) {
        return ERR_OK;
    }
    cyw43_arch_lwip_check();

    // Liberar el búfer de entrada ya que no lo usamos
    pbuf_free(p);

    // Actualizar y enviar los datos del sensor
    update_and_send_dht_data(arg, tpcb);

    return ERR_OK;
}


static err_t tcp_server_poll(void *arg, struct tcp_pcb *tpcb)
{
    printf("tcp_server_poll_fn\n");
    return tcp_server_result(arg, -1); // no response is an error?
}

static void tcp_server_err(void *arg, err_t err)
{
    if (err != ERR_ABRT)
    {
        printf("tcp_client_err_fn %d\n", err);
        tcp_server_result(arg, err);
    }
}

static err_t tcp_server_accept(void *arg, struct tcp_pcb *client_pcb, err_t err)
{
    TCP_SERVER_T *state = (TCP_SERVER_T *)arg;

    if (err != ERR_OK || client_pcb == NULL)
    {
        printf("Failure in accept\n");
        tcp_server_result(arg, err);
        return ERR_VAL;
    }
    printf("Client connected\n");

    state->client_pcb = client_pcb;
    tcp_arg(client_pcb, state);
    tcp_recv(client_pcb, tcp_server_recv);
    tcp_sent(client_pcb, tcp_server_sent);
    tcp_err(client_pcb, tcp_server_err);

    // enviar el valor de my_variable al cliente TCP
    // Actualizar y enviar los datos del sensor por primera vez
    update_and_send_dht_data(arg, client_pcb);
    return ERR_OK; // tcp_server_send_data(arg, state->client_pcb);
}

static bool tcp_server_open(void *arg)
{
    TCP_SERVER_T *state = (TCP_SERVER_T *)arg;
    printf("Starting server at %s on port %u\n", ip4addr_ntoa(netif_ip4_addr(netif_list)), TCP_PORT);

    struct tcp_pcb *pcb = tcp_new_ip_type(IPADDR_TYPE_ANY);
    if (!pcb)
    {
        printf("failed to create pcb\n");
        return false;
    }

    err_t err = tcp_bind(pcb, NULL, TCP_PORT);
    if (err)
    {
        printf("failed to bind to port %d\n");
        return false;
    }

    state->server_pcb = tcp_listen_with_backlog(pcb, 1);
    if (!state->server_pcb)
    {
        printf("failed to listen\n");
        if (pcb)
        {
            tcp_close(pcb);
        }
        return false;
    }

    tcp_arg(state->server_pcb, state);
    tcp_accept(state->server_pcb, tcp_server_accept);

    return true;
}



void run_tcp_server_testTask(void *pvParameters)
{
    TCP_SERVER_T *state = tcp_server_init();
    if (!state)
    {
        return;
    }
    if (!tcp_server_open(state))
    {
        tcp_server_result(state, -1);
        return;
    }

    while (1)
    { 

        //! state->complete) {
        // the following #ifdef is only here so this same example can be used in multiple modes;
        // you do not need it in your code
#if PICO_CYW43_ARCH_POLL
        // if you are using pico_cyw43_arch_poll, then you must poll periodically from your
        // main loop (not from a timer) to check for WiFi driver or lwIP work that needs to be done.
        cyw43_arch_poll();
        sleep_ms(1);
        // printf("Starting server at %s on port %u\n", ip4addr_ntoa(netif_ip4_addr(netif_list)), TCP_PORT);
#else
        // if you are not using pico_cyw43_arch_poll, then WiFI driver and lwIP work
        // is done via interrupt in the background. This sleep is just an example of some (blocking)
        // work you might be doing.
        sleep_ms(1000);
        // printf("Starting server at %s on port %u\n", ip4addr_ntoa(netif_ip4_addr(netif_list)), TCP_PORT);
#endif
    }

    free(state);
}

void read_from_dht(dht_reading *result)
{
    int data[5] = {0, 0, 0, 0, 0};
    uint last = 1;
    uint j = 0;

    gpio_set_dir(DHT_PIN, GPIO_OUT);
    gpio_put(DHT_PIN, 0);
    sleep_ms(20);
    gpio_set_dir(DHT_PIN, GPIO_IN);

#ifdef LED_PIN
    gpio_put(LED_PIN, 1);
#endif
    for (uint i = 0; i < MAX_TIMINGS; i++)
    {
        uint count = 0;
        while (gpio_get(DHT_PIN) == last)
        {
            count++;
            sleep_us(1);
            if (count == 255)
                break;
        }
        last = gpio_get(DHT_PIN);
        if (count == 255)
            break;

        if ((i >= 4) && (i % 2 == 0))
        {
            data[j / 8] <<= 1;
            if (count > 46)
                data[j / 8] |= 1;
            j++;
        }
    }
#ifdef LED_PIN
    gpio_put(LED_PIN, 0);
#endif

    if ((j >= 40) && (data[4] == ((data[0] + data[1] + data[2] + data[3]) & 0xFF)))
    {
        result->humidity = (float)((data[0] << 8) + data[1]) / 10;
        if (result->humidity > 100)
        {
            result->humidity = data[0];
        }
        result->temp_celsius = (float)(((data[2] & 0x7F) << 8) + data[3]) / 10;
        if (result->temp_celsius > 125)
        {
            result->temp_celsius = data[2];
        }
        if (data[2] & 0x80)
        {
            result->temp_celsius = -result->temp_celsius;
        }
    }
}