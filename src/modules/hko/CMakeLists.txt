include(FindPkgConfig)
pkg_check_modules(CURLPP REQUIRED curlpp)
pkg_check_modules(Mosquitto IMPORTED_TARGET libmosquitto REQUIRED)
find_package(nlohmann_json CONFIG REQUIRED)
find_package(fmt CONFIG REQUIRED)

add_library(hko
    hko.cpp
   
)

add_library(mqtt
    ../libs/mqtt.c
)

target_link_libraries(hko
  fmt
  ${CURLPP_LDFLAGS} PkgConfig::Mosquitto mqtt
)
