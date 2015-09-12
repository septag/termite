#pragma once

#include <stdio.h>
#include <sys/select.h>
#include <sys/ioctl.h>
#include <termios.h>

#ifndef __APPLE__
#   include <stropts.h>
#endif

inline int _kbhit()
{
    static const int STDIN = 0;
    static bool initialized = false;

    if (!initialized) {
        // Use termios to turn off line buffering
        termios term;
        tcgetattr(STDIN, &term);
        term.c_lflag &= ~ICANON;
        tcsetattr(STDIN, TCSANOW, &term);
        setbuf(stdin, NULL);
        initialized = true;
    }

    int bytesWaiting;
    ioctl(STDIN, FIONREAD, &bytesWaiting);
    return bytesWaiting;
}

inline char _getch()
{
    static termios told, tnew;

    tcgetattr(0, &told); /* grab old terminal i/o settings */
    tnew = told; /* make new settings same as old settings */
    tnew.c_lflag &= ~ICANON; /* disable buffered i/o */
    tnew.c_lflag &= ~ECHO; /* set echo mode */
    tcsetattr(0, TCSANOW, &tnew); /* use these new terminal i/o settings now */

    char ch = getchar();

    tcsetattr(0, TCSANOW, &told);

    return ch;
}
