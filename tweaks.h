#pragma once
#include <windows.h>
#include <string>
#include <vector>

enum class ModuleId { Dvr, Telemetry, Ntfs, Dns, Power, Temp, Mouse, Priority, Gpu, GameMode, EmulatorGuide, InputGuide, Count };
struct ModuleInfo { ModuleId id; const wchar_t* name; const wchar_t* detail; bool reversible; };
const std::vector<ModuleInfo>& Modules();
std::string ApplyModule(ModuleId id);
std::string UndoModule(ModuleId id);
std::wstring ScanEmulator();

