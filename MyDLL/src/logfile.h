#pragma once
#include <string>

// Global file path for shortcut.
extern std::string LogFilePath;

// Initialize log file path. Relative to module.
void LogInit(HMODULE hModule);

// Shortcut.
void AppendLog(std::string message);
// Open log file. Append message. Close log file.
void AppendLog(std::string filePath, std::string message);