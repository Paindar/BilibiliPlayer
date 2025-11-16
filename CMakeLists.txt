cmake_minimum_required(VERSION 3.16)

project(BilibiliPlayer VERSION 0.1 LANGUAGES CXX)

set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(QT NAMES Qt6 Qt5 REQUIRED COMPONENTS Widgets LinguistTools Concurrent)
find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Widgets LinguistTools Concurrent)

# Set the translation files path
set(TS_FILES resource/lang/BilibiliPlayer_zh_CN.ts)


enable_testing()
# Add the src subdirectory
add_subdirectory(src)
# Add tests
add_subdirectory(test)

# Translation files will be handled in the src directory
if(${QT_VERSION_MAJOR} GREATER_EQUAL 6)
    qt_create_translation(QM_FILES ${CMAKE_SOURCE_DIR} ${TS_FILES})
else()
    qt5_create_translation(QM_FILES ${CMAKE_SOURCE_DIR} ${TS_FILES})
endif()

# Installation and other configurations will be handled in src directory
find_program(GCOVR_EXE gcovr)
if(GCOVR_EXE)
    add_custom_target(coverage
        COMMAND ${CMAKE_CTEST_COMMAND} --output-on-failure
        COMMAND ${GCOVR_EXE}
            -r ${CMAKE_SOURCE_DIR}
            --html --html-details
            -o ${CMAKE_BINARY_DIR}/coverage.html
        WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
        COMMENT "Running tests + generating coverage report"
    )
endif()
option(ENABLE_COVERAGE "Enable coverage reporting" OFF)

if(ENABLE_COVERAGE)
    if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
        message(STATUS "Coverage enabled: adding --coverage flags")
        add_compile_options(--coverage -O0 -g)
        add_link_options(--coverage)
    else()
        message(WARNING "Coverage enabled but compiler is not GCC/Clang")
    endif()
endif()