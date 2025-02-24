﻿// dllmain.cpp : 定义 DLL 应用程序的入口点。

#include "pch.h"
#include "detours.h"
#pragma comment(lib, "detours.lib")
#include <iostream>
#include <Psapi.h> // To get process info
#include <process.h>
#include <stdlib.h>

#define AttachRVA 0x205AF
DWORD BaseAddr = 0;
using namespace std;
int pic_count = 1;
char filename[100];
string path;


string get_global_path() {
	string strMapName("ShareMemory");                // 内存映射对象名称
	LPVOID pBuffer;                                    // 共享内存指针

	// 首先试图打开一个命名的内存映射文件对象
	HANDLE hMap = ::OpenFileMapping(FILE_MAP_ALL_ACCESS, 0, strMapName.c_str());
	// 打开成功，映射对象的一个视图，得到指向共享内存的指针，显示出里面的数据
	pBuffer = ::MapViewOfFile(hMap, FILE_MAP_ALL_ACCESS, 0, 0, 0);
	//cout << "读取共享内存数据：" << (char *)pBuffer << endl;
	return (char *)pBuffer;
}

void __stdcall SaveImage(void* pic_addr, unsigned int length)
{
	sprintf(filename, "%03d.jpg", pic_count);
	string tmp = path + "\\" + string(filename);
	FILE *fp = fopen(tmp.c_str(), "wb+");
	fwrite(pic_addr, length, 1, fp);
	fclose(fp);
	pic_count++;
	return;
}

void* attached_pointer = (void*)(0);
void __declspec(naked) save_image()
{
	__asm
	{
		pushad
		pushfd
		
		mov ebx, dword ptr[esi+68h]	 // jpg data
		mov	ecx, dword ptr[esi+64h]  // size
		push ecx
		push ebx
		call SaveImage
		
		popfd
		popad
		jmp attached_pointer
	}
}

void Attach() {
	DetourTransactionBegin();
	DetourUpdateThread(GetCurrentThread());
	DetourAttach(&attached_pointer, save_image);
	if (DetourTransactionCommit() != NO_ERROR)
	{
		MessageBoxW(NULL, L"Hook Error.", L"ImagePatch", MB_OK | MB_ICONERROR);
	}
}

DWORD GetBaseAddress(DWORD processID) {
	HANDLE hProcess(NULL);
	hProcess = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, processID);

	if (hProcess == NULL)
		return NULL; // No access to the process

	HMODULE lphModule[1024]; // Array that receives the list of module handles
	DWORD lpcbNeeded(NULL); // Output of EnumProcessModules, giving the number of bytes requires to store all modules handles in the lphModule array

	if (!EnumProcessModules(hProcess, lphModule, sizeof(lphModule), &lpcbNeeded))
		return NULL; // Impossible to read modules

	TCHAR szModName[MAX_PATH];
	if (!GetModuleFileNameEx(hProcess, lphModule[0], szModName, sizeof(szModName) / sizeof(TCHAR)))
		return NULL; // Impossible to get module info

	return (DWORD)lphModule[0]; // Module 0 is apparently always the EXE itself, returning its address
}


void Init() {
	int iPid = (int)_getpid();
	BaseAddr = GetBaseAddress(iPid);
	attached_pointer = (void*)(BaseAddr + AttachRVA);
	path = get_global_path();
}


BOOL APIENTRY DllMain( HMODULE hModule,
                       DWORD  ul_reason_for_call,
                       LPVOID lpReserved
                     )
{
    switch (ul_reason_for_call)
    {
    case DLL_PROCESS_ATTACH:
		Init();
		Attach();
    case DLL_THREAD_ATTACH:
    case DLL_THREAD_DETACH:
    case DLL_PROCESS_DETACH:
        break;
    }
    return TRUE;
}

