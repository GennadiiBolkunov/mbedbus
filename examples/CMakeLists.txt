find_package(PkgConfig REQUIRED)
pkg_check_modules(DBUS REQUIRED dbus-1)

set(EXAMPLES
    simple_server
    simple_client
    async_client
    signal_emitter
    signal_listener
    object_manager
    properties_monitor
    skeleton_server
)

foreach(example ${EXAMPLES})
    add_executable(${example} ${example}.cpp)
    target_link_libraries(${example} mbedbus)
    target_include_directories(${example} PRIVATE ${DBUS_INCLUDE_DIRS})
endforeach()
