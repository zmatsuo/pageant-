#undef UNICODE

#include <windows.h>
#include <stdio.h>
#include <malloc.h>

#if 0
#include "cert_common.h"

#define DEFINE_VARIABLES
#include "cert_pkcs.h"
#undef DEFINE_VARIABLES

#ifndef SSH_AGENT_SUCCESS
#include "ssh.h"
#endif

// required to be defined by pkcs headers
#define CK_PTR *
#define NULL_PTR 0

// required to be defined by pkcs headers
#define CK_DECLARE_FUNCTION(returnType, name) \
	returnType __declspec(dllimport) name
#define CK_DECLARE_FUNCTION_POINTER(returnType, name) \
	returnType __declspec(dllimport) (* name)
#define CK_CALLBACK_FUNCTION(returnType, name) \
    returnType (* name)

// required to be defined by pkcs headers
#pragma pack(push, cryptoki, 1)
#include "pkcs\pkcs11.h"
#pragma pack(pop, cryptoki)
#endif

#include "pkcs_helper.h"
void test_wm_devicechange(void);

#define strdup(p)		_strdup(p)
#define	stricmp(p1,p2)	_stricmp(p1,p2)

typedef struct PROGRAM_ITEM
{
	struct PROGRAM_ITEM * NextItem;
	//LPSTR * Path;
	LPCSTR Path;
	HMODULE Library;
	CK_FUNCTION_LIST_PTR FunctionList;
	int Initialized;
} PROGRAM_ITEM;

static PROGRAM_ITEM * LibraryList = NULL;

CK_FUNCTION_LIST_PTR cert_pkcs_load_library(LPCSTR szLibrary)
{
	// see if module was already loaded
	for (PROGRAM_ITEM * hCurItem = LibraryList; hCurItem != NULL; hCurItem = hCurItem->NextItem)
	{
		if (stricmp(hCurItem->Path,szLibrary) != 0)
		{
			continue;
		}

		if (hCurItem->Initialized != 1)
		{
			CK_RV rv = hCurItem->FunctionList->C_Initialize(NULL_PTR);
			if (rv == CKR_OK || rv == CKR_CRYPTOKI_ALREADY_INITIALIZED)
			{
				hCurItem->Initialized = 1;
			}
		}
		return hCurItem->FunctionList;
	}

	// load the library and allow the loader to search the directory the dll is 
	// being loaded in and the system directory
	HMODULE hModule = LoadLibraryExA(szLibrary, 
		NULL, LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR | LOAD_LIBRARY_SEARCH_SYSTEM32);

	// validate library was loaded
	if (hModule == NULL)
	{
		LPCSTR szMessage = "PuTTY could not load the selected PKCS library. " \
			"Either the file is corrupted or not appropriate for this version " \
			"of PuTTY. Remember 32-bit PuTTY can only load 32-bit PKCS libraries and " \
			"64-bit PuTTY can only load 64-bit PKCS libraries.";
		MessageBox(NULL, szMessage, "PuTTY Could Not Load Library", MB_OK | MB_ICONERROR);
		return NULL;
	}

	// load the master function list for the librar
	CK_FUNCTION_LIST_PTR hFunctionList = NULL;
	CK_C_GetFunctionList C_GetFunctionList =
		(CK_C_GetFunctionList)GetProcAddress(hModule, "C_GetFunctionList");
	if (C_GetFunctionList == NULL || C_GetFunctionList(&hFunctionList) != CKR_OK)
	{
		// does not look like a valid PKCS library
		LPCSTR szMessage = "PuTTY was able to read the selected library file " \
			"but it does not appear to be a PKCS library.  It does not contain " \
			"the functions necessary to interface with PKCS.";
		MessageBox(NULL, szMessage, "PuTTY PKCS Library Problem", MB_OK | MB_ICONERROR);

		// error - cleanup and return
		FreeLibrary(hModule);
		return NULL;
	}

	// run the library initialization
	CK_LONG iLong = hFunctionList->C_Initialize(NULL_PTR);
	if (iLong != CKR_OK &&
		iLong != CKR_CRYPTOKI_ALREADY_INITIALIZED)
	{
		// error - cleanup and return
		FreeLibrary(hModule);
		return NULL;
	}

	// add the item to the linked list
	PROGRAM_ITEM * hItem = (PROGRAM_ITEM *)calloc(1, sizeof(struct PROGRAM_ITEM));
	hItem->Path = strdup(szLibrary);
	hItem->Library = hModule;
	hItem->FunctionList = hFunctionList;
	hItem->NextItem = LibraryList;
	hItem->Initialized = 1;
	LibraryList = hItem;
	return hItem->FunctionList;
}

void test_wm_devicechange(void)
{
	for (PROGRAM_ITEM * hCurItem = LibraryList; hCurItem != NULL; hCurItem = hCurItem->NextItem)
	{
		if (hCurItem->Initialized != 0)
		{
			hCurItem->FunctionList->C_Finalize(NULL_PTR);
			hCurItem->Initialized = 0;
		}
	}
}
