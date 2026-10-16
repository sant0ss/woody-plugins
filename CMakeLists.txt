cmake_minimum_required(VERSION 3.22)

project(WoodyPlugins VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# JUCE is shared by every Woody plugin
add_subdirectory(JUCE)

add_subdirectory(drum-woodys)
