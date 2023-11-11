add_executable(thm
    main.c
    global_vars.c
    event_loops.c
)

target_link_libraries(thm 
    iotctrl
    gpiod modbus pthread curl
)