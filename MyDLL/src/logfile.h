#pragma once
#include <string>

// Global file path for shortcut.
extern std::string LogFilePath;

// Shortcut.
void AppendLog(std::string message);
void AppendLog(std::string filePath, std::string message);