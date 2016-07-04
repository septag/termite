#include "termite/core.h"
#include "bxx/path.h"
#include "bxx/logger.h"

#include <conio.h>
#include "termite/job_dispatcher.h"
#include "bx/thread.h"
#include "bxx/hash_table.h"

using namespace termite;

static void subJobCallback1(int jobIndex, void* userParam)
{
    printf("SUB_JOB1 - %d (Thread: %u)\n", jobIndex, bx::getTid());
    bx::sleep(500);
    printf("SUB_JOB1_END - %d (Thread: %u)\n", jobIndex, bx::getTid());
}

static void jobCallback2(int jobIndex, void* userParam)
{
    printf("JOB2 - %d (Thread: %u)\n", jobIndex, bx::getTid());

    jobDesc jobs[] = {
        jobDesc(subJobCallback1),
        jobDesc(subJobCallback1)
    };
    JobHandle handle = dispatchSmallJobs(jobs, 2);
    waitJobs(handle);
    printf("JOB2_END - %d (Thread: %u)\n", jobIndex, bx::getTid());
}

static void jobCallback1(int jobIndex, void* userParam)
{
    printf("JOB1 - %d (Thread: %u)\n", jobIndex, bx::getTid());
    bx::sleep(1000);
    printf("JOB1_END - %d (Thread: %u)\n", jobIndex, bx::getTid());
}

int main(int argc, char* argv[])
{
    bx::enableLogToFileHandle(stdout, stderr);
    
    termite::Config conf;
    bx::Path pluginPath(argv[0]);
    strcpy(conf.rendererName, "[none]");

    strcpy(conf.pluginPath, pluginPath.getDirectory().cstr());

    if (termite::initialize(conf, nullptr)) {
        BX_FATAL(termite::getErrorString());
        BX_VERBOSE(termite::getErrorCallstack());
        termite::shutdown();
        return -1;
    }

    BX_TRACE("");
    puts("Press ESC to quit ...");

    // start some jobs
    const jobDesc jobs[] = {
        jobDesc(jobCallback1),
        jobDesc(jobCallback1),
        jobDesc(jobCallback2)
    };

    JobHandle handle = dispatchSmallJobs(jobs, 3);
    waitJobs(handle);

    while (true) {
        if (_kbhit()) {
            if (_getch() == 27)
                break;
        }

        termite::doFrame();
    }

    termite::shutdown();

    return 0;
}