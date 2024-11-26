add_library(ch
    ch.c
    ../module_lib.c
)

target_link_libraries(ch
    iotctrl
    modbus mosquitto gpiod
)
