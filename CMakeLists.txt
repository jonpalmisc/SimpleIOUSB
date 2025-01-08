cmake_minimum_required(VERSION 3.14 FATAL_ERROR)

project(SimpleIOUSB LANGUAGES C)

option(SI_BUILD_EXAMPLES "Build example applications" NO)

add_library(SimpleIOUSB Source/SimpleIOUSB.c)
target_compile_features(SimpleIOUSB PRIVATE c_std_99)
target_compile_options(SimpleIOUSB PRIVATE "-Wall" "-Wextra" "-Wpedantic" "-Wno-gcc-compat")
target_include_directories(SimpleIOUSB PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/Source)
target_link_libraries(SimpleIOUSB PUBLIC "-framework CoreFoundation -framework IOKit")

if(SI_BUILD_EXAMPLES)
    message(STATUS "SimpleIOUSB: Example applications will be built")

    add_executable(connect Examples/Connect.c)
    target_link_libraries(connect PRIVATE SimpleIOUSB)

    add_executable(connect-async Examples/ConnectAsync.c)
    target_link_libraries(connect-async PRIVATE SimpleIOUSB)

    add_executable(inspect Examples/Inspect.c)
    target_link_libraries(inspect PRIVATE SimpleIOUSB)
endif()

install(TARGETS SimpleIOUSB)
install(FILES SimpleIOUSB.h TYPE INCLUDE)
