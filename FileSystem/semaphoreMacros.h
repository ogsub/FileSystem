#pragma once

#include <Windows.h>

#define mySignal(x) ReleaseSemaphore(x,1,NULL)
#define myWait(x) WaitForSingleObject(x,INFINITE)