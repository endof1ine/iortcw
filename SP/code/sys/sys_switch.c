/*
===========================================================================
Return to Castle Wolfenstein single player GPL Source Code
Copyright (C) 1999-2010 id Software LLC, a ZeniMax Media company.

This file is part of the Return to Castle Wolfenstein single player GPL Source Code (RTCW SP Source Code).

RTCW SP Source Code is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

RTCW SP Source Code is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with RTCW SP Source Code.  If not, see <http://www.gnu.org/licenses/>.

===========================================================================
*/

/*
sys_switch.c: the Nintendo Switch's (libnx homebrew) in place of sys_unix.c.

The game's files live on the SD card in /switch/iortcw (the base path and
the home path both: main/ holds the pk3s, and the configs and saves go
beside them). There is no dynamic loading: the three game modules are linked
into the executable (the Makefile's SWITCH_MODULE_RULE), and
Sys_SwitchLoadGameModule hands out their entry points by name.
*/

#include "../qcommon/q_shared.h"
#include "../qcommon/qcommon.h"
#include "sys_local.h"

#include <switch.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <errno.h>
#include <stdio.h>
#include <dirent.h>
#include <unistd.h>
#include <fcntl.h>
#include <fenv.h>
#include <time.h>

#define SWITCH_GAME_PATH "/switch/iortcw"

/*
==================
Sys_DefaultHomePath
==================
*/
char *Sys_DefaultHomePath( void )
{
	return SWITCH_GAME_PATH;
}

#ifndef STANDALONE
char *Sys_SteamPath( void )
{
	return "";
}

char *Sys_GogPath( void )
{
	return "";
}
#endif

/*
================
Sys_Milliseconds
================
*/
int Sys_Milliseconds( void )
{
	static u64 base;
	u64 now = armTicksToNs( armGetSystemTick() ) / 1000000ULL;

	if ( !base )
		base = now;
	return (int)( now - base );
}

qboolean Sys_RandomBytes( byte *string, int len )
{
	csrngGetRandomBytes( string, len );
	return qtrue;
}

char *Sys_GetCurrentUser( void )
{
	return "player";
}

qboolean Sys_LowPhysicalMemory( void )
{
	return qfalse;
}

/* (newlib here has no libgen: POSIX's basename and dirname, which may
change path) */
const char *Sys_Basename( char *path )
{
	char *end = path + strlen( path );
	char *slash;

	if ( !*path )
		return ".";
	while ( end > path + 1 && end[-1] == '/' )
		*--end = '\0';
	slash = strrchr( path, '/' );
	return slash && slash[1] ? slash + 1 : path;
}

const char *Sys_Dirname( char *path )
{
	char *slash = strrchr( path, '/' );

	if ( !slash )
		return ".";
	while ( slash > path && slash[-1] == '/' )
		slash--;
	if ( slash == path )
		return "/";
	*slash = '\0';
	return path;
}

/* (newlib here has none, and the SD card's files have no permissions:
common.c's, around writing the key) */
mode_t umask( mode_t mask )
{
	return 0;
}

FILE *Sys_FOpen( const char *ospath, const char *mode )
{
	struct stat buf;
	FILE *f = fopen( ospath, mode );

	// (not a directory; checked after the open, as the engine looks for
	// many files that aren't there, each look a call to the file service)
	if ( f && !fstat( fileno( f ), &buf ) && S_ISDIR( buf.st_mode ) ) {
		fclose( f );
		return NULL;
	}
	return f;
}

qboolean Sys_Mkdir( const char *path )
{
	int result = mkdir( path, 0750 );

	if ( result != 0 )
		return errno == EEXIST;

	return qtrue;
}

FILE *Sys_Mkfifo( const char *ospath )
{
	return NULL;
}

char *Sys_Cwd( void )
{
	static char cwd[MAX_OSPATH];

	char *result = getcwd( cwd, sizeof( cwd ) - 1 );
	if ( result != cwd )
		return NULL;

	cwd[MAX_OSPATH-1] = 0;

	return cwd;
}

/*
==============================================================

DIRECTORY SCANNING (sys_unix.c's)

==============================================================
*/

#define MAX_FOUND_FILES 0x1000

void Sys_ListFilteredFiles( const char *basedir, char *subdirs, char *filter, char **list, int *numfiles )
{
	char          search[MAX_OSPATH], newsubdirs[MAX_OSPATH];
	char          filename[MAX_OSPATH];
	DIR           *fdir;
	struct dirent *d;
	struct stat   st;

	if ( *numfiles >= MAX_FOUND_FILES - 1 ) {
		return;
	}

	if ( basedir[0] == '\0' ) {
		return;
	}

	if ( strlen( subdirs ) ) {
		Com_sprintf( search, sizeof( search ), "%s/%s", basedir, subdirs );
	}
	else {
		Com_sprintf( search, sizeof( search ), "%s", basedir );
	}

	if ( ( fdir = opendir( search ) ) == NULL ) {
		return;
	}

	while ( ( d = readdir( fdir ) ) != NULL ) {
		Com_sprintf( filename, sizeof( filename ), "%s/%s", search, d->d_name );
		if ( stat( filename, &st ) == -1 )
			continue;

		if ( st.st_mode & S_IFDIR ) {
			if ( Q_stricmp( d->d_name, "." ) && Q_stricmp( d->d_name, ".." ) ) {
				if ( strlen( subdirs ) ) {
					Com_sprintf( newsubdirs, sizeof( newsubdirs ), "%s/%s", subdirs, d->d_name );
				}
				else {
					Com_sprintf( newsubdirs, sizeof( newsubdirs ), "%s", d->d_name );
				}
				Sys_ListFilteredFiles( basedir, newsubdirs, filter, list, numfiles );
			}
		}
		if ( *numfiles >= MAX_FOUND_FILES - 1 ) {
			break;
		}
		Com_sprintf( filename, sizeof( filename ), "%s/%s", subdirs, d->d_name );
		if ( !Com_FilterPath( filter, filename, qfalse ) )
			continue;
		list[ *numfiles ] = CopyString( filename );
		( *numfiles )++;
	}

	closedir( fdir );
}

char **Sys_ListFiles( const char *directory, const char *extension, char *filter, int *numfiles, qboolean wantsubs )
{
	struct dirent *d;
	DIR           *fdir;
	qboolean      dironly = wantsubs;
	char          search[MAX_OSPATH];
	int           nfiles;
	char          **listCopy;
	char          *list[MAX_FOUND_FILES];
	int           i;
	struct stat   st;
	int           extLen;

	if ( filter ) {
		nfiles = 0;
		Sys_ListFilteredFiles( directory, "", filter, list, &nfiles );

		list[ nfiles ] = NULL;
		*numfiles = nfiles;

		if ( !nfiles )
			return NULL;

		listCopy = Z_Malloc( ( nfiles + 1 ) * sizeof( *listCopy ) );
		for ( i = 0 ; i < nfiles ; i++ ) {
			listCopy[i] = list[i];
		}
		listCopy[i] = NULL;

		return listCopy;
	}

	if ( directory[0] == '\0' ) {
		*numfiles = 0;
		return NULL;
	}

	if ( !extension )
		extension = "";

	if ( extension[0] == '/' && extension[1] == 0 ) {
		extension = "";
		dironly = qtrue;
	}

	extLen = strlen( extension );

	nfiles = 0;

	if ( ( fdir = opendir( directory ) ) == NULL ) {
		*numfiles = 0;
		return NULL;
	}

	while ( ( d = readdir( fdir ) ) != NULL ) {
		// (the name first: a stat is a call to the file service)
		if ( *extension ) {
			if ( strlen( d->d_name ) < extLen ||
				Q_stricmp(
					d->d_name + strlen( d->d_name ) - extLen,
					extension ) ) {
				continue; // didn't match
			}
		}

		Com_sprintf( search, sizeof( search ), "%s/%s", directory, d->d_name );
		if ( stat( search, &st ) == -1 )
			continue;
		if ( ( dironly && !( st.st_mode & S_IFDIR ) ) ||
			( !dironly && ( st.st_mode & S_IFDIR ) ) )
			continue;

		if ( nfiles == MAX_FOUND_FILES - 1 )
			break;
		list[ nfiles ] = CopyString( d->d_name );
		nfiles++;
	}

	list[ nfiles ] = NULL;

	closedir( fdir );

	*numfiles = nfiles;

	if ( !nfiles ) {
		return NULL;
	}

	listCopy = Z_Malloc( ( nfiles + 1 ) * sizeof( *listCopy ) );
	for ( i = 0 ; i < nfiles ; i++ ) {
		listCopy[i] = list[i];
	}
	listCopy[i] = NULL;

	return listCopy;
}

void Sys_FreeFileList( char **list )
{
	int i;

	if ( !list ) {
		return;
	}

	for ( i = 0 ; list[i] ; i++ ) {
		Z_Free( list[i] );
	}

	Z_Free( list );
}

void Sys_Sleep( int msec )
{
	if ( msec == 0 )
		return;

	// (with no console to wait on, a while)
	if ( msec < 0 )
		msec = 10;

	svcSleepThread( (s64)msec * 1000000LL );
}

/*
==============
Sys_ErrorDialog

The error in the log (fs_homepath's crashlog.txt, as sys_unix.c writes it)
==============
*/
void Sys_ErrorDialog( const char *error )
{
	char buffer[ 1024 ];
	unsigned int size;
	FILE *f;
	const char *homepath = Cvar_VariableString( "fs_homepath" );
	const char *gamedir = Cvar_VariableString( "fs_game" );
	char *ospath = FS_BuildOSPath( homepath, gamedir, "crashlog.txt" );

	Sys_Print( va( "%s\n", error ) );

	f = fopen( ospath, "wb" );
	if ( !f )
		return;
	while ( ( size = CON_LogRead( buffer, sizeof( buffer ) ) ) > 0 ) {
		if ( fwrite( buffer, 1, size, f ) != size )
			break;
	}
	fclose( f );
}

/*
==============
Sys_Dialog

No dialogs: the answer that leaves things as they are (no to "start with
safe video settings?" after a crash, which would draw at 640x480)
==============
*/
dialogResult_t Sys_Dialog( dialogType_t type, const char *message, const char *title )
{
	Com_Printf( "%s: %s\n", title, message );
	switch ( type )
	{
		case DT_YES_NO:    return DR_NO;
		case DT_OK_CANCEL: return DR_OK;
		default:           return DR_OK;
	}
}

void Sys_GLimpSafeInit( void )
{
}

void Sys_GLimpInit( void )
{
}

void Sys_SetFloatEnv( void )
{
	// rounding toward nearest
	fesetround( FE_TONEAREST );
}

void Sys_PlatformInit( void )
{
	// (internet and LAN play's sockets; then, started by the Homebrew Menu's
	// netloader, the console's output to it, which needs them)
	socketInitializeDefault();
	nxlinkStdio();
	setvbuf( stdout, NULL, _IONBF, 0 );
	// (the port's own game files inside the program, files.c's romfs:)
	romfsInit();
	Sys_SwitchCpuBoost( qtrue );
	Sys_SetFloatEnv();
}

void Sys_PlatformExit( void )
{
	socketExit();
}

void Sys_SetEnv( const char *name, const char *value )
{
	if ( value && *value )
		setenv( name, value, 1 );
	else
		unsetenv( name );
}

int Sys_PID( void )
{
	return 1;
}

qboolean Sys_PIDIsRunning( int pid )
{
	return qfalse;
}

qboolean Sys_DllExtension( const char *name )
{
	return COM_CompareExtension( name, DLL_EXT );
}

char *Sys_GetDLLName( const char *name )
{
	return va( "%s.sp." ARCH_STRING DLL_EXT, name );
}

int Sys_GetHighQualityCPU( void )
{
	return 1;
}

void Sys_StartProcess( char *cmdline, qboolean doexit )
{
	Com_DPrintf( "Sys_StartProcess %s: not on the Switch\n", cmdline );
}

void Sys_OpenURL( char *url, qboolean doexit )
{
	Com_Printf( "Sys_OpenURL %s: not on the Switch\n", url );
}

/*
==============
Sys_SwitchDocked

1 when the Switch is docked (a television of 1080 lines), 0 in handheld
mode (the screen's 720)
==============
*/
int Sys_SwitchDocked( void )
{
	return appletGetOperationMode() == AppletOperationMode_Console;
}

/*
==============
Sys_SwitchCpuBoost

The CPU at its fastest (1785 MHz; the GPU slowed meanwhile) while loading:
at start-up until the main menu, and from a level's loading (CL_MapLoading)
until it runs (CL_Frame). Each load's time logged
==============
*/
void Sys_SwitchCpuBoost( qboolean on )
{
	static qboolean boosted;
	static int since;

	if ( on == boosted ) {
		return;
	}
	appletSetCpuBoostMode( on ? ApmCpuBoostMode_FastLoad : ApmCpuBoostMode_Normal );
	boosted = on;
	if ( on ) {
		since = Sys_Milliseconds( );
	} else {
		Com_Printf( "loaded in %d ms (CPU boosted)\n", Sys_Milliseconds( ) - since );
	}
}

/*
==============
Sys_SwitchRumble

The controller's motors, low (160 Hz) and high (320 Hz) from 0 to 1, until
the next call: HD rumble straight from libnx, SDL's Switch rumble being
broken (no handles for the first controller, amplitudes out of range).
Handheld and the first controller both, whichever is connected
==============
*/
void Sys_SwitchRumble( float low, float high )
{
	static const HidNpadIdType ids[2] = { HidNpadIdType_Handheld, HidNpadIdType_No1 };
	static HidVibrationDeviceHandle handles[2][2];
	static u32 styles[2];
	static s32 counts[2];
	HidVibrationValue values[2];
	int i;

	values[0].amp_low = low;
	values[0].freq_low = 160.0f;
	values[0].amp_high = high;
	values[0].freq_high = 320.0f;
	values[1] = values[0];
	for ( i = 0; i < 2; i++ ) {
		u32 style = hidGetNpadStyleSet( ids[i] );

		if ( !style ) {
			continue;
		}
		if ( style != styles[i] ) {
			// (two motors, one per side; one on a single Joy-Con)
			counts[i] = 2;
			if ( R_FAILED( hidInitializeVibrationDevices( handles[i], 2, ids[i], style ) ) ) {
				counts[i] = 1;
				if ( R_FAILED( hidInitializeVibrationDevices( handles[i], 1, ids[i], style ) ) ) {
					continue;
				}
			}
			styles[i] = style;
		}
		hidSendVibrationValues( handles[i], values, counts[i] );
	}
}

/*
==============================================================

THE GAME MODULES, LINKED IN

==============================================================
*/

typedef void ( *dllEntryProc )( intptr_t ( *syscallptr )( intptr_t, ... ) );

void cgame_dllEntry( intptr_t ( *syscallptr )( intptr_t, ... ) );
intptr_t cgame_vmMain( intptr_t command, ... );
void qagame_dllEntry( intptr_t ( *syscallptr )( intptr_t, ... ) );
intptr_t qagame_vmMain( intptr_t command, ... );
void ui_dllEntry( intptr_t ( *syscallptr )( intptr_t, ... ) );
intptr_t ui_vmMain( intptr_t command, ... );

static const struct
{
	const char *name;
	dllEntryProc dllEntry;
	vmMainProc vmMain;
} switchGameModules[] =
{
	{ "cgame", cgame_dllEntry, (vmMainProc)cgame_vmMain },
	{ "qagame", qagame_dllEntry, (vmMainProc)qagame_vmMain },
	{ "ui", ui_dllEntry, (vmMainProc)ui_vmMain },
};

/*
==============
Sys_SwitchLoadGameModule

The module a path (FS_FindVM's, "<dir>/qagame.sp.aarch64.a") names: its
entry point, with its system calls given to it; NULL for none. The handle is
the module's table entry, which Sys_UnloadDll leaves be.
==============
*/
void *Sys_SwitchLoadGameModule( const char *path, vmMainProc *entryPoint,
	intptr_t ( *systemcalls )( intptr_t, ... ) )
{
	const char *file = COM_SkipPath( (char *)path );
	size_t length;
	int i;

	length = strcspn( file, "." );
	for ( i = 0; i < ARRAY_LEN( switchGameModules ); i++ )
	{
		if ( strlen( switchGameModules[i].name ) == length &&
			!Q_stricmpn( file, switchGameModules[i].name, length ) )
		{
			*entryPoint = switchGameModules[i].vmMain;
			switchGameModules[i].dllEntry( systemcalls );
			return (void *)&switchGameModules[i];
		}
	}
	Com_Printf( "Sys_LoadGameDll(%s): no such module in this build\n", path );
	return NULL;
}
