if(NOT DEFINED MODULE_NAME)
    message( FATAL_ERROR "variable MODULE_NAME not set. Set it like this: -DMODULE_NAME=module_temp_display" )
endif()

add_executable(sdp
    main.c
    global_vars.c
    event_loops.c
    utils.c
    module_lib.c
    ${MODULE_NAME}.c
    # module_temp_display.c
)

target_link_libraries(sdp
    iotctrl
    gpiod modbus pthread curl json-c
)