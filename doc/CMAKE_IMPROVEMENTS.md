# CMake 配置完善说明

## 概述

本次完善将项目从纯 C 项目扩展为 C/C++ 混合项目，并优化了 CMake 配置以支持跨平台编译。

## 主要改进

### 1. 项目配置 (CMakeLists.txt)

#### 1.1 语言支持

- **添加 C++ 支持**：`LANGUAGES C CXX`
- **C++17 标准**：设置 `CMAKE_CXX_STANDARD 17`
- **禁用扩展**：`CMAKE_CXX_EXTENSIONS OFF` 确保标准合规

#### 1.2 编译选项

- **MSVC 特定选项**：
  - `/EHsc` - 启用 C++ 异常处理
  - `/utf-8` - 使用 UTF-8 编码
- **GCC/Clang 选项**：
  - `-fexceptions` - 启用异常处理支持

#### 1.3 包含目录

添加了更完整的包含路径：

```cmake
include_directories(${CMAKE_SOURCE_DIR}/include)
include_directories(${CMAKE_SOURCE_DIR}/src)
include_directories(${CMAKE_SOURCE_DIR}/src/util)
include_directories(${CMAKE_SOURCE_DIR}/src/adb)
```

### 2. 源文件管理 (src/CMakeLists.txt)

#### 2.1 C 源文件

```cmake
set(CORE_C_SOURCES
    common.c
    net.c
    device.c
    controller.c
    str_util.c
)
```

#### 2.2 C++ 源文件

按模块组织：

```cmake
set(CORE_CXX_SOURCES
    # 主框架
    server.cpp
    srccpy.cpp

    # ADB 相关
    adb/adb.cpp
    adb/adb_tunnel.cpp

    # 工具类
    util/rand.cpp
    util/sc_intr.cpp
    util/tick.cpp

    # 系统相关
    env.cpp
)
```

#### 2.3 库配置

```cmake
# 设置为 CXX 链接语言，确保链接 C++ 标准库
set_target_properties(scrcpy_core PROPERTIES
    LINKER_LANGUAGE CXX
)
```

### 3. 跨平台兼容性修复

#### 3.1 MSVC 原子操作支持

修改 `include/net.h`，为 MSVC 提供替代方案：

```cpp
#ifdef _MSC_VER
// MSVC 不支持 C11 <stdatomic.h>，使用 volatile LONG
typedef struct sc_socket_wrapper {
    sc_raw_socket socket;
    volatile LONG closed;
} *sc_socket;
#else
// GCC/Clang 使用 C11 原子操作
#include <stdatomic.h>
typedef struct sc_socket_wrapper {
    sc_raw_socket socket;
    atomic_flag closed;
} *sc_socket;
#endif
```

#### 3.2 C++ 头文件标准化

- **rand.h**：使用 `<cstdint>` 和 `<random>` 替代 `<inttypes.h>`
- **sc_intr.h**：使用 `<atomic>` 替代 `<stdatomic.h>`
- **server.hpp**：添加 include guard (`#ifndef SC_SERVER_HPP`)

### 4. 编译输出

#### 4.1 配置摘要

添加了详细的配置信息输出：

```
========================================
Scrcpy Lite Configuration Summary:
  Version: 1.0.0
  Build Type: Debug
  C Compiler: MSVC 19.44.35225.0
  C++ Compiler: MSVC 19.44.35225.0
  Install Prefix: C:/Program Files/scrcpy_lite
  Output Directory: E:/vsSource/scrcpy_cmake/build/bin
========================================
```

#### 4.2 输出目录

- **可执行文件**：`build/bin/`
- **静态库**：`build/lib/`
- **头文件**：`build/include/`

## 构建步骤

### Windows (MSVC)

```powershell
# 配置项目
cmake -B build -G "Visual Studio 17 2022" -A x64

# 编译
cmake --build build --config Debug

# 运行
.\build\bin\Debug\scrcpy_app.exe
```

### Linux/macOS (GCC/Clang)

```bash
# 配置项目
cmake -B build -DCMAKE_BUILD_TYPE=Release

# 编译
cmake --build build -j$(nproc)

# 运行
./build/bin/scrcpy_app
```

## 已知警告及处理

### 1. C4828 编码警告

**原因**：某些文件包含 UTF-8 BOM 或特殊字符  
**影响**：无实际影响，仅警告  
**建议**：保存文件时使用 UTF-8 无 BOM 编码

### 2. C4996 弃用警告

**原因**：`std::codecvt` 在 C++17 中已弃用  
**影响**：编译成功，运行时正常  
**建议**：未来可迁移到 `MultiByteToWideChar()` API

### 3. LNK4006 重复定义警告

**原因**：`sc_mutex_init` 在头文件中定义为非内联函数  
**建议**：将函数实现移到 `.cpp` 文件或标记为 `inline`

## 项目结构

```
scrcpy_cmake/
├── CMakeLists.txt              # 主 CMake 配置
├── include/                    # 公共头文件
│   ├── common.h
│   ├── compat.h
│   ├── controller.h
│   ├── device.h
│   ├── net.h
│   └── str_util.h
└── src/                        # 源代码
    ├── CMakeLists.txt          # 子目录 CMake 配置
    ├── common.c
    ├── net.c
    ├── device.c
    ├── controller.c
    ├── str_util.c
    ├── main.c
    ├── server.cpp              # C++ 服务器实现
    ├── srccpy.cpp              # C++ 主框架
    ├── env.cpp                 # 环境变量处理
    ├── adb/                    # ADB 相关模块
    │   ├── adb.cpp
    │   ├── adb.h
    │   ├── adb_tunnel.cpp
    │   └── adb_tunnel.h
    └── util/                   # 工具类
        ├── rand.cpp/h          # 随机数生成
        ├── sc_intr.cpp/h       # 中断处理
        └── tick.cpp/h          # 时间戳工具
```

## 下一步建议

1. **修复编码问题**：将所有源文件转换为 UTF-8 无 BOM 格式
2. **内联函数优化**：将 `sc_mutex_init` 等小函数标记为 `inline`
3. **实现移动语义**：完善 `sc_server` 的移动构造函数和赋值运算符
4. **添加单元测试**：集成 CTest 或 Google Test
5. **CI/CD 配置**：添加 GitHub Actions 或 GitLab CI 配置
6. **文档生成**：集成 Doxygen 自动生成 API 文档

## 总结

本次 CMake 完善成功将项目从纯 C 升级为 C/C++ 混合项目，实现了：

- ✅ 跨平台编译支持（Windows/Linux/macOS）
- ✅ C++17 标准支持
- ✅ 模块化源文件组织
- ✅ MSVC 和 GCC/Clang 兼容性
- ✅ 详细的配置信息输出

项目现在可以正常编译和运行！
