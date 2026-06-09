/*
 * microrl configuration for the Bandoneo project.
 *
 * This file overrides third_party/microrl/src/config.h. It is force-included
 * into microrl.c by CMake (see target's COMPILE_FLAGS), and its include guard
 * matches the one in the upstream config so that the upstream file is skipped
 * when microrl.h pulls in "config.h".
 */
#ifndef _MICRORL_CONFIG_H_
#define _MICRORL_CONFIG_H_

#define _COMMAND_LINE_LEN (1+100)
#define _COMMAND_TOKEN_NMB 8
#define _PROMPT_DEFAULT "\033[32mIRin >\033[0m "
#define _PROMPT_LEN 7
#define _USE_COMPLETE
#define _USE_HISTORY
#define _RING_HISTORY_LEN 64
#define _USE_ESC_SEQ
#define _USE_LIBC_STDIO
#define _USE_CTLR_C
#define _ENABLE_INIT_PROMPT
#define _ENDL_CR
#define ENDL "\r"

#endif
