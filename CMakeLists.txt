cmake_minimum_required(VERSION 3.16)
project(xv6_crash_consistency_fsck C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall -Wextra")

include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/kernel
    ${CMAKE_CURRENT_SOURCE_DIR}/user
)

add_executable(mkfs mkfs/mkfs.c)
add_executable(fsck fsck/fsck.c)
add_executable(corrupt fsck/corrupt.c)

add_custom_target(xv6_build
    COMMAND make kernel/kernel fs.img fsck/fsck fsck/corrupt
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
)
