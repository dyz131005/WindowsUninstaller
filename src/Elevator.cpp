#include "Elevator.h"
#include "Logger.h"
#include <filesystem>

Elevator::Elevator() : m_nsudoAvailable(false) {}

Elevator::~Elevator() {}

bool Elevator::setNSudoPath(const std::wstring& path) {
    m_nsudoPath = path;
    return checkNSudoExists();
}

bool Elevator::isNSudoAvailable() const {
    return m_nsudoAvailable;
}

bool Elevator::executeAsTrustedInstaller(const std::wstring& command, int& exitCode) {
    if (!m_nsudoAvailable) {
        return false;
    }
    
    std::wstring nsudoCommand = buildNSudoCommand(command);
    
    STARTUPINFOW si = { sizeof(si) };
    PROCESS_INFORMATION pi = { 0 };
    
    si.dwFlags = STARTF_USESHOWWINDOW;
    si.wShowWindow = SW_HIDE;
    
    bool success = CreateProcessW(
        nullptr,
        const_cast<wchar_t*>(nsudoCommand.c_str()),
        nullptr,
        nullptr,
        FALSE,
        0,
        nullptr,
        nullptr,
        &si,
        &pi
    );
    
    if (!success) {
        return false;
    }
    
    WaitForSingleObject(pi.hProcess, INFINITE);
    
    DWORD exitCodeTemp;
    GetExitCodeProcess(pi.hProcess, &exitCodeTemp);
    exitCode = static_cast<int>(exitCodeTemp);
    
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);
    
    return true;
}

bool Elevator::executeAsTrustedInstaller(const std::wstring& command) {
    int exitCode;
    return executeAsTrustedInstaller(command, exitCode);
}

bool Elevator::checkNSudoExists() {
    if (m_nsudoPath.empty()) {
        m_nsudoAvailable = false;
        return false;
    }
    
    std::filesystem::path path(m_nsudoPath);
    if (std::filesystem::exists(path)) {
        m_nsudoAvailable = true;
        return true;
    } else {
        m_nsudoAvailable = false;
        return false;
    }
}

std::wstring Elevator::buildNSudoCommand(const std::wstring& command) const {
    return m_nsudoPath + L" -U:T -P:E cmd /c \"" + command + L"\"";
}