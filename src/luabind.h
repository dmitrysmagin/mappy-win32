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
#define MAXLUAMENU 40
extern char luascriptmenu[MAXLUAMENU][260];
extern int  numluascript;
extern int  luafilemenu;

const char *LuaGetMapwinPath (void);
int  LuaFileMenuRun (int cmd);

/* Mouse / property-dialog hooks */
void LuaSetMouseBlock (int x, int y, int x2, int y2);
int  LuaRunMouseButton (int button, int x, int y);
void LuaRunBlockProp (int editidx);
void LuaRunObjectProp (int editidx);
int LuaGetStrBlkUser (void);
int LuaGetStrObjUser (void);
void LuaRefreshBlockSort (void);
void LuaRefreshObjectSort (void);
int LuaBlockSortCount (void);
int LuaBlockSortAt (int i);
int LuaObjectSortCount (void);
int LuaObjectSortAt (int i);

#endif
