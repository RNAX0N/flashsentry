cmake_minimum_required(VERSION 3.20)
project(flashsentry
    VERSION 1.1.0
    DESCRIPTION "USB Flash Drive Security Monitor"
    LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

set(CMAKE_CXX_FLAGS_RELEASE "-O3 -DNDEBUG")
set(CMAKE_CXX_FLAGS_DEBUG "-g -O0 -Wall -Wextra -Wpedantic")

find_package(Qt6 REQUIRED COMPONENTS Core Gui Widgets DBus Concurrent Network)

find_package(PkgConfig REQUIRED)
pkg_check_modules(LIBUDEV REQUIRED libudev)
pkg_check_modules(OPENSSL REQUIRED openssl)
pkg_check_modules(LIBNOTIFY libnotify)

set(SOURCES
    src/main.cpp
    src/MainWindow.cpp
    src/MainWindow_verify_ext.cpp
    src/DeviceMonitor.cpp
    src/HashWorker.cpp
    src/RawDeviceHash.cpp
    src/MerkleTree.cpp
    src/ManifestService.cpp
    src/ManifestWorker.cpp
    src/IsoCatalog.cpp
    src/IsoVerifier.cpp
    src/IsoVerifierWorker.cpp
    src/WatchListDialog.cpp
    src/IsoVerifierWidget.cpp
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
    include/RawDeviceHash.h
    include/MerkleTree.h
    include/ManifestService.h
    include/ManifestWorker.h
    include/IsoCatalog.h
    include/IsoVerifier.h
    include/IsoVerifierWorker.h
    include/WatchListDialog.h
    include/IsoVerifierWidget.h
    include/DatabaseManager.h
    include/MountManager.h
    include/DeviceCard.h
    include/TrayIcon.h
    include/SettingsDialog.h
    include/StyleManager.h
    include/Types.h
)

set(RESOURCES resources/resources.qrc)

add_executable(${PROJECT_NAME} ${SOURCES} ${HEADERS} ${RESOURCES})

target_include_directories(${PROJECT_NAME} PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${LIBUDEV_INCLUDE_DIRS}
    ${OPENSSL_INCLUDE_DIRS}
)

target_link_libraries(${PROJECT_NAME} PRIVATE
    Qt6::Core Qt6::Gui Qt6::Widgets Qt6::DBus Qt6::Concurrent
    Qt6::Network
    ${LIBUDEV_LIBRARIES} ${OPENSSL_LIBRARIES} pthread
)

if(LIBNOTIFY_FOUND)
    target_compile_definitions(${PROJECT_NAME} PRIVATE HAS_LIBNOTIFY)
    target_link_libraries(${PROJECT_NAME} PRIVATE ${LIBNOTIFY_LIBRARIES})
endif()

include(GNUInstallDirs)

set(FLASHSENTRY_HELPER_RELDIR "${CMAKE_INSTALL_LIBDIR}/flashsentry")
set(FLASHSENTRY_READ_HELPER "${CMAKE_INSTALL_PREFIX}/${FLASHSENTRY_HELPER_RELDIR}/flashsentry-read-helper")
target_compile_definitions(${PROJECT_NAME} PRIVATE
    FLASHSENTRY_READ_HELPER_PATH="${FLASHSENTRY_READ_HELPER}"
)

configure_file(
    packaging/org.flashsentry.policy.in
    ${CMAKE_CURRENT_BINARY_DIR}/org.flashsentry.policy
    @ONLY
)

add_executable(flashsentry-read-helper
    src/flashsentry-read-helper.cpp
    src/RawDeviceHash.cpp
)
target_include_directories(flashsentry-read-helper PRIVATE include ${OPENSSL_INCLUDE_DIRS})
target_link_libraries(flashsentry-read-helper PRIVATE Qt6::Core ${OPENSSL_LIBRARIES})

install(TARGETS ${PROJECT_NAME} RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
install(TARGETS flashsentry-read-helper RUNTIME DESTINATION ${FLASHSENTRY_HELPER_RELDIR})
install(FILES packaging/flashsentry.desktop DESTINATION ${CMAKE_INSTALL_DATADIR}/applications)
install(FILES resources/icons/flashsentry.svg DESTINATION ${CMAKE_INSTALL_DATADIR}/icons/hicolor/scalable/apps)
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/org.flashsentry.policy DESTINATION ${CMAKE_INSTALL_DATADIR}/polkit-1/actions)
install(FILES packaging/99-flashsentry.rules DESTINATION ${CMAKE_INSTALL_LIBDIR}/udev/rules.d)

install(FILES LICENSE DESTINATION ${CMAKE_INSTALL_DATADIR}/licenses/flashsentry)

install(
    FILES
        README.md
        CHANGELOG.md
        docs/USER_GUIDE.md
        docs/VERIFICATION.md
    DESTINATION ${CMAKE_INSTALL_DOCDIR}
)
install(FILES docs/README.md DESTINATION ${CMAKE_INSTALL_DOCDIR} RENAME docs-README.md)

option(FLASHSENTRY_BUILD_TESTS "Build unit tests" ON)
if(FLASHSENTRY_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()
