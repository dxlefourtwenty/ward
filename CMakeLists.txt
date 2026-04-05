cmake_minimum_required(VERSION 3.16)
project(ward)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

find_package(Qt6 REQUIRED COMPONENTS Core DBus Widgets)
find_package(LayerShellQt QUIET)

qt_add_executable(ward
    src/main.cpp
    src/NotificationServer.cpp
    src/WardControl.cpp
    src/NotificationCenter.cpp
    src/NotificationPopup.cpp
    src/WardConfig.cpp
)

target_link_libraries(ward PRIVATE
    Qt6::Core
    Qt6::DBus
    Qt6::Widgets
)

if(LayerShellQt_FOUND)
    target_link_libraries(ward PRIVATE LayerShellQt::Interface)
    target_compile_definitions(ward PRIVATE WARD_HAS_LAYERSHELLQT=1)
else()
    target_compile_definitions(ward PRIVATE WARD_HAS_LAYERSHELLQT=0)
endif()

install(TARGETS ward RUNTIME DESTINATION bin)
