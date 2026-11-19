#pragma once
#include "prx/libc/include/general/VabiMacros.hpp"
#include "SceTypes.hpp"
#include <cstdint>

using TraceEntry = void (APS5_VABI*)(const void*, std::int32_t);
using TraceExit = void (APS5_VABI*)(const void*, std::int32_t, const char*, VaList*);
using TraceData = void (APS5_VABI*)(const void*, std::int32_t, std::int32_t, const char*, VaList*);

extern "C" {
void APS5_VABI utrace_setFunctions_nid_postfix(const void* context, TraceEntry entry, TraceExit exit, TraceData data);
void APS5_VABI utrace_getFunctions_nid_postfix(const void** context, TraceEntry* entry, TraceExit* exit, TraceData* data);
void APS5_VABI utrace_setLevel_nid_postfix(std::int32_t level);
std::int32_t APS5_VABI utrace_getLevel_nid_postfix();
void APS5_VABI utrace_entry_nid_postfix(std::int32_t function);
void APS5_VABI utrace_exit_nid_postfix(std::int32_t function, std::int32_t returnType, ...);
void APS5_VABI utrace_data_nid_postfix(std::int32_t function, std::int32_t level, const char* format, ...);
std::int32_t APS5_VABI utrace_vformat_nid_postfix(char* output, std::int32_t capacity, std::int32_t indent, const char* format, VaList* arguments);
std::int32_t APS5_VABI utrace_format_nid_postfix(char* output, std::int32_t capacity, std::int32_t indent, const char* format, ...);
const char* APS5_VABI utrace_functionName_nid_postfix(std::int32_t function);
}
