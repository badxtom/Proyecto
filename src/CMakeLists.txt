
cmake_minimum_required(VERSION 3.12)

set(PICO_BOARD pico_w)
set(WIFI_SSID "DANTE")
set(WIFI_PASSWORD "TIRURIN16")

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)

add_executable(${NAME}
        main.c
        )


# Pull in our pico_stdlib which pulls in commonly used features
target_link_libraries(${NAME} 
     pico_binary_info
     hardware_i2c
	 pico_stdlib
     FreeRTOS-Kernel-Heap4 
     hardware_gpio
     pico_time
     pico_cyw43_arch_lwip_poll
	)
	
target_include_directories(${NAME} PRIVATE
    ${CMAKE_CURRENT_LIST_DIR}
    ${CMAKE_CURRENT_LIST_DIR}/.. # for our common lwipopts
     )
     
target_compile_definitions(${NAME} PRIVATE
    configNUM_CORES=2
    WIFI_SSID=\"${WIFI_SSID}\"
    WIFI_PASSWORD=\"${WIFI_PASSWORD}\"
)

# create map/bin/hex file etc.
pico_add_extra_outputs(${NAME})

# enable usb output, disable uart output
pico_enable_stdio_usb(${NAME} 1)
pico_enable_stdio_uart(${NAME} 0)