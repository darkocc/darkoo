#include "includes.h"

Client g_ctx{ };

// init routine: interfaces, familyhook detection, hooks (CSGO::init ->
// detours::init -> helper::init).
ulong_t __stdcall Client::init( void* arg ) {
	return g_csgo.init( ) ? 1 : 0;
}
