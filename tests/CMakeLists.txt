find_package(PkgConfig REQUIRED)
pkg_check_modules(DBUS REQUIRED dbus-1)

find_package(GTest REQUIRED)

# Unit tests (no dbus-daemon needed)
add_executable(test_type_traits test_type_traits.cpp)
target_link_libraries(test_type_traits mbedbus GTest::gtest_main)
target_include_directories(test_type_traits PRIVATE ${DBUS_INCLUDE_DIRS})
add_test(NAME TypeTraits COMMAND test_type_traits)

add_executable(test_message test_message.cpp)
target_link_libraries(test_message mbedbus GTest::gtest_main)
target_include_directories(test_message PRIVATE ${DBUS_INCLUDE_DIRS})
add_test(NAME Message COMMAND test_message)

add_executable(test_variant test_variant.cpp)
target_link_libraries(test_variant mbedbus GTest::gtest_main)
target_include_directories(test_variant PRIVATE ${DBUS_INCLUDE_DIRS})
add_test(NAME Variant COMMAND test_variant)

add_executable(test_expected test_expected.cpp)
target_link_libraries(test_expected mbedbus GTest::gtest_main)
target_include_directories(test_expected PRIVATE ${DBUS_INCLUDE_DIRS})
add_test(NAME Expected COMMAND test_expected)

add_executable(test_introspection test_introspection.cpp)
target_link_libraries(test_introspection mbedbus GTest::gtest_main)
target_include_directories(test_introspection PRIVATE ${DBUS_INCLUDE_DIRS})
add_test(NAME Introspection COMMAND test_introspection)

# Integration tests (require dbus-daemon)
add_executable(test_connection test_connection.cpp)
target_link_libraries(test_connection mbedbus GTest::gtest_main)
target_include_directories(test_connection PRIVATE ${DBUS_INCLUDE_DIRS})
add_test(NAME Connection COMMAND test_connection)

add_executable(test_service_object test_service_object.cpp)
target_link_libraries(test_service_object mbedbus GTest::gtest_main)
target_include_directories(test_service_object PRIVATE ${DBUS_INCLUDE_DIRS})
add_test(NAME ServiceObject COMMAND test_service_object)

add_executable(test_proxy test_proxy.cpp)
target_link_libraries(test_proxy mbedbus GTest::gtest_main)
target_include_directories(test_proxy PRIVATE ${DBUS_INCLUDE_DIRS})
add_test(NAME Proxy COMMAND test_proxy)

add_executable(test_object_manager test_object_manager.cpp)
target_link_libraries(test_object_manager mbedbus GTest::gtest_main)
target_include_directories(test_object_manager PRIVATE ${DBUS_INCLUDE_DIRS})
add_test(NAME ObjectManager COMMAND test_object_manager)

add_executable(test_integration test_integration.cpp)
target_link_libraries(test_integration mbedbus GTest::gtest_main)
target_include_directories(test_integration PRIVATE ${DBUS_INCLUDE_DIRS})
add_test(NAME Integration COMMAND test_integration)
