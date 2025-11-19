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
    ui/player_status_bar.cpp
    ui/player_status_bar.h
    ui/player_status_bar.ui

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

    # UI utility components
    ui/util/elided_label.cpp
    ui/util/elided_label.h
    
    # Theme management
    ui/theme/theme_manager.cpp
    ui/theme/theme_manager.h

    # Event bus components
    event/event_bus.hpp
    
    # Network components
    network/network_manager.cpp
    network/network_manager.h
    network/platform/bili_network_interface.cpp
    network/platform/bili_network_interface.h
    
    # Network platform abstraction
    network/platform/i_platform.h
    
    # Audio components
    audio/ffmpeg_decoder.cpp
    audio/ffmpeg_probe.cpp
    audio/taglib_cover.cpp
    audio/ffmpeg_log.cpp
    audio/ffmpeg_decoder.h
    audio/taglib_cover.h
    audio/wasapi_audio_output.cpp
    audio/wasapi_audio_output.h
    audio/audio_player_controller.cpp
    audio/audio_player_controller_callback.cpp
    audio/audio_player_controller.h
    audio/audio_event_processor.cpp
    audio/audio_event_processor.h
    audio/audio_frame.h

    # Streaming components
    stream/realtime_pipe.cpp
    stream/realtime_pipe.hpp

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
    util/safe_queue.hpp
    util/md5.h
    util/md5.cpp
    util/urlencode.h
    util/urlencode.cpp
    util/cover_cache.cpp
    util/cover_cache.h
    
    # Resources
    ../resource/translations.qrc
)

# Find required dependencies
find_package(httplib REQUIRED)
find_package(ffmpeg REQUIRED)
find_package(JsonCpp REQUIRED)
find_package(OpenSSL REQUIRED)
find_package(spdlog REQUIRED)
find_package(fmt REQUIRED)
find_package(magic_enum REQUIRED)
find_package(taglib REQUIRED)

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
    magic_enum::magic_enum
    taglib::tag
    # Windows-specific libraries for WASAPI
    ole32
    oleaut32
    # dbghelp for stack trace capture on Windows
    dbghelp
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

# Copy necessary DLLs on Windows after build

# Find the directory where Qt6Core.dll or libQt6Core.a lives
get_target_property(QtCore_location Qt6::Core LOCATION)
get_filename_component(QT_BIN_DIR "${QtCore_location}" DIRECTORY)
get_filename_component(QT_INSTALL_DIR "${QT_BIN_DIR}" DIRECTORY)

# Now we can form the plugin path (works on both Windows & Linux)
# Post-build step for copying runtime DLLs
add_custom_command(TARGET BilibiliPlayer POST_BUILD
    # 1. Copy all DLLs from the Conan bin folder (already populated by conan install)
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${CMAKE_BINARY_DIR}/bin"
        "$<TARGET_FILE_DIR:BilibiliPlayer>"

    # 2. Ensure the Qt platform folder exists
    COMMAND ${CMAKE_COMMAND} -E make_directory
        "$<TARGET_FILE_DIR:BilibiliPlayer>/platforms"

    # 3. Copy the Qt core DLL
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${QT_INSTALL_DIR}/bin/Qt6Core.dll"
        "$<TARGET_FILE_DIR:BilibiliPlayer>"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${QT_INSTALL_DIR}/bin/Qt6Gui.dll"
        "$<TARGET_FILE_DIR:BilibiliPlayer>"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${QT_INSTALL_DIR}/bin/Qt6Widgets.dll"
        "$<TARGET_FILE_DIR:BilibiliPlayer>"

    # 4. Copy the Qt Windows platform plugin
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${QT_INSTALL_DIR}/plugins/platforms/qwindows.dll"
        "$<TARGET_FILE_DIR:BilibiliPlayer>/platforms"

    
    # 5. Copy the Qt Image plugin
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${QT_INSTALL_DIR}/plugins/imageformats/qjpeg.dll"
        "$<TARGET_FILE_DIR:BilibiliPlayer>/imageformats"

    COMMENT "Copying DLLs (Conan + Qt) to runtime output folder"
)
