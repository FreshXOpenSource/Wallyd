#include "plugins.h"
#include "util.h"
#include <stdbool.h>

#define PLUGIN_SCOPE "sys"

#ifndef WALLY_sys
#define WALLY_sys

duk_context *ctx = NULL;
extern pluginHandler *ph;

typedef struct sysStructure
{
    const char *name;
} sysStructure;

extern const duk_function_list_entry sysMethods[];

int dumpDebug(void *p){
  return 0;
}

int wally_sleep(void *time){
    int t = atoi(time);
    if(t > 0){
       slog(DEBUG,LOG_CORE,"Command processing sleeling for %d seconds",t);
       sleep(t);
       slog(DEBUG,LOG_CORE,"Command continues.");
    }
    return 0;
}

int wally_registerCallback(void *str){
   char *all = str;
   char *name = strtok(str," ");
   char *rest = all+strlen(name)+1;
   slog(DEBUG,LOG_CORE,"Registering callback %s = %s",name,rest);
   ht_insert_simple(ph->callbacks,name,rest);
   return 0;
}

void setDebug(void *str){
    getNum(strtok(str," "),&ph->loglevel);
    slog(DEBUG,LOG_CORE,"Setting new debug level to : %d",ph->loglevel);
}

char *cleanupPlugin(void *p){
   slog(DEBUG,LOG_PLUGIN,"Plugin "PLUGIN_SCOPE" uninitialized");
   return NULL;
}

// the destructor
duk_ret_t js_sys_dtor(duk_context *ctx)
{
    // The object to delete is passed as first argument instead
    duk_get_prop_string(ctx, 0, "\xff""\xff""deleted");

    bool deleted = duk_to_boolean(ctx, -1);
    duk_pop(ctx);

    // Get the pointer and free it
    if (!deleted) {
        duk_get_prop_string(ctx, 0, "\xff""\xff""data");
        void *freePtr = duk_to_pointer(ctx, -1);
        free(freePtr);
        duk_pop(ctx);

        // Mark as deleted
        duk_push_boolean(ctx, true);
        duk_put_prop_string(ctx, 0, "\xff""\xff""deleted");
    }
    return 0;
}

// Constructor of the JS Object
duk_ret_t js_sys_ctor(duk_context *ctx)
{
    slog(DEBUG,LOG_JS, "Creating new object of "PLUGIN_SCOPE);

    sysStructure *mps = malloc(sizeof(sysStructure));
    mps->name = duk_require_string(ctx, 0);
    
    // Push special this binding to the function being constructed
    duk_push_this(ctx);

    // Store the underlying object
    duk_push_pointer(ctx, mps);
    duk_put_prop_string(ctx, -2, "\xff""\xff""data");

    // TODO : - if not existand create a hash_map
    //        - store structure to a hash_map('name');
    //          so that it can be reached from JS and C
    
    // Store a boolean flag to mark the object as deleted because the destructor may be called several times
    duk_push_boolean(ctx, false);
    duk_put_prop_string(ctx, -2, "\xff""\xff""deleted");

    // Store the function destructor
    duk_push_c_function(ctx, js_sys_dtor, 1);
    duk_set_finalizer(ctx, -2);

    return 0;
}

int sysInfo(void *i){
   if(i) {
      slog(DEBUG,LOG_UTIL,"Info : %s", i);
      // TODO : Get structure from hashmap('name');
      return true;
   } else {
      slog(DEBUG,LOG_UTIL,"Wrong parameters calling "PLUGIN_SCOPE"::info <name>");
      return false;
   }
}

duk_ret_t js_sys_toString(duk_context *ctx)
{
   duk_push_this(ctx);  /* -> stack: [ this ] */
   duk_get_prop_string(ctx, 0, "\xff""\xff""data");
   sysStructure *mps = duk_to_pointer(ctx, -1);
   duk_pop(ctx);
   duk_push_sprintf(ctx, "%s",mps->name);
   return 1;
}

//duk_ret_t js_log(duk_context *ctx)
//{
 //  int num = 0;
 //  char *elem=NULL;
//   slog(DEBUG,DEBUG,"Found %d log elements",duk_get_top(ctx));
//   for(int i = 0 ; i < duk_get_top(ctx); i++){
//   }
//    return 0;
//}


duk_ret_t js_sys_info(duk_context *ctx)
{
   duk_push_this(ctx);  /* -> stack: [ this ] */
   duk_get_prop_string(ctx, 0, "\xff""\xff""data");
   sysStructure *mps = duk_to_pointer(ctx, -1);
   duk_pop(ctx);
   duk_push_sprintf(ctx, "{ name : %s }",mps->name);
   return 1;
}

const duk_function_list_entry sysMethods[] = {
    { "info",       js_sys_info,      0   },
//    { "log",        js_log,  DUK_VARARGS  },
    { "toString",   js_sys_toString,  0   },
    { NULL,    NULL,            0 }
};


duk_ret_t my_getter(duk_context *ctx){
   return 0;
}
duk_ret_t my_setter(duk_context *ctx){
   return 0;
}

int js_initSysPlugin(duk_context *ctx){
   duk_push_c_function(ctx, js_sys_ctor, 1 );
   duk_push_object(ctx);
   duk_put_function_list(ctx, -1, sysMethods);
   duk_put_prop_string(ctx, -2, "prototype");
   duk_put_global_string(ctx, "Sys");  /* -> stack: [ ] */

//   duk_idx_t obj_idx = 0;

//  duk_push_string(ctx, "debugLevel");
//   duk_push_c_function(ctx, my_getter, 0 /*nargs*/);
//   duk_push_c_function(ctx, my_setter, 1 /*nargs*/);
//   duk_def_prop(ctx, obj_idx,
//             DUK_DEFPROP_HAVE_GETTER |
//             DUK_DEFPROP_HAVE_SETTER);

   return 0;
}

//const function_list_entry c_systemMethods[] = {
//   {  "quit"                        ,WFUNC_SYNC, c_cleanupWally, 0},
//   {  PLUGIN_SCOPE"::quit"          ,WFUNC_SYNC, c_cleanupWally, 0},
//   {  PLUGIN_SCOPE"::setDebug"      ,WFUNC_SYNC, setDebug, 0},
//   {  PLUGIN_SCOPE"::debug"         ,WFUNC_SYNC, dumpDebug, 0},
//   {  PLUGIN_SCOPE"::sleep"         ,WFUNC_SYNC, wally_sleep, 0},
//   {  PLUGIN_SCOPE"::loadPlugins"   ,WFUNC_SYNC, pluginLoader, 0},
//   {  PLUGIN_SCOPE"::callback"      ,WFUNC_SYNC, wally_registerCallback, 0},
//   {  PLUGIN_SCOPE"::info"          ,WFUNC_SYNC, sysInfo, 0},
//   {  NULL, 0, NULL, 0 }
//};
 
char *initSysPlugin(){
   slog(DEBUG,LOG_PLUGIN,"Plugin "PLUGIN_SCOPE" initializing.");
   ctx = ph->ctx;

   wally_put_function("quit"                        ,WFUNC_SYNC, c_cleanupWally, 0);
   wally_put_function(PLUGIN_SCOPE"::quit"          ,WFUNC_SYNC, c_cleanupWally, 0);
   wally_put_function(PLUGIN_SCOPE"::debug"         ,WFUNC_SYNC, dumpDebug, 0);
   wally_put_function(PLUGIN_SCOPE"::sleep"         ,WFUNC_SYNC, wally_sleep, 0);
   wally_put_function(PLUGIN_SCOPE"::loadPlugins"   ,WFUNC_SYNC, pluginLoader, 0);
   wally_put_function(PLUGIN_SCOPE"::callback"      ,WFUNC_SYNC, wally_registerCallback, 0);

//   js_initSysPlugin(ctx);

  return PLUGIN_SCOPE;
}


#endif
