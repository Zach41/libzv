cmake_minimum_required(VERSION 3.0)
project(libzv)

set (ZV_VERSION_MAJOR 0)
set (ZV_VERSION_MINOR 1)

set (ZV_MAX_PRI 127)
set (ZV_MIN_PRI 0)

if(CMAKE_HOST_UNIX)
  execute_process(COMMAND getconf OPEN_MAX
    OUTPUT_VARIABLE ZV_OPENFD_MAX)
else(CMAKE_HOST_UNIX)
  set (ZV_OPENFD_MAX 0)
endif(CMAKE_HOST_UNIX)

include (CheckFunctionExists)

check_function_exists (epoll_create EPOLL_BACKEND)
check_function_exists (kqueue KQUEUE_BACKEND)
check_function_exists (select SELECT_BACKEND)
check_function_exists (poll POLL_BACKEND)

check_function_exists (clock_gettime CLOCK_TIME_BACKEND)

if(EPOLL_BACKEND)
set (EPOLL_EVENTBLK 64)
endif(EPOLL_BACKEND)

configure_file (
  "${PROJECT_SOURCE_DIR}/config.h.in"
  "${PROJECT_BINARY_DIR}/config.h"
  )

add_executable(theap_test.out zv_theaptest.c zv.c timer_heap.c)
target_link_libraries(theap_test.out cmocka)

# add_library(libzv)
