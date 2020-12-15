#pragma once

typedef struct {
	PBYTE baseAddress;
	HMODULE(WINAPI* loadLibraryA)(PCSTR);
	FARPROC(WINAPI* getProcAddress)(HMODULE, PCSTR);
	void(WINAPI* rtlZeroMemory)(PVOID, SIZE_T);

	DWORD imageBase;
	DWORD relocVirtualAddress;
	DWORD importVirtualAddress;
	DWORD addressOfEntryPoint;
} LoaderData;

