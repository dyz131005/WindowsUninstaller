#include "Logger.h"
#include <iostream>
#include <ctime>
#include <iomanip>

Logger& Logger::getInstance() {
    static Logger instance;
    return instance;
}

Logger::Logger() : m_logLevel(LogLevel::INFO) {}

Logger::~Logger() {
    if (m_logFile.is_open()) {
        m_logFile.close();
    }
}

void Logger::setLogLevel(LogLevel level) {
    m_logLevel = level;
}

void Logger::setLogFile(const std::string& filePath) {
    if (m_logFile.is_open()) {
        m_logFile.close();
    }
    m_logFile.open(filePath, std::ios::out | std::ios::app);
}

void Logger::debug(const std::string& message) {
    log(LogLevel::DEBUG, message);
}

void Logger::info(const std::string& message) {
    log(LogLevel::INFO, message);
}

void Logger::warning(const std::string& message) {
    log(LogLevel::WARNING, message);
}

void Logger::error(const std::string& message) {
    log(LogLevel::LOG_ERROR, message);
}

void Logger::log(LogLevel level, const std::string& message) {
    if (level < m_logLevel) {
        return;
    }
    
    std::lock_guard<std::mutex> lock(m_mutex);
    
    std::string logMessage = "[" + getTimestamp() + "] [" + getLevelString(level) + "] " + message;
    
    std::cout << logMessage << std::endl;
    
    if (m_logFile.is_open()) {
        m_logFile << logMessage << std::endl;
        m_logFile.flush();
    }
}

std::string Logger::getTimestamp() {
    std::time_t now = std::time(nullptr);
    std::tm localTime;
    localtime_s(&localTime, &now);
    
    std::ostringstream oss;
    oss << std::setw(4) << std::setfill('0') << localTime.tm_year + 1900 << "-"
        << std::setw(2) << std::setfill('0') << localTime.tm_mon + 1 << "-"
        << std::setw(2) << std::setfill('0') << localTime.tm_mday << " "
        << std::setw(2) << std::setfill('0') << localTime.tm_hour << ":"
        << std::setw(2) << std::setfill('0') << localTime.tm_min << ":"
        << std::setw(2) << std::setfill('0') << localTime.tm_sec;
    
    return oss.str();
}

std::string Logger::getLevelString(LogLevel level) {
    switch (level) {
        case LogLevel::DEBUG:    return "DEBUG";
        case LogLevel::INFO:     return "INFO";
        case LogLevel::WARNING:  return "WARNING";
        case LogLevel::LOG_ERROR:    return "ERROR";
        default:                 return "UNKNOWN";
    }
}