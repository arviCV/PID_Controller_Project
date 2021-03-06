project(PID)

cmake_minimum_required (VERSION 3.5)

option(WITH_DEBUG_PRINT "Output debug text to console" OFF)

if(WITH_DEBUG_PRINT)
  add_definitions(-DDEBUG_PRINT)
endif()

add_definitions(-std=c++11)

set(CXX_FLAGS "-Wall")
set(CMAKE_CXX_FLAGS "${CXX_FLAGS}")

set(sources src/PID.cpp src/twiddle.cpp src/simple_timer.cpp src/vehicle_controller.cpp src/cte_eval.cpp)
set(test_sources test/setup.cpp test/test_PID.cpp test/test_twiddle.cpp test/test_cte_eval.cpp)

include_directories(/usr/local/include)
link_directories(/usr/local/lib)

if(${CMAKE_SYSTEM_NAME} MATCHES "Darwin")

include_directories(/usr/local/opt/openssl/include)
link_directories(/usr/local/opt/openssl/lib)
link_directories(/usr/local/Cellar/libuv/1*/lib)

endif(${CMAKE_SYSTEM_NAME} MATCHES "Darwin")

# Add source files to a library to avoid additional compile for unit tests.
add_library (pid_lib ${sources})

# Target app
add_executable(pid src/main.cpp)
target_link_libraries(pid pid_lib z ssl uv uWS)

# Unit tests
add_executable(pid_unit_tests ${test_sources})
target_link_libraries(pid_unit_tests pid_lib)
enable_testing()
add_test(NAME PID_Test_Suite COMMAND pid_unit_tests)
