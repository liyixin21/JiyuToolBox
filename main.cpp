// ============================================================================
// 极域工具箱 (JiyuToolBox) - C++ 版
// Copyright (C) 2026 liyixin21
// SPDX-License-Identifier: GPL-3.0-or-later
// 由 Python 4.1 版改写，Win32 原生对话框程序
// 界面使用 VS 资源编辑器（resource.rc 对话框模板）图形化设计
//
// 功能（原版 + 反控制增强）:
//   1. 解除断网    : 驱动欺骗(IOCTL 0x120014) + 暴力杀线程 MasterHelper/GATESRV + 停止 tdnetfilter 服务(不卸载)
//   2. 解除U盘限制 : 停止 tdfilefilter 服务(不卸载)
//   3. 启动/杀死极域: 检测 StudentMain.exe，有则暴力杀线程、无则降权启动(CreateProcessAsUser)
//   4. 挂起/恢复极域: NtSuspendProcess/NtResumeProcess
//   5. 解除键盘锁  : 循环 WH_KEYBOARD_LL 钩子 + TDKeybd 驱动欺骗(IOCTL 0x220000)
//   6. 窗口化广播  : 状态感知双向切换窗口化/全屏化（支持自动/CTRL+Q快捷键）
//   7. 退出黑屏    : 4 级递进（隐藏→取消置顶最小化→模拟ESC→确认杀极域）
//   8. 自我保护    : 防杀进程(ACL)/防截屏(WDA)/置顶窗口
//   9. 更新检查    : 拉取远端版本号，不一致时弹窗提示（ovr.txt 为 no 时强制退出）
// ============================================================================

#define _WIN32_WINNT 0x0A00
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX

#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#include <windowsx.h>
#include <wincon.h>
#include <commctrl.h>
#include <tlhelp32.h>
#include <shellapi.h>
#include <winhttp.h>
#include <winternl.h>
#include <wincrypt.h>
#include <winreg.h>
#include <winsvc.h>

#include <string>
#include <vector>
#include <thread>
#include <atomic>
#include <utility>
#include <cstdio>
#include <cwchar>

#pragma comment(lib, "winhttp.lib")
#pragma comment(lib, "ws2_32.lib")
#pragma comment(lib, "comctl32.lib")
#pragma comment(lib, "shell32.lib")
#pragma comment(lib, "advapi32.lib")

#include "resource.h"

// ---------------------------------------------------------------------------
// 常量（与原 Python 版保持一致）
// ---------------------------------------------------------------------------
static const wchar_t* kCurrentVersion = L"5.0";
static const wchar_t* kVersionUrl     = L"https://jiyutool.liyixin.vip/version.txt";
static const wchar_t* kOvrUrl         = L"https://jiyutool.liyixin.vip/ovr.txt";
static const wchar_t* kWebsiteUrl     = L"https://jiyutool.liyixin.vip/";
static const wchar_t* kMyIpUrl        = L"https://my.ip.cn/";
static const wchar_t* kBroadcastTitle   = L"屏幕广播";          // 极域投屏/广播窗口标题
static const wchar_t* kBlackScreenTitle = L"BlackScreen Window"; // 极域"黑屏安静"窗口标题（任务管理器可见）

// 查找极域"黑屏安静"窗口。注意：退出黑屏只针对该窗口，投屏(屏幕广播)等其他窗口一律不处理
static HWND FindJiyuDisplayWindow() {
    return FindWindowW(nullptr, kBlackScreenTitle);
}

// 自定义消息
#define WM_APP_LOG         (WM_APP + 1)   // lParam = 堆上的 wchar_t*（日志文本）
#define WM_APP_WINDOWIZE   (WM_APP + 2)   // CTRL+Q 热键触发窗口化广播
#define WM_APP_SHOW_UPDATE (WM_APP + 3)   // 显示更新提示对话框

// 热键 ID（RegisterHotKey 方式，不依赖键盘钩子，避免与"解除键盘锁"的钩子装卸互相干扰）
#define HOTKEY_ID_WINDOWIZE 1             // CTRL+Q → 窗口化广播
#define HOTKEY_ID_TOPMOST   2             // CTRL+W → 切换置顶

// ---------------------------------------------------------------------------
// 全局状态
// ---------------------------------------------------------------------------
static HWND g_hDlg = nullptr;
static std::atomic<bool> g_keyboardLockOn{ false };    // 键盘锁解除循环开关
static std::thread g_unlockThread;                     // 键盘锁解除线程
static HWND g_lastBroadcastHwnd = nullptr;             // 上次检测到的广播窗口
static std::atomic<bool> g_topmostOn{ false };         // 置顶窗口开关
static std::thread g_topmostThread;                    // 置顶窗口线程
static CRITICAL_SECTION g_topmostCs;                   // 置顶操作串行化（轮询线程/事件驱动并发保护）
static std::atomic<bool> g_blackScreenBusy{ false };   // 退出黑屏流程防重入（防止多次点击弹多个确认框）
static HHOOK g_cbtHook = nullptr;                      // 防截屏 CBT 钩子（保护弹窗）

// ---------------------------------------------------------------------------
// 工具函数
// ---------------------------------------------------------------------------
static std::wstring TrimWs(const std::wstring& s) {
    size_t b = s.find_first_not_of(L" \t\r\n");
    if (b == std::wstring::npos) return L"";
    size_t e = s.find_last_not_of(L" \t\r\n");
    return s.substr(b, e - b + 1);
}

// GBK/GB2312 字节串 -> UTF-16（用于解析 my.ip.cn 等 GBK 页面/输出）
static std::wstring DecodeGbk(const std::string& s) {
    if (s.empty()) return L"";
    UINT cp = 936;
    int n = MultiByteToWideChar(936, 0, s.c_str(), (int)s.size(), nullptr, 0);
    if (n <= 0) {
        cp = CP_ACP;
        n = MultiByteToWideChar(cp, 0, s.c_str(), (int)s.size(), nullptr, 0);
    }
    if (n <= 0) return L"";
    std::wstring w(n, L'\0');
    MultiByteToWideChar(cp, 0, s.c_str(), (int)s.size(), &w[0], n);
    return w;
}

static std::string WideToUtf8(const std::wstring& w) {
    if (w.empty()) return "";
    int n = WideCharToMultiByte(CP_UTF8, 0, w.c_str(), (int)w.size(), nullptr, 0, nullptr, nullptr);
    std::string s(n, '\0');
    WideCharToMultiByte(CP_UTF8, 0, w.c_str(), (int)w.size(), &s[0], n, nullptr, nullptr);
    return s;
}

// UTF-8 字节串 -> UTF-16（my.ip.cn 等页面新版为 UTF-8）
static std::wstring DecodeUtf8(const std::string& s) {
    if (s.empty()) return L"";
    int n = MultiByteToWideChar(CP_UTF8, 0, s.c_str(), (int)s.size(), nullptr, 0);
    if (n <= 0) return L"";
    std::wstring w(n, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, s.c_str(), (int)s.size(), &w[0], n);
    return w;
}

// 把字符串转成 JSON 字符串字面量（转义引号/反斜杠/换行）
static std::string JsonEscape(const std::string& s) {
    std::string out;
    out.reserve(s.size());
    for (char c : s) {
        switch (c) {
            case '"':  out += "\\\""; break;
            case '\\': out += "\\\\"; break;
            case '\n': out += "\\n";  break;
            case '\r': out += "\\r";  break;
            case '\t': out += "\\t";  break;
            default:   out += c;      break;
        }
    }
    return out;
}

// ---------------------------------------------------------------------------
// 日志（跨线程安全：工作线程 PostMessage，UI 线程负责写入日志框）
// ---------------------------------------------------------------------------

static void LogToDlg(const std::wstring& msg) {
    wchar_t* p = new wchar_t[msg.size() + 1];
    wcscpy_s(p, msg.size() + 1, msg.c_str());
    PostMessageW(g_hDlg, WM_APP_LOG, 0, (LPARAM)p);
}

static void AppendLog(HWND hDlg, const std::wstring& msg) {
    SYSTEMTIME st;
    GetLocalTime(&st);
    wchar_t buf[512];
    swprintf_s(buf, L"%02d:%02d:%02d - %s\r\n",
               st.wHour, st.wMinute, st.wSecond, msg.c_str());
    HWND hLog = GetDlgItem(hDlg, IDC_LOG);
    if (!hLog) return;
    int len = GetWindowTextLengthW(hLog);
    SendMessageW(hLog, EM_SETSEL, len, len);
    SendMessageW(hLog, EM_REPLACESEL, FALSE, (LPARAM)buf);
    SendMessageW(hLog, EM_SCROLLCARET, 0, 0);
}

// ---------------------------------------------------------------------------
// 简单的 WinHTTP 请求封装（GET / POST JSON）
// ---------------------------------------------------------------------------
static std::string HttpRequest(const std::wstring& url, const std::wstring& method, const std::string* body) {
    bool https = _wcsnicmp(url.c_str(), L"https://", 8) == 0;
    std::wstring rest = https ? url.substr(8) : url.substr(7);
    size_t slash = rest.find(L'/');
    std::wstring hostport = (slash == std::wstring::npos) ? rest : rest.substr(0, slash);
    std::wstring path = (slash == std::wstring::npos) ? L"/" : rest.substr(slash);

    INTERNET_PORT port = https ? INTERNET_DEFAULT_HTTPS_PORT : INTERNET_DEFAULT_HTTP_PORT;
    std::wstring host = hostport;
    size_t colon = hostport.rfind(L':');
    if (colon != std::wstring::npos) {
        host = hostport.substr(0, colon);
        port = (INTERNET_PORT)_wtoi(hostport.substr(colon + 1).c_str());
    }

    HINTERNET hSession = WinHttpOpen(L"JiyuToolBox/5.0",
                                     WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
                                     WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
    if (!hSession) return "";

    HINTERNET hConnect = WinHttpConnect(hSession, host.c_str(), port, 0);
    if (!hConnect) { WinHttpCloseHandle(hSession); return ""; }

    std::wstring verb = method.empty() ? L"GET" : method;
    HINTERNET hReq = WinHttpOpenRequest(hConnect, verb.c_str(), path.c_str(), nullptr,
                                        WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES,
                                        https ? WINHTTP_FLAG_SECURE : 0);
    if (!hReq) { WinHttpCloseHandle(hConnect); WinHttpCloseHandle(hSession); return ""; }

    WinHttpSetTimeouts(hReq, 5000, 5000, 5000, 5000);

    std::wstring extraHeaders;
    if (body) extraHeaders = L"Content-Type: application/json\r\n";
    LPCWSTR hdrs = extraHeaders.empty() ? WINHTTP_NO_ADDITIONAL_HEADERS : extraHeaders.c_str();

    BOOL sent = WinHttpSendRequest(hReq, hdrs, -1L,
                                   body ? (LPVOID)body->data() : WINHTTP_NO_REQUEST_DATA,
                                   body ? (DWORD)body->size() : 0,
                                   body ? (DWORD)body->size() : 0, 0);

    std::string out;
    if (sent && WinHttpReceiveResponse(hReq, nullptr)) {
        DWORD avail = 0;
        for (;;) {
            if (!WinHttpQueryDataAvailable(hReq, &avail)) break;
            if (avail == 0) break;
            std::string chunk(avail, '\0');
            DWORD read = 0;
            if (!WinHttpReadData(hReq, &chunk[0], avail, &read)) break;
            chunk.resize(read);
            out += chunk;
        }
    }

    WinHttpCloseHandle(hReq);
    WinHttpCloseHandle(hConnect);
    WinHttpCloseHandle(hSession);
    return out;
}

static std::string HttpGet(const std::wstring& url) {
    return HttpRequest(url, L"GET", nullptr);
}


// ---------------------------------------------------------------------------
// 进程检测
// ---------------------------------------------------------------------------
static bool ProcessRunning(const wchar_t* name, DWORD* outPid) {
    bool found = false;
    HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snap != INVALID_HANDLE_VALUE) {
        PROCESSENTRY32W pe;
        pe.dwSize = sizeof(pe);
        if (Process32FirstW(snap, &pe)) {
            do {
                if (_wcsicmp(pe.szExeFile, name) == 0) {
                    found = true;
                    if (outPid) *outPid = pe.th32ProcessID;
                    break;
                }
            } while (Process32NextW(snap, &pe));
        }
        CloseHandle(snap);
    }
    return found;
}

// ---------------------------------------------------------------------------
// 执行 exe 命令并捕获输出（GBK），返回退出码；失败时 errText 给出原因
// ---------------------------------------------------------------------------

// Win32 错误码 -> 可读文本（含系统本地化描述）
static std::wstring WinErrorText(DWORD code) {
    wchar_t* buf = nullptr;
    DWORD n = FormatMessageW(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM |
                             FORMAT_MESSAGE_IGNORE_INSERTS,
                             nullptr, code, 0, (LPWSTR)&buf, 0, nullptr);
    std::wstring msg = (n && buf) ? TrimWs(buf) : L"";
    if (buf) LocalFree(buf);
    if (!msg.empty()) return L"错误码 " + std::to_wstring(code) + L"（" + msg + L"）";
    return L"错误码 " + std::to_wstring(code);
}

static int RunExeCapture(const std::wstring& exe, const std::wstring& args,
                         std::wstring* outText, std::wstring* errText) {
    wchar_t tmpDir[MAX_PATH];
    wchar_t tmpFile[MAX_PATH];
    if (!GetTempPathW(MAX_PATH, tmpDir) || !GetTempFileNameW(tmpDir, L"jtb", 0, tmpFile)) {
        if (errText) *errText = L"无法创建临时文件";
        return -1;
    }

    HANDLE hFile = CreateFileW(tmpFile, GENERIC_WRITE, FILE_SHARE_READ, nullptr,
                               CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (hFile == INVALID_HANDLE_VALUE) {
        if (errText) *errText = L"无法创建临时文件";
        return -1;
    }

    STARTUPINFOW si;
    ZeroMemory(&si, sizeof(si));
    si.cb = sizeof(si);
    si.dwFlags = STARTF_USESTDHANDLES;
    si.hStdOutput = hFile;
    si.hStdError = hFile;
    // GUI 程序没有控制台标准句柄，直接继承 GetStdHandle 会得到无效句柄，
    // 导致 CreateProcessW 失败（ERROR_INVALID_HANDLE）；改用 NUL 设备。
    si.hStdInput = CreateFileW(L"NUL", GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE,
                               nullptr, OPEN_EXISTING, 0, nullptr);

    std::wstring cmdline = L"\"" + exe + L"\" " + args;
    std::vector<wchar_t> cmdBuf(cmdline.begin(), cmdline.end());
    cmdBuf.push_back(L'\0');

    PROCESS_INFORMATION pi;
    ZeroMemory(&pi, sizeof(pi));
    // lpApplicationName 传 NULL，模块名放在命令行第一个 token（带引号），
    // 系统会按标准搜索路径查找 System32 下的 taskkill/sc 等；直接传裸文件名不会走 PATH
    BOOL ok = CreateProcessW(nullptr, cmdBuf.data(), nullptr, nullptr, TRUE,
                             CREATE_NO_WINDOW, nullptr, nullptr, &si, &pi);
    DWORD creErr = GetLastError();
    if (si.hStdInput != nullptr && si.hStdInput != INVALID_HANDLE_VALUE) CloseHandle(si.hStdInput);
    CloseHandle(hFile);

    int code = -1;
    if (ok) {
        WaitForSingleObject(pi.hProcess, INFINITE);
        GetExitCodeProcess(pi.hProcess, (DWORD*)&code);
        CloseHandle(pi.hThread);
        CloseHandle(pi.hProcess);
    } else if (errText) {
        *errText = WinErrorText(creErr);
    }

    if (outText) {
        HANDLE hR = CreateFileW(tmpFile, GENERIC_READ, FILE_SHARE_READ, nullptr,
                                OPEN_EXISTING, 0, nullptr);
        if (hR != INVALID_HANDLE_VALUE) {
            LARGE_INTEGER sz;
            sz.QuadPart = 0;
            GetFileSizeEx(hR, &sz);
            if (sz.QuadPart > 0 && sz.QuadPart < 16 * 1024 * 1024) {
                std::string bytes((size_t)sz.QuadPart, '\0');
                DWORD rd = 0;
                ReadFile(hR, &bytes[0], (DWORD)sz.QuadPart, &rd, nullptr);
                bytes.resize(rd);
                *outText = DecodeGbk(bytes);
            }
            CloseHandle(hR);
        }
        DeleteFileW(tmpFile);
    }
    return code;
}

// 一条命令：cmd 为"exe 参数"，runMsg 执行前提示，okMsg 成功提示
struct CmdStep {
    std::wstring cmd;
    std::wstring runMsg;
    std::wstring okMsg;
};

static void RunCommandSteps(const std::vector<CmdStep>& steps) {
    for (const auto& s : steps) {
        LogToDlg(s.runMsg);
        size_t sp = s.cmd.find(L' ');
        std::wstring exe = s.cmd.substr(0, sp);
        std::wstring args = (sp == std::wstring::npos) ? L"" : s.cmd.substr(sp + 1);
        std::wstring out, err;
        int code = RunExeCapture(exe, args, &out, &err);
        LogToDlg(code == 0 ? s.okMsg : L"操作失败（可能缺少权限或未安装相关组件）");
    }
}

// ---------------------------------------------------------------------------
// 反控制 / 系统工具函数（借鉴 MythwareToolkit 思路）
// ---------------------------------------------------------------------------

static bool EnablePrivilege(const wchar_t* priv);   // 前向声明（定义在其后）

// 暴力杀进程：枚举目标进程全部线程逐个 TerminateThread。
// 极域 SSDT Hook 了 NtOpenProcess（TerminateProcess 依赖它），但没 Hook TerminateThread。
// 返回是否至少终止了一条线程。
static bool KillProcessByThreads(DWORD pid) {
    // MasterHelper/GATESRV 等以 SYSTEM 权限运行，管理员需先启用 SeDebugPrivilege
    // 才能打开其线程句柄（管理员令牌默认含该特权但禁用）
    EnablePrivilege(L"SeDebugPrivilege");

    bool ok = false;
    HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPTHREAD, pid);
    if (snap != INVALID_HANDLE_VALUE) {
        THREADENTRY32 te;
        te.dwSize = sizeof(te);
        for (BOOL f = Thread32First(snap, &te); f; f = Thread32Next(snap, &te)) {
            if (te.th32OwnerProcessID == pid) {
                HANDLE h = OpenThread(THREAD_TERMINATE, FALSE, te.th32ThreadID);
                if (h) {
                    if (TerminateThread(h, 0)) ok = true;
                    CloseHandle(h);
                }
            }
        }
        CloseHandle(snap);
    }
    return ok;
}

// 按名称杀死全部同名进程（极域可能存在多个实例，必须全杀，否则剩余进程继续运行像"重启"）
static void KillAllByName(const wchar_t* name, const wchar_t* label) {
    DWORD killed = 0;
    HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snap != INVALID_HANDLE_VALUE) {
        PROCESSENTRY32W pe;
        pe.dwSize = sizeof(pe);
        if (Process32FirstW(snap, &pe)) {
            do {
                if (_wcsicmp(pe.szExeFile, name) == 0) {
                    if (KillProcessByThreads(pe.th32ProcessID)) ++killed;
                }
            } while (Process32NextW(snap, &pe));
        }
        CloseHandle(snap);
    }
    if (killed > 0) LogToDlg(std::wstring(label) + L" 已终止 " + std::to_wstring(killed) + L" 个");
    else            LogToDlg(std::wstring(label) + L" 未运行");
}

// 按进程名查 PID（不区分大小写），未找到返回 0
static DWORD GetProcessIDFromName(const std::wstring& name) {
    DWORD pid = 0;
    HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snap != INVALID_HANDLE_VALUE) {
        PROCESSENTRY32W pe;
        pe.dwSize = sizeof(pe);
        if (Process32FirstW(snap, &pe)) {
            do {
                if (_wcsicmp(pe.szExeFile, name.c_str()) == 0) { pid = pe.th32ProcessID; break; }
            } while (Process32NextW(snap, &pe));
        }
        CloseHandle(snap);
    }
    return pid;
}

// 启用当前进程的指定特权（CreateProcessAsUser 需要 SE_ASSIGNPRIMARYTOKEN / SE_INCREASE_QUOTA）
static bool EnablePrivilege(const wchar_t* priv) {
    HANDLE hTok = nullptr;
    if (!OpenProcessToken(GetCurrentProcess(), TOKEN_ADJUST_PRIVILEGES | TOKEN_QUERY, &hTok)) return false;
    LUID luid;
    if (!LookupPrivilegeValueW(nullptr, priv, &luid)) { CloseHandle(hTok); return false; }
    TOKEN_PRIVILEGES tp;
    tp.PrivilegeCount = 1;
    tp.Privileges[0].Luid = luid;
    tp.Privileges[0].Attributes = SE_PRIVILEGE_ENABLED;
    bool ok = AdjustTokenPrivileges(hTok, FALSE, &tp, 0, nullptr, nullptr) && GetLastError() == ERROR_SUCCESS;
    CloseHandle(hTok);
    return ok;
}

typedef LONG (NTAPI* pfnNtSusRes)(HANDLE);
typedef LONG (NTAPI* pfnNtQSI)(ULONG, PVOID, ULONG, PULONG);

static LONG NtSuspend(DWORD pid) {
    static pfnNtSusRes f = (pfnNtSusRes)GetProcAddress(GetModuleHandleW(L"ntdll.dll"),
                                                       "NtSuspendProcess");
    if (!f) return -1;
    HANDLE h = OpenProcess(PROCESS_SUSPEND_RESUME, FALSE, pid);
    if (!h) return -1;
    LONG r = f(h);
    CloseHandle(h);
    return r;
}

static LONG NtResume(DWORD pid) {
    static pfnNtSusRes f = (pfnNtSusRes)GetProcAddress(GetModuleHandleW(L"ntdll.dll"),
                                                       "NtResumeProcess");
    if (!f) return -1;
    HANDLE h = OpenProcess(PROCESS_SUSPEND_RESUME, FALSE, pid);
    if (!h) return -1;
    LONG r = f(h);
    CloseHandle(h);
    return r;
}

static bool SuspendProcess(DWORD pid) { return NtSuspend(pid) == 0; }
static bool ResumeProcess(DWORD pid)  { return NtResume(pid) == 0; }

// 判断进程是否处于挂起状态（NtQuerySystemInformation 遍历线程 WaitReason==Suspended）
static bool ProcessIsSuspended(DWORD pid) {
    static pfnNtQSI q = (pfnNtQSI)GetProcAddress(GetModuleHandleW(L"ntdll.dll"),
                                                 "NtQuerySystemInformation");
    if (!q) return false;
    ULONG sz = 0;
    q(5, nullptr, 0, &sz);                       // SystemProcessInformation = 5
    if (sz == 0) return false;
    std::vector<BYTE> buf(sz + 64);
    ULONG ret = 0;
    if (q(5, buf.data(), (ULONG)buf.size(), &ret) != 0) return false;
    SYSTEM_PROCESS_INFORMATION* spi = reinterpret_cast<SYSTEM_PROCESS_INFORMATION*>(buf.data());
    for (;;) {
        if ((ULONG_PTR)spi->UniqueProcessId == (ULONG_PTR)pid && spi->NumberOfThreads > 0) {
            SYSTEM_THREAD_INFORMATION* st =
                reinterpret_cast<SYSTEM_THREAD_INFORMATION*>(spi + 1);
            for (ULONG i = 0; i < spi->NumberOfThreads; ++i) {
                if (st[i].ThreadState == 5 /*Waiting*/ && st[i].WaitReason == 5 /*Suspended*/)
                    return true;
            }
            return false;
        }
        if (spi->NextEntryOffset == 0) break;
        spi = reinterpret_cast<SYSTEM_PROCESS_INFORMATION*>(
                reinterpret_cast<BYTE*>(spi) + spi->NextEntryOffset);
    }
    return false;
}

// 退出黑屏：只对极域"黑屏安静"窗口(BlackScreen Window)生效，其它窗口一律不处理。
// 步骤：激活后ESC → 隐藏黑屏窗口 → 确认是否真的退出（未退出才询问是否杀极域）。
// 安全约束：ESC 是全局按键，只在"确认前台确实是黑屏窗口"时才发送，避免误关其他窗口。
static void ExitBlackScreen() {
    // 防重入：一次只能有一个退出黑屏流程（否则多次点击会弹多个确认框）
    bool expected = false;
    if (!g_blackScreenBusy.compare_exchange_strong(expected, true)) {
        LogToDlg(L"退出黑屏流程已在进行中，请等待确认框处理完");
        return;
    }
    std::thread([] {
        HWND bw = FindJiyuDisplayWindow();   // 只查 BlackScreen Window
        if (!bw) {
            LogToDlg(L"当前没有黑屏窗口，无需操作");
            g_blackScreenBusy = false;
            return;
        }
        // [1/3] 尝试用 ESC 退出黑屏（只在前台确认为黑屏窗口时发送）
        SetForegroundWindow(bw);
        Sleep(50);
        if (GetForegroundWindow() == bw) {
            keybd_event(VK_ESCAPE, 0, 0, 0);
            keybd_event(VK_ESCAPE, 0, KEYEVENTF_KEYUP, 0);
            LogToDlg(L"[1/3] 已尝试退出黑屏");
        } else {
            LogToDlg(L"[1/3] 无法操作黑屏窗口，改用隐藏");
        }
        // [2/3] 隐藏黑屏窗口（ESC 无效或未发送时的兜底）
        ShowWindow(bw, SW_HIDE);
        LogToDlg(L"[2/3] 已隐藏黑屏窗口");
        // [3/3] 等待片刻给极域响应时间，再检查黑屏是否真的退出
        Sleep(500);
        HWND now = FindJiyuDisplayWindow();
        if (now && IsWindowVisible(now)) {
            if (MessageBoxW(nullptr, L"仍未退出黑屏，是否强制结束极域程序？", L"退出黑屏",
                            MB_YESNO | MB_ICONQUESTION | MB_SETFOREGROUND) == IDYES) {
                DWORD pid = GetProcessIDFromName(L"StudentMain.exe");
                if (pid && KillProcessByThreads(pid)) {
                    LogToDlg(L"[3/3] 已强制结束极域程序");
                } else {
                    LogToDlg(L"[3/3] 极域程序未找到或结束失败");
                }
            }
        } else {
            LogToDlg(L"[3/3] 黑屏已退出");
        }
        g_blackScreenBusy = false;
    }).detach();
}

// 防截屏：WDA_EXCLUDEFROMCAPTURE 使窗口在录屏/截图中不显示
static void SetWindowCaptureProtect(HWND hwnd, bool on) {
    if (hwnd && IsWindow(hwnd))
        SetWindowDisplayAffinity(hwnd, on ? WDA_EXCLUDEFROMCAPTURE : WDA_NONE);
}

// WH_CBT 线程钩子：弹窗（#32770）激活时自动防截屏
static LRESULT CALLBACK CbtHookProc(int nCode, WPARAM wParam, LPARAM lParam) {
    if (nCode == HCBT_ACTIVATE) {
        HWND h = (HWND)wParam;
        wchar_t cls[64];
        GetClassNameW(h, cls, 64);
        if (_wcsicmp(cls, L"#32770") == 0) SetWindowDisplayAffinity(h, WDA_EXCLUDEFROMCAPTURE);
    }
    return CallNextHookEx(nullptr, nCode, wParam, lParam);
}

static void InstallCbtHook() {
    if (!g_cbtHook) g_cbtHook = SetWindowsHookExW(WH_CBT, CbtHookProc, nullptr, GetCurrentThreadId());
}

static void RemoveCbtHook() {
    if (g_cbtHook) { UnhookWindowsHookEx(g_cbtHook); g_cbtHook = nullptr; }
}

// 保持置顶（方案2，防黑屏/投屏闪烁）：
//  仅当自己窗口中心点被其它窗口盖住时：
//  1) 把所有盖住该点的可见 TOPMOST 窗口（极域黑屏/投屏等，不论是否全屏）降级为普通窗口；
//  2) 再把自己顶回 HWND_TOPMOST。
//  未被盖住时零操作，避免无谓 SetWindowPos 造成的闪烁。
// 轮询线程与 WM_WINDOWPOSCHANGED 事件驱动均调用；临界区保证并发安全。
static void KeepTopmost(HWND hwnd) {
    if (!hwnd || !IsWindow(hwnd)) return;
    RECT rc;
    if (!GetWindowRect(hwnd, &rc) || rc.right <= rc.left || rc.bottom <= rc.top) return;
    POINT pt = { (rc.left + rc.right) / 2, (rc.top + rc.bottom) / 2 };
    HWND top = WindowFromPoint(pt);
    if (top == hwnd || IsChild(hwnd, top)) return;      // 已是最前 → 零操作

    static POINT s_pt;
    static HWND s_self;
    EnterCriticalSection(&g_topmostCs);
    s_pt = pt;
    s_self = hwnd;
    // 降级盖住自己中心点的所有可见 TOPMOST 窗口
    EnumWindows([](HWND h, LPARAM) -> BOOL {
        if (h == s_self) return TRUE;
        if (!IsWindowVisible(h)) return TRUE;
        if (GetWindowLongPtrW(h, GWL_EXSTYLE) & WS_EX_TOPMOST) {
            RECT r;
            if (GetWindowRect(h, &r) &&
                r.left <= s_pt.x && s_pt.x <= r.right &&
                r.top <= s_pt.y && s_pt.y <= r.bottom) {
                SetWindowPos(h, HWND_NOTOPMOST, 0, 0, 0, 0,
                             SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
            }
        }
        return TRUE;
    }, 0);
    LeaveCriticalSection(&g_topmostCs);
    // 顶回自己
    SetWindowPos(hwnd, HWND_TOPMOST, 0, 0, 0, 0,
                 SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
}

// 置顶轮询线程：被盖时立即处理，间隔 200ms 兜底（事件驱动响应不了的情况）
static void TopmostLoop() {
    while (g_topmostOn.load()) {
        KeepTopmost(g_hDlg);
        Sleep(200);
    }
}

// 设置/切换置顶（复选框与 CTRL+W 热键共用），并同步复选框勾选状态
static void SetTopmost(HWND hDlg, bool on) {
    if (on) {
        g_topmostOn = true;
        if (g_topmostThread.joinable()) g_topmostThread.join();
        g_topmostThread = std::thread(TopmostLoop);
        LogToDlg(L"置顶窗口已开启");
    } else {
        g_topmostOn = false;
        if (g_topmostThread.joinable()) g_topmostThread.join();
        SetWindowPos(hDlg, HWND_NOTOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE);
        LogToDlg(L"置顶窗口已关闭");
    }
    CheckDlgButton(hDlg, IDC_CHK_TOPMOST, on ? BST_CHECKED : BST_UNCHECKED);
}

// ---------------------------------------------------------------------------
// 解除键盘锁：循环安装/卸载低级键盘钩子（与原 Python 版算法一致）
// ---------------------------------------------------------------------------
static LRESULT CALLBACK UnlockHookProc(int nCode, WPARAM wParam, LPARAM lParam) {
    (void)wParam; (void)lParam;
    return CallNextHookEx(nullptr, nCode, wParam, lParam);
}

static void KeyboardUnlockLoop() {
    while (g_keyboardLockOn.load()) {
        HHOOK h = SetWindowsHookExW(WH_KEYBOARD_LL, UnlockHookProc, GetModuleHandleW(nullptr), 0);
        if (!h) {
            Sleep(100);
            continue;
        }
        Sleep(50);
        UnhookWindowsHookEx(h);
    }
}

// ---------------------------------------------------------------------------
// CTRL+Q / CTRL+W 热键：RegisterHotKey 注册系统热键（WM_HOTKEY 消息）。
// 不用 WH_KEYBOARD_LL 钩子，避免与"解除键盘锁"的高频钩子装卸互相干扰导致热键失效。
// ---------------------------------------------------------------------------
static void ToggleHotkey(HWND hDlg, bool enable) {
    if (enable) {
        if (RegisterHotKey(hDlg, HOTKEY_ID_WINDOWIZE, MOD_CONTROL | MOD_NOREPEAT, 'Q')) {
            LogToDlg(L"CTRL+Q快捷键已启用");
        } else {
            LogToDlg(L"CTRL+Q注册失败（可能已被其他程序占用）");
        }
    } else {
        UnregisterHotKey(hDlg, HOTKEY_ID_WINDOWIZE);
        LogToDlg(L"CTRL+Q快捷键已禁用");
    }
}

static void ToggleHotkeyW(HWND hDlg, bool enable) {
    if (enable) {
        if (RegisterHotKey(hDlg, HOTKEY_ID_TOPMOST, MOD_CONTROL | MOD_NOREPEAT, 'W')) {
            LogToDlg(L"CTRL+W置顶快捷键已启用");
        } else {
            LogToDlg(L"CTRL+W注册失败（可能已被其他程序占用）");
        }
    } else {
        UnregisterHotKey(hDlg, HOTKEY_ID_TOPMOST);
        LogToDlg(L"CTRL+W置顶快捷键已禁用");
    }
}

// ---------------------------------------------------------------------------
// 窗口化广播
// ---------------------------------------------------------------------------
// 广播窗口化/全屏化：先读样式判断当前状态，再发命令双向切换
static void ToggleBroadcastWindow() {
    HWND hwnd = FindWindowW(nullptr, kBroadcastTitle);
    if (!hwnd) {
        LogToDlg(L"未找到投屏窗口");
        return;
    }
    LONG style = GetWindowLongW(hwnd, GWL_STYLE);
    bool bWindowing = (style & (WS_CAPTION | WS_SIZEBOX)) != 0;   // 有标题/可调大小 = 已窗口化
    BOOL r = PostMessageW(hwnd, WM_COMMAND, (BM_CLICK << 16) | 1004, 0);
    LogToDlg(r ? L"已切换投屏窗口" : L"切换失败");
    if (g_hDlg) {
        SetDlgItemTextW(g_hDlg, IDC_BTN_WINDOWIZE, bWindowing ? L"全屏化广播" : L"窗口化广播");
    }
}

// ---------------------------------------------------------------------------
// 按钮动作
// ---------------------------------------------------------------------------

// 查询驱动服务状态：返回 -1=服务不存在/无权限，0=已停止，1=运行中
static int QueryServiceState(const wchar_t* svc) {
    SC_HANDLE mgr = OpenSCManagerW(nullptr, nullptr, SC_MANAGER_CONNECT);
    if (!mgr) return -1;
    int result = -1;
    SC_HANDLE s = OpenServiceW(mgr, svc, SERVICE_QUERY_STATUS);
    if (s) {
        SERVICE_STATUS ss;
        if (QueryServiceStatus(s, &ss)) {
            result = (ss.dwCurrentState == SERVICE_STOPPED) ? 0 : 1;
        }
        CloseServiceHandle(s);
    }
    CloseServiceHandle(mgr);
    return result;
}

static void OnNetwork() {
    std::thread([] {
        // 第1层：欺骗限网驱动（IOCTL 0x120014 由 IDA 反编译得出）
        HANDLE h = CreateFileW(L"\\\\.\\TDNetFilter", GENERIC_READ | GENERIC_WRITE,
                               FILE_SHARE_READ, nullptr, OPEN_EXISTING, 0, nullptr);
        if (h != INVALID_HANDLE_VALUE) {
            DeviceIoControl(h, 0x120014, nullptr, 0, nullptr, 0, nullptr, nullptr);
            CloseHandle(h);
            LogToDlg(L"限网驱动已放行");
        } else {
            // 打不开驱动设备：可能之前已停止过（重复点击解除），查服务状态给出准确提示
            int st = QueryServiceState(L"tdnetfilter");
            if (st == 0) LogToDlg(L"限网驱动已停止");
            else         LogToDlg(L"限网驱动不可用（未装极域或驱动已停）");
        }
        // 第2层：终止网关服务进程（暴力杀线程，绕过 NtOpenProcess 钩子）
        auto killByName = [](const wchar_t* name, const wchar_t* label) {
            DWORD pid = GetProcessIDFromName(name);
            if (pid) {
                LogToDlg(L"正在终止 " + std::wstring(label));
                LogToDlg(KillProcessByThreads(pid) ? (std::wstring(label) + L" 已终止")
                                                   : (std::wstring(label) + L" 终止失败"));
            } else {
                LogToDlg(std::wstring(label) + L" 未运行");
            }
        };
        killByName(L"MasterHelper.exe", L"MasterHelper");
        killByName(L"GATESRV.exe", L"GATESRV");
        // 第3层：先确认 tdnetfilter 驱动状态，存在且运行中才停止（不卸载，为恢复留空间）
        int st = QueryServiceState(L"tdnetfilter");
        if (st < 0) {
            LogToDlg(L"未检测到限网驱动，跳过停止");
        } else if (st == 0) {
            LogToDlg(L"限网驱动已停止");
        } else {
            RunCommandSteps({
                { L"sc.exe stop tdnetfilter", L"正在停止限网驱动...", L"网络限制已解除" }
            });
        }
    }).detach();
}

static void OnUsb() {
    std::thread([] {
        // 先确认 tdfilefilter 驱动状态，存在且运行中才停止
        int st = QueryServiceState(L"tdfilefilter");
        if (st < 0) {
            LogToDlg(L"未检测到U盘限制驱动，跳过停止");
        } else if (st == 0) {
            LogToDlg(L"U盘限制已解除");
        } else {
            RunCommandSteps({
                { L"sc.exe stop tdfilefilter", L"正在解除U盘限制...", L"U盘限制已解除" }
            });
        }
    }).detach();
}

// 恢复限制：重新启动限网/文件过滤驱动（反向操作解除），并重新拉起 MasterHelper/GATESRV。
// 经验：重启极域(StudentMain)不会恢复 MasterHelper/GATESRV；
// MasterHelper 需直接启动其 exe，GATESRV 是服务需从服务启动（直接运行 exe 无效）。
static std::wstring GetJiyuStudentPath();   // 前向声明（定义在其后）
static void OnRestore() {
    std::thread([] {
        // 1) 恢复网络限制：启动 tdnetfilter
        int st = QueryServiceState(L"tdnetfilter");
        if (st < 0) {
            LogToDlg(L"未检测到限网驱动，无法恢复网络限制");
        } else if (st == 1) {
            LogToDlg(L"网络限制已生效");
        } else {
            RunCommandSteps({
                { L"sc.exe start tdnetfilter", L"正在恢复网络限制...", L"网络限制已恢复" }
            });
        }
        // 2) 恢复U盘限制：启动 tdfilefilter
        int st2 = QueryServiceState(L"tdfilefilter");
        if (st2 < 0) {
            LogToDlg(L"未检测到U盘限制驱动，无法恢复U盘限制");
        } else if (st2 == 1) {
            LogToDlg(L"U盘限制已生效");
        } else {
            RunCommandSteps({
                { L"sc.exe start tdfilefilter", L"正在恢复U盘限制...", L"U盘限制已恢复" }
            });
        }
        // 3) 启动 MasterHelper（直接运行 exe，路径为极域安装目录下）
        std::wstring jiyuPath = GetJiyuStudentPath();
        size_t pos = jiyuPath.find_last_of(L"\\/");
        std::wstring helperPath = (pos != std::wstring::npos) ? jiyuPath.substr(0, pos + 1) : std::wstring();
        helperPath += L"MasterHelper.exe";
        if (GetFileAttributesW(helperPath.c_str()) != INVALID_FILE_ATTRIBUTES) {
            HINSTANCE r = ShellExecuteW(nullptr, L"open", helperPath.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
            LogToDlg((INT_PTR)r > 32 ? L"MasterHelper 已启动" : L"MasterHelper 启动失败");
        } else {
            LogToDlg(L"未找到 MasterHelper 程序");
        }
        // 4) 启动 GATESRV 服务（GATESRV 是服务，服务名 STUDSRV；直接运行 exe 无效，必须从服务启动）
        int st3 = QueryServiceState(L"STUDSRV");
        if (st3 < 0) {
            LogToDlg(L"未检测到 GATESRV 服务");
        } else if (st3 == 1) {
            LogToDlg(L"GATESRV 服务已在运行");
        } else {
            RunCommandSteps({
                { L"sc.exe start STUDSRV", L"正在启动 GATESRV 服务...", L"GATESRV 服务已启动" }
            });
        }
    }).detach();
}

// 从注册表读极域安装目录（借鉴 MythwareToolkit）：
// HKLM\SOFTWARE\TopDomain\e-Learning Class Standard\1.00\TargetDirectory
// 程序是 64 位，先读 32 位注册表视图(KEY_WOW64_32KEY)，失败再试 64 位视图。
// 读到的路径校验文件存在后才用，否则回退默认安装路径。返回 StudentMain.exe 完整路径。
static std::wstring GetJiyuStudentPath() {
    const std::wstring fallback = L"C:\\Program Files (x86)\\Mythware\\Classroom Management by Mythware\\StudentMain.exe";
    const wchar_t* subKey = L"SOFTWARE\\TopDomain\\e-Learning Class Standard\\1.00";
    wchar_t dir[MAX_PATH * 2] = {};
    bool got = false;
    for (DWORD view : { KEY_WOW64_32KEY, KEY_WOW64_64KEY }) {
        HKEY hk = nullptr;
        if (RegOpenKeyExW(HKEY_LOCAL_MACHINE, subKey, 0, KEY_QUERY_VALUE | view, &hk) == ERROR_SUCCESS) {
            DWORD type = REG_SZ, cb = sizeof(dir);
            if (RegQueryValueExW(hk, L"TargetDirectory", nullptr, &type, (LPBYTE)dir, &cb) == ERROR_SUCCESS &&
                type == REG_SZ && dir[0]) {
                got = true;
            }
            RegCloseKey(hk);
            if (got) break;
        }
    }
    if (got) {
        std::wstring p = dir;
        if (!p.empty() && p.back() != L'\\') p += L'\\';
        p += L"StudentMain.exe";
        if (GetFileAttributesW(p.c_str()) != INVALID_FILE_ATTRIBUTES) return p;
    }
    return fallback;
}

// 启动极域：优先用 explorer 的 token 降权启动（避免极域继承管理员权限），失败回退 ShellExecute。
// 返回：0=启动失败，1=降权启动成功，2=ShellExecute 回退启动成功
static int StartJiyuAsUser() {
    std::wstring path = GetJiyuStudentPath();

    HWND hShell = FindWindowW(L"Shell_TrayWnd", nullptr);
    DWORD expPid = 0;
    if (hShell) GetWindowThreadProcessId(hShell, &expPid);

    bool ok = false;
    if (expPid) {
        HANDLE hProc = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, expPid);
        if (hProc) {
            HANDLE hTok = nullptr;
            if (OpenProcessToken(hProc, TOKEN_DUPLICATE | TOKEN_QUERY, &hTok)) {
                HANDLE hDup = nullptr;
                if (DuplicateTokenEx(hTok, MAXIMUM_ALLOWED, nullptr, SecurityImpersonation,
                                     TokenPrimary, &hDup)) {
                    // CreateProcessWithTokenW 只需 SeImpersonatePrivilege（管理员默认拥有），
                    // 不依赖 SeAssignPrimaryTokenPrivilege（部分环境管理员令牌缺失，导致 1314）
                    EnablePrivilege(L"SeImpersonatePrivilege");
                    STARTUPINFOW si;
                    PROCESS_INFORMATION pi;
                    ZeroMemory(&si, sizeof(si));
                    ZeroMemory(&pi, sizeof(pi));
                    si.cb = sizeof(si);
                    ok = CreateProcessWithTokenW(hDup, 0, path.c_str(), nullptr,
                                                 CREATE_NEW_PROCESS_GROUP | NORMAL_PRIORITY_CLASS,
                                                 nullptr, nullptr, &si, &pi);
                    if (ok) { CloseHandle(pi.hThread); CloseHandle(pi.hProcess); }
                    CloseHandle(hDup);
                }
                CloseHandle(hTok);
            }
            CloseHandle(hProc);
        }
    }

    if (!ok) {
        // 降权启动失败，改用普通方式启动
        HINSTANCE r = ShellExecuteW(nullptr, L"open", path.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
        if ((INT_PTR)r <= 32) return 0;
        return 2;
    }
    return 1;
}

static void OnJiyu() {
    DWORD pid = 0;
    if (ProcessRunning(L"StudentMain.exe", &pid)) {
        std::thread([] {
            // 杀死极域全部相关进程（可能存在多个实例，全杀），并停止网关服务防止重新拉起。
            // 顺序：先停 STUDSRV 服务再杀 GATESRV 进程，避免先杀进程导致服务状态异常、stop 报失败
            KillAllByName(L"StudentMain.exe", L"极域主程序");
            KillAllByName(L"MasterHelper.exe", L"MasterHelper");
            int st = QueryServiceState(L"STUDSRV");
            if (st == 1) {
                RunCommandSteps({
                    { L"sc.exe stop STUDSRV", L"正在停止 GATESRV 服务...", L"GATESRV 服务已停止" }
                });
            }
            KillAllByName(L"GATESRV.exe", L"GATESRV");
            LogToDlg(L"极域已全部停止");
        }).detach();
    } else {
        LogToDlg(L"正在启动极域...");
        LogToDlg(StartJiyuAsUser() > 0 ? L"极域已启动" : L"极域启动失败");
    }
}

static void OnKeyboard(HWND hDlg) {
    if (g_keyboardLockOn.load()) {
        // 恢复键盘锁
        g_keyboardLockOn = false;
        if (g_unlockThread.joinable()) g_unlockThread.join();
        SetDlgItemTextW(hDlg, IDC_BTN_KEYBOARD, L"解除键盘锁");
        LogToDlg(L"键盘锁已恢复");
    } else {
        // 解除键盘锁
        g_keyboardLockOn = true;
        if (g_unlockThread.joinable()) g_unlockThread.join();
        g_unlockThread = std::thread(KeyboardUnlockLoop);
        SetDlgItemTextW(hDlg, IDC_BTN_KEYBOARD, L"关闭键盘锁");
        // 欺骗底层键盘驱动（解决 WH_KEYBOARD_LL 拦不住的 Ctrl+Alt+Del），控制码 0x220000
        HANDLE hDev = CreateFileW(L"\\\\.\\TDKeybd", GENERIC_READ | GENERIC_WRITE,
                                  FILE_SHARE_READ, nullptr, OPEN_EXISTING, 0, nullptr);
        if (hDev != INVALID_HANDLE_VALUE) {
            BOOL bEnable = TRUE;
            DeviceIoControl(hDev, 0x220000, &bEnable, sizeof(bEnable), nullptr, 0, nullptr, nullptr);
            CloseHandle(hDev);
        }
        LogToDlg(L"正在解除键盘锁...");
    }
}

// ---------------------------------------------------------------------------
// 周期检查（每 2 秒）：极域进程状态 + 广播窗口状态
// ---------------------------------------------------------------------------
static void OnPeriodicCheck(HWND hDlg) {
    DWORD pid = 0;
    bool running = ProcessRunning(L"StudentMain.exe", &pid);
    SetDlgItemTextW(hDlg, IDC_BTN_JIYU, running ? L"杀死极域" : L"启动极域");

    // 挂起/恢复按钮：按状态切换文字与可用性
    HWND hSuspend = GetDlgItem(hDlg, IDC_BTN_SUSPEND);
    if (running) {
        bool suspended = ProcessIsSuspended(pid);
        SetDlgItemTextW(hDlg, IDC_BTN_SUSPEND, suspended ? L"恢复极域" : L"挂起极域");
        EnableWindow(hSuspend, TRUE);
        SetDlgItemTextW(hDlg, IDC_STATUS,
            (std::wstring(suspended ? L"极域已挂起[PID:" : L"极域运行中[PID:") +
             std::to_wstring(pid) + L"]").c_str());
    } else {
        SetDlgItemTextW(hDlg, IDC_BTN_SUSPEND, L"挂起极域");
        EnableWindow(hSuspend, FALSE);
        SetDlgItemTextW(hDlg, IDC_STATUS, L"极域未运行");
    }

    HWND bw = FindWindowW(nullptr, kBroadcastTitle);
    if (bw) {
        EnableWindow(GetDlgItem(hDlg, IDC_BTN_WINDOWIZE), TRUE);
        // 按钮文字反映当前状态：已窗口化则显示"全屏化广播"
        LONG style = GetWindowLongW(bw, GWL_STYLE);
        bool bWindowing = (style & (WS_CAPTION | WS_SIZEBOX)) != 0;
        SetDlgItemTextW(hDlg, IDC_BTN_WINDOWIZE, bWindowing ? L"全屏化广播" : L"窗口化广播");
        if (bw != g_lastBroadcastHwnd) {          // 新出现的广播窗口
            g_lastBroadcastHwnd = bw;
            if (IsDlgButtonChecked(hDlg, IDC_CHK_AUTO) == BST_CHECKED) {
                ToggleBroadcastWindow();
            }
        }
    } else {
        EnableWindow(GetDlgItem(hDlg, IDC_BTN_WINDOWIZE), FALSE);
        SetDlgItemTextW(hDlg, IDC_BTN_WINDOWIZE, L"窗口化广播");
        g_lastBroadcastHwnd = nullptr;
    }
}

// ---------------------------------------------------------------------------
// 更新检查线程
// ---------------------------------------------------------------------------
static void UpdateCheckThreadFunc() {
    std::string verRaw = HttpGet(kVersionUrl);
    if (verRaw.empty()) {
        LogToDlg(L"检查更新失败（请检查网络）");
        return;
    }
    std::string ovrRaw = HttpGet(kOvrUrl);
    std::wstring latest = TrimWs(DecodeGbk(verRaw));
    std::wstring ovr = TrimWs(DecodeGbk(ovrRaw));

    // 只有服务端版本更高时才提示（避免服务端 version.txt 未更新时误报"发现新版本"）
    int ma = 0, sa = 0, mb = 0, sb = 0;
    swscanf_s(latest.c_str(), L"%d.%d", &ma, &sa);
    swscanf_s(kCurrentVersion, L"%d.%d", &mb, &sb);
    bool newer = (ma > mb) || (ma == mb && sa > sb);
    if (!newer) {
        LogToDlg(L"已是最新版本");
        return;
    }
    LogToDlg(L"发现新版本 " + latest + L"（当前 " + kCurrentVersion + L"）");
    PostMessageW(g_hDlg, WM_APP_SHOW_UPDATE, 0,
                 (LPARAM)new std::pair<std::wstring, std::wstring>(latest, ovr));
}


// ---------------------------------------------------------------------------
// 对话框过程
// ---------------------------------------------------------------------------
static void ShowUpdateDialog(HWND hDlg, const std::wstring& latest, const std::wstring& ovr) {
    std::wstring content = L"当前版本：" + std::wstring(kCurrentVersion) +
                           L"\n最新版本：" + latest +
                           L"\n请前往官网下载更新！";

    TASKDIALOGCONFIG cfg;
    ZeroMemory(&cfg, sizeof(cfg));
    cfg.cbSize = sizeof(cfg);
    cfg.hwndParent = hDlg;
    cfg.pszWindowTitle = L"更新提示";
    cfg.pszMainIcon = TD_INFORMATION_ICON;
    cfg.pszMainInstruction = L"发现新版本";
    cfg.pszContent = content.c_str();

    TASKDIALOG_BUTTON buttons[] = {
        { 1001, L"前往官网" },
        { 1002, L"关闭" }
    };
    cfg.pButtons = buttons;
    cfg.cButtons = 2;

    int sel = 0;
    TaskDialogIndirect(&cfg, &sel, nullptr, nullptr);

    if (sel == 1001) {
        ShellExecuteW(nullptr, L"open", kWebsiteUrl, nullptr, nullptr, SW_SHOWNORMAL);
    }
    if (ovr == L"no") {                                // 服务端要求强制退出
        PostMessageW(hDlg, WM_CLOSE, 0, 0);
    }
}

static INT_PTR CALLBACK DlgProc(HWND hDlg, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
    case WM_INITDIALOG: {
        g_hDlg = hDlg;

        // 窗口图标
        HICON hIcon = LoadIconW(GetModuleHandleW(nullptr), MAKEINTRESOURCEW(IDI_APP_ICON));
        SendMessageW(hDlg, WM_SETICON, ICON_BIG, (LPARAM)hIcon);
        SendMessageW(hDlg, WM_SETICON, ICON_SMALL, (LPARAM)hIcon);

        // 官网链接：蓝色下划线字体（保持正体，不斜体）
        HFONT linkFont = CreateFontW(-11, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                                     DEFAULT_CHARSET, OUT_DEFAULT_PRECIS,
                                     CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY,
                                     DEFAULT_PITCH, L"Microsoft YaHei UI");
        SetWindowFont(GetDlgItem(hDlg, IDC_LINK_WEBSITE), linkFont, TRUE);

        // 功能开关默认状态：防截屏默认开；置顶默认关；快捷键(CTRL+Q/CTRL+W)默认开
        CheckDlgButton(hDlg, IDC_CHK_ANTICAPTURE, BST_CHECKED);
        if (IsDlgButtonChecked(hDlg, IDC_CHK_ANTICAPTURE) == BST_CHECKED) {
            SetWindowCaptureProtect(hDlg, true);
            InstallCbtHook();
        }
        CheckDlgButton(hDlg, IDC_CHK_HOTKEY, BST_CHECKED);    // CTRL+Q 快捷键默认开启
        CheckDlgButton(hDlg, IDC_CHK_HOTKEY_W, BST_CHECKED);  // CTRL+W 置顶快捷键默认开启
        ToggleHotkey(hDlg, true);
        ToggleHotkeyW(hDlg, true);

        // 每 2 秒轮询进程/窗口状态
        SetTimer(hDlg, TIMER_PERIODIC, 2000, nullptr);

        std::thread(UpdateCheckThreadFunc).detach();
        return TRUE;
    }

    case WM_COMMAND: {
        int id = LOWORD(wParam);
        switch (id) {
        case IDC_BTN_NETWORK:   OnNetwork(); break;
        case IDC_BTN_USB:       OnUsb(); break;
        case IDC_BTN_RESTORE:   OnRestore(); break;
        case IDC_BTN_JIYU:      OnJiyu(); break;
        case IDC_BTN_KEYBOARD:  OnKeyboard(hDlg); break;
        case IDC_BTN_WINDOWIZE: ToggleBroadcastWindow(); break;
        case IDC_BTN_SUSPEND: {
            DWORD pid = GetProcessIDFromName(L"StudentMain.exe");
            if (pid) {
                if (ProcessIsSuspended(pid)) {
                    LogToDlg(ResumeProcess(pid) ? L"极域已恢复" : L"恢复失败");
                } else {
                    LogToDlg(SuspendProcess(pid) ? L"极域已挂起（画面冻结）" : L"挂起失败");
                }
            } else {
                LogToDlg(L"极域程序未运行");
            }
            break;
        }
        case IDC_BTN_BLACKSCREEN: ExitBlackScreen(); break;
        case IDC_CHK_TOPMOST:
            if (HIWORD(wParam) == BN_CLICKED) {
                bool on = IsDlgButtonChecked(hDlg, IDC_CHK_TOPMOST) == BST_CHECKED;
                if (on) {
                    g_topmostOn = true;
                    if (g_topmostThread.joinable()) g_topmostThread.join();
                    g_topmostThread = std::thread(TopmostLoop);
                    LogToDlg(L"置顶窗口已开启");
                } else {
                    g_topmostOn = false;
                    if (g_topmostThread.joinable()) g_topmostThread.join();
                    SetWindowPos(hDlg, HWND_NOTOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE);
                    LogToDlg(L"置顶窗口已关闭");
                }
            }
            break;
        case IDC_CHK_ANTICAPTURE:
            if (HIWORD(wParam) == BN_CLICKED) {
                bool on = IsDlgButtonChecked(hDlg, IDC_CHK_ANTICAPTURE) == BST_CHECKED;
                SetWindowCaptureProtect(hDlg, on);
                if (on) { InstallCbtHook(); LogToDlg(L"防止截屏已开启"); }
                else    { RemoveCbtHook(); LogToDlg(L"防止截屏已关闭"); }
            }
            break;
        case IDC_CHK_HOTKEY:
            if (HIWORD(wParam) == BN_CLICKED) {
                ToggleHotkey(hDlg, IsDlgButtonChecked(hDlg, IDC_CHK_HOTKEY) == BST_CHECKED);
            }
            break;
        case IDC_CHK_HOTKEY_W:
            if (HIWORD(wParam) == BN_CLICKED) {
                ToggleHotkeyW(hDlg, IsDlgButtonChecked(hDlg, IDC_CHK_HOTKEY_W) == BST_CHECKED);
            }
            break;
        case IDC_LINK_WEBSITE:
            if (HIWORD(wParam) == STN_CLICKED) {
                ShellExecuteW(nullptr, L"open", kWebsiteUrl, nullptr, nullptr, SW_SHOWNORMAL);
            }
            break;
        case IDCANCEL:
            // ESC 不再关闭程序：防止「退出黑屏」模拟全局 ESC 时误触发退出
            // （点击按钮后主对话框是前台窗口，全局 ESC 会命中自己）
            break;
        }
        return TRUE;
    }

    case WM_WINDOWPOSCHANGED: {
        // 事件驱动置顶：z-order 一被改变（被极域盖住）立即处理，不等轮询周期
        if (g_topmostOn.load()) {
            WINDOWPOS* wp = reinterpret_cast<WINDOWPOS*>(lParam);
            if (wp && !(wp->flags & SWP_NOZORDER) && hDlg == (HWND)wp->hwnd) {
                KeepTopmost(hDlg);
            }
        }
        break;
    }

    case WM_HOTKEY:
        if (wParam == HOTKEY_ID_WINDOWIZE) {
            ToggleBroadcastWindow();   // CTRL+Q
        } else if (wParam == HOTKEY_ID_TOPMOST) {
            SetTopmost(hDlg, !g_topmostOn.load());   // CTRL+W 切换置顶
        }
        return TRUE;

    case WM_TIMER:
        if (wParam == TIMER_PERIODIC) OnPeriodicCheck(hDlg);
        return TRUE;

    case WM_CTLCOLORSTATIC: {
        HDC hdc = (HDC)wParam;
        if ((HWND)lParam == GetDlgItem(hDlg, IDC_LINK_WEBSITE)) {
            SetTextColor(hdc, RGB(30, 144, 255));
            SetBkMode(hdc, TRANSPARENT);
            return (INT_PTR)GetSysColorBrush(COLOR_BTNFACE);
        }
        break;
    }

    case WM_APP_LOG: {
        wchar_t* p = reinterpret_cast<wchar_t*>(lParam);
        if (p) {
            AppendLog(hDlg, p);
            delete[] p;
        }
        return TRUE;
    }

    case WM_APP_WINDOWIZE:
        ToggleBroadcastWindow();
        return TRUE;

    case WM_APP_SHOW_UPDATE: {
        auto* info = reinterpret_cast<std::pair<std::wstring, std::wstring>*>(lParam);
        if (info) {
            ShowUpdateDialog(hDlg, info->first, info->second);
            delete info;
        }
        return TRUE;
    }

    case WM_CLOSE:
        DestroyWindow(hDlg);
        return TRUE;

    case WM_DESTROY:
        KillTimer(hDlg, TIMER_PERIODIC);
        UnregisterHotKey(hDlg, HOTKEY_ID_WINDOWIZE);
        UnregisterHotKey(hDlg, HOTKEY_ID_TOPMOST);
        g_keyboardLockOn = false;
        if (g_unlockThread.joinable()) g_unlockThread.join();
        g_topmostOn = false;
        if (g_topmostThread.joinable()) g_topmostThread.join();
        RemoveCbtHook();
        PostQuitMessage(0);
        return TRUE;
    }
    return FALSE;
}

// ---------------------------------------------------------------------------
// 入口
// ---------------------------------------------------------------------------
int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE, LPWSTR, int) {
    // 单实例运行
    CreateMutexW(nullptr, TRUE, L"JiyuToolBox_SingleInstance");
    if (GetLastError() == ERROR_ALREADY_EXISTS) {
        MessageBoxW(nullptr, L"极域工具箱已在运行！", L"提示",
                    MB_OK | MB_ICONINFORMATION);
        return 0;
    }

    InitializeCriticalSection(&g_topmostCs);
    DialogBoxParamW(hInstance, MAKEINTRESOURCEW(IDD_MAIN_DIALOG), nullptr, DlgProc, 0);
    return 0;
}
