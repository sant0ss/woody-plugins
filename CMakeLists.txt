cmake_minimum_required(VERSION 3.22)

project(WoodyPlugins VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# JUCE is shared by every Woody plugin
add_subdirectory(JUCE)

# Which plugins to configure; pass -DWOODY_PLUGINS="a;b" to build only some of them.
# A plugin folder that does not exist yet is skipped.
set(WOODY_PLUGINS "drum-woodys;synth-woodys;eq-woodys;sat-woodys;comp-woodys" CACHE STRING "Plugins to configure")

foreach(plugin IN LISTS WOODY_PLUGINS)
    if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/${plugin}/CMakeLists.txt")
        add_subdirectory(${plugin})
    endif()
endforeach()
