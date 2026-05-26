cmake_minimum_required(VERSION 3.20)
project(flashsentry
    VERSION 1.4.2
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

set(FLASHSENTRY_QT_COMPONENTS Core Gui Widgets Concurrent Network)
if(NOT WIN32)
    list(APPEND FLASHSENTRY_QT_COMPONENTS DBus)
endif()
find_package(Qt6 REQUIRED COMPONENTS ${FLASHSENTRY_QT_COMPONENTS})

if(WIN32)
    find_package(OpenSSL REQUIRED)
    set(OPENSSL_LIBRARIES OpenSSL::Crypto)
else()
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(LIBUDEV REQUIRED libudev)
    pkg_check_modules(OPENSSL REQUIRED openssl)
    pkg_check_modules(LIBNOTIFY libnotify)
endif()

set(SOURCES
    src/main.cpp
    src/Platform.cpp
    src/MainWindow.cpp
    src/MainWindow_verify_ext.cpp
    src/MainWindow_badusb_ext.cpp
    src/DeviceMonitor.cpp
    src/HidDeviceMonitor.cpp
    src/HashWorker.cpp
    src/RawDeviceHash.cpp
    src/MerkleTree.cpp
    src/ManifestService.cpp
    src/ManifestWorker.cpp
    src/iso_catalog/IsoCatalogBuilders.cpp
    src/iso_catalog/IsoCatalogMatch.cpp
    src/iso_catalog/IsoCatalogUtil.cpp
    src/IsoCatalogManifest.cpp
    src/IsoHttpClient.cpp
    src/IsoChecksum.cpp
    src/IsoVerifyCache.cpp
    src/IsoVerifyReport.cpp
    src/AuditLog.cpp
    src/BadUsbBaselineStore.cpp
    src/BadUsbAnalyzer.cpp
    src/BadUsbWidget.cpp
    src/UsbmonCapture.cpp
    src/VerifyCli.cpp
    src/IsoVerifySettingsLoader.cpp
    src/SettingsProfiles.cpp
    src/WelcomeWizard.cpp
    src/IsoScanRules.cpp
    src/IsoVerifier.cpp
    src/IsoVerifierWorker.cpp
    src/WatchListDialog.cpp
    src/IsoVerifierWidget.cpp
    src/DatabaseManager.cpp
    src/MountManager.cpp
    src/DeviceCard.cpp
    src/TrayIcon.cpp
    src/SettingsDialog.cpp
    src/AutostartManager.cpp
    src/StyleManager.cpp
)

set(HEADERS
    include/MainWindow.h
    include/Platform.h
    include/DeviceMonitor.h
    include/HidDeviceMonitor.h
    include/HashWorker.h
    include/RawDeviceHash.h
    include/MerkleTree.h
    include/ManifestService.h
    include/ManifestWorker.h
    include/IsoCatalog.h
    include/IsoCatalogInternal.h
    include/IsoCatalogManifest.h
    include/IsoHttpClient.h
    include/IsoChecksum.h
    include/IsoVerifyOptions.h
    include/IsoVerifyCache.h
    include/IsoVerifyReport.h
    include/AuditLog.h
    include/BadUsbBaselineStore.h
    include/BadUsbAnalyzer.h
    include/BadUsbWidget.h
    include/UsbmonCapture.h
    include/VerifyCli.h
    include/IsoVerifySettingsLoader.h
    include/SettingsProfiles.h
    include/WelcomeWizard.h
    include/IsoScanRules.h
    include/IsoVerifier.h
    include/IsoVerifierWorker.h
    include/WatchListDialog.h
    include/IsoVerifierWidget.h
    include/DatabaseManager.h
    include/MountManager.h
    include/DeviceCard.h
    include/TrayIcon.h
    include/SettingsDialog.h
    include/AutostartManager.h
    include/StyleManager.h
    include/Types.h
)

set(RESOURCES resources/resources.qrc)

add_executable(${PROJECT_NAME} ${SOURCES} ${HEADERS} ${RESOURCES})
if(WIN32)
    set_target_properties(${PROJECT_NAME} PROPERTIES
        WIN32_EXECUTABLE TRUE
        OUTPUT_NAME "FlashSentry"
    )
endif()

target_include_directories(${PROJECT_NAME} PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${OPENSSL_INCLUDE_DIRS}
)
if(NOT WIN32)
    target_include_directories(${PROJECT_NAME} PRIVATE ${LIBUDEV_INCLUDE_DIRS})
endif()

target_link_libraries(${PROJECT_NAME} PRIVATE
    Qt6::Core Qt6::Gui Qt6::Widgets Qt6::Concurrent
    Qt6::Network
    ${OPENSSL_LIBRARIES}
)
if(NOT WIN32)
    target_link_libraries(${PROJECT_NAME} PRIVATE Qt6::DBus ${LIBUDEV_LIBRARIES} pthread)
endif()

if(LIBNOTIFY_FOUND)
    target_compile_definitions(${PROJECT_NAME} PRIVATE HAS_LIBNOTIFY)
    target_link_libraries(${PROJECT_NAME} PRIVATE ${LIBNOTIFY_LIBRARIES})
endif()

include(GNUInstallDirs)

set(FLASHSENTRY_HELPER_RELDIR "${CMAKE_INSTALL_LIBDIR}/flashsentry")
set(FLASHSENTRY_READ_HELPER "${CMAKE_INSTALL_PREFIX}/${FLASHSENTRY_HELPER_RELDIR}/flashsentry-read-helper")
target_compile_definitions(${PROJECT_NAME} PRIVATE
    FLASHSENTRY_READ_HELPER_PATH="${FLASHSENTRY_READ_HELPER}"
    FLASHSENTRY_VERSION="${PROJECT_VERSION}"
)

if(NOT WIN32)
    configure_file(
        "${CMAKE_CURRENT_SOURCE_DIR}/packaging/org.flashsentry.policy.in"
        "${CMAKE_CURRENT_BINARY_DIR}/org.flashsentry.policy"
        @ONLY
    )

    add_executable(flashsentry-read-helper
        src/flashsentry-read-helper.cpp
        src/RawDeviceHash.cpp
    )
    target_include_directories(flashsentry-read-helper PRIVATE include ${OPENSSL_INCLUDE_DIRS})
    target_link_libraries(flashsentry-read-helper PRIVATE Qt6::Core ${OPENSSL_LIBRARIES})
    target_compile_definitions(flashsentry-read-helper PRIVATE
        FLASHSENTRY_VERSION="${PROJECT_VERSION}"
    )
endif()

install(TARGETS ${PROJECT_NAME} RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})

if(NOT WIN32)
install(TARGETS flashsentry-read-helper RUNTIME DESTINATION ${FLASHSENTRY_HELPER_RELDIR})
install(CODE "
  set(_helper \"\${CMAKE_INSTALL_PREFIX}/${FLASHSENTRY_HELPER_RELDIR}/flashsentry-read-helper\")
  set(_legacy \"\${CMAKE_INSTALL_PREFIX}/${CMAKE_INSTALL_LIBEXECDIR}/flashsentry\")
  file(MAKE_DIRECTORY \"\$ENV{DESTDIR}\${_legacy}\")
  execute_process(
    COMMAND \${CMAKE_COMMAND} -E copy
      \"\$ENV{DESTDIR}\${_helper}\"
      \"\$ENV{DESTDIR}\${_legacy}/flashsentry-read-helper\"
    RESULT_VARIABLE _copy_result
  )
  if(NOT _copy_result EQUAL 0)
    message(WARNING \"Could not install libexec compatibility copy of flashsentry-read-helper\")
  endif()
")
endif()

if(NOT WIN32)
install(FILES packaging/flashsentry.desktop DESTINATION ${CMAKE_INSTALL_DATADIR}/applications)
install(FILES resources/icons/flashsentry.svg DESTINATION ${CMAKE_INSTALL_DATADIR}/icons/hicolor/scalable/apps)
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/org.flashsentry.policy DESTINATION ${CMAKE_INSTALL_DATADIR}/polkit-1/actions)
install(FILES packaging/99-flashsentry.rules DESTINATION ${CMAKE_INSTALL_LIBDIR}/udev/rules.d)
install(FILES packaging/flashsentry.service DESTINATION ${CMAKE_INSTALL_DATADIR}/systemd/user)
endif()

install(FILES LICENSE DESTINATION ${CMAKE_INSTALL_DATADIR}/licenses/flashsentry)

install(
    FILES
        README.md
        CHANGELOG.md
        docs/USER_GUIDE.md
        docs/VERIFICATION.md
        docs/WINDOWS.md
    DESTINATION ${CMAKE_INSTALL_DOCDIR}
)
install(FILES docs/README.md DESTINATION ${CMAKE_INSTALL_DOCDIR} RENAME docs-README.md)
install(FILES resources/iso-catalog/embedded-manifest.json
    DESTINATION ${CMAKE_INSTALL_DATADIR}/flashsentry/iso-catalog)
install(DIRECTORY packaging/iso-catalog.d/
    DESTINATION ${CMAKE_INSTALL_DATADIR}/flashsentry/iso-catalog.d
    FILES_MATCHING PATTERN "*.md" PATTERN "*.json")

if(WIN32)
    install(FILES resources/iso-catalog/embedded-manifest.json
        DESTINATION iso-catalog)
    install(DIRECTORY packaging/iso-catalog.d/
        DESTINATION iso-catalog.d
        FILES_MATCHING PATTERN "*.md" PATTERN "*.json")

    find_program(WINDEPLOYQT_EXECUTABLE windeployqt
        HINTS "${Qt6_DIR}/../../../bin" "$ENV{Qt6_DIR}/../../../bin")
    if(WINDEPLOYQT_EXECUTABLE)
        add_custom_target(deploy-windows-runtime
            COMMAND "${WINDEPLOYQT_EXECUTABLE}"
                    --release
                    --compiler-runtime
                    --no-translations
                    "$<TARGET_FILE:${PROJECT_NAME}>"
            DEPENDS ${PROJECT_NAME}
            COMMENT "Deploying Qt runtime with windeployqt")
    endif()

    set(CPACK_GENERATOR "NSIS;ZIP")
    set(CPACK_PACKAGE_NAME "FlashSentry")
    set(CPACK_PACKAGE_VENDOR "FlashSentry")
    set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "USB and ISO security monitor")
    set(CPACK_PACKAGE_INSTALL_DIRECTORY "FlashSentry")
    set(CPACK_NSIS_DISPLAY_NAME "FlashSentry")
    set(CPACK_NSIS_PACKAGE_NAME "FlashSentry")
    set(CPACK_NSIS_ENABLE_UNINSTALL_BEFORE_INSTALL ON)
    set(CPACK_NSIS_MODIFY_PATH OFF)
    set(CPACK_PACKAGE_EXECUTABLES "FlashSentry" "FlashSentry")
endif()

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

option(FLASHSENTRY_BUILD_TESTS "Build unit tests" ON)
if(FLASHSENTRY_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

message(STATUS "FlashSentry v${PROJECT_VERSION}")
message(STATUS "  Build type:    ${CMAKE_BUILD_TYPE}")
message(STATUS "  C++ Standard:  ${CMAKE_CXX_STANDARD}")
message(STATUS "  Qt6 Version:   ${Qt6_VERSION}")
message(STATUS "  libnotify:     ${LIBNOTIFY_FOUND}")
message(STATUS "  Install to:    ${CMAKE_INSTALL_PREFIX}")
message(STATUS "  Read helper:   ${FLASHSENTRY_READ_HELPER}")
message(STATUS "")

if(WIN32)
    include(CPack)
endif()
