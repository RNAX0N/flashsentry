find_package(Qt6 REQUIRED COMPONENTS Test)

set(CMAKE_AUTOMOC ON)

add_executable(test_types test_types.cpp)
target_include_directories(test_types PRIVATE ${CMAKE_SOURCE_DIR}/include)
target_link_libraries(test_types PRIVATE Qt6::Test)
add_test(NAME test_types COMMAND test_types)

add_executable(test_database_manager
    test_database_manager.cpp
    ${CMAKE_SOURCE_DIR}/src/DatabaseManager.cpp
    ${CMAKE_SOURCE_DIR}/include/DatabaseManager.h
)
target_include_directories(test_database_manager PRIVATE ${CMAKE_SOURCE_DIR}/include)
target_include_directories(test_database_manager PRIVATE ${OPENSSL_INCLUDE_DIRS})
target_link_libraries(test_database_manager PRIVATE Qt6::Test Qt6::Core ${OPENSSL_LIBRARIES})
add_test(NAME test_database_manager COMMAND test_database_manager)

add_executable(test_merkle test_merkle.cpp ${CMAKE_SOURCE_DIR}/src/MerkleTree.cpp)
target_include_directories(test_merkle PRIVATE ${CMAKE_SOURCE_DIR}/include ${OPENSSL_INCLUDE_DIRS})
target_link_libraries(test_merkle PRIVATE Qt6::Test Qt6::Core ${OPENSSL_LIBRARIES})
add_test(NAME test_merkle COMMAND test_merkle)

add_executable(test_iso_catalog test_iso_catalog.cpp ${CMAKE_SOURCE_DIR}/src/IsoCatalog.cpp)
target_include_directories(test_iso_catalog PRIVATE ${CMAKE_SOURCE_DIR}/include)
target_link_libraries(test_iso_catalog PRIVATE Qt6::Test Qt6::Core)
add_test(NAME test_iso_catalog COMMAND test_iso_catalog)

add_executable(test_autostart
    test_autostart.cpp
    ${CMAKE_SOURCE_DIR}/src/AutostartManager.cpp
)
target_include_directories(test_autostart PRIVATE ${CMAKE_SOURCE_DIR}/include)
target_link_libraries(test_autostart PRIVATE Qt6::Test Qt6::Core)
add_test(NAME test_autostart COMMAND test_autostart)
