cmake_minimum_required(VERSION 3.7)
project(hashcash_server)

set(CMAKE_C_STANDARD 99)

set(SOURCE_FILES server.c log.c log.h sha256.c sha256.h uint256.h server.h)
add_executable(hashcash_server ${SOURCE_FILES})