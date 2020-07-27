#include <iostream>
#include <fstream>
#include <stdio.h>
#include <thread>
#include <stdlib.h>
#include <time.h>
#include <sstream>
#include "inj.h"
#include <Windows.h>
#include <TlHelp32.h>
#define CREATE_THREAD_ACCESS (PROCESS_CREATE_THREAD | PROCESS_QUERY_INFORMATION | PROCESS_VM_OPERATION | PROCESS_VM_WRITE | PROCESS_VM_READ)

using namespace std;

Injector::Injector(void) {}
Injector::~Injector(void) {}

bool Injector::Inject(const char* procName, const char* dllName)
{
	DWORD pID = GetTargetThreadIDFromProcName(procName);

	char DLL_NAME[MAX_PATH] = { 0 };
	GetFullPathNameA(dllName, MAX_PATH, DLL_NAME, NULL);

	HANDLE Proc = 0;
	char buf[50] = { 0 };
	LPVOID RemoteString, LoadLibAddy;

	LPVOID ntOpenFile = GetProcAddress(LoadLibraryW(L"ntdll"), "NtOpenFile");
	if (ntOpenFile) {
		char originalBytes[5];
		memcpy(originalBytes, ntOpenFile, 5);
		WriteProcessMemory(Proc, ntOpenFile, originalBytes, 5, NULL);
		Beep(190, 300);
	}
	if (!pID)
		return false;

	Proc = OpenProcess(PROCESS_ALL_ACCESS, FALSE, pID);
	if (!Proc)
	{
		return false;
	}

	LoadLibAddy = (LPVOID)GetProcAddress(GetModuleHandleA("kernel32.dll"), "LoadLibraryA");
	RemoteString = (LPVOID)VirtualAllocEx(Proc, NULL, strlen(DLL_NAME), MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);

	WriteProcessMemory(Proc, (LPVOID)RemoteString, DLL_NAME, strlen(DLL_NAME), NULL);
	CreateRemoteThread(Proc, NULL, NULL, (LPTHREAD_START_ROUTINE)LoadLibAddy, (LPVOID)RemoteString, NULL, NULL);

	CloseHandle(Proc);

	return true;
}

bool Injector::Inject(DWORD pID, char* dllName)
{
	char DLL_NAME[MAX_PATH] = { 0 };
	GetFullPathNameA(dllName, MAX_PATH, DLL_NAME, NULL);

	HANDLE Proc = 0;
	char buf[50] = { 0 };
	LPVOID RemoteString, LoadLibAddy;

	LPVOID ntOpenFile = GetProcAddress(LoadLibraryW(L"ntdll"), "NtOpenFile");
	if (ntOpenFile) {
		char originalBytes[5];
		memcpy(originalBytes, ntOpenFile, 5);
		WriteProcessMemory(Proc, ntOpenFile, originalBytes, 5, NULL);
		Beep(190, 300);
	}
	if (!pID)
		return false;

	Proc = OpenProcess(PROCESS_ALL_ACCESS, FALSE, pID);
	if (!Proc)
	{
		return false;
	}

	LoadLibAddy = (LPVOID)GetProcAddress(GetModuleHandleA("kernel32.dll"), "LoadLibraryA");
	RemoteString = (LPVOID)VirtualAllocEx(Proc, NULL, strlen(DLL_NAME), MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);

	WriteProcessMemory(Proc, (LPVOID)RemoteString, DLL_NAME, strlen(DLL_NAME), NULL);
	CreateRemoteThread(Proc, NULL, NULL, (LPTHREAD_START_ROUTINE)LoadLibAddy, (LPVOID)RemoteString, NULL, NULL);

	CloseHandle(Proc);

	return true;
}

DWORD GetTargetThreadIDFromProcName(const char * ProcName)
{
	PROCESSENTRY32 pe;
	HANDLE thSnapShot;
	BOOL retval, ProcFound = false;

	thSnapShot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
	if (thSnapShot == INVALID_HANDLE_VALUE)
	{
		return false;
	}

	pe.dwSize = sizeof(PROCESSENTRY32);

	retval = Process32First(thSnapShot, &pe);
	while (retval)
	{
		char test[128];
		wcstombs(test, pe.szExeFile, sizeof(test));
		if (!strcmp(test, ProcName))
		{
			return pe.th32ProcessID;
		}
		retval = Process32Next(thSnapShot, &pe);
	}
	return 0;
}

BOOL Injector::IsProcessRunning(const char * procname) {
	HANDLE Proc = OpenProcess(SYNCHRONIZE, FALSE, GetTargetThreadIDFromProcName(procname));
	DWORD ret = WaitForSingleObject(Proc, 0);
	CloseHandle(Proc);
	return ret == WAIT_TIMEOUT;
}

BOOL Injector::IsProcessRunning(DWORD pid)
{
	HANDLE process = OpenProcess(SYNCHRONIZE, FALSE, pid);
	DWORD ret = WaitForSingleObject(process, 0);
	CloseHandle(process);
	return ret == WAIT_TIMEOUT;
}
