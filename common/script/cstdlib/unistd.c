#include <fcntl.h>
#include <limits.h>

#include "interpreter.h"

static int ZeroValue = 0;

void UnistdAccess(
    struct ParseState* Parser, struct Value* ReturnValue, struct Value** Param, int NumArgs)
{
    ReturnValue->Val->Integer = access(Param[0]->Val->Pointer, Param[1]->Val->Integer);
}

void UnistdChdir(
    struct ParseState* Parser, struct Value* ReturnValue, struct Value** Param, int NumArgs)
{
    ReturnValue->Val->Integer = chdir(Param[0]->Val->Pointer);
}

void UnistdClose(
    struct ParseState* Parser, struct Value* ReturnValue, struct Value** Param, int NumArgs)
{
    ReturnValue->Val->Integer = close(Param[0]->Val->Integer);
}

void UnistdDup(
    struct ParseState* Parser, struct Value* ReturnValue, struct Value** Param, int NumArgs)
{
    ReturnValue->Val->Integer = dup(Param[0]->Val->Integer);
}

void UnistdDup2(
    struct ParseState* Parser, struct Value* ReturnValue, struct Value** Param, int NumArgs)
{
    ReturnValue->Val->Integer = dup2(Param[0]->Val->Integer, Param[1]->Val->Integer);
}

void UnistdFork(
    struct ParseState* Parser, struct Value* ReturnValue, struct Value** Param, int NumArgs)
{
    ReturnValue->Val->Integer = fork();
}

void UnistdFsync(
    struct ParseState* Parser, struct Value* ReturnValue, struct Value** Param, int NumArgs)
{
    ReturnValue->Val->Integer = fsync(Param[0]->Val->Integer);
}

void UnistdFtruncate(
    struct ParseState* Parser, struct Value* ReturnValue, struct Value** Param, int NumArgs)
{
    ReturnValue->Val->Integer = ftruncate(Param[0]->Val->Integer, Param[1]->Val->Integer);
}

void UnistdGetcwd(
    struct ParseState* Parser, struct Value* ReturnValue, struct Value** Param, int NumArgs)
{
    ReturnValue->Val->Pointer = getcwd(Param[0]->Val->Pointer, Param[1]->Val->Integer);
}

void UnistdGethostid(
    struct ParseState* Parser, struct Value* ReturnValue, struct Value** Param, int NumArgs)
{
    ReturnValue->Val->Integer = gethostid();
}

void UnistdGetpid(
    struct ParseState* Parser, struct Value* ReturnValue, struct Value** Param, int NumArgs)
{
    ReturnValue->Val->Integer = getpid();
}

void UnistdGetwd(
    struct ParseState* Parser, struct Value* ReturnValue, struct Value** Param, int NumArgs)
{
    ReturnValue->Val->Pointer = getcwd(Param[0]->Val->Pointer, PATH_MAX);
}

void UnistdIsatty(
    struct ParseState* Parser, struct Value* ReturnValue, struct Value** Param, int NumArgs)
{
    ReturnValue->Val->Integer = isatty(Param[0]->Val->Integer);
}

void UnistdLink(
    struct ParseState* Parser, struct Value* ReturnValue, struct Value** Param, int NumArgs)
{
    ReturnValue->Val->Integer = link(Param[0]->Val->Pointer, Param[1]->Val->Pointer);
}

void UnistdLseek(
    struct ParseState* Parser, struct Value* ReturnValue, struct Value** Param, int NumArgs)
{
    ReturnValue->Val->Integer =
        lseek(Param[0]->Val->Integer, Param[1]->Val->Integer, Param[2]->Val->Integer);
}

void UnistdRead(
    struct ParseState* Parser, struct Value* ReturnValue, struct Value** Param, int NumArgs)
{
    ReturnValue->Val->Integer =
        read(Param[0]->Val->Integer, Param[1]->Val->Pointer, Param[2]->Val->Integer);
}

void UnistdRmdir(
    struct ParseState* Parser, struct Value* ReturnValue, struct Value** Param, int NumArgs)
{
    ReturnValue->Val->Integer = rmdir(Param[0]->Val->Pointer);
}

void UnistdMkdir(
    struct ParseState* Parser, struct Value* ReturnValue, struct Value** Param, int NumArgs)
{
    ReturnValue->Val->Integer = mkdir(Param[0]->Val->Pointer, Param[1]->Val->Integer);
}

void UnistdSbrk(
    struct ParseState* Parser, struct Value* ReturnValue, struct Value** Param, int NumArgs)
{
    ReturnValue->Val->Pointer = sbrk(Param[0]->Val->Integer);
}

void UnistdSleep(
    struct ParseState* Parser, struct Value* ReturnValue, struct Value** Param, int NumArgs)
{
    ReturnValue->Val->Integer = sleep(Param[0]->Val->Integer);
}

void UnistdTruncate(
    struct ParseState* Parser, struct Value* ReturnValue, struct Value** Param, int NumArgs)
{
    ReturnValue->Val->Integer = truncate(Param[0]->Val->Pointer, Param[1]->Val->Integer);
}

void UnistdUnlink(
    struct ParseState* Parser, struct Value* ReturnValue, struct Value** Param, int NumArgs)
{
    ReturnValue->Val->Integer = unlink(Param[0]->Val->Pointer);
}

void UnistdUsleep(
    struct ParseState* Parser, struct Value* ReturnValue, struct Value** Param, int NumArgs)
{
    ReturnValue->Val->Integer = usleep(Param[0]->Val->Integer);
}

void UnistdWrite(
    struct ParseState* Parser, struct Value* ReturnValue, struct Value** Param, int NumArgs)
{
    ReturnValue->Val->Integer =
        write(Param[0]->Val->Integer, Param[1]->Val->Pointer, Param[2]->Val->Integer);
}

const char UnistdDefs[] = "\
typedef int uid_t; \
typedef int gid_t; \
typedef int pid_t; \
typedef int off_t; \
typedef int size_t; \
typedef int ssize_t; \
typedef int useconds_t;\
typedef int intptr_t;\
typedef int mode_t;\
";

struct LibraryFunction UnistdFunctions[] = {
    {UnistdAccess,    "int access(char *, int);"           },

    {UnistdChdir,     "int chdir(char *);"                 },

    {UnistdClose,     "int close(int);"                    },

    {UnistdDup,       "int dup(int);"                      },
    {UnistdDup2,      "int dup2(int, int);"                },

    {UnistdFork,      "pid_t fork(void);"                  },

    {UnistdFsync,     "int fsync(int);"                    },
    {UnistdFtruncate, "int ftruncate(int, off_t);"         },
    {UnistdGetcwd,    "char *getcwd(char *, size_t);"      },

    {UnistdGethostid, "long gethostid(void);"              },

    {UnistdGetpid,    "pid_t getpid(void);"                },

    {UnistdGetwd,     "char *getwd(char *);"               },
    {UnistdIsatty,    "int isatty(int);"                   },

    {UnistdLink,      "int link(char *, char *);"          },

    {UnistdLseek,     "off_t lseek(int, off_t, int);"      },

    {UnistdRead,      "ssize_t read(int, void *, size_t);" },

    {UnistdRmdir,     "int rmdir(char *);"                 },
    {UnistdMkdir,     "int mkdir(char *, mode_t);"         },
    {UnistdSbrk,      "void *sbrk(intptr_t);"              },

    {UnistdSleep,     "unsigned int sleep(unsigned int);"  },

    {UnistdTruncate,  "int truncate(char *, off_t);"       },

    {UnistdUnlink,    "int unlink(char *);"                },
    {UnistdUsleep,    "int usleep(useconds_t);"            },

    {UnistdWrite,     "ssize_t write(int, void *, size_t);"},
    {NULL,            NULL                                 }
};

extern char* optarg;
extern int optind, opterr, optopt;

void UnistdSetupFunc(Picoc* pc)
{

    if (!VariableDefined(pc, TableStrRegister(pc, "NULL")))
    {
        VariableDefinePlatformVar(
            pc, NULL, "NULL", &pc->IntType, (union AnyValue*)&ZeroValue, false);
    }

    VariableDefinePlatformVar(pc, NULL, "optarg", pc->CharPtrType, (union AnyValue*)&optarg, true);
    VariableDefinePlatformVar(pc, NULL, "optind", &pc->IntType, (union AnyValue*)&optind, true);
    VariableDefinePlatformVar(pc, NULL, "opterr", &pc->IntType, (union AnyValue*)&opterr, true);
    VariableDefinePlatformVar(pc, NULL, "optopt", &pc->IntType, (union AnyValue*)&optopt, true);
}
