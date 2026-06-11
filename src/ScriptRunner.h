#pragma once

#include <string>
#include <vector>
#include <filesystem>

class ScriptRunner {
public:
    ScriptRunner();
    ~ScriptRunner();
    
    void setElevatorPath(const std::wstring& path);
    bool addScript(const std::wstring& scriptPath);
    bool addScriptsFromDirectory(const std::wstring& directory);
    void clearScripts();
    
    bool runScripts(bool useElevated = true);
    bool runScript(const std::wstring& scriptPath, bool useElevated = true);
    
private:
    std::vector<std::filesystem::path> m_scripts;
    std::wstring m_elevatorPath;
    
    bool fileExists(const std::wstring& path) const;
};