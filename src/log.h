#pragma once

#include <cstdarg>

void logf(const char* format, ...);
void logv(const char* format, va_list args);
void logm(const char* message);
