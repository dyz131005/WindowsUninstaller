#include "ScriptRunner.h"
#include "Elevator.h"
#include <algorithm>

ScriptRunner::ScriptRunner() {}

ScriptRunner::~ScriptRunner() {}

void ScriptRunner::setElevatorPath(const std::wstring& path) {
    m_elevatorPath = path;
}

bool ScriptRunner::addScript(const std::wstring& scriptPath) {
    std::filesystem::path path(scriptPath);
    if (std::filesystem::exists(path)) {
        m_scripts.push_back(path);
        return true;
    }
    return false;
}

bool ScriptRunner::addScriptsFromDirectory(const std::wstring& directory) {
    std::filesystem::path dirPath(directory);
    if (!std::filesystem::exists(dirPath) || !std::filesystem::is_directory(dirPath)) {
        return false;
    }
    
    bool addedAny = false;
    for (const auto& entry : std::filesystem::directory_iterator(dirPath)) {
        if (entry.is_regular_file()) {
            std::wstring extension = entry.path().extension().wstring();
            std::transform(extension.begin(), extension.end(), extension.begin(), ::towlower);
            
            if (extension == L".bat" || extension == L".cmd" || extension == L".ps1") {
                m_scripts.push_back(entry.path());
                addedAny = true;
            }
        }
    }
    
    return addedAny;
}

void ScriptRunner::clearScripts() {
    m_scripts.clear();
}

bool ScriptRunner::runScripts(bool useElevated) {
    if (m_scripts.empty()) {
        return false;
    }
    
    Elevator elevator;
    bool elevatorReady = false;
    
    if (useElevated) {
        elevatorReady = elevator.setNSudoPath(m_elevatorPath);
        if (!elevatorReady) {
            useElevated = false;
        }
    }
    
    bool allSuccess = true;
    for (const auto& script : m_scripts) {
        bool success = runScript(script.wstring(), useElevated && elevatorReady);
        if (!success) {
            allSuccess = false;
        }
    }
    
    return allSuccess;
}

bool ScriptRunner::runScript(const std::wstring& scriptPath, bool useElevated) {
    if (!fileExists(scriptPath)) {
        return false;
    }
    
    if (useElevated) {
        Elevator elevator;
        if (!elevator.setNSudoPath(m_elevatorPath)) {
            return false;
        }
        
        int exitCode;
        bool success = elevator.executeAsTrustedInstaller(scriptPath, exitCode);
        return success && exitCode == 0;
    } else {
        STARTUPINFOW si = { sizeof(si) };
        PROCESS_INFORMATION pi = { 0 };
        
        std::wstring command = L"cmd /c \"" + scriptPath + L"\"";
        
        bool success = CreateProcessW(
            nullptr,
            const_cast<wchar_t*>(command.c_str()),
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
        
        DWORD exitCode;
        GetExitCodeProcess(pi.hProcess, &exitCode);
        
        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);
        
        return exitCode == 0;
    }
}

bool ScriptRunner::fileExists(const std::wstring& path) const {
    return std::filesystem::exists(path);
}