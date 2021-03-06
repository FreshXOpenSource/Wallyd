#include "../lib/plugins.h"
#include "../lib/util.h"
#include <stdbool.h>

#define PLUGIN_SCOPE "zbar"

int getCode(char *filename)
{
    return 0;
}

char *initPlugin(pluginHandler *_ph){
    slog(DEBUG,FULLDEBUG,"Plugin "PLUGIN_SCOPE" initializing");
    exportSync(PLUGIN_SCOPE"::getCode",(*filename));
    slog(DEBUG,FULLDEBUG,"Plugin initialized. PH is at 0x%x",_ph);
    return PLUGIN_SCOPE;
}
