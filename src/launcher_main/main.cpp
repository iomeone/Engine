//========= Copyright (C) 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: A redirection tool that allows the DLLs to reside elsewhere.
//
//=======================================================================================//

#if defined( _WIN32 )
#include <windows.h>
#include <stdio.h>
#include <assert.h>
#include <direct.h>
#elif defined(_POSIX)
#include <dlfcn.h>
#include <unistd.h>
#endif

#include "tier0/platform.h"
#include "tier4/MessageBox.h"

// Move this to a common include?
#ifdef _WIN32
#define BIN_DIR "bin/win32"
#elif defined(_WIN64)
#define BIN_DIR "bin/win64"
#elif defined(_POSIX) && defined(PLATFORM_32BITS)
#define BIN_DIR "bin/linux32"
#elif defined(_POSIX64)
#define BIN_DIR "bin/linux64"
#endif

typedef int (*LauncherMain_t)( void* hInstance, void* hPrevInstance, 
							  char* lpCmdLine, int nCmdShow );

//-----------------------------------------------------------------------------
// Purpose: Return the directory where this .exe is running from
// Output : char
//-----------------------------------------------------------------------------
static char *GetBaseDir( const char *pszBuffer )
{
	static char	basedir[ MAX_PATH ];
	char szBuffer[ MAX_PATH ];
	size_t j;
	char *pBuffer = NULL;

	strcpy( szBuffer, pszBuffer );

	pBuffer = strrchr( szBuffer,'\\' );
	if ( pBuffer )
	{
		*(pBuffer+1) = '\0';
	}

	strcpy( basedir, szBuffer );

	j = strlen( basedir );
	if (j > 0)
	{
		if ( ( basedir[ j-1 ] == '\\' ) || 
			 ( basedir[ j-1 ] == '/' ) )
		{
			basedir[ j-1 ] = 0;
		}
	}

	return basedir;
}

/*

Main entry point to the application

*/
int main(int argc, char** argv)
{
	// Load library
	char sFullPath[MAX_PATH];
	sprintf(sFullPath, "%s/launcher" DLL_EXT_STRING "", BIN_DIR);

	if(!Plat_FileExists(sFullPath))
	{
		printf("%s does not exist. Launching will fail\n", sFullPath);
	}

	void* pLib = Plat_LoadLibrary(sFullPath);

	if(!pLib)
	{
		printf("Unable to load launcher dynamic library.\nYou're most likely missing binaries.\nExpected path: %s\n", sFullPath);
		getcwd(sFullPath, MAX_PATH);
		printf("Current working directory: %s\n", sFullPath);
#ifdef _LINUX
		printf("LD_LIBRARY_PATH: %s\n", getenv("LD_LIBRARY_PATH"));
		printf("Last reported DL Error: %s\n", dlerror());
#endif
		Plat_ShowMessageBox("Fatal Error", "Could not load launcher\n", MB_TYPE_ERROR, MB_BUTTONS_OK);
		return 1;
	}

	LauncherMain_t pLauncherMain = (LauncherMain_t)Plat_FindProc(pLib, "LauncherMain");

	if(!pLauncherMain)
	{
		printf("Unable to find the entry point in the launcher DLL.\nYou might be using an old version of the launcher or something isn't compiled right.\n");
		Plat_ShowMessageBox("Fatal Error", "Unable to find entry point in launcher library!", MB_TYPE_ERROR, MB_BUTTONS_OK);
		return 1;
	}

	// form commandline
	char sCmdLine[1024];

	for(int i = 0; i < argc; i++)
		sprintf(sCmdLine, "%s %s", sCmdLine, argv[i]);

	pLauncherMain(NULL, NULL, sCmdLine, argc);
}

#ifdef _WINDOWS 
int APIENTRY WinMain( HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow )
{
	// Must add 'bin' to the path....
	char* pPath = getenv("PATH");

	// Use the .EXE name to determine the root directory
	char moduleName[ MAX_PATH ];
	char szBuffer[4096];
	if ( !GetModuleFileName( hInstance, moduleName, MAX_PATH ) )
	{
		MessageBox( 0, "Failed calling GetModuleFileName", "Launcher Error", MB_OK );
		return 0;
	}

	// Get the root directory the .exe is in
	char* pRootDir = GetBaseDir( moduleName );

#ifdef _DEBUG
	int len = 
#endif
	_snprintf( szBuffer, sizeof( szBuffer ), "PATH=%s\\;%s", pRootDir, pPath );
	szBuffer[sizeof( szBuffer ) - 1] = '\0';
	assert( len < sizeof( szBuffer ) );
	_putenv( szBuffer );

	// Assemble the full path to our "launcher.dll"
	_snprintf( szBuffer, sizeof( szBuffer ), "%s\\launcher.dll", pRootDir );
	szBuffer[sizeof( szBuffer ) - 1] = '\0';

	// STEAM OK ... filesystem not mounted yet
#if defined(_X360)
	HINSTANCE launcher = LoadLibrary( szBuffer );
#else
	HINSTANCE launcher = LoadLibraryEx( szBuffer, NULL, LOAD_WITH_ALTERED_SEARCH_PATH );
#endif
	if ( !launcher )
	{
		char *pszError;
		FormatMessage(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS, NULL, GetLastError(), MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), (LPTSTR)&pszError, 0, NULL);

		char szBuf[1024];
		_snprintf(szBuf, sizeof( szBuf ), "Failed to load the launcher DLL:\n\n%s", pszError);
		szBuf[sizeof( szBuf ) - 1] = '\0';
		MessageBox( 0, szBuf, "Launcher Error", MB_OK );

		LocalFree(pszError);
		return 0;
	}

	LauncherMain_t main = (LauncherMain_t)GetProcAddress( launcher, "LauncherMain" );
	int ret = main( hInstance, hPrevInstance, lpCmdLine, nCmdShow );
	FreeLibrary( launcher );
	return ret;
}
#endif