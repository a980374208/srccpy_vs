# 核心库 - 所有源文件（已统一为 C++）
set(CORE_SOURCES
    # 核心模块
    common.cpp
    net.cpp
    device.cpp
    controller.cpp
    str_util.cpp
    
    # 主框架
    server.cpp
    srccpy.cpp
    
    # ADB 相关
    adb/adb.cpp
    adb/adb_tunnel.cpp
    adb/adb_device.cpp
    adb/adb_parser.cpp
    # 工具类
    util/rand.cpp
    util/sc_intr.cpp
    util/tick.cpp
    util/sc_file.cpp
    util/sc_log.cpp
    util/options.cpp

    # 系统相关
    env.cpp
    util/sc_thread.cpp
    util/sc_process.cpp
    util/process_intr.cpp
    #事件
    asio/sc_event.cpp

    #编解码
    codec/demuxer.cpp
    codec/packet_source.cpp
    codec/packet_sink.cpp
    codec/packet_merge.cpp


)

# 创建静态库（使用 C++ 编译器以支持混合编译）
add_library(scrcpy_core STATIC ${CORE_SOURCES})
target_compile_features(scrcpy_core PUBLIC cxx_std_17)
target_compile_definitions(scrcpy_core PRIVATE ASIO_STANDALONE)


target_include_directories(scrcpy_core PUBLIC
    ${CMAKE_SOURCE_DIR}/third_party/asio
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
