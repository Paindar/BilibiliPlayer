cmake_minimum_required(VERSION 3.16)

project(BilibiliPlayerTests)

# Prefer the Catch2 package provided by Conan (CMakeDeps). If not available, the project
# can optionally fall back to other methods, but with Conan in use this is unnecessary.
find_package(httplib REQUIRED)
find_package(Catch2 CONFIG REQUIRED)
find_package(spdlog CONFIG REQUIRED)
find_package(fmt CONFIG REQUIRED)
find_package(Qt6 COMPONENTS Core Gui REQUIRED)
find_package(JsonCpp REQUIRED)
find_package(OpenSSL REQUIRED)

## Single combined test executable for the project
add_executable(BilibiliPlayerTest
	realtime_pipe_test.cpp
	# streaming_buffer_test.cpp
	# streaming_integration_test.cpp
	# streaming_cancellation_test.cpp
	# fake_network_producer_test.cpp
	# config_manager_test.cpp
	# playlist_manager_test.cpp
	# test_http_pool.cpp
	# test_cookie_jar.cpp
)

# Include project source headers (handled per-target below)

# Build a small static target for the streaming implementation so the test can link against it
add_library(realtime_pipe_impl STATIC ${CMAKE_SOURCE_DIR}/src/stream/realtime_pipe.cpp)
target_include_directories(realtime_pipe_impl PRIVATE ${CMAKE_SOURCE_DIR}/src)

# # Config and playlist implementation targets for testing
# add_library(config_impl STATIC ${CMAKE_SOURCE_DIR}/src/config/config_manager.cpp ${CMAKE_SOURCE_DIR}/src/log/log_manager.cpp)
# target_include_directories(config_impl PRIVATE ${CMAKE_SOURCE_DIR}/src)

# add_library(playlist_impl STATIC ${CMAKE_SOURCE_DIR}/src/playlist/playlist_manager.cpp)
# target_include_directories(playlist_impl PRIVATE ${CMAKE_SOURCE_DIR}/src)

# # Link libraries required by config/playlist implementations
# target_link_libraries(config_impl PRIVATE Qt${QT_VERSION_MAJOR}::Core Qt${QT_VERSION_MAJOR}::Gui spdlog::spdlog fmt::fmt)
# target_link_libraries(playlist_impl PRIVATE config_impl Qt${QT_VERSION_MAJOR}::Core Qt${QT_VERSION_MAJOR}::Gui spdlog::spdlog)

target_link_libraries(BilibiliPlayerTest PRIVATE
	Catch2::Catch2WithMain
	realtime_pipe_impl
	# config_impl
	# playlist_impl
	Qt${QT_VERSION_MAJOR}::Core
	Qt${QT_VERSION_MAJOR}::Gui
    httplib::httplib
    JsonCpp::JsonCpp
	OpenSSL::SSL 
	OpenSSL::Crypto
	spdlog::spdlog
	fmt::fmt
)
target_include_directories(BilibiliPlayerTest PRIVATE ${CMAKE_SOURCE_DIR}/src)

# Build network implementation into a small static library for tests and enable
# UNIT_TEST define so test helpers are present.
# add_library(network_impl STATIC
# 	${CMAKE_SOURCE_DIR}/src/network/bili_network_interface.cpp
# 	${CMAKE_SOURCE_DIR}/src/util/md5.cpp
# 	${CMAKE_SOURCE_DIR}/src/util/urlencode.cpp
# )
# target_include_directories(network_impl PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_compile_definitions(BilibiliPlayerTest PRIVATE UNIT_TEST)
# target_link_libraries(network_impl PRIVATE httplib::httplib JsonCpp::JsonCpp OpenSSL::SSL OpenSSL::Crypto spdlog::spdlog fmt::fmt)

# Use C++17
set_target_properties(BilibiliPlayerTest PROPERTIES CXX_STANDARD 17 CXX_STANDARD_REQUIRED YES)

# Copy necessary DLLs on Windows after build
add_custom_command(TARGET BilibiliPlayerTest POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        $<TARGET_RUNTIME_DLLS:BilibiliPlayerTest>
        $<TARGET_FILE_DIR:BilibiliPlayerTest>
    COMMAND_EXPAND_LISTS
)
