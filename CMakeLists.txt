cmake_minimum_required(VERSION 3.16.0)

execute_process(
  COMMAND git rev-parse --short HEAD
  OUTPUT_VARIABLE GIT_COMMIT_HASH
  OUTPUT_STRIP_TRAILING_WHITESPACE
)

add_definitions(-DGIT_COMMIT_HASH="${GIT_COMMIT_HASH}")

message(STATUS "GIT_COMMIT_HASH: ${GIT_COMMIT_HASH}")

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(SensorLink)