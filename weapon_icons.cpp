#include "includes.h"
#include "weapon_icons.h"
#include "weapon_icon_font.h"

namespace weapon_icons {
    namespace {
        // Familyhook 2024 (mapped from its disassembly):
        //   100886D1  std::string& icon_map[ const int& weapon_id ]
        //             (unordered_map find-or-insert, only caller the ESP at
        //             1008D38F, which draws the result with the CS font)
        //   1007DA2D  font setup: "Counter-Strike", 28 px, weight 500,
        //             antialias | dropshadow, stored to the Font at [+27]
        constexpr const char* k_sig_lookup = "55 8B EC 51 51 FF 75 08 8D 45 F8 50 E8 ? ? ? ? 8B 00 83 C0 0C C9 C2 04 00";
        constexpr const char* k_sig_font   = "68 90 00 00 00 68 F4 01 00 00 6A 1C 8D 45 ? 50 8D 4D ? E8 ? ? ? ? 8B F0 BF";
        constexpr size_t      k_font_operand = 27;

        // Icon font as applied to familyhook's handle. 28 (familyhook's
        // size for this font) draws the icons oversized under the box.
        constexpr const char* k_face = "weaponicons";
        constexpr int         k_tall = 18;
        constexpr int         k_weight = 400;

        using lookup_t = std::string*( __thiscall* )( void*, const int* );
        CDetourHook g_lookup;

        uintptr_t g_font_global = 0;    // familyhook's Font { HFont handle; size }
        HFont     g_applied_handle = 0;
        int       g_applied_w = 0, g_applied_h = 0;
        bool      g_font_loaded = false;

        // weapon definition index -> weaponicons.ttf character. Built once
        // in init and never modified after, so the returned pointers stay
        // valid.
        std::unordered_map< int, std::string > g_icons;
        std::string g_none;

        void build_table( ) {
            const std::pair< int, const char* > table[ ] = {
                { 1, "A" },  { 2, "B" },  { 3, "C" },  { 4, "D" },              // deagle, elite, fiveseven, glock
                { 7, "W" },  { 8, "U" },  { 9, "Z" },  { 10, "R" },             // ak47, aug, awp, famas
                { 11, "X" }, { 13, "Q" }, { 14, "g" }, { 16, "S" },             // g3sg1, galil, m249, m4a4
                { 17, "K" }, { 19, "P" }, { 23, "N" }, { 24, "L" },             // mac10, p90, mp5 (mp7 icon), ump45
                { 25, "b" }, { 26, "M" }, { 27, "d" }, { 28, "f" },             // xm1014, bizon, mag7, negev
                { 29, "c" }, { 30, "H" }, { 31, "h" }, { 32, "E" },             // sawedoff, tec9, taser, p2000
                { 33, "N" }, { 34, "O" }, { 35, "e" }, { 36, "F" },             // mp7, mp9, nova, p250
                { 38, "Y" }, { 39, "V" }, { 40, "a" },                          // scar20, sg556, ssg08
                { 41, "i" }, { 42, "i" }, { 59, "j" }, { 74, "i" },             // knives: gg, ct, t, spanner
                { 43, "k" }, { 44, "l" }, { 45, "m" }, { 46, "n" },             // flash, he, smoke, molotov
                { 47, "o" }, { 48, "p" }, { 49, "q" },                          // decoy, incendiary, c4
                { 60, "T" }, { 61, "G" }, { 63, "I" }, { 64, "J" },             // m4a1-s, usp-s, cz75, revolver
                { 500, "1" }, { 505, "2" }, { 506, "3" }, { 507, "4" },         // bayonet, flip, gut, karambit
                { 508, "5" }, { 509, "6" }, { 512, "0" }, { 514, "7" },         // m9, huntsman, falchion, bowie
                { 515, "8" }, { 516, "9" },                                     // butterfly, shadow daggers
            };
            for( const auto& [ id, glyph ] : table )
                g_icons.emplace( id, glyph );
        }

        void log( const std::string& s ) {
            if( g_csgo.m_cvar ) {
                g_csgo.m_cvar->ConsoleColorPrintf( Color( 112, 128, 144 ), "[icons] " );
                g_csgo.m_cvar->ConsoleColorPrintf( Color( 220, 220, 225 ), "%s\n", s.c_str( ) );
            }
        }

        // Familyhook's font handle -> the icon font. Again whenever the handle
        // or the screen size changes (familyhook rebuilds its fonts then).
        // Runs on the game thread (from inside familyhook's ESP drawing).
        void apply_font( ) {
            if( !g_font_global ) return;
            const HFont handle = *( HFont* )g_font_global;
            int w = 0, h = 0;
            g_csgo.m_engine->GetScreenSize( w, h );
            if( !handle || ( handle == g_applied_handle && w == g_applied_w && h == g_applied_h ) )
                return;
            if( !g_font_loaded ) {
                DWORD installed = 0;
                g_font_loaded = AddFontMemResourceEx( ( void* )k_weapon_icon_font, ( DWORD )sizeof( k_weapon_icon_font ), nullptr, &installed ) && installed;
                if( !g_font_loaded ) { log( "weapon icon font could not be loaded" ); g_font_global = 0; return; }
            }
            g_csgo.m_surface->SetFontGlyphSet( handle, k_face, k_tall, k_weight, 0, 0, FONTFLAG_ANTIALIAS | FONTFLAG_DROPSHADOW );
            g_applied_handle = handle;
            g_applied_w = w;
            g_applied_h = h;
            log( "weapon icon font applied to familyhook's ESP" );
        }

        std::string* __fastcall hk_lookup( void* self, void* edx, const int* key ) {
            apply_font( );
            if( !key ) return &g_none;
            // the key is m_iItemDefinitionIndex, a 16-bit field read as int
            const auto it = g_icons.find( *key & 0xFFFF );
            return it != g_icons.end( ) ? &it->second : &g_none;
        }
    }

    void init( ) {
        if( g_fam.build != FamilyBuild::fh2024 )
            return;
        build_table( );

        Address font = pattern::find( g_csgo.m_family_dll, k_sig_font );
        g_font_global = font ? font.at< uintptr_t >( k_font_operand ) : 0;
        if( !g_font_global ) log( "familyhook's icon font not found: icons fall back to its own font" );

        Address lookup = pattern::find( g_csgo.m_family_dll, k_sig_lookup );
        if( !lookup || !g_lookup.Create( lookup.as< void* >( ), ( void* )&hk_lookup ) ) {
            log( "icon lookup hook failed: weapon icon mode unchanged" );
            return;
        }
        log( "weapon icons ready (familyhook visuals: weapon -> icon)" );
    }
}
