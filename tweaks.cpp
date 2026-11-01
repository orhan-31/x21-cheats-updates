#include "tweaks.h"
#include <windows.h>
#include <tlhelp32.h>
#include <powrprof.h>
#include <winsvc.h>
#include <windns.h>
#include <filesystem>
#include <system_error>
#include <vector>
#include <string>
#include <algorithm>
#include <cstring>
#include <cwchar>
#include <sstream>
#pragma comment(lib, "PowrProf.lib")
#pragma comment(lib, "Dnsapi.lib")
namespace fs = std::filesystem;
namespace {
struct RegValue { HKEY root; std::wstring key, name; DWORD type=0; std::vector<BYTE> data; bool existed=false; };
std::vector<RegValue> saved;
GUID previousPower{}; bool havePreviousPower=false;
DWORD previousTelemetryStart=SERVICE_DEMAND_START; bool telemetryWasRunning=false, haveTelemetryState=false;
DWORD previousPriority=NORMAL_PRIORITY_CLASS; DWORD emulatorPid=0; std::wstring emulatorProcess;
bool freeFireRunning=false;
std::wstring gpuExe; std::wstring oldGpu; bool hadGpu=false;
const GUID HighPerformance={0x8c5e7fda,0xe8bf,0x4a96,{0x9a,0x85,0xa6,0xe2,0x3a,0x8c,0x63,0x5c}};

bool SetDword(HKEY root,const wchar_t* key,const wchar_t* name,DWORD value) {
    HKEY h{}; if(RegCreateKeyExW(root,key,0,nullptr,0,KEY_SET_VALUE,nullptr,&h,nullptr)!=ERROR_SUCCESS) return false;
    auto e=RegSetValueExW(h,name,0,REG_DWORD,reinterpret_cast<const BYTE*>(&value),sizeof(value)); RegCloseKey(h); return e==ERROR_SUCCESS;
}
void SaveDword(HKEY root,const wchar_t* key,const wchar_t* name) {
    RegValue v{root,key,name}; HKEY h{}; DWORD size=0;
    if(RegOpenKeyExW(root,key,0,KEY_QUERY_VALUE,&h)==ERROR_SUCCESS) {
        if(RegQueryValueExW(h,name,nullptr,&v.type,nullptr,&size)==ERROR_SUCCESS && size>0) {
            v.data.resize(size);
            if(RegQueryValueExW(h,name,nullptr,&v.type,v.data.data(),&size)==ERROR_SUCCESS) v.existed=true; else v.data.clear();
        }
        RegCloseKey(h);
    }
    saved.push_back(std::move(v));
}
void RestoreSaved(const wchar_t* prefix) {
    for(auto it=saved.rbegin();it!=saved.rend();++it) {
        if(it->key.rfind(prefix,0)!=0) continue;
        HKEY h{}; if(RegCreateKeyExW(it->root,it->key.c_str(),0,nullptr,0,KEY_SET_VALUE,nullptr,&h,nullptr)==ERROR_SUCCESS) {
            if(it->existed) RegSetValueExW(h,it->name.c_str(),0,it->type,it->data.data(),static_cast<DWORD>(it->data.size()));
            else RegDeleteValueW(h,it->name.c_str()); RegCloseKey(h);
        }
    }
}
bool Run(const std::wstring& command) {
    STARTUPINFOW si{}; si.cb=sizeof(si); si.dwFlags=STARTF_USESHOWWINDOW; si.wShowWindow=SW_HIDE;
    PROCESS_INFORMATION pi{}; std::wstring mutableCommand=command;
    if(!CreateProcessW(nullptr,mutableCommand.data(),nullptr,nullptr,FALSE,CREATE_NO_WINDOW,nullptr,nullptr,&si,&pi)) return false;
    WaitForSingleObject(pi.hProcess,30000); DWORD code=1; GetExitCodeProcess(pi.hProcess,&code); CloseHandle(pi.hProcess);CloseHandle(pi.hThread);return code==0;
}
DWORD FindPlayerPid() {
    HANDLE snap=CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS,0); if(snap==INVALID_HANDLE_VALUE)return 0;
    PROCESSENTRY32W p{};p.dwSize=sizeof(p);DWORD found=0;emulatorProcess.clear();
    if(Process32FirstW(snap,&p)) do { if(_wcsicmp(p.szExeFile,L"HD-Player.exe")==0||_wcsicmp(p.szExeFile,L"dnplayer.exe")==0||_wcsicmp(p.szExeFile,L"LdVBoxHeadless.exe")==0){found=p.th32ProcessID;emulatorProcess=p.szExeFile;break;} } while(Process32NextW(snap,&p));
    CloseHandle(snap);return found;
}
std::wstring ProcessImagePath(DWORD pid){HANDLE p=OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION,FALSE,pid);if(!p)return {};wchar_t path[32768]{};DWORD n=(DWORD)std::size(path);BOOL ok=QueryFullProcessImageNameW(p,0,path,&n);CloseHandle(p);return ok?std::wstring(path,n):std::wstring{};}
std::wstring FindAdbPath(){std::wstring image=ProcessImagePath(emulatorPid);if(image.empty())return {};fs::path dir=fs::path(image).parent_path();const wchar_t* names[]={L"HD-Adb.exe",L"adb.exe",L"ldadb.exe"};for(auto n:names){fs::path p=dir/n;if(fs::exists(p))return p.wstring();}return {};}
bool RunCapture(const std::wstring& exe,const std::wstring& args,std::string& output){SECURITY_ATTRIBUTES sa{sizeof(sa),nullptr,TRUE};HANDLE readPipe{},writePipe{};if(!CreatePipe(&readPipe,&writePipe,&sa,0))return false;SetHandleInformation(readPipe,HANDLE_FLAG_INHERIT,0);STARTUPINFOW si{};si.cb=sizeof(si);si.dwFlags=STARTF_USESTDHANDLES|STARTF_USESHOWWINDOW;si.wShowWindow=SW_HIDE;si.hStdOutput=writePipe;si.hStdError=writePipe;si.hStdInput=GetStdHandle(STD_INPUT_HANDLE);PROCESS_INFORMATION pi{};std::wstring cmd=L"\""+exe+L"\" "+args;BOOL started=CreateProcessW(exe.c_str(),cmd.data(),nullptr,nullptr,TRUE,CREATE_NO_WINDOW,nullptr,nullptr,&si,&pi);CloseHandle(writePipe);if(!started){CloseHandle(readPipe);return false;}DWORD start=GetTickCount();bool finished=false;while(GetTickCount()-start<5000){DWORD available=0;if(PeekNamedPipe(readPipe,nullptr,0,nullptr,&available,nullptr)&&available){char buf[1024];DWORD got=0;if(ReadFile(readPipe,buf,std::min<DWORD>(available,sizeof(buf)),&got,nullptr)&&got)output.append(buf,got);}if(WaitForSingleObject(pi.hProcess,40)==WAIT_OBJECT_0){finished=true;break;}}DWORD available=0;if(PeekNamedPipe(readPipe,nullptr,0,nullptr,&available,nullptr)&&available){char buf[2048];DWORD got=0;if(ReadFile(readPipe,buf,std::min<DWORD>(available,sizeof(buf)),&got,nullptr)&&got)output.append(buf,got);}if(!finished)TerminateProcess(pi.hProcess,1);CloseHandle(pi.hProcess);CloseHandle(pi.hThread);CloseHandle(readPipe);return finished;}
bool DetectFreeFire(const std::wstring& adb){freeFireRunning=false;if(adb.empty())return false;std::string devices;if(!RunCapture(adb,L"devices",devices))return false;bool online=false;std::istringstream lines(devices);std::string line;while(std::getline(lines,line)){auto tab=line.find('\t');if(tab==std::string::npos||line.substr(tab+1,6)!="device")continue;online=true;std::string serial=line.substr(0,tab);for(const char* package:{"com.dts.freefireth","com.dts.freefiremax"}){std::string result;std::wstring args=L"-s "+std::wstring(serial.begin(),serial.end())+L" shell pidof "+std::wstring(package,package+strlen(package));if(RunCapture(adb,args,result)&&result.find_first_of("0123456789")!=std::string::npos){freeFireRunning=true;return true;}}}return online;}
bool SetTelemetry(bool disable) {
    SC_HANDLE scm=OpenSCManagerW(nullptr,nullptr,SC_MANAGER_CONNECT); if(!scm)return false;
    SC_HANDLE svc=OpenServiceW(scm,L"DiagTrack",SERVICE_QUERY_STATUS|SERVICE_QUERY_CONFIG|SERVICE_CHANGE_CONFIG|SERVICE_STOP|SERVICE_START);
    if(!svc){CloseServiceHandle(scm);return false;}
    bool ok=false;
    if(disable) {
        BYTE buf[1024]{};DWORD need=0;
        if(QueryServiceConfigW(svc,reinterpret_cast<LPQUERY_SERVICE_CONFIGW>(buf),sizeof(buf),&need)) {
            auto cfg=reinterpret_cast<LPQUERY_SERVICE_CONFIGW>(buf); previousTelemetryStart=cfg->dwStartType;
            SERVICE_STATUS_PROCESS st{};DWORD n=0; telemetryWasRunning=QueryServiceStatusEx(svc,SC_STATUS_PROCESS_INFO,reinterpret_cast<BYTE*>(&st),sizeof(st),&n)&&st.dwCurrentState==SERVICE_RUNNING; haveTelemetryState=true;
            SERVICE_STATUS status{}; if(telemetryWasRunning) ControlService(svc,SERVICE_CONTROL_STOP,&status);
            ok=ChangeServiceConfigW(svc,SERVICE_NO_CHANGE,SERVICE_DISABLED,SERVICE_NO_CHANGE,nullptr,nullptr,nullptr,nullptr,nullptr,nullptr,nullptr)!=FALSE;
        }
    } else if(haveTelemetryState) {
        ok=ChangeServiceConfigW(svc,SERVICE_NO_CHANGE,previousTelemetryStart,SERVICE_NO_CHANGE,nullptr,nullptr,nullptr,nullptr,nullptr,nullptr,nullptr)!=FALSE;
        if(ok&&telemetryWasRunning) ok=StartServiceW(svc,0,nullptr)!=FALSE;
        haveTelemetryState=false;
    }
    CloseServiceHandle(svc);CloseServiceHandle(scm);return ok;
}
std::wstring FindBlueStacks() {
    const wchar_t* keys[]={L"SOFTWARE\\BlueStacks_nxt",L"SOFTWARE\\BlueStacks_msi5",L"SOFTWARE\\BlueStacks"};
    for(auto k:keys){wchar_t dir[MAX_PATH]{};DWORD n=sizeof(dir);if(RegGetValueW(HKEY_LOCAL_MACHINE,k,L"InstallDir",RRF_RT_REG_SZ,nullptr,dir,&n)==ERROR_SUCCESS){fs::path p=fs::path(dir)/L"HD-Player.exe";if(fs::exists(p))return p.wstring();}}
    return {};
}
bool ReadString(HKEY root,const wchar_t* key,const wchar_t* name,std::wstring& value) {
    HKEY h{};if(RegOpenKeyExW(root,key,0,KEY_QUERY_VALUE,&h)!=ERROR_SUCCESS)return false;DWORD type=0,n=0;LONG e=RegQueryValueExW(h,name,nullptr,&type,nullptr,&n);if(e!=ERROR_SUCCESS||type!=REG_SZ){RegCloseKey(h);return false;}std::vector<wchar_t>b(n/sizeof(wchar_t)+1);e=RegQueryValueExW(h,name,nullptr,&type,reinterpret_cast<BYTE*>(b.data()),&n);RegCloseKey(h);if(e==ERROR_SUCCESS)value=b.data();return e==ERROR_SUCCESS;
}
bool SetString(HKEY root,const wchar_t* key,const wchar_t* name,const std::wstring& val){HKEY h{};if(RegCreateKeyExW(root,key,0,nullptr,0,KEY_SET_VALUE,nullptr,&h,nullptr)!=ERROR_SUCCESS)return false;LONG e=RegSetValueExW(h,name,0,REG_SZ,reinterpret_cast<const BYTE*>(val.c_str()),static_cast<DWORD>((val.size()+1)*sizeof(wchar_t)));RegCloseKey(h);return e==ERROR_SUCCESS;}
}
const std::vector<ModuleInfo>& Modules(){static const std::vector<ModuleInfo> m={
 {ModuleId::Dvr,L"Disable Game DVR & Xbox Captures",L"Turn off background gameplay recording.",true},
 {ModuleId::Telemetry,L"Disable Windows Telemetry",L"Pause the DiagTrack service; restore its prior state anytime.",true},
 {ModuleId::Ntfs,L"NTFS Filesystem Optimization",L"Reduce last-access timestamp writes on NTFS.",true},
 {ModuleId::Dns,L"Flush DNS Resolver Cache",L"Clear DNS lookups; this does not reduce in-game ping.",false},
 {ModuleId::Power,L"High Performance Power Profile",L"Temporarily select Windows High Performance plan.",true},
 {ModuleId::Temp,L"Clean Temporary & Cache Junk",L"Remove files under the current user's TEMP folder.",false},
 {ModuleId::Mouse,L"Mouse Delay Remover / Acceleration 0",L"Disable Windows pointer acceleration; reversible.",true},
 {ModuleId::Priority,L"Emulator Priority",L"Apply Above Normal only while a supported emulator is running.",true},
 {ModuleId::Gpu,L"Emulator GPU Preference",L"Save the GPU preference only while BlueStacks is running.",true},
 {ModuleId::GameMode,L"Windows Game Mode",L"Enable Windows Game Mode for the current user.",true},
 {ModuleId::EmulatorGuide,L"Emulator & Free Fire Performance",L"Detect an online ADB emulator and Free Fire; use supported in-game FPS settings.",false},
 {ModuleId::InputGuide,L"Mouse Delay & Network Latency Guide",L"Honest guidance; no fake one-click latency tweak.",false}
};return m;}
std::wstring ScanEmulator(){emulatorPid=FindPlayerPid();freeFireRunning=false;if(!emulatorPid)return L"No running emulator found. Start BlueStacks, MSI App Player, or LDPlayer, then scan again.";std::wstring result=emulatorProcess+L" detected  |  PID: "+std::to_wstring(emulatorPid);std::wstring adb=FindAdbPath();if(adb.empty())return result+L"  |  ADB unavailable; game detection is off.";if(DetectFreeFire(adb))return result+(freeFireRunning?L"  |  Free Fire is running (ADB).":L"  |  ADB online; Free Fire is not running.");return result+L"  |  ADB device is not online; start the emulator fully and rescan.";}
std::string ApplyModule(ModuleId id){
 switch(id){
 case ModuleId::Dvr:{SaveDword(HKEY_CURRENT_USER,L"System\\GameConfigStore",L"GameDVR_Enabled");SaveDword(HKEY_CURRENT_USER,L"Software\\Microsoft\\Windows\\CurrentVersion\\GameDVR",L"AppCaptureEnabled");bool a=SetDword(HKEY_CURRENT_USER,L"System\\GameConfigStore",L"GameDVR_Enabled",0),b=SetDword(HKEY_CURRENT_USER,L"Software\\Microsoft\\Windows\\CurrentVersion\\GameDVR",L"AppCaptureEnabled",0);return a&&b?"Game DVR capture disabled.":"Could not update Game DVR settings.";}
 case ModuleId::Telemetry:return SetTelemetry(true)?"DiagTrack paused and set to disabled.":"Could not change DiagTrack; run elevated and retry.";
 case ModuleId::Ntfs:return Run(L"fsutil behavior set disablelastaccess 1")?"NTFS last-access updates disabled.":"fsutil failed. Requires an elevated session.";
 case ModuleId::Dns:return Run(L"ipconfig /flushdns")?"DNS resolver cache cleared.":"DNS cache could not be flushed.";
 case ModuleId::Power:{GUID* current=nullptr;if(PowerGetActiveScheme(nullptr,&current)!=ERROR_SUCCESS)return "Could not read current power plan.";previousPower=*current;havePreviousPower=true;LocalFree(current);return PowerSetActiveScheme(nullptr,&HighPerformance)==ERROR_SUCCESS?"High Performance plan selected.":"Could not select High Performance plan.";}
 case ModuleId::Temp:{wchar_t temp[MAX_PATH]{};GetTempPathW(MAX_PATH,temp);std::error_code ec;uint64_t count=0;for(fs::directory_iterator it(temp,fs::directory_options::skip_permission_denied,ec),end;it!=end&&!ec;it.increment(ec)){std::error_code e;fs::remove_all(it->path(),e);if(!e)++count;}return "Temporary entries removed: "+std::to_string(count)+". Files in use were skipped.";}
 case ModuleId::Mouse:{const wchar_t*k=L"Control Panel\\Mouse";SaveDword(HKEY_CURRENT_USER,k,L"MouseSpeed");SaveDword(HKEY_CURRENT_USER,k,L"MouseThreshold1");SaveDword(HKEY_CURRENT_USER,k,L"MouseThreshold2");bool a=SetString(HKEY_CURRENT_USER,k,L"MouseSpeed",L"0"),b=SetString(HKEY_CURRENT_USER,k,L"MouseThreshold1",L"0"),c=SetString(HKEY_CURRENT_USER,k,L"MouseThreshold2",L"0");return a&&b&&c?"Mouse acceleration disabled. Sign out or reopen Settings for full effect.":"Could not update mouse settings.";}
 case ModuleId::Priority:{emulatorPid=FindPlayerPid();if(!emulatorPid)return "Open a supported emulator first, then Scan.";HANDLE p=OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION|PROCESS_SET_INFORMATION,FALSE,emulatorPid);if(!p)return "Could not open emulator process.";previousPriority=GetPriorityClass(p);bool ok=SetPriorityClass(p,ABOVE_NORMAL_PRIORITY_CLASS)!=FALSE;CloseHandle(p);return ok?"Running emulator priority set to Above Normal.":"Could not change emulator priority.";}
 case ModuleId::Gpu:{emulatorPid=FindPlayerPid();if(!emulatorPid)return "Open BlueStacks first; no GPU preference was changed.";gpuExe=FindBlueStacks();if(gpuExe.empty())return "BlueStacks installation was not found.";oldGpu.clear();hadGpu=ReadString(HKEY_CURRENT_USER,L"Software\\Microsoft\\DirectX\\UserGpuPreferences",gpuExe.c_str(),oldGpu);return SetString(HKEY_CURRENT_USER,L"Software\\Microsoft\\DirectX\\UserGpuPreferences",gpuExe.c_str(),L"GpuPreference=2;")?"High-performance GPU preference saved. Restart BlueStacks to apply.":"Could not save GPU preference.";}
 case ModuleId::EmulatorGuide:return "Open the emulator settings guide in the application.";
 case ModuleId::InputGuide:return "Open the input and network latency guide in the application.";
 case ModuleId::GameMode:{SaveDword(HKEY_CURRENT_USER,L"Software\\Microsoft\\GameBar",L"AutoGameModeEnabled");return SetDword(HKEY_CURRENT_USER,L"Software\\Microsoft\\GameBar",L"AutoGameModeEnabled",1)?"Windows Game Mode enabled.":"Could not update Game Mode.";}
 default:return "Unknown module.";
 }
}
std::string UndoModule(ModuleId id){switch(id){
 case ModuleId::Dvr:RestoreSaved(L"System\\GameConfigStore");RestoreSaved(L"Software\\Microsoft\\Windows\\CurrentVersion\\GameDVR");return "Previous Game DVR values restored.";
 case ModuleId::Telemetry:return SetTelemetry(false)?"DiagTrack restored to its previous startup state.":"Could not restore DiagTrack.";
 case ModuleId::Ntfs:return Run(L"fsutil behavior set disablelastaccess 2")?"NTFS last-access behavior returned to system default.":"Could not restore NTFS behavior.";
 case ModuleId::Power:return havePreviousPower&&PowerSetActiveScheme(nullptr,&previousPower)==ERROR_SUCCESS?"Previous power plan restored.":"No saved power plan to restore.";
 case ModuleId::Mouse:RestoreSaved(L"Control Panel\\Mouse");return "Previous mouse acceleration values restored.";
 case ModuleId::Priority:{if(!emulatorPid)return "Scan emulator before restoring priority.";HANDLE p=OpenProcess(PROCESS_SET_INFORMATION,FALSE,emulatorPid);if(!p)return "Could not open emulator process.";bool ok=SetPriorityClass(p,previousPriority)!=FALSE;CloseHandle(p);return ok?"Previous emulator priority restored.":"Could not restore process priority.";}
 case ModuleId::Gpu:if(gpuExe.empty())gpuExe=FindBlueStacks();if(gpuExe.empty())return "BlueStacks installation was not found.";if(hadGpu)return SetString(HKEY_CURRENT_USER,L"Software\\Microsoft\\DirectX\\UserGpuPreferences",gpuExe.c_str(),oldGpu)?"Previous GPU preference restored.":"Could not restore GPU preference.";else {HKEY h{};if(RegOpenKeyExW(HKEY_CURRENT_USER,L"Software\\Microsoft\\DirectX\\UserGpuPreferences",0,KEY_SET_VALUE,&h)==ERROR_SUCCESS){RegDeleteValueW(h,gpuExe.c_str());RegCloseKey(h);}return "GPU preference reset.";}
 case ModuleId::EmulatorGuide:return "This guide does not make system changes.";
 case ModuleId::InputGuide:return "This guide does not make system changes.";
 case ModuleId::GameMode:RestoreSaved(L"Software\\Microsoft\\GameBar");return "Previous Game Mode setting restored.";
 default:return "This action does not have an undo operation.";
}}




