cmake_minimum_required(VERSION 3.16)

project(BilibiliPlayerTests)

# Prefer the Catch2 package provided by Conan (CMakeDeps). If not available, the project
# can optionally fall back to other methods, but with Conan in use this is unnecessary.
find_package(Catch2 CONFIG REQUIRED)
find_package(spdlog CONFIG REQUIRED)
find_package(fmt CONFIG REQUIRED)
find_package(Qt6 COMPONENTS Core Gui REQUIRED)

## Single combined test executable for the project
add_executable(BilibiliPlayerTest
	streaming_buffer_test.cpp
	streaming_integration_test.cpp
	streaming_cancellation_test.cpp
	fake_network_producer_test.cpp
	config_manager_test.cpp
	playlist_manager_test.cpp
)

# Include project source headers (handled per-target below)

# Build a small static target for the streaming implementation so the test can link against it
add_library(streaming_impl STATIC ${CMAKE_SOURCE_DIR}/src/stream/streaming_audio_buffer.cpp)
target_include_directories(streaming_impl PRIVATE ${CMAKE_SOURCE_DIR}/src)

# Config and playlist implementation targets for testing
add_library(config_impl STATIC ${CMAKE_SOURCE_DIR}/src/config/config_manager.cpp ${CMAKE_SOURCE_DIR}/src/log/log_manager.cpp)
target_include_directories(config_impl PRIVATE ${CMAKE_SOURCE_DIR}/src)

add_library(playlist_impl STATIC ${CMAKE_SOURCE_DIR}/src/playlist/playlist_manager.cpp)
target_include_directories(playlist_impl PRIVATE ${CMAKE_SOURCE_DIR}/src)

# Link libraries required by config/playlist implementations
target_link_libraries(config_impl PRIVATE Qt${QT_VERSION_MAJOR}::Core Qt${QT_VERSION_MAJOR}::Gui spdlog::spdlog fmt::fmt)
target_link_libraries(playlist_impl PRIVATE config_impl Qt${QT_VERSION_MAJOR}::Core Qt${QT_VERSION_MAJOR}::Gui spdlog::spdlog)

target_link_libraries(BilibiliPlayerTest PRIVATE
	Catch2::Catch2WithMain
	streaming_impl
	config_impl
	playlist_impl
	Qt${QT_VERSION_MAJOR}::Core
	Qt${QT_VERSION_MAJOR}::Gui
	spdlog::spdlog
	fmt::fmt
)
target_include_directories(BilibiliPlayerTest PRIVATE ${CMAKE_SOURCE_DIR}/src)

# Use C++17
set_target_properties(BilibiliPlayerTest PROPERTIES CXX_STANDARD 17 CXX_STANDARD_REQUIRED YES)

# Include project source headers for tests
## project headers already added to BilibiliPlayerTest above
