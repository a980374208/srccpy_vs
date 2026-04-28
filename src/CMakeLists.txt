# 核心库 - 所有源文件和头文件（已统一为 C++）
# 按类型分开定义，便于使用 source_group(TREE ...) 自动分组

# 源文件列表
set(CORE_SOURCES
    common.cpp
    net.cpp
    device.cpp
    controller.cpp
    str_util.cpp
    server.cpp
    srccpy.cpp
    env.cpp
    adb/adb.cpp
    adb/adb_tunnel.cpp
    adb/adb_device.cpp
    adb/adb_parser.cpp
    util/rand.cpp
    util/sc_intr.cpp
    util/tick.cpp
    util/sc_file.cpp
    util/sc_log.cpp
    util/options.cpp
    util/sc_thread.cpp
    util/sc_process.cpp
    util/process_intr.cpp
    asio/sc_event.cpp
    codec/demuxer.cpp
    codec/packet_source.cpp
    codec/packet_sink.cpp
    codec/packet_merge.cpp
)

# 头文件列表
set(CORE_HEADERS
    common.h
    net.h
    device.h
    controller.h
    str_util.h
    compat.h
    server.hpp
    srccpy.h
    srccpy.hpp
    env.h
    adb/adb.h
    adb/adb_tunnel.h
    adb/adb_device.h
    adb/adb_parser.h
    util/rand.h
    util/sc_intr.h
    util/tick.h
    util/sc_file.h
    util/sc_log.h
    util/options.h
    util/sc_thread.h
    util/sc_process.h
    util/process_intr.h
    asio/sc_event.h
    codec/demuxer.h
    codec/packet_source.h
    codec/packet_sink.h
    codec/packet_merge.h
)

# 创建静态库（使用 C++ 编译器以支持混合编译）
# 注意：必须同时包含源文件和头文件，Visual Studio 才能显示它们
add_library(scrcpy_core STATIC ${CORE_SOURCES} ${CORE_HEADERS})
target_compile_features(scrcpy_core PUBLIC cxx_std_17)
target_compile_definitions(scrcpy_core PRIVATE ASIO_STANDALONE)

# 设置 Visual Studio 文件夹结构 - 使用 TREE 自动按目录分组
# 参考 OBS 项目的做法

# 源文件分组
source_group(TREE "${CMAKE_CURRENT_SOURCE_DIR}" PREFIX "Source Files" FILES ${CORE_SOURCES})

# 头文件分组
source_group(TREE "${CMAKE_CURRENT_SOURCE_DIR}" PREFIX "Header Files" FILES ${CORE_HEADERS})


target_include_directories(scrcpy_core PUBLIC
    ${CMAKE_SOURCE_DIR}/third_party/ffmpeg/include
)

# 添加编译定义（特别是 Windows 平台）
if(WIN32)
    target_compile_definitions(scrcpy_core PRIVATE
        _CRT_SECURE_NO_WARNINGS
        _WIN32_WINNT=0x0601
    )
else()
    find_package(Threads REQUIRED)
    target_link_libraries(scrcpy_core PRIVATE Threads::Threads)  #for asio
endif()

# 设置库的语言为 CXX，以便链接 C++ 标准库
set_target_properties(scrcpy_core PROPERTIES
    LINKER_LANGUAGE CXX
)

# Windows 平台特定链接库
if(WIN32)
    target_link_libraries(scrcpy_core PRIVATE ws2_32)
    
    # 链接 FFmpeg 库
    target_link_libraries(scrcpy_core PRIVATE
        ${CMAKE_SOURCE_DIR}/third_party/ffmpeg/lib/avcodec.lib
        ${CMAKE_SOURCE_DIR}/third_party/ffmpeg/lib/avformat.lib
        ${CMAKE_SOURCE_DIR}/third_party/ffmpeg/lib/avutil.lib
        ${CMAKE_SOURCE_DIR}/third_party/ffmpeg/lib/swscale.lib
        ${CMAKE_SOURCE_DIR}/third_party/ffmpeg/lib/swresample.lib
    )
else()
    # Linux/macOS 平台链接 FFmpeg
    target_link_libraries(scrcpy_core PRIVATE
        avcodec
        avformat
        avutil
        swscale
        swresample
        pthread  # for asio
    )
endif()

# 主应用程序
set(APP_SOURCES
    main.cpp
)

add_executable(scrcpy_app ${APP_SOURCES})

# 设置 Visual Studio 文件夹结构
source_group("Main" FILES main.cpp)

target_link_libraries(scrcpy_app PRIVATE scrcpy_core)

# 设置可执行文件的语言为 CXX
target_link_libraries(scrcpy_app PRIVATE)
if(WIN32)
    target_link_libraries(scrcpy_app PRIVATE ws2_32)
    
    # 链接 FFmpeg 库到可执行文件（因为 scrcpy_core 是静态库）
    target_link_libraries(scrcpy_app PRIVATE
        ${CMAKE_SOURCE_DIR}/third_party/ffmpeg/lib/avcodec.lib
        ${CMAKE_SOURCE_DIR}/third_party/ffmpeg/lib/avformat.lib
        ${CMAKE_SOURCE_DIR}/third_party/ffmpeg/lib/avutil.lib
        ${CMAKE_SOURCE_DIR}/third_party/ffmpeg/lib/swscale.lib
        ${CMAKE_SOURCE_DIR}/third_party/ffmpeg/lib/swresample.lib
    )
else()
    # Linux/macOS 平台链接 FFmpeg
    target_link_libraries(scrcpy_app PRIVATE
        avcodec
        avformat
        avutil
        swscale
        swresample
    )
endif()
