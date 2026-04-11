cmake_minimum_required(VERSION 3.5)
project(mbedbus VERSION 0.0.1 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(PkgConfig REQUIRED)
pkg_check_modules(DBUS REQUIRED dbus-1)

option(MBEDBUS_BUILD_TESTS "Build tests" ON)
option(MBEDBUS_BUILD_EXAMPLES "Build examples" ON)
option(MBEDBUS_ENABLE_LOGGING "Enable debug logging" OFF)

add_library(mbedbus
    src/Connection.cpp
    src/Message.cpp
    src/ServiceObject.cpp
    src/ObjectManager.cpp
    src/Proxy.cpp
    src/Variant.cpp
    src/TypeTraits.cpp
    src/Introspection.cpp
)

target_include_directories(mbedbus
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
    PRIVATE
        ${DBUS_INCLUDE_DIRS}
)

target_link_libraries(mbedbus PUBLIC ${DBUS_LIBRARIES} pthread)
target_compile_options(mbedbus PRIVATE ${DBUS_CFLAGS_OTHER} -Wall -Wextra -Wpedantic)

if(MBEDBUS_ENABLE_LOGGING)
    target_compile_definitions(mbedbus PUBLIC MBEDBUS_LOGGING_ENABLED)
endif()

if(MBEDBUS_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

if(MBEDBUS_BUILD_EXAMPLES)
    add_subdirectory(examples)
endif()

install(TARGETS mbedbus EXPORT mbedbusTargets
    LIBRARY DESTINATION lib
    ARCHIVE DESTINATION lib
)
install(DIRECTORY include/mbedbus DESTINATION include)
