#pragma once

#include "bx/platform.h"

#if BX_PLATFORM_WINDOWS || BX_PLATFORM_ANDROID || BX_PLATFORM_IOS
#   define  TERM_RESET          ""
#   define  TERM_RED            ""
#   define  TERM_YELLOW         ""
#   define  TERM_CYAN           ""
#   define  TERM_RED_BOLD       ""
#   define  TERM_YELLOW_BOLD    ""
#   define  TERM_CYAN_BOLD      ""
#   define  TERM_DIM            ""
#   define  TERM_GREEN          ""
#   define  TERM_GREEN_BOLD     ""
#   define  TERM_MAGENTA        ""
#   define  TERM_WHITE          ""
#   define  TERM_BLACK          ""
#else
#   define  TERM_RESET          "\033[0m"
#   define  TERM_RED            "\033[31m"
#   define  TERM_YELLOW         "\033[33m"
#   define  TERM_CYAN           "\033[36m"
#   define  TERM_GREEN          "\033[32m"
#   define  TERM_RED_BOLD       "\033[1m\033[31m"
#   define  TERM_YELLOW_BOLD    "\033[1m\033[33m"
#   define  TERM_CYAN_BOLD      "\033[1m\033[36m"
#   define  TERM_GREEN_BOLD     "\033[1m\033[32m"
#   define  TERM_DIM            "\033[2m"
#   define  TERM_MAGENTA        "\033[35m"
#   define  TERM_WHITE          "\033[37m"
#   define  TERM_BLACK          "\033[30m"
#endif
