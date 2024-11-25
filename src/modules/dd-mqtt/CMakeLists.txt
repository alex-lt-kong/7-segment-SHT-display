
find_package(spdlog REQUIRED)

add_library(dd-mqtt
    producer.c
)
target_link_libraries(dd-mqtt
    modbus mosquitto
)

add_executable(dd-mqtt-consumer
    consumer.cpp    
)

target_link_libraries(dd-mqtt-consumer
    ${BUILD_MODULE}
    iotctrl
    gpiod pthread spdlog mosquitto
)