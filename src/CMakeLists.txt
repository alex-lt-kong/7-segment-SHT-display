#
# ===== module selection =====
#
# Define all available modules in a list
set(AVAILABLE_MODULES
    dd-mqtt
    hko
    ch
    sample
)
# Create the selection option
set(BUILD_MODULE "NONE" CACHE STRING "Select which module to build")
# Build the list of valid choices
set(VALID_MODULES "NONE")
foreach(MODULE IN LISTS AVAILABLE_MODULES)
    list(APPEND VALID_MODULES ${MODULE})
endforeach()
# Set valid options for cmake-gui and ccmake
set_property(CACHE BUILD_MODULE PROPERTY STRINGS ${VALID_MODULES})

# Validate selection
if(BUILD_MODULE STREQUAL "NONE")
    message(FATAL_ERROR "Please select one module using -DBUILD_MODULE=<module_name>
    Available modules: ${AVAILABLE_MODULES}")
endif()

# Verify that the selected module is valid
list(FIND AVAILABLE_MODULES ${BUILD_MODULE} MODULE_INDEX)
if(MODULE_INDEX EQUAL -1)
    message(FATAL_ERROR "Invalid module '${BUILD_MODULE}'. 
    Available modules: ${AVAILABLE_MODULES}")
endif()

# Include the selected module
add_subdirectory(./modules/${BUILD_MODULE})


# Optional: Print selected module
message(STATUS "Building with module: ${BUILD_MODULE}")
add_definitions(-DMODULE_NAME="${BUILD_MODULE}")
#
# ===== module selection ends =====
#

add_executable(sdp
    main.c
    global_vars.c
    event_loops.c
    utils.c
)

target_link_libraries(sdp
    ${BUILD_MODULE}
    #iotctrl gpiod
    pthread json-c
)