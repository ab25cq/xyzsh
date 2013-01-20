#include "config.h"
#include "xyzsh/xyzsh.h"
#include <string.h>
#include <stdio.h>
#include <errno.h>

sObject* fd_new_on_stack()
{
    sObject* self = stack_get_free_object(T_FD);

    SFD(self).mBuf = MALLOC(1024);
    SFD(self).mBuf[0] = 0;

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
    ASSERT(TYPE(self) == T_FD);

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

BOOL fd_guess_lf(sObject* self, eLineField* lf)
{
    ASSERT(TYPE(self) == T_FD);

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

void fd_split(sObject* self, eLineField lf)
{
    assert(TYPE(self) == T_FD);

    sObject* v = SFD(self).mLines;

    if(SFD(self).mLinesLineField != lf) {
        int i;
        for(i=0; i<vector_count(v); i++) {
            FREE(vector_item(v, i));
        }
        vector_clear(v);
        SFD(self).mReadedLineNumber = 0;
    }

    if(vector_count(v) == 0) {
        char* p = SFD(self).mBuf;
        char* new_line = p;
        if(lf == kCRLF) {
            while(1) {
                if(*p == '\r' && *(p+1) == '\n') {
                    p+=2;
                    const int size = p - new_line;
                    char* line = MALLOC(size + 1);
                    memcpy(line, new_line, size);
                    line[size] = 0;
                    vector_add(v, line);
                    new_line = p;
                }
                else if(p - SFD(self).mBuf >= SFD(self).mBufLen) {
                    const int size = p - new_line;
                    if(size > 0) {
                        char* line = MALLOC(size + 1 + 2);
                        memcpy(line, new_line, size);
                        line[size] = '\r';
                        line[size+1] = '\n';
                        line[size+2] = 0;
                        vector_add(v, line);
                    }
                    break;
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
                if(*p == split_char) {
                    const int size = p - new_line + 1;
                    char* line = MALLOC(size + 1);
                    memcpy(line, new_line, size);
                    line[size] = 0;
                    vector_add(v, line);
                    p++;
                    new_line = p;
                }
                else if(p - SFD(self).mBuf >= SFD(self).mBufLen) {
                    const int size = p - new_line;
                    if(size > 0) {
                        char* line = MALLOC(size + 1 + 1);
                        memcpy(line, new_line, size);
                        line[size] = split_char;
                        line[size+1] = 0;
                        vector_add(v, line);
                    }
                    break;
                }
                else {
                    p++;
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
    assert(TYPE(self) == T_FD);

    SFD(self).mBufLen = 0;
    SFD(self).mBuf[0] = 0;

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
    ASSERT(TYPE(self) == T_FD || TYPE(self) == T_FD2);

    if(TYPE(self) == T_FD2) {
        if(write(SFD2(self).mFD, str, len) < 0) {
            return FALSE;
        }
    }
    else {
        if((SFD(self).mBufLen+len+1) >= SFD(self).mBufSize) {
            int new_size = (SFD(self).mBufSize + len+1) * 1.8;

            SFD(self).mBuf = REALLOC(SFD(self).mBuf, new_size);
            SFD(self).mBufSize = new_size;
        }

        if(!memcpy_buf(SFD(self).mBuf + SFD(self).mBufLen, str, len)) {
            return FALSE;
        }
        SFD(self).mBufLen += len;
        SFD(self).mBuf[SFD(self).mBufLen] = 0;
    }

    return TRUE;
}

// TRUE: Success
// FALSE: signal interrupt. set RCODE_SIGNAL_INTERRUPT on rcode and err message
BOOL fd_writec(sObject* self, char c)
{
    ASSERT(TYPE(self) == T_FD || TYPE(self) == T_FD2);
    
    if(TYPE(self) == T_FD2) {
        if(write(SFD2(self).mFD, &c, 1) < 0) {
            return FALSE;
        }
    }
    else {
        if((SFD(self).mBufLen) >= SFD(self).mBufSize) {
            int new_size = (SFD(self).mBufSize) * 1.8;
            SFD(self).mBuf = REALLOC(SFD(self).mBuf, new_size);
            SFD(self).mBufSize = new_size;
        }

        SFD(self).mBuf[SFD(self).mBufLen++] = c;
        SFD(self).mBuf[SFD(self).mBufLen] = 0;

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
    ASSERT(TYPE(self) == T_FD);

    if(TYPE(self) == T_FD) {
        if(!bufsiz_write(fd, SFD(self).mBuf, SFD(self).mBufLen)) {
            return FALSE;
        }

        SFD(self).mBufLen = 0;
        SFD(self).mBuf[0] = 0;
    }

    return TRUE;
}

void fd_trunc(sObject* self, int index)
{
    if(index >= 0 && index < SFD(self).mBufLen) {
        SFD(self).mBufLen = index;
        SFD(self).mBuf[SFD(self).mBufLen] = 0;
    }
}

sObject* fd2_new_on_stack(uint fd)
{
    sObject* self = stack_get_free_object(T_FD2);

    SFD2(self).mFD = fd;

    return self;
}
