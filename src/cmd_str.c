#include "config.h"
#include <errno.h>
#include <time.h>
#include <stdlib.h>
#include <fcntl.h>
#include <libgen.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <math.h>
#include <oniguruma.h>
#include <sys/stat.h>
#include <signal.h>
#include <limits.h>
#include <dirent.h>

#ifdef __LINUX__
char *strcasestr(const char *haystack, const char *needle);
#endif

#if defined(HAVE_CURSES_H)
#include <curses.h>
#elif defined(HAVE_NCURSES_H)
#include <ncurses.h>
#elif defined(HAVE_NCURSES_NCURSES_H)
#include <ncurses/ncurses.h>
#endif

#include "xyzsh/xyzsh.h"

int get_onig_regex(regex_t** reg, sRunInfo* runinfo, char* regex)
{
    enum eKanjiCode code = gKanjiCode;
    if(sRunInfo_option(runinfo, "-byte")) {
        code = kByte;
    }
    else if(sRunInfo_option(runinfo, "-utf8")) {
        code = kUtf8;
    }
    else if(sRunInfo_option(runinfo, "-sjis")) {
        code = kSjis;
    }
    else if(sRunInfo_option(runinfo, "-eucjp")) {
        code = kEucjp;
    }

    OnigEncoding enc;
    switch(code) {
        case kUtf8:
            enc = ONIG_ENCODING_UTF8;
            break;

        case kEucjp:
            enc = ONIG_ENCODING_EUC_JP;
            break;

        case kSjis:
            enc = ONIG_ENCODING_SJIS;
            break;

        case kByte:
            enc = ONIG_ENCODING_ASCII;
            break;
    }

    OnigOptionType option = ONIG_OPTION_DEFAULT;
    if(sRunInfo_option(runinfo, "-ignore-case")) {
        option |= ONIG_OPTION_IGNORECASE;
    }
    else if(sRunInfo_option(runinfo, "-multi-line")) {
        option |= ONIG_OPTION_MULTILINE;
    }

    OnigErrorInfo err_info;
    return onig_new(reg, regex
        , regex + strlen((char*)regex)
        , option
        , enc
        , ONIG_SYNTAX_DEFAULT
        , &err_info);
}

BOOL cmd_quote(sObject* nextin, sObject* nextout, sRunInfo* runinfo)
{
    enum eKanjiCode code = gKanjiCode;
    if(sRunInfo_option(runinfo, "-byte")) {
        code = kByte;
    }
    else if(sRunInfo_option(runinfo, "-utf8")) {
        code = kUtf8;
    }
    else if(sRunInfo_option(runinfo, "-sjis")) {
        code = kSjis;
    }
    else if(sRunInfo_option(runinfo, "-eucjp")) {
        code = kEucjp;
    }

    /// output
    if(runinfo->mFilter) {
        char* p = SFD(nextin).mBuf;
        if(code == kByte) {
            while(*p) {
                if(isalpha(*p)) {
                    char buf[32];
                    int size = snprintf(buf, 32, "%c", *p);

                    if(!fd_write(nextout, buf, size)) {
                        err_msg("signal interrupt", runinfo->mSName, runinfo->mSLine, runinfo->mArgs[0]);
                        runinfo->mRCode = RCODE_SIGNAL_INTERRUPT;
                        return FALSE;
                    }
                }
                else {
                    char buf[32];
                    int size = snprintf(buf, 32, "\\%c", *p);

                    if(!fd_write(nextout, buf, size)) {
                        err_msg("signal interrupt", runinfo->mSName, runinfo->mSLine, runinfo->mArgs[0]);
                        runinfo->mRCode = RCODE_SIGNAL_INTERRUPT;
                        return FALSE;
                    }
                }

                p++;
            }
        } else if(code == kSjis || code == kEucjp) {
            while(*p) {
                if(is_kanji(code, *p)) {
                    char buf[32];
                    int size = snprintf(buf, 32, "%c%c", *p, *(p+1));
                    p++;

                    if(!fd_write(nextout, buf, size)) {
                        err_msg("signal interrupt", runinfo->mSName, runinfo->mSLine, runinfo->mArgs[0]);
                        runinfo->mRCode = RCODE_SIGNAL_INTERRUPT;
                        return FALSE;
                    }
                }
                else if(isalpha(*p)) {
                    char buf[32];
                    int size = snprintf(buf, 32, "%c", *p);

                    if(!fd_write(nextout, buf, size)) {
                        err_msg("signal interrupt", runinfo->mSName, runinfo->mSLine, runinfo->mArgs[0]);
                        runinfo->mRCode = RCODE_SIGNAL_INTERRUPT;
                        return FALSE;
                    }
                }
                else {
                    char buf[32];
                    int size = snprintf(buf, 32, "\\%c", *p);

                    if(!fd_write(nextout, buf, size)) {
                        err_msg("signal interrupt", runinfo->mSName, runinfo->mSLine, runinfo->mArgs[0]);
                        runinfo->mRCode = RCODE_SIGNAL_INTERRUPT;
                        return FALSE;
                    }
                }

                p++;
            }
        } else if(code == kUtf8) {
            while(*p) {
                if(is_kanji(code, *p) || isalpha(*p)) {
                    char buf[32];
                    int size = snprintf(buf, 32, "%c", *p);

                    if(!fd_write(nextout, buf, size)) {
                        err_msg("signal interrupt", runinfo->mSName, runinfo->mSLine, runinfo->mArgs[0]);
                        runinfo->mRCode = RCODE_SIGNAL_INTERRUPT;
                        return FALSE;
                    }
                }
                else {
                    char buf[32];
                    int size = snprintf(buf, 32, "\\%c", *p);

                    if(!fd_write(nextout, buf, size)) {
                        err_msg("signal interrupt", runinfo->mSName, runinfo->mSLine, runinfo->mArgs[0]);
                        runinfo->mRCode = RCODE_SIGNAL_INTERRUPT;
                        return FALSE;
                    }
                }

                p++;
            }
        }

        if(SFD(nextin).mBufLen == 0) {
            runinfo->mRCode = RCODE_NFUN_NULL_INPUT;
        }
        else {
            runinfo->mRCode = 0;
        }
    }

    return TRUE;
}

BOOL cmd_length(sObject* nextin, sObject* nextout, sRunInfo* runinfo)
{
    enum eKanjiCode code = gKanjiCode;
    if(sRunInfo_option(runinfo, "-byte")) {
        code = kByte;
    }
    else if(sRunInfo_option(runinfo, "-utf8")) {
        code = kUtf8;
    }
    else if(sRunInfo_option(runinfo, "-sjis")) {
        code = kSjis;
    }
    else if(sRunInfo_option(runinfo, "-eucjp")) {
        code = kEucjp;
    }

    eLineField lf = gLineField;
    if(sRunInfo_option(runinfo, "-Lw")) {
        lf = kCRLF;
    }
    else if(sRunInfo_option(runinfo, "-Lm")) {
        lf = kCR;
    }
    else if(sRunInfo_option(runinfo, "-Lu")) {
        lf = kLF;
    }
    else if(sRunInfo_option(runinfo, "-La")) {
        lf = kBel;
    }

    /// output
    if(runinfo->mFilter) {
        /// line number
        if(sRunInfo_option(runinfo, "-line-num")) {
            int result = 0;
            char* p = SFD(nextin).mBuf;
            if(lf == kCRLF) {
                while(1) {
                    if(*p == '\r' && *(p+1) == '\n') {
                        p+=2;
                        result++;
                    }
                    else if(*p == 0) {
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
                        p++;
                        result++;
                    }
                    else if(*p == 0) {
                        break;
                    }
                    else {
                        p++;
                    }
                }
            }

            char buf[128];
            int size = snprintf(buf, 128, "%d\n", result);

            if(!fd_write(nextout, buf, size)) {
                err_msg("signal interrupt", runinfo->mSName, runinfo->mSLine, runinfo->mArgs[0]);
                runinfo->mRCode = RCODE_SIGNAL_INTERRUPT;
                return FALSE;
            }
        }
        else {
            char* arg = SFD(nextin).mBuf;
            int len = str_kanjilen(code, arg);

            char buf[128];
            int size = snprintf(buf, 128, "%d\n", len);

            if(!fd_write(nextout, buf, size)) {
                err_msg("signal interrupt", runinfo->mSName, runinfo->mSLine, runinfo->mArgs[0]);
                runinfo->mRCode = RCODE_SIGNAL_INTERRUPT;
                return FALSE;
            }
        }

        if(SFD(nextin).mBufLen == 0) {
            runinfo->mRCode = RCODE_NFUN_NULL_INPUT;
        }
        else {
            runinfo->mRCode = 0;
        }
    }

    return TRUE;
}

BOOL cmd_x(sObject* nextin, sObject* nextout, sRunInfo* runinfo)
{
    /// input
    if(runinfo->mFilter) {
        if(runinfo->mArgsNumRuntime == 2) {
            int multiple = atoi(runinfo->mArgsRuntime[1]);
            if(multiple < 1) multiple = 1;

            int i;
            for(i=0; i<multiple; i++) {
                if(!fd_write(nextout, SFD(nextin).mBuf, SFD(nextin).mBufLen)) {
                    err_msg("signal interrupt", runinfo->mSName, runinfo->mSLine, runinfo->mArgs[0]);
                    runinfo->mRCode = RCODE_SIGNAL_INTERRUPT;
                    return FALSE;
                }
            }


            if(SFD(nextin).mBufLen == 0) {
                runinfo->mRCode = RCODE_NFUN_NULL_INPUT;
            }
            else {
                runinfo->mRCode = 0;
            }
        }
    }

    return TRUE;
}


BOOL cmd_lc(sObject* nextin, sObject* nextout, sRunInfo* runinfo)
{
    enum eKanjiCode code = gKanjiCode;
    if(sRunInfo_option(runinfo, "-byte")) {
        code = kByte;
    }
    else if(sRunInfo_option(runinfo, "-utf8")) {
        code = kUtf8;
    }
    else if(sRunInfo_option(runinfo, "-sjis")) {
        code = kSjis;
    }
    else if(sRunInfo_option(runinfo, "-eucjp")) {
        code = kEucjp;
    }

    if(runinfo->mFilter) {
        sObject* str = STRING_NEW_STACK(SFD(nextin).mBuf);
        string_tolower(str, code);

        if(!fd_write(nextout, string_c_str(str), string_length(str))) {
            err_msg("signal interrupt", runinfo->mSName, runinfo->mSLine, runinfo->mArgs[0]);
            runinfo->mRCode = RCODE_SIGNAL_INTERRUPT;
            return FALSE;
        }

        if(SFD(nextin).mBufLen == 0) {
            runinfo->mRCode = RCODE_NFUN_NULL_INPUT;
        }
        else {
            runinfo->mRCode = 0;
        }
    }

    return TRUE;
}

BOOL cmd_uc(sObject* nextin, sObject* nextout, sRunInfo* runinfo)
{
    enum eKanjiCode code = gKanjiCode;
    if(sRunInfo_option(runinfo, "-byte")) {
        code = kByte;
    }
    else if(sRunInfo_option(runinfo, "-utf8")) {
        code = kUtf8;
    }
    else if(sRunInfo_option(runinfo, "-sjis")) {
        code = kSjis;
    }
    else if(sRunInfo_option(runinfo, "-eucjp")) {
        code = kEucjp;
    }

    if(runinfo->mFilter) {
        sObject* str = STRING_NEW_STACK(SFD(nextin).mBuf);
        string_toupper(str, code);

        if(!fd_write(nextout, string_c_str(str), string_length(str))) {
            err_msg("signal interrupt", runinfo->mSName, runinfo->mSLine, runinfo->mArgs[0]);
            runinfo->mRCode = RCODE_SIGNAL_INTERRUPT;
            return FALSE;
        }

        if(SFD(nextin).mBufLen == 0) {
            runinfo->mRCode = RCODE_NFUN_NULL_INPUT;
        }
        else {
            runinfo->mRCode = 0;
        }
    }

    return TRUE;
}

BOOL cmd_chomp(sObject* nextin, sObject* nextout, sRunInfo* runinfo)
{
    if(runinfo->mFilter) {
        sObject* str = STRING_NEW_STACK(SFD(nextin).mBuf);
        if(string_chomp(str)) {
            if(SFD(nextin).mBufLen == 0) {
                runinfo->mRCode = RCODE_NFUN_NULL_INPUT;
            }
            else {
                runinfo->mRCode = 0;
            }
        }
        else {
            if(SFD(nextin).mBufLen == 0) {
                runinfo->mRCode = RCODE_NFUN_NULL_INPUT;
            }
            else {
                runinfo->mRCode = 1;
            }
        }

        if(!fd_write(nextout, string_c_str(str), string_length(str))) {
            err_msg("signal interrupt", runinfo->mSName, runinfo->mSLine, runinfo->mArgs[0]);
            runinfo->mRCode = RCODE_SIGNAL_INTERRUPT;
            return FALSE;
        }
    }

    return TRUE;
}

BOOL cmd_chop(sObject* nextin, sObject* nextout, sRunInfo* runinfo)
{
    enum eKanjiCode code = gKanjiCode;
    if(sRunInfo_option(runinfo, "-byte")) {
        code = kByte;
    }
    else if(sRunInfo_option(runinfo, "-utf8")) {
        code = kUtf8;
    }
    else if(sRunInfo_option(runinfo, "-sjis")) {
        code = kSjis;
    }
    else if(sRunInfo_option(runinfo, "-eucjp")) {
        code = kEucjp;
    }

    if(runinfo->mFilter) {
        sObject* str = STRING_NEW_STACK(SFD(nextin).mBuf);

        if(code == kByte) {
            char* s = string_c_str(str);
            const int len = strlen(s);
            
            if(len >= 2 && s[len-2] == '\r' && s[len-1] == '\n')
            {
                string_trunc(str, len-2);

                if(SFD(nextin).mBufLen == 0) {
                    runinfo->mRCode = RCODE_NFUN_NULL_INPUT;
                }
                else {
                    runinfo->mRCode = 0;
                }
            }
            else if(len >= 1) {
                string_trunc(str, len-1);

                if(SFD(nextin).mBufLen == 0) {
                    runinfo->mRCode = RCODE_NFUN_NULL_INPUT;
                }
                else {
                    runinfo->mRCode = 0;
                }
            }
            else {
                if(SFD(nextin).mBufLen == 0) {
                    runinfo->mRCode = RCODE_NFUN_NULL_INPUT;
                }
                else {
                    runinfo->mRCode = 1;
                }
            }
        }
        else {
            char* s = string_c_str(str);
            const int len = str_kanjilen(code, s);

            char* last_char_head = str_kanjipos2pointer(code, s, len-1);
            string_erase(str, last_char_head-s, s + strlen(s) - last_char_head);

            if(len > 0) {
                if(SFD(nextin).mBufLen == 0) {
                    runinfo->mRCode = RCODE_NFUN_NULL_INPUT;
                }
                else {
                    runinfo->mRCode = 0;
                }
            }
            else {
                if(SFD(nextin).mBufLen == 0) {
                    runinfo->mRCode = RCODE_NFUN_NULL_INPUT;
                }
                else {
                    runinfo->mRCode = 1;
                }
            }
        }

        if(!fd_write(nextout, string_c_str(str), string_length(str))) {
            err_msg("signal interrupt", runinfo->mSName, runinfo->mSLine, runinfo->mArgs[0]);
            runinfo->mRCode = RCODE_SIGNAL_INTERRUPT;
            return FALSE;
        }
    }

    return TRUE;
}

BOOL cmd_strip(sObject* nextin, sObject* nextout, sRunInfo* runinfo)
{
    if(runinfo->mFilter) {
        BOOL first = TRUE;
        char* last_point = NULL;
        char* p = SFD(nextin).mBuf;
        while(*p) {
            if(*p == '\n' || *p =='\r' || *p == '\t' || *p == ' ' || *p == '\a')
            {
                if(!first) {
                    if(!fd_writec(nextout, *p++)) {
                        err_msg("signal interrupt", runinfo->mSName, runinfo->mSLine, runinfo->mArgs[0]);
                        runinfo->mRCode = RCODE_SIGNAL_INTERRUPT;
                        return FALSE;
                    }
                }
                else {
                    p++;
                }
            }
            else {
                first = FALSE;
                last_point = p;
                if(!fd_writec(nextout, *p++)) {
                    err_msg("signal interrupt", runinfo->mSName, runinfo->mSLine, runinfo->mArgs[0]);
                    runinfo->mRCode = RCODE_SIGNAL_INTERRUPT;
                    return FALSE;
                }
            }
        }

        const int space_of_tail_len = SFD(nextin).mBufLen - (last_point - SFD(nextin).mBuf) - 1;
        if(space_of_tail_len > 0) {
            fd_trunc(nextout, SFD(nextout).mBufLen - space_of_tail_len);
        }

        if(SFD(nextin).mBufLen == 0) {
            runinfo->mRCode = RCODE_NFUN_NULL_INPUT;
        }
        else {
            runinfo->mRCode = 0;
        }
    }

    return TRUE;
}

BOOL cmd_pomch(sObject* nextin, sObject* nextout, sRunInfo* runinfo)
{
    eLineField lf = gLineField;
    if(sRunInfo_option(runinfo, "-Lw")) {
        lf = kCRLF;
    }
    else if(sRunInfo_option(runinfo, "-Lm")) {
        lf = kCR;
    }
    else if(sRunInfo_option(runinfo, "-Lu")) {
        lf = kLF;
    }
    else if(sRunInfo_option(runinfo, "-La")) {
        lf = kBel;
    }

    if(runinfo->mFilter) {
        sObject* str = STRING_NEW_STACK(SFD(nextin).mBuf);
        if(string_pomch(str, lf)) {
            if(SFD(nextin).mBufLen == 0) {
                runinfo->mRCode = RCODE_NFUN_NULL_INPUT;
            }
            else {
                runinfo->mRCode = 0;
            }
        }
        else {
            if(SFD(nextin).mBufLen == 0) {
                runinfo->mRCode = RCODE_NFUN_NULL_INPUT;
            }
            else {
                runinfo->mRCode = 1;
            }
        }

        if(!fd_write(nextout, string_c_str(str), string_length(str))) {
            err_msg("signal interrupt", runinfo->mSName, runinfo->mSLine, runinfo->mArgs[0]);
            runinfo->mRCode = RCODE_SIGNAL_INTERRUPT;
            return FALSE;
        }
    }

    return TRUE;
}

BOOL cmd_printf(sObject* nextin, sObject* nextout, sRunInfo* runinfo)
{
    eLineField lf = gLineField;
    if(sRunInfo_option(runinfo, "-Lw")) {
        lf = kCRLF;
    }
    else if(sRunInfo_option(runinfo, "-Lm")) {
        lf = kCR;
    }
    else if(sRunInfo_option(runinfo, "-Lu")) {
        lf = kLF;
    }
    else if(sRunInfo_option(runinfo, "-La")) {
        lf = kBel;
    }

    if(runinfo->mArgsNumRuntime == 2) {
        if(runinfo->mFilter) {
            fd_split(nextin, lf);
        }

        /// go ///
        char* p = runinfo->mArgsRuntime[1];

        int strings_num = 0;
        while(*p) {
            if(*p == '%') {
                p++;

                if(*p == '%') {
                    p++;
                    if(!fd_writec(nextout, '%')) {
                        err_msg("signal interrupt", runinfo->mSName, runinfo->mSLine, runinfo->mArgs[0]);
                        runinfo->mRCode = RCODE_SIGNAL_INTERRUPT;
                        return FALSE;
                    }
                }
                else {
                    if(*p == 'd' || *p == 'i' || *p == 'o' || *p == 'u' || *p == 'x' || *p == 'X' || *p == 'c') 
                    {
                        char aformat[16];
                        snprintf(aformat, 16, "%%%c", *p);
                        p++;

                        char* arg;
                        if(runinfo->mFilter && strings_num < vector_count(SFD(nextin).mLines)) {
                            arg = vector_item(SFD(nextin).mLines, strings_num);
                        }
                        else {
                            arg = "0";
                        }
                        strings_num++;

                        char* buf;
                        int size = asprintf(&buf, aformat, atoi(arg));

                        if(!fd_write(nextout, buf, size)) {
                            err_msg("signal interrupt", runinfo->mSName, runinfo->mSLine, runinfo->mArgs[0]);
                            runinfo->mRCode = RCODE_SIGNAL_INTERRUPT;
                            return FALSE;
                        }
                        free(buf);
                    }
                    else if(*p == 'e' || *p == 'E' || *p == 'f' || *p == 'F' || *p == 'g' || *p == 'G' || *p == 'a' || *p == 'A')
                    {
                        char aformat[16];
                        snprintf(aformat, 16, "%%%c", *p);
                        p++;

                        char* arg;
                        if(runinfo->mFilter && strings_num < vector_count(SFD(nextin).mLines)) {
                            arg = vector_item(SFD(nextin).mLines, strings_num);
                        }
                        else {
                            arg = "0";
                        }
                        strings_num++;

                        char* buf;
                        int size = asprintf(&buf, aformat, atof(arg));

                        if(!fd_write(nextout, buf, size)) {
                            err_msg("signal interrupt", runinfo->mSName, runinfo->mSLine, runinfo->mArgs[0]);
                            runinfo->mRCode = RCODE_SIGNAL_INTERRUPT;
                            return FALSE;
                        }
                        free(buf);
                    }
                    else if(*p == 's') {
                        char aformat[16];
                        snprintf(aformat, 16, "%%%c", *p);
                        p++;

                        sObject* arg;
                        if(runinfo->mFilter && strings_num < vector_count(SFD(nextin).mLines)) {
                            arg = STRING_NEW_STACK(vector_item(SFD(nextin).mLines, strings_num));
                        }
                        else {
                            arg = STRING_NEW_STACK("");
                        }
                        strings_num++;

                        string_chomp(arg);

                        char* buf;
                        int size = asprintf(&buf, aformat, string_c_str(arg));

                        if(!fd_write(nextout, buf, size)) {
                            err_msg("signal interrupt", runinfo->mSName, runinfo->mSLine, runinfo->mArgs[0]);
                            runinfo->mRCode = RCODE_SIGNAL_INTERRUPT;
                            return FALSE;
                        }
                        free(buf);
                    }
                    else {
                        err_msg("invalid format at printf", runinfo->mSName, runinfo->mSLine, runinfo->mArgs[0]);
                        return FALSE;
                    }
                }
            }
            else {
                if(!fd_writec(nextout, *p++)) {
                    err_msg("signal interrupt", runinfo->mSName, runinfo->mSLine, runinfo->mArgs[0]);
                    runinfo->mRCode = RCODE_SIGNAL_INTERRUPT;
                    return FALSE;
                }
            }
        }

        if(runinfo->mFilter) {
            if(strings_num > vector_count(SFD(nextin).mLines)) {
                if(SFD(nextin).mBufLen == 0) {
                    runinfo->mRCode = RCODE_NFUN_NULL_INPUT;
                }
                else {
                    runinfo->mRCode = 1;
                }
            }
            else {
                if(SFD(nextin).mBufLen == 0) {
                    runinfo->mRCode = RCODE_NFUN_NULL_INPUT;
                }
                else {
                    runinfo->mRCode = 0;
                }
            }
        }
        else {
            runinfo->mRCode = 0;
        }
    }

    return TRUE;
}

BOOL cmd_add(sObject* nextin, sObject* nextout, sRunInfo* runinfo)
{
    enum eKanjiCode code = gKanjiCode;
    if(sRunInfo_option(runinfo, "-byte")) {
        code = kByte;
    }
    else if(sRunInfo_option(runinfo, "-utf8")) {
        code = kUtf8;
    }
    else if(sRunInfo_option(runinfo, "-sjis")) {
        code = kSjis;
    }
    else if(sRunInfo_option(runinfo, "-eucjp")) {
        code = kEucjp;
    }

    if(runinfo->mFilter && runinfo->mArgsNumRuntime == 2) {
        char* arg = runinfo->mArgsRuntime[1];
        int number;
        char* argument;
        if(argument = sRunInfo_option_with_argument(runinfo, "-number")) {
            number = atoi(argument);
        }
        else if(argument = sRunInfo_option_with_argument(runinfo, "-index")) {
            number = atoi(argument);
        }
        else {
            number = -1;
        }

        const int len = str_kanjilen(code, SFD(nextin).mBuf);

        if(number < 0) {
            number += len + 1;
            if(number < 0) number = 0;
        }

        if(number < len) {
            int point = str_kanjipos2pointer(code, SFD(nextin).mBuf, number) - SFD(nextin).mBuf;
            if(!fd_write(nextout, SFD(nextin).mBuf, point)) {
                err_msg("signal interrupt", runinfo->mSName, runinfo->mSLine, runinfo->mArgs[0]);
                runinfo->mRCode = RCODE_SIGNAL_INTERRUPT;
                return FALSE;
            }
            if(!fd_write(nextout, arg, strlen(arg))) {
                err_msg("signal interrupt", runinfo->mSName, runinfo->mSLine, runinfo->mArgs[0]);
                runinfo->mRCode = RCODE_SIGNAL_INTERRUPT;
                return FALSE;
            }
            if(!fd_write(nextout, SFD(nextin).mBuf + point, SFD(nextin).mBufLen - point)) {
                err_msg("signal interrupt", runinfo->mSName, runinfo->mSLine, runinfo->mArgs[0]);
                runinfo->mRCode = RCODE_SIGNAL_INTERRUPT;
                return FALSE;
            }
        }
        else {
            if(!fd_write(nextout, SFD(nextin).mBuf, SFD(nextin).mBufLen)) {
                err_msg("signal interrupt", runinfo->mSName, runinfo->mSLine, runinfo->mArgs[0]);
                runinfo->mRCode = RCODE_SIGNAL_INTERRUPT;
                return FALSE;
            }
            if(!fd_write(nextout, arg, strlen(arg))) {
                err_msg("signal interrupt", runinfo->mSName, runinfo->mSLine, runinfo->mArgs[0]);
                runinfo->mRCode = RCODE_SIGNAL_INTERRUPT;
                return FALSE;
            }
        }

        if(SFD(nextin).mBufLen == 0) {
            runinfo->mRCode = RCODE_NFUN_NULL_INPUT;
        }
        else {
            runinfo->mRCode = 0;
        }
    }

    return TRUE;
}

BOOL cmd_del(sObject* nextin, sObject* nextout, sRunInfo* runinfo)
{
    enum eKanjiCode code = gKanjiCode;
    if(sRunInfo_option(runinfo, "-byte")) {
        code = kByte;
    }
    else if(sRunInfo_option(runinfo, "-utf8")) {
        code = kUtf8;
    }
    else if(sRunInfo_option(runinfo, "-sjis")) {
        code = kSjis;
    }
    else if(sRunInfo_option(runinfo, "-eucjp")) {
        code = kEucjp;
    }

    
    if(runinfo->mFilter && runinfo->mArgsNumRuntime == 2) {
        char* arg = runinfo->mArgsRuntime[1];
        int index = atoi(arg);

        int number;
        char* argument;
        if(argument = sRunInfo_option_with_argument(runinfo, "-number")) {
            number = atoi(argument);
        }
        else {
            number = 1;
        }

        const int len = str_kanjilen(code, SFD(nextin).mBuf);

        if(index < 0) {
            index += len;
            if(index < 0) index = 0;
        }
        if(index >= len) {
            index = len -1;
            if(index < 0) index = 0;
        }

        int point = str_kanjipos2pointer(code, SFD(nextin).mBuf, index) - SFD(nextin).mBuf;
        
        if(!fd_write(nextout, SFD(nextin).mBuf, point)) {
            err_msg("signal interrupt", runinfo->mSName, runinfo->mSLine, runinfo->mArgs[0]);
            runinfo->mRCode = RCODE_SIGNAL_INTERRUPT;
            return FALSE;
        }
        if(index + number < len) {
            char* point = str_kanjipos2pointer(code, SFD(nextin).mBuf, index + number);
            if(!fd_write(nextout, point, strlen(point))) {
                err_msg("signal interrupt", runinfo->mSName, runinfo->mSLine, runinfo->mArgs[0]);
                runinfo->mRCode = RCODE_SIGNAL_INTERRUPT;
                return FALSE;
            }
        }

        if(SFD(nextin).mBufLen == 0) {
            runinfo->mRCode = RCODE_NFUN_NULL_INPUT;
        }
        else {
            runinfo->mRCode = 0;
        }
    }

    return TRUE;
}

BOOL cmd_rows(sObject* nextin, sObject* nextout, sRunInfo* runinfo)
{
    enum eKanjiCode code = gKanjiCode;
    if(sRunInfo_option(runinfo, "-byte")) {
        code = kByte;
    }
    else if(sRunInfo_option(runinfo, "-utf8")) {
        code = kUtf8;
    }
    else if(sRunInfo_option(runinfo, "-sjis")) {
        code = kSjis;
    }
    else if(sRunInfo_option(runinfo, "-eucjp")) {
        code = kEucjp;
    }

    if(runinfo->mFilter && runinfo->mArgsNumRuntime > 1 && runinfo->mBlocksNum <= runinfo->mArgsNumRuntime-1) {
        if(SFD(nextin).mBufLen == 0) {
            runinfo->mRCode = RCODE_NFUN_NULL_INPUT;
        }
        else {
            runinfo->mRCode = 0;
        }

        int i;
        for(i=1; i<runinfo->mArgsNumRuntime; i++) {
            char* arg = runinfo->mArgsRuntime[i];
            char* p;
            if(p = strstr(arg, "..")) {
                char buf[128+1];
                char buf2[128+1];
                const int len = p - arg;
                const int len2 = arg + strlen(arg) - (p + 2);
                if(len < 128 || len2 < 128) {
                    memcpy(buf, arg, len);
                    buf[len] = 0;

                    memcpy(buf2, p + 2, len2);
                    buf2[len2] = 0;
                }
                else {
                    err_msg("invalid range", runinfo->mSName, runinfo->mSLine, runinfo->mArgs[0]);
                    return FALSE;
                }

                const int kanjilen = str_kanjilen(code, SFD(nextin).mBuf);
                if(kanjilen > 0) {
                    int first = atoi(buf);
                    int second = atoi(buf2);

                    if(first < 0) {
                        first += kanjilen;
                        if(first < 0) first = 0;
                    }
                    if(second < 0) {
                        second += kanjilen;
                        if(second < 0) second = 0;
                    }
                    if(first >= kanjilen) {
                        first = kanjilen -1;
                        if(first < 0) first = 0;
                    }
                    if(second >= kanjilen) {
                        second = kanjilen -1;
                        if(second < 0) second = 0;
                    }

                    /// make table to indexing access ///
                    char** array = MALLOC(sizeof(char*)*(kanjilen+1));
                    if(code == kByte) {
                        int k;
                        for(k=0; k<kanjilen; k++) {
                            array[k] = SFD(nextin).mBuf + k;
                        }
                        array[k] = SFD(nextin).mBuf + k;
                    }
                    else if(code == kUtf8) {
                        char* p = SFD(nextin).mBuf;

                        int k;
                        for(k=0; k<kanjilen; k++) {
                            array[k] = p;
                            if(((unsigned char)*p) > 127) {
                                const int size = ((*p & 0x80) >> 7) + ((*p & 0x40) >> 6) + ((*p & 0x20) >> 5) + ((*p & 0x10) >> 4);
                                p+=size;
                            }
                            else {
                                p++;
                            }
                        }
                        array[k] = p;
                    }
                    else {
                        char* p = SFD(nextin).mBuf;

                        int k;
                        for(k=0; k<kanjilen; k++) {
                            const int size = is_kanji(code, *p) ? 2 : 1;
                            array[k] = p;
                            p+=size;
                        }
                        array[k] = p;
                    }

                    if(first < second) {
                        sObject* nextin2 = FD_NEW_STACK();

                        int j;
                        for(j=first; j<=second; j++) {
                            fd_clear(nextin2);

                            if(!fd_write(nextin2, array[j], array[j+1] -array[j])) {
                                err_msg("interrupt", runinfo->mSName, runinfo->mSLine, runinfo->mArgs[0]);
                                runinfo->mRCode = RCODE_SIGNAL_INTERRUPT;
                                FREE(array);
                                return FALSE;
                            }

                            if(i-1 < runinfo->mBlocksNum) {
                                int rcode = 0;
                                if(!run(runinfo->mBlocks[i-1], nextin2, nextout, &rcode, runinfo->mCurrentObject, runinfo->mRunningObject)) {
                                    runinfo->mRCode = rcode;
                                    FREE(array);
                                    return FALSE;
                                }
                                runinfo->mRCode = rcode;
                            }
                            else {
                                if(!fd_write(nextout, SFD(nextin2).mBuf, SFD(nextin2).mBufLen)) 
                                {
                                    err_msg("interrupt", runinfo->mSName, runinfo->mSLine, runinfo->mArgs[0]);
                                    runinfo->mRCode = RCODE_SIGNAL_INTERRUPT;
                                    FREE(array);
                                    return FALSE;
                                }
                                runinfo->mRCode = 0;
                            }
                        }
                    }
                    else {
                        sObject* nextin2 = FD_NEW_STACK();

                        int j;
                        for(j=first; j>=second; j--) {
                            fd_clear(nextin2);

                            if(!fd_write(nextin2, array[j], array[j+1]-array[j])) {
                                err_msg("interrupt", runinfo->mSName, runinfo->mSLine, runinfo->mArgs[0]);
                                runinfo->mRCode = RCODE_SIGNAL_INTERRUPT;
                                FREE(array);
                                return FALSE;
                            }

                            if(i-1 < runinfo->mBlocksNum) {
                                int rcode = 0;
                                if(!run(runinfo->mBlocks[i-1], nextin2, nextout, &rcode, runinfo->mCurrentObject, runinfo->mRunningObject)) {
                                    runinfo->mRCode = rcode;
                                    FREE(array);
                                    return FALSE;
                                }
                                runinfo->mRCode = rcode;
                            }
                            else {
                                if(!fd_write(nextout, SFD(nextin2).mBuf, SFD(nextin2).mBufLen)) 
                                {
                                    err_msg("interrupt", runinfo->mSName, runinfo->mSLine, runinfo->mArgs[0]);
                                    runinfo->mRCode = RCODE_SIGNAL_INTERRUPT;
                                    FREE(array);
                                    return FALSE;
                                }
                                runinfo->mRCode = 0;
                            }
                        }
                    }

                    FREE(array);
                }
            }
            else {
                const int len = str_kanjilen(code, SFD(nextin).mBuf);
                int num = atoi(arg);

                if(num < 0) {
                    num += len;
                    if(num < 0) num = 0;
                }
                if(num >= len) {
                    num = len -1;
                    if(num < 0) num = 0;
                }

                sObject* nextin2 = FD_NEW_STACK();

                char* str = str_kanjipos2pointer(code, SFD(nextin).mBuf, num);
                char* str2 = str_kanjipos2pointer(code, str, 1);
                if(!fd_write(nextin2, str, str2 -str)) {
                    err_msg("interrupt", runinfo->mSName, runinfo->mSLine, runinfo->mArgs[0]);
                    runinfo->mRCode = RCODE_SIGNAL_INTERRUPT;
                    return FALSE;
                }

                if(i-1 < runinfo->mBlocksNum) {
                    int rcode = 0;
                    if(!run(runinfo->mBlocks[i-1], nextin2, nextout, &rcode, runinfo->mCurrentObject, runinfo->mRunningObject)) {
                        runinfo->mRCode = rcode;
                        return FALSE;
                    }
                    runinfo->mRCode = rcode;
                }
                else {
                    if(!fd_write(nextout, SFD(nextin2).mBuf, SFD(nextin2).mBufLen)) 
                    {
                        err_msg("interrupt", runinfo->mSName, runinfo->mSLine, runinfo->mArgs[0]);
                        runinfo->mRCode = RCODE_SIGNAL_INTERRUPT;
                        return FALSE;
                    }
                    runinfo->mRCode = 0;
                }
            }
        }
    }

    return TRUE;
}

void transform_argument_to_tr_style_chars_on_byte(sRunInfo* runinfo, int* chars_len, char** chars, int* chars_num, int* rchars_len, char** rchars, int* rchars_num)
{
    int i;
    for(i=1; i<runinfo->mArgsNumRuntime; i++) {
        char* pattern = runinfo->mArgsRuntime[i];

        /// split pattern ///
        char* p = pattern;
        if(*p == '^') {
            p++;

            while(*p) {
                if(*(p+1) == '-' && *(p+2) != 0) {
                    if(*p >= '0' && *p <= '9' && *(p+2) >= '0' && *(p+2) <= '9') {
                        char c = *p;
                        while(c <= *(p+2)) {
                            if(*rchars_num >= *rchars_len) {
                                (*rchars_len) *= 2;
                                *rchars = REALLOC(*rchars, sizeof(char)*(*rchars_len));
                            }
                            (*rchars)[(*rchars_num)++] = c++;
                        }
                        p+=3;
                    }
                    else if(*p >= 'a' && *p <='z' && *(p+2) >= 'a' && *(p+2) <= 'z') {
                        char c = *p;
                        while(c <= *(p+2)) {
                            if(*rchars_num >= *rchars_len) {
                                (*rchars_len) *= 2;
                                *rchars = REALLOC(*rchars, sizeof(char)*(*rchars_len));
                            }
                            (*rchars)[(*rchars_num)++] = c++;
                        }
                        p+=3;
                    }
                    else if(*p >= 'A' && *p <='Z' && *(p+2) >= 'A' && *(p+2) <= 'Z') {
                        char c = *p;
                        while(c <= *(p+2)) {
                            if(*rchars_num >= *rchars_len) {
                                (*rchars_len) *= 2;
                                *rchars = REALLOC(*rchars, sizeof(char)*(*rchars_len));
                            }
                            (*rchars)[(*rchars_num)++] = c++;
                        }
                        p+=3;
                    }
                    else {
                        p+=3;
                    }
                }
                else {
                    if(*rchars_num >= *rchars_len) {
                        (*rchars_len) *= 2;
                        *rchars = REALLOC(*rchars, sizeof(char)*(*rchars_len));
                    }
                    (*rchars)[(*rchars_num)++] = *p++;
                }
            }
            (*rchars)[*rchars_num] = 0;
        }
        else {
            while(*p) {
                if(*(p+1) == '-' && *(p+2) != 0) {
                    if(*p >= '0' && *p <= '9' && *(p+2) >= '0' && *(p+2) <= '9') {
                        char c = *p;
                        while(c <= *(p+2)) {
                            if(*chars_num >= *chars_len) {
                                (*chars_len) *= 2;
                                *chars = REALLOC(chars, sizeof(char)*(*chars_len));
                            }
                            (*chars)[(*chars_num)++] = c++;
                        }
                        p+=3;
                    }
                    else if(*p >= 'a' && *p <='z' && *(p+2) >= 'a' && *(p+2) <= 'z') {
                        char c = *p;
                        while(c <= *(p+2)) {
                            if(*chars_num >= *chars_len) {
                                (*chars_len) *= 2;
                                *chars = REALLOC(chars, sizeof(char)*(*chars_len));
                            }
                            (*chars)[(*chars_num)++] = c++;
                        }
                        p+=3;
                    }
                    else if(*p >= 'A' && *p <='Z' && *(p+2) >= 'A' && *(p+2) <= 'Z') {
                        char c = *p;
                        while(c <= *(p+2)) {
                            if(*chars_num >= *chars_len) {
                                (*chars_len) *= 2;
                                *chars = REALLOC(*chars, sizeof(char)*(*chars_len));
                            }
                            (*chars)[(*chars_num)++] = c++;
                        }
                        p+=3;
                    }
                    else {
                        p+=3;
                    }
                }
                else {
                    if(*chars_num >= *chars_len) {
                        (*chars_len) *= 2;
                        *chars = REALLOC(chars, sizeof(char)*(*chars_len));
                    }
                    (*chars)[(*chars_num)++] = *p++;
                }
            }
            (*chars)[*chars_num] = 0;
        }
    }
}

void transform_argument_to_tr_style_chars_on_utf8(sRunInfo* runinfo, int* chars_num, int* chars_size, char*** chars, int* rchars_num, int* rchars_size, char*** rchars)
{
    int i;
    for(i=1; i<runinfo->mArgsNumRuntime; i++) {
        char* pattern = runinfo->mArgsRuntime[i];

        /// split pattern ///
        if(*pattern == '^') {
            char* p = pattern + 1;
            while(*p) {
                int size;
                if(((unsigned char)*p) > 127) {
                    size = ((*p & 0x80) >> 7) + ((*p & 0x40) >> 6) + ((*p & 0x20) >> 5) + ((*p & 0x10) >> 4);
                }
                else {
                    size = 1;
                }

                if(size == 1 && *(p+1) == '-' && *(p+2) != 0) {
                    if(*p >= '0' && *p <= '9' && *(p+2) >= '0' && *(p+2) <= '9') {
                        char c = *p;
                        while(c <= *(p+2)) {
                            if(*rchars_num > *rchars_size) {
                                (*rchars_size) *= 2;
                                *rchars = REALLOC(*rchars, sizeof(char*)*(*rchars_size));
                            }

                            char* str = MALLOC(2);
                            *str = c++;
                            *(str+1) = 0;

                            (*rchars)[(*rchars_num)++] = str;
                        }
                        p+=3;
                    }
                    else if(*p >= 'a' && *p <='z' && *(p+2) >= 'a' && *(p+2) <= 'z') {
                        char c = *p;
                        while(c <= *(p+2)) {
                            if(*rchars_num > *rchars_size) {
                                (*rchars_size) *= 2;
                                *rchars = REALLOC(*rchars, sizeof(char*)*(*rchars_size));
                            }

                            char* str = MALLOC(2);
                            *str = c++;
                            *(str+1) = 0;

                            (*rchars)[(*rchars_num)++] = str;
                        }
                        p+=3;
                    }
                    else if(*p >= 'A' && *p <='Z' && *(p+2) >= 'A' && *(p+2) <= 'Z') {
                        char c = *p;
                        while(c <= *(p+2)) {
                            if(*rchars_num > *rchars_size) {
                                (*rchars_size) *= 2;
                                *rchars = REALLOC(*rchars, sizeof(char*)*(*rchars_size));
                            }

                            char* str = MALLOC(2);
                            *str = c++;
                            *(str+1) = 0;

                            (*rchars)[(*rchars_num)++] = str;
                        }
                        p+=3;
                    }
                    else {
                        p+=3;
                    }
                }
                else {
                    if(*rchars_num > *rchars_size) {
                        (*rchars_size) *= 2;
                        *rchars = REALLOC(*rchars, sizeof(char*)*(*rchars_size));
                    }

                    char* str = MALLOC(sizeof(char)*size + 1);
                    memcpy(str, p, size);
                    str[size] = 0;

                    (*rchars)[(*rchars_num)++] = str;

                    p+= size;
                }
            }
        }
        else {
            char* p = pattern;
            while(*p) {
                int size;
                if(((unsigned char)*p) > 127) {
                    size = ((*p & 0x80) >> 7) + ((*p & 0x40) >> 6) + ((*p & 0x20) >> 5) + ((*p & 0x10) >> 4);
                }
                else {
                    size = 1;
                }

                if(size == 1 && *(p+1) == '-' && *(p+2) != 0) {
                    if(*p >= '0' && *p <= '9' && *(p+2) >= '0' && *(p+2) <= '9') {
                        char c = *p;
                        while(c <= *(p+2)) {
                            if(*chars_num > *chars_size) {
                                (*chars_size) *= 2;
                                *chars = REALLOC(chars, sizeof(char*)*(*chars_size));
                            }

                            char* str = MALLOC(2);
                            *str = c++;
                            *(str+1) = 0;

                            (*chars)[(*chars_num)++] = str;
                        }
                        p+=3;
                    }
                    else if(*p >= 'a' && *p <='z' && *(p+2) >= 'a' && *(p+2) <= 'z') {
                        char c = *p;
                        while(c <= *(p+2)) {
                            if(*chars_num > *chars_size) {
                                (*chars_size) *= 2;
                                *chars = REALLOC(*chars, sizeof(char*)*(*chars_size));
                            }

                            char* str = MALLOC(2);
                            *str = c++;
                            *(str+1) = 0;

                            (*chars)[(*chars_num)++] = str;
                        }
                        p+=3;
                    }
                    else if(*p >= 'A' && *p <='Z' && *(p+2) >= 'A' && *(p+2) <= 'Z') {
                        char c = *p;
                        while(c <= *(p+2)) {
                            if(*chars_num > *chars_size) {
                                (*chars_size) *= 2;
                                *chars = REALLOC(*chars, sizeof(char*)*(*chars_size));
                            }

                            char* str = MALLOC(2);
                            *str = c++;
                            *(str+1) = 0;

                            (*chars)[(*chars_num)++] = str;
                        }
                        p+=3;
                    }
                    else {
                        p+=3;
                    }
                }
                else {
                    if(*chars_num > *chars_size) {
                        (*chars_size) *= 2;
                        *chars = REALLOC(*chars, sizeof(char*)*(*chars_size));
                    }

                    char* str = MALLOC(sizeof(char)*size + 1);
                    memcpy(str, p, size);
                    str[size] = 0;

                    (*chars)[(*chars_num)++] = str;

                    p+= size;
                }
            }
        }
    }
}

void transform_argument_to_tr_style_chars_on_sjis_or_eucjp(sRunInfo* runinfo, enum eKanjiCode code, int* chars_num, int* chars_size, char*** chars, int* rchars_num, int* rchars_size, char*** rchars)
{
    int i;
    for(i=1; i<runinfo->mArgsNumRuntime; i++) {
        char* pattern = runinfo->mArgsRuntime[i];

        /// split pattern ///
        if(*pattern == '^') {
            char* p = pattern + 1;
            while(*p) {
                int size;
                if(is_kanji(code, *p)) {
                    size = 2;
                }
                else {
                    size = 1;
                }

                if(size == 1 && *(p+1) == '-' && *(p+2) != 0) {
                    if(*p >= '0' && *p <= '9' && *(p+2) >= '0' && *(p+2) <= '9') {
                        char c = *p;
                        while(c <= *(p+2)) {
                            if(*rchars_num > *rchars_size) {
                                (*rchars_size) *= 2;
                                *rchars = REALLOC(*rchars, sizeof(char*)*(*rchars_size));
                            }

                            char* str = MALLOC(2);
                            *str = c++;
                            *(str+1) = 0;

                            (*rchars)[(*rchars_num)++] = str;
                        }
                        p+=3;
                    }
                    else if(*p >= 'a' && *p <='z' && *(p+2) >= 'a' && *(p+2) <= 'z') {
                        char c = *p;
                        while(c <= *(p+2)) {
                            if(*rchars_num > *rchars_size) {
                                (*rchars_size) *= 2;
                                *rchars = REALLOC(*rchars, sizeof(char*)*(*rchars_size));
                            }

                            char* str = MALLOC(2);
                            *str = c++;
                            *(str+1) = 0;

                            (*rchars)[(*rchars_num)++] = str;
                        }
                        p+=3;
                    }
                    else if(*p >= 'A' && *p <='Z' && *(p+2) >= 'A' && *(p+2) <= 'Z') {
                        char c = *p;
                        while(c <= *(p+2)) {
                            if(*rchars_num > *rchars_size) {
                                (*rchars_size) *= 2;
                                *rchars = REALLOC(*rchars, sizeof(char*)*(*rchars_size));
                            }

                            char* str = MALLOC(2);
                            *str = c++;
                            *(str+1) = 0;

                            (*rchars)[(*rchars_num)++] = str;
                        }
                        p+=3;
                    }
                    else {
                        p+=3;
                    }
                }
                else {
                    if(*rchars_num > *rchars_size) {
                        (*rchars_size) *= 2;
                        *rchars = REALLOC(*rchars, sizeof(char*)*(*rchars_size));
                    }

                    char* str = MALLOC(sizeof(char)*size + 1);
                    memcpy(str, p, size);
                    str[size] = 0;

                    (*rchars)[(*rchars_num)++] = str;

                    p+= size;
                }
            }
        }
        else {
            char* p = pattern;
            while(*p) {
                int size;
                if(is_kanji(code, *p)) {
                    size = 2;
                }
                else {
                    size = 1;
                }

                if(size == 1 && *(p+1) == '-' && *(p+2) != 0) {
                    if(*p >= '0' && *p <= '9' && *(p+2) >= '0' && *(p+2) <= '9') {
                        char c = *p;
                        while(c <= *(p+2)) {
                            if(*chars_num > *chars_size) {
                                (*chars_size) *= 2;
                                *chars = REALLOC(*chars, sizeof(char*)*(*chars_size));
                            }

                            char* str = MALLOC(2);
                            *str = c++;
                            *(str+1) = 0;

                            (*chars)[(*chars_num)++] = str;
                        }
                        p+=3;
                    }
                    else if(*p >= 'a' && *p <='z' && *(p+2) >= 'a' && *(p+2) <= 'z') {
                        char c = *p;
                        while(c <= *(p+2)) {
                            if(*chars_num > *chars_size) {
                                (*chars_size) *= 2;
                                *chars = REALLOC(*chars, sizeof(char*)*(*chars_size));
                            }

                            char* str = MALLOC(2);
                            *str = c++;
                            *(str+1) = 0;

                            (*chars)[(*chars_num)++] = str;
                        }
                        p+=3;
                    }
                    else if(*p >= 'A' && *p <='Z' && *(p+2) >= 'A' && *(p+2) <= 'Z') {
                        char c = *p;
                        while(c <= *(p+2)) {
                            if(*chars_num > *chars_size) {
                                (*chars_size) *= 2;
                                *chars = REALLOC(*chars, sizeof(char*)*(*chars_size));
                            }

                            char* str = MALLOC(2);
                            *str = c++;
                            *(str+1) = 0;

                            (*chars)[(*chars_num)++] = str;
                        }
                        p+=3;
                    }
                    else {
                        p+=3;
                    }
                }
                else {
                    if(*chars_num > *chars_size) {
                        (*chars_size) *= 2;
                        *chars = REALLOC(*chars, sizeof(char*)*(*chars_size));
                    }

                    char* str = MALLOC(sizeof(char)*size + 1);
                    memcpy(str, p, size);
                    str[size] = 0;

                    (*chars)[(*chars_num)++] = str;

                    p+= size;
                }
            }
        }
    }
}

BOOL cmd_count(sObject* nextin, sObject* nextout, sRunInfo* runinfo)
{
    enum eKanjiCode code = gKanjiCode;
    if(sRunInfo_option(runinfo, "-byte")) {
        code = kByte;
    }
    else if(sRunInfo_option(runinfo, "-utf8")) {
        code = kUtf8;
    }
    else if(sRunInfo_option(runinfo, "-sjis")) {
        code = kSjis;
    }
    else if(sRunInfo_option(runinfo, "-eucjp")) {
        code = kEucjp;
    }

    if(runinfo->mFilter && runinfo->mArgsNumRuntime >= 2) {
        char* target = SFD(nextin).mBuf;

        int ret = 0;
        if(code == kByte) {
            int chars_len = 128;
            char* chars = MALLOC(sizeof(char)*chars_len);
            int chars_num = 0;

            int rchars_len = 128;
            char* rchars = MALLOC(sizeof(char)*rchars_len);
            int rchars_num = 0;
            
            transform_argument_to_tr_style_chars_on_byte(runinfo, &chars_len, &chars, &chars_num, &rchars_len, &rchars, &rchars_num);

            /// go ///
            char* p = target;
            if(chars_num > 0) {
                while(*p) {
                    char* p2 = chars;

                    while(*p2) {
                        if(*p == *p2) {
                            BOOL found = FALSE;
                            char* p3 = rchars;
                            while(*p3) {
                                if(*p == *p3) {
                                    found = TRUE;
                                    break;
                                }
                                else {
                                    *p3++;
                                }
                            }
                            if(!found) {
                                ret++;
                            }
                            break;
                        }
                        else {
                            *p2++;
                        }
                    }
                    p++;
                }
            }
            else if(rchars_num > 0) {
                while(*p) {
                    BOOL found = FALSE;
                    char* p3 = rchars;
                    while(*p3) {
                        if(*p == *p3) {
                            found = TRUE;
                            break;
                        }
                        else {
                            *p3++;
                        }
                    }
                    if(!found) {
                        ret++;
                    }
                    p++;
                }
            }

            FREE(chars);
            FREE(rchars);
        }
        else if(code == kUtf8) {
            int chars_num = 0;
            int chars_size = 128;
            char** chars = MALLOC(sizeof(char*)*chars_size);

            int rchars_num = 0;
            int rchars_size = 128;
            char** rchars = MALLOC(sizeof(char*)*rchars_size);

            transform_argument_to_tr_style_chars_on_utf8(runinfo, &chars_num, &chars_size, &chars, &rchars_num, &rchars_size, &rchars);

            /// go ///
            char* p = target;
            if(chars_num > 0) {
                while(*p) {
                    int size;
                    if(((unsigned char)*p) > 127) {
                        size = ((*p & 0x80) >> 7) + ((*p & 0x40) >> 6) + ((*p & 0x20) >> 5) + ((*p & 0x10) >> 4);
                    }
                    else {
                        size = 1;
                    }

                    BOOL chars_true = FALSE;
                    int i;
                    for(i=0; i<chars_num; i++) {
                        if(memcmp(p, chars[i], size) == 0) {
                            chars_true = TRUE;
                            break;
                        }
                    }

                    BOOL rchars_true = FALSE;
                    for(i=0; i<rchars_num; i++) {
                        if(memcmp(p, rchars[i], size) == 0) {
                            rchars_true = TRUE;
                            break;
                        }
                    }

                    if(chars_true && !rchars_true) {
                        ret++;
                    }

                    p+=size;
                }
            }
            else if(rchars_num > 0) {
                while(*p) {
                    int size;
                    if(((unsigned char)*p) > 127) {
                        size = ((*p & 0x80) >> 7) + ((*p & 0x40) >> 6) + ((*p & 0x20) >> 5) + ((*p & 0x10) >> 4);
                    }
                    else {
                        size = 1;
                    }

                    BOOL rchars_true = FALSE;
                    int i;
                    for(i=0; i<rchars_num; i++) {
                        if(memcmp(p, rchars[i], size) == 0) {
                            rchars_true = TRUE;
                            break;
                        }
                    }

                    if(!rchars_true) {
                        ret++;
                    }

                    p+=size;
                }
            }

            int i;
            for(i=0; i<chars_num; i++) {
                FREE(chars[i]);
            }
            FREE(chars);
            for(i=0; i<rchars_num; i++) {
                FREE(rchars[i]);
            }
            FREE(rchars);
        }
        else {
            int chars_num = 0;
            int chars_size = 128;
            char** chars = MALLOC(sizeof(char*)*chars_size);

            int rchars_num = 0;
            int rchars_size = 128;
            char** rchars = MALLOC(sizeof(char*)*rchars_size);
            
            transform_argument_to_tr_style_chars_on_sjis_or_eucjp(runinfo, code, &chars_num, &chars_size, &chars, &rchars_num, &rchars_size, &rchars);

            /// go ///
            char* p = target;
            if(chars_num > 0) {
                while(*p) {
                    int size;
                    if(is_kanji(code, *p)) {
                        size = 2;
                    }
                    else {
                        size = 1;
                    }

                    BOOL chars_true = FALSE;
                    int i;
                    for(i=0; i<chars_num; i++) {
                        if(memcmp(p, chars[i], size) == 0) {
                            chars_true = TRUE;
                            break;
                        }
                    }

                    BOOL rchars_true = FALSE;
                    for(i=0; i<rchars_num; i++) {
                        if(memcmp(p, rchars[i], size) == 0) {
                            rchars_true = TRUE;
                            break;
                        }
                    }

                    if(chars_true && !rchars_true) {
                        ret++;
                    }

                    p+=size;
                }
            }
            else if(rchars_num > 0) {
                while(*p) {
                    int size;
                    if(is_kanji(code, *p)) {
                        size = 2;
                    }
                    else {
                        size = 1;
                    }

                    BOOL rchars_true = FALSE;
                    int i;
                    for(i=0; i<rchars_num; i++) {
                        if(memcmp(p, rchars[i], size) == 0) {
                            rchars_true = TRUE;
                            break;
                        }
                    }

                    if(!rchars_true) {
                        ret++;
                    }

                    p+=size;
                }
            }

            int i;
            for(i=0; i<chars_num; i++) {
                FREE(chars[i]);
            }
            FREE(chars);
            for(i=0; i<rchars_num; i++) {
                FREE(rchars[i]);
            }
            FREE(rchars);
        }

        char buf[128];
        int size = snprintf(buf, 128, "%d\n", ret);
        if(!fd_write(nextout, buf, size)) {
            err_msg("signal interrupt", runinfo->mSName, runinfo->mSLine, runinfo->mArgs[0]);
            runinfo->mRCode = RCODE_SIGNAL_INTERRUPT;
            return FALSE;
        }

        if(SFD(nextin).mBufLen == 0) {
            runinfo->mRCode = RCODE_NFUN_NULL_INPUT;
        }
        else {
            runinfo->mRCode = 0;
        }
    }

    return TRUE;
}

BOOL cmd_delete(sObject* nextin, sObject* nextout, sRunInfo* runinfo)
{
    enum eKanjiCode code = gKanjiCode;
    if(sRunInfo_option(runinfo, "-byte")) {
        code = kByte;
    }
    else if(sRunInfo_option(runinfo, "-utf8")) {
        code = kUtf8;
    }
    else if(sRunInfo_option(runinfo, "-sjis")) {
        code = kSjis;
    }
    else if(sRunInfo_option(runinfo, "-eucjp")) {
        code = kEucjp;
    }

    if(runinfo->mFilter && runinfo->mArgsNumRuntime >= 2) {
        char* target = SFD(nextin).mBuf;

        if(code == kByte) {
            int chars_len = 128;
            char* chars = MALLOC(sizeof(char)*chars_len);
            int chars_num = 0;

            int rchars_len = 128;
            char* rchars = MALLOC(sizeof(char)*rchars_len);
            int rchars_num = 0;
            
            transform_argument_to_tr_style_chars_on_byte(runinfo, &chars_len, &chars, &chars_num, &rchars_len, &rchars, &rchars_num);

            /// go ///
            char* p = target;
            if(chars_num > 0) {
                while(*p) {
                    BOOL delete_ = FALSE;

                    char* p2 = chars;
                    while(*p2) {
                        if(*p == *p2) {
                            BOOL found = FALSE;
                            char* p3 = rchars;
                            while(*p3) {
                                if(*p == *p3) {
                                    found = TRUE;
                                    break;
                                }
                                else {
                                    *p3++;
                                }
                            }
                            if(!found) {
                                delete_ = TRUE;
                            }
                            break;
                        }
                        else {
                            *p2++;
                        }
                    }

                    if(!delete_) {
                        if(!fd_writec(nextout, *p)) {
                            err_msg("signal interrupt", runinfo->mSName, runinfo->mSLine, runinfo->mArgs[0]);
                            runinfo->mRCode = RCODE_SIGNAL_INTERRUPT;
                            FREE(chars);
                            FREE(rchars);
                            return FALSE;
                        }
                    }

                    p++;
                }
            }
            else if(rchars_num > 0) {
                while(*p) {
                    BOOL found = FALSE;
                    char* p3 = rchars;
                    while(*p3) {
                        if(*p == *p3) {
                            found = TRUE;
                            break;
                        }
                        else {
                            *p3++;
                        }
                    }

                    if(found) {
                        if(!fd_writec(nextout, *p)) {
                            err_msg("signal interrupt", runinfo->mSName, runinfo->mSLine, runinfo->mArgs[0]);
                            runinfo->mRCode = RCODE_SIGNAL_INTERRUPT;
                            FREE(chars);
                            FREE(rchars);
                            return FALSE;
                        }
                    }
                    p++;
                }
            }

            FREE(chars);
            FREE(rchars);
        }
        else if(code == kUtf8) {
            int chars_num = 0;
            int chars_size = 128;
            char** chars = MALLOC(sizeof(char*)*chars_size);

            int rchars_num = 0;
            int rchars_size = 128;
            char** rchars = MALLOC(sizeof(char*)*rchars_size);

            transform_argument_to_tr_style_chars_on_utf8(runinfo, &chars_num, &chars_size, &chars, &rchars_num, &rchars_size, &rchars);

            /// go ///
            char* p = target;
            if(chars_num > 0) {
                while(*p) {
                    int size;
                    if(((unsigned char)*p) > 127) {
                        size = ((*p & 0x80) >> 7) + ((*p & 0x40) >> 6) + ((*p & 0x20) >> 5) + ((*p & 0x10) >> 4);
                    }
                    else {
                        size = 1;
                    }

                    BOOL chars_true = FALSE;
                    int i;
                    for(i=0; i<chars_num; i++) {
                        if(memcmp(p, chars[i], size) == 0) {
                            chars_true = TRUE;
                            break;
                        }
                    }

                    BOOL rchars_true = FALSE;
                    for(i=0; i<rchars_num; i++) {
                        if(memcmp(p, rchars[i], size) == 0) {
                            rchars_true = TRUE;
                            break;
                        }
                    }

                    if(!chars_true || rchars_true) {
                        if(!fd_write(nextout, p, size)) {
                            err_msg("signal interrupt", runinfo->mSName, runinfo->mSLine, runinfo->mArgs[0]);
                            runinfo->mRCode = RCODE_SIGNAL_INTERRUPT;
                            int i;
                            for(i=0; i<chars_num; i++) {
                                FREE(chars[i]);
                            }
                            FREE(chars);
                            for(i=0; i<rchars_num; i++) {
                                FREE(rchars[i]);
                            }
                            FREE(rchars);
                            return FALSE;
                        }
                    }

                    p+=size;
                }
            }
            else if(rchars_num > 0) {
                while(*p) {
                    int size;
                    if(((unsigned char)*p) > 127) {
                        size = ((*p & 0x80) >> 7) + ((*p & 0x40) >> 6) + ((*p & 0x20) >> 5) + ((*p & 0x10) >> 4);
                    }
                    else {
                        size = 1;
                    }

                    BOOL rchars_true = FALSE;
                    int i;
                    for(i=0; i<rchars_num; i++) {
                        if(memcmp(p, rchars[i], size) == 0) {
                            rchars_true = TRUE;
                            break;
                        }
                    }

                    if(rchars_true) {
                        if(!fd_write(nextout, p, size)) {
                            err_msg("signal interrupt", runinfo->mSName, runinfo->mSLine, runinfo->mArgs[0]);
                            runinfo->mRCode = RCODE_SIGNAL_INTERRUPT;
                            int i;
                            for(i=0; i<chars_num; i++) {
                                FREE(chars[i]);
                            }
                            FREE(chars);
                            for(i=0; i<rchars_num; i++) {
                                FREE(rchars[i]);
                            }
                            FREE(rchars);
                            return FALSE;
                        }
                    }

                    p+=size;
                }
            }

            int i;
            for(i=0; i<chars_num; i++) {
                FREE(chars[i]);
            }
            FREE(chars);
            for(i=0; i<rchars_num; i++) {
                FREE(rchars[i]);
            }
            FREE(rchars);
        }
        else {
            int chars_num = 0;
            int chars_size = 128;
            char** chars = MALLOC(sizeof(char*)*chars_size);

            int rchars_num = 0;
            int rchars_size = 128;
            char** rchars = MALLOC(sizeof(char*)*rchars_size);
            
            transform_argument_to_tr_style_chars_on_sjis_or_eucjp(runinfo, code, &chars_num, &chars_size, &chars, &rchars_num, &rchars_size, &rchars);

            /// go ///
            char* p = target;
            if(chars_num > 0) {
                while(*p) {
                    int size;
                    if(is_kanji(code, *p)) {
                        size = 2;
                    }
                    else {
                        size = 1;
                    }

                    BOOL chars_true = FALSE;
                    int i;
                    for(i=0; i<chars_num; i++) {
                        if(memcmp(p, chars[i], size) == 0) {
                            chars_true = TRUE;
                            break;
                        }
                    }

                    BOOL rchars_true = FALSE;
                    for(i=0; i<rchars_num; i++) {
                        if(memcmp(p, rchars[i], size) == 0) {
                            rchars_true = TRUE;
                            break;
                        }
                    }

                    if(!chars_true || rchars_true) {
                        if(!fd_write(nextout, p, size)) {
                            err_msg("signal interrupt", runinfo->mSName, runinfo->mSLine, runinfo->mArgs[0]);
                            runinfo->mRCode = RCODE_SIGNAL_INTERRUPT;
                            int i;
                            for(i=0; i<chars_num; i++) {
                                FREE(chars[i]);
                            }
                            FREE(chars);
                            for(i=0; i<rchars_num; i++) {
                                FREE(rchars[i]);
                            }
                            FREE(rchars);
                            return FALSE;
                        }
                    }

                    p+=size;
                }
            }
            else if(rchars_num > 0) {
                while(*p) {
                    int size;
                    if(is_kanji(code, *p)) {
                        size = 2;
                    }
                    else {
                        size = 1;
                    }

                    BOOL rchars_true = FALSE;
                    int i;
                    for(i=0; i<rchars_num; i++) {
                        if(memcmp(p, rchars[i], size) == 0) {
                            rchars_true = TRUE;
                            break;
                        }
                    }

                    if(rchars_true) {
                        if(!fd_write(nextout, p, size)) {
                            err_msg("signal interrupt", runinfo->mSName, runinfo->mSLine, runinfo->mArgs[0]);
                            runinfo->mRCode = RCODE_SIGNAL_INTERRUPT;
                            int i;
                            for(i=0; i<chars_num; i++) {
                                FREE(chars[i]);
                            }
                            FREE(chars);
                            for(i=0; i<rchars_num; i++) {
                                FREE(rchars[i]);
                            }
                            FREE(rchars);
                            return FALSE;
                        }
                    }

                    p+=size;
                }
            }


            int i;
            for(i=0; i<chars_num; i++) {
                FREE(chars[i]);
            }
            FREE(chars);
            for(i=0; i<rchars_num; i++) {
                FREE(rchars[i]);
            }
            FREE(rchars);
        }


        if(SFD(nextin).mBufLen == 0) {
            runinfo->mRCode = RCODE_NFUN_NULL_INPUT;
        }
        else {
            runinfo->mRCode = 0;
        }
    }

    return TRUE;
}

BOOL cmd_squeeze(sObject* nextin, sObject* nextout, sRunInfo* runinfo)
{
    enum eKanjiCode code = gKanjiCode;
    if(sRunInfo_option(runinfo, "-byte")) {
        code = kByte;
    }
    else if(sRunInfo_option(runinfo, "-utf8")) {
        code = kUtf8;
    }
    else if(sRunInfo_option(runinfo, "-sjis")) {
        code = kSjis;
    }
    else if(sRunInfo_option(runinfo, "-eucjp")) {
        code = kEucjp;
    }

    if(runinfo->mFilter) {
        char* target = SFD(nextin).mBuf;

        if(code == kByte) {
            if(runinfo->mArgsNumRuntime == 1) {
                char* p = target;
                char char_before = 0;
                while(*p) {
                    if(char_before != *p) {
                        if(!fd_writec(nextout, *p)) {
                            err_msg("signal interrupt", runinfo->mSName, runinfo->mSLine, runinfo->mArgs[0]);
                            runinfo->mRCode = RCODE_SIGNAL_INTERRUPT;
                            return FALSE;
                        }
                    }

                    char_before = *p;
                    p++;
                }
            }
            else {
                int chars_len = 128;
                char* chars = MALLOC(sizeof(char)*chars_len);
                int chars_num = 0;

                int rchars_len = 128;
                char* rchars = MALLOC(sizeof(char)*rchars_len);
                int rchars_num = 0;

                transform_argument_to_tr_style_chars_on_byte(runinfo, &chars_len, &chars, &chars_num, &rchars_len, &rchars, &rchars_num);

                /// go ///
                char* p = target;
                char char_before = 0;
                if(chars_num > 0) {
                    while(*p) {
                        BOOL squeeze_char = FALSE;

                        char* p2 = chars;
                        while(*p2) {
                            if(*p == *p2) {
                                BOOL found = FALSE;
                                char* p3 = rchars;
                                while(*p3) {
                                    if(*p == *p3) {
                                        found = TRUE;
                                        break;
                                    }
                                    else {
                                        *p3++;
                                    }
                                }
                                if(!found) {
                                    squeeze_char = TRUE;
                                }
                                break;
                            }
                            else {
                                *p2++;
                            }
                        }

                        if(!squeeze_char || char_before != *p) {
                            if(!fd_writec(nextout, *p)) {
                                err_msg("signal interrupt", runinfo->mSName, runinfo->mSLine, runinfo->mArgs[0]);
                                runinfo->mRCode = RCODE_SIGNAL_INTERRUPT;
                                FREE(chars);
                                FREE(rchars);
                                return FALSE;
                            }
                        }

                        char_before = *p;
                        p++;
                    }
                }
                else if(rchars_num > 0) {
                    while(*p) {
                        BOOL squeeze_char = FALSE;
                        char* p3 = rchars;
                        while(*p3) {
                            if(*p == *p3) {
                                squeeze_char = TRUE;
                                break;
                            }
                            else {
                                *p3++;
                            }
                        }

                        if(squeeze_char || char_before != *p) {
                            if(!fd_writec(nextout, *p)) {
                                err_msg("signal interrupt", runinfo->mSName, runinfo->mSLine, runinfo->mArgs[0]);
                                runinfo->mRCode = RCODE_SIGNAL_INTERRUPT;
                                FREE(chars);
                                FREE(rchars);
                                return FALSE;
                            }
                        }

                        char_before = *p;
                        p++;
                    }
                }

                FREE(chars);
                FREE(rchars);
            }
        }
        else if(code == kUtf8) {
            if(runinfo->mArgsNumRuntime == 1) {
                char* p = target;
                char string_before[32];
                *string_before = 0;

                while(*p) {
                    int size;
                    if(((unsigned char)*p) > 127) {
                        size = ((*p & 0x80) >> 7) + ((*p & 0x40) >> 6) + ((*p & 0x20) >> 5) + ((*p & 0x10) >> 4);
                    }
                    else {
                        size = 1;
                    }

                    if(memcmp(string_before, p, size) != 0) {
                        if(!fd_write(nextout, p, size)) {
                            err_msg("signal interrupt", runinfo->mSName, runinfo->mSLine, runinfo->mArgs[0]);
                            runinfo->mRCode = RCODE_SIGNAL_INTERRUPT;
                            return FALSE;
                        }
                    }

                    memcpy(string_before, p, size);
                    string_before[size] = 0;
                    p+=size;
                }
            }
            else {
                int chars_num = 0;
                int chars_size = 128;
                char** chars = MALLOC(sizeof(char*)*chars_size);

                int rchars_num = 0;
                int rchars_size = 128;
                char** rchars = MALLOC(sizeof(char*)*rchars_size);

                transform_argument_to_tr_style_chars_on_utf8(runinfo, &chars_num, &chars_size, &chars, &rchars_num, &rchars_size, &rchars);

                /// go ///
                char* p = target;
                if(chars_num > 0) {
                    char string_before[32];
                    *string_before = 0;
                    while(*p) {
                        int size;
                        if(((unsigned char)*p) > 127) {
                            size = ((*p & 0x80) >> 7) + ((*p & 0x40) >> 6) + ((*p & 0x20) >> 5) + ((*p & 0x10) >> 4);
                        }
                        else {
                            size = 1;
                        }

                        BOOL chars_true = FALSE;
                        int i;
                        for(i=0; i<chars_num; i++) {
                            if(memcmp(p, chars[i], size) == 0) {
                                chars_true = TRUE;
                                break;
                            }
                        }

                        BOOL rchars_true = FALSE;
                        for(i=0; i<rchars_num; i++) {
                            if(memcmp(p, rchars[i], size) == 0) {
                                rchars_true = TRUE;
                                break;
                            }
                        }

                        if(!chars_true || rchars_true || memcmp(string_before, p, size) != 0) {
                            if(!fd_write(nextout, p, size)) {
                                err_msg("signal interrupt", runinfo->mSName, runinfo->mSLine, runinfo->mArgs[0]);
                                runinfo->mRCode = RCODE_SIGNAL_INTERRUPT;
                                int i;
                                for(i=0; i<chars_num; i++) {
                                    FREE(chars[i]);
                                }
                                FREE(chars);
                                for(i=0; i<rchars_num; i++) {
                                    FREE(rchars[i]);
                                }
                                FREE(rchars);
                                return FALSE;
                            }
                        }

                        memcpy(string_before, p, size);
                        string_before[size] = 0;
                        p+=size;
                    }
                }
                else if(rchars_num > 0) {
                    char string_before[32];
                    *string_before = 0;
                    while(*p) {
                        int size;
                        if(((unsigned char)*p) > 127) {
                            size = ((*p & 0x80) >> 7) + ((*p & 0x40) >> 6) + ((*p & 0x20) >> 5) + ((*p & 0x10) >> 4);
                        }
                        else {
                            size = 1;
                        }

                        BOOL rchars_true = FALSE;
                        int i;
                        for(i=0; i<rchars_num; i++) {
                            if(memcmp(p, rchars[i], size) == 0) {
                                rchars_true = TRUE;
                                break;
                            }
                        }

                        if(rchars_true || memcmp(string_before, p, size) != 0) {
                            if(!fd_write(nextout, p, size)) {
                                err_msg("signal interrupt", runinfo->mSName, runinfo->mSLine, runinfo->mArgs[0]);
                                runinfo->mRCode = RCODE_SIGNAL_INTERRUPT;
                                int i;
                                for(i=0; i<chars_num; i++) {
                                    FREE(chars[i]);
                                }
                                FREE(chars);
                                for(i=0; i<rchars_num; i++) {
                                    FREE(rchars[i]);
                                }
                                FREE(rchars);
                                return FALSE;
                            }
                        }

                        memcpy(string_before, p, size);
                        string_before[size] = 0;
                        p+=size;
                    }
                }

                int i;
                for(i=0; i<chars_num; i++) {
                    FREE(chars[i]);
                }
                FREE(chars);
                for(i=0; i<rchars_num; i++) {
                    FREE(rchars[i]);
                }
                FREE(rchars);
            }
        }
        else {
            if(runinfo->mArgsNumRuntime == 1) {
                char* p = target;
                char string_before[32];
                *string_before = 0;

                while(*p) {
                    int size;
                    if(is_kanji(code, *p)) {
                        size = 2;
                    }
                    else {
                        size = 1;
                    }

                    if(memcmp(string_before, p, size) != 0) {
                        if(!fd_write(nextout, p, size)) {
                            err_msg("signal interrupt", runinfo->mSName, runinfo->mSLine, runinfo->mArgs[0]);
                            runinfo->mRCode = RCODE_SIGNAL_INTERRUPT;
                            return FALSE;
                        }
                    }

                    memcpy(string_before, p, size);
                    string_before[size] = 0;
                    p+=size;
                }
            }
            else {
                int chars_num = 0;
                int chars_size = 128;
                char** chars = MALLOC(sizeof(char*)*chars_size);

                int rchars_num = 0;
                int rchars_size = 128;
                char** rchars = MALLOC(sizeof(char*)*rchars_size);
                
                transform_argument_to_tr_style_chars_on_sjis_or_eucjp(runinfo, code, &chars_num, &chars_size, &chars, &rchars_num, &rchars_size, &rchars);

                /// go ///
                char* p = target;
                if(chars_num > 0) {
                    char string_before[32];
                    *string_before = 0;

                    while(*p) {
                        int size;
                        if(is_kanji(code, *p)) {
                            size = 2;
                        }
                        else {
                            size = 1;
                        }

                        BOOL chars_true = FALSE;
                        int i;
                        for(i=0; i<chars_num; i++) {
                            if(memcmp(p, chars[i], size) == 0) {
                                chars_true = TRUE;
                                break;
                            }
                        }

                        BOOL rchars_true = FALSE;
                        for(i=0; i<rchars_num; i++) {
                            if(memcmp(p, rchars[i], size) == 0) {
                                rchars_true = TRUE;
                                break;
                            }
                        }

                        if(!chars_true || rchars_true || memcmp(string_before, p, size) != 0) {
                            if(!fd_write(nextout, p, size)) {
                                err_msg("signal interrupt", runinfo->mSName, runinfo->mSLine, runinfo->mArgs[0]);
                                runinfo->mRCode = RCODE_SIGNAL_INTERRUPT;
                                int i;
                                for(i=0; i<chars_num; i++) {
                                    FREE(chars[i]);
                                }
                                FREE(chars);
                                for(i=0; i<rchars_num; i++) {
                                    FREE(rchars[i]);
                                }
                                FREE(rchars);
                                return FALSE;
                            }
                        }

                        memcpy(string_before, p, size);
                        string_before[size] = 0;
                        p+=size;
                    }
                }
                else if(rchars_num > 0) {
                    char string_before[32];
                    *string_before = 0;

                    while(*p) {
                        int size;
                        if(is_kanji(code, *p)) {
                            size = 2;
                        }
                        else {
                            size = 1;
                        }

                        BOOL rchars_true = FALSE;
                        int i;
                        for(i=0; i<rchars_num; i++) {
                            if(memcmp(p, rchars[i], size) == 0) {
                                rchars_true = TRUE;
                                break;
                            }
                        }

                        if(rchars_true || memcmp(string_before, p, size) != 0) {
                            if(!fd_write(nextout, p, size)) {
                                err_msg("signal interrupt", runinfo->mSName, runinfo->mSLine, runinfo->mArgs[0]);
                                runinfo->mRCode = RCODE_SIGNAL_INTERRUPT;
                                int i;
                                for(i=0; i<chars_num; i++) {
                                    FREE(chars[i]);
                                }
                                FREE(chars);
                                for(i=0; i<rchars_num; i++) {
                                    FREE(rchars[i]);
                                }
                                FREE(rchars);
                                return FALSE;
                            }
                        }

                        memcpy(string_before, p, size);
                        string_before[size] = 0;
                        p+=size;
                    }
                }

                int i;
                for(i=0; i<chars_num; i++) {
                    FREE(chars[i]);
                }
                FREE(chars);
                for(i=0; i<rchars_num; i++) {
                    FREE(rchars[i]);
                }
                FREE(rchars);
            }
        }

        if(SFD(nextin).mBufLen == 0) {
            runinfo->mRCode = RCODE_NFUN_NULL_INPUT;
        }
        else {
            runinfo->mRCode = 0;
        }
    }

    return TRUE;
}

void tr_transform_pattern_to_chars_on_byte(char* pattern, int* chars_len, char** chars, int* chars_num)
{
    char* p = pattern;

    while(*p) {
        if(*(p+1) == '-' && *(p+2) != 0) {
            if(*p >= '0' && *p <= '9' && *(p+2) >= '0' && *(p+2) <= '9') {
                char c = *p;
                while(c <= *(p+2)) {
                    if(*chars_num >= *chars_len) {
                        (*chars_len) *= 2;
                        *chars = REALLOC(chars, sizeof(char)*(*chars_len));
                    }
                    (*chars)[(*chars_num)++] = c++;
                }
                p+=3;
            }
            else if(*p >= 'a' && *p <='z' && *(p+2) >= 'a' && *(p+2) <= 'z') {
                char c = *p;
                while(c <= *(p+2)) {
                    if(*chars_num >= *chars_len) {
                        (*chars_len) *= 2;
                        *chars = REALLOC(chars, sizeof(char)*(*chars_len));
                    }
                    (*chars)[(*chars_num)++] = c++;
                }
                p+=3;
            }
            else if(*p >= 'A' && *p <='Z' && *(p+2) >= 'A' && *(p+2) <= 'Z') {
                char c = *p;
                while(c <= *(p+2)) {
                    if(*chars_num >= *chars_len) {
                        (*chars_len) *= 2;
                        *chars = REALLOC(*chars, sizeof(char)*(*chars_len));
                    }
                    (*chars)[(*chars_num)++] = c++;
                }
                p+=3;
            }
            else {
                p+=3;
            }
        }
        else {
            if(*chars_num >= *chars_len) {
                (*chars_len) *= 2;
                *chars = REALLOC(chars, sizeof(char)*(*chars_len));
            }
            (*chars)[(*chars_num)++] = *p++;
        }
    }
    (*chars)[*chars_num] = 0;
}

void tr_transform_pattern_to_chars_on_utf8(char* pattern, int* chars_size, char*** chars, int* chars_num)
{
    char* p = pattern;
    while(*p) {
        int size;
        if(((unsigned char)*p) > 127) {
            size = ((*p & 0x80) >> 7) + ((*p & 0x40) >> 6) + ((*p & 0x20) >> 5) + ((*p & 0x10) >> 4);
        }
        else {
            size = 1;
        }

        if(size == 1 && *(p+1) == '-' && *(p+2) != 0) {
            if(*p >= '0' && *p <= '9' && *(p+2) >= '0' && *(p+2) <= '9') {
                char c = *p;
                while(c <= *(p+2)) {
                    if(*chars_num > *chars_size) {
                        (*chars_size) *= 2;
                        *chars = REALLOC(chars, sizeof(char*)*(*chars_size));
                    }

                    char* str = MALLOC(2);
                    *str = c++;
                    *(str+1) = 0;

                    (*chars)[(*chars_num)++] = str;
                }
                p+=3;
            }
            else if(*p >= 'a' && *p <='z' && *(p+2) >= 'a' && *(p+2) <= 'z') {
                char c = *p;
                while(c <= *(p+2)) {
                    if(*chars_num > *chars_size) {
                        (*chars_size) *= 2;
                        *chars = REALLOC(*chars, sizeof(char*)*(*chars_size));
                    }

                    char* str = MALLOC(2);
                    *str = c++;
                    *(str+1) = 0;

                    (*chars)[(*chars_num)++] = str;
                }
                p+=3;
            }
            else if(*p >= 'A' && *p <='Z' && *(p+2) >= 'A' && *(p+2) <= 'Z') {
                char c = *p;
                while(c <= *(p+2)) {
                    if(*chars_num > *chars_size) {
                        (*chars_size) *= 2;
                        *chars = REALLOC(*chars, sizeof(char*)*(*chars_size));
                    }

                    char* str = MALLOC(2);
                    *str = c++;
                    *(str+1) = 0;

                    (*chars)[(*chars_num)++] = str;
                }
                p+=3;
            }
            else {
                p+=3;
            }
        }
        else {
            if(*chars_num > *chars_size) {
                (*chars_size) *= 2;
                *chars = REALLOC(*chars, sizeof(char*)*(*chars_size));
            }

            char* str = MALLOC(sizeof(char)*size + 1);
            memcpy(str, p, size);
            str[size] = 0;

            (*chars)[(*chars_num)++] = str;

            p+= size;
        }
    }
}

void tr_transform_pattern_to_chars_on_sjis_or_eucjp(enum eKanjiCode code, char* pattern, int* chars_size, char*** chars, int* chars_num)
{
    /// split pattern ///
    char* p = pattern;
    while(*p) {
        int size;
        if(is_kanji(code, *p)) {
            size = 2;
        }
        else {
            size = 1;
        }

        if(size == 1 && *(p+1) == '-' && *(p+2) != 0) {
            if(*p >= '0' && *p <= '9' && *(p+2) >= '0' && *(p+2) <= '9') {
                char c = *p;
                while(c <= *(p+2)) {
                    if(*chars_num > *chars_size) {
                        (*chars_size) *= 2;
                        *chars = REALLOC(*chars, sizeof(char*)*(*chars_size));
                    }

                    char* str = MALLOC(2);
                    *str = c++;
                    *(str+1) = 0;

                    (*chars)[(*chars_num)++] = str;
                }
                p+=3;
            }
            else if(*p >= 'a' && *p <='z' && *(p+2) >= 'a' && *(p+2) <= 'z') {
                char c = *p;
                while(c <= *(p+2)) {
                    if(*chars_num > *chars_size) {
                        (*chars_size) *= 2;
                        *chars = REALLOC(*chars, sizeof(char*)*(*chars_size));
                    }

                    char* str = MALLOC(2);
                    *str = c++;
                    *(str+1) = 0;

                    (*chars)[(*chars_num)++] = str;
                }
                p+=3;
            }
            else if(*p >= 'A' && *p <='Z' && *(p+2) >= 'A' && *(p+2) <= 'Z') {
                char c = *p;
                while(c <= *(p+2)) {
                    if(*chars_num > *chars_size) {
                        (*chars_size) *= 2;
                        *chars = REALLOC(*chars, sizeof(char*)*(*chars_size));
                    }

                    char* str = MALLOC(2);
                    *str = c++;
                    *(str+1) = 0;

                    (*chars)[(*chars_num)++] = str;
                }
                p+=3;
            }
            else {
                p+=3;
            }
        }
        else {
            if(*chars_num > *chars_size) {
                (*chars_size) *= 2;
                *chars = REALLOC(*chars, sizeof(char*)*(*chars_size));
            }

            char* str = MALLOC(sizeof(char)*size + 1);
            memcpy(str, p, size);
            str[size] = 0;

            (*chars)[(*chars_num)++] = str;

            p+= size;
        }
    }
}

BOOL cmd_tr(sObject* nextin, sObject* nextout, sRunInfo* runinfo)
{
    enum eKanjiCode code = gKanjiCode;
    if(sRunInfo_option(runinfo, "-byte")) {
        code = kByte;
    }
    else if(sRunInfo_option(runinfo, "-utf8")) {
        code = kUtf8;
    }
    else if(sRunInfo_option(runinfo, "-sjis")) {
        code = kSjis;
    }
    else if(sRunInfo_option(runinfo, "-eucjp")) {
        code = kEucjp;
    }

    if(runinfo->mFilter && runinfo->mArgsNumRuntime == 3) {
        char* target = SFD(nextin).mBuf;

        if(code == kByte) {
            char* pattern = runinfo->mArgsRuntime[1];

            if(*pattern == '^') {
                pattern++;

                /// split pattern ///
                int chars_len = 128;
                char* chars = MALLOC(sizeof(char)*chars_len);
                int chars_num = 0;

                tr_transform_pattern_to_chars_on_byte(pattern, &chars_len, &chars, &chars_num);

                char* replace = runinfo->mArgsRuntime[2];

                int rchars_len = 128;
                char* rchars = MALLOC(sizeof(char)*rchars_len);
                int rchars_num = 0;

                tr_transform_pattern_to_chars_on_byte(replace, &rchars_len, &rchars, &rchars_num);

                /// go ///
                char replace_char;
                if(rchars_num == 0) {
                    replace_char = -1;
                }
                else {
                    replace_char = rchars[rchars_num-1];
                }

                char* p = target;
                while(*p) {
                    char* p2 = chars;

                    BOOL found = FALSE;
                    while(*p2) {
                        if(*p == *p2) {
                            found = TRUE;
                            break;
                        }
                        else {
                            *p2++;
                        }
                    }

                    if(found) {
                        if(!fd_writec(nextout, *p)) {
                            err_msg("signal interrupt", runinfo->mSName, runinfo->mSLine, runinfo->mArgs[0]);
                            runinfo->mRCode = RCODE_SIGNAL_INTERRUPT;
                            FREE(chars);
                            FREE(rchars);
                            return FALSE;
                        }
                    }
                    else {
                        if(replace_char != -1) {
                            if(!fd_writec(nextout, replace_char)) {
                                err_msg("signal interrupt", runinfo->mSName, runinfo->mSLine, runinfo->mArgs[0]);
                                runinfo->mRCode = RCODE_SIGNAL_INTERRUPT;
                                FREE(chars);
                                FREE(rchars);
                                return FALSE;
                            }
                        }
                    }
                    p++;
                }

                FREE(chars);
                FREE(rchars);
            }
            else {
                /// split pattern ///
                int chars_len = 128;
                char* chars = MALLOC(sizeof(char)*chars_len);
                int chars_num = 0;

                tr_transform_pattern_to_chars_on_byte(pattern, &chars_len, &chars, &chars_num);

                char* replace = runinfo->mArgsRuntime[2];

                int rchars_len = 128;
                char* rchars = MALLOC(sizeof(char)*rchars_len);
                int rchars_num = 0;

                tr_transform_pattern_to_chars_on_byte(replace, &rchars_len, &rchars, &rchars_num);

                /// adjust values ///
                if(rchars_num < chars_num) {
                    char last_rchar;
                    if(rchars_num == 0) {
                        last_rchar = -1;
                    }
                    else {
                        last_rchar = rchars[rchars_num-1];
                    }

                    int i;
                    for(i=rchars_num; i<chars_num; i++) {
                        if(rchars_num >= rchars_len) {
                            rchars_len *= 2;
                            rchars = REALLOC(rchars, sizeof(char)*rchars_len);
                        }
                        rchars[rchars_num++] = last_rchar;
                    }
                }

                /// go ///
                char* p = target;
                while(*p) {
                    char* p2 = chars;

                    BOOL found = FALSE;
                    while(*p2) {
                        if(*p == *p2) {
                            found = TRUE;
                            char c = rchars[p2 - chars];
                            if(c != -1) {
                                if(!fd_writec(nextout, c)) {
                                    err_msg("signal interrupt", runinfo->mSName, runinfo->mSLine, runinfo->mArgs[0]);
                                    runinfo->mRCode = RCODE_SIGNAL_INTERRUPT;
                                    FREE(chars);
                                    FREE(rchars);
                                    return FALSE;
                                }
                            }
                            break;
                        }
                        else {
                            *p2++;
                        }
                    }

                    if(!found) {
                        if(!fd_writec(nextout, *p)) {
                            err_msg("signal interrupt", runinfo->mSName, runinfo->mSLine, runinfo->mArgs[0]);
                            runinfo->mRCode = RCODE_SIGNAL_INTERRUPT;
                            FREE(chars);
                            FREE(rchars);
                            return FALSE;
                        }
                    }
                    p++;
                }

                FREE(chars);
                FREE(rchars);
            }
        }
        else if(code == kUtf8) {
            char* pattern = runinfo->mArgsRuntime[1];

            if(*pattern == '^') {
                pattern++;

                /// split pattern ///
                int chars_num = 0;
                int chars_size = 128;
                char** chars = MALLOC(sizeof(char*)*chars_size);

                tr_transform_pattern_to_chars_on_utf8(pattern, &chars_size, &chars, &chars_num);

                char* replace = runinfo->mArgsRuntime[2];

                int rchars_num = 0;
                int rchars_size = 128;
                char** rchars = MALLOC(sizeof(char*)*rchars_size);

                tr_transform_pattern_to_chars_on_utf8(replace, &rchars_size, &rchars, &rchars_num);

                /// go ///
                char* replace_char;
                if(rchars_num == 0) {
                    replace_char = "";
                }
                else {
                    replace_char = rchars[rchars_num-1];
                }

                char* p = target;
                while(*p) {
                    int size;
                    if(((unsigned char)*p) > 127) {
                        size = ((*p & 0x80) >> 7) + ((*p & 0x40) >> 6) + ((*p & 0x20) >> 5) + ((*p & 0x10) >> 4);
                    }
                    else {
                        size = 1;
                    }

                    BOOL found = FALSE;
                    int i;
                    for(i=0; i<chars_num; i++) {
                        if(memcmp(p, chars[i], size) == 0) {
                            found = TRUE;
                            break;
                        }
                    }

                    if(!found) {
                        if(!fd_write(nextout, replace_char, strlen(replace_char))) {
                            err_msg("signal interrupt", runinfo->mSName, runinfo->mSLine, runinfo->mArgs[0]);
                            runinfo->mRCode = RCODE_SIGNAL_INTERRUPT;
                            int i;
                            for(i=0; i<chars_num; i++) {
                                FREE(chars[i]);
                            }
                            FREE(chars);
                            for(i=0; i<rchars_num; i++) {
                                FREE(rchars[i]);
                            }
                            FREE(rchars);
                            return FALSE;
                        }
                    }
                    else {
                        if(!fd_write(nextout, p, size)) {
                            err_msg("signal interrupt", runinfo->mSName, runinfo->mSLine, runinfo->mArgs[0]);
                            runinfo->mRCode = RCODE_SIGNAL_INTERRUPT;
                            int i;
                            for(i=0; i<chars_num; i++) {
                                FREE(chars[i]);
                            }
                            FREE(chars);
                            for(i=0; i<rchars_num; i++) {
                                FREE(rchars[i]);
                            }
                            FREE(rchars);
                            return FALSE;
                        }
                    }

                    p+=size;
                }

                int i;
                for(i=0; i<chars_num; i++) {
                    FREE(chars[i]);
                }
                FREE(chars);
                for(i=0; i<rchars_num; i++) {
                    FREE(rchars[i]);
                }
                FREE(rchars);
            }
            else {
                /// split pattern ///
                int chars_num = 0;
                int chars_size = 128;
                char** chars = MALLOC(sizeof(char*)*chars_size);

                tr_transform_pattern_to_chars_on_utf8(pattern, &chars_size, &chars, &chars_num);

                char* replace = runinfo->mArgsRuntime[2];

                int rchars_num = 0;
                int rchars_size = 128;
                char** rchars = MALLOC(sizeof(char*)*rchars_size);

                tr_transform_pattern_to_chars_on_utf8(replace, &rchars_size, &rchars, &rchars_num);

                /// adjust values ///
                if(rchars_num < chars_num) {
                    char* last_rchar;
                    if(rchars_num == 0) {
                        last_rchar = "";
                    }
                    else {
                        last_rchar = rchars[rchars_num-1];
                    }
                    int i;
                    for(i=rchars_num; i<chars_num; i++) {
                        if(rchars_num > rchars_size) {
                            rchars_size *= 2;
                            rchars = REALLOC(rchars, sizeof(char*)*rchars_size);
                        }
                        rchars[rchars_num++] = STRDUP(last_rchar);
                    }
                }

                /// go ///
                char* p = target;
                while(*p) {
                    int size;
                    if(((unsigned char)*p) > 127) {
                        size = ((*p & 0x80) >> 7) + ((*p & 0x40) >> 6) + ((*p & 0x20) >> 5) + ((*p & 0x10) >> 4);
                    }
                    else {
                        size = 1;
                    }

                    int matched_chars = -1;
                    int i;
                    for(i=0; i<chars_num; i++) {
                        if(memcmp(p, chars[i], size) == 0) {
                            matched_chars = i;
                            break;
                        }
                    }

                    if(matched_chars >= 0) {
                        if(!fd_write(nextout, rchars[matched_chars], strlen(rchars[matched_chars]))) {
                            err_msg("signal interrupt", runinfo->mSName, runinfo->mSLine, runinfo->mArgs[0]);
                            runinfo->mRCode = RCODE_SIGNAL_INTERRUPT;
                            int i;
                            for(i=0; i<chars_num; i++) {
                                FREE(chars[i]);
                            }
                            FREE(chars);
                            for(i=0; i<rchars_num; i++) {
                                FREE(rchars[i]);
                            }
                            FREE(rchars);
                            return FALSE;
                        }
                    }
                    else {
                        if(!fd_write(nextout, p, size)) {
                            err_msg("signal interrupt", runinfo->mSName, runinfo->mSLine, runinfo->mArgs[0]);
                            runinfo->mRCode = RCODE_SIGNAL_INTERRUPT;
                            int i;
                            for(i=0; i<chars_num; i++) {
                                FREE(chars[i]);
                            }
                            FREE(chars);
                            for(i=0; i<rchars_num; i++) {
                                FREE(rchars[i]);
                            }
                            FREE(rchars);
                            return FALSE;
                        }
                    }

                    p+=size;
                }

                int i;
                for(i=0; i<chars_num; i++) {
                    FREE(chars[i]);
                }
                FREE(chars);
                for(i=0; i<rchars_num; i++) {
                    FREE(rchars[i]);
                }
                FREE(rchars);
            }
        }
        else {
            char* pattern = runinfo->mArgsRuntime[1];

            if(*pattern == '^') {
                pattern++;

                /// split pattern ///
                int chars_num = 0;
                int chars_size = 128;
                char** chars = MALLOC(sizeof(char*)*chars_size);

                tr_transform_pattern_to_chars_on_utf8(pattern, &chars_size, &chars, &chars_num);

                char* replace = runinfo->mArgsRuntime[2];

                int rchars_num = 0;
                int rchars_size = 128;
                char** rchars = MALLOC(sizeof(char*)*rchars_size);

                tr_transform_pattern_to_chars_on_utf8(replace, &rchars_size, &rchars, &rchars_num);

                /// go ///
                char* replace_char;
                if(rchars_num == 0) {
                    replace_char = "";
                }
                else {
                    replace_char = rchars[rchars_num-1];
                }

                char* p = target;
                while(*p) {
                    int size;
                    if(is_kanji(code, *p)) {
                        size = 2;
                    }
                    else {
                        size = 1;
                    }

                    BOOL found = FALSE;
                    int i;
                    for(i=0; i<chars_num; i++) {
                        if(memcmp(p, chars[i], size) == 0) {
                            found = TRUE;
                            break;
                        }
                    }

                    if(!found) {
                        if(!fd_write(nextout, replace_char, strlen(replace_char))) {
                            err_msg("signal interrupt", runinfo->mSName, runinfo->mSLine, runinfo->mArgs[0]);
                            runinfo->mRCode = RCODE_SIGNAL_INTERRUPT;
                            int i;
                            for(i=0; i<chars_num; i++) {
                                FREE(chars[i]);
                            }
                            FREE(chars);
                            for(i=0; i<rchars_num; i++) {
                                FREE(rchars[i]);
                            }
                            FREE(rchars);
                            return FALSE;
                        }
                    }
                    else {
                        if(!fd_write(nextout, p, size)) {
                            err_msg("signal interrupt", runinfo->mSName, runinfo->mSLine, runinfo->mArgs[0]);
                            runinfo->mRCode = RCODE_SIGNAL_INTERRUPT;
                            int i;
                            for(i=0; i<chars_num; i++) {
                                FREE(chars[i]);
                            }
                            FREE(chars);
                            for(i=0; i<rchars_num; i++) {
                                FREE(rchars[i]);
                            }
                            FREE(rchars);
                            return FALSE;
                        }
                    }

                    p+=size;
                }

                int i;
                for(i=0; i<chars_num; i++) {
                    FREE(chars[i]);
                }
                FREE(chars);
                for(i=0; i<rchars_num; i++) {
                    FREE(rchars[i]);
                }
                FREE(rchars);
            }
            else {
                /// split pattern ///
                int chars_num = 0;
                int chars_size = 128;
                char** chars = MALLOC(sizeof(char*)*chars_size);

                tr_transform_pattern_to_chars_on_sjis_or_eucjp(code, pattern, &chars_size, &chars, &chars_num);

                char* replace = runinfo->mArgsRuntime[2];

                int rchars_num = 0;
                int rchars_size = 128;
                char** rchars = MALLOC(sizeof(char*)*rchars_size);

                tr_transform_pattern_to_chars_on_sjis_or_eucjp(code, replace, &rchars_size, &rchars, &rchars_num);

                /// adjust values ///
                if(rchars_num < chars_num) {
                    char* last_rchar;
                    if(rchars_num == 0) {
                        last_rchar = "";
                    }
                    else {
                        last_rchar = rchars[rchars_num-1];
                    }
                    int i;
                    for(i=rchars_num; i<chars_num; i++) {
                        if(rchars_num > rchars_size) {
                            rchars_size *= 2;
                            rchars = REALLOC(rchars, sizeof(char*)*rchars_size);
                        }
                        rchars[rchars_num++] = STRDUP(last_rchar);
                    }
                }

                /// go ///
                char* p = target;
                while(*p) {
                    int size;
                    if(is_kanji(code, *p)) {
                        size = 2;
                    }
                    else {
                        size = 1;
                    }

                    int matched_chars = -1;
                    int i;
                    for(i=0; i<chars_num; i++) {
                        if(memcmp(p, chars[i], size) == 0) {
                            matched_chars = i;
                            break;
                        }
                    }

                    if(matched_chars >= 0) {
                        if(!fd_write(nextout, rchars[matched_chars], strlen(rchars[matched_chars]))) {
                            err_msg("signal interrupt", runinfo->mSName, runinfo->mSLine, runinfo->mArgs[0]);
                            runinfo->mRCode = RCODE_SIGNAL_INTERRUPT;
                            int i;
                            for(i=0; i<chars_num; i++) {
                                FREE(chars[i]);
                            }
                            FREE(chars);
                            for(i=0; i<rchars_num; i++) {
                                FREE(rchars[i]);
                            }
                            FREE(rchars);
                            return FALSE;
                        }
                    }
                    else {
                        if(!fd_write(nextout, p, size)) {
                            err_msg("signal interrupt", runinfo->mSName, runinfo->mSLine, runinfo->mArgs[0]);
                            runinfo->mRCode = RCODE_SIGNAL_INTERRUPT;
                            int i;
                            for(i=0; i<chars_num; i++) {
                                FREE(chars[i]);
                            }
                            FREE(chars);
                            for(i=0; i<rchars_num; i++) {
                                FREE(rchars[i]);
                            }
                            FREE(rchars);
                            return FALSE;
                        }
                    }

                    p+=size;
                }

                int i;
                for(i=0; i<chars_num; i++) {
                    FREE(chars[i]);
                }
                FREE(chars);
                for(i=0; i<rchars_num; i++) {
                    FREE(rchars[i]);
                }
                FREE(rchars);
            }
        }

        if(SFD(nextin).mBufLen == 0) {
            runinfo->mRCode = RCODE_NFUN_NULL_INPUT;
        }
        else {
            runinfo->mRCode = 0;
        }
    }

    return TRUE;
}

BOOL cmd_succ(sObject* nextin, sObject* nextout, sRunInfo* runinfo)
{
    if(runinfo->mFilter) {
        char* target = SFD(nextin).mBuf;
        int target_len = SFD(nextin).mBufLen;

        if(target_len > 0) {
            char* p = target + target_len - 1;

            int ignore_num = 0;
            while(p - target > 0) {
                if(*p < ' ' || *p >= 127) {
                    ignore_num++;
                    p--;
                }
                else {
                    break;
                }
            }

            int increase_num = 1;
            while(p - target > 0) {
                if(*p >= '0' && *p <= '8') {
                    break;
                }
                else if(*p  == '9') {
                    increase_num++;
                }
                else if(*p >= 'a' && *p <= 'y') {
                    break;
                }
                else if(*p == 'z') {
                    increase_num++;
                }
                else if(*p >= 'A' && *p <= 'Y') {
                    break;
                }
                else if(*p == 'Z') {
                    increase_num++;
                }
                else {
                    increase_num++;
                }

                p--;
            }

            /// write head of string ///
            if(increase_num+ignore_num == target_len) {
                int i;
                for(i=0; i<target_len; i++) {
                    char c = target[i];
                    if(c >= 'a' && c <= 'y') {
                        break;
                    }
                    else if(c == 'z') {
                        if(!fd_writec(nextout, 'a')) {
                            err_msg("signal interrupt", runinfo->mSName, runinfo->mSLine, runinfo->mArgs[0]);
                            runinfo->mRCode = RCODE_SIGNAL_INTERRUPT;
                            return FALSE;
                        }
                        break;
                    }
                    else if(c >= 'A' && c <= 'Y') {
                        break;
                    }
                    else if(c == 'Z') {
                        if(!fd_writec(nextout, 'A')) {
                            err_msg("signal interrupt", runinfo->mSName, runinfo->mSLine, runinfo->mArgs[0]);
                            runinfo->mRCode = RCODE_SIGNAL_INTERRUPT;
                            return FALSE;
                        }
                        break;
                    }
                    else if(c >= '0' && c <= '8') {
                        break;
                    }
                    else if(c == '9') {
                        if(!fd_writec(nextout, '1')) {
                            err_msg("signal interrupt", runinfo->mSName, runinfo->mSLine, runinfo->mArgs[0]);
                            runinfo->mRCode = RCODE_SIGNAL_INTERRUPT;
                            return FALSE;
                        }
                        break;
                    }
                    else {
                    }
                }
            }
            else {
                if(!fd_write(nextout, target, target_len-increase_num-ignore_num)) {
                    err_msg("signal interrupt", runinfo->mSName, runinfo->mSLine, runinfo->mArgs[0]);
                    runinfo->mRCode = RCODE_SIGNAL_INTERRUPT;
                    return FALSE;
                }
            }

            /// write increased string ///
            int i;
            for(i=0; i<increase_num; i++) {
                char c = target[target_len -increase_num-ignore_num+i];
                if(c >= 'a' && c <= 'y') {
                    if(!fd_writec(nextout, ++c)) {
                        err_msg("signal interrupt", runinfo->mSName, runinfo->mSLine, runinfo->mArgs[0]);
                        runinfo->mRCode = RCODE_SIGNAL_INTERRUPT;
                        return FALSE;
                    }
                }
                else if(c == 'z') {
                    if(!fd_writec(nextout, 'a')) {
                        err_msg("signal interrupt", runinfo->mSName, runinfo->mSLine, runinfo->mArgs[0]);
                        runinfo->mRCode = RCODE_SIGNAL_INTERRUPT;
                        return FALSE;
                    }
                }
                else if(c >= 'A' && c <= 'Y') {
                    if(!fd_writec(nextout, ++c)) {
                        err_msg("signal interrupt", runinfo->mSName, runinfo->mSLine, runinfo->mArgs[0]);
                        runinfo->mRCode = RCODE_SIGNAL_INTERRUPT;
                        return FALSE;
                    }
                }
                else if(c == 'Z') {
                    if(!fd_writec(nextout, 'A')) {
                        err_msg("signal interrupt", runinfo->mSName, runinfo->mSLine, runinfo->mArgs[0]);
                        runinfo->mRCode = RCODE_SIGNAL_INTERRUPT;
                        return FALSE;
                    }
                }
                else if(c >= '0' && c <= '8') {
                    if(!fd_writec(nextout, ++c)) {
                        err_msg("signal interrupt", runinfo->mSName, runinfo->mSLine, runinfo->mArgs[0]);
                        runinfo->mRCode = RCODE_SIGNAL_INTERRUPT;
                        return FALSE;
                    }
                }
                else if(c == '9') {
                    if(!fd_writec(nextout, '0')) {
                        err_msg("signal interrupt", runinfo->mSName, runinfo->mSLine, runinfo->mArgs[0]);
                        runinfo->mRCode = RCODE_SIGNAL_INTERRUPT;
                        return FALSE;
                    }
                }
                else {
                    if(increase_num == 1) {
                        if(!fd_writec(nextout, ++c)) {
                            err_msg("signal interrupt", runinfo->mSName, runinfo->mSLine, runinfo->mArgs[0]);
                            runinfo->mRCode = RCODE_SIGNAL_INTERRUPT;
                            return FALSE;
                        }
                    }
                    else {
                        if(!fd_writec(nextout, c)) {
                            err_msg("signal interrupt", runinfo->mSName, runinfo->mSLine, runinfo->mArgs[0]);
                            runinfo->mRCode = RCODE_SIGNAL_INTERRUPT;
                            return FALSE;
                        }
                    }
                }
            }

            /// write ignore string ///
            for(i=0; i<ignore_num; i++) {
                char c = target[target_len - ignore_num+i];
                if(!fd_writec(nextout, c)) {
                    err_msg("signal interrupt", runinfo->mSName, runinfo->mSLine, runinfo->mArgs[0]);
                    runinfo->mRCode = RCODE_SIGNAL_INTERRUPT;
                    return FALSE;
                }
            }

            runinfo->mRCode = 0;
        }
        else {
            runinfo->mRCode = RCODE_NFUN_NULL_INPUT;
        }
    }

    return TRUE;
}

BOOL cmd_scan(sObject* nextin, sObject* nextout, sRunInfo* runinfo)
{
    char* field = "\n";
    if(sRunInfo_option(runinfo, "-Lw")) {
        field = "\r\n";
    }
    else if(sRunInfo_option(runinfo, "-Lm")) {
        field = "\r";
    }
    else if(sRunInfo_option(runinfo, "-Lu")) {
        field = "\n";
    }
    else if(sRunInfo_option(runinfo, "-La")) {
        field = "\a";
    }
    
    if(runinfo->mFilter && runinfo->mArgsNumRuntime == 2) {
        sObject* block;
        sObject* nextin2;

        if(runinfo->mBlocksNum >= 1) {
            block = runinfo->mBlocks[0];
            nextin2 = FD_NEW_STACK();
        }
        else {
            block = NULL;
        }

        int match_count = 0;
        char* regex = runinfo->mArgsRuntime[1];

        regex_t* reg;
        int r = get_onig_regex(&reg, runinfo, regex);

        if(r == ONIG_NORMAL) {
            char* target = SFD(nextin).mBuf;
            char* p = SFD(nextin).mBuf;
            char* end = SFD(nextin).mBuf + strlen(SFD(nextin).mBuf);
            while(p < end) {
                OnigRegion* region = onig_region_new();
                int r2 = onig_search(reg, target
                   , target + strlen(target)
                   , p
                   , p + strlen(p)
                   , region, ONIG_OPTION_NONE);

                if(r2 >= 0) {
                    match_count++;

                    if(block) {
                        fd_clear(nextin2);

                        /// no group ///
                        if(region->num_regs == 1) {
                            const int n = region->end[0] - region->beg[0];

                            if(!fd_write(nextin2, target + region->beg[0], n)) {
                                err_msg("signal interrupt", runinfo->mSName, runinfo->mSLine, runinfo->mArgs[0]);
                                runinfo->mRCode = RCODE_SIGNAL_INTERRUPT;
                                onig_free(reg);
                                onig_region_free(region, 1);
                                return FALSE;
                            }
                            if(!fd_write(nextin2, field, strlen(field))) {
                                err_msg("signal interrupt", runinfo->mSName, runinfo->mSLine, runinfo->mArgs[0]);
                                runinfo->mRCode = RCODE_SIGNAL_INTERRUPT;
                                onig_free(reg);
                                onig_region_free(region, 1);
                                return FALSE;
                            }
                        }
                        /// group ///
                        else {
                            int i;
                            for (i=1; i<region->num_regs; i++) {
                                const int size = region->end[i] - region->beg[i];

                                if(!fd_write(nextin2, target + region->beg[i], size)) {
                                    err_msg("signal interrupt", runinfo->mSName, runinfo->mSLine, runinfo->mArgs[0]);
                                    runinfo->mRCode = RCODE_SIGNAL_INTERRUPT;
                                    onig_free(reg);
                                    onig_region_free(region, 1);
                                    return FALSE;
                                }

                                if(i==region->num_regs-1) {
                                    if(!fd_write(nextin2, field, strlen(field))) {
                                        err_msg("signal interrupt", runinfo->mSName, runinfo->mSLine, runinfo->mArgs[0]);
                                        runinfo->mRCode = RCODE_SIGNAL_INTERRUPT;
                                        onig_free(reg);
                                        onig_region_free(region, 1);
                                        return FALSE;
                                    }
                                }
                                else {
                                    if(!fd_write(nextin2, "\t", 1)) {
                                        err_msg("signal interrupt", runinfo->mSName, runinfo->mSLine, runinfo->mArgs[0]);
                                        runinfo->mRCode = RCODE_SIGNAL_INTERRUPT;
                                        onig_free(reg);
                                        onig_region_free(region, 1);
                                        return FALSE;
                                    }
                                }
                            }
                        }

                        clear_matching_info_variable();

                        const int size = region->beg[0] - (p - target);
                        if(size > 0) {
                            uobject_put(gRootObject, "PREMATCH", STRING_NEW_GC3(p, size, FALSE));
                        }

                        const int size2 = region->end[0] - region->beg[0];

                        uobject_put(gRootObject, "MATCH", STRING_NEW_GC3(target + region->beg[0], size2, FALSE));
                        uobject_put(gRootObject, "0", STRING_NEW_GC3(target + region->beg[0], size2, FALSE));

                        const int n = strlen(target)-region->end[0];
                        if(n > 0) {
                            uobject_put(gRootObject, "POSTMATCH", STRING_NEW_GC3(target + region->end[0], n, FALSE));
                        }

                        int i;
                        for (i=1; i<region->num_regs; i++) {
                            const int size = region->end[i] - region->beg[i];

                            char name[16];
                            snprintf(name, 16, "%d", i);

                            uobject_put(gRootObject, name, STRING_NEW_GC3(target + region->beg[i], size, FALSE));
                        }

                        if(region->num_regs > 0) {
                            const int n = region->num_regs -1;

                            const int size = region->end[n] - region->beg[n];

                            uobject_put(gRootObject, "LAST_MATCH", STRING_NEW_GC3(target + region->beg[n], size, FALSE));
                        }

                        char buf[128];
                        snprintf(buf, 128, "%d", region->num_regs);
                        uobject_put(gRootObject, "MATCH_NUMBER", STRING_NEW_GC(buf, FALSE));

                        int rcode = 0;
                        if(!run(block, nextin2, nextout, &rcode, runinfo->mCurrentObject, runinfo->mRunningObject)) {
                            runinfo->mRCode = rcode;
                            onig_region_free(region, 1);
                            onig_free(reg);
                            return FALSE;
                        }
                    }
                    else {
                        /// no group ///
                        if(region->num_regs == 1) {
                            const int n = region->end[0] - region->beg[0];

                            if(!fd_write(nextout, target + region->beg[0], n)) {
                                err_msg("signal interrupt", runinfo->mSName, runinfo->mSLine, runinfo->mArgs[0]);
                                runinfo->mRCode = RCODE_SIGNAL_INTERRUPT;
                                onig_free(reg);
                                onig_region_free(region, 1);
                                return FALSE;
                            }
                            if(!fd_write(nextout, field, strlen(field))) {
                                err_msg("signal interrupt", runinfo->mSName, runinfo->mSLine, runinfo->mArgs[0]);
                                runinfo->mRCode = RCODE_SIGNAL_INTERRUPT;
                                onig_free(reg);
                                onig_region_free(region, 1);
                                return FALSE;
                            }
                        }
                        /// group ///
                        else {
                            int i;
                            for (i=1; i<region->num_regs; i++) {
                                const int size = region->end[i] - region->beg[i];

                                if(!fd_write(nextout, target + region->beg[i], size)) {
                                    err_msg("signal interrupt", runinfo->mSName, runinfo->mSLine, runinfo->mArgs[0]);
                                    runinfo->mRCode = RCODE_SIGNAL_INTERRUPT;
                                    onig_free(reg);
                                    onig_region_free(region, 1);
                                    return FALSE;
                                }

                                if(i==region->num_regs-1) {
                                    if(!fd_write(nextout, field, strlen(field))) {
                                        err_msg("signal interrupt", runinfo->mSName, runinfo->mSLine, runinfo->mArgs[0]);
                                        runinfo->mRCode = RCODE_SIGNAL_INTERRUPT;
                                        onig_free(reg);
                                        onig_region_free(region, 1);
                                        return FALSE;
                                    }
                                }
                                else {
                                    if(!fd_write(nextout, "\t", 1)) {
                                        err_msg("signal interrupt", runinfo->mSName, runinfo->mSLine, runinfo->mArgs[0]);
                                        runinfo->mRCode = RCODE_SIGNAL_INTERRUPT;
                                        onig_free(reg);
                                        onig_region_free(region, 1);
                                        return FALSE;
                                    }
                                }
                            }
                        }
                    }

                    if(p == SFD(nextin).mBuf + region->end[0]) {
                        p = SFD(nextin).mBuf + region->end[0] + 1;
                    }
                    else {
                        p = SFD(nextin).mBuf + region->end[0];
                    }
                }
                else {
                    p++;
                }

                onig_region_free(region, 1);
            }

            onig_free(reg);

            char buf[128];
            snprintf(buf, 128, "%d", match_count);
            uobject_put(gRootObject, "MATCH_COUNT", STRING_NEW_GC(buf, FALSE));

            if(match_count > 0) {
                if(SFD(nextin).mBufLen == 0) {
                    runinfo->mRCode = RCODE_NFUN_NULL_INPUT;
                }
                else {
                    runinfo->mRCode = 0;
                }
            }
            else {
                runinfo->mRCode = RCODE_NFUN_FALSE;
            }
        }
        else {
            err_msg("invalid regex", runinfo->mSName, runinfo->mSLine, runinfo->mArgs[0]);
            return FALSE;
        }
    }

    return TRUE;
}

BOOL cmd_index(sObject* nextin, sObject* nextout, sRunInfo* runinfo)
{
    enum eKanjiCode code = gKanjiCode;
    if(sRunInfo_option(runinfo, "-byte")) {
        code = kByte;
    }
    else if(sRunInfo_option(runinfo, "-utf8")) {
        code = kUtf8;
    }
    else if(sRunInfo_option(runinfo, "-sjis")) {
        code = kSjis;
    }
    else if(sRunInfo_option(runinfo, "-eucjp")) {
        code = kEucjp;
    }

    if(sRunInfo_option(runinfo, "-regex")) {
        if(runinfo->mFilter) {
            if(runinfo->mArgsNumRuntime == 2) {
                char* target = SFD(nextin).mBuf;
                char* regex = runinfo->mArgsRuntime[1];

                regex_t* reg;
                int r = get_onig_regex(&reg, runinfo, regex);

                if(r == ONIG_NORMAL) {
                    /// get starting point ///
                    int start;
                    char* number;
                    if(number = sRunInfo_option_with_argument(runinfo, "-number")) {
                        start = atoi(number);

                        int len = str_kanjilen(code, target);
                        if(len < 0) {
                            err_msg("invalid target string", runinfo->mSName, runinfo->mSLine, runinfo->mArgs[0]);
                            return FALSE;
                        }

                        if(start < 0) { 
                            start += len; 
                            if(start < 0) start = 0;
                        } 
                        if(start >= len) { 
                            start = len -1; 
                            if(start < 0) start = 0;
                        }
                    }
                    else {
                        start = 0;
                    }

                    /// get search count ///
                    int match_count;
                    char* count;
                    if(count = sRunInfo_option_with_argument(runinfo, "-count")) {
                        match_count = atoi(count);
                        if(match_count <= 0) { match_count = 1; }
                    }
                    else {
                        match_count = 1;
                    }

                    char* start_byte = str_kanjipos2pointer(code, target, start);
                    char* p = start_byte;
                    char* result = NULL;
                    while(p < start_byte + strlen(start_byte)) {
                        if(gXyzshSigInt) {
                            gXyzshSigInt = FALSE;
                            err_msg("signal interrupt", runinfo->mSName, runinfo->mSLine, runinfo->mArgs[0]);
                            runinfo->mRCode = RCODE_SIGNAL_INTERRUPT;
                            return FALSE;
                        }

                        OnigRegion* region = onig_region_new();
                        OnigErrorInfo err_info;

                        const int point = p - target;
                        int r2 = onig_search(reg, target
                           , target +  strlen(target)
                           , p
                           , p + strlen(p)
                           , region, ONIG_OPTION_NONE);

                        if(r2 == ONIG_MISMATCH) {
                            onig_region_free(region, 1);
                            break;
                        }

                        if(r2 >= 0) {
                            result = target + region->beg[0];

                            match_count--;
                            if(match_count == 0) {
                                break;
                            }
                            p = target + region->beg[0] + 1;

                            onig_region_free(region, 1);
                        }
                        else {
                            onig_region_free(region, 1);
                            break;
                        }
                    }

                    char msg[64];
                    int size;
                    if(result == NULL || match_count !=0) {
                        size = snprintf(msg, 64, "-1");
                        runinfo->mRCode = RCODE_NFUN_FALSE;
                    }
                    else {
                        int c = str_pointer2kanjipos(code, target, result);
                        size = snprintf(msg, 64, "%d", c);

                        if(SFD(nextin).mBufLen == 0) {
                            runinfo->mRCode = RCODE_NFUN_NULL_INPUT;
                        }
                        else {
                            runinfo->mRCode = 0;
                        }
                    }

                    if(!sRunInfo_option(runinfo, "-quiet")) {
                        if(!fd_write(nextout, msg, size)) {
                            err_msg("signal interrupt", runinfo->mSName, runinfo->mSLine, runinfo->mArgs[0]);
                            runinfo->mRCode = RCODE_SIGNAL_INTERRUPT;
                            return FALSE;
                        }
                        if(!fd_write(nextout, "\n", 1)) {
                            err_msg("signal interrupt", runinfo->mSName, runinfo->mSLine, runinfo->mArgs[0]);
                            runinfo->mRCode = RCODE_SIGNAL_INTERRUPT;
                            return FALSE;
                        }
                    }
                }
                else {
                    err_msg("invalid regex", runinfo->mSName, runinfo->mSLine, runinfo->mArgs[0]);
                    return FALSE;
                }

                onig_free(reg);
            }
        }
    }
    else {
        if(runinfo->mFilter) {
            if(runinfo->mArgsNumRuntime == 2) {
                char* target = SFD(nextin).mBuf;

                char* word = runinfo->mArgsRuntime[1];

                /// get starting point ///
                int start;
                char* number;
                if(number = sRunInfo_option_with_argument(runinfo, "-number")) {
                    start = atoi(number);

                    int len = str_kanjilen(code, target);
                    if(len < 0) {
                        err_msg("invalid target string", runinfo->mSName, runinfo->mSLine, runinfo->mArgs[0]);
                        return FALSE;
                    }

                    if(start < 0) { 
                        start += len; 
                        if(start < 0) start = 0;
                    } 
                    if(start >= len) { 
                        start = len -1; 
                        if(start < 0) start = 0;
                    }
                }
                else {
                    start = 0;
                }

                /// get search count ///
                int match_count;
                char* count;
                if(count = sRunInfo_option_with_argument(runinfo, "-count")) {
                    match_count = atoi(count);
                    if(match_count <= 0) { match_count = 1; }
                }
                else {
                    match_count = 1;
                }

                char* start_byte = str_kanjipos2pointer(code, target, start);
                char* p = start_byte;
                char* result = NULL;
                if(sRunInfo_option(runinfo, "-ignore-case")) {
                    while(p < start_byte + strlen(start_byte)) {
                        if(gXyzshSigInt) {
                            gXyzshSigInt = FALSE;
                            err_msg("signal interrupt", runinfo->mSName, runinfo->mSLine, runinfo->mArgs[0]);
                            runinfo->mRCode = RCODE_SIGNAL_INTERRUPT;
                            return FALSE;
                        }

                        result = strcasestr(p, word);
                        if(result) {
                            match_count--;
                            if(match_count == 0) {
                                break;
                            }
                            p = result+strlen(word);
                        }
                        else {
                            break;
                        }
                    }
                }
                else {
                    while(p < start_byte + strlen(start_byte)) {
                        if(gXyzshSigInt) {
                            gXyzshSigInt = FALSE;
                            err_msg("signal interrupt", runinfo->mSName, runinfo->mSLine, runinfo->mArgs[0]);
                            runinfo->mRCode = RCODE_SIGNAL_INTERRUPT;
                            return FALSE;
                        }

                        result = strstr(p, word);
                        if(result) {
                            match_count--;
                            if(match_count == 0) {
                                break;
                            }
                            p = result+strlen(word);
                        }
                        else {
                            break;
                        }
                    }
                }

                char msg[64];
                int size;
                if(result == NULL || match_count !=0) {
                    size = snprintf(msg, 64, "-1");
                    runinfo->mRCode = RCODE_NFUN_FALSE;
                }
                else {
                    int c = str_pointer2kanjipos(code, target, result);
                    size = snprintf(msg, 64, "%d", c);

                    if(SFD(nextin).mBufLen == 0) {
                        runinfo->mRCode = RCODE_NFUN_NULL_INPUT;
                    }
                    else {
                        runinfo->mRCode = 0;
                    }
                }

                if(!sRunInfo_option(runinfo, "-quiet")) {
                    if(!fd_write(nextout, msg, size)) {
                        err_msg("signal interrupt", runinfo->mSName, runinfo->mSLine, runinfo->mArgs[0]);
                        runinfo->mRCode = RCODE_SIGNAL_INTERRUPT;
                        return FALSE;
                    }
                    if(!fd_write(nextout, "\n", 1)) {
                        err_msg("signal interrupt", runinfo->mSName, runinfo->mSLine, runinfo->mArgs[0]);
                        runinfo->mRCode = RCODE_SIGNAL_INTERRUPT;
                        return FALSE;
                    }
                }
            }
        }
    }

    return TRUE;
}

static char* strstr_back(char* p, char* start, char* word, char* sname, int sline, char* command)
{
    int n = strlen(word);

    if(word[0] == 0) {
        return p -1;
    }

    while(p >= start) {
        BOOL flg = TRUE;
        int i;
        for(i=-1; i>=-n; i--) {
            if(p[i] != word[n+i]) {
                flg = FALSE;
                break;
            }

            if(gXyzshSigInt) {
                err_msg("interrupt", sname, sline, command);
                gXyzshSigInt = FALSE;
                return NULL;
            }
        }

        if(flg) {
            return p -n;
        }
        else {
            p--;
        }
    }

    return NULL;
}

static char* strcasestr_back(char* p, char* start, char* word, char* sname, int sline, char* command)
{
    int n = strlen(word);

    if(word[0] == 0) {
        return p - 1;
    }

    while(p >= start) {
        BOOL flg = TRUE;
        int i;
        for(i=-1; i>=-n; i--) {
            if(isascii(p[i]) && isascii(word[n+i])) {
                if(tolower(p[i]) != tolower(word[n+i])) {
                    flg = FALSE;
                    break;
                }
            }
            else {
                if(p[i] != word[n+i]) {
                    flg = FALSE;
                    break;
                }
            }

            if(gXyzshSigInt) {
                gXyzshSigInt = FALSE;
                err_msg("interrupt", sname, sline, command);
                return NULL;
            }
        }

        if(flg) {
            return p -n;
        }
        else {
            p--;
        }
    }

    return NULL;
}

BOOL cmd_rindex(sObject* nextin, sObject* nextout, sRunInfo* runinfo)
{
    enum eKanjiCode code = gKanjiCode;
    if(sRunInfo_option(runinfo, "-byte")) {
        code = kByte;
    }
    else if(sRunInfo_option(runinfo, "-utf8")) {
        code = kUtf8;
    }
    else if(sRunInfo_option(runinfo, "-sjis")) {
        code = kSjis;
    }
    else if(sRunInfo_option(runinfo, "-eucjp")) {
        code = kEucjp;
    }

    if(sRunInfo_option(runinfo, "-regex")) {
        if(runinfo->mFilter) {
            if(runinfo->mArgsNumRuntime == 2) {
                char* target = SFD(nextin).mBuf;
                char* regex = runinfo->mArgsRuntime[1];

                regex_t* reg;
                int r = get_onig_regex(&reg, runinfo, regex);

                if(r == ONIG_NORMAL) {
                    /// get starting point ///
                    int len = str_kanjilen(code, target);
                    if(len < 0) {
                        err_msg("invalid target string", runinfo->mSName, runinfo->mSLine, runinfo->mArgs[0]);
                        return FALSE;
                    }

                    int start;
                    char* number;
                    if(number = sRunInfo_option_with_argument(runinfo, "-number")) {
                        start = atoi(number);

                        if(start < 0) { 
                            start += len;
                            if(start < 0) start = 0;
                        }
                        if(start >= len) { 
                            start = len -1 ; 
                            if(start < 0) start = 0;
                        }
                    }
                    else {
                        start = len -1;
                        if(start < 0) start = 0;
                    }

                    /// get search count ///
                    int match_count;
                    char* count;
                    if(count = sRunInfo_option_with_argument(runinfo, "-count")) {
                        match_count = atoi(count);
                        if(match_count <= 0) { match_count = 1; }
                    }
                    else {
                        match_count = 1;
                    }
                    
                    char* start_byte = str_kanjipos2pointer(code, target, start);
                    char* p = start_byte;
                    char* result = NULL;
                    while(p>=target) {
                        if(gXyzshSigInt) {
                            gXyzshSigInt = FALSE;
                            err_msg("signal interrupt", runinfo->mSName, runinfo->mSLine, runinfo->mArgs[0]);
                            runinfo->mRCode = RCODE_SIGNAL_INTERRUPT;
                            return FALSE;
                        }

                        OnigRegion* region = onig_region_new();
                        OnigErrorInfo err_info;

                        int r2 = onig_search(reg, target
                           , target +  strlen(target)
                           , p
                           , target
                           , region, ONIG_OPTION_NONE);

                        if(r2 == ONIG_MISMATCH) {
                            onig_region_free(region, 1);
                            break;
                        }

                        if(r2 >= 0) {
                            result = target + region->beg[0];

                            match_count--;
                            if(match_count == 0) {
                                break;
                            }
                            p = target + region->beg[0] - 1;

                            onig_region_free(region, 1);
                        }
                        else {
                            onig_region_free(region, 1);
                            break;
                        }
                    }

                    char msg[64];
                    int size;
                    if(result == NULL || match_count !=0) {
                        size = snprintf(msg, 64, "-1");
                        runinfo->mRCode = RCODE_NFUN_FALSE;
                    }
                    else {
                        int c = str_pointer2kanjipos(code, target, result);
                        size = snprintf(msg, 64, "%d", c);

                        if(SFD(nextin).mBufLen == 0) {
                            runinfo->mRCode = RCODE_NFUN_NULL_INPUT;
                        }
                        else {
                            runinfo->mRCode = 0;
                        }
                    }

                    if(!sRunInfo_option(runinfo, "-quiet")) {
                        if(!fd_write(nextout, msg, size)) {
                            err_msg("signal interrupt", runinfo->mSName, runinfo->mSLine, runinfo->mArgs[0]);
                            runinfo->mRCode = RCODE_SIGNAL_INTERRUPT;
                            return FALSE;
                        }
                        if(!fd_write(nextout, "\n", 1)) {
                            err_msg("signal interrupt", runinfo->mSName, runinfo->mSLine, runinfo->mArgs[0]);
                            runinfo->mRCode = RCODE_SIGNAL_INTERRUPT;
                            return FALSE;
                        }
                    }
                }
                else {
                    err_msg("invalid regex", runinfo->mSName, runinfo->mSLine, runinfo->mArgs[0]);
                    return FALSE;
                }
            }
        }
    }
    else {
        if(runinfo->mFilter) {
            if(runinfo->mArgsNumRuntime == 2) {
                char* target = SFD(nextin).mBuf;
                char* word = runinfo->mArgsRuntime[1];

                /// get starting point ///
                int len = str_kanjilen(code, target);
                if(len < 0) {
                    err_msg("invalid target string", runinfo->mSName, runinfo->mSLine, runinfo->mArgs[0]);
                    return FALSE;
                }

                int start;
                char* number;
                if(number = sRunInfo_option_with_argument(runinfo, "-number")) {
                    start = atoi(number);

                    if(start < 0) { 
                        start += len;
                        if(start < 0) start = 0;
                    }
                    if(start >= len) { 
                        start = len -1 ; 
                        if(start < 0) start = 0;
                    }
                }
                else {
                    start = len -1;
                    if(start < 0) start = 0;
                }

                /// get search count ///
                int match_count;
                char* count;
                if(count = sRunInfo_option_with_argument(runinfo, "-count")) {
                    match_count = atoi(count);
                    if(match_count <= 0) { match_count = 1; }
                }
                else {
                    match_count = 1;
                }

                
                char* start_byte = str_kanjipos2pointer(code, target, start+1);
                char* p = start_byte;
                char* result = NULL;
                if(sRunInfo_option(runinfo, "-ignore-case")) {
                    while(p>=target) {
                        result = strcasestr_back(p, target, word, runinfo->mSName, runinfo->mSLine, runinfo->mArgs[0]);

                        if(gXyzshSigInt) {
                            gXyzshSigInt = FALSE;
                            err_msg("signal interrupt", runinfo->mSName, runinfo->mSLine, runinfo->mArgs[0]);
                            runinfo->mRCode = RCODE_SIGNAL_INTERRUPT;
                            return FALSE;
                        }

                        if(result != NULL) {
                            match_count--;
                            if(match_count == 0) {
                                break;
                            }
                            p = result - 1;
                        }
                        else {
                            break;
                        }
                    }
                }
                else {
                    while(p>=target) {
                        result = strstr_back(p, target, word, runinfo->mSName, runinfo->mSLine, runinfo->mArgs[0]);

                        if(gXyzshSigInt) {
                            gXyzshSigInt = FALSE;
                            err_msg("signal interrupt", runinfo->mSName, runinfo->mSLine, runinfo->mArgs[0]);
                            runinfo->mRCode = RCODE_SIGNAL_INTERRUPT;
                            return FALSE;
                        }

                        if(result != NULL) {
                            match_count--;
                            if(match_count == 0) {
                                break;
                            }
                            p = result - 1;
                        }
                        else {
                            break;
                        }
                    }
                }

                char msg[64];
                int size;
                if(result == NULL || match_count !=0) {
                    size = snprintf(msg, 64, "-1");
                    runinfo->mRCode = RCODE_NFUN_FALSE;
                }
                else {
                    int c = str_pointer2kanjipos(code, target, result);
                    size = snprintf(msg, 64, "%d", c);

                    if(SFD(nextin).mBufLen == 0) {
                        runinfo->mRCode = RCODE_NFUN_NULL_INPUT;
                    }
                    else {
                        runinfo->mRCode = 0;
                    }
                }

                /// Ω–Œœ ///
                if(!sRunInfo_option(runinfo, "-quiet")) {
                    if(!fd_write(nextout, msg, size)) {
                        err_msg("signal interrupt", runinfo->mSName, runinfo->mSLine, runinfo->mArgs[0]);
                        runinfo->mRCode = RCODE_SIGNAL_INTERRUPT;
                        return FALSE;
                    }
                    if(!fd_write(nextout, "\n", 1)) {
                        err_msg("signal interrupt", runinfo->mSName, runinfo->mSLine, runinfo->mArgs[0]);
                        runinfo->mRCode = RCODE_SIGNAL_INTERRUPT;
                        return FALSE;
                    }
                }
            }
        }
    }

    return TRUE;
}

BOOL cmd_sub(sObject* nextin, sObject* nextout, sRunInfo* runinfo)
{
    BOOL quiet = sRunInfo_option(runinfo, "-quiet");
    BOOL ignore = sRunInfo_option(runinfo, "-ignore-case");

    if(sRunInfo_option(runinfo, "-no-regex")) {
        if(runinfo->mFilter && runinfo->mArgsNumRuntime == 3) {
            BOOL global = sRunInfo_option(runinfo, "-global");

            char* word = runinfo->mArgsRuntime[1];
            char* destination = runinfo->mArgsRuntime[2];
            const int destination_len = strlen(destination);

            int sub_count = 0;

            char* target = SFD(nextin).mBuf;
            char* p = target;

            while(1) {
                char* result = strstr(p, word);
                if(ignore) {
                    result = strcasestr(p, word);
                }
                else {
                    result = strstr(p, word);
                }

                if(result == NULL) {
                    if(!quiet) {
                        if(!fd_write(nextout, p, strlen(p))) {
                            err_msg("signal interrupt", runinfo->mSName, runinfo->mSLine, runinfo->mArgs[0]);
                            runinfo->mRCode = RCODE_SIGNAL_INTERRUPT;
                            return FALSE;
                        }
                    }
                    break;
                }

                if(!quiet) {
                    if(!fd_write(nextout, p, result - p)) {
                        err_msg("signal interrupt", runinfo->mSName, runinfo->mSLine, runinfo->mArgs[0]);
                        runinfo->mRCode = RCODE_SIGNAL_INTERRUPT;
                        return FALSE;
                    }
                    if(!fd_write(nextout, destination, destination_len)) {
                        err_msg("signal interrupt", runinfo->mSName, runinfo->mSLine, runinfo->mArgs[0]);
                        runinfo->mRCode = RCODE_SIGNAL_INTERRUPT;
                        return FALSE;
                    }
                }

                p = result + strlen(word);
                sub_count++;

                if(!global) {
                    if(!quiet) {
                        if(!fd_write(nextout, p, strlen(p))) {
                            err_msg("signal interrupt", runinfo->mSName, runinfo->mSLine, runinfo->mArgs[0]);
                            runinfo->mRCode = RCODE_SIGNAL_INTERRUPT;
                            return FALSE;
                        }
                    }
                    break;
                }
            }

            char buf[128];
            snprintf(buf, 128, "%d", sub_count);
            uobject_put(gRootObject, "SUB_COUNT", STRING_NEW_GC(buf, FALSE));

            if(sub_count > 0) {
                if(SFD(nextin).mBufLen == 0) {
                    runinfo->mRCode = RCODE_NFUN_NULL_INPUT;
                }
                else {
                    runinfo->mRCode = 0;
                }
            }
            else {
                runinfo->mRCode = RCODE_NFUN_FALSE;
            }
        }
    }
    else {
        if(runinfo->mFilter && (runinfo->mArgsNumRuntime == 3 || runinfo->mArgsNumRuntime == 2 && runinfo->mBlocksNum >= 1)) {
            sObject* block;
            sObject* nextin2;
            sObject* nextout2;

            if(runinfo->mBlocksNum >= 1) {
                block = runinfo->mBlocks[0];
                nextin2 = FD_NEW_STACK();
                nextout2 = FD_NEW_STACK();
            }
            else {
                block = NULL;
            }

            BOOL global = sRunInfo_option(runinfo, "-global");

            char* regex = runinfo->mArgsRuntime[1];
            char* destination = runinfo->mArgsRuntime[2];

            int sub_count = 0;

            regex_t* reg;
            int r = get_onig_regex(&reg, runinfo, regex);

            if(r == ONIG_NORMAL) {
               char* p = SFD(nextin).mBuf;

               sObject* sub_str = STRING_NEW_STACK("");

                while(1) {
                    OnigRegion* region = onig_region_new();
                    OnigErrorInfo err_info;

                    char* target = SFD(nextin).mBuf;

                    const int point = p - target;
                    int r2 = onig_search(reg, target
                       , target + strlen(target)
                       , p
                       , p + strlen(p)
                       , region, ONIG_OPTION_NONE);

                    if(r2 == ONIG_MISMATCH) {
                        onig_region_free(region, 1);

                        if(!quiet) {
                            if(!fd_write(nextout, p, strlen(p))) {
                                err_msg("signal interrupt", runinfo->mSName, runinfo->mSLine, runinfo->mArgs[0]);
                                runinfo->mRCode = RCODE_SIGNAL_INTERRUPT;
                                onig_free(reg);
                                return FALSE;
                            }
                        }
                        break;
                    }
                    else {
                        /// make distination ///
                        string_put(sub_str, "");

                        if(block) {
                            clear_matching_info_variable();

                            const int size = region->beg[0] - (p - target);
                            if(size > 0) {
                                uobject_put(gRootObject, "PREMATCH", STRING_NEW_GC3(p, size, FALSE));
                            }

                            const int size2 = region->end[0] - region->beg[0];

                            uobject_put(gRootObject, "MATCH", STRING_NEW_GC3(target + region->beg[0], size2, FALSE));
                            uobject_put(gRootObject, "0", STRING_NEW_GC3(target + region->beg[0], size2, FALSE));

                            const int n = strlen(target)-region->end[0];
                            if(n > 0) {
                                uobject_put(gRootObject, "POSTMATCH", STRING_NEW_GC3(target + region->end[0], n, FALSE));
                            }

                            int i;
                            for (i=1; i<region->num_regs; i++) {
                                const int size = region->end[i] - region->beg[i];

                                char name[16];
                                snprintf(name, 16, "%d", i);

                                uobject_put(gRootObject, name, STRING_NEW_GC3(target + region->beg[i], size, FALSE));
                            }

                            if(region->num_regs > 0) {
                                const int n = region->num_regs -1;

                                const int size = region->end[n] - region->beg[n];

                                uobject_put(gRootObject, "LAST_MATCH", STRING_NEW_GC3(target + region->beg[n], size, FALSE));
                            }

                            char buf[128];
                            snprintf(buf, 128, "%d", region->num_regs);
                            uobject_put(gRootObject, "MATCH_NUMBER", STRING_NEW_GC(buf, FALSE));

                            fd_clear(nextin2);
                            fd_clear(nextout2);

                            if(!fd_write(nextin2, target + region->beg[0], region->end[0]-region->beg[0])) {
                                err_msg("interrupt", runinfo->mSName, runinfo->mSLine, runinfo->mArgs[0]);
                                runinfo->mRCode = RCODE_SIGNAL_INTERRUPT;
                                onig_region_free(region, 1);
                                onig_free(reg);
                                return FALSE;
                            }

                            int rcode = 0;
                            if(!run(block, nextin2, nextout2, &rcode, runinfo->mCurrentObject, runinfo->mRunningObject)) {
                                runinfo->mRCode = rcode;
                                onig_region_free(region, 1);
                                onig_free(reg);
                                return FALSE;
                            }

                            string_put(sub_str, SFD(nextout2).mBuf);
                        }
                        else {
                            char* p2 = destination;

                            while(*p2) {
                                if(*p2 == '\\') {
                                    if(*(p2+1) == '\\') {
                                        p2+=2;
                                        string_push_back2(sub_str , '\\');
                                    }
                                    else if(*(p2+1) >= '0' && *(p2+1) <= '9') {
                                        int n = *(p2+1) - '0';

                                        if(n < region->num_regs) {
                                            p2+=2;

                                            const int size = region->end[n] - region->beg[n];

                                            string_push_back3(sub_str, target + region->beg[n], size);
                                        }
                                        else {
                                            string_push_back2(sub_str, *p2++);
                                            string_push_back2(sub_str , *p2++);
                                        }
                                    }
                                    else if(*(p2+1) == '&') {
                                        p2 += 2;

                                        const int size = region->end[0] - region->beg[0];

                                        string_push_back3(sub_str, target + region->beg[0], size);
                                    }
                                    else if(*(p2+1) == '`') {
                                        p2+=2;

                                        string_push_back3(sub_str, target, region->beg[0]);
                                    }
                                    else if(*(p2+1) == '\'') {
                                        p2+=2;

                                        string_push_back(sub_str, target + region->end[0]);
                                    }
                                    else if(*(p2+1) == '+') {
                                        p2+=2;

                                        if(region->num_regs > 0) {
                                            const int n = region->num_regs - 1;

                                            const int size = region->end[n] - region->beg[n];

                                            string_push_back3(sub_str, target + region->beg[n], size);
                                        }
                                    }
                                    else {
                                        string_push_back2(sub_str, *p2++);
                                    }
                                }
                                else { 
                                    string_push_back2(sub_str, *p2++);
                                }
                            }
                        }

                        if(!quiet) {
                            if(!fd_write(nextout, p, region->beg[0]-point)) {
                                err_msg("signal interrupt", runinfo->mSName, runinfo->mSLine, runinfo->mArgs[0]);
                                runinfo->mRCode = RCODE_SIGNAL_INTERRUPT;
                                onig_region_free(region, 1);
                                onig_free(reg);
                                return FALSE;
                            }
                            if(!fd_write(nextout, string_c_str(sub_str), string_length(sub_str))) {
                                err_msg("signal interrupt", runinfo->mSName, runinfo->mSLine, runinfo->mArgs[0]);
                                runinfo->mRCode = RCODE_SIGNAL_INTERRUPT;
                                onig_region_free(region, 1);
                                onig_free(reg);
                                return FALSE;
                            }
                        }

                        sub_count++;

                        if(region->beg[0] == region->end[0]) {
                            char buf[2];
                            buf[0] = target[region->beg[0]];
                            buf[1] = 0;

                            if(!quiet) {
                                if(!fd_write(nextout, buf, 1)) {
                                    err_msg("signal interrupt", runinfo->mSName, runinfo->mSLine, runinfo->mArgs[0]);
                                    runinfo->mRCode = RCODE_SIGNAL_INTERRUPT;
                                    onig_region_free(region, 1);
                                    onig_free(reg);
                                    return FALSE;
                                }
                            }

                            p = target + region->end[0] + 1;

                            if(p > target + strlen(target)) {
                                break;
                            }
                        }
                        else {
                            p= target + region->end[0];
                        }

                        onig_region_free(region, 1);

                        if(!global && !quiet) {
                            if(!fd_write(nextout, p, strlen(p))) {
                                err_msg("signal interrupt", runinfo->mSName, runinfo->mSLine, runinfo->mArgs[0]);
                                runinfo->mRCode = RCODE_SIGNAL_INTERRUPT;
                                onig_free(reg);
                                return FALSE;
                            }
                            break;
                        }
                    }
                }
            }
            else {
                err_msg("invalid regex", runinfo->mSName, runinfo->mSLine, runinfo->mArgs[0]);
                return FALSE;
            }

            onig_free(reg);

            char buf[128];
            snprintf(buf, 128, "%d", sub_count);
            uobject_put(gRootObject, "SUB_COUNT", STRING_NEW_GC(buf, FALSE));

            if(sub_count > 0) {
                if(SFD(nextin).mBufLen == 0) {
                    runinfo->mRCode = RCODE_NFUN_NULL_INPUT;
                }
                else {
                    runinfo->mRCode = 0;
                }
            }
            else {
                runinfo->mRCode = RCODE_NFUN_FALSE;
            }
        }
    }

    return TRUE;
}

BOOL cmd_split(sObject* nextin, sObject* nextout, sRunInfo* runinfo)
{
    char* field = "\n";
    eLineField lf = gLineField;
    if(sRunInfo_option(runinfo, "-Lw")) {
        lf = kCRLF;
        field = "\r\n";
    }
    else if(sRunInfo_option(runinfo, "-Lm")) {
        lf = kCR;
        field = "\r";
    }
    else if(sRunInfo_option(runinfo, "-Lu")) {
        lf = kLF;
        field = "\n";
    }
    else if(sRunInfo_option(runinfo, "-La")) {
        lf = kBel;
        field = "\a";
    }

    enum eKanjiCode code = gKanjiCode;
    if(sRunInfo_option(runinfo, "-byte")) {
        code = kByte;
    }
    else if(sRunInfo_option(runinfo, "-utf8")) {
        code = kUtf8;
    }
    else if(sRunInfo_option(runinfo, "-sjis")) {
        code = kSjis;
    }
    else if(sRunInfo_option(runinfo, "-eucjp")) {
        code = kEucjp;
    }

    if(runinfo->mFilter) {
        if(sRunInfo_option(runinfo, "-no-regex")) {
            BOOL ignore_case = sRunInfo_option(runinfo, "-ignore-case");
            char* word;
            int word_len;

            if(runinfo->mArgsNumRuntime == 1) {
                word = " ";
                word_len = 1;
            }
            else if(runinfo->mArgsNumRuntime >= 2) {
                word = runinfo->mArgsRuntime[1];
                word_len = strlen(word);
            }

            char* target;
            char* p = target = SFD(nextin).mBuf;
            int target_len = SFD(nextin).mBufLen;

            int split_count = 0;

            while(p - target < target_len) {
                char* result;
                if(word[0] == 0) {
                    result = p + 1;
                }
                else if(ignore_case) {
                    result = strcasestr(p, word);
                }
                else {
                    result = strstr(p, word);
                }

                if(result == NULL) {
                    if(*p) {
                        if(!fd_write(nextout, p, strlen(p))) {
                            err_msg("singal interrupt", runinfo->mSName, runinfo->mSLine, runinfo->mArgs[0]);
                            runinfo->mRCode = RCODE_SIGNAL_INTERRUPT;
                            return FALSE;
                        }
                    }

                    if(!fd_write(nextout, field, strlen(field))) {
                        err_msg("singal interrupt", runinfo->mSName, runinfo->mSLine, runinfo->mArgs[0]);
                        runinfo->mRCode = RCODE_SIGNAL_INTERRUPT;
                        return FALSE;
                    }
                    break;
                }
                else {
                    split_count++;

                    if(!fd_write(nextout, p, result - p)) {
                        err_msg("singal interrupt", runinfo->mSName, runinfo->mSLine, runinfo->mArgs[0]);
                        runinfo->mRCode = RCODE_SIGNAL_INTERRUPT;
                        return FALSE;
                    }

                    if(!fd_write(nextout, field, strlen(field))) {
                        err_msg("singal interrupt", runinfo->mSName, runinfo->mSLine, runinfo->mArgs[0]);
                        runinfo->mRCode = RCODE_SIGNAL_INTERRUPT;
                        return FALSE;
                    }

                    p = result + word_len;
                }
            }

            if(split_count > 0) {
                if(SFD(nextin).mBufLen == 0) {
                    runinfo->mRCode = RCODE_NFUN_NULL_INPUT;
                }
                else {
                    runinfo->mRCode = 0;
                }
            }
            else {
                runinfo->mRCode = 1;
            }
        } else {
            char* regex;

            if(runinfo->mArgsNumRuntime == 1) {
                regex = "\\s+";
            }
            else if(runinfo->mArgsNumRuntime >= 2) {
                regex = runinfo->mArgsRuntime[1];
            }

            regex_t* reg;
            int r = get_onig_regex(&reg, runinfo, regex);

            if(r == ONIG_NORMAL) {
                char* target;
                char* p = target = SFD(nextin).mBuf;
                int target_len = SFD(nextin).mBufLen;

                int split_count = 0;

                while(p - target < target_len) {
                    OnigRegion* region = onig_region_new();

                    int r2 = onig_search(reg, target
                       , target + strlen(target)
                       , p, p + strlen(p)
                       , region, ONIG_OPTION_NONE);
                       
                    if(r2 == ONIG_MISMATCH) {
                        onig_region_free(region, 1);

                        if(*p) {
                            if(!fd_write(nextout, p, strlen(p))) {
                                err_msg("singal interrupt", runinfo->mSName, runinfo->mSLine, runinfo->mArgs[0]);
                                runinfo->mRCode = RCODE_SIGNAL_INTERRUPT;
                                onig_free(reg);
                                return FALSE;
                            }
                        }

                        if(!fd_write(nextout, field, strlen(field))) {
                            err_msg("singal interrupt", runinfo->mSName, runinfo->mSLine, runinfo->mArgs[0]);
                            runinfo->mRCode = RCODE_SIGNAL_INTERRUPT;
                            onig_free(reg);
                            return FALSE;
                        }
                        break;
                    }
                    else {
                        split_count++;

                        if(region->beg[0] == region->end[0]) {
                            if(target + region->beg[0] == p) {
                                char* end_byte = str_kanjipos2pointer(code, p, 1);
                                int size = end_byte - p;

                                if(size > 0) {
                                    if(!fd_write(nextout, p, size)) {
                                        err_msg("singal interrupt", runinfo->mSName, runinfo->mSLine, runinfo->mArgs[0]);
                                        runinfo->mRCode = RCODE_SIGNAL_INTERRUPT;
                                        onig_region_free(region, 1);
                                        onig_free(reg);
                                        return FALSE;
                                    }

                                    if(!fd_write(nextout, field, strlen(field))) {
                                        err_msg("singal interrupt", runinfo->mSName, runinfo->mSLine, runinfo->mArgs[0]);
                                        runinfo->mRCode = RCODE_SIGNAL_INTERRUPT;
                                        onig_region_free(region, 1);
                                        onig_free(reg);
                                        return FALSE;
                                    }

                                    p += size;
                                }
                                else {
                                    onig_region_free(region, 1);

                                    break;
                                }
                            }
                            else {
                                int size = region->beg[0] - (p-target);

                                if(size > 0) {
                                    if(!fd_write(nextout, p, size)) {
                                        err_msg("singal interrupt", runinfo->mSName, runinfo->mSLine, runinfo->mArgs[0]);
                                        runinfo->mRCode = RCODE_SIGNAL_INTERRUPT;
                                        onig_region_free(region, 1);
                                        onig_free(reg);
                                        return FALSE;
                                    }

                                    if(!fd_write(nextout, field, strlen(field))) {
                                        err_msg("singal interrupt", runinfo->mSName, runinfo->mSLine, runinfo->mArgs[0]);
                                        runinfo->mRCode = RCODE_SIGNAL_INTERRUPT;
                                        onig_region_free(region, 1);
                                        onig_free(reg);
                                        return FALSE;
                                    }
                                }
                                else if(size == 0) {
                                    if(!fd_write(nextout, field, strlen(field))) {
                                        err_msg("singal interrupt", runinfo->mSName, runinfo->mSLine, runinfo->mArgs[0]);
                                        runinfo->mRCode = RCODE_SIGNAL_INTERRUPT;
                                        onig_region_free(region, 1);
                                        onig_free(reg);
                                        return FALSE;
                                    }
                                }

                                p = target + region->end[0];
                            }
                        }
                        else {
                            int size = region->beg[0] - (p-target);

                            if(size > 0) {
                                if(!fd_write(nextout, p, size)) {
                                    err_msg("singal interrupt", runinfo->mSName, runinfo->mSLine, runinfo->mArgs[0]);
                                    runinfo->mRCode = RCODE_SIGNAL_INTERRUPT;
                                    onig_region_free(region, 1);
                                    onig_free(reg);
                                    return FALSE;
                                }

                                if(!fd_write(nextout, field, strlen(field))) {
                                    err_msg("singal interrupt", runinfo->mSName, runinfo->mSLine, runinfo->mArgs[0]);
                                    runinfo->mRCode = RCODE_SIGNAL_INTERRUPT;
                                    onig_region_free(region, 1);
                                    onig_free(reg);
                                    return FALSE;
                                }
                            }
                            else if(size == 0) {
                                if(!fd_write(nextout, field, strlen(field))) {
                                    err_msg("singal interrupt", runinfo->mSName, runinfo->mSLine, runinfo->mArgs[0]);
                                    runinfo->mRCode = RCODE_SIGNAL_INTERRUPT;
                                    onig_region_free(region, 1);
                                    onig_free(reg);
                                    return FALSE;
                                }
                            }

                            p = target + region->end[0];
                        }

                        if(region->num_regs > 1) {
                            /// group strings ///
                            int i;
                            for(i=1; i<region->num_regs; i++) {
                                const int size = region->end[i]-region->beg[i];
                                if(size > 0) {
                                    if(!fd_write(nextout, target + region->beg[i], size)) {
                                        err_msg("singal interrupt", runinfo->mSName, runinfo->mSLine, runinfo->mArgs[0]);
                                        runinfo->mRCode = RCODE_SIGNAL_INTERRUPT;
                                        onig_region_free(region, 1);
                                        onig_free(reg);
                                        return FALSE;
                                    }

                                    if(!fd_write(nextout, field, strlen(field))) {
                                        err_msg("singal interrupt", runinfo->mSName, runinfo->mSLine, runinfo->mArgs[0]);
                                        runinfo->mRCode = RCODE_SIGNAL_INTERRUPT;
                                        onig_region_free(region, 1);
                                        onig_free(reg);
                                        return FALSE;
                                    }
                                }
                            }
                        }
                    }

                    onig_region_free(region, 1);
                }

                if(split_count > 0) {
                    if(SFD(nextin).mBufLen == 0) {
                        runinfo->mRCode = RCODE_NFUN_NULL_INPUT;
                    }
                    else {
                        runinfo->mRCode = 0;
                    }
                }
                else {
                    runinfo->mRCode = 1;
                }
            }
            else {
                err_msg("invalid regex", runinfo->mSName, runinfo->mSLine, runinfo->mArgs[0]);
                return FALSE;
            }

            onig_free(reg);
        }
    }

    return TRUE;
}


