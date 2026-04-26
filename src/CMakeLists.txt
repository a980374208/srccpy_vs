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
    
    # 系统相关
    env.cpp
    util/sc_thread.cpp
    util/sc_process.cpp
    util/process_intr.cpp

)

# 创建静态库（使用 C++ 编译器以支持混合编译）
add_library(scrcpy_core STATIC ${CORE_SOURCES})

# 添加编译定义（特别是 Windows 平台）
if(WIN32)
    target_compile_definitions(scrcpy_core PRIVATE
        _CRT_SECURE_NO_WARNINGS
        _WIN32_WINNT=0x0601
    )
endif()

# 设置库的语言为 CXX，以便链接 C++ 标准库
set_target_properties(scrcpy_core PROPERTIES
    LINKER_LANGUAGE CXX
)

# Windows 平台特定链接库
if(WIN32)
    target_link_libraries(scrcpy_core PRIVATE ws2_32)
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
endif()
