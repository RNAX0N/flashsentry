find_package(Qt6 REQUIRED COMPONENTS Test)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

set(FLASHSPARTAN_TEST_FIXTURES_DIR "${CMAKE_SOURCE_DIR}/tests/fixtures")

add_executable(test_types test_types.cpp)
target_include_directories(test_types PRIVATE ${CMAKE_SOURCE_DIR}/include)
target_link_libraries(test_types PRIVATE Qt6::Test)
add_test(NAME test_types COMMAND test_types)

set(FLASHSPARTAN_POLICY_SOURCES
    ${CMAKE_SOURCE_DIR}/src/AppPaths.cpp
    ${CMAKE_SOURCE_DIR}/src/policy/PolicyPaths.cpp
    ${CMAKE_SOURCE_DIR}/src/policy/PolicySocketAuth.cpp
    ${CMAKE_SOURCE_DIR}/src/policy/PolicyBlobCodec.cpp
    ${CMAKE_SOURCE_DIR}/src/policy/PolicyAudit.cpp
    ${CMAKE_SOURCE_DIR}/src/policy/PolicyStoreEngine.cpp
    ${CMAKE_SOURCE_DIR}/src/policy/PolicyGateway.cpp
    ${CMAKE_SOURCE_DIR}/src/policy/PolicyInProcessGateway.cpp
    ${CMAKE_SOURCE_DIR}/src/policy/PolicyDaemonClient.cpp
    ${CMAKE_SOURCE_DIR}/src/policy/PolicyDaemonLauncher.cpp
    ${CMAKE_SOURCE_DIR}/src/policy/PolicyServiceLocator.cpp
)

add_executable(test_database_manager
    test_database_manager.cpp
    ${CMAKE_SOURCE_DIR}/src/DatabaseManager.cpp
    ${CMAKE_SOURCE_DIR}/src/DeviceIdUtil.cpp
    ${CMAKE_SOURCE_DIR}/include/DatabaseManager.h
    ${FLASHSPARTAN_POLICY_SOURCES}
)
target_include_directories(test_database_manager PRIVATE ${CMAKE_SOURCE_DIR}/include)
target_include_directories(test_database_manager PRIVATE ${OPENSSL_INCLUDE_DIRS})
target_link_libraries(test_database_manager PRIVATE Qt6::Test Qt6::Core Qt6::Network ${OPENSSL_LIBRARIES})
add_test(NAME test_database_manager COMMAND test_database_manager)

add_executable(test_policy_auth
    test_policy_auth.cpp
    ${CMAKE_SOURCE_DIR}/src/policy/PolicySocketAuth.cpp
)
target_include_directories(test_policy_auth PRIVATE ${CMAKE_SOURCE_DIR}/include)
target_link_libraries(test_policy_auth PRIVATE Qt6::Test Qt6::Core)
add_test(NAME test_policy_auth COMMAND test_policy_auth)

add_executable(test_policy_daemon test_policy_daemon.cpp
    ${CMAKE_SOURCE_DIR}/src/AppPaths.cpp
    ${CMAKE_SOURCE_DIR}/src/policy/PolicyPaths.cpp
    ${CMAKE_SOURCE_DIR}/src/policy/PolicySocketAuth.cpp
)
target_include_directories(test_policy_daemon PRIVATE ${CMAKE_SOURCE_DIR}/include)
target_link_libraries(test_policy_daemon PRIVATE Qt6::Test Qt6::Core Qt6::Network)
add_dependencies(test_policy_daemon flashspartan-policyd)
add_test(NAME test_policy_daemon COMMAND test_policy_daemon)
set_tests_properties(test_policy_daemon PROPERTIES
    ENVIRONMENT "FLASHSPARTAN_POLICYD_TEST_BIN=$<TARGET_FILE:flashspartan-policyd>"
)

add_executable(test_device_whitelist_service
    test_device_whitelist_service.cpp
    ${CMAKE_SOURCE_DIR}/src/DeviceWhitelistService.cpp
    ${CMAKE_SOURCE_DIR}/src/DeviceIdUtil.cpp
    ${CMAKE_SOURCE_DIR}/src/DatabaseManager.cpp
    ${CMAKE_SOURCE_DIR}/include/DatabaseManager.h
    ${FLASHSPARTAN_POLICY_SOURCES}
)
target_include_directories(test_device_whitelist_service PRIVATE ${CMAKE_SOURCE_DIR}/include)
target_include_directories(test_device_whitelist_service PRIVATE ${OPENSSL_INCLUDE_DIRS})
target_link_libraries(test_device_whitelist_service PRIVATE Qt6::Test Qt6::Core Qt6::Network ${OPENSSL_LIBRARIES})
add_test(NAME test_device_whitelist_service COMMAND test_device_whitelist_service)

set(DEVICE_TRUST_TEST_SOURCES
    ${CMAKE_SOURCE_DIR}/src/DeviceDriveUtil.cpp
    ${CMAKE_SOURCE_DIR}/src/DeviceTrustCoordinator.cpp
    ${CMAKE_SOURCE_DIR}/src/DeviceWhitelistService.cpp
    ${CMAKE_SOURCE_DIR}/src/DeviceIdUtil.cpp
    ${CMAKE_SOURCE_DIR}/src/BlockedDriveStore.cpp
)

add_executable(test_device_drive_util
    test_device_drive_util.cpp
    ${DEVICE_TRUST_TEST_SOURCES}
    ${CMAKE_SOURCE_DIR}/src/DatabaseManager.cpp
    ${CMAKE_SOURCE_DIR}/include/DatabaseManager.h
    ${FLASHSPARTAN_POLICY_SOURCES}
)
target_include_directories(test_device_drive_util PRIVATE ${CMAKE_SOURCE_DIR}/include)
target_include_directories(test_device_drive_util PRIVATE ${OPENSSL_INCLUDE_DIRS})
target_link_libraries(test_device_drive_util PRIVATE Qt6::Test Qt6::Core Qt6::Network ${OPENSSL_LIBRARIES})
add_test(NAME test_device_drive_util COMMAND test_device_drive_util)

add_executable(test_device_trust_coordinator
    test_device_trust_coordinator.cpp
    ${DEVICE_TRUST_TEST_SOURCES}
    ${CMAKE_SOURCE_DIR}/src/DatabaseManager.cpp
    ${CMAKE_SOURCE_DIR}/include/DatabaseManager.h
    ${FLASHSPARTAN_POLICY_SOURCES}
)
target_include_directories(test_device_trust_coordinator PRIVATE ${CMAKE_SOURCE_DIR}/include)
target_include_directories(test_device_trust_coordinator PRIVATE ${OPENSSL_INCLUDE_DIRS})
target_link_libraries(test_device_trust_coordinator PRIVATE Qt6::Test Qt6::Core Qt6::Network ${OPENSSL_LIBRARIES})
add_test(NAME test_device_trust_coordinator COMMAND test_device_trust_coordinator)

add_executable(test_mount_options
    test_mount_options.cpp
    ${CMAKE_SOURCE_DIR}/src/MountOptionsUtil.cpp
)
target_include_directories(test_mount_options PRIVATE ${CMAKE_SOURCE_DIR}/include)
target_link_libraries(test_mount_options PRIVATE Qt6::Test Qt6::Core)
add_test(NAME test_mount_options COMMAND test_mount_options)

add_executable(test_mount_dbus_util
    test_mount_dbus_util.cpp
    ${CMAKE_SOURCE_DIR}/src/MountDBusUtil.cpp
)
target_include_directories(test_mount_dbus_util PRIVATE ${CMAKE_SOURCE_DIR}/include)
target_link_libraries(test_mount_dbus_util PRIVATE Qt6::Test Qt6::Core)
add_test(NAME test_mount_dbus_util COMMAND test_mount_dbus_util)

add_executable(test_device_id_util
    test_device_id_util.cpp
    ${CMAKE_SOURCE_DIR}/src/DeviceIdUtil.cpp
)
target_include_directories(test_device_id_util PRIVATE ${CMAKE_SOURCE_DIR}/include)
target_link_libraries(test_device_id_util PRIVATE Qt6::Test Qt6::Core)
add_test(NAME test_device_id_util COMMAND test_device_id_util)

add_executable(test_device_verification_planner
    test_device_verification_planner.cpp
    ${CMAKE_SOURCE_DIR}/src/DeviceVerificationPlanner.cpp
)
target_include_directories(test_device_verification_planner PRIVATE ${CMAKE_SOURCE_DIR}/include)
target_link_libraries(test_device_verification_planner PRIVATE Qt6::Test Qt6::Core)
add_test(NAME test_device_verification_planner COMMAND test_device_verification_planner)

add_executable(test_manifest_worker
    test_manifest_worker.cpp
    ${CMAKE_SOURCE_DIR}/src/ManifestWorker.cpp
    ${CMAKE_SOURCE_DIR}/include/ManifestWorker.h
    ${CMAKE_SOURCE_DIR}/src/ManifestService.cpp
    ${CMAKE_SOURCE_DIR}/src/MerkleTree.cpp
)
target_include_directories(test_manifest_worker PRIVATE ${CMAKE_SOURCE_DIR}/include ${OPENSSL_INCLUDE_DIRS})
target_link_libraries(test_manifest_worker PRIVATE Qt6::Test Qt6::Core Qt6::Concurrent ${OPENSSL_LIBRARIES})
add_test(NAME test_manifest_worker COMMAND test_manifest_worker)

add_executable(test_hash_worker
    test_hash_worker.cpp
    ${CMAKE_SOURCE_DIR}/src/HashWorker.cpp
    ${CMAKE_SOURCE_DIR}/include/HashWorker.h
    ${CMAKE_SOURCE_DIR}/src/AppPaths.cpp
    ${CMAKE_SOURCE_DIR}/src/HashCheckpoint.cpp
    ${CMAKE_SOURCE_DIR}/src/RawDeviceHash.cpp
    ${CMAKE_SOURCE_DIR}/src/RawDeviceHashAdvanced.cpp
)
target_include_directories(test_hash_worker PRIVATE ${CMAKE_SOURCE_DIR}/include ${OPENSSL_INCLUDE_DIRS})
target_link_libraries(test_hash_worker PRIVATE Qt6::Test Qt6::Core Qt6::Concurrent ${OPENSSL_LIBRARIES})
add_test(NAME test_hash_worker COMMAND test_hash_worker)

add_executable(test_merkle test_merkle.cpp ${CMAKE_SOURCE_DIR}/src/MerkleTree.cpp)
target_include_directories(test_merkle PRIVATE ${CMAKE_SOURCE_DIR}/include ${OPENSSL_INCLUDE_DIRS})
target_link_libraries(test_merkle PRIVATE Qt6::Test Qt6::Core ${OPENSSL_LIBRARIES})
add_test(NAME test_merkle COMMAND test_merkle)

add_executable(test_badusb_analyzer
    test_badusb_analyzer.cpp
    ${CMAKE_SOURCE_DIR}/src/BadUsbAnalyzer.cpp
)
target_include_directories(test_badusb_analyzer PRIVATE ${CMAKE_SOURCE_DIR}/include)
target_link_libraries(test_badusb_analyzer PRIVATE Qt6::Test Qt6::Core)
add_test(NAME test_badusb_analyzer COMMAND test_badusb_analyzer)

add_executable(test_badusb_baseline
    test_badusb_baseline.cpp
    ${CMAKE_SOURCE_DIR}/src/BadUsbBaselineStore.cpp
    ${CMAKE_SOURCE_DIR}/include/BadUsbBaselineStore.h
)
target_include_directories(test_badusb_baseline PRIVATE ${CMAKE_SOURCE_DIR}/include)
target_link_libraries(test_badusb_baseline PRIVATE Qt6::Test Qt6::Core)
add_test(NAME test_badusb_baseline COMMAND test_badusb_baseline)

set(ISO_CATALOG_SOURCES
    ${CMAKE_SOURCE_DIR}/src/iso_catalog/IsoCatalogBuilders.cpp
    ${CMAKE_SOURCE_DIR}/src/iso_catalog/IsoCatalogMatch.cpp
    ${CMAKE_SOURCE_DIR}/src/iso_catalog/IsoCatalogUtil.cpp
)

add_executable(test_iso_catalog
    test_iso_catalog.cpp
    ${ISO_CATALOG_SOURCES}
    ${CMAKE_SOURCE_DIR}/src/IsoCatalogManifest.cpp
)
target_include_directories(test_iso_catalog PRIVATE ${CMAKE_SOURCE_DIR}/include)
target_link_libraries(test_iso_catalog PRIVATE Qt6::Test Qt6::Core Qt6::Network)
target_compile_definitions(test_iso_catalog PRIVATE FLASHSPARTAN_VERSION="${PROJECT_VERSION}")
target_sources(test_iso_catalog PRIVATE ${CMAKE_SOURCE_DIR}/resources/resources.qrc)
add_test(NAME test_iso_catalog COMMAND test_iso_catalog)

add_executable(test_autostart
    test_autostart.cpp
    ${CMAKE_SOURCE_DIR}/src/AutostartManager.cpp
)
target_include_directories(test_autostart PRIVATE ${CMAKE_SOURCE_DIR}/include)
target_link_libraries(test_autostart PRIVATE Qt6::Test Qt6::Core)
add_test(NAME test_autostart COMMAND test_autostart)

add_executable(test_iso_checksum test_iso_checksum.cpp ${CMAKE_SOURCE_DIR}/src/IsoChecksum.cpp)
target_include_directories(test_iso_checksum PRIVATE ${CMAKE_SOURCE_DIR}/include)
target_link_libraries(test_iso_checksum PRIVATE Qt6::Test Qt6::Core)
add_test(NAME test_iso_checksum COMMAND test_iso_checksum)

add_executable(test_iso_scan_rules
    test_iso_scan_rules.cpp
    ${CMAKE_SOURCE_DIR}/src/SettingsProfiles.cpp
    ${CMAKE_SOURCE_DIR}/src/IsoScanRules.cpp
    ${CMAKE_SOURCE_DIR}/src/IsoVerifier.cpp
    ${ISO_CATALOG_SOURCES}
    ${CMAKE_SOURCE_DIR}/src/IsoCatalogManifest.cpp
    ${CMAKE_SOURCE_DIR}/src/IsoHttpClient.cpp
    ${CMAKE_SOURCE_DIR}/src/IsoChecksum.cpp
    ${CMAKE_SOURCE_DIR}/src/IsoVerifyCache.cpp
    ${CMAKE_SOURCE_DIR}/src/IsoVerifyReport.cpp
    ${CMAKE_SOURCE_DIR}/src/IsoVerifyUi.cpp
    ${CMAKE_SOURCE_DIR}/src/IsoBaselineService.cpp
    ${CMAKE_SOURCE_DIR}/src/IsoQuickFingerprint.cpp
    ${CMAKE_SOURCE_DIR}/src/AuditLog.cpp
)
target_include_directories(test_iso_scan_rules PRIVATE ${CMAKE_SOURCE_DIR}/include ${OPENSSL_INCLUDE_DIRS})
target_link_libraries(test_iso_scan_rules PRIVATE
    Qt6::Test Qt6::Core Qt6::Concurrent Qt6::Network ${OPENSSL_LIBRARIES})
target_compile_definitions(test_iso_scan_rules PRIVATE
    FLASHSPARTAN_VERSION="${PROJECT_VERSION}"
    FLASHSPARTAN_TEST_FIXTURES_DIR="${FLASHSPARTAN_TEST_FIXTURES_DIR}"
)
target_sources(test_iso_scan_rules PRIVATE ${CMAKE_SOURCE_DIR}/resources/resources.qrc)
add_test(NAME test_iso_scan_rules COMMAND test_iso_scan_rules)

add_executable(test_verify_report
    test_verify_report.cpp
    ${CMAKE_SOURCE_DIR}/src/IsoVerifyReport.cpp
    ${CMAKE_SOURCE_DIR}/src/IsoVerifyUi.cpp
)
target_include_directories(test_verify_report PRIVATE ${CMAKE_SOURCE_DIR}/include)
target_link_libraries(test_verify_report PRIVATE Qt6::Test Qt6::Core)
add_test(NAME test_verify_report COMMAND test_verify_report)

add_executable(test_iso_baseline_service
    test_iso_baseline_service.cpp
    ${CMAKE_SOURCE_DIR}/src/IsoBaselineService.cpp
    ${CMAKE_SOURCE_DIR}/src/IsoQuickFingerprint.cpp
)
target_include_directories(test_iso_baseline_service PRIVATE ${CMAKE_SOURCE_DIR}/include ${OPENSSL_INCLUDE_DIRS})
target_link_libraries(test_iso_baseline_service PRIVATE Qt6::Test Qt6::Core ${OPENSSL_LIBRARIES})
add_test(NAME test_iso_baseline_service COMMAND test_iso_baseline_service)

add_executable(test_main_window_iso_flow
    test_main_window_iso_flow.cpp
    ${CMAKE_SOURCE_DIR}/src/IsoBaselineService.cpp
    ${CMAKE_SOURCE_DIR}/src/IsoQuickFingerprint.cpp
    ${CMAKE_SOURCE_DIR}/src/IsoVerifyReport.cpp
    ${CMAKE_SOURCE_DIR}/src/IsoVerifyUi.cpp
    ${CMAKE_SOURCE_DIR}/src/IsoVerifier.cpp
    ${ISO_CATALOG_SOURCES}
    ${CMAKE_SOURCE_DIR}/src/IsoCatalogManifest.cpp
    ${CMAKE_SOURCE_DIR}/src/IsoHttpClient.cpp
    ${CMAKE_SOURCE_DIR}/src/IsoChecksum.cpp
    ${CMAKE_SOURCE_DIR}/src/IsoVerifyCache.cpp
    ${CMAKE_SOURCE_DIR}/src/AuditLog.cpp
    ${CMAKE_SOURCE_DIR}/src/IsoScanRules.cpp
)
target_include_directories(test_main_window_iso_flow PRIVATE ${CMAKE_SOURCE_DIR}/include ${OPENSSL_INCLUDE_DIRS})
target_link_libraries(test_main_window_iso_flow PRIVATE
    Qt6::Test Qt6::Core Qt6::Concurrent Qt6::Network ${OPENSSL_LIBRARIES})
target_compile_definitions(test_main_window_iso_flow PRIVATE FLASHSPARTAN_VERSION="${PROJECT_VERSION}")
target_sources(test_main_window_iso_flow PRIVATE ${CMAKE_SOURCE_DIR}/resources/resources.qrc)
add_test(NAME test_main_window_iso_flow COMMAND test_main_window_iso_flow)

if(NOT WIN32)
add_executable(test_udisks2_integration
    test_udisks2_integration.cpp
    ${CMAKE_SOURCE_DIR}/src/MountManager.cpp
    ${CMAKE_SOURCE_DIR}/include/MountManager.h
    ${CMAKE_SOURCE_DIR}/src/MountDBusUtil.cpp
    ${CMAKE_SOURCE_DIR}/src/MountOptionsUtil.cpp
)
target_include_directories(test_udisks2_integration PRIVATE ${CMAKE_SOURCE_DIR}/include)
target_link_libraries(test_udisks2_integration PRIVATE Qt6::Test Qt6::Core Qt6::DBus)
add_test(NAME test_udisks2_integration COMMAND test_udisks2_integration)
endif()

set(ISO_VERIFY_TEST_SOURCES
    ${CMAKE_SOURCE_DIR}/src/IsoScanRules.cpp
    ${CMAKE_SOURCE_DIR}/src/IsoVerifier.cpp
    ${ISO_CATALOG_SOURCES}
    ${CMAKE_SOURCE_DIR}/src/IsoCatalogManifest.cpp
    ${CMAKE_SOURCE_DIR}/src/IsoHttpClient.cpp
    ${CMAKE_SOURCE_DIR}/src/IsoChecksum.cpp
    ${CMAKE_SOURCE_DIR}/src/IsoVerifyCache.cpp
    ${CMAKE_SOURCE_DIR}/src/IsoVerifyReport.cpp
    ${CMAKE_SOURCE_DIR}/src/IsoVerifyUi.cpp
    ${CMAKE_SOURCE_DIR}/src/IsoBaselineService.cpp
    ${CMAKE_SOURCE_DIR}/src/IsoQuickFingerprint.cpp
    ${CMAKE_SOURCE_DIR}/src/AuditLog.cpp
)
add_executable(test_iso_verify_integration
    test_iso_verify_integration.cpp
    ${ISO_VERIFY_TEST_SOURCES}
)
target_include_directories(test_iso_verify_integration PRIVATE ${CMAKE_SOURCE_DIR}/include ${OPENSSL_INCLUDE_DIRS})
target_link_libraries(test_iso_verify_integration PRIVATE
    Qt6::Test Qt6::Core Qt6::Concurrent Qt6::Network ${OPENSSL_LIBRARIES})
target_compile_definitions(test_iso_verify_integration PRIVATE
    FLASHSPARTAN_VERSION="${PROJECT_VERSION}"
    FLASHSPARTAN_TEST_FIXTURES_DIR="${FLASHSPARTAN_TEST_FIXTURES_DIR}"
)
target_sources(test_iso_verify_integration PRIVATE ${CMAKE_SOURCE_DIR}/resources/resources.qrc)
add_test(NAME test_iso_verify_integration COMMAND test_iso_verify_integration)

add_executable(test_iso_verify_publisher_mock
    test_iso_verify_publisher_mock.cpp
    ${ISO_VERIFY_TEST_SOURCES}
)
target_include_directories(test_iso_verify_publisher_mock PRIVATE ${CMAKE_SOURCE_DIR}/include ${OPENSSL_INCLUDE_DIRS})
target_link_libraries(test_iso_verify_publisher_mock PRIVATE
    Qt6::Test Qt6::Core Qt6::Concurrent Qt6::Network ${OPENSSL_LIBRARIES})
target_compile_definitions(test_iso_verify_publisher_mock PRIVATE
    FLASHSPARTAN_VERSION="${PROJECT_VERSION}"
    FLASHSPARTAN_TEST_FIXTURES_DIR="${FLASHSPARTAN_TEST_FIXTURES_DIR}"
)
target_sources(test_iso_verify_publisher_mock PRIVATE ${CMAKE_SOURCE_DIR}/resources/resources.qrc)
add_test(NAME test_iso_verify_publisher_mock COMMAND test_iso_verify_publisher_mock)

add_executable(test_iso_http_mock test_iso_http_mock.cpp ${CMAKE_SOURCE_DIR}/src/IsoHttpClient.cpp)
target_include_directories(test_iso_http_mock PRIVATE ${CMAKE_SOURCE_DIR}/include)
target_link_libraries(test_iso_http_mock PRIVATE Qt6::Test Qt6::Core Qt6::Network)
target_compile_definitions(test_iso_http_mock PRIVATE FLASHSPARTAN_VERSION="${PROJECT_VERSION}")
add_test(NAME test_iso_http_mock COMMAND test_iso_http_mock)
