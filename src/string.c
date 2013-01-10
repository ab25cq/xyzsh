#include "config.h"
#include <string.h>
#include <stdio.h>
#include <ctype.h>
#include "config.h"
#include "xyzsh/xyzsh.h"

////////////////////////////////////////////////////
// Initialization
////////////////////////////////////////////////////
#ifndef MDEBUG
sObject* string_new_from_malloc(char* str)
{
    sObject* self = MALLOC(sizeof(sObject));

    self->mFlg = T_STRING;

    const int len = strlen(str);

    SSTRING(self).mMallocLen = (len < 127) ? 128 : (len + 1) * 2;
    SSTRING(self).mStr = (char*)MALLOC(sizeof(char)*SSTRING(self).mMallocLen);
    xstrncpy(SSTRING(self).mStr, str, SSTRING(self).mMallocLen);
    
    SSTRING(self).mLen = len;

    return self;
}

#else

sObject* string_new_from_malloc_debug(char* str, const char* fname, int line, const char* func_name)
{
    sObject* self = CheckMemLeak_Malloc(sizeof(sObject), fname, line, func_name);

    self->mFlg = T_STRING;

    const int len = strlen(str);

    SSTRING(self).mMallocLen = (len < 127) ? 128 : (len + 1) * 2;
    SSTRING(self).mStr = (char*)MALLOC(sizeof(char)*SSTRING(self).mMallocLen);
    xstrncpy(SSTRING(self).mStr, str, SSTRING(self).mMallocLen);
    
    SSTRING(self).mLen = len;

    return self;
}

#endif

sObject* string_new_from_gc(char* str, BOOL user_object)
{
    sObject* self = gc_get_free_object(T_STRING, user_object);

    const int len = strlen(str);

    SSTRING(self).mMallocLen = (len < 127) ? 128 : (len + 1) * 2;
    SSTRING(self).mStr = (char*)MALLOC(sizeof(char)*SSTRING(self).mMallocLen);
    xstrncpy(SSTRING(self).mStr, str, SSTRING(self).mMallocLen);
    
    SSTRING(self).mLen = len;

    return self;
}

sObject* string_new_from_gc3(char* str, int size, BOOL user_object)
{
    sObject* self = gc_get_free_object(T_STRING, user_object);

    const int len = size;

    SSTRING(self).mMallocLen = (len < 127) ? 128 : (len + 1) * 2;
    SSTRING(self).mStr = (char*)MALLOC(sizeof(char)*SSTRING(self).mMallocLen);
    memcpy(SSTRING(self).mStr, str, size);
    SSTRING(self).mStr[size] = 0;
    
    SSTRING(self).mLen = len;

    return self;
}

sObject* string_new_from_stack(char* str)
{
    sObject* self = stack_get_free_object(T_STRING);

    const int len = strlen(str);

    SSTRING(self).mMallocLen = (len < 127) ? 128 : (len + 1) * 2;
    SSTRING(self).mStr = (char*)MALLOC(sizeof(char)*SSTRING(self).mMallocLen);
    xstrncpy(SSTRING(self).mStr, str, SSTRING(self).mMallocLen);
    
    SSTRING(self).mLen = len;

    return self;
}

/////////////////////////////////////////////////////
// Finalization
/////////////////////////////////////////////////////
void string_delete_malloc(sObject* self)
{
    FREE(SSTRING(self).mStr);

    FREE(self);
}

void string_delete_gc(sObject* self)
{
    FREE(SSTRING(self).mStr);
}

void string_delete_stack(sObject* self)
{
    FREE(SSTRING(self).mStr);
}

/////////////////////////////////////////////////////
// Some functions
/////////////////////////////////////////////////////
void string_insert(sObject* self, int pos, char* str)
{
    char* new_str;
    char* tmp_str;
    const int str_len = strlen(str);

    if(pos <= SSTRING(self).mLen) {
        if(SSTRING(self).mLen+str_len+1 > SSTRING(self).mMallocLen) {
            SSTRING(self).mMallocLen = (SSTRING(self).mLen + str_len + 1) * 2;
            new_str = (char*)MALLOC(SSTRING(self).mMallocLen);

            memcpy(new_str, SSTRING(self).mStr, pos);
            memcpy(new_str + pos, str, str_len);
            strcpy(new_str + pos + str_len, SSTRING(self).mStr + pos);

            FREE(SSTRING(self).mStr);

            SSTRING(self).mStr = new_str;
            SSTRING(self).mLen = strlen(new_str);
        }
        else {
            tmp_str = (char*)MALLOC(SSTRING(self).mMallocLen);

            memcpy(tmp_str, SSTRING(self).mStr, pos);
            memcpy(tmp_str + pos, str, str_len);
            strcpy(tmp_str + pos + str_len, SSTRING(self).mStr + pos);

            strcpy(SSTRING(self).mStr, tmp_str);
    
            FREE(tmp_str);
    
            SSTRING(self).mLen = strlen(SSTRING(self).mStr);
        }
    }
}

void string_push_back(sObject* self, char* str)
{
    char* new_str;
    const int str_len = strlen(str);

    if(SSTRING(self).mLen + str_len < SSTRING(self).mMallocLen) {
        strcat(SSTRING(self).mStr, str);
        SSTRING(self).mLen += str_len;
    }
    else {
        SSTRING(self).mMallocLen = (SSTRING(self).mLen + str_len + 1) * 2;
        new_str = (char*)MALLOC(SSTRING(self).mMallocLen);

        strcpy(new_str, SSTRING(self).mStr);
        strcat(new_str, str);

        FREE(SSTRING(self).mStr);

        SSTRING(self).mStr = new_str;
        SSTRING(self).mLen = SSTRING(self).mLen + str_len;
    }
}

void string_push_back2(sObject* self, char c)
{
    char* new_str;

    if(SSTRING(self).mLen + 1 < SSTRING(self).mMallocLen) {
        SSTRING(self).mStr[SSTRING(self).mLen] = c;
        SSTRING(self).mStr[SSTRING(self).mLen + 1] = 0;
        SSTRING(self).mLen++;
    }
    else {
        SSTRING(self).mMallocLen = (SSTRING(self).mLen + 1) * 2;
        new_str = (char*)MALLOC(SSTRING(self).mMallocLen);

        strcpy(new_str, SSTRING(self).mStr);
        new_str[SSTRING(self).mLen] = c;
        new_str[SSTRING(self).mLen + 1] = 0;

        FREE(SSTRING(self).mStr);

        SSTRING(self).mStr = new_str;
        SSTRING(self).mLen++;
    }
}

void string_push_back3(sObject* self, char* str, int n)
{
    char* new_str;
    char* tmp_str;
    const int str_len = strlen(str);

    if(n > str_len) {
        n = str_len;
    }

    if(SSTRING(self).mLen+n+1 > SSTRING(self).mMallocLen) {
        SSTRING(self).mMallocLen = (SSTRING(self).mLen + n + 1) * 2;
        new_str = (char*)MALLOC(SSTRING(self).mMallocLen);

        int len = strlen(SSTRING(self).mStr);

        strcpy(new_str, SSTRING(self).mStr);
        memcpy(new_str + len, str, n);
        new_str[len + n] = 0;

        FREE(SSTRING(self).mStr);

        SSTRING(self).mStr = new_str;
        SSTRING(self).mLen = strlen(new_str);
    }
    else {
        int len = strlen(SSTRING(self).mStr);
        memcpy(SSTRING(self).mStr + len, str, n);
        (SSTRING(self).mStr)[len + n] = 0;

        SSTRING(self).mLen = strlen(SSTRING(self).mStr);
    }
}

void string_erase(sObject* obj, int pos, int len)
{
    if(pos == 0 && len == SSTRING(obj).mLen) {
        string_put(obj, "");
    }
    else if(len > 0 && pos>=0 && pos<SSTRING(obj).mLen && pos+len <= SSTRING(obj).mLen) {
        memmove(SSTRING(obj).mStr + pos, SSTRING(obj).mStr+pos+len, SSTRING(obj).mLen-pos-len+1);

        SSTRING(obj).mLen-=len;
    }
}

void string_put(sObject* self, char* str)
{
    const int len = strlen(str);

    if(len+1 < SSTRING(self).mMallocLen) {
        strcpy(SSTRING(self).mStr, str);
        SSTRING(self).mLen = len;
    }
    else {
        FREE(SSTRING(self).mStr);
        
        SSTRING(self).mMallocLen = (len < 16) ? 16 : (len + 1) * 2;
        SSTRING(self).mStr = (char*)MALLOC(sizeof(char)*SSTRING(self).mMallocLen);
       
        strcpy(SSTRING(self).mStr, str);
        SSTRING(self).mLen = len;
    }
}

void string_put2(sObject* self, char c)
{
    if(SSTRING(self).mMallocLen > 1) {
        SSTRING(self).mStr[0] = c;
        SSTRING(self).mStr[1] = 0;
        SSTRING(self).mLen = 1;
    }
    else {
        FREE(SSTRING(self).mStr);
        
        SSTRING(self).mMallocLen = 16;
        SSTRING(self).mStr = (char*)MALLOC(sizeof(char)*SSTRING(self).mMallocLen);
        
        SSTRING(self).mStr[0] = c;
        SSTRING(self).mStr[1] = 0;
        SSTRING(self).mLen = 1;
    }
}

void string_trunc(sObject* self, int n)
{
    if(n < strlen(SSTRING(self).mStr)) {
        SSTRING(self).mStr[n] = 0;
        SSTRING(self).mLen = strlen(SSTRING(self).mStr);
    }
}

void string_tolower(sObject* self, enum eKanjiCode code)
{
    if(code == kSjis || code == kEucjp) {
        int i;
        for(i=0; i<SSTRING(self).mLen; i++) {
            if(is_kanji(code, SSTRING(self).mStr[i])) {
                SSTRING(self).mStr[i] = SSTRING(self).mStr[i];
                SSTRING(self).mStr[i+1] = SSTRING(self).mStr[i+1];
                i++;
            }
            else {
                SSTRING(self).mStr[i] = tolower(SSTRING(self).mStr[i]);
            }
        }
    }
    else {
        int i;
        for(i=0; i<SSTRING(self).mLen; i++) {
            SSTRING(self).mStr[i] = tolower(SSTRING(self).mStr[i]);
        }
    }
}

void string_toupper(sObject* self, enum eKanjiCode code)
{
    if(code == kSjis || code == kEucjp) {
        int i;
        for(i=0; i<SSTRING(self).mLen; i++) {
            if(is_kanji(code, SSTRING(self).mStr[i])) {
                SSTRING(self).mStr[i] = SSTRING(self).mStr[i];
                SSTRING(self).mStr[i+1] = SSTRING(self).mStr[i+1];
                i++;
            }
            else {
                SSTRING(self).mStr[i] = toupper(SSTRING(self).mStr[i]);
            }
        }
    }
    else {
        int i;
        for(i=0; i<SSTRING(self).mLen; i++) {
            SSTRING(self).mStr[i] = toupper(SSTRING(self).mStr[i]);
        }
    }
}

BOOL string_pomch(sObject* str, eLineField lf)
{
    char* s = string_c_str(str);
    const int len = strlen(s);

    if(len >= 2 && s[len-2] == '\r' && s[len-1] == '\n') {
        return FALSE;
    }
    else if(len >= 1 && (s[len-1] == '\r' || s[len-1] == '\n' || s[len-1] == '\a')) {
        return FALSE;
    }
    else {
        if(lf == kCR) {
            string_push_back2(str, '\r');
        }
        else if(lf == kCRLF) {
            string_push_back(str, "\r\n");
        }
        else if(lf == kLF) {
            string_push_back2(str, '\n');
        }
        else {
            string_push_back2(str, '\a');
        }

        return TRUE;
    }
}

BOOL string_chomp(sObject* str)
{
    char* s = string_c_str(str);
    const int len = strlen(s);

    if(len >= 2 && s[len-2] == '\r' && s[len-1] == '\n')
    {
        string_trunc(str, len-2);

        return TRUE;
    }
    else if(len >= 1 && (s[len-1] == '\r' || s[len-1] == '\n' || s[len-1] == '\a')) 
    {
        string_trunc(str, len-1);
        return TRUE;
    }

    return FALSE;
}

