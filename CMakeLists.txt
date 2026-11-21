cmake_minimum_required(VERSION 3.21)
project(EquipmentORM LANGUAGES CXX C)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(FetchContent)

FetchContent_Declare(
    sqlite_orm
    GIT_REPOSITORY https://github.com/fnc12/sqlite_orm.git
    GIT_TAG        v1.8.2
)
FetchContent_GetProperties(sqlite_orm)
if(NOT sqlite_orm_POPULATED)
    FetchContent_Populate(sqlite_orm)
endif()

FetchContent_Declare(
    sqlite3
    GIT_REPOSITORY https://github.com/azadkuh/sqlite-amalgamation.git
    GIT_TAG        master
)
FetchContent_GetProperties(sqlite3)
if(NOT sqlite3_POPULATED)
    FetchContent_Populate(sqlite3)
endif()

add_executable(EquipmentORM 
    src/main.cpp 
    src/SQLiteEquipmentRepository.cpp
    ${sqlite3_SOURCE_DIR}/sqlite3.c
)

target_include_directories(EquipmentORM PRIVATE 
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${sqlite_orm_SOURCE_DIR}/include 
    ${sqlite3_SOURCE_DIR}
)