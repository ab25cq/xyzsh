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

int get_onig_regex(regex_t** reg, sCommand* command, char* regex)
{
    enum eKanjiCode code = gKanjiCode;
    if(sCommand_option_item(command, "-byte")) {
        code = kByte;
    }
    else if(sCommand_option_item(command, "-utf8")) {
        code = kUtf8;
    }
    else if(sCommand_option_item(command, "-sjis")) {
        code = kSjis;
    }
    else if(sCommand_option_item(command, "-eucjp")) {
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
    if(sCommand_option_item(command, "-ignore-case")) {
        option |= ONIG_OPTION_IGNORECASE;
    }
    else if(sCommand_option_item(command, "-multi-line")) {
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
    sCommand* command = runinfo->mCommand;

    enum eKanjiCode code = gKanjiCode;
    if(sCommand_option_item(command, "-byte")) {
        code = kByte;
    }
    else if(sCommand_option_item(command, "-utf8")) {
        code = kUtf8;
    }
    else if(sCommand_option_item(command, "-sjis")) {
        code = kSjis;
    }
    else if(sCommand_option_item(command, "-eucjp")) {
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
                        err_msg("signal interrupt", runinfo->mSName, runinfo->mSLine, command->mArgs[0]);
                        runinfo->mRCode = RCODE_SIGNAL_INTERRUPT;
                        return FALSE;
                    }
                }
                else {
                    char buf[32];
                    int size = snprintf(buf, 32, "\\%c", *p);

                    if(!fd_write(nextout, buf, size)) {
                        err_msg("signal interrupt", runinfo->mSName, runinfo->mSLine, command->mArgs[0]);
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
                        err_msg("signal interrupt", runinfo->mSName, runinfo->mSLine, command->mArgs[0]);
                        runinfo->mRCode = RCODE_SIGNAL_INTERRUPT;
                        return FALSE;
                    }
                }
                else if(isalpha(*p)) {
                    char buf[32];
                    int size = snprintf(buf, 32, "%c", *p);

                    if(!fd_write(nextout, buf, size)) {
                        err_msg("signal interrupt", runinfo->mSName, runinfo->mSLine, command->mArgs[0]);
                        runinfo->mRCode = RCODE_SIGNAL_INTERRUPT;
                        return FALSE;
                    }
                }
                else {
                    char buf[32];
                    int size = snprintf(buf, 32, "\\%c", *p);

                    if(!fd_write(nextout, buf, size)) {
                        err_msg("signal interrupt", runinfo->mSName, runinfo->mSLine, command->mArgs[0]);
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
                        err_msg("signal interrupt", runinfo->mSName, runinfo->mSLine, command->mArgs[0]);
                        runinfo->mRCode = RCODE_SIGNAL_INTERRUPT;
                        return FALSE;
                    }
                }
                else {
                    char buf[32];
                    int size = snprintf(buf, 32, "\\%c", *p);

                    if(!fd_write(nextout, buf, size)) {
                        err_msg("signal interrupt", runinfo->mSName, runinfo->mSLine, command->mArgs[0]);
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
    sCommand* command = runinfo->mCommand;

    enum eKanjiCode code = gKanjiCode;
    if(sCommand_option_item(command, "-byte")) {
        code = kByte;
    }
    else if(sCommand_option_item(command, "-utf8")) {
        code = kUtf8;
    }
    else if(sCommand_option_item(command, "-sjis")) {
        code = kSjis;
    }
    else if(sCommand_option_item(command, "-eucjp")) {
        code = kEucjp;
    }

    eLineField lf = gLineField;
    if(sCommand_option_item(command, "-Lw")) {
        lf = kCRLF;
    }
    else if(sCommand_option_item(command, "-Lm")) {
        lf = kCR;
    }
    else if(sCommand_option_item(command, "-Lu")) {
        lf = kLF;
    }
    else if(sCommand_option_item(command, "-La")) {
        lf = kBel;
    }

    /// output
    if(runinfo->mFilter) {
        /// line number
        if(sCommand_option_item(command, "-line-num")) {
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
                err_msg("signal interrupt", runinfo->mSName, runinfo->mSLine, command->mArgs[0]);
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
                err_msg("signal interrupt", runinfo->mSName, runinfo->mSLine, command->mArgs[0]);
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
    sCommand* command = runinfo->mCommand;

    /// input
    if(runinfo->mFilter) {
        if(command->mArgsNumRuntime == 2) {
            int multiple = atoi(command->mArgsRuntime[1]);
            if(multiple < 1) multiple = 1;

            int i;
            for(i=0; i<multiple; i++) {
                if(!fd_write(nextout, SFD(nextin).mBuf, SFD(nextin).mBufLen)) {
                    err_msg("signal interrupt", runinfo->mSName, runinfo->mSLine, command->mArgs[0]);
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

BOOL cmd_index(sObject* nextin, sObject* nextout, sRunInfo* runinfo)
{
    sCommand* command = runinfo->mCommand;

    enum eKanjiCode code = gKanjiCode;
    if(sCommand_option_item(command, "-byte")) {
        code = kByte;
    }
    else if(sCommand_option_item(command, "-utf8")) {
        code = kUtf8;
    }
    else if(sCommand_option_item(command, "-sjis")) {
        code = kSjis;
    }
    else if(sCommand_option_item(command, "-eucjp")) {
        code = kEucjp;
    }

    /// output
    if(runinfo->mFilter) {
        if(command->mArgsNumRuntime == 2) {
            char* target = SFD(nextin).mBuf;

            char* word = command->mArgsRuntime[1];

            /// get starting point ///
            int start;
            char* number;
            if(number = sCommand_option_with_argument_item(command, "-number")) {
                start = atoi(number);

                int len = str_kanjilen(code, target);
                if(len < 0) {
                    err_msg("invalid target string", runinfo->mSName, runinfo->mSLine, command->mArgs[0]);
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
            if(count = sCommand_option_with_argument_item(command, "-count")) {
                match_count = atoi(count);
                if(match_count <= 0) { match_count = 1; }
            }
            else {
                match_count = 1;
            }

            char* start_byte = str_kanjipos2pointer(code, target, start);
            char* p = start_byte;
            char* result = NULL;
            if(sCommand_option_item(command, "-ignore-case")) {
                while(p < start_byte + strlen(start_byte)) {
                    if(gXyzshSigInt) {
                        gXyzshSigInt = FALSE;
                        err_msg("signal interrupt", runinfo->mSName, runinfo->mSLine, command->mArgs[0]);
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
                        err_msg("signal interrupt", runinfo->mSName, runinfo->mSLine, command->mArgs[0]);
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

            /// Ω–Œœ ///
            if(!sCommand_option_item(command, "-quiet")) {
                if(!fd_write(nextout, msg, size)) {
                    err_msg("signal interrupt", runinfo->mSName, runinfo->mSLine, command->mArgs[0]);
                    runinfo->mRCode = RCODE_SIGNAL_INTERRUPT;
                    return FALSE;
                }
                if(!fd_write(nextout, "\n", 1)) {
                    err_msg("signal interrupt", runinfo->mSName, runinfo->mSLine, command->mArgs[0]);
                    runinfo->mRCode = RCODE_SIGNAL_INTERRUPT;
                    return FALSE;
                }
            }
        }
    }

    return TRUE;
}

static char* strstr_back(char* p, char* start, char* word, char* sname, int sline, char* command)
{
    int n = strlen(word);

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
    sCommand* command = runinfo->mCommand;

    enum eKanjiCode code = gKanjiCode;
    if(sCommand_option_item(command, "-byte")) {
        code = kByte;
    }
    else if(sCommand_option_item(command, "-utf8")) {
        code = kUtf8;
    }
    else if(sCommand_option_item(command, "-sjis")) {
        code = kSjis;
    }
    else if(sCommand_option_item(command, "-eucjp")) {
        code = kEucjp;
    }

    /// output
    if(runinfo->mFilter) {
        if(command->mArgsNumRuntime == 2) {
            char* target = SFD(nextin).mBuf;
            char* word = command->mArgsRuntime[1];

            /// get starting point ///
            int len = str_kanjilen(code, target);
            if(len < 0) {
                err_msg("invalid target string", runinfo->mSName, runinfo->mSLine, command->mArgs[0]);
                return FALSE;
            }

            int start;
            char* number;
            if(number = sCommand_option_with_argument_item(command, "-number")) {
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
            if(count = sCommand_option_with_argument_item(command, "-count")) {
                match_count = atoi(count);
                if(match_count <= 0) { match_count = 1; }
            }
            else {
                match_count = 1;
            }

            
            char* start_byte = str_kanjipos2pointer(code, target, start+1);
            char* p = start_byte;
            char* result = NULL;
            if(sCommand_option_item(command, "-ignore-case")) {
                while(p>=target) {
                    result = strcasestr_back(p, target, word, runinfo->mSName, runinfo->mSLine, command->mArgs[0]);

                    if(gXyzshSigInt) {
                        gXyzshSigInt = FALSE;
                        err_msg("signal interrupt", runinfo->mSName, runinfo->mSLine, command->mArgs[0]);
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
                    result = strstr_back(p, target, word, runinfo->mSName, runinfo->mSLine, command->mArgs[0]);

                    if(gXyzshSigInt) {
                        gXyzshSigInt = FALSE;
                        err_msg("signal interrupt", runinfo->mSName, runinfo->mSLine, command->mArgs[0]);
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
            if(!sCommand_option_item(command, "-quiet")) {
                if(!fd_write(nextout, msg, size)) {
                    err_msg("signal interrupt", runinfo->mSName, runinfo->mSLine, command->mArgs[0]);
                    runinfo->mRCode = RCODE_SIGNAL_INTERRUPT;
                    return FALSE;
                }
                if(!fd_write(nextout, "\n", 1)) {
                    err_msg("signal interrupt", runinfo->mSName, runinfo->mSLine, command->mArgs[0]);
                    runinfo->mRCode = RCODE_SIGNAL_INTERRUPT;
                    return FALSE;
                }
            }
        }
    }

    return TRUE;
}

BOOL cmd_lc(sObject* nextin, sObject* nextout, sRunInfo* runinfo)
{
    sCommand* command = runinfo->mCommand;

    enum eKanjiCode code = gKanjiCode;
    if(sCommand_option_item(command, "-byte")) {
        code = kByte;
    }
    else if(sCommand_option_item(command, "-utf8")) {
        code = kUtf8;
    }
    else if(sCommand_option_item(command, "-sjis")) {
        code = kSjis;
    }
    else if(sCommand_option_item(command, "-eucjp")) {
        code = kEucjp;
    }

    if(runinfo->mFilter) {
        sObject* str = STRING_NEW_STACK(SFD(nextin).mBuf);
        string_tolower(str, code);

        if(!fd_write(nextout, string_c_str(str), string_length(str))) {
            err_msg("signal interrupt", runinfo->mSName, runinfo->mSLine, command->mArgs[0]);
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
    sCommand* command = runinfo->mCommand;

    enum eKanjiCode code = gKanjiCode;
    if(sCommand_option_item(command, "-byte")) {
        code = kByte;
    }
    else if(sCommand_option_item(command, "-utf8")) {
        code = kUtf8;
    }
    else if(sCommand_option_item(command, "-sjis")) {
        code = kSjis;
    }
    else if(sCommand_option_item(command, "-eucjp")) {
        code = kEucjp;
    }

    if(runinfo->mFilter) {
        sObject* str = STRING_NEW_STACK(SFD(nextin).mBuf);
        string_toupper(str, code);

        if(!fd_write(nextout, string_c_str(str), string_length(str))) {
            err_msg("signal interrupt", runinfo->mSName, runinfo->mSLine, command->mArgs[0]);
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
    sCommand* command = runinfo->mCommand;

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
            err_msg("signal interrupt", runinfo->mSName, runinfo->mSLine, command->mArgs[0]);
            runinfo->mRCode = RCODE_SIGNAL_INTERRUPT;
            return FALSE;
        }
    }

    return TRUE;
}

BOOL cmd_chop(sObject* nextin, sObject* nextout, sRunInfo* runinfo)
{
    sCommand* command = runinfo->mCommand;

    enum eKanjiCode code = gKanjiCode;
    if(sCommand_option_item(command, "-byte")) {
        code = kByte;
    }
    else if(sCommand_option_item(command, "-utf8")) {
        code = kUtf8;
    }
    else if(sCommand_option_item(command, "-sjis")) {
        code = kSjis;
    }
    else if(sCommand_option_item(command, "-eucjp")) {
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
            err_msg("signal interrupt", runinfo->mSName, runinfo->mSLine, command->mArgs[0]);
            runinfo->mRCode = RCODE_SIGNAL_INTERRUPT;
            return FALSE;
        }
    }

    return TRUE;
}

BOOL cmd_pomch(sObject* nextin, sObject* nextout, sRunInfo* runinfo)
{
    sCommand* command = runinfo->mCommand;

    eLineField lf = gLineField;
    if(sCommand_option_item(command, "-Lw")) {
        lf = kCRLF;
    }
    else if(sCommand_option_item(command, "-Lm")) {
        lf = kCR;
    }
    else if(sCommand_option_item(command, "-Lu")) {
        lf = kLF;
    }
    else if(sCommand_option_item(command, "-La")) {
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
            err_msg("signal interrupt", runinfo->mSName, runinfo->mSLine, command->mArgs[0]);
            runinfo->mRCode = RCODE_SIGNAL_INTERRUPT;
            return FALSE;
        }
    }

    return TRUE;
}

BOOL cmd_printf(sObject* nextin, sObject* nextout, sRunInfo* runinfo)
{
    sCommand* command = runinfo->mCommand;

    eLineField lf = gLineField;
    if(sCommand_option_item(command, "-Lw")) {
        lf = kCRLF;
    }
    else if(sCommand_option_item(command, "-Lm")) {
        lf = kCR;
    }
    else if(sCommand_option_item(command, "-Lu")) {
        lf = kLF;
    }
    else if(sCommand_option_item(command, "-La")) {
        lf = kBel;
    }

    if(runinfo->mFilter && command->mArgsNumRuntime == 2) {
        fd_split(nextin, lf);

        /// go ///
        char* p = command->mArgsRuntime[1];

        int strings_num = 0;
        while(*p) {
            if(*p == '%') {
                p++;

                if(*p == '%') {
                    p++;
                    if(!fd_writec(nextout, '%')) {
                        err_msg("signal interrupt", runinfo->mSName, runinfo->mSLine, command->mArgs[0]);
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
                        if(strings_num < vector_count(SFD(nextin).mLines)) {
                            arg = vector_item(SFD(nextin).mLines, strings_num);
                        }
                        else {
                            arg = "0";
                        }
                        strings_num++;

                        char* buf;
                        int size = asprintf(&buf, aformat, atoi(arg));

                        if(!fd_write(nextout, buf, size)) {
                            err_msg("signal interrupt", runinfo->mSName, runinfo->mSLine, command->mArgs[0]);
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
                        if(strings_num < vector_count(SFD(nextin).mLines)) {
                            arg = vector_item(SFD(nextin).mLines, strings_num);
                        }
                        else {
                            arg = "0";
                        }
                        strings_num++;

                        char* buf;
                        int size = asprintf(&buf, aformat, atof(arg));

                        if(!fd_write(nextout, buf, size)) {
                            err_msg("signal interrupt", runinfo->mSName, runinfo->mSLine, command->mArgs[0]);
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
                        if(strings_num < vector_count(SFD(nextin).mLines)) {
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
                            err_msg("signal interrupt", runinfo->mSName, runinfo->mSLine, command->mArgs[0]);
                            runinfo->mRCode = RCODE_SIGNAL_INTERRUPT;
                            return FALSE;
                        }
                        free(buf);
                    }
                    else {
                        err_msg("invalid format at printf", runinfo->mSName, runinfo->mSLine, command->mArgs[0]);
                        return FALSE;
                    }
                }
            }
            else {
                if(!fd_writec(nextout, *p++)) {
                    err_msg("signal interrupt", runinfo->mSName, runinfo->mSLine, command->mArgs[0]);
                    runinfo->mRCode = RCODE_SIGNAL_INTERRUPT;
                    return FALSE;
                }
            }
        }

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

    return TRUE;
}

BOOL cmd_sub(sObject* nextin, sObject* nextout, sRunInfo* runinfo)
{
    sCommand* command = runinfo->mCommand;
    
    BOOL quiet = sCommand_option_item(command, "-quiet");

    if(runinfo->mFilter && (command->mArgsNumRuntime == 3 || command->mArgsNumRuntime == 2 && command->mBlocksNum >= 1)) {
        sObject* block;
        sObject* nextin2;
        sObject* nextout2;

        if(command->mBlocksNum >= 1) {
            block = command->mBlocks[0];
            nextin2 = FD_NEW_STACK();
            nextout2 = FD_NEW_STACK();
        }
        else {
            block = NULL;
        }

        BOOL global = sCommand_option_item(command, "-global");

        char* regex = command->mArgsRuntime[1];
        char* destination = command->mArgsRuntime[2];

        int sub_count = 0;

        regex_t* reg;
        int r = get_onig_regex(&reg, command, regex);

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
                            err_msg("signal interrupt", runinfo->mSName, runinfo->mSLine, command->mArgs[0]);
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
                            err_msg("interrupt", runinfo->mSName, runinfo->mSLine, command->mArgs[0]);
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
                            err_msg("signal interrupt", runinfo->mSName, runinfo->mSLine, command->mArgs[0]);
                            runinfo->mRCode = RCODE_SIGNAL_INTERRUPT;
                            onig_region_free(region, 1);
                            onig_free(reg);
                            return FALSE;
                        }
                        if(!fd_write(nextout, string_c_str(sub_str), string_length(sub_str))) {
                            err_msg("signal interrupt", runinfo->mSName, runinfo->mSLine, command->mArgs[0]);
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
                                err_msg("signal interrupt", runinfo->mSName, runinfo->mSLine, command->mArgs[0]);
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
                            err_msg("signal interrupt", runinfo->mSName, runinfo->mSLine, command->mArgs[0]);
                            runinfo->mRCode = RCODE_SIGNAL_INTERRUPT;
                            onig_free(reg);
                            return FALSE;
                        }
                        break;
                    }
                }
            }
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

    return TRUE;
}

BOOL cmd_scan(sObject* nextin, sObject* nextout, sRunInfo* runinfo)
{
    sCommand* command = runinfo->mCommand;

    char* field = "\n";
    if(sCommand_option_item(command, "-Lw")) {
        field = "\r\n";
    }
    else if(sCommand_option_item(command, "-Lm")) {
        field = "\r";
    }
    else if(sCommand_option_item(command, "-Lu")) {
        field = "\n";
    }
    else if(sCommand_option_item(command, "-La")) {
        field = "\a";
    }
    
    if(runinfo->mFilter && command->mArgsNumRuntime == 2) {
        sObject* block;
        sObject* nextin2;

        if(command->mBlocksNum >= 1) {
            block = command->mBlocks[0];
            nextin2 = FD_NEW_STACK();
        }
        else {
            block = NULL;
        }

        int match_count = 0;
        char* regex = command->mArgsRuntime[1];

        regex_t* reg;
        int r = get_onig_regex(&reg, command, regex);

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
                                err_msg("signal interrupt", runinfo->mSName, runinfo->mSLine, command->mArgs[0]);
                                runinfo->mRCode = RCODE_SIGNAL_INTERRUPT;
                                onig_free(reg);
                                onig_region_free(region, 1);
                                return FALSE;
                            }
                            if(!fd_write(nextin2, field, strlen(field))) {
                                err_msg("signal interrupt", runinfo->mSName, runinfo->mSLine, command->mArgs[0]);
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
                                    err_msg("signal interrupt", runinfo->mSName, runinfo->mSLine, command->mArgs[0]);
                                    runinfo->mRCode = RCODE_SIGNAL_INTERRUPT;
                                    onig_free(reg);
                                    onig_region_free(region, 1);
                                    return FALSE;
                                }

                                if(i==region->num_regs-1) {
                                    if(!fd_write(nextin2, field, strlen(field))) {
                                        err_msg("signal interrupt", runinfo->mSName, runinfo->mSLine, command->mArgs[0]);
                                        runinfo->mRCode = RCODE_SIGNAL_INTERRUPT;
                                        onig_free(reg);
                                        onig_region_free(region, 1);
                                        return FALSE;
                                    }
                                }
                                else {
                                    if(!fd_write(nextin2, "\t", 1)) {
                                        err_msg("signal interrupt", runinfo->mSName, runinfo->mSLine, command->mArgs[0]);
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
                                err_msg("signal interrupt", runinfo->mSName, runinfo->mSLine, command->mArgs[0]);
                                runinfo->mRCode = RCODE_SIGNAL_INTERRUPT;
                                onig_free(reg);
                                onig_region_free(region, 1);
                                return FALSE;
                            }
                            if(!fd_write(nextout, field, strlen(field))) {
                                err_msg("signal interrupt", runinfo->mSName, runinfo->mSLine, command->mArgs[0]);
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
                                    err_msg("signal interrupt", runinfo->mSName, runinfo->mSLine, command->mArgs[0]);
                                    runinfo->mRCode = RCODE_SIGNAL_INTERRUPT;
                                    onig_free(reg);
                                    onig_region_free(region, 1);
                                    return FALSE;
                                }

                                if(i==region->num_regs-1) {
                                    if(!fd_write(nextout, field, strlen(field))) {
                                        err_msg("signal interrupt", runinfo->mSName, runinfo->mSLine, command->mArgs[0]);
                                        runinfo->mRCode = RCODE_SIGNAL_INTERRUPT;
                                        onig_free(reg);
                                        onig_region_free(region, 1);
                                        return FALSE;
                                    }
                                }
                                else {
                                    if(!fd_write(nextout, "\t", 1)) {
                                        err_msg("signal interrupt", runinfo->mSName, runinfo->mSLine, command->mArgs[0]);
                                        runinfo->mRCode = RCODE_SIGNAL_INTERRUPT;
                                        onig_free(reg);
                                        onig_region_free(region, 1);
                                        return FALSE;
                                    }
                                }
                            }
                        }
                    }

                    p = SFD(nextin).mBuf + region->end[0];
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
    }

    return TRUE;
}

BOOL cmd_split(sObject* nextin, sObject* nextout, sRunInfo* runinfo)
{
    sCommand* command = runinfo->mCommand;

    char* field = "\n";
    eLineField lf = gLineField;
    if(sCommand_option_item(command, "-Lw")) {
        lf = kCRLF;
        field = "\r\n";
    }
    else if(sCommand_option_item(command, "-Lm")) {
        lf = kCR;
        field = "\r";
    }
    else if(sCommand_option_item(command, "-Lu")) {
        lf = kLF;
        field = "\n";
    }
    else if(sCommand_option_item(command, "-La")) {
        lf = kBel;
        field = "\a";
    }

    enum eKanjiCode code = gKanjiCode;
    if(sCommand_option_item(command, "-byte")) {
        code = kByte;
    }
    else if(sCommand_option_item(command, "-utf8")) {
        code = kUtf8;
    }
    else if(sCommand_option_item(command, "-sjis")) {
        code = kSjis;
    }
    else if(sCommand_option_item(command, "-eucjp")) {
        code = kEucjp;
    }

    if(runinfo->mFilter) {
        char* regex;

        if(command->mArgsNumRuntime == 1) {
            regex = "\\s+";
        }
        else if(command->mArgsNumRuntime >= 2) {
            regex = command->mArgsRuntime[1];
        }

        regex_t* reg;
        int r = get_onig_regex(&reg, command, regex);

        if(r == ONIG_NORMAL) {
            char* target;
            char* p = target = SFD(nextin).mBuf;

            int split_count = 0;

            while(1) {
                OnigRegion* region = onig_region_new();

                int r2 = onig_search(reg, target
                   , target + strlen(target)
                   , p, p + strlen(p)
                   , region, ONIG_OPTION_NONE);
                   
                if(r2 == ONIG_MISMATCH) {
                    onig_region_free(region, 1);

                    if(*p) {
                        if(!fd_write(nextout, p, strlen(p))) {
                            err_msg("singal interrupt", runinfo->mSName, runinfo->mSLine, command->mArgs[0]);
                            runinfo->mRCode = RCODE_SIGNAL_INTERRUPT;
                            onig_free(reg);
                            return FALSE;
                        }
                    }

                    if(!fd_write(nextout, field, strlen(field))) {
                        err_msg("singal interrupt", runinfo->mSName, runinfo->mSLine, command->mArgs[0]);
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
                                    err_msg("singal interrupt", runinfo->mSName, runinfo->mSLine, command->mArgs[0]);
                                    runinfo->mRCode = RCODE_SIGNAL_INTERRUPT;
                                    onig_region_free(region, 1);
                                    onig_free(reg);
                                    return FALSE;
                                }

                                if(!fd_write(nextout, field, strlen(field))) {
                                    err_msg("singal interrupt", runinfo->mSName, runinfo->mSLine, command->mArgs[0]);
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
                                    err_msg("singal interrupt", runinfo->mSName, runinfo->mSLine, command->mArgs[0]);
                                    runinfo->mRCode = RCODE_SIGNAL_INTERRUPT;
                                    onig_region_free(region, 1);
                                    onig_free(reg);
                                    return FALSE;
                                }

                                if(!fd_write(nextout, field, strlen(field))) {
                                    err_msg("singal interrupt", runinfo->mSName, runinfo->mSLine, command->mArgs[0]);
                                    runinfo->mRCode = RCODE_SIGNAL_INTERRUPT;
                                    onig_region_free(region, 1);
                                    onig_free(reg);
                                    return FALSE;
                                }
                            }
                            else if(size == 0) {
                                if(!fd_write(nextout, field, strlen(field))) {
                                    err_msg("singal interrupt", runinfo->mSName, runinfo->mSLine, command->mArgs[0]);
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
                                err_msg("singal interrupt", runinfo->mSName, runinfo->mSLine, command->mArgs[0]);
                                runinfo->mRCode = RCODE_SIGNAL_INTERRUPT;
                                onig_region_free(region, 1);
                                onig_free(reg);
                                return FALSE;
                            }

                            if(!fd_write(nextout, field, strlen(field))) {
                                err_msg("singal interrupt", runinfo->mSName, runinfo->mSLine, command->mArgs[0]);
                                runinfo->mRCode = RCODE_SIGNAL_INTERRUPT;
                                onig_region_free(region, 1);
                                onig_free(reg);
                                return FALSE;
                            }
                        }
                        else if(size == 0) {
                            if(!fd_write(nextout, field, strlen(field))) {
                                err_msg("singal interrupt", runinfo->mSName, runinfo->mSLine, command->mArgs[0]);
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
                                    err_msg("singal interrupt", runinfo->mSName, runinfo->mSLine, command->mArgs[0]);
                                    runinfo->mRCode = RCODE_SIGNAL_INTERRUPT;
                                    onig_region_free(region, 1);
                                    onig_free(reg);
                                    return FALSE;
                                }

                                if(!fd_write(nextout, field, strlen(field))) {
                                    err_msg("singal interrupt", runinfo->mSName, runinfo->mSLine, command->mArgs[0]);
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

        onig_free(reg);
    }

    return TRUE;
}

BOOL cmd_add(sObject* nextin, sObject* nextout, sRunInfo* runinfo)
{
    sCommand* command = runinfo->mCommand;

    enum eKanjiCode code = gKanjiCode;
    if(sCommand_option_item(command, "-byte")) {
        code = kByte;
    }
    else if(sCommand_option_item(command, "-utf8")) {
        code = kUtf8;
    }
    else if(sCommand_option_item(command, "-sjis")) {
        code = kSjis;
    }
    else if(sCommand_option_item(command, "-eucjp")) {
        code = kEucjp;
    }

    if(runinfo->mFilter && command->mArgsNumRuntime == 2) {
        char* arg = command->mArgsRuntime[1];
        int number;
        char* argument;
        if(argument = sCommand_option_with_argument_item(command, "-number")) {
            number = atoi(argument);
        }
        else if(argument = sCommand_option_with_argument_item(command, "-index")) {
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
                err_msg("signal interrupt", runinfo->mSName, runinfo->mSLine, command->mArgs[0]);
                runinfo->mRCode = RCODE_SIGNAL_INTERRUPT;
                return FALSE;
            }
            if(!fd_write(nextout, arg, strlen(arg))) {
                err_msg("signal interrupt", runinfo->mSName, runinfo->mSLine, command->mArgs[0]);
                runinfo->mRCode = RCODE_SIGNAL_INTERRUPT;
                return FALSE;
            }
            if(!fd_write(nextout, SFD(nextin).mBuf + point, SFD(nextin).mBufLen - point)) {
                err_msg("signal interrupt", runinfo->mSName, runinfo->mSLine, command->mArgs[0]);
                runinfo->mRCode = RCODE_SIGNAL_INTERRUPT;
                return FALSE;
            }
        }
        else {
            if(!fd_write(nextout, SFD(nextin).mBuf, SFD(nextin).mBufLen)) {
                err_msg("signal interrupt", runinfo->mSName, runinfo->mSLine, command->mArgs[0]);
                runinfo->mRCode = RCODE_SIGNAL_INTERRUPT;
                return FALSE;
            }
            if(!fd_write(nextout, arg, strlen(arg))) {
                err_msg("signal interrupt", runinfo->mSName, runinfo->mSLine, command->mArgs[0]);
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
    sCommand* command = runinfo->mCommand;

    enum eKanjiCode code = gKanjiCode;
    if(sCommand_option_item(command, "-byte")) {
        code = kByte;
    }
    else if(sCommand_option_item(command, "-utf8")) {
        code = kUtf8;
    }
    else if(sCommand_option_item(command, "-sjis")) {
        code = kSjis;
    }
    else if(sCommand_option_item(command, "-eucjp")) {
        code = kEucjp;
    }

    
    if(runinfo->mFilter && command->mArgsNumRuntime == 2) {
        char* arg = command->mArgsRuntime[1];
        int index = atoi(arg);

        int number;
        char* argument;
        if(argument = sCommand_option_with_argument_item(command, "-number")) {
            number = atoi(argument);
        }
        else if(argument = sCommand_option_with_argument_item(command, "-index")) {
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
            err_msg("signal interrupt", runinfo->mSName, runinfo->mSLine, command->mArgs[0]);
            runinfo->mRCode = RCODE_SIGNAL_INTERRUPT;
            return FALSE;
        }
        if(index + number < len) {
            char* point = str_kanjipos2pointer(code, SFD(nextin).mBuf, index + number);
            if(!fd_write(nextout, point, strlen(point))) {
                err_msg("signal interrupt", runinfo->mSName, runinfo->mSLine, command->mArgs[0]);
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
    sCommand* command = runinfo->mCommand;

    enum eKanjiCode code = gKanjiCode;
    if(sCommand_option_item(command, "-byte")) {
        code = kByte;
    }
    else if(sCommand_option_item(command, "-utf8")) {
        code = kUtf8;
    }
    else if(sCommand_option_item(command, "-sjis")) {
        code = kSjis;
    }
    else if(sCommand_option_item(command, "-eucjp")) {
        code = kEucjp;
    }

    if(runinfo->mFilter && command->mArgsNumRuntime > 1 && command->mBlocksNum <= command->mArgsNumRuntime-1) {
        if(SFD(nextin).mBufLen == 0) {
            runinfo->mRCode = RCODE_NFUN_NULL_INPUT;
        }
        else {
            runinfo->mRCode = 0;
        }

        int i;
        for(i=1; i<command->mArgsNumRuntime; i++) {
            char* arg = command->mArgsRuntime[i];
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
                    err_msg("invalid range", runinfo->mSName, runinfo->mSLine, command->mArgs[0]);
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
                                err_msg("interrupt", runinfo->mSName, runinfo->mSLine, command->mArgs[0]);
                                runinfo->mRCode = RCODE_SIGNAL_INTERRUPT;
                                FREE(array);
                                return FALSE;
                            }

                            if(i-1 < command->mBlocksNum) {
                                int rcode = 0;
                                if(!run(command->mBlocks[i-1], nextin2, nextout, &rcode, runinfo->mCurrentObject, runinfo->mRunningObject)) {
                                    runinfo->mRCode = rcode;
                                    FREE(array);
                                    return FALSE;
                                }
                                runinfo->mRCode = rcode;
                            }
                            else {
                                if(!fd_write(nextout, SFD(nextin2).mBuf, SFD(nextin2).mBufLen)) 
                                {
                                    err_msg("interrupt", runinfo->mSName, runinfo->mSLine, command->mArgs[0]);
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
                                err_msg("interrupt", runinfo->mSName, runinfo->mSLine, command->mArgs[0]);
                                runinfo->mRCode = RCODE_SIGNAL_INTERRUPT;
                                FREE(array);
                                return FALSE;
                            }

                            if(i-1 < command->mBlocksNum) {
                                int rcode = 0;
                                if(!run(command->mBlocks[i-1], nextin2, nextout, &rcode, runinfo->mCurrentObject, runinfo->mRunningObject)) {
                                    runinfo->mRCode = rcode;
                                    FREE(array);
                                    return FALSE;
                                }
                                runinfo->mRCode = rcode;
                            }
                            else {
                                if(!fd_write(nextout, SFD(nextin2).mBuf, SFD(nextin2).mBufLen)) 
                                {
                                    err_msg("interrupt", runinfo->mSName, runinfo->mSLine, command->mArgs[0]);
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
                    err_msg("interrupt", runinfo->mSName, runinfo->mSLine, command->mArgs[0]);
                    runinfo->mRCode = RCODE_SIGNAL_INTERRUPT;
                    return FALSE;
                }

                if(i-1 < command->mBlocksNum) {
                    int rcode = 0;
                    if(!run(command->mBlocks[i-1], nextin2, nextout, &rcode, runinfo->mCurrentObject, runinfo->mRunningObject)) {
                        runinfo->mRCode = rcode;
                        return FALSE;
                    }
                    runinfo->mRCode = rcode;
                }
                else {
                    if(!fd_write(nextout, SFD(nextin2).mBuf, SFD(nextin2).mBufLen)) 
                    {
                        err_msg("interrupt", runinfo->mSName, runinfo->mSLine, command->mArgs[0]);
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

