# Source files for BilibiliPlayer organized by components
set(PROJECT_SOURCES
    # Main application
    main.cpp
    
    # UI components
    ui/main_window.cpp
    ui/main_window.h
    ui/main_window.ui
    ui/custom_title_bar.cpp
    ui/custom_title_bar.h
    ui/custom_title_bar.ui
    ui/menu_widget.cpp
    ui/menu_widget.h
    ui/menu_widget.ui

    # UI navigation components
    ui/navigator/i_navigable_page.h
    ui/navigator/navigator.cpp
    ui/navigator/navigator.h

    # UI pages
    ui/page/search_page.cpp
    ui/page/search_page.h
    ui/page/search_page.ui
    ui/page/playlist_page.cpp
    ui/page/playlist_page.h
    ui/page/playlist_page.ui
    ui/page/settings_page.cpp
    ui/page/settings_page.h
    ui/page/settings_page.ui
    
    # Network components
    network/bili_network_interface.cpp
    network/bili_network_interface.h
    network/network_manager.cpp
    network/network_manager.h
    
    # Audio components
    audio/ffmpeg_decoder.cpp
    audio/ffmpeg_decoder.h
    audio/wasapi_audio_player.cpp
    audio/wasapi_audio_player.h
    audio/audio_player_controller.cpp
    audio/audio_player_controller.h
    
    # Streaming components
    stream/streaming_audio_buffer.cpp
    stream/streaming_audio_buffer.h
    
    # Manager components
    manager/application_context.cpp
    manager/application_context.h
    
    # Configuration management
    config/config_manager.cpp
    config/config_manager.h
    
    # Playlist management
    playlist/playlist_manager.cpp
    playlist/playlist_manager.h
    
    # Logging system
    log/log_manager.cpp
    log/log_manager.h

    # Utility components
    util/circular_buffer.hpp
    util/md5.h
    util/md5.cpp
    util/urlencode.h
    util/urlencode.cpp
)

# Find required dependencies
find_package(httplib REQUIRED)
find_package(ffmpeg REQUIRED)
find_package(JsonCpp REQUIRED)
find_package(OpenSSL REQUIRED)
find_package(spdlog REQUIRED)
find_package(fmt REQUIRED)

# Create the executable
if(${QT_VERSION_MAJOR} GREATER_EQUAL 6)
    qt_add_executable(BilibiliPlayer
        MANUAL_FINALIZATION
        ${PROJECT_SOURCES}
    )
# Define target properties for Android with Qt 6 as:
#    set_property(TARGET BilibiliPlayer APPEND PROPERTY QT_ANDROID_PACKAGE_SOURCE_DIR
#                 ${CMAKE_CURRENT_SOURCE_DIR}/android)
# For more information, see https://doc.qt.io/qt-6/qt-add-executable.html#target-creation
else()
    if(ANDROID)
        add_library(BilibiliPlayer SHARED
            ${PROJECT_SOURCES}
        )
# Define properties for Android with Qt 5 after find_package() calls as:
#    set(ANDROID_PACKAGE_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/android")
    else()
        add_executable(BilibiliPlayer
            ${PROJECT_SOURCES}
        )
    endif()
endif()

# Link Qt libraries and external dependencies
target_link_libraries(BilibiliPlayer PRIVATE 
    Qt${QT_VERSION_MAJOR}::Widgets
    Qt${QT_VERSION_MAJOR}::Concurrent
    httplib::httplib
    ffmpeg::ffmpeg
    JsonCpp::JsonCpp
    OpenSSL::SSL
    OpenSSL::Crypto
    spdlog::spdlog
    fmt::fmt
    # Windows-specific libraries for WASAPI
    ole32
    oleaut32
)

# Add include directories for organized header structure
target_include_directories(BilibiliPlayer PRIVATE 
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/audio
    ${CMAKE_CURRENT_SOURCE_DIR}/network
    ${CMAKE_CURRENT_SOURCE_DIR}/stream
    ${CMAKE_CURRENT_SOURCE_DIR}/ui
    ${CMAKE_CURRENT_SOURCE_DIR}/util
)

# Qt for iOS sets MACOSX_BUNDLE_GUI_IDENTIFIER automatically since Qt 6.1.
# If you are developing for iOS or macOS you should consider setting an
# explicit, fixed bundle identifier manually though.
if(${QT_VERSION} VERSION_LESS 6.1.0)
  set(BUNDLE_ID_OPTION MACOSX_BUNDLE_GUI_IDENTIFIER com.example.BilibiliPlayer)
endif()
set_target_properties(BilibiliPlayer PROPERTIES
    ${BUNDLE_ID_OPTION}
    MACOSX_BUNDLE_BUNDLE_VERSION ${PROJECT_VERSION}
    MACOSX_BUNDLE_SHORT_VERSION_STRING ${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}
    MACOSX_BUNDLE TRUE
    WIN32_EXECUTABLE TRUE
)

# Installation configuration
include(GNUInstallDirs)
install(TARGETS BilibiliPlayer
    BUNDLE DESTINATION .
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

# Finalize executable for Qt6
if(QT_VERSION_MAJOR EQUAL 6)
    qt_finalize_executable(BilibiliPlayer)
endif()
