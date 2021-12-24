cmake_minimum_required(VERSION 3.7)

project(modbus-parser C)

add_library(base INTERFACE)
target_include_directories(base
  INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}/inc/
)

if (MINGW)
  target_compile_options(base
    INTERFACE
      -std=gnu11
      -O3
      -g
      -fno-omit-frame-pointer
      -Wall
      #-Wextra
  )
endif()

add_library(modbus-parser
  src/modbus.c
  inc/modbus.h
)
target_link_libraries(modbus-parser
  PUBLIC
    base
)

add_executable(tests
  tests.c
)
target_link_libraries(tests
  PRIVATE
    base
    modbus-parser
)
