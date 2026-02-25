cmake_minimum_required(VERSION 3.10.0)
project(general_server VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include_directories(
    ${PROJECT_SOURCE_DIR}/include
)

find_library(MYSQL_LIB mysqlclient)

if(NOT MYSQL_LIB)
    message(FATAL_ERROR "mysqlclient not found")
endif()

file(GLOB SRC_FILES
    ${PROJECT_SOURCE_DIR}/src/http_conn.cpp
    ${PROJECT_SOURCE_DIR}/src/mysql_conn_pool.cpp
    ${PROJECT_SOURCE_DIR}/src/server.cpp
)

add_executable(general_server ${SRC_FILES})

target_link_libraries(general_server
    pthread
    ${MYSQL_LIB}
)
