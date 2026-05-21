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
