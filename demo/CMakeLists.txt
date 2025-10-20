cmake_minimum_required(VERSION 3.16)

# Project name
project(BilibiliPlayerDemo)

# Set C++ standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Find dependencies
find_package(httplib REQUIRED)
find_package(ffmpeg REQUIRED)
find_package(JsonCpp REQUIRED)
find_package(OpenSSL REQUIRED)

set(SIMPLE_WAV_TEST_SOURCES
    test/simple_wav_test.cpp
    src/ffmpeg_decoder.cpp
    src/wasapi_audio_player.cpp
)

set(BILI_STREAM_TEST_SOURCES
    test/bilibli_stream_test.cpp
    src/ffmpeg_decoder.cpp
    src/wasapi_audio_player.cpp
    src/bili_network_interface.cpp
    src/streaming_audio_buffer.cpp
)

# Create executables
add_executable(SimpleWavTest ${SIMPLE_WAV_TEST_SOURCES})
add_executable(BiliStreamTest ${BILI_STREAM_TEST_SOURCES})


target_link_libraries(SimpleWavTest PRIVATE 
    ffmpeg::ffmpeg
    ole32      # COM library
    oleaut32   # COM automation
)

target_link_libraries(BiliStreamTest PRIVATE 
    ffmpeg::ffmpeg
    httplib::httplib
    JsonCpp::JsonCpp
    OpenSSL::SSL
    OpenSSL::Crypto
    ole32      # COM library
    oleaut32   # COM automation
)