include(FindPkgConfig)
pkg_check_modules(CURLPP REQUIRED curlpp)
pkg_check_modules(Mosquitto IMPORTED_TARGET libmosquitto REQUIRED)
find_package(nlohmann_json CONFIG REQUIRED)

add_library(hko
    hko.cpp
)

target_link_libraries(hko
  ${CURLPP_LDFLAGS} PkgConfig::Mosquitto
)
