#pragma once
#include <cstdlib>

class CCommand {
public:
	int ArgC() const {
		return m_argc;
	}

	const char* Arg(int index) const {
		if (index < 0 || index >= m_argc)
			return "";

		return m_argv[index];
	}

private:
	enum {
		COMMAND_MAX_ARGC = 64,
		COMMAND_MAX_LENGTH = 512
	};

	int m_argc;
	int m_argv0_size;
	char m_args_buffer[COMMAND_MAX_LENGTH];
	char m_argv_buffer[COMMAND_MAX_LENGTH];
	const char* m_argv[COMMAND_MAX_ARGC];
};

using FnCommandCallback_t = void(__cdecl*)(const CCommand& command);

class ConCommandBase {
public:
	virtual ~ConCommandBase() {}

	virtual bool IsCommand() const {
		return true;
	}

	virtual bool IsFlagSet(int flag) const {
		return (m_flags & flag) != 0;
	}

	virtual void AddFlags(int flags) {
		m_flags |= flags;
	}

	virtual void RemoveFlags(int flags) {
		m_flags &= ~flags;
	}

	virtual int GetFlags() const {
		return m_flags;
	}

	virtual const char* GetName() const {
		return m_name;
	}

	virtual const char* GetHelpText() const {
		return m_help_string;
	}

	virtual bool IsRegistered() const {
		return m_registered != 0;
	}

	virtual int GetDLLIdentifier() const {
		return -1;
	}

	virtual void Create(const char* name, const char* help_string, int flags) {
		m_next = nullptr;
		m_registered = 0;
		m_name = name;
		m_help_string = help_string;
		m_flags = flags;
	}

	virtual void Init() {}

public:
	ConCommandBase* m_next;
	int m_registered;
	const char* m_name;
	const char* m_help_string;
	int m_flags;
};

class ConCommand : public ConCommandBase {
public:
	ConCommand(
		const char* name,
		FnCommandCallback_t callback,
		const char* help_string = "",
		int flags = 0
	) {
		Create(name, help_string, flags);

		m_callback = callback;
		m_completion_callback = nullptr;
		m_has_completion_callback = false;
		m_using_new_command_callback = true;
		m_using_command_callback_interface = false;
	}

	virtual bool IsCommand() const override {
		return true;
	}

	virtual int AutoCompleteSuggest(const char*, void*) {
		return 0;
	}

	virtual bool CanAutoComplete() {
		return false;
	}

	virtual void Dispatch(const CCommand& command) {
		if (m_callback)
			m_callback(command);
	}

private:
	FnCommandCallback_t m_callback;
	void* m_completion_callback;
	bool m_has_completion_callback;
	bool m_using_new_command_callback;
	bool m_using_command_callback_interface;
};

enum {
	FCVAR_NONE                  = 0,
	FCVAR_UNREGISTERED          = ( 1 << 0 ),
	FCVAR_DEVELOPMENTONLY       = ( 1 << 1 ),
	FCVAR_GAMEDLL               = ( 1 << 2 ),
	FCVAR_CLIENTDLL             = ( 1 << 3 ),
	FCVAR_HIDDEN                = ( 1 << 4 ),
	FCVAR_PROTECTED             = ( 1 << 5 ),
	FCVAR_SPONLY                = ( 1 << 6 ),
	FCVAR_ARCHIVE               = ( 1 << 7 ),
	FCVAR_NOTIFY                = ( 1 << 8 ),
	FCVAR_USERINFO              = ( 1 << 9 ),
	FCVAR_CHEAT                 = ( 1 << 14 ),
	FCVAR_PRINTABLEONLY         = ( 1 << 10 ),
	FCVAR_UNLOGGED              = ( 1 << 11 ),
	FCVAR_NEVER_AS_STRING       = ( 1 << 12 ),
	FCVAR_REPLICATED            = ( 1 << 13 ),
	FCVAR_DEMO                  = ( 1 << 16 ),
	FCVAR_DONTRECORD            = ( 1 << 17 ),
	FCVAR_RELEASE               = ( 1 << 19 ),
	FCVAR_NOT_CONNECTED         = ( 1 << 22 ),
	FCVAR_ARCHIVE_XBOX          = ( 1 << 24 ),
	FCVAR_SERVER_CAN_EXECUTE    = ( 1 << 28 ),
	FCVAR_SERVER_CANNOT_QUERY   = ( 1 << 29 ),
	FCVAR_CLIENTCMD_CAN_EXECUTE = ( 1 << 30 )
};

// callback prototypes.
using ChangeCallbackV1_t = void( __cdecl* )( );
using ChangeCallback_t   = void( __cdecl* )( void* var, const char* old, float flold );

// do not use these, use the virtuals, please!
struct CVValue_t {
	char* m_string;
	int   m_str_len;
	float m_float;
	int   m_int;
};

class ConVar {
private:
	void* vtable; // 0x0000

public:
	ConVar*                          m_next; // 0x0004
	int                              m_registered; // 0x0008
	char*                            m_name;	// 0x000C
	char*                            m_help_string;// 0x0010
	int                              m_flags;// 0x0014
	ChangeCallbackV1_t				 m_callback;//0x0018
	ConVar*                          m_parent;
	char*                            m_default_value;
	CVValue_t                        m_value;
	int                              m_has_min;
	float                            m_min;
	int                              m_has_max;
	float                            m_max;
	CUtlVector< ChangeCallback_t >   m_callbacks;

public:
	enum indices : size_t {
		GETSTRING      = 11,
		GETFLOAT       = 12,
		GETINT         = 13,
		GETBOOL        = 13,
		SETVALUE_SZ    = 14,
		SETVALUE_FLOAT = 15,
		SETVALUE_INT   = 16,
	};

public:
	__forceinline const char* GetString( ) {
		return util::get_method< const char*( __thiscall* )( decltype( this ) ) >( this, GETSTRING )( this );
	}

	__forceinline float GetFloat( ) {
		return util::get_method< float( __thiscall* )( decltype( this ) ) >( this, GETFLOAT )( this );
	}

	__forceinline int GetInt( ) {
		return util::get_method< int( __thiscall* )( decltype( this ) ) >( this, GETINT )( this );
	}

	__forceinline void SetValue( const char* value ) {
		return util::get_method< void( __thiscall* )( decltype( this ), const char* ) >( this, SETVALUE_SZ )( this, value );
	}

	__forceinline void SetValue( float value ) {
		return util::get_method< void( __thiscall* )( decltype( this ), float ) >( this, SETVALUE_FLOAT )( this, value );
	}

	__forceinline void SetValue( int value ) {
		return util::get_method< void( __thiscall* )( decltype( this ), int ) >( this, SETVALUE_INT )( this, value );
	}
};
class ConCommandBase;
class ICvar {
public:
	enum indices {
		REGISTERCONCOMMAND = 10,
		UNREGISTERCONCOMMAND = 11,

		FINDVAR = 14,
		CONSOLECOLORPRINTF = 25,
	};

private:
	static bool IsReadablePtr(const void* ptr)
	{
		if (!ptr)
			return false;

		MEMORY_BASIC_INFORMATION mbi{};
		if (!VirtualQuery(ptr, &mbi, sizeof(mbi)))
			return false;

		if (!(mbi.State & MEM_COMMIT))
			return false;

		if (mbi.Protect & (PAGE_NOACCESS | PAGE_GUARD))
			return false;

		return true;
	}

	static bool IsExecutablePtr(const void* ptr)
	{
		if (!ptr)
			return false;

		MEMORY_BASIC_INFORMATION mbi{};
		if (!VirtualQuery(ptr, &mbi, sizeof(mbi)))
			return false;

		if (!(mbi.State & MEM_COMMIT))
			return false;

		if (mbi.Protect & (PAGE_NOACCESS | PAGE_GUARD))
			return false;

		const DWORD protect = mbi.Protect & 0xff;

		return protect == PAGE_EXECUTE ||
			protect == PAGE_EXECUTE_READ ||
			protect == PAGE_EXECUTE_READWRITE ||
			protect == PAGE_EXECUTE_WRITECOPY;
	}

	template <typename Fn>
	static Fn GetSafeVFunc(void* base, size_t index)
	{
		if (!IsReadablePtr(base))
			return nullptr;

		auto vtable = *reinterpret_cast<void***>(base);
		if (!IsReadablePtr(vtable))
			return nullptr;

		if (!IsReadablePtr(&vtable[index]))
			return nullptr;

		void* fn = vtable[index];
		if (!IsExecutablePtr(fn))
			return nullptr;

		return reinterpret_cast<Fn>(fn);
	}

public:
	__forceinline ConVar* GetCommands()
	{
		auto list_ptr = reinterpret_cast<ConVar**>((uintptr_t)this + 0x30);

		if (!IsReadablePtr(list_ptr))
			return nullptr;

		return *list_ptr;
	}

	bool SafeRegisterConCommand(ConCommandBase* command)
	{
		if (!command)
			return false;

		using fn_t = void(__thiscall*)(ICvar*, ConCommandBase*);

		auto fn = GetSafeVFunc<fn_t>(this, REGISTERCONCOMMAND);
		if (!fn)
			return false;

		__try
		{
			fn(this, command);
			return true;
		}
		__except (EXCEPTION_EXECUTE_HANDLER)
		{
			return false;
		}
	}

	bool SafeUnregisterConCommand(ConCommandBase* command)
	{
		if (!command)
			return false;

		using fn_t = void(__thiscall*)(ICvar*, ConCommandBase*);

		auto fn = GetSafeVFunc<fn_t>(this, UNREGISTERCONCOMMAND);
		if (!fn)
			return false;

		__try
		{
			fn(this, command);
			return true;
		}
		__except (EXCEPTION_EXECUTE_HANDLER)
		{
			return false;
		}
	}

	bool RegisterConCommand(ConCommandBase* command)
	{
		return SafeRegisterConCommand(command);
	}

	bool UnregisterConCommand(ConCommandBase* command)
	{
		return SafeUnregisterConCommand(command);
	}

	__forceinline ConVar* FindVar(const hash32_t hash)
	{
		ConVar* list = GetCommands();
		if (!list)
			return nullptr;

		for (auto it = list; it != nullptr; it = it->m_next) {
			if (!IsReadablePtr(it))
				break;

			if (!it->m_name)
				continue;

			if (FNV1a::get(it->m_name) == hash)
				return it;
		}

		return nullptr;
	}

	template< typename... arguments >
	void ConsoleColorPrintf(const Color& color, const char* format, arguments ... args)
	{
		using fn_t = void(__cdecl*)(decltype(this), const Color&, const char*, ...);

		auto fn = GetSafeVFunc<fn_t>(this, CONSOLECOLORPRINTF);
		if (!fn)
			return;

		__try
		{
			fn(this, color, format, args...);
		}
		__except (EXCEPTION_EXECUTE_HANDLER)
		{
			return;
		}
	}
};