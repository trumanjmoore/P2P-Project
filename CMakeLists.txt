cmake_minimum_required(VERSION 3.26)
project(P2P_Project)

set(CMAKE_CXX_STANDARD 14)

include_directories(.)

add_executable(P2P_Project
        main.cpp
        PeerProcess.cpp
        PeerProcess.h
        BitfieldManager.h
        BitfieldManager.cpp
        FileHandling.cpp
        messageSender.cpp
        logger.cpp)
