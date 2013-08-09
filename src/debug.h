#ifndef DEBUG_H
#define DEBUG_H

#include <stdlib.h>
#include <assert.h>

#ifndef BOOL 
#define BOOL int
#endif

#ifndef TRUE
#define TRUE 1
#endif

#ifndef FALSE
#define FALSE 0
#endif

#ifndef ALLOC
#define ALLOC                   // indicated this memory should be freed after used
#endif

#ifndef MANAGED
#define MANAGED                 // indicated this memory is managed inside the function or object
#endif

#if !defined(MDEBUG)

#   define CHECKML_BEGIN()
#   define CHECKML_END()

ALLOC void* xmalloc(size_t size);
ALLOC char* xstrdup(char* str);
ALLOC void* xrealloc(void* ptr, size_t size);

#   define MALLOC(o) xmalloc(o)
#   define STRDUP(o) xstrdup(o)
#   define REALLOC(o, o2) xrealloc(o, o2)
#   define FREE(o) free(o)

#   define ASSERT(o)

#else 

void debug_init();
void debug_final();

ALLOC void* debug_malloc(size_t size, const char* file_name, int line, const char* func_name);
ALLOC char* debug_strdup(char* str, const char* file_name, int line, const char* func_name);
ALLOC void* debug_realloc(void* ptr, size_t size, const char* file_name, int line, const char* func_name);
void debug_free(MANAGED void* memory, const char* file_name, int line, const char* func_name);
 
#   define CHECKML_BEGIN() debug_init();
#   define CHECKML_END() debug_final(); 

#   define MALLOC(o) debug_malloc(o, __FILE__, __LINE__, __FUNCTION__)
#   define STRDUP(o) debug_strdup(o, __FILE__, __LINE__, __FUNCTION__)
#   define REALLOC(o, o2) debug_realloc(o, o2, __FILE__, __LINE__, __FUNCTION__)
#   define FREE(o) debug_free(o, __FILE__, __LINE__, __FUNCTION__)

#   define ASSERT(o) assert(o)

#endif

#endif
