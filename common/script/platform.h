#ifndef PLATFORM_H
#define PLATFORM_H

#include <assert.h>
#include <ctype.h>
#include <math.h>
#include <setjmp.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>

#ifdef UNIX_HOST
#include <stdint.h>
#include <unistd.h>
#elif defined(WIN32)
#else
#error ***** A platform must be explicitly defined! *****
#endif

#if defined(WIN32)
#undef USE_READLINE
#endif

#if defined(__hppa__) || defined(__sparc__)

#define ALIGN_TYPE double
#else

#define ALIGN_TYPE void*
#endif

#define GLOBAL_TABLE_SIZE (97)
#define STRING_TABLE_SIZE (97)
#define STRING_LITERAL_TABLE_SIZE (97)
#define RESERVED_WORD_TABLE_SIZE (97)
#define PARAMETER_MAX (16)
#define LINEBUFFER_MAX (256)
#define LOCAL_TABLE_SIZE (11)
#define STRUCT_TABLE_SIZE (11)
#define GOTO_LABELS_TABLE_SIZE (5)

#define INTERACTIVE_PROMPT_START "starting picoc " PICOC_VERSION " (Ctrl+D to exit)\n"
#define INTERACTIVE_PROMPT_STATEMENT "picoc> "
#define INTERACTIVE_PROMPT_LINE "     > "

#endif
