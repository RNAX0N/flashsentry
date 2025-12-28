cmake_minimum_required(VERSION 3.20)
project(flashsentry
    VERSION 1.0.0
    DESCRIPTION "USB Flash Drive Security Monitor"
    LANGUAGES CXX
)

# ============================================================================
# Build Configuration
# ============================================================================
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

# Build type defaults to Release for speed optimization
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

# Optimization flags
set(CMAKE_CXX_FLAGS_RELEASE "-O3 -march=native -flto -DNDEBUG")
set(CMAKE_CXX_FLAGS_DEBUG "-g -O0 -Wall -Wextra -Wpedantic -fsanitize=address,undefined")

# ============================================================================
# Dependencies
# ============================================================================
find_package(Qt6 REQUIRED COMPONENTS
    Core
    Gui
    Widgets
    DBus
    Concurrent
)

find_package(PkgConfig REQUIRED)
pkg_check_modules(LIBUDEV REQUIRED libudev)
pkg_check_modules(OPENSSL REQUIRED openssl)
pkg_check_modules(LIBNOTIFY libnotify)

# ============================================================================
# Source Files
# ============================================================================
set(SOURCES
    src/main.cpp
    src/MainWindow.cpp
    src/DeviceMonitor.cpp
    src/HashWorker.cpp
    src/DatabaseManager.cpp
    src/MountManager.cpp
    src/DeviceCard.cpp
    src/TrayIcon.cpp
    src/SettingsDialog.cpp
    src/StyleManager.cpp
)

set(HEADERS
    include/MainWindow.h
    include/DeviceMonitor.h
    include/HashWorker.h
    include/DatabaseManager.h
    include/MountManager.h
    include/DeviceCard.h
    include/TrayIcon.h
    include/SettingsDialog.h
    include/StyleManager.h
    include/Types.h
)

set(RESOURCES
    resources/resources.qrc
)

# ============================================================================
# Executable
# ============================================================================
add_executable(${PROJECT_NAME}
    ${SOURCES}
    ${HEADERS}
    ${RESOURCES}
)

target_include_directories(${PROJECT_NAME} PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${LIBUDEV_INCLUDE_DIRS}
    ${OPENSSL_INCLUDE_DIRS}
)

target_link_libraries(${PROJECT_NAME} PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
    Qt6::DBus
    Qt6::Concurrent
    ${LIBUDEV_LIBRARIES}
    ${OPENSSL_LIBRARIES}
    pthread
)

# Optional libnotify support
if(LIBNOTIFY_FOUND)
    target_compile_definitions(${PROJECT_NAME} PRIVATE HAS_LIBNOTIFY)
    target_include_directories(${PROJECT_NAME} PRIVATE ${LIBNOTIFY_INCLUDE_DIRS})
    target_link_libraries(${PROJECT_NAME} PRIVATE ${LIBNOTIFY_LIBRARIES})
endif()

# ============================================================================
# Installation
# ============================================================================
include(GNUInstallDirs)

install(TARGETS ${PROJECT_NAME}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

install(FILES packaging/flashsentry.desktop
    DESTINATION ${CMAKE_INSTALL_DATADIR}/applications
)

install(FILES resources/icons/flashsentry.svg
    DESTINATION ${CMAKE_INSTALL_DATADIR}/icons/hicolor/scalable/apps
)

install(FILES packaging/org.flashsentry.policy
    DESTINATION ${CMAKE_INSTALL_DATADIR}/polkit-1/actions
)

install(FILES packaging/99-flashsentry.rules
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/udev/rules.d
)

# ============================================================================
# Uninstall Target
# ============================================================================
if(NOT TARGET uninstall)
    configure_file(
        "${CMAKE_CURRENT_SOURCE_DIR}/cmake/cmake_uninstall.cmake.in"
        "${CMAKE_CURRENT_BINARY_DIR}/cmake_uninstall.cmake"
        IMMEDIATE @ONLY
    )
    add_custom_target(uninstall
        COMMAND ${CMAKE_COMMAND} -P ${CMAKE_CURRENT_BINARY_DIR}/cmake_uninstall.cmake
    )
endif()

# ============================================================================
# Package Info
# ============================================================================
message(STATUS "")
message(STATUS "FlashSentry v${PROJECT_VERSION}")
message(STATUS "  Build type:    ${CMAKE_BUILD_TYPE}")
message(STATUS "  C++ Standard:  ${CMAKE_CXX_STANDARD}")
message(STATUS "  Qt6 Version:   ${Qt6_VERSION}")
message(STATUS "  libnotify:     ${LIBNOTIFY_FOUND}")
message(STATUS "  Install to:    ${CMAKE_INSTALL_PREFIX}")
message(STATUS "")
