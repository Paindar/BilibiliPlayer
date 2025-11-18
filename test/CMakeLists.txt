cmake_minimum_required(VERSION 3.16)

project(BilibiliPlayerTest)

# Find all required packages
find_package(Catch2 CONFIG REQUIRED)
find_package(Qt6 COMPONENTS Core Gui Widgets Test REQUIRED)
find_package(httplib REQUIRED)
find_package(ffmpeg REQUIRED)
find_package(JsonCpp REQUIRED)
find_package(OpenSSL REQUIRED)
find_package(spdlog REQUIRED)
find_package(fmt REQUIRED)
find_package(magic_enum REQUIRED)
find_package(taglib REQUIRED)

## Single combined test executable for the project
add_executable(BilibiliPlayerTest
    # Core utility tests (header-only implementations)
    event_bus_test.cpp
    realtime_pipe_test.cpp
    circular_buffer_test.cpp
    safe_queue_test.cpp

    # Config manager test
    config_manager_test.cpp

    # Playlist manager test
    playlist_manager_test.cpp
    playlist_test.cpp
    playlist_local_add_test.cpp
    playlist_local_duration_test.cpp
    playlist_local_cover_test.cpp

    # Theme manager test
    theme_manager_test.cpp
    
    # FFmpeg decoder test (requires FFmpeg)
    ffmpeg_decoder_test.cpp
    ffmpeg_decoder_wav_test.cpp
    util/audio_generator.cpp
    i18n_test.cpp
    
    # Diagnostics tests
    diagnostics/sine_wave_playback_test.cpp
    
    # Bilibili Network Interface test (requires network implementation)
    bili_network_interface_test.cpp
)

# Build static libraries for testable components

# 1. RealtimePipe implementation
add_library(core_utility_test STATIC 
    ${CMAKE_SOURCE_DIR}/src/stream/realtime_pipe.cpp
    ${CMAKE_SOURCE_DIR}/src/event/event_bus.hpp
    ${CMAKE_SOURCE_DIR}/src/util/circular_buffer.hpp
    ${CMAKE_SOURCE_DIR}/src/util/safe_queue.hpp
)
# NOTE: CMAKE_INCLUDE_PATH is for find_*() commands, not for direct use.
# Modern CMake uses imported targets which handle include paths automatically.
# If you're debugging include issues, use: cmake --trace-expand to see include resolution.

target_include_directories(core_utility_test PUBLIC ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(core_utility_test PUBLIC spdlog::spdlog fmt::fmt)

# 2. Config Manager implementation
add_library(config_impl STATIC
    ${CMAKE_SOURCE_DIR}/src/config/config_manager.cpp
)
target_include_directories(config_impl PUBLIC ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(config_impl PUBLIC 
    Qt6::Core
    Qt6::Gui
    Qt6::Test
    spdlog::spdlog 
    fmt::fmt
)

# 3. Playlist Manager implementation
add_library(playlist_manager_impl STATIC
    ${CMAKE_SOURCE_DIR}/src/playlist/playlist_manager.cpp
    ${CMAKE_SOURCE_DIR}/src/audio/ffmpeg_probe.cpp
    ${CMAKE_SOURCE_DIR}/src/audio/taglib_cover.cpp
    ${CMAKE_SOURCE_DIR}/src/util/cover_cache.cpp
    ${CMAKE_SOURCE_DIR}/src/log/log_manager.cpp
	${CMAKE_SOURCE_DIR}/src/util/md5.cpp
)
target_include_directories(playlist_manager_impl PUBLIC ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(playlist_manager_impl PUBLIC 
    Qt${QT_VERSION_MAJOR}::Widgets
    JsonCpp::JsonCpp
    spdlog::spdlog
    fmt::fmt
    OpenSSL::SSL
    ffmpeg::ffmpeg
    taglib::tag
    config_impl
	magic_enum::magic_enum
)

# 4. Theme Manager implementation
add_library(theme_impl STATIC
    ${CMAKE_SOURCE_DIR}/src/ui/theme/theme_manager.cpp
)
target_include_directories(theme_impl PUBLIC ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(theme_impl PUBLIC
    Qt${QT_VERSION_MAJOR}::Core
    Qt${QT_VERSION_MAJOR}::Gui
    Qt${QT_VERSION_MAJOR}::Widgets
    JsonCpp::JsonCpp
    spdlog::spdlog
    fmt::fmt
    config_impl
)

# 5. FFmpeg Decoder implementation
add_library(ffmpeg_impl STATIC
    ${CMAKE_SOURCE_DIR}/src/audio/ffmpeg_decoder.cpp
    ${CMAKE_SOURCE_DIR}/src/audio/ffmpeg_log.cpp
	${CMAKE_SOURCE_DIR}/src/audio/audio_event_processor.cpp
)
target_include_directories(ffmpeg_impl PUBLIC ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(ffmpeg_impl PUBLIC
    Qt${QT_VERSION_MAJOR}::Core
    ffmpeg::avformat
    ffmpeg::avcodec
    ffmpeg::avutil
    ffmpeg::swresample
    spdlog::spdlog
    fmt::fmt
    magic_enum::magic_enum
)

# 6. Network implementation (for future use)
add_library(network_impl STATIC
    ${CMAKE_SOURCE_DIR}/src/network/platform/bili_network_interface.cpp
    ${CMAKE_SOURCE_DIR}/src/util/md5.cpp
    ${CMAKE_SOURCE_DIR}/src/util/urlencode.cpp
)
target_include_directories(network_impl PUBLIC ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(network_impl PUBLIC
    httplib::httplib
    JsonCpp::JsonCpp
    OpenSSL::SSL
    OpenSSL::Crypto
    Qt${QT_VERSION_MAJOR}::Core
    spdlog::spdlog
    fmt::fmt
)

# Enable UNIT_TEST for network implementation to build test methods
target_compile_definitions(network_impl PRIVATE UNIT_TEST)

# Link test executable with all dependencies
target_link_libraries(BilibiliPlayerTest PRIVATE
    Catch2::Catch2WithMain
    
    # Implementation libraries
    core_utility_test
    config_impl
    playlist_manager_impl
    theme_impl
    ffmpeg_impl
    network_impl
    
    # Also link directly with dependencies needed by test files
    spdlog::spdlog
    fmt::fmt
	magic_enum::magic_enum
    Qt6::Core
    Qt6::Gui
    Qt6::Test
)

target_include_directories(BilibiliPlayerTest PRIVATE ${CMAKE_SOURCE_DIR}/src)
# Make the current test directory available for test-only headers (test/)
target_include_directories(BilibiliPlayerTest PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_compile_definitions(BilibiliPlayerTest PRIVATE UNIT_TEST)

# Use C++17
set_target_properties(BilibiliPlayerTest PROPERTIES 
    CXX_STANDARD 17 
    CXX_STANDARD_REQUIRED YES
)

# Copy test data directory to build output directory
add_custom_command(TARGET BilibiliPlayerTest POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${CMAKE_CURRENT_SOURCE_DIR}/data"
        "$<TARGET_FILE_DIR:BilibiliPlayerTest>/data"
    COMMENT "Copying test data to build directory"
)

 

# Copy necessary DLLs on Windows after build
if(WIN32)
    add_custom_command(TARGET BilibiliPlayerTest POST_BUILD
        # 1. Copy all DLLs from the Conan bin folder (already populated by conan install)
        COMMAND ${CMAKE_COMMAND} -E copy_directory
            "${CMAKE_BINARY_DIR}/bin"
            "$<TARGET_FILE_DIR:BilibiliPlayerTest>"

        COMMENT "Copying DLLs (Conan + Qt) to runtime output folder"
    )
endif()

# Register with CTest
add_test(NAME BilibiliPlayerTest COMMAND $<TARGET_FILE:BilibiliPlayerTest>)