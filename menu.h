#pragma once

#define TOTAL_TABS 8
#define ELEMENT_BORDER				20
#define ELEMENT_COLUMNS				2
#define ELEMENT_DISTANCE			10
#define ELEMENT_COMBINED_DISTANCE	7
#define ELEMENT_WIDTH				175
#define ELEMENT_COLUMN_SEPARATOR	60

enum Tabs : int {
	aimbot = 0,
	antiaim = 1,
	players = 2,
	visuals = 3,
	movement = 4,
	skins = 5,
	misc = 6,
	config = 7,
};

// familyhook's menu, read through the loaded build's layout (g_fam). Every
// pointer and std::string read is checked first: a wrong offset on an
// unmapped build reads as "not found" instead of crashing the game.
class menu_instance {
	static bool readable( const void* p, size_t n ) {
		MEMORY_BASIC_INFORMATION mbi{ };
		if ( !p || !VirtualQuery( p, &mbi, sizeof( mbi ) ) ) return false;
		if ( mbi.State != MEM_COMMIT || ( mbi.Protect & ( PAGE_GUARD | PAGE_NOACCESS ) ) ) return false;
		return ( uintptr_t )p + n <= ( uintptr_t )mbi.BaseAddress + mbi.RegionSize;
	}

	// MSVC std::string: 16-byte SSO buffer or heap pointer, size at +0x10,
	// capacity at +0x14.
	static bool read_string( const void* s, std::string& out ) {
		if ( !readable( s, 0x18 ) ) return false;
		const size_t size = *( const size_t* )( ( const uint8_t* )s + 0x10 );
		const size_t cap  = *( const size_t* )( ( const uint8_t* )s + 0x14 );
		if ( size > 255 || cap < size || cap < 15 ) return false;
		const char* data = cap >= 16 ? *( const char* const* )s : ( const char* )s;
		if ( !readable( data, size ) ) return false;
		out.assign( data, size );
		return true;
	}

	std::vector< void* > elements( void* tab ) {
		std::vector< void* > out;
		if ( !tab || !g_fam.tab_elements ) return out;
		const uint8_t* vec = ( const uint8_t* )tab + g_fam.tab_elements;
		if ( !readable( vec, 8 ) ) return out;
		void* const* first = *( void* const* const* )vec;
		void* const* last  = *( void* const* const* )( vec + 4 );
		if ( !first || last < first || last - first > 512 || !readable( first, ( last - first ) * 4 ) ) return out;
		out.assign( first, last );
		return out;
	}

public:
	// the form's tab vector, read live: tabs added later (the native menu)
	// reallocate it, so a pointer cached at startup would dangle
	int tab_count( ) {
		if ( !g_csgo.MainForm ) return 0;
		const uint8_t* vec = g_csgo.MainForm.as< const uint8_t* >( ) + 0x28;
		if ( !readable( vec, 8 ) ) return 0;
		void* const* first = *( void* const* const* )vec;
		void* const* last  = *( void* const* const* )( vec + 4 );
		if ( !first || last < first || last - first > 64 ) return 0;
		return ( int )( last - first );
	}

	__forceinline void* get_tab( int index ) {
		if ( index < 0 || index >= tab_count( ) )
			return nullptr;
		void* const* first = *( void* const* const* )( g_csgo.MainForm.as< const uint8_t* >( ) + 0x28 );
		return readable( first + index, 4 ) ? first[ index ] : nullptr;
	}

	// a Tab's title: its first member
	std::string title_of( void* tab ) {
		std::string s;
		if ( tab ) read_string( tab, s );
		return s;
	}

	std::string label_of( void* elem ) {
		std::string s;
		if ( elem && g_fam.el_label ) read_string( ( uint8_t* )elem + g_fam.el_label, s );
		return s;
	}

	std::string file_id_of( void* elem ) {
		std::string s;
		if ( elem && g_fam.el_file_id ) read_string( ( uint8_t* )elem + g_fam.el_file_id, s );
		return s;
	}

	void* find_element( int tab_index, const std::string& label_to_find, bool slider = false ) {
		for ( void* elem : elements( get_tab( tab_index ) ) )
			if ( elem && label_of( elem ) == label_to_find )
				return elem;
		return nullptr;
	}

	void* find_element_with_file_id( int tab_index, const std::string& file_id_to_find ) {
		for ( void* elem : elements( get_tab( tab_index ) ) )
			if ( elem && file_id_of( elem ) == file_id_to_find )
				return elem;
		return nullptr;
	}

	// Any tab: the tab order is not guaranteed across familyhook builds.
	void* find_element_any( const std::string& label_to_find ) {
		for ( int t = 0, n = tab_count( ); t < n; ++t )
			if ( void* e = find_element( t, label_to_find ) ) return e;
		return nullptr;
	}

	void* find_element_with_file_id_any( const std::string& file_id_to_find ) {
		for ( int t = 0, n = tab_count( ); t < n; ++t )
			if ( void* e = find_element_with_file_id( t, file_id_to_find ) ) return e;
		return nullptr;
	}

	void find_all_elements( int tab_index ) {
		for ( void* elem : elements( get_tab( tab_index ) ) )
			std::cout << "found element with label " << label_of( elem ) << " and address " << std::hex << ( uintptr_t )elem << std::endl;
	}

	// the bool / int at the element's value offset (checkbox / keybind)
	template< typename T >
	T* value_of( void* elem ) {
		if ( !elem || !g_fam.el_value ) return nullptr;
		T* p = ( T* )( ( uint8_t* )elem + g_fam.el_value );
		return readable( p, sizeof( T ) ) ? p : nullptr;
	}
};

extern menu_instance g_menu;
