#pragma once

#ifdef _WIN32
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
#endif
