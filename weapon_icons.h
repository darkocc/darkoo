#pragma once

// Familyhook 2024's ESP has a weapon "icon" mode (visuals -> weapon ->
// "text" / "icon"), but its id -> icon table is never filled (only read,
// through find-or-insert), so icon mode drew nothing. This fills it in: the
// lookup is hooked to return the icon character for the weapon, and
// familyhook's "Counter-Strike" font handle is pointed at an embedded weapon
// icon font. Choose "icon" in familyhook's menu to use it.
namespace weapon_icons {
    void init( );   // detours::init, Familyhook 2024 only
}
