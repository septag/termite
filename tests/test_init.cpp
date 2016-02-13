#include "stengine/core.h"
#include "bxx/path.h"
#include "bxx/logger.h"

#include <conio.h>

int main(int argc, char* argv[])
{
    bx::enableLogToFileHandle(stdout, stderr);
    
    st::coreConfig conf;
    bx::Path pluginPath(argv[0]);

    strcpy(conf.pluginPath, pluginPath.getDirectory().cstr());

    if (st::coreInit(conf, nullptr)) {
        BX_FATAL(st::errGetString());
        BX_VERBOSE(st::errGetCallstack());
        st::coreShutdown();
        return -1;
    }

    BX_TRACE("");
    puts("Press ESC to quit ...");
    while (true) {
        if (_kbhit()) {
            if (_getch() == 27)
                break;
        }

        st::coreFrame();
    }

    st::coreShutdown();

    return 0;
}