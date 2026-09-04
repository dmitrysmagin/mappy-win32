/* luabind.h -- Lua 5.0.3 <-> Mappy bridge */
#ifndef LUABIND_H
#define LUABIND_H

#include "lua.h"
#include "lauxlib.h"
#include "lualib.h"

/* Engine lifecycle */
int  LuaInit (void);
void LuaShutdown (void);
int  LuaRunScript (const char * filename);
int  LuaRunString (const char * code);

/* Menu integration */
#define MAXLUAMENU 32
extern char luascriptmenu[MAXLUAMENU][260];
extern int  numluascript;

/* Callback for mouse-button Lua triggers (button >= 5) */
int LuaRunMouseButton (int button, int x, int y);

#endif
