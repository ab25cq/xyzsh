#include "config.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "xyzsh/xyzsh.h"

#ifdef MDEBUG

static FILE* gFile;
static BOOL gLogging = FALSE;

#include <unistd.h>
#include <time.h>
#include <fcntl.h>
#include <sys/time.h>
/*
struct timeval gTV;
char gTimerMsg[1024];

void timer_count_start(char* msg)
{
    gettimeofday(&gTV, NULL);
    strcpy(gTimerMsg, msg);
}

void timer_count_end(const char* file, int line, const char* fun)
{
    struct timeval tv;
    gettimeofday(&tv, NULL);

    FILE* f = fopen("timer.log", "a");
    fprintf(f, "%s:%d %s %ld micro sec %s\n", file, line, fun, tv.tv_usec - gTV.tv_usec, gTimerMsg);
    fclose(f);
}
*/

//////////////////////////////////////////////////////////////////////
// メモリーリークチェック用定義
//////////////////////////////////////////////////////////////////////
#define NAME_SIZE 32

typedef struct _sMallocEntry
{
    void* mMemory;

    char mFileName[NAME_SIZE];
    int mLine;
    char mFuncName[NAME_SIZE];

    struct _sMallocEntry* mNextEntry;
} sMallocEntry;
      
#define ARRAY_SIZE 65535
static sMallocEntry* gMallocEntries[ARRAY_SIZE];

void release_entry(void* memory, const char* file_name, int line, const char* func_name)
{
    sMallocEntry* entry;
#ifdef __64bit__
    unsigned long long hash = ((unsigned long long)memory) % ARRAY_SIZE;
#else
    unsigned long hash = ((unsigned long )memory) % ARRAY_SIZE;
#endif
    
    entry = gMallocEntries[hash];
    if(entry->mMemory == memory) { 
        sMallocEntry* next_entry = entry->mNextEntry;
if(gLogging) fprintf(gFile, "mem %p freed for debug info\n", entry);
        free(entry);
        gMallocEntries[hash] = next_entry;
        return ;
    }
    else {
        while(entry->mNextEntry) {
            if(entry->mNextEntry->mMemory == memory) {
                sMallocEntry* next_entry = entry->mNextEntry->mNextEntry;
if(gLogging) fprintf(gFile, "mem %p freed for debug info\n", entry->mNextEntry);
                free(entry->mNextEntry);
                entry->mNextEntry = next_entry;

                return;
            }
            entry = entry->mNextEntry;
        }
    }

#ifdef __64bit__
    fprintf(stderr, "\tinvalid free at file: %s line:%d function:%s() addr:%llx\n", entry->mFileName, entry->mLine, entry->mFuncName, (unsigned long long)entry->mMemory);
#else
    fprintf(stderr, "\tinvalid free at file: %s line:%d function:%s() addr:%lx\n", entry->mFileName, entry->mLine, entry->mFuncName, (unsigned long)entry->mMemory);
#endif
}

//////////////////////////////////////////////////////////////////////
// メモリーリークチェック用開始
//////////////////////////////////////////////////////////////////////
void CheckMemLeak_Begin(BOOL log)
{
    if(log) {
        gLogging = TRUE;
        gFile = fopen("memory.log", "w");

    }
    memset(gMallocEntries, 0, sizeof(sMallocEntry*)*ARRAY_SIZE);
}

//////////////////////////////////////////////////////////////////////
// メモリーリークチェック用解放
//////////////////////////////////////////////////////////////////////
void CheckMemLeak_End()
{
    int i;

    fprintf(stderr, "Detecting memory leak...\n");
//FILE* f = fopen("memleak.log", "w");
    for(i=0; i<ARRAY_SIZE; i++) {
        sMallocEntry* entry = gMallocEntries[i];
      
        while(entry) {
#ifdef __64bit__
            fprintf(stderr, "\tDetected!! at file: %s line:%d function:%s() addr:%llx\n"
                                             , entry->mFileName, entry->mLine
                                             , entry->mFuncName, (unsigned long long)entry->mMemory);
#else
            fprintf(stderr, "\tDetected!! at file: %s line:%d function:%s() addr:%lx\n"
                                             , entry->mFileName, entry->mLine
                                             , entry->mFuncName, (unsigned long)entry->mMemory);
#endif
            //fprintf(f, "\tDetected!! at file: %s line:%d function:%s() addr:%x\n"
                                             //, entry->mFileName, entry->mLine
                                             //, entry->mFuncName, (unsigned int)entry->mMemory);
            entry = entry->mNextEntry;
        }
    }

//fclose(f);

    fprintf(stderr, "done.\n");
}

//////////////////////////////////////////////////////////////////////
// メモリーリークチェック用メモリーアロケート
//////////////////////////////////////////////////////////////////////
ALLOC void* CheckMemLeak_Malloc(size_t size, const char* file_name, int line, const char* func_name)
{
    sMallocEntry* entry;
    int i;
    int hash;
    
    entry = (sMallocEntry*)malloc(sizeof(sMallocEntry));
if(gLogging) fprintf(gFile, "mem %p malloced for debug info\n", entry);

    xstrncpy(entry->mFileName, (char*)file_name, NAME_SIZE);
    entry->mLine = line;
    xstrncpy(entry->mFuncName, (char*)func_name, NAME_SIZE);
    entry->mMemory = malloc(size);
   
#ifdef __64bit__
    hash = (unsigned long long)entry->mMemory % ARRAY_SIZE;
#else
    hash = (unsigned long)entry->mMemory % ARRAY_SIZE;
#endif
    entry->mNextEntry = gMallocEntries[hash];
    gMallocEntries[hash] = entry;

if(gLogging) fprintf(gFile, "mem %p malloced at file %s line %d fun %s\n", entry->mMemory, file_name, line, func_name);
    return entry->mMemory;
}

//////////////////////////////////////////////////////////////////////
// メモリーリークチェック用メモリーアロケート
//////////////////////////////////////////////////////////////////////
ALLOC void* CheckMemLeak_Realloc(void* memory, size_t size, const char* file_name, int line, const char* func_name)
{
    sMallocEntry* entry;
    int hash;

    /// delete old entry ///
    if(memory) release_entry(memory, file_name, line, func_name);

    /// add new entry ///
    entry = (sMallocEntry*)malloc(sizeof(sMallocEntry));
if(gLogging) fprintf(gFile, "mem %p malloced for debug info\n", entry);
      
    xstrncpy(entry->mFileName, (char*)file_name, NAME_SIZE);
    entry->mLine = line;
    xstrncpy(entry->mFuncName, (char*)func_name, NAME_SIZE);

    entry->mMemory = realloc(memory, size);
    if(entry->mMemory == NULL) {
        fprintf(stderr,"false in allocating memory.");
        exit(1);
    }

#ifdef __64bit__
    hash = (unsigned long long)entry->mMemory % ARRAY_SIZE;
#else
    hash = (unsigned long)entry->mMemory % ARRAY_SIZE;
#endif
    entry->mNextEntry = gMallocEntries[hash];
    gMallocEntries[hash] = entry;
if(gLogging) fprintf(gFile, "mem %p realloced at file %s line %d fun %s\n", entry->mMemory, file_name, line, func_name);

    return entry->mMemory;
}

//////////////////////////////////////////////////////////////////////
// メモリーリークチェック用メモリーアロケート
//////////////////////////////////////////////////////////////////////
ALLOC char* CheckMemLeak_Strdup(char* str, const char* file_name, int line, const char* func_name)
{
    char* result;

    result = (char*)CheckMemLeak_Malloc(sizeof(char)*(strlen(str)+1), file_name, line, func_name);
if(gLogging) fprintf(gFile, "mem %p strdupped at file %s line %d fun %s\n", str, file_name, line, func_name);

    xstrncpy(result, str, strlen(str)+1);

    return result;
}
   
//////////////////////////////////////////////////////////////////////
// メモリーリークチェック用メモリー解放
//////////////////////////////////////////////////////////////////////
void CheckMemLeak_Free(void* memory, const char* file_name, int line, const char* func_name)
{
if(gLogging) fprintf(gFile, "mem %p freed at file %s line %d fun %s\n", memory, file_name, line, func_name);
    release_entry(memory, file_name, line, func_name);
    free(memory);
}

#endif

