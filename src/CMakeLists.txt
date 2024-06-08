add_executable(thm
    main.c
    global_vars.c
    event_loops.c
    utils.c
    callback_stdout.c
)

target_link_libraries(thm 
    iotctrl
    gpiod modbus pthread curl json-c
)