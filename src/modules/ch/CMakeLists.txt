add_library(ch
    ch.c
    
)

add_library(7seg
    ../libs/7seg.c
)

add_library(mqtt
    ../libs/mqtt.c
)


target_link_libraries(ch
    iotctrl
    7seg mqtt
    modbus mosquitto gpiod
)
