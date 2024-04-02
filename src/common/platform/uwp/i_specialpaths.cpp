/*
** i_specialpaths.cpp
** Gets special system folders where data should be stored. (Windows version)
**
**---------------------------------------------------------------------------
** Copyright 2013-2016 Randy Heit
** Copyright 2016 Christoph Oelckers
** All rights reserved.
**
** Redistribution and use in source and binary forms, with or without
** modification, are permitted provided that the following conditions
** are met:
**
** 1. Redistributions of source code must retain the above copyright
**    notice, this list of conditions and the following disclaimer.
** 2. Redistributions in binary form must reproduce the above copyright
**    notice, this list of conditions and the following disclaimer in the
**    documentation and/or other materials provided with the distribution.
** 3. The name of the author may not be used to endorse or promote products
**    derived from this software without specific prior written permission.
**
** THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
** IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
** OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
** IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
** INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
** NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
** DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
** THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
** (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
** THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
**---------------------------------------------------------------------------
**
*/

#include <windows.h>
#include <lmcons.h>
#include <shlobj.h>
#include <Shlwapi.h>
#include <VersionHelpers.h>

#include "i_specialpaths.h"
#include "printf.h"
#include "cmdlib.h"
#include "findfile.h"
#include "version.h"	// for GAMENAME
#include "gstrings.h"
#include "engineerrors.h"

//===========================================================================
//
// M_GetAppDataPath													Windows
//
// Returns the path for the AppData folder.
//
//===========================================================================

extern FString uwp_GetAppDataPath();

FString M_GetAppDataPath(bool create)
{
	FString path = uwp_GetAppDataPath(); // GetKnownFolder(CSIDL_LOCAL_APPDATA, FOLDERID_LocalAppData, create);

	path += "/" GAMENAMELOWERCASE;
	if (create)
	{
		CreatePath(path.GetChars());
	}
	return path;
}

//===========================================================================
//
// M_GetCachePath													Windows
//
// Returns the path for cache GL nodes.
//
//===========================================================================

FString M_GetCachePath(bool create)
{
	FString path = uwp_GetAppDataPath(); //= GetKnownFolder(CSIDL_LOCAL_APPDATA, FOLDERID_LocalAppData, create);

	// Don't use GAME_DIR and such so that ZDoom and its child ports can
	// share the node cache.
	path += "/zdoom/cache";
	if (create)
	{
		CreatePath(path.GetChars());
	}
	return path;
}

//===========================================================================
//
// M_GetAutoexecPath												Windows
//
// Returns the expected location of autoexec.cfg.
//
//===========================================================================

FString M_GetAutoexecPath()
{
	return "autoexec.cfg";
}

//===========================================================================
//
// M_GetConfigPath													Windows
//
// Returns the path to the config file. On Windows, this can vary for reading
// vs writing. i.e. If the user specific ini does not exist, it will try
// to read from a neutral version, but never write to it.
//
//===========================================================================

FString M_GetConfigPath(bool for_reading)
{
	//if (IsPortable())
	//{
		return FStringf("%s" GAMENAMELOWERCASE "_portable.ini", progdir.GetChars());
	//}
}

//===========================================================================
//
// M_GetScreenshotsPath												Windows
//
// Returns the path to the default screenshots directory.
//
//===========================================================================

FString M_GetScreenshotsPath()
{
	FString path;

	//if (IsPortable())
	//{
		path << progdir << "Screenshots/";
	//}
	return path;
}

//===========================================================================
//
// M_GetSavegamesPath												Windows
//
// Returns the path to the default save games directory.
//
//===========================================================================

FString M_GetSavegamesPath()
{
	FString path;

	//if (IsPortable())
	//{
		path << progdir << "Save/";
	//}
	return path;
}

//===========================================================================
//
// M_GetDocumentsPath												Windows
//
// Returns the path to the default documents directory.
//
//===========================================================================

FString M_GetDocumentsPath()
{
	FString path;

	//if (IsPortable())
	//{
		return progdir;
	//}
}

//===========================================================================
//
// M_GetDemoPath												Windows
//
// Returns the path to the default demp directory.
//
//===========================================================================

FString M_GetDemoPath()
{
	FString path;

	// A portable INI means that this storage location should also be portable.
	//if (IsPortable())
	//{
		path << progdir << "Demos/";
	//}
	return path;
}

//===========================================================================
//
// M_NormalizedPath
//
// Normalizes the given path and returns the result.
//
//===========================================================================

FString M_GetNormalizedPath(const char* path)
{
	std::wstring wpath = WideString(path);
	wchar_t buffer[MAX_PATH];
	GetFullPathNameW(wpath.c_str(), MAX_PATH, buffer, nullptr);
	FString result(buffer);
	FixPathSeperator(result);
	return result;
}
