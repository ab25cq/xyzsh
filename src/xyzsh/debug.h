/*
 * debug routines, especially for detecting memory leaks
 */

#ifndef XYZSH_DEBUG_H
#define XYZSH_DEBUG_H

#include <stdlib.h>
#include <assert.h>

#define P(F, O) { FILE* fp = fopen(F, "a"); fprintf(fp, O); fprintf(fp, "\n"); fclose(fp); }

#if !defined(MDEBUG)

#   define CHECKML_BEGIN(o)
#   define CHECKML_END()  
#   define MALLOC(o) malloc(o)
#   define STRDUP(o) strdup(o)
#   define REALLOC(o, o2) realloc(o, o2)
#   define FREE(o) free(o)
#   define TIMER_BEGIN(msg)
#   define TIMER_END()
#   define ASSERT(o)

#else 

void CheckMemLeak_Begin(BOOL log);
void CheckMemLeak_End();
ALLOC void* CheckMemLeak_Malloc(size_t size, const char* file_name, int line, const char* func_name);
ALLOC char* CheckMemLeak_Strdup(char* str, const char* file_name, int line, const char* func_name);
ALLOC void* CheckMemLeak_Realloc(void* ptr, size_t size, const char* file_name, int line, const char* func_name);
void CheckMemLeak_Free(MANAGED void* memory, const char* file_name, int line, const char* func_name);
 
void timer_count_start(char* msg);
void timer_count_end(const char* file, int line, const char* fun);

#   define CHECKML_BEGIN(o) CheckMemLeak_Begin(o);
#   define CHECKML_END() CheckMemLeak_End(); 

#   define MALLOC(o) CheckMemLeak_Malloc(o, __FILE__, __LINE__, __FUNCTION__)
#   define STRDUP(o) CheckMemLeak_Strdup(o, __FILE__, __LINE__, __FUNCTION__)
#   define REALLOC(o, o2) CheckMemLeak_Realloc(o, o2, __FILE__, __LINE__, __FUNCTION__)
#   define FREE(o) CheckMemLeak_Free(o, __FILE__, __LINE__, __FUNCTION__)

#   define TIMER_BEGIN(msg) timer_count_start(msg)
#   define TIMER_END() timer_count_end(__FILE__, __LINE__, __FUNCTION__)
#    define ASSERT(o) assert(o)

#endif

#endif
