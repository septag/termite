#pragma once

#include "bx/bx.h"
#include <cstdlib>

namespace bx
{
    static void generateUUID(char _uuid[37])
    {
        int t = 0;
        char* temp = "xxxxxxxx-xxxx-4xxx-yxxx-xxxxxxxxxxxx";
        char* hex = "0123456789ABCDEF-";
        int len = (int)strlen(temp);

        for (t = 0; t < len + 1; t++) {
            int r = rand() % 16;
            char c = ' ';

            switch (temp[t]) {
            case 'x': { c = hex[r]; } break;
            case 'y': { c = hex[r & 0x03 | 0x08]; } break;
            case '-': { c = '-'; } break;
            case '4': { c = '4'; } break;
            }

            _uuid[t] = (t < len) ? c : 0x00;
        }
    }
}   // namespace: bx