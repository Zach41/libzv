cmake_minimum_required(VERSION 3.0)
project(libzv)

set (ZV_VERSION_MAJOR 0)
set (ZV_VERSION_MINOR 1)

include (CheckFunctionExists)

check_function_exists (epoll_create EPOLL_BACKEND)
check_function_exists (kqueue KQUEUE_BACKEND)

configure_file (
  "${PROJECT_SOURCE_DIR}/config.h.in"
  "${PROJECT_BINARY_DIR}/config.h"
  )

# add_library(libzv)
