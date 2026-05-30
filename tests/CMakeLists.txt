find_package(Qt6 REQUIRED COMPONENTS Test)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

set(FLASHSENTRY_TEST_FIXTURES_DIR "${CMAKE_SOURCE_DIR}/tests/fixtures")

add_executable(test_types test_types.cpp)
target_include_directories(test_types PRIVATE ${CMAKE_SOURCE_DIR}/include)
target_link_libraries(test_types PRIVATE Qt6::Test)
add_test(NAME test_types COMMAND test_types)

set(FLASHSENTRY_POLICY_SOURCES
    ${CMAKE_SOURCE_DIR}/src/policy/PolicyPaths.cpp
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
    ${CMAKE_SOURCE_DIR}/include/DatabaseManager.h
    ${FLASHSENTRY_POLICY_SOURCES}
)
target_include_directories(test_database_manager PRIVATE ${CMAKE_SOURCE_DIR}/include)
target_include_directories(test_database_manager PRIVATE ${OPENSSL_INCLUDE_DIRS})
target_link_libraries(test_database_manager PRIVATE Qt6::Test Qt6::Core Qt6::Network ${OPENSSL_LIBRARIES})
add_test(NAME test_database_manager COMMAND test_database_manager)

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
target_compile_definitions(test_iso_catalog PRIVATE FLASHSENTRY_VERSION="${PROJECT_VERSION}")
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
    ${CMAKE_SOURCE_DIR}/src/AuditLog.cpp
)
target_include_directories(test_iso_scan_rules PRIVATE ${CMAKE_SOURCE_DIR}/include ${OPENSSL_INCLUDE_DIRS})
target_link_libraries(test_iso_scan_rules PRIVATE
    Qt6::Test Qt6::Core Qt6::Concurrent Qt6::Network ${OPENSSL_LIBRARIES})
target_compile_definitions(test_iso_scan_rules PRIVATE
    FLASHSENTRY_VERSION="${PROJECT_VERSION}"
    FLASHSENTRY_TEST_FIXTURES_DIR="${FLASHSENTRY_TEST_FIXTURES_DIR}"
)
target_sources(test_iso_scan_rules PRIVATE ${CMAKE_SOURCE_DIR}/resources/resources.qrc)
add_test(NAME test_iso_scan_rules COMMAND test_iso_scan_rules)

add_executable(test_verify_report test_verify_report.cpp ${CMAKE_SOURCE_DIR}/src/IsoVerifyReport.cpp)
target_include_directories(test_verify_report PRIVATE ${CMAKE_SOURCE_DIR}/include)
target_link_libraries(test_verify_report PRIVATE Qt6::Test Qt6::Core)
add_test(NAME test_verify_report COMMAND test_verify_report)

set(ISO_VERIFY_TEST_SOURCES
    ${CMAKE_SOURCE_DIR}/src/IsoScanRules.cpp
    ${CMAKE_SOURCE_DIR}/src/IsoVerifier.cpp
    ${ISO_CATALOG_SOURCES}
    ${CMAKE_SOURCE_DIR}/src/IsoCatalogManifest.cpp
    ${CMAKE_SOURCE_DIR}/src/IsoHttpClient.cpp
    ${CMAKE_SOURCE_DIR}/src/IsoChecksum.cpp
    ${CMAKE_SOURCE_DIR}/src/IsoVerifyCache.cpp
    ${CMAKE_SOURCE_DIR}/src/IsoVerifyReport.cpp
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
    FLASHSENTRY_VERSION="${PROJECT_VERSION}"
    FLASHSENTRY_TEST_FIXTURES_DIR="${FLASHSENTRY_TEST_FIXTURES_DIR}"
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
    FLASHSENTRY_VERSION="${PROJECT_VERSION}"
    FLASHSENTRY_TEST_FIXTURES_DIR="${FLASHSENTRY_TEST_FIXTURES_DIR}"
)
target_sources(test_iso_verify_publisher_mock PRIVATE ${CMAKE_SOURCE_DIR}/resources/resources.qrc)
add_test(NAME test_iso_verify_publisher_mock COMMAND test_iso_verify_publisher_mock)

add_executable(test_iso_http_mock test_iso_http_mock.cpp ${CMAKE_SOURCE_DIR}/src/IsoHttpClient.cpp)
target_include_directories(test_iso_http_mock PRIVATE ${CMAKE_SOURCE_DIR}/include)
target_link_libraries(test_iso_http_mock PRIVATE Qt6::Test Qt6::Core Qt6::Network)
target_compile_definitions(test_iso_http_mock PRIVATE FLASHSENTRY_VERSION="${PROJECT_VERSION}")
add_test(NAME test_iso_http_mock COMMAND test_iso_http_mock)
