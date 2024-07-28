cmake_minimum_required(VERSION 3.28)
project(dbserver C)

set(CMAKE_C_STANDARD 11)

add_executable(dbserver main.c
        hashtable.c
        dbserver.c
        appserver.c
        client.c
)

target_link_libraries(dbserver ws2_32)
