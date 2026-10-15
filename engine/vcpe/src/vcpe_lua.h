/* Optional Lua 5.1 embedding. No PSP dependency: use the same VM in host tests.
 * Link Lua separately; the application supplies the licensed interpreter. */
#ifndef VCPE_LUA_H
#define VCPE_LUA_H
#include <stddef.h>
typedef struct lua_State lua_State;
typedef struct {
    lua_State *state;
    size_t used, peak, limit;
    unsigned instructions, instruction_limit;
    char error[1024];
} VcpeLua;

/* The standard libraries need a minimum cap of 64 KiB. */
int vcpe_lua_open(VcpeLua *vm, size_t memory_limit);
void vcpe_lua_close(VcpeLua *vm);
/* Set zero to disable the per-call instruction bound during content loading. */
void vcpe_lua_instruction_limit(VcpeLua *vm, unsigned limit);
int vcpe_lua_file(VcpeLua *vm, const char *path);
int vcpe_lua_string(VcpeLua *vm, const char *source, const char *name);
/* Function and arguments must be on the Lua stack. Errors restore the stack. */
int vcpe_lua_call(VcpeLua *vm, int nargs, int nresults);
const char *vcpe_lua_error(const VcpeLua *vm);
/* os.time() returns t and os.clock() counts in fixed 1/60 s steps from 0: scripts that seed from the clock run
 * the same every time (autotest). Call after vcpe_lua_open. */
void vcpe_lua_fixed_clock(VcpeLua *vm, double t);
#endif
