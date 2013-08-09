#include "config.h"
#include "xyzsh.h"
#include <string.h>
#include <stdio.h>
#include <errno.h>

sObject* fd_new_on_stack()
{
    sObject* self = stack_get_free_object(T_FD);

    SFD(self).mBuf = MALLOC(1024);
    SFD(self).mBuf[0] = 0; // sentinel

    SFD(self).mBufSize = 1024;
    SFD(self).mBufLen = 0;

    SFD(self).mLines = VECTOR_NEW_MALLOC(32);
    SFD(self).mReadedLineNumber = 0;

    return self;
}

void fd_delete_on_stack(sObject* self)
{
    sObject* v = SFD(self).mLines;
    int i;
    for(i=0; i<vector_count(v); i++) {
       FREE(vector_item(v, i));
    }
    vector_delete_on_malloc(SFD(self).mLines);

    FREE(SFD(self).mBuf);
}

void fd_put(sObject* self, MANAGED char* buf, int buf_size, int buf_len)
{
    ASSERT(STYPE(self) == T_FD);

    FREE(SFD(self).mBuf);

    SFD(self).mBuf = MANAGED buf;
    SFD(self).mBufSize = buf_size;
    SFD(self).mBufLen = buf_len;

    int i;
    for(i=0; i<vector_count(SFD(self).mLines); i++) {
        FREE(vector_item(SFD(self).mLines, i));
    }
    vector_clear(SFD(self).mLines);
    SFD(self).mReadedLineNumber = 0;
}

// TRUE: Success
// FALSE: signal interrupt. set RCODE_SIGNAL_INTERRUPT on rcode and err message
BOOL bufsiz_write(int fd, char* buf, int buf_size)
{
    char* p = buf;
    while(p - buf < buf_size) {
        int size;
        if(buf_size - (p - buf) < BUFSIZ) {
            size = buf_size - (p-buf);
        }
        else {
            size = BUFSIZ;
        }

        if(write(fd, p, size) < 0) {
            if(errno == EINTR) {
                while(!gXyzshSigInt);
                gXyzshSigInt = FALSE;
                return FALSE;
            }
            return FALSE;
        }

        if(gXyzshSigInt) {
            gXyzshSigInt = FALSE;
            return FALSE;
        }

        p+=size;
    }

    return TRUE;
}

// TRUE: Success. result is malloced.
// FALSE: signal interrupt. set RCODE_SIGNAL_INTERRUPT on rcode and err message. result is not malloced.
BOOL bufsiz_read(int fd, ALLOC char** result, int* result_len, int* result_size)
{
    int source_size = BUFSIZ * 8;
    int source_len = 0;
    char* source = MALLOC(BUFSIZ*8);
    source[0] = 0;

    char buf[BUFSIZ+1];
    while(TRUE) {
        int size = read(fd, buf, BUFSIZ);
        if(size < 0) {
            FREE(source);
            return FALSE;
        }
        buf[size] = 0;
        if(size < BUFSIZ) {
            if(source_len + size + 1 >= source_size) {
                source_size *= 2;
                source = REALLOC(source, source_size);
            }
            strcat(source, buf);
            source_len += size;
            break;
        }
        else {
            if(source_len + size + 1 >= source_size) {
                source_size *= 2;
                source = REALLOC(source, source_size);
            }
            strcat(source, buf);
            source_len += size;
        }
    }

    *result = source;
    *result_len = source_len;
    *result_size = source_size;
    
    return TRUE;
}

BOOL fd_guess_lf(sObject* self, eLineField* lf)
{
    ASSERT(STYPE(self) == T_FD);

    *lf = -1;
    char* p = SFD(self).mBuf;

    while(*p) {
        if(*p == '\a') {
            *lf = kBel;
            return TRUE;
        }
        else {
            p++;
        }
    }

    if(*lf == -1) {
        p = SFD(self).mBuf;

        while(*p) {
            if(*p == '\r' && *(p+1) == '\n') {
                *lf = kCRLF;
                return TRUE;
            }
            else {
                p++;
            }
        }
    }

    if(*lf == -1) {
        p = SFD(self).mBuf;

        while(*p) {
            if(*p == '\r') {
                *lf = kCR;
                return TRUE;
            }
            else {
                p++;
            }
        }
    }

    if(*lf == -1) {
        p = SFD(self).mBuf;

        while(*p) {
            if(*p == '\n') {
                *lf = kLF;
                return TRUE;
            }
            else {
                p++;
            }
        }
    }

    return FALSE;
}

BOOL fd_chomp(sObject* self)
{
    assert(STYPE(self) == T_FD);

    char* s = SFD(self).mBuf;
    const int len = SFD(self).mBufLen;

    if(len >= 2 && s[len-2] == '\r' && s[len-1] == '\n')
    {
        fd_trunc(self, len-2);

        return TRUE;
    }
    else if(len >= 1 && (s[len-1] == '\r' || s[len-1] == '\n' || s[len-1] == '\a')) 
    {
        fd_trunc(self, len-1);
        return TRUE;
    }

    return FALSE;
}

void fd_split(sObject* self, eLineField lf, BOOL pomch_to_last_line, BOOL chomp_before_split, BOOL split_with_all_item_chompped)
{
    assert(STYPE(self) == T_FD);

    sObject* v = SFD(self).mLines;

    if(SFD(self).mLinesLineField != lf) {
        int i;
        for(i=0; i<vector_count(v); i++) {
            FREE(vector_item(v, i));
        }
        vector_clear(v);
        SFD(self).mReadedLineNumber = 0;
    }

    /// chomp before split ///
    int chomped_len;
    if(chomp_before_split) {
        char* s = SFD(self).mBuf;
        chomped_len = SFD(self).mBufLen;

        if(chomped_len >= 2 && s[chomped_len-2] == '\r' && s[chomped_len-1] == '\n')
        {
            chomped_len -= 2;
        }
        else if(chomped_len >= 1 && (s[chomped_len-1] == '\r' || s[chomped_len-1] == '\n' || s[chomped_len-1] == '\a')) 
        {
            chomped_len --;
        }
    }
    else {
        chomped_len = SFD(self).mBufLen;
    }

    if(vector_count(v) == 0) {
        if(split_with_all_item_chompped) {
            char* p = SFD(self).mBuf;
            char* new_line = p;
            if(lf == kCRLF) {
                while(1) {
                    if(p - SFD(self).mBuf >= chomped_len) {
                        const int size = p - new_line;
                        if(size > 0) {
                            char* line;
                            line = MALLOC(size + 1);
                            memcpy(line, new_line, size);
                            line[size] = 0;
                            vector_add(v, line);
                        }
                        break;
                    }
                    else if(*p == '\r' && *(p+1) == '\n') {
                        const int size = p - new_line;
                        char* line = MALLOC(size + 1);
                        memcpy(line, new_line, size);
                        line[size] = 0;
                        vector_add(v, line);
                        p+=2;
                        new_line = p;
                    }
                    else {
                        p++;
                    }
                }
            }
            else {
                char split_char;
                if(lf == kLF) {
                    split_char = '\n';
                }
                else if(lf == kCR) {
                    split_char = '\r';
                }
                else {
                    split_char = '\a';
                }

                while(1) {
                    if(p - SFD(self).mBuf >= chomped_len) {
                        const int size = p - new_line;
                        if(size > 0) {
                            char* line;
                            line = MALLOC(size + 1);
                            memcpy(line, new_line, size);
                            line[size] = 0;
                            vector_add(v, line);
                        }
                        break;
                    }
                    else if(*p == split_char) {
                        const int size = p - new_line;
                        char* line = MALLOC(size + 1);
                        memcpy(line, new_line, size);
                        line[size] = 0;
                        vector_add(v, line);
                        p++;
                        new_line = p;
                    }
                    else {
                        p++;
                    }
                }
            }
        }
        else {
            char* p = SFD(self).mBuf;
            char* new_line = p;
            if(lf == kCRLF) {
                while(1) {
                    if(p - SFD(self).mBuf >= chomped_len) {
                        const int size = p - new_line;
                        if(size > 0) {
                            char* line;
                            if(pomch_to_last_line) {
                                line = MALLOC(size + 1 + 2);
                                memcpy(line, new_line, size);
                                line[size] = '\r';
                                line[size+1] = '\n';
                                line[size+2] = 0;
                            }
                            else {
                                line = MALLOC(size + 1);
                                memcpy(line, new_line, size);
                                line[size] = 0;
                            }
                            vector_add(v, line);
                        }
                        break;
                    }
                    else if(*p == '\r' && *(p+1) == '\n') {
                        p+=2;
                        const int size = p - new_line;
                        char* line = MALLOC(size + 1);
                        memcpy(line, new_line, size);
                        line[size] = 0;
                        vector_add(v, line);
                        new_line = p;
                    }
                    else {
                        p++;
                    }
                }
            }
            else {
                char split_char;
                if(lf == kLF) {
                    split_char = '\n';
                }
                else if(lf == kCR) {
                    split_char = '\r';
                }
                else {
                    split_char = '\a';
                }

                while(1) {
                    if(p - SFD(self).mBuf >= chomped_len) {
                        const int size = p - new_line;
                        if(size > 0) {
                            char* line;
                            if(pomch_to_last_line) {
                                line = MALLOC(size + 1 + 1);
                                memcpy(line, new_line, size);
                                line[size] = split_char;
                                line[size+1] = 0;
                            }
                            else {
                                line = MALLOC(size + 1);
                                memcpy(line, new_line, size);
                                line[size] = 0;
                            }
                            vector_add(v, line);
                        }
                        break;
                    }
                    else if(*p == split_char) {
                        const int size = p - new_line + 1;
                        char* line = MALLOC(size + 1);
                        memcpy(line, new_line, size);
                        line[size] = 0;
                        vector_add(v, line);
                        p++;
                        new_line = p;
                    }
                    else {
                        p++;
                    }
                }
            }
        }

        SFD(self).mLinesLineField = lf;
    }
}

// TRUE: Success
// FALSE: signal interrupt
static BOOL memcpy_buf(char* dest, char* src, size_t size)
{
    char* p = dest;
    char* p2 = src;
    while(1) {
        if(gXyzshSigInt) {
            gXyzshSigInt = FALSE;
            return FALSE;
        }
        if(size - (p - dest) < BUFSIZ) {
            memcpy(p, p2, size - (p-dest));
            break;
        }
        else {
            memcpy(p, p2, BUFSIZ);
            p+=BUFSIZ;
            p2+=BUFSIZ;
        }
    }

    return TRUE;
}

void fd_clear(sObject* self)
{
    assert(STYPE(self) == T_FD);

    SFD(self).mBufLen = 0;
    SFD(self).mBuf[0] = 0; // sentinel

    int i;
    for(i=0; i<vector_count(SFD(self).mLines); i++) {
        FREE(vector_item(SFD(self).mLines, i));
   }
   vector_clear(SFD(self).mLines);
    SFD(self).mReadedLineNumber = 0;
}

// TRUE: Success
// FALSE: signal interrupt. set RCODE_SIGNAL_INTERRUPT on rcode and err message
BOOL fd_write(sObject* self, char* str, int len)
{
    ASSERT(STYPE(self) == T_FD || STYPE(self) == T_FD2);

    if(STYPE(self) == T_FD2) {
        if(write(SFD2(self).mFD, str, len) < 0) {
            return FALSE;
        }
    }
    else {
        if((SFD(self).mBufLen+len+1) >= SFD(self).mBufSize) {
            int new_size = (SFD(self).mBufSize + len+1) * 2;

            SFD(self).mBuf = REALLOC(SFD(self).mBuf, new_size);
            SFD(self).mBufSize = new_size;
        }

        if(!memcpy_buf(SFD(self).mBuf + SFD(self).mBufLen, str, len)) {
            return FALSE;
        }
        SFD(self).mBufLen += len;
        SFD(self).mBuf[SFD(self).mBufLen] = 0; // sentinel
    }

    return TRUE;
}

// TRUE: Success
// FALSE: signal interrupt. set RCODE_SIGNAL_INTERRUPT on rcode and err message
BOOL fd_writec(sObject* self, char c)
{
    ASSERT(STYPE(self) == T_FD || STYPE(self) == T_FD2);
    
    if(STYPE(self) == T_FD2) {
        if(write(SFD2(self).mFD, &c, 1) < 0) {
            return FALSE;
        }
    }
    else {
/*
        if((SFD(self).mBufLen) >= SFD(self).mBufSize) {
            int new_size = (SFD(self).mBufSize) * 1.8;
            SFD(self).mBuf = REALLOC(SFD(self).mBuf, new_size);
            SFD(self).mBufSize = new_size;
        }
*/
        if((SFD(self).mBufLen+2) >= SFD(self).mBufSize) {
            int new_size = (SFD(self).mBufSize + 2) * 2;
            SFD(self).mBuf = REALLOC(SFD(self).mBuf, new_size);
            SFD(self).mBufSize = new_size;
        }

        SFD(self).mBuf[SFD(self).mBufLen++] = c;
        SFD(self).mBuf[SFD(self).mBufLen] = 0; // sentinel

        if(gXyzshSigInt) {
            gXyzshSigInt = FALSE;
            return FALSE;
        }
    }

    return TRUE;
}

// TRUE: Success
// FALSE: signal interrupt. set RCODE_SIGNAL_INTERRUPT on rcode and err message
BOOL fd_flash(sObject* self, int fd)
{
    ASSERT(STYPE(self) == T_FD);

    if(STYPE(self) == T_FD) {
        if(!bufsiz_write(fd, SFD(self).mBuf, SFD(self).mBufLen)) {
            return FALSE;
        }

        SFD(self).mBufLen = 0;
        SFD(self).mBuf[0] = 0; // sentinel
    }

    return TRUE;
}

void fd_trunc(sObject* self, int index)
{
    assert(STYPE(self) == T_FD);
    if(index >= 0 && index < SFD(self).mBufLen) {
        SFD(self).mBufLen = index;
        SFD(self).mBuf[SFD(self).mBufLen] = 0; // sentinel
    }
}

sObject* fd2_new_on_stack(uint fd)
{
    sObject* self = stack_get_free_object(T_FD2);

    SFD2(self).mFD = fd;

    return self;
}
