/*
 * String Object Library
 */

#ifndef XYZSH_STRING_H
#define XYZSH_STRING_H

#ifdef MDEBUG
    sObject* string_new_on_malloc_debug(char* str, const char* fname, int line, const char* func_name);
#else
    sObject* string_new_on_malloc(char* str);
#endif
sObject* string_new_on_gc(char* str, BOOL user_object);
sObject* string_new_on_gc3(char* str, int size, BOOL user_object);
sObject* string_new_on_stack(char* str);

#ifdef MDEBUG
    #define STRING_NEW_MALLOC(str) string_new_on_malloc_debug(str, __FILE__, __LINE__, __FUNCTION__)
#else
    #define STRING_NEW_MALLOC(str) string_new_on_malloc(str)
#endif

#define STRING_NEW_GC(str, user_object) string_new_on_gc(str, user_object)
#define STRING_NEW_GC3(str, size, user_object) string_new_on_gc3(str, size, user_object)
#define STRING_NEW_STACK(str) string_new_on_stack(str)

void string_delete_on_malloc(sObject* self);
void string_delete_on_gc(sObject* self);
void string_delete_on_stack(sObject* self);

#define string_length(o) SSTRING(o).mLen

#define string_c_str(o) SSTRING(o).mStr

void string_push_back(sObject* self, char* key);
void string_push_back2(sObject* self, char key);
void string_push_back3(sObject* self, char* str, int n);
void string_put(sObject* self, char* str);

void string_put2(sObject* self, char c);

void string_trunc(sObject* self, int n);

void string_insert(sObject* obj, int pos, char* str);
void string_erase(sObject* obj, int pos, int len);

void string_toupper(sObject* self, enum eKanjiCode code);
void string_tolower(sObject* self, enum eKanjiCode code);

BOOL string_pomch(sObject* str, eLineField lf);
BOOL string_chomp(sObject* str);
void string_quote(sObject* self, enum eKanjiCode code);

unsigned int string_size(sObject* self);

#endif
