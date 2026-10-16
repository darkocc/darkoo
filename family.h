#pragma once

// The familyhook builds the grenade helper hooks into and what differs
// between them: hook patterns and the menu's object layout. Picked once at
// startup (CSGO::init_family_signatures) from which StartMove pattern the
// loaded familyhook contains. Values were mapped from the two DLLs'
// disassembly; see the comments for where each came from.
enum class FamilyBuild { unknown, fh25, fh2024 };

struct FamilyLayout {
    FamilyBuild build = FamilyBuild::unknown;
    const char* name  = "unknown";

    // hook patterns
    const char* sig_start_move  = nullptr;
    const char* sig_do_move     = nullptr;
    const char* sig_lock_cursor = nullptr;

    // menu layout (supremacy-style Form / Tab / Element), used to find the
    // "stop" / "straight nade" checkboxes the helper switches off
    size_t form_active_tab = 0;   // Tab* of the open tab (RegisterTab)
    size_t tab_elements    = 0;   // std::vector< Element* > (RegisterElement)
    size_t el_label        = 0;   // std::string, the label shown
    size_t el_file_id      = 0;   // std::string, config key
    size_t el_value        = 0;   // derived class's first member: checkbox bool / keybind key
    size_t el_type         = 0;   // ElementTypes, 0 = unknown (checked by label + value instead)
};

inline FamilyLayout g_fam{ };

namespace family_layouts {
    // 25.dll. StartMove 100544B0, DoMove 1004F7B0, LockCursor hook 100A6C80.
    inline FamilyLayout fh25( ) {
        FamilyLayout l;
        l.build = FamilyBuild::fh25;
        l.name  = "familyhook (25)";
        l.sig_start_move  = "55 8B EC 83 EC ? 56 8B 75 ? 57 8B F9 89 7D";
        l.sig_do_move     = "55 8B EC 83 EC ? 53 56 8B D9";
        l.sig_lock_cursor = "80 3D ? ? ? ? ? 74 ? 8B 0D ? ? ? ? 8B 01 8B 80";
        l.form_active_tab = 0x54;
        l.tab_elements    = 0x1C;
        l.el_label        = 0x28;
        l.el_file_id      = 0x40;
        l.el_value        = 0xD4;
        l.el_type         = 0x5C;
        return l;
    }

    // Familyhook 2024.dll. StartMove 100346C8, DoMove 1003111E (OnTick
    // 10033120's first / tail calls); LockCursor hook 10073222; RegisterTab
    // 100437B6 (active tab +0x34); RegisterElement 10028EFD (elements +0x48);
    // Checkbox::setup 1002AF1D / Keybind::setup 100595C4 (label +0xC0, file id
    // +0x40, value +0xBC).
    inline FamilyLayout fh2024( ) {
        FamilyLayout l;
        l.build = FamilyBuild::fh2024;
        l.name  = "Familyhook 2024";
        l.sig_start_move  = "55 8B EC A1 ? ? ? ? 83 EC 18 56 8B F1 33 C9 41 57 8B 7D 08 84 C1";
        l.sig_do_move     = "55 8B EC 83 E4 F0 81 EC 58 01 00 00 56 8B F1 B9 96 00 00 00";
        l.sig_lock_cursor = "80 3D ? ? ? ? 00 74 ? 8B 0D ? ? ? ? 8B 01 FF 90 08 01 00 00";
        l.form_active_tab = 0x34;
        l.tab_elements    = 0x48;
        l.el_label        = 0xC0;
        l.el_file_id      = 0x40;
        l.el_value        = 0xBC;
        l.el_type         = 0;
        return l;
    }
}
