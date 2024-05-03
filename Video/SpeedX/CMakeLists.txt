cmake_minimum_required(VERSION 3.14)
project(PLAYBACK_SPEED)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_FLAGS "-D__STDC_CONSTANT_MACROS")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -g")

# Set the path to the directory containing the .pc files
set(ENV{PKG_CONFIG_PATH} "/home/phani/MMMediaSuite/Deps/FFMPEG/v6.1/build/Linux/x86_64/ffmpeg/lib/pkgconfig")

find_package(PkgConfig REQUIRED)
pkg_check_modules(AVCODEC    REQUIRED IMPORTED_TARGET libavcodec)
pkg_check_modules(AVFORMAT   REQUIRED IMPORTED_TARGET libavformat)
pkg_check_modules(AVUTIL     REQUIRED IMPORTED_TARGET libavutil)
pkg_check_modules(AVFILTER   REQUIRED IMPORTED_TARGET libavfilter)
pkg_check_modules(AVDEVICE   REQUIRED IMPORTED_TARGET libavdevice)
pkg_check_modules(SWRESAMPLE REQUIRED IMPORTED_TARGET libswresample)
pkg_check_modules(SWSCALE    REQUIRED IMPORTED_TARGET libswscale)

list(APPEND SOURCES
    src/playback_speed.cpp
)

add_executable(PLAYBACK_SPEED ${SOURCES})
target_include_directories(PLAYBACK_SPEED PRIVATE
        ${AVCODEC_INCLUDE_DIRS} 
        ${AVFORMAT_INCLUDE_DIRS} 
        ${AVFILTER_INCLUDE_DIRS} 
        ${AVDEVICE_INCLUDE_DIRS} 
        ${AVUTIL_INCLUDE_DIRS} 
        ${SWRESAMPLE_INCLUDE_DIRS} 
        ${SWSCALE_INCLUDE_DIRS}
        ${POSTPROC_INCLUDE_DIRS}
)
target_link_libraries(PLAYBACK_SPEED PRIVATE
        ${AVCODEC_LDFLAGS} 
        ${AVFORMAT_LDFLAGS} 
        ${AVFILTER_LDFLAGS} 
        ${AVDEVICE_LDFLAGS} 
        ${AVUTIL_LDFLAGS}
        ${SWRESAMPLE_LDFLAGS} 
        ${SWSCALE_LDFLAGS}
        ${POSTPROC_LDFLAGS}
        -lnuma
)