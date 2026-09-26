#ifndef JTB_VERSION_H
#define JTB_VERSION_H

// ---------------------------------------------------------------------------
// 版本号唯一来源。
// main.cpp 与 resource.rc 都 include 本文件，改这一处即可同步：
//   - 程序内用于检查更新的版本号 (main.cpp kCurrentVersion)
//   - exe 文件属性里显示的版本号   (resource.rc VERSIONINFO)
//
// 注意：Windows 资源编译器 (rc.exe) 的预处理器与 C 预处理器并不相同，
// 有两处差异直接影响本文件：
//   1) rc.exe 不识别 #pragma once，必须用传统 include guard（本文件已用）
//   2) 对 .h 文件，rc.exe 会忽略所有非预处理指令的行，
//      因此本文件只放 #define，不放变量声明或注释以外的内容
// ---------------------------------------------------------------------------

#define JTB_VER_MAJOR 5
#define JTB_VER_MINOR 1
#define JTB_VER_PATCH 0

// 窄字符串形式，供 resource.rc 的 VERSIONINFO 使用
#define JTB_VER_STR   "5.1"

// 宽字符串形式，供 main.cpp 使用。
// 必须两步展开：JTB_WIDEN 先把参数展开成 "5.1"，再由 JTB_WIDEN2 粘上 L 前缀。
// 只写一层的话 L##JTB_VER_STR 会粘成 LJTB_VER_STR，反而失效。
#define JTB_WIDEN2(x) L##x
#define JTB_WIDEN(x)  JTB_WIDEN2(x)
#define JTB_VER_WSTR  JTB_WIDEN(JTB_VER_STR)

// HTTP User-Agent 串，同样随版本号变化
#define JTB_UA_STR    "JiyuToolBox/" JTB_VER_STR
#define JTB_UA_WSTR   JTB_WIDEN(JTB_UA_STR)

#endif // JTB_VERSION_H
