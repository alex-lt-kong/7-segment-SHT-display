
find_package(spdlog REQUIRED)

add_library(dd
    producer.c
)
target_link_libraries(dd
    iotctrl
    modbus mosquitto
)

add_executable(dd-consumer
    consumer.cpp    
)

target_link_libraries(dd-consumer
    ${BUILD_MODULE}
    iotctrl
    gpiod pthread spdlog mosquitto
)