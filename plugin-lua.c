#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#ifdef _LUA_PLUGIN

/*******************************************************************************
 *
 * Lua plugin
 *
 */

#include <lua.h>
#include <lualib.h>
#include <lauxlib.h>

#include <alloca.h>
#include <string.h>

#include "core.h"
#include "utils.h"
#include "main.h"
#include "plugin.h"


/**
 *
 */
int proxenet_lua_load_file(plugin_t* plugin)
{
	char* pathname;
	lua_State* lua_interpreter;

        if(plugin->state != INACTIVE){
#ifdef DEBUG
                if(cfg->verbose > 2)
                        xlog(LOG_DEBUG, "Plugin '%s' is already loaded. Skipping...\n", plugin->name);
#endif
                return 0;
        }


        pathname = plugin->fullpath;

	lua_interpreter = (lua_State*) plugin->interpreter->vm;

	if (luaL_dofile(lua_interpreter, pathname)) {
		xlog(LOG_ERROR, "Failed to load '%s'\n", pathname);
		return -1;
	}

	return 0;
}


/**
 *
 */
int proxenet_lua_initialize_vm(plugin_t* plugin)
{
	interpreter_t* interpreter;
	lua_State* lua_interpreter;

	interpreter = plugin->interpreter;

	if (interpreter->ready)
		return 0;

	lua_interpreter = luaL_newstate();
	luaL_openlibs(lua_interpreter);

	plugin->interpreter->vm = lua_interpreter;
	plugin->interpreter->ready = true;

	return 0;
}


/**
 *
 */
int proxenet_lua_destroy_plugin(plugin_t* plugin)
{
        plugin->state = INACTIVE;
        plugin->pre_function = NULL;
        plugin->post_function = NULL;

        return 0;
}


/**
 *
 */
int proxenet_lua_destroy_vm(interpreter_t* interpreter)
{
	lua_State* lua_interpreter;
	lua_interpreter = (lua_State*)interpreter->vm;

	lua_close(lua_interpreter);
	interpreter->ready = false;
        interpreter->vm = NULL;

	return 0;
}


/**
 *
 */
static char* proxenet_lua_execute_function(interpreter_t* interpreter, request_t *request)
{
	const char *lRet;
	char *buf;
	lua_State* lua_interpreter;
	size_t len;
	char *uri;

	uri = request->http_infos.uri;
	if (!uri)
		return NULL;

	lua_interpreter = (lua_State*) interpreter->vm;

	if (request->type == REQUEST)
		lua_getglobal(lua_interpreter, CFG_REQUEST_PLUGIN_FUNCTION);
	else
		lua_getglobal(lua_interpreter, CFG_RESPONSE_PLUGIN_FUNCTION);

	lua_pushnumber(lua_interpreter, request->id);
	lua_pushlstring(lua_interpreter, request->data, request->size);
	lua_pushlstring(lua_interpreter, uri, strlen(uri));


	lua_call(lua_interpreter, 3, 1);
	lRet = lua_tolstring(lua_interpreter, -1, &len);
	if (!lRet)
		return NULL;

	buf = proxenet_xstrdup(lRet, len);
	if (!buf)
		return NULL;

	request->size = len;
	return buf;
}

/**
 *
 */
static void proxenet_lua_lock_vm(interpreter_t *interpreter)
{
	pthread_mutex_lock(&interpreter->mutex);
}


/**
 *
 */
static void proxenet_lua_unlock_vm(interpreter_t *interpreter)
{
	pthread_mutex_unlock(&interpreter->mutex);
}

/**
 *
 */
char* proxenet_lua_plugin(plugin_t* plugin, request_t *request)
{
	char* buf = NULL;
	interpreter_t *interpreter = plugin->interpreter;

	proxenet_lua_lock_vm(interpreter);
	buf = proxenet_lua_execute_function(interpreter, request);
	proxenet_lua_unlock_vm(interpreter);

	return buf;
}

#endif /* _LUA_PLUGIN */
