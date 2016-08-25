#ifndef WALLY_PLUGIN_H
#define WALLY_PLUGIN_H

#define _GNU_SOURCE

#include <stdbool.h>
#include <stdio.h>
#include <pthread.h>
#include "default.h"
#include "hashtable.h"
#include <duktape.h>

#define CALL_TYPE_PTR  0
#define CALL_TYPE_STR  1
#define CALL_TYPE_PS   2
#define CALL_TYPE_CTX  3
#define CALL_TYPE_NULL 4
#define CALL_TYPE_PSA 4
#define WALLY_EVENT (SDL_USEREVENT+5)
#define WALLY_CALL_PTR  (WALLY_EVENT+CALL_TYPE_PTR)
#define WALLY_CALL_STR  (WALLY_EVENT+CALL_TYPE_STR)
#define WALLY_CALL_PS   (WALLY_EVENT+CALL_TYPE_PS)
#define WALLY_CALL_PSA  (WALLY_EVENT+CALL_TYPE_PSA)
#define WALLY_CALL_CTX  (WALLY_EVENT+CALL_TYPE_CTX)
#define WALLY_CALL_NULL (WALLY_EVENT+CALL_TYPE_NULL)
#define WFUNC_THRD true
#define WFUNC_SYNC false

typedef struct{
    // SDL Image / Screen Stuff
    bool SDL;
    void *sdl_tid;
    void *window;
    void *glcontext;
    void *renderer;
    void **texturePrio;
    hash_table *baseTextures;
    void *tempTexture;
    void *guiTexture;
    bool autorender;
    int width;
    int height;
    bool broadcomInit;
    pthread_t videoThread;
    bool playVideo;
    bool disableAudio;
    bool disableVideo;
    bool disableVideoPQ;
    bool disableVideoDisplay;
    bool disableVideoAfterFinish;

    duk_context *ctx;

    // The global configuration and flags
    int loglevel;
    bool daemonizing;
    bool logfile;
    FILE *logfileHandle;
    void *logCat;

    // SSDP / cloud Client
    bool registered;
    char *basedir;
    char *location;
    char *uuid;
    bool ssdp;
    bool cloud;
    bool clientThreadRunning;
    bool ssdpRunning;
    int threadDelay;
    pthread_t wallyClientThread;
    pthread_t mainThread;
    
    bool VFSOpen;
    char *VFSName;
    long VFSSize;

    // Plugin system
    hash_table *callbacks;
    hash_table *thr_functions;
    hash_table *functionWaitConditions;
    hash_table *functions;
    hash_table *plugins;
    hash_table *fonts;
    hash_table *colors;
    hash_table *configMap;
    hash_table *configFlagsMap;

    void *funcMutex;
    bool pluginLoaderDone;
    int pluginCount;

    int uiAllCount;
    int uiOwnCount;
    int callCount;
    int textureCount;
    int eventDelay;
    int uiEventTimeout;
    int conditionTimeout;

} pluginHandler;

extern pluginHandler *ph;
typedef int (*wally_c_function)(char *parameter);
typedef struct function_list_entry function_list_entry;
struct function_list_entry {
        const char *name;
        int threaded;
        wally_c_function value;
        int nargs;
};

int pluginLoader(char *path);
bool exportSync(const char *, void *);
bool exportThreaded(const char *, void *);
bool callWithData(char *name, void *ret, void *params);
bool callWithString(char *name, void *ret, char *paramString);
bool call(char *name, void *ret, char *paramString);
bool callNonBlocking(char *funcname, int *ret, void *params);
int sendWallyCommand(char *cmd, char *log);
int cleanupPlugins(void);
bool callEx(char *, void *, void *,int ,bool );
void export_function_list(char *scope, const function_list_entry *funcs);
void wally_put_function_list(const function_list_entry *);

#endif
