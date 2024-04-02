/*
** i_system.cpp
** Main startup code
**
**---------------------------------------------------------------------------
** Copyright 1999-2016 Randy Heit
** Copyright 2019-2020 Christoph Oelckers
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

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include <stdarg.h>
#include <fcntl.h>
#include <time.h>

#include <SDL.h>

#include "version.h"
#include "cmdlib.h"
#include "m_argv.h"
#include "i_sound.h"
#include "i_interface.h"
#include "v_font.h"
#include "c_cvars.h"
#include "palutil.h"
#include "st_start.h"
#include "printf.h"
#include "launcherwindow.h"

#ifndef NO_GTK
bool I_GtkAvailable ();
void I_ShowFatalError_Gtk(const char* errortext);
#elif defined(__APPLE__)
int I_PickIWad_Cocoa (WadStuff *wads, int numwads, bool showwin, int defaultiwad);
#endif

double PerfToSec, PerfToMillisec;
CVAR(Bool, con_printansi, true, CVAR_GLOBALCONFIG|CVAR_ARCHIVE);
CVAR(Bool, con_4bitansi, false, CVAR_GLOBALCONFIG|CVAR_ARCHIVE);

extern FStartupScreen *StartWindow;

void I_SetIWADInfo()
{
}

//
// I_Error
//


void I_ShowFatalError(const char *message)
{
#ifdef __APPLE__
	Mac_I_FatalError(message);
#elif defined __unix__
	Unix_I_FatalError(message);
#else
	// ???
#endif
}

bool PerfAvailable;

void CalculateCPUSpeed()
{
	PerfAvailable = false;
	PerfToMillisec = PerfToSec = 0.;
#ifdef __aarch64__
	// [MK] on aarch64 rather than having to calculate cpu speed, there is
	// already an independent frequency for the perf timer
	uint64_t frq;
	asm volatile("mrs %0, cntfrq_el0":"=r"(frq));
	PerfAvailable = true;
	PerfToSec = 1./frq;
	PerfToMillisec = PerfToSec*1000.;
#elif defined(__linux__)
	// [MK] read from perf values if we can
	struct perf_event_attr pe;
	memset(&pe,0,sizeof(struct perf_event_attr));
	pe.type = PERF_TYPE_HARDWARE;
	pe.size = sizeof(struct perf_event_attr);
	pe.config = PERF_COUNT_HW_INSTRUCTIONS;
	pe.disabled = 1;
	pe.exclude_kernel = 1;
	pe.exclude_hv = 1;
	int fd = syscall(__NR_perf_event_open, &pe, 0, -1, -1, 0);
	if (fd == -1)
	{
		return;
	}
	void *addr = mmap(nullptr, 4096, PROT_READ, MAP_SHARED, fd, 0);
	if (addr == nullptr)
	{
		close(fd);
		return;
	}
	struct perf_event_mmap_page *pc = (struct perf_event_mmap_page *)addr;
	if (pc->cap_user_time != 1)
	{
		close(fd);
		return;
	}
	double mhz = (1000LU << pc->time_shift) / (double)pc->time_mult;
	PerfAvailable = true;
	PerfToSec = .000001/mhz;
	PerfToMillisec = PerfToSec*1000.;
	if (!batchrun) Printf("CPU speed: %.0f MHz\n", mhz);
	close(fd);
#endif
}

void I_PrintStr(const char *cp)
{
	const char * srcp = cp;
	FString printData = "";

	fputs(printData.GetChars(),stdout);
}

extern int uwp_ChooseWad(WadStuff* wads, int numwads, int defaultiwad, int& autoloadflags);

int I_PickIWad (WadStuff *wads, int numwads, bool showwin, int defaultiwad, int& autoloadflags)
{
	if (!showwin)
	{
		return defaultiwad;
	}

	return uwp_ChooseWad(wads, numwads, defaultiwad, autoloadflags);
}

void I_PutInClipboard (const char *str)
{
	SDL_SetClipboardText(str);
}

FString I_GetFromClipboard (bool use_primary_selection)
{
	if(char *ret = SDL_GetClipboardText())
	{
		FString text(ret);
		SDL_free(ret);
		return text;
	}
	return "";
}

extern FString uwp_GetCWD();
 
FString I_GetCWD()
{
	return uwp_GetCWD();
}

//bool I_ChDir(const char* path)
//{
//	return chdir(path) == 0;
//}

extern unsigned int uwp_MakeRNGSeed();

// Return a random seed, preferably one with lots of entropy.
unsigned int I_MakeRNGSeed()
{
	return uwp_MakeRNGSeed();
}

//void I_OpenShellFolder(const char* infolder)
//{
//	char* curdir = getcwd(NULL,0);
//
//	if (!chdir(infolder))
//	{
//		Printf("Opening folder: %s\n", infolder);
//		std::system("xdg-open .");
//		chdir(curdir);
//	}
//	else
//	{
//		Printf("Unable to open directory '%s\n", infolder);
//	}
//	free(curdir);
//}

