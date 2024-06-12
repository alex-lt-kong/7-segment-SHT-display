message( STATUS "Available modules at ${CMAKE_CURRENT_SOURCE_DIR}/modules:" )
    set(DIRECTORY_TO_LIST "${CMAKE_CURRENT_SOURCE_DIR}/modules")

    # Use file(GLOB) to get a list of all files in the directory
    file(GLOB FILES_IN_DIRECTORY "${DIRECTORY_TO_LIST}/*")

    # Iterate through the list of files and print just the file names to stdout
    foreach(FILE ${FILES_IN_DIRECTORY})
        get_filename_component(FILENAME ${FILE} NAME)
        message(STATUS "${FILENAME}")
    endforeach()

if(NOT DEFINED MODULE_NAME)
    message( FATAL_ERROR "MODULE_NAME undefined, set your MODULE_NAME by -DMODULE_NAME=<module_name>.c")
endif()

add_executable(sdp
    main.c
    global_vars.c
    event_loops.c
    utils.c
    module_lib.c
    ./modules/${MODULE_NAME}
)

target_link_libraries(sdp
    iotctrl
    gpiod modbus pthread curl json-c
)