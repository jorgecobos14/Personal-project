cmake_minimum_required(VERSION 3.22)
project(GTAEngine)

add_library(gta_engine SHARED
    main.cpp
)

target_link_libraries(gta_engine
    android
    EGL
    GLESv3
    log
)
