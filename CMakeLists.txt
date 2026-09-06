cmake_minimum_required(VERSION 3.24)

project(la_native LANGUAGES C)

find_package(Python3 COMPONENTS Interpreter QUIET)

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

if(MSVC)
  add_compile_options(/W4 /WX)
else()
  add_compile_options(-Wall -Wextra -Werror -Wpedantic)
endif()

add_library(la_native SHARED
  common/la_proto/src/la_proto.c
  common/la_decode/src/la_decode.c
  fw-zynq/tests/host/segment_pool.c
)

set_target_properties(la_native PROPERTIES WINDOWS_EXPORT_ALL_SYMBOLS ON)
target_compile_definitions(la_native PRIVATE LA_NATIVE_BUILD=1)

target_include_directories(la_native
  PUBLIC
    common/la_proto/include
    common/la_decode/include
    fw-zynq/tests/host
)

if(NOT WIN32)
  find_package(Threads REQUIRED)
  target_link_libraries(la_native PRIVATE Threads::Threads)
endif()

add_executable(la_native_tests
  tests/native/native_tests.c
)
target_link_libraries(la_native_tests PRIVATE la_native)
target_include_directories(la_native_tests PRIVATE
  common/la_proto/include
  common/la_decode/include
  fw-zynq/tests/host
)

add_executable(la_esp32_mocks
  tests/native/esp32_mocks.c
)
target_link_libraries(la_esp32_mocks PRIVATE la_native)
target_include_directories(la_esp32_mocks PRIVATE common/la_proto/include)

add_executable(la_esp32_mocks_relay
  tests/native/esp32_relay_host_tests.c
  fw-esp32/components/la_link/src/la_relay.c
)
target_link_libraries(la_esp32_mocks_relay PRIVATE la_native)
target_include_directories(la_esp32_mocks_relay PRIVATE
  common/la_proto/include
  fw-esp32/components/la_link/include
)

add_executable(la_native_regs
  tests/native/regs_host_tests.c
  fw-zynq/app/src/drivers/la_regs_drv.c
)
target_include_directories(la_native_regs PRIVATE
  common/la_proto/include
  fw-zynq/app/src/drivers
)

add_executable(la_native_txwin
  tests/native/txwin_host_tests.c
  fw-zynq/app/src/tasks/la_tx_win.c
)
target_include_directories(la_native_txwin PRIVATE
  fw-zynq/app/src/tasks
)

add_executable(la_native_acq
  tests/native/acq_fsm_host_tests.c
  fw-zynq/app/src/tasks/la_acq_fsm.c
)
target_include_directories(la_native_acq PRIVATE
  fw-zynq/app/src/tasks
)

add_executable(la_native_mem
  tests/native/mem_geom_host_tests.c
  fw-zynq/app/src/la_mem_geom.c
)
target_include_directories(la_native_mem PRIVATE
  fw-zynq/app/src
)

add_executable(la_native_cmd
  tests/native/cmd_dispatch_host_tests.c
  fw-zynq/app/src/tasks/la_cmd_dispatch.c
)
target_link_libraries(la_native_cmd PRIVATE la_native)
target_include_directories(la_native_cmd PRIVATE
  common/la_proto/include
  fw-zynq/app/src/tasks
)

add_executable(la_native_txpack
  tests/native/tx_pack_host_tests.c
  fw-zynq/app/src/tasks/la_tx_pack.c
)
target_link_libraries(la_native_txpack PRIVATE la_native)
target_include_directories(la_native_txpack PRIVATE
  common/la_proto/include
  fw-zynq/app/src/tasks
)

enable_testing()
add_test(NAME la_native_tests COMMAND la_native_tests)
add_test(NAME la_esp32_mocks COMMAND la_esp32_mocks)
add_test(NAME la_esp32_mocks_relay COMMAND la_esp32_mocks_relay)
add_test(NAME la_native_regs COMMAND la_native_regs)
add_test(NAME la_native_txwin COMMAND la_native_txwin)
add_test(NAME la_native_acq COMMAND la_native_acq)
add_test(NAME la_native_mem COMMAND la_native_mem)
add_test(NAME la_native_cmd COMMAND la_native_cmd)
add_test(NAME la_native_txpack COMMAND la_native_txpack)

if(Python3_EXECUTABLE)
  add_test(NAME la_native_ctypes
    COMMAND ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/tests/native/test_abi.py
            --library $<TARGET_FILE:la_native>)
  add_test(NAME la_native_ctypes_models
    COMMAND ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/tests/native/test_ctypes_models.py
            --library $<TARGET_FILE:la_native>)
endif()
