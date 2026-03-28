cmake_minimum_required(VERSION 3.16)
project(berkide-tui LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Platform-conditional compiler flags
# Platforma ozel derleyici bayraklari
if(MSVC)
    add_compile_options(/W4)
else()
    add_compile_options(-Wall -Wextra -Wpedantic)
endif()

# Sources
file(GLOB_RECURSE SOURCES src/*.cpp)

# IXWebSocket sources
set(IXWS_DIR ${CMAKE_SOURCE_DIR}/third_party/ixwebsocket)
file(GLOB IXWS_SOURCES ${IXWS_DIR}/*.cpp)

add_executable(berkide-tui ${SOURCES} ${IXWS_SOURCES})

target_include_directories(berkide-tui PRIVATE
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/third_party
    ${IXWS_DIR}
)

# IXWebSocket settings
set(IXWEBSOCKET_USE_TLS OFF CACHE BOOL "" FORCE)
set(IXWEBSOCKET_USE_ZLIB OFF CACHE BOOL "" FORCE)

# Platform-conditional libraries
# Platforma ozel kutuphaneler
if(WIN32)
    target_link_libraries(berkide-tui PRIVATE ws2_32)
    if(NOT MSVC)
        target_link_libraries(berkide-tui PRIVATE pthread)
    endif()
else()
    target_link_libraries(berkide-tui PRIVATE pthread dl m)
    if(APPLE)
        target_link_libraries(berkide-tui PRIVATE "-framework CoreFoundation")
    endif()
endif()
