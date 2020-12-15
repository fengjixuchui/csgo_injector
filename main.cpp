#include "inj.h"
#include "binary.h"
#include <Windows.h>
#include <iostream>
#include <ShlObj.h>
#include <shlwapi.h>
#include <TlHelp32.h>
#include "main.h"

#include <iostream>
using namespace std;

#pragma comment(lib, "Shlwapi.lib")

#define ERASE_ENTRY_POINT    TRUE

Injector load;
DWORD dwProcessId = 0;

DWORD WINAPI loadLibrary(LoaderData* loaderData)
{
    PIMAGE_BASE_RELOCATION relocation = (PIMAGE_BASE_RELOCATION)(loaderData->baseAddress + loaderData->relocVirtualAddress);
    DWORD delta = (DWORD)(loaderData->baseAddress - loaderData->imageBase);
    while (relocation->VirtualAddress) {
        PWORD relocationInfo = (PWORD)(relocation + 1);
        for (int i = 0, count = (relocation->SizeOfBlock - sizeof(IMAGE_BASE_RELOCATION)) / sizeof(WORD); i < count; i++)
            if (relocationInfo[i] >> 12 == IMAGE_REL_BASED_HIGHLOW)
                *(PDWORD)(loaderData->baseAddress + (relocation->VirtualAddress + (relocationInfo[i] & 0xFFF))) += delta;

        relocation = (PIMAGE_BASE_RELOCATION)((LPBYTE)relocation + relocation->SizeOfBlock);
    }

    PIMAGE_IMPORT_DESCRIPTOR importDirectory = (PIMAGE_IMPORT_DESCRIPTOR)(loaderData->baseAddress + loaderData->importVirtualAddress);

    while (importDirectory->Characteristics) {
        PIMAGE_THUNK_DATA originalFirstThunk = (PIMAGE_THUNK_DATA)(loaderData->baseAddress + importDirectory->OriginalFirstThunk);
        PIMAGE_THUNK_DATA firstThunk = (PIMAGE_THUNK_DATA)(loaderData->baseAddress + importDirectory->FirstThunk);

        HMODULE module = loaderData->loadLibraryA((LPCSTR)loaderData->baseAddress + importDirectory->Name);

        if (!module)
            return FALSE;

        while (originalFirstThunk->u1.AddressOfData) {
            DWORD Function = (DWORD)loaderData->getProcAddress(module, originalFirstThunk->u1.Ordinal & IMAGE_ORDINAL_FLAG ? (LPCSTR)(originalFirstThunk->u1.Ordinal & 0xFFFF) : ((PIMAGE_IMPORT_BY_NAME)((LPBYTE)loaderData->baseAddress + originalFirstThunk->u1.AddressOfData))->Name);

            if (!Function)
                return FALSE;

            firstThunk->u1.Function = Function;
            originalFirstThunk++;
            firstThunk++;
        }
        importDirectory++;
    }

    if (loaderData->addressOfEntryPoint) {
        DWORD result = ((DWORD(__stdcall*)(HMODULE, DWORD, LPVOID))
            (loaderData->baseAddress + loaderData->addressOfEntryPoint))
            ((HMODULE)loaderData->baseAddress, DLL_PROCESS_ATTACH, NULL);

#if ERASE_ENTRY_POINT
        loaderData->rtlZeroMemory(loaderData->baseAddress + loaderData->addressOfEntryPoint, 32);
#endif

        return result;
    }
    return TRUE;
}

VOID stub(VOID) { }

VOID waitOnModule(DWORD processId, PCWSTR moduleName)
{
    BOOL foundModule = FALSE;

    while (!foundModule) {
        HANDLE moduleSnapshot = INVALID_HANDLE_VALUE;

        while (moduleSnapshot == INVALID_HANDLE_VALUE)
            moduleSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE, processId);

        MODULEENTRY32W moduleEntry;
        moduleEntry.dwSize = sizeof(moduleEntry);

        if (Module32FirstW(moduleSnapshot, &moduleEntry)) {
            do {
                if (!lstrcmpiW(moduleEntry.szModule, moduleName)) {
                    foundModule = TRUE;
                    break;
                }
            } while (Module32NextW(moduleSnapshot, &moduleEntry));
        }
        CloseHandle(moduleSnapshot);
    }
}

VOID killAnySteamProcess()
{
    HANDLE processSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);

    PROCESSENTRY32W processEntry;
    processEntry.dwSize = sizeof(processEntry);

    if (Process32FirstW(processSnapshot, &processEntry)) {
        PCWSTR steamProcesses[] = { L"Steam.exe", L"SteamService.exe", L"steamwebhelper.exe" };
        do {
            for (INT i = 0; i < _countof(steamProcesses); i++) {
                if (!lstrcmpiW(processEntry.szExeFile, steamProcesses[i])) {
                    HANDLE processHandle = OpenProcess(PROCESS_TERMINATE, FALSE, processEntry.th32ProcessID);
                    if (processHandle) {
                        TerminateProcess(processHandle, 0);
                        CloseHandle(processHandle);
                    }
                }
            }
        } while (Process32NextW(processSnapshot, &processEntry));
    }
    CloseHandle(processSnapshot);
}

INT WINAPI Do()
{
    HKEY key = NULL;
    if (!RegOpenKeyExW(HKEY_CURRENT_USER, L"Software\\Valve\\Steam", 0, KEY_QUERY_VALUE, &key)) {
        WCHAR steamPath[MAX_PATH];
        steamPath[0] = L'"';
        DWORD steamPathSize = sizeof(steamPath) - sizeof(WCHAR);

        if (!RegQueryValueExW(key, L"SteamExe", NULL, NULL, (LPBYTE)(steamPath + 1), &steamPathSize)) {
            lstrcatW(steamPath, L"\"");
            lstrcatW(steamPath, PathGetArgsW(GetCommandLineW()));

            killAnySteamProcess();

            STARTUPINFOW info = { sizeof(info) };
            PROCESS_INFORMATION processInfo;

            if (CreateProcessW(NULL, steamPath, NULL, NULL, FALSE, 0, NULL, NULL, &info, &processInfo)) {
                waitOnModule(processInfo.dwProcessId, L"Steam.exe");
                SuspendThread(processInfo.hThread);

                PIMAGE_NT_HEADERS ntHeaders = (PIMAGE_NT_HEADERS)(binary + ((PIMAGE_DOS_HEADER)binary)->e_lfanew);

                PBYTE executableImage = (PBYTE)VirtualAllocEx(processInfo.hProcess, NULL, ntHeaders->OptionalHeader.SizeOfImage,
                    MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);

                PIMAGE_SECTION_HEADER sectionHeaders = (PIMAGE_SECTION_HEADER)(ntHeaders + 1);
                for (INT i = 0; i < ntHeaders->FileHeader.NumberOfSections; i++)
                    WriteProcessMemory(processInfo.hProcess, executableImage + sectionHeaders[i].VirtualAddress,
                        binary + sectionHeaders[i].PointerToRawData, sectionHeaders[i].SizeOfRawData, NULL);
                
                LPVOID loaderMemory =  VirtualAllocEx(processInfo.hProcess, NULL, 4096, MEM_COMMIT | MEM_RESERVE,
                    PAGE_EXECUTE_READ);

                LoaderData loaderParams;
                loaderParams.baseAddress = executableImage;
                loaderParams.loadLibraryA = LoadLibraryA;
                loaderParams.getProcAddress = GetProcAddress;
                VOID(NTAPI RtlZeroMemory)(VOID * Destination, SIZE_T Length);
                //loaderParams.rtlZeroMemory = RtlZeroMemory;
                loaderParams.imageBase = ntHeaders->OptionalHeader.ImageBase;
                loaderParams.relocVirtualAddress = ntHeaders->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_BASERELOC].VirtualAddress;
                loaderParams.importVirtualAddress = ntHeaders->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT].VirtualAddress;
                loaderParams.addressOfEntryPoint = ntHeaders->OptionalHeader.AddressOfEntryPoint;

                WriteProcessMemory(processInfo.hProcess, loaderMemory, &loaderParams, sizeof(LoaderData),
                    NULL);
                WriteProcessMemory(processInfo.hProcess, loaderMemory , loadLibrary,
                    (DWORD)stub - (DWORD)loadLibrary, NULL);
                HANDLE thread = CreateRemoteThread(processInfo.hProcess, NULL, 0, (LPTHREAD_START_ROUTINE)(loaderMemory),
                    loaderMemory, 0, NULL);

                ResumeThread(processInfo.hThread);
                WaitForSingleObject(thread, INFINITE);
                VirtualFreeEx(processInfo.hProcess, loaderMemory, 0, MEM_RELEASE);

                CloseHandle(processInfo.hProcess);
                CloseHandle(processInfo.hThread);
            }
        }
        RegCloseKey(key);
    }
    return TRUE;
}

void ClearPE()
{
	char* pBaseAddr = (char*)GetModuleHandle(NULL);
	DWORD dwOldProtect = 0;
	VirtualProtect(pBaseAddr, 4096, PAGE_READWRITE, &dwOldProtect);
	ZeroMemory(pBaseAddr, 4096);
	VirtualProtect(pBaseAddr, 4096, dwOldProtect, &dwOldProtect);
}

void show(int i)
{
    const char a[] = "NO\0YES\0";
    cout << (a + i * 3);
}

int main()
{
    system("title insage.xyz - loader");
    system("color 3");
    char ch;
    cout << "enable vac bypass ? (y/n)\n";
    cin >> ch;
    if (ch == 'y')
    {
        while (!dwProcessId)
        {
            {
                Do();
                Sleep(1000);
                WinExec("C:\\Program Files (x86)\\Steam\\steam.exe", 1);
                printf("waiting for cs:go\n");
                dwProcessId = GetTargetThreadIDFromProcName("csgo.exe");
                Sleep(1000);
                ClearPE();
            }

            while (!(FindWindowA("Valve001", NULL)))
                Sleep(200);
            {
                load.Inject("csgo.exe", "overlay.dll");
                Sleep(200);
                printf("injected!");
                Sleep(1000);
                return 0;
            }
        }
    }
    else if (ch == 'n')
    {
        while (!dwProcessId)
        {
            {
                printf("waiting for cs:go\n");
                dwProcessId = GetTargetThreadIDFromProcName("csgo.exe");
                Sleep(1000);
                ClearPE();
            }

            while (!(FindWindowA("Valve001", NULL)))
                Sleep(200);
            {
                load.Inject("csgo.exe", "overlay.dll");
                Sleep(200);
                printf("injected!");
                Sleep(1000);
                return 0;
            }
        }
    }

}
