#include <Windows.h>
#include <iostream>

class Injector
{
public:
	Injector(void);
	~Injector(void);

	bool Inject(const char* procName, const char* dllName);
	bool Inject(DWORD pID, char* dllName);
	BOOL IsProcessRunning(DWORD pid);
	BOOL IsProcessRunning(const char * procname);

private:
};
DWORD GetTargetThreadIDFromProcName(const char * ProcName);
