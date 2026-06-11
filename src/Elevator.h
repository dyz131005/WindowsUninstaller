#pragma once

#include <string>
#include <windows.h>

class Elevator {
public:
    Elevator();
    ~Elevator();
    
    bool setNSudoPath(const std::wstring& path);
    bool isNSudoAvailable() const;
    
    bool executeAsTrustedInstaller(const std::wstring& command, int& exitCode);
    bool executeAsTrustedInstaller(const std::wstring& command);
    
private:
    std::wstring m_nsudoPath;
    bool m_nsudoAvailable;
    
    bool checkNSudoExists();
    std::wstring buildNSudoCommand(const std::wstring& command) const;
};