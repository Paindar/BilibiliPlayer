# Source files for BilibiliPlayer organized by components
set(PROJECT_SOURCES
    # Main application
    main.cpp
    
    # UI components
    ui/mainwindow.cpp
    ui/mainwindow.h
    ui/mainwindow.ui
    ui/customtitlebar.cpp
    ui/customtitlebar.h
    ui/customtitlebar.ui
    ui/menuwidget.cpp
    ui/menuwidget.h
    ui/menuwidget.ui
    
    # Network components
    network/bili_network_interface.cpp
    network/bili_network_interface.h
    
    # Audio components
    audio/ffmpeg_decoder.cpp
    audio/ffmpeg_decoder.h
    audio/wasapi_audio_player.cpp
    audio/wasapi_audio_player.h
    
    # Streaming components
    stream/streaming_audio_buffer.cpp
    stream/streaming_audio_buffer.h
    
    # Utility components
    util/circular_buffer.hpp
)

# Find required dependencies
find_package(httplib REQUIRED)
find_package(ffmpeg REQUIRED)
find_package(JsonCpp REQUIRED)
find_package(OpenSSL REQUIRED)

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
    httplib::httplib
    ffmpeg::ffmpeg
    JsonCpp::JsonCpp
    OpenSSL::SSL
    OpenSSL::Crypto
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
