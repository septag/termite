#include "stengine/core.h"
#include "bxx/path.h"

static void uvSignalInt(uv_signal_t* handle, int signum)
{
    st::coreShutdown();
}

int main(int argc, char* argv[])
{
    uv_signal_t shutdownSignal;

    bx::enableLogToFileHandle(stdout, stderr);
    
    st::coreConfig conf;
    bx::Path pluginPath(argv[0]);
    conf.updateInterval = 10;

    strcpy(conf.pluginPath, pluginPath.getDirectory().cstr());

    if (st::coreInit(conf, nullptr)) {
        BX_FATAL(st::errGetString());
        BX_VERBOSE(st::errGetCallstack());
        st::coreShutdown();
        return -1;
    }

    uv_signal_init(st::coreGetMainLoop(), &shutdownSignal);
    uv_signal_start(&shutdownSignal, uvSignalInt, SIGINT);

    st::coreRun();

    return 0;
}