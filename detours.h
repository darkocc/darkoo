#pragma once

// The addon is the grenade helper only. Three familyhook hooks feed it:
// StartMove (the command, after familyhook saved its strafe angles), DoMove
// (after familyhook's anti-aim: silent throw angles) and LockCursor (cursor
// free while the helper's menu is open). The helper draws and takes input
// through its own paint and window hooks (helper/helper_menu.cpp).
namespace originals {
	inline CDetourHook FamilyStartMove;
	inline CDetourHook FamilyDoMove;
	inline CDetourHook FamilyLockCursor;
}

namespace detours {
	void init( );
	void __fastcall FamilyStartMove( void* ecx, char edx, __m128, CUserCmd* cmd );
	void __fastcall FamilyDoMove( void* ecx, void* edx );
	void __fastcall FamilyLockCursor( void* ecx, void* edx );
};
