#define DWALLYD
#include "wallyd.h"
#include <uv.h>

uv_fs_t openReq;
uv_fs_t readReq;
uv_fs_t closeReq;
uv_pipe_t server;
uv_tcp_t tcp;
uv_loop_t loop;

int gargc;
char *gargv[2];
static char *startupScript = WALLYD_CONFDIR"/wallyd.d";
static char *pluginFolder  = INSTALL_PREFIX"/lib/wallyd";

int main(int argc, char *argv[]) 
{
    int ret;
    char *cs;

    // Make argv[0] globally available
    gargc = argc;
    gargv[0] = strdup(argv[0]);
    gargv[1] = startupScript;

    // read startup parameters
    readOptions(argc,argv);

    // init the core (variables, log system, plugin system, duktape core)
    utilInit(DEFAULT_LOG_LEVEL, LOG_ALL,0);
    //utilInit(DEFAULT_LOG_LEVEL, LOG_ALL ^ LOG_DUMMY ^ LOG_SDL ^ LOG_PLUGIN, 0);
    //utilInit(DEFAULT_LOG_LEVEL, LOG_PLUGIN, 0);

    slog(INFO,LOG_CORE,"Wally Image Server R%u (Build %u) starting.",BUILD_NUMBER,BUILD_DATE);

    // Initialize duktape thread but wait for plugins to be ready
    pthread_mutex_lock(&core_ready_mutex);
    if(pthread_create(&ph->uv_thr, NULL, &duvThread, ph) != 0){
       slog(ERROR,LOG_CORE,"Failed to create seaduk thread!");
    }

    // assing signal handlers for ctrl+c
    setupSignalHandler();

    // read and set config
    initializeConfig();

    // daemonize if set
    daemonize(ph->daemonizing);

    // init SDL2
    if(!sdlInit()){
        exit(1);
    }

    //ctx = duk_create_heap(NULL, NULL, NULL, &loop, (duk_fatal_function)my_duk_fatal);
    //ph->ctx = duk_create_heap(NULL, NULL, NULL, &loop,NULL);
    //if (!ph->ctx) {
    //  slog(ERROR,LOG_CORE, "Problem initiailizing duktape heap");
    //  return -1;
    //}

    if(ht_get_simple(ph->configMap,"basedir") != NULL) {
      ph->basedir=ht_get_simple(ph->configMap,"basedir");
    } else {
      asprintf(&ph->basedir,".");
    } 

    // export system plugin functions
    initSysPlugin();

    // load plugins
    if(ht_get_simple(ph->configMap,"plugins") != NULL) {
        asprintf(&cs,"%s",ht_get_simple(ph->configMap,"plugins"));
        callSync("sys::loadPlugins",&ret,cs);
        //scall("sys::loadPlugins %s",ht_get_simple(ph->configMap,"plugins"));
        free(cs);
    } else {
        callSync("sys::loadPlugins",&ret,pluginFolder);
    }

    if(ph->loglevel > DEBUG){
        hashtable_dumpkeys(ph->functions,"Exported sync commands : ");
        hashtable_dumpkeys(ph->thr_functions,"Exported async commands : ");
    }

    // Signal the duktape core to start
    pthread_cond_signal(&core_ready_condition);
    //pthread_mutex_unlock(&core_ready_mutex);

   slog(DEBUG,LOG_CORE,"PH Size : %d, WTX Size : %d",sizeof(pluginHandler), sizeof(wally_call_ctx));
   // remove old socket
    ret = unlink(FIFO);
    if(ret == 0){
        slog(INFO,LOG_CORE,"Old FIFO found and removed.");
    }

   //slog(DEBUG,LOG_CORE,"Seaduk initializing, ctx is at 0x%x.",ph->ctx);
   //if(pthread_create(&ph->uv_thr, NULL, &duvThread, ph) != 0){
   //    slog(ERROR,LOG_CORE,"Failed to create seaduk thread!");
   //}

   // Loop the SDL stuff in the main thread
   uiLoop();
   uv_loop_close(&loop);
   // TODO : Free this
   //void *pret;
   //if(pthread_join(ph->uv_thr,&pret) == 0){
   //   free(ph->uv_thr);
   //}
}

void allocBuffer(uv_handle_t* handle, size_t size, uv_buf_t* buf) {
      buf->base = malloc(size);
      buf->len = size;
      memset(buf->base,0,size);
}

void onClose(uv_handle_t* handle){
    slog(DEBUG,LOG_CORE,"Handle closed. Freeing up.");
    if(handle) { free(handle); }
}

void onNewConnection(uv_stream_t *server, int status)
{
    slog(DEBUG,LOG_CORE,"New connection on socket "FIFO);
    if (status == -1) {
        slog(ERROR,LOG_CORE,"New connection error on socket "FIFO);
        return;
    }
    uv_tcp_t client; //= (uv_tcp_t*) malloc(sizeof(uv_tcp_t));
    uv_tcp_init(&loop, &client);
    if (uv_accept(server, (uv_stream_t*) &client) == 0) {
        uv_read_start((uv_stream_t*) &client, allocBuffer, onRead);
    }
    else {
        uv_close((uv_handle_t*) &client, NULL);
    }
}

void onRead(uv_stream_t* _client, ssize_t nread, const uv_buf_t* buffer) {
    slog(DEBUG,LOG_CORE,"onRead");
    if (nread == -1) {
        slog(DEBUG,LOG_CORE, "Reached EOF on command socket");
        uv_close((uv_handle_t *) _client, onClose);
        return;
    }
    // TODO : Find out why this happens? (nread == -4095)
    if (strlen(buffer->base) == 0 ) {
        free(buffer->base);
        return;
    }
    slog(DEBUG,LOG_CORE,"Read (%u/%u bytes) : %s",nread,strlen(buffer->base),buffer->base);
    if(nread > 1){
        processCommand(buffer->base);
    }
    free(buffer->base);
    slog(DEBUG,LOG_CORE,"Closing handle");
    uv_close((uv_handle_t*) _client, onClose);
}

bool processCommand(char *buf)
{
    int ret;
    int validCmd = 0;
    bool nextLine = true;
    char *lineBreak, *spaceBreak;
    char *lineCopy = NULL;
    char *cmd = strtok_r(buf,"\n",&lineBreak);
    while( nextLine ){
        // TODO : Keep track of this and clean it up!
        unsigned long cmdLen = strlen(cmd);
        lineCopy = malloc(cmdLen+1);
        memset(lineCopy, 0, cmdLen+1);
        strncpy(lineCopy,cmd,cmdLen);
        slog(DEBUG,LOG_CORE,"Processing line (%d) : %s",cmdLen,lineCopy);
        // NOTE : strtok changes strlen of cmd, so we save its length before
        if(cmd[0] != '#') {
            char *myCmd = strtok_r(lineCopy, " ", &spaceBreak);
            char *params=NULL;
            if(cmdLen > strlen(myCmd)){
                params = lineCopy+strlen(myCmd)+1;
            }
            slog(DEBUG,LOG_CORE,"Command split into(%d) : %s(%s)",strlen(myCmd), myCmd, params);
            if(callSync(myCmd,&ret, params)){
                validCmd++;
            }
        } else {
            slog(DEBUG,LOG_CORE,"Ignoring comment line");
        }
        cmd = strtok_r(NULL,"\n",&lineBreak);
        if(cmd == NULL) nextLine=false;
        free(lineCopy);
    }
    slog(DEBUG,LOG_CORE,"Command stack executed.");
    return validCmd;
}

void initializeFlags(void){
   slog(DEBUG,LOG_CORE,"Initializing flags file");
   // if /etc/wally.conf has h/w defined dont set DEFAULT_W/H
   if(getConfig(ph->configFlagsMap,ETC_FLAGS) == 0){
      slog(DEBUG,LOG_CORE,"Trying to open "ETC_FLAGS);
      if(getConfig(ph->configFlagsMap,ETC_FLAGS_BAK) == 0){
        slog(INFO,LOG_CORE,"Configfile in "ETC_FLAGS" nor "ETC_FLAGS_BAK" not found! Using default values.");
        ph->width =  DEFAULT_WINDOW_WIDTH;
        ph->height = DEFAULT_WINDOW_HEIGHT;
      }
  } 

  // params from /tmp/flags can still override w/h from wally.conf
  if(ht_contains_simple(ph->configFlagsMap,"W_WIDTH")){
      ph->width = atoi(ht_get_simple(ph->configFlagsMap,"W_WIDTH"));
  } else {
      ph->width = ph->width ? ph->width : DEFAULT_WINDOW_WIDTH;
  }
  if(ht_get_simple(ph->configFlagsMap,"W_HEIGHT")){
      ph->height = atoi(ht_get_simple(ph->configFlagsMap,"W_HEIGHT"));
  } else {
      ph->height = ph->height ? ph->height : DEFAULT_WINDOW_HEIGHT;
  }
  slog(DEBUG,LOG_CORE,"W = %d / H = %d",ph->width,ph->height);
  if(!ht_contains_simple(ph->configFlagsMap,"W_MAC")){
     slog(WARN,LOG_CORE,"No MAC address found in configs/flags. Can not determine uuid for this device");
     ht_insert_simple(ph->configFlagsMap,"W_MAC",DEFAULT_MAC);
     slog(DEBUG,LOG_CORE,"Setting MAC to "DEFAULT_MAC);
     ph->uuid = DEFAULT_MAC;
  }
  ph->uuid = replace(ht_get_simple(ph->configFlagsMap,"W_MAC"),":","");
  slog(DEBUG,LOG_CORE,"UUID is : %s",ph->uuid);

  if(ht_contains_simple(ph->configFlagsMap,"W_CONNECT")){
      slog(DEBUG,LOG_CORE,"Connectivity type is : %s",ht_get_simple(ph->configFlagsMap,"W_CONNECT"));
      if(strncmp(ht_get_simple(ph->configFlagsMap,"W_CONNECT"),"ssdp",4)){
        ph->ssdp = true; 
      }
      if(strncmp(ht_get_simple(ph->configFlagsMap,"W_CONNECT"),"cloud",5)){
        ph->cloud = true; 
      }
      if(strncmp(ht_get_simple(ph->configFlagsMap,"W_CONNECT"),"manual",6)){
        if(!ht_contains_simple(ph->configFlagsMap,"W_SERVER")){
          slog(ERROR,LOG_CORE,"Connectivity type set to manual but no server defined");
        }
      }
  }
}

void initializeConfig(void){
  initializeFlags();

  //ph->configMap=map_create();
  if(getConfig(ph->configMap,WALLYD_CONFIG) > 0){
    if(ht_contains_simple(ph->configMap,"threadDelay")) { 
      ph->threadDelay = atoi(ht_get_simple(ph->configFlagsMap,"threadDelay"));
    }
    if(ht_contains_simple(ph->configMap,"sdldebug") && strncmp(ht_get_simple(ph->configMap,"sdldebug"),"true",4) == 0) { 
	ph->sdldebug = true; 
        slog(DEBUG,LOG_SDL,"SDL Debug Log enabled.");
    } else {
	ph->sdldebug = false; 
    }
    if(ht_contains_simple(ph->configMap,"raspberry") && strncmp(ht_get_simple(ph->configMap,"raspberry"),"true",4) == 0) { 
	ph->broadcomInit = true; 
        slog(DEBUG,LOG_PI,"BCM Chip support enabled.");
    } else {
	ph->broadcomInit = false; 
    }
    if(ht_contains_simple(ph->configMap,"disableAudio") && strncmp(ht_get_simple(ph->configMap,"disableAudio"),"true",4) == 0) { 
	ph->disableAudio = true; 
        slog(DEBUG,LOG_VIDEO,"Audio stream part of video playing disabled.");
    }
    if(ht_contains_simple(ph->configMap,"disableVideo") && strncmp(ht_get_simple(ph->configMap,"disableVideo"),"true",4) == 0) { 
	ph->disableVideo = true; 
        slog(DEBUG,LOG_VIDEO,"Video stream part of video playing disabled.");
    }
    if(ht_contains_simple(ph->configMap,"disableVideoPQ") && strncmp(ht_get_simple(ph->configMap,"disableVideoPQ"),"true",4) == 0) { 
	ph->disableVideoPQ = true; 
        slog(DEBUG,LOG_VIDEO,"Video stream Queue of video playing disabled.");
    }
    if(ht_contains_simple(ph->configMap,"disableVideoDisplay") && strncmp(ht_get_simple(ph->configMap,"disableVideoDisplay"),"true",4) == 0) { 
	ph->disableVideoDisplay = true; 
        slog(DEBUG,LOG_VIDEO,"Video stream display of video playing disabled.");
    }
    if(ht_get_simple(ph->configMap,"foreground") != NULL && strncmp(ht_get_simple(ph->configMap,"foreground"),"true",4) == 0) { 
	ph->daemonizing = false; 
    }
    if(ht_get_simple(ph->configMap,"ssdp") != NULL && strncmp(ht_get_simple(ph->configMap,"ssdp"),"true",4) == 0) { 
	ph->ssdp = true; 
    }
    if(ht_get_simple(ph->configMap,"novsync") != NULL && strncmp(ht_get_simple(ph->configMap,"novsync"),"true",4) == 0) { 
	ph->vsync = false; 
    }
    if(ht_get_simple(ph->configMap,"logfile") != NULL) { 
        ph->logfileHandle = openLogfile(ht_get_simple(ph->configMap,"logfile"));
    }
    if(ht_get_simple(ph->configMap,"debug") != NULL) { 
        ph->loglevel = atoi(ht_get_simple(ph->configMap,"debug"));
	slog(DEBUG,LOG_CORE,"Set loglevel to : %d",ph->loglevel);
    }
    if(ht_get_simple(ph->configMap,"width") != NULL) { 
        ph->width = atoi(ht_get_simple(ph->configMap,"width"));
	slog(DEBUG,LOG_SDL,"Set Window width to : %d",ph->width);
    }
    if(ht_get_simple(ph->configMap,"height") != NULL) { 
        ph->height = atoi(ht_get_simple(ph->configMap,"height"));
	slog(DEBUG,LOG_SDL,"Set Window height to : %d",ph->height);
    }
  } else {
    slog(WARN,LOG_CORE,"Configfile "ETC_FLAGS" not found. Using defaults.");
  }
}

void readOptions(int argc, char **argv){
    char *cvalue = NULL;
    int c;
    while ((c = getopt (argc, argv, "fhc:s:")) != -1)
        switch (c){
            case 'h':
                printf("Usage: wallyd [-h|-f|-s <startscript>|-c <configfile>]\n");
                printf("\t-h : this help\n");
                printf("\t-f : run in foreground\n");
                printf("\t-c : use <configfile> (default "WALLYD_CONFIG")\n");
                printf("\t-s : run <startscript> (default "WALLYD_CONFDIR"/wallyd.startup)\n");
                exit(0);
                break;
            case 'f':
                slog(INFO,LOG_CORE,"Running in foreground");
                ph->daemonizing = false;
                break;
            case 's':
                startupScript = optarg;
                slog(INFO,LOG_CORE,"Using startscript %s",startupScript);
                break;
            case 'c':
                cvalue = optarg;
                slog(INFO,LOG_CORE,"Using config %s",cvalue);
                break;
            case '?':
              if (optopt == 'c')
                fprintf (stderr, "Option -%c requires an argument.\n", optopt);
              else if (isprint (optopt))
                fprintf (stderr, "Unknown option `-%c'.\n", optopt);
              else
                fprintf (stderr,
                         "Unknown option character `\\x%x'.\n",
                         optopt);
              return;
            default:
              abort();
        }
}

void processStartupScript(char *file){
  slog(DEBUG,LOG_CORE,"Reading wallyd.startup script : %s",file);
  long fsize=0;
  char *cmds=NULL;

  FILE *f = fopen(file, "rb");
  if(!f){
      slog(DEBUG,LOG_CORE,"File not found. Not running any startup commands");
      return;
  }

  fseek(f, 0, SEEK_END);
  fsize = ftell(f);
  fseek(f, 0, SEEK_SET);

  cmds = malloc(fsize + 1);
  fread(cmds, fsize, 1, f);
  fclose(f);

  cmds[fsize] = 0;
  slog(DEBUG,LOG_CORE,"Processing %d bytes from startupScript",fsize);
  processCommand(cmds);
  free(cmds);
}



