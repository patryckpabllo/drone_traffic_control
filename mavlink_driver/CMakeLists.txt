cmake_minimum_required (VERSION 3.0)

project (mavlink_driver)

option(USE_STANDALONE_ASIO "set ON to use standalone Asio instead of Boost.Asio" OFF)
option(BUILD_TESTING "set ON to build library tests" OFF)
option(USE_OPENSSL "set OFF to build without OpenSSL" ON)


if(NOT MSVC)
    add_compile_options(-std=c++11 -Wall -Wextra)
    if (CMAKE_CXX_COMPILER_ID MATCHES "Clang")
        add_compile_options(-Wthread-safety)
    endif()
else()
    add_compile_options(/W1)
endif()

add_library(mavlink_driver INTERFACE)

target_include_directories(mavlink_driver INTERFACE ${CMAKE_CURRENT_SOURCE_DIR})

find_package(Threads REQUIRED)
target_link_libraries(mavlink_driver INTERFACE ${CMAKE_THREAD_LIBS_INIT})

## BEGIN MAVSDK
find_package(MAVSDK REQUIRED)

target_link_libraries(mavlink_driver INTERFACE
    MAVSDK::mavsdk_telemetry
    MAVSDK::mavsdk_action
    MAVSDK::mavsdk
)
## END MAVSDK

# TODO 2020 when Debian Jessie LTS ends:
# Remove Boost system, thread, regex components; use Boost::<component> aliases; remove Boost target_include_directories
if(USE_STANDALONE_ASIO)
    target_compile_definitions(mavlink_driver INTERFACE USE_STANDALONE_ASIO)
    find_path(ASIO_PATH asio.hpp)
    if(NOT ASIO_PATH)
        message(FATAL_ERROR "Standalone Asio not found")
    else()
        target_include_directories(mavlink_driver INTERFACE ${ASIO_PATH})
    endif()
else()
    find_package(Boost 1.53.0 COMPONENTS system thread REQUIRED)
    target_link_libraries(mavlink_driver INTERFACE ${Boost_LIBRARIES})
    target_include_directories(mavlink_driver INTERFACE ${Boost_INCLUDE_DIR})
    if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU" AND CMAKE_CXX_COMPILER_VERSION VERSION_LESS 4.9)
        target_compile_definitions(mavlink_driver INTERFACE USE_BOOST_REGEX)
        find_package(Boost 1.53.0 COMPONENTS regex REQUIRED)
        target_link_libraries(mavlink_driver INTERFACE ${Boost_LIBRARIES})
        target_include_directories(mavlink_driver INTERFACE ${Boost_INCLUDE_DIR})
    endif()
endif()


# If mavlink_driver is not a sub-project:
if(CMAKE_SOURCE_DIR STREQUAL "${CMAKE_CURRENT_SOURCE_DIR}")
    find_package(Boost 1.53.0 COMPONENTS system thread filesystem)
    if(Boost_FOUND)
        add_executable(http_server main.cpp)
        target_link_libraries(http_server mavlink_driver)
        target_link_libraries(http_server ${Boost_LIBRARIES})
        target_include_directories(http_server PRIVATE ${Boost_INCLUDE_DIR})
     endif()

    install(TARGETS http_server DESTINATION bin)

 endif()
