#include "interpreter.h"
#include "picoc.h"
#include "scriptconsole_c.h"

#ifdef USE_READLINE
#include <readline/history.h>
#include <readline/readline.h>
#endif

#ifdef DEBUGGER
static int gEnableDebugger = true;
#else
static int gEnableDebugger = false;
#endif
void PlatformInit(Picoc* pc) {}

void PlatformCleanup(Picoc* pc) {}

char* PlatformGetLine(char* Buf, int MaxLen, const char* Prompt)
{
#ifdef USE_READLINE
    if (Prompt != NULL) {

        char* InLine = readline(Prompt);
        if (InLine == NULL) {
            return NULL;
        }

        Buf[MaxLen - 1] = '\0';
        strncpy(Buf, InLine, MaxLen - 2);
        strncat(Buf, "\n", MaxLen - 2);

        if (InLine[0] != '\0') {
            add_history(InLine);
        }

        free(InLine);
        return Buf;
    }
#endif

    if (Prompt != NULL) {
        printf("%s", Prompt);
    }

    fflush(stdout);
    return fgets(Buf, MaxLen, stdin);
}

int PlatformGetCharacter()
{
    fflush(stdout);
    return getchar();
}

void PlatformPutc(unsigned char OutCh, union OutputStreamInfo* Stream)
{

    char ch = (char)OutCh;
    ckpt_console_write(&ch, 1);
}

char* PlatformReadFile(Picoc* pc, const char* FileName)
{
    struct stat FileInfo;
    char* ReadText;
    FILE* InFile;
    int BytesRead;
    char* p;

    if (stat(FileName, &FileInfo)) {
        ProgramFailNoParser(pc, "can't read file %s\n", FileName);
    }

    ReadText = malloc(FileInfo.st_size + 1);
    if (ReadText == NULL) {
        ProgramFailNoParser(pc, "out of memory\n");
    }

    InFile = fopen(FileName, "r");
    if (InFile == NULL) {
        ProgramFailNoParser(pc, "can't read file %s\n", FileName);
    }

    BytesRead = fread(ReadText, 1, FileInfo.st_size, InFile);
    if (BytesRead == 0) {
        ProgramFailNoParser(pc, "can't read file %s\n", FileName);
    }

    ReadText[BytesRead] = '\0';
    fclose(InFile);

    if ((ReadText[0] == '#') && (ReadText[1] == '!')) {
        for (p = ReadText; (*p != '\0') && (*p != '\r') && (*p != '\n'); ++p) {
            *p = ' ';
        }
    }

    return ReadText;
}

void PicocPlatformScanFile(Picoc* pc, const char* FileName)
{
    char* SourceStr = PlatformReadFile(pc, FileName);

    if (SourceStr != NULL && SourceStr[0] == '#' && SourceStr[1] == '!') {
        SourceStr[0] = '/';
        SourceStr[1] = '/';
    }

    PicocParse(pc, FileName, SourceStr, strlen(SourceStr), true, false, true, gEnableDebugger);
}

void PlatformExit(Picoc* pc, int RetVal)
{
    pc->PicocExitValue = RetVal;
    longjmp(pc->PicocExitBuf, 1);
}
