cmake_minimum_required(VERSION 3.10)
project(route)

# C++のバージョン指定
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)

add_executable(route
    main.cpp
    route.cpp
    eliminate_route.cpp
    king_route.cpp
    knight_route.cpp
)
