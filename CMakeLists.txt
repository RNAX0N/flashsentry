cmake_minimum_required(VERSION 3.20)
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/VERSION")
    file(READ "${CMAKE_CURRENT_SOURCE_DIR}/VERSION" _flashspartan_version_raw)
    string(STRIP "${_flashspartan_version_raw}" FLASHSPARTAN_PROJECT_VERSION)
else()
    set(FLASHSPARTAN_PROJECT_VERSION "1.4.2")
endif()
project(flashspartan
    VERSION ${FLASHSPARTAN_PROJECT_VERSION}
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

if(MSVC)
    set(CMAKE_CXX_FLAGS_RELEASE "/O2 /DNDEBUG /W4")
    set(CMAKE_CXX_FLAGS_DEBUG "/Zi /Od /W4")
else()
    set(CMAKE_CXX_FLAGS_RELEASE "-O3 -DNDEBUG -Wall -Wextra")
    set(CMAKE_CXX_FLAGS_DEBUG "-g -O0 -Wall -Wextra -Wpedantic")
endif()

set(FLASHSPARTAN_QT_COMPONENTS Core Gui Widgets Concurrent Network)
if(NOT WIN32)
    list(APPEND FLASHSPARTAN_QT_COMPONENTS DBus)
endif()
find_package(Qt6 REQUIRED COMPONENTS ${FLASHSPARTAN_QT_COMPONENTS})

if(WIN32)
    find_package(OpenSSL REQUIRED)
    set(OPENSSL_LIBRARIES OpenSSL::Crypto)
    set(OPENSSL_INCLUDE_DIRS ${OPENSSL_INCLUDE_DIR})
else()
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(LIBUDEV REQUIRED libudev)
    pkg_check_modules(OPENSSL REQUIRED openssl)
    pkg_check_modules(LIBNOTIFY libnotify)
endif()

set(SOURCES
    src/main.cpp
    src/AppPaths.cpp
    src/AppDiagnostics.cpp
    src/CrashReporter.cpp
    src/Platform.cpp
    src/MainWindow.cpp
    src/MainWindow_verify_ext.cpp
    src/MainWindow_badusb_ext.cpp
    src/MainWindow_usbpcap_ext.cpp
    src/DeviceMonitor.cpp
    src/HidDeviceMonitor.cpp
    src/HashWorker.cpp
    src/RawDeviceHash.cpp
    src/RawDeviceHashAdvanced.cpp
    src/HashCheckpoint.cpp
    src/HashOptionsDialog.cpp
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
    src/UsbPcapLocator.cpp
    src/UsbPcapInstaller.cpp
    src/VerifyCli.cpp
    src/IsoVerifySettingsLoader.cpp
    src/SettingsProfiles.cpp
    src/VerifyHistory.cpp
    src/WelcomeWizard.cpp
    src/IsoScanRules.cpp
    src/IsoVerifier.cpp
    src/IsoVerifierWorker.cpp
    src/WatchListDialog.cpp
    src/WatchListsPanel.cpp
    src/NavSidebar.cpp
    src/UsbMonitorPage.cpp
    src/DeviceHistoryPage.cpp
    src/DeviceTimelineLog.cpp
    src/BlockedDriveStore.cpp
    src/AllowBlockListPage.cpp
    src/AlertsPage.cpp
    src/ReportsPage.cpp
    src/AboutPage.cpp
    src/ContentPageShell.cpp
    src/SettingsPage.cpp
    src/PlaceholderModulePage.cpp
    src/EventDetailDialog.cpp
    src/IsoVerifierWidget.cpp
    src/DatabaseManager.cpp
    src/DeviceWhitelistService.cpp
    src/DeviceDriveUtil.cpp
    src/DeviceTrustCoordinator.cpp
    src/MountOptionsUtil.cpp
    src/MountDBusUtil.cpp
    src/DeviceIdUtil.cpp
    src/DeviceVerificationPlanner.cpp
    src/policy/PolicyPaths.cpp
    src/policy/PolicySocketAuth.cpp
    src/policy/PolicyBlobCodec.cpp
    src/policy/PolicyAudit.cpp
    src/policy/PolicyStoreEngine.cpp
    src/policy/PolicyGateway.cpp
    src/policy/PolicyInProcessGateway.cpp
    src/policy/PolicyDaemonClient.cpp
    src/policy/PolicyDaemonLauncher.cpp
    src/policy/PolicyServiceLocator.cpp
    src/MountManager.cpp
    src/DeviceCard.cpp
    src/TrayIcon.cpp
    src/SettingsDialog.cpp
    src/AutostartManager.cpp
    src/StyleManager.cpp
)

list(APPEND SOURCES src/UsbHostMonitor.cpp)

if(WIN32)
    list(APPEND SOURCES
        src/WinStorage.cpp
        src/WinUsbEnumerator.cpp
        src/HidDeviceMonitor_win.cpp)
endif()

set(HEADERS
    include/WinStorage.h
    include/WinUsbEnumerator.h
    include/UsbHostMonitor.h
    include/AppPaths.h
    include/Platform.h
    include/MainWindow.h
    include/DeviceMonitor.h
    include/HidDeviceMonitor.h
    include/HashWorker.h
    include/RawDeviceHash.h
    include/RawDeviceHashAdvanced.h
    include/HashCheckpoint.h
    include/HashOptionsDialog.h
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
    include/UsbPcapLocator.h
    include/UsbPcapInstaller.h
    include/VerifyCli.h
    include/IsoVerifySettingsLoader.h
    include/SettingsProfiles.h
    include/VerifyHistory.h
    include/WelcomeWizard.h
    include/IsoScanRules.h
    include/IsoVerifier.h
    include/IsoVerifierWorker.h
    include/WatchListDialog.h
    include/WatchListsPanel.h
    include/AppNavigation.h
    include/NavSidebar.h
    include/UsbMonitorPage.h
    include/DeviceHistoryPage.h
    include/DeviceTimelineLog.h
    include/BlockedDriveStore.h
    include/AllowBlockListPage.h
    include/AlertsPage.h
    include/ReportsPage.h
    include/AboutPage.h
    include/ContentPageShell.h
    include/SettingsPage.h
    include/PlaceholderModulePage.h
    include/EventDetailDialog.h
    include/UiEventTypes.h
    include/IsoVerifierWidget.h
    include/DatabaseManager.h
    include/policy/PolicyPaths.h
    include/policy/PolicyBlobCodec.h
    include/policy/PolicyAudit.h
    include/policy/PolicyStoreEngine.h
    include/policy/PolicyGateway.h
    include/policy/PolicyInProcessGateway.h
    include/policy/PolicyDaemonClient.h
    include/policy/PolicyDaemonLauncher.h
    include/policy/PolicyServiceLocator.h
    include/policy/PolicySnapshot.h
    include/MountManager.h
    include/DeviceCard.h
    include/TrayIcon.h
    include/SettingsDialog.h
    include/AutostartManager.h
    include/StyleManager.h
    include/Types.h
)

set(RESOURCES resources/resources.qrc)
if(WIN32)
    configure_file(
        "${CMAKE_CURRENT_SOURCE_DIR}/packaging/windows/FlashSpartan.rc.in"
        "${CMAKE_CURRENT_BINARY_DIR}/FlashSpartan.rc"
        @ONLY
    )
    list(APPEND RESOURCES "${CMAKE_CURRENT_BINARY_DIR}/FlashSpartan.rc")
endif()

add_executable(${PROJECT_NAME} ${SOURCES} ${HEADERS} ${RESOURCES})
if(WIN32)
    set_target_properties(${PROJECT_NAME} PROPERTIES
        WIN32_EXECUTABLE TRUE
        OUTPUT_NAME "FlashSpartan"
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
    Qt6::Core Qt6::Gui Qt6::Widgets Qt6::Concurrent Qt6::Network
    ${OPENSSL_LIBRARIES}
)
if(WIN32)
    target_link_libraries(${PROJECT_NAME} PRIVATE setupapi cfgmgr32 hid shell32)
else()
    target_link_libraries(${PROJECT_NAME} PRIVATE Qt6::DBus ${LIBUDEV_LIBRARIES} pthread)
endif()

if(LIBNOTIFY_FOUND)
    target_compile_definitions(${PROJECT_NAME} PRIVATE HAS_LIBNOTIFY)
    target_link_libraries(${PROJECT_NAME} PRIVATE ${LIBNOTIFY_LIBRARIES})
endif()

include(GNUInstallDirs)

set(FLASHSPARTAN_HELPER_RELDIR "${CMAKE_INSTALL_LIBDIR}/flashspartan")
set(FLASHSPARTAN_READ_HELPER "${CMAKE_INSTALL_PREFIX}/${FLASHSPARTAN_HELPER_RELDIR}/flashspartan-read-helper")

add_executable(flashspartan-policyd
    src/flashspartan-policyd.cpp
    src/AppPaths.cpp
    src/policy/PolicyPaths.cpp
    src/policy/PolicySocketAuth.cpp
    src/policy/PolicyBlobCodec.cpp
    src/policy/PolicyAudit.cpp
    src/policy/PolicyStoreEngine.cpp
)
target_include_directories(flashspartan-policyd PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${OPENSSL_INCLUDE_DIRS}
)
target_link_libraries(flashspartan-policyd PRIVATE
    Qt6::Core Qt6::Network
    ${OPENSSL_LIBRARIES}
)

add_executable(flashspartan-read-helper
    src/flashspartan-read-helper.cpp
    src/RawDeviceHash.cpp
    src/RawDeviceHashAdvanced.cpp
)
target_include_directories(flashspartan-read-helper PRIVATE include ${OPENSSL_INCLUDE_DIRS})
target_link_libraries(flashspartan-read-helper PRIVATE Qt6::Core ${OPENSSL_LIBRARIES})
if(WIN32)
    target_sources(flashspartan-read-helper PRIVATE src/WinStorage.cpp)
    target_link_libraries(flashspartan-read-helper PRIVATE setupapi cfgmgr32 shell32)
    set_target_properties(flashspartan-read-helper PROPERTIES OUTPUT_NAME "flashspartan-read-helper")
endif()
target_compile_definitions(flashspartan-read-helper PRIVATE
    FLASHSPARTAN_VERSION="${PROJECT_VERSION}"
)

if(NOT WIN32)
    configure_file(
        "${CMAKE_CURRENT_SOURCE_DIR}/packaging/org.flashspartan.policy.in"
        "${CMAKE_CURRENT_BINARY_DIR}/org.flashspartan.policy"
        @ONLY
    )
endif()

target_compile_definitions(${PROJECT_NAME} PRIVATE
    FLASHSPARTAN_READ_HELPER_PATH="$<TARGET_FILE:flashspartan-read-helper>"
    FLASHSPARTAN_VERSION="${PROJECT_VERSION}"
    FLASHSPARTAN_POLICYD_PATH="$<TARGET_FILE:flashspartan-policyd>"
)
add_dependencies(${PROJECT_NAME} flashspartan-read-helper flashspartan-policyd)

install(TARGETS ${PROJECT_NAME} RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})

if(WIN32)
    install(TARGETS flashspartan-policyd flashspartan-read-helper RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
else()
    install(TARGETS flashspartan-policyd RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
    install(TARGETS flashspartan-read-helper RUNTIME DESTINATION ${FLASHSPARTAN_HELPER_RELDIR})
    install(CODE "
      set(_helper \"\${CMAKE_INSTALL_PREFIX}/${FLASHSPARTAN_HELPER_RELDIR}/flashspartan-read-helper\")
      set(_legacy \"\${CMAKE_INSTALL_PREFIX}/${CMAKE_INSTALL_LIBEXECDIR}/flashspartan\")
      file(MAKE_DIRECTORY \"\$ENV{DESTDIR}\${_legacy}\")
      execute_process(
        COMMAND \${CMAKE_COMMAND} -E copy
          \"\$ENV{DESTDIR}\${_helper}\"
          \"\$ENV{DESTDIR}\${_legacy}/flashspartan-read-helper\"
        RESULT_VARIABLE _copy_result
      )
      if(NOT _copy_result EQUAL 0)
        message(WARNING \"Could not install libexec compatibility copy of flashspartan-read-helper\")
      endif()
    ")
    install(FILES packaging/flashspartan.desktop DESTINATION ${CMAKE_INSTALL_DATADIR}/applications)
    install(FILES resources/icons/flashspartan.svg DESTINATION ${CMAKE_INSTALL_DATADIR}/icons/hicolor/scalable/apps)
    install(FILES ${CMAKE_CURRENT_BINARY_DIR}/org.flashspartan.policy DESTINATION ${CMAKE_INSTALL_DATADIR}/polkit-1/actions)
    install(FILES packaging/99-flashspartan.rules DESTINATION ${CMAKE_INSTALL_LIBDIR}/udev/rules.d)
    install(FILES packaging/flashspartan.service DESTINATION ${CMAKE_INSTALL_DATADIR}/systemd/user)
endif()

install(FILES LICENSE DESTINATION ${CMAKE_INSTALL_DATADIR}/licenses/flashspartan)

install(
    FILES
        README.md
        CHANGELOG.md
        docs/USER_GUIDE.md
        docs/VERIFICATION.md
        docs/WINDOWS.md
        docs/MIGRATION-FROM-FLASHSENTRY.md
    DESTINATION ${CMAKE_INSTALL_DOCDIR}
)
install(FILES docs/README.md DESTINATION ${CMAKE_INSTALL_DOCDIR} RENAME docs-README.md)
install(FILES resources/iso-catalog/embedded-manifest.json
    DESTINATION ${CMAKE_INSTALL_DATADIR}/flashspartan/iso-catalog)
install(DIRECTORY packaging/iso-catalog.d/
    DESTINATION ${CMAKE_INSTALL_DATADIR}/flashspartan/iso-catalog.d
    FILES_MATCHING PATTERN "*.md" PATTERN "*.json")

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

option(FLASHSPARTAN_BUILD_TESTS "Build unit tests" ON)
if(FLASHSPARTAN_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

message(STATUS "FlashSpartan v${PROJECT_VERSION}")
message(STATUS "  Build type:    ${CMAKE_BUILD_TYPE}")
message(STATUS "  C++ Standard:  ${CMAKE_CXX_STANDARD}")
message(STATUS "  Qt6 Version:   ${Qt6_VERSION}")
message(STATUS "  libnotify:     ${LIBNOTIFY_FOUND}")
message(STATUS "  Install to:    ${CMAKE_INSTALL_PREFIX}")
message(STATUS "  Read helper:   ${FLASHSPARTAN_READ_HELPER}")
message(STATUS "  Platform:      ${CMAKE_SYSTEM_NAME}")
message(STATUS "")

if(WIN32)
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

    set(CPACK_GENERATOR "ZIP")
    set(CPACK_PACKAGE_NAME "FlashSpartan")
    set(CPACK_PACKAGE_VENDOR "FlashSpartan")
    set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "USB and ISO security monitor")
    set(CPACK_PACKAGE_INSTALL_DIRECTORY "FlashSpartan")

    set(FLASHSPARTAN_USBPCAP_INSTALLER "" CACHE FILEPATH
        "Path to official USBPcapSetup.exe to offer during NSIS install (optional)")
    if(FLASHSPARTAN_USBPCAP_INSTALLER AND EXISTS "${FLASHSPARTAN_USBPCAP_INSTALLER}")
        file(TO_NATIVE_PATH "${FLASHSPARTAN_USBPCAP_INSTALLER}" _flashspartan_usbpcap_installer)
        get_filename_component(_flashspartan_usbpcap_name "${FLASHSPARTAN_USBPCAP_INSTALLER}" NAME)
        list(APPEND CPACK_GENERATOR "NSIS")
        set(CPACK_NSIS_ENABLE_UNINSTALL_BEFORE_INSTALL ON)
        set(CPACK_NSIS_EXTRA_INSTALL_COMMANDS
            "MessageBox MB_YESNO|MB_ICONQUESTION \"Install USBPcap now? (Recommended for BadUSB USB packet capture. Requires a one-time driver install.)\" IDYES flashspartan_do_usbpcap"
            "Goto flashspartan_skip_usbpcap"
            "flashspartan_do_usbpcap:"
            "SetOutPath \"$$PLUGINSDIR\""
            "File /oname=${_flashspartan_usbpcap_name} \"${_flashspartan_usbpcap_installer}\""
            "ExecWait '$$PLUGINSDIR\\${_flashspartan_usbpcap_name} /S' $$0"
            "Delete \"$$PLUGINSDIR\\${_flashspartan_usbpcap_name}\""
            "flashspartan_skip_usbpcap:")
        message(STATUS "  NSIS+USBPcap:   ${_flashspartan_usbpcap_installer}")
    endif()

    include(CPack)

    add_subdirectory(packaging/windows)
endif()
