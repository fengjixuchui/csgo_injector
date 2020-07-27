#include "inj.h"
#include <Windows.h>
#include <iostream>;

Injector load;
DWORD dwProcessId = 0;

typedef NTSTATUS(NTAPI* pfnNtSetInformationThread)(
	_In_ HANDLE ThreadHandle,
	_In_ ULONG  ThreadInformationClass,
	_In_ PVOID  ThreadInformation,
	_In_ ULONG  ThreadInformationLength
	);
const ULONG ThreadHideFromDebugger = 0x11;

void ClearPE()
{
	char* pBaseAddr = (char*)GetModuleHandle(NULL);
	DWORD dwOldProtect = 0;
	VirtualProtect(pBaseAddr, 4096, PAGE_READWRITE, &dwOldProtect);
	ZeroMemory(pBaseAddr, 4096);
	VirtualProtect(pBaseAddr, 4096, dwOldProtect, &dwOldProtect);
}

void HideFromDebugger()
{
	HMODULE hNtDll = LoadLibrary(TEXT("ntdll.dll"));
	pfnNtSetInformationThread NtSetInformationThread = (pfnNtSetInformationThread)
		GetProcAddress(hNtDll, "NtSetInformationThread");
	NTSTATUS status = NtSetInformationThread(GetCurrentThread(),
		ThreadHideFromDebugger, NULL, 0);
}

int main()
{
	system("title Loader @insage1337");
	system("color 2");
	while (!dwProcessId)
	{
		{
			printf("Waiting for cs:go.exe\n");
			dwProcessId = GetTargetThreadIDFromProcName("csgo.exe");
			Sleep(1000);
			HideFromDebugger();
			ClearPE();
		}

		while (!(FindWindowA("Valve001", NULL)))
			Sleep(200);
		{
			load.Inject("csgo.exe", "overlay.dll");
			Sleep(200);
			printf("injected.");
			Sleep(1000);
			return 0;
		}
	}
}