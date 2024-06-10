add_executable(sdp
    main.c
    global_vars.c
    event_loops.c
    utils.c
    module_temp_display.c
)

target_link_libraries(sdp
    iotctrl
    gpiod modbus pthread curl json-c
)