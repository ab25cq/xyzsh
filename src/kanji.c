#include "config.h"
#include "kanji.h"
#include "debug.h"
#include "xfunc.h"

#include <wctype.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <limits.h>

char* gKanjiCodeString[6] = {
    "eucjp", "sjis", "utf8", "utf8-mac", "byte", "unknown"
};

char* gIconvKanjiCodeName[6] = {
    "eucjp", "sjis", "utf-8", "utf-8-mac", "unknown", "unknown"
};


int str_termlen_range(enum eKanjiCode code, char* mbs, int utfpos1, int utfpos2)
{
    if(code == kByte) {
        return utfpos2 - utfpos1;
    }
    else if(code == kUtf8) {
        if(is_utf8_bytes(mbs)) {
            if(utfpos1 < utfpos2) {
                char* putfpos1 = str_kanjipos2pointer(kUtf8, mbs, utfpos1);
                char* putfpos2 = str_kanjipos2pointer(kUtf8, mbs, utfpos2);
                
                char* mbs2 = MALLOC(putfpos2 - putfpos1 + 1);
                memcpy(mbs2, putfpos1, putfpos2 - putfpos1);
                mbs2[putfpos2 - putfpos1] = 0;

                int result = str_termlen(kUtf8, mbs2);
                FREE(mbs2);

                return result;
            }
            else {
                return -1;
            }
        }
        else {
            return strlen(mbs);
        }
    }
    else {
        fprintf(stderr, "not defined kEucjp, kSjis");
        exit(1);
    }
}

BOOL is_utf8_bytes(char* mbs)
{
    const int len = strlen(mbs) + 1;
    wchar_t* wcs = (wchar_t*)MALLOC(sizeof(wchar_t)*len);
    char* mbs2 = MALLOC(len * MB_LEN_MAX);

    if(mbstowcs(wcs, mbs, len) < 0) {
        FREE(wcs);
        FREE(mbs2);
        return FALSE;
    }

    if(wcstombs(mbs2, wcs, len * MB_LEN_MAX) < 0) {
        FREE(wcs);
        FREE(mbs2);
        return FALSE;
    }

    if(strcmp(mbs, mbs2) == 0) {
        FREE(wcs);
        FREE(mbs2);
        return TRUE;
    }
    else {
        FREE(wcs);
        FREE(mbs2);
        return FALSE;
    }
}

int str_pointer2kanjipos(enum eKanjiCode code, char* mbs, char* point)
{
    if(code == kEucjp || code == kSjis) {
        int result = 0;

        char* p = mbs;
        while(p < point) {
            if(is_kanji(code, *p)) {
                p+=2;
                result++;
            }
            else {
                p++;
                result++;
            }
        }

        return result;
    }
    else if(code == kByte) {
        return point - mbs;
    }
    else {
        char* mbs2;
        int result;

        mbs2 = STRDUP(mbs);
        *(mbs2 + (point-mbs)) = 0;

        result = str_kanjilen(code, mbs2);

        FREE(mbs2);

        return result;
    }
}

char* str_kanjipos2pointer(enum eKanjiCode code, char* mbs, int pos)
{
    if(code == kUtf8) {
        if(pos<0) return mbs;

        char* p = mbs;
        int n = 0;
    
        char* max = mbs + strlen(mbs);
        while(p < max) {
            if(n == pos) {
                return p;
            }
            if(((unsigned char)*p) > 127) {
                int size = ((*p & 0x80) >> 7) + ((*p & 0x40) >> 6) + ((*p & 0x20) >> 5) + ((*p & 0x10) >> 4);
                p+= size;
                n++;
            }
            else {
                p++;
                n++;
            }
/*
            if(gKitutukiSigInt) {
                return NULL;
            }
*/
        }

        return mbs + strlen(mbs);
    }
    else if(code == kByte) {
        return mbs + pos;
    }
    else {
        if(pos<0) return mbs;

        char* p = mbs;
        int c = 0;
        char* max = mbs + strlen(mbs);
        while(p < max) {
            if(c==pos) {
                return p;
            }

            if(is_kanji(code, *p)) {
                p+=2;
                c++;
            }
            else {
                p++;
                c++;
            }
        }

        return mbs + strlen(mbs);
    }
}

int str_kanjilen(enum eKanjiCode code, char* mbs)
{
    if(code == kByte) {
        return strlen(mbs);
    }
    else if(code == kUtf8) {
        int n = 0;
        char* p = mbs;

        char* max = strlen(mbs) + mbs;
        while(p < max) {
            if(((unsigned char)*p) > 127) {
                int size = ((*p & 0x80) >> 7) + ((*p & 0x40) >> 6) + ((*p & 0x20) >> 5) + ((*p & 0x10) >> 4);
                p+= size;
                n++;
            }
            else {
                p++;
                n++;
            }
        }

        return n;
    }
    else {
        int result = 0;
        char* p = mbs;
        while(*p) {
            if(is_kanji(code, *p) && *(p+1) != 0) {
                p+=2;
                result++;
            }
            else {
                p++;
                result++;
            }
        }
        return result;
    }
}

int is_hankaku(enum eKanjiCode code, unsigned char c)
{
    if(code == kEucjp)
        return c == 0x8e;
    else
        return 0;
}

int is_kanji(enum eKanjiCode code, unsigned char c)
{
    if(code == kEucjp) {
        return c >= 0xA1 && c <= 0xFE || c == 0x8e;
//        return c >= 161 && c <= 254;
    }
    else if(code == kSjis) {
        return c >= 0x81 && c <= 0x9f || c >= 0xE0;
    }
    else if(code == kUtf8) {
        return c > 127;
    }

    return 0;
}

int is_all_ascii(enum eKanjiCode code, char* buf)
{
    if(code == kUtf8 || code == kByte) {
        char* p = buf;
        while(*p) {
            if(((unsigned char)*p) > 127) {
                return FALSE;
            }
            p++;
        }
    }
    else if(code == kSjis || code == kEucjp) {
        fprintf(stderr, "unexpected err in is_all_ascii\n");
        exit(1);
    }

    return TRUE;
}

int str_termlen(enum eKanjiCode code, char* mbs)
{
    if(code == kUtf8) {
        int result;
        wchar_t* wcs;

        const int len = strlen(mbs) + 1;
        wcs = (wchar_t*)MALLOC(sizeof(wchar_t)*len);
        if(mbstowcs(wcs, mbs, len) < 0) {
            FREE(wcs);
            return -1;
        }
        result = wcswidth(wcs, wcslen(wcs));
        FREE(wcs);
        if(result < 0) {
            return strlen(mbs);
        }

        return result;
    }
    else {
        return strlen(mbs);
    }
}

int str_termlen2(enum eKanjiCode code, char* mbs, int pos)
{
    if(code == kUtf8) {
        char* mbs2;
        int result;
        char* point;

        mbs2 = STRDUP(mbs);
        point = str_kanjipos2pointer(kUtf8, mbs2, pos);
        if(point == NULL) {
            if(pos < 0) 
                result = -1;
            else
                result = str_termlen(code, mbs2);
        }
        else {
            *(mbs2 + (point-mbs2)) = 0;
            result = str_termlen(code, mbs2);
        }

        FREE(mbs2);

        return result;
    }
    else {
        return pos*2;
    }
}

#if defined(HAVE_ICONV_H)
#include <iconv.h>

#if !defined(__FREEBSD__) && !defined(__SUNOS__) && !defined(__DARWIN__)
extern size_t iconv (iconv_t cd, char* * inbuf, size_t *inbytesleft, char* * outbuf, size_t *outbytesleft);
#endif

int kanji_convert(char* input_buf, char* output_buf, size_t output_buf_size, enum eKanjiCode input_kanji_encode_type, enum eKanjiCode output_kanji_encode_type)
{
    iconv_t ic;
    char* ptr_in;
    char* ptr_out;
    const int len = strlen(input_buf);
    size_t result;
    size_t bufsz_in;
    size_t bufsz_out;

    if(strlen(input_buf) == 0) {
        xstrncpy(output_buf, input_buf, output_buf_size);
    }
    else {
        if(is_all_ascii(kByte, input_buf)
            || input_kanji_encode_type == output_kanji_encode_type
            || input_kanji_encode_type < 0
            || input_kanji_encode_type >= kUnknown 
            || output_kanji_encode_type < 0
            || output_kanji_encode_type >= kUnknown)
        {
            xstrncpy(output_buf, input_buf, output_buf_size);
            return 0;
        }
        
        ptr_in = input_buf;
        ptr_out = output_buf;
        bufsz_in = (size_t) len;
        bufsz_out = (size_t) output_buf_size;

        if(input_kanji_encode_type > kByte || input_kanji_encode_type < 0) 
        {
            fprintf(stderr, "input kanji encode type error\n");
            exit(1);
            //memcpy(ptr_out, ptr_in, bufsz_in);
            //ptr_out[bufsz_in] = 0;
        }
        else if(output_kanji_encode_type >= kByte || output_kanji_encode_type < 0 || input_kanji_encode_type == kByte) 
        {
            memcpy(ptr_out, ptr_in, bufsz_in);
            ptr_out[bufsz_in] = 0;
        }
        else {
            ic = iconv_open(gIconvKanjiCodeName[output_kanji_encode_type]
                            , gIconvKanjiCodeName[input_kanji_encode_type]);
            if(ic < 0)
            {
                return -1;
            }

#if defined(__FREEBSD__) || defined(__SUNOS__)
            if(iconv(ic, (const char**)&ptr_in, &bufsz_in, &ptr_out, &bufsz_out) < 0
#else
            if(iconv(ic, &ptr_in, &bufsz_in, &ptr_out, &bufsz_out) < 0
#endif
                    || len-(int)bufsz_in < len) 
            {
                iconv_close(ic);

                return -1;
            }
            output_buf[output_buf_size - bufsz_out] = 0;

            iconv_close(ic);
        }
    }

    return 0;
}


#endif
