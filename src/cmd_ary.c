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

#if defined(HAVE_CURSES_H)
#include <curses.h>
#elif defined(HAVE_NCURSES_H)
#include <ncurses.h>
#elif defined(HAVE_NCURSES_NCURSES_H)
#include <ncurses/ncurses.h>
#endif

#include "xyzsh/xyzsh.h"

BOOL cmd_each(sObject* nextin, sObject* nextout, sRunInfo* runinfo)
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

    if(runinfo->mBlocksNum == 1 && runinfo->mFilter) {
        fd_split(nextin, lf);

        char* argument;
        if(argument = sRunInfo_option_with_argument(runinfo, "-number")) {
            if(SFD(nextin).mBufLen == 0) {
                runinfo->mRCode = RCODE_NFUN_NULL_INPUT;
            }
            else {
                runinfo->mRCode = 0;
            }

            int number = atoi(argument);

            sObject* nextin2 = FD_NEW_STACK();

            int i;
            for(i=0; i<vector_count(SFD(nextin).mLines); i+=number) {
                fd_clear(nextin2);

                int j;
                for(j=0; j<number && i+j<vector_count(SFD(nextin).mLines); j++)
                {
                    char* str = vector_item(SFD(nextin).mLines, i+j);
                    if(!fd_write(nextin2, str, strlen(str))) {
                        err_msg("interrupt", runinfo->mSName, runinfo->mSLine, runinfo->mArgs[0]);
                        runinfo->mRCode = RCODE_SIGNAL_INTERRUPT;
                        return FALSE;
                    }
                }

                int rcode = 0;
                if(!run(runinfo->mBlocks[0], nextin2, nextout, &rcode, runinfo->mCurrentObject, runinfo->mRunningObject)) {
                    if(rcode == RCODE_BREAK) {
                        runinfo->mRCode = rcode = 0;
                        break;
                    }
                    else {
                        runinfo->mRCode = rcode;
                        return FALSE;
                    }
                }

                runinfo->mRCode = rcode;
            }
        }
        else {
            if(SFD(nextin).mBufLen == 0) {
                runinfo->mRCode = RCODE_NFUN_NULL_INPUT;
            }
            else {
                runinfo->mRCode = 0;
            }

            sObject* nextin2 = FD_NEW_STACK();

            int i;
            for(i=0; i<vector_count(SFD(nextin).mLines); i++) {
                fd_clear(nextin2);

                char* str = vector_item(SFD(nextin).mLines, i);

                if(!fd_write(nextin2, str, strlen(str))) {
                    err_msg("interrupt", runinfo->mSName, runinfo->mSLine, runinfo->mArgs[0]);
                    runinfo->mRCode = RCODE_SIGNAL_INTERRUPT;
                    return FALSE;
                }

                int rcode = 0;
                if(!run(runinfo->mBlocks[0], nextin2, nextout, &rcode, runinfo->mCurrentObject, runinfo->mRunningObject)) {
                    if(rcode == RCODE_BREAK) {
                        runinfo->mRCode = rcode = 0;
                        break;
                    }
                    else {
                        runinfo->mRCode = rcode;
                        return FALSE;
                    }
                }

                runinfo->mRCode = rcode;
            }
        }
    }

    return TRUE;
}

BOOL cmd_join(sObject* nextin, sObject* nextout, sRunInfo* runinfo)
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
        fd_split(nextin, lf);

        char* field;
        if(runinfo->mArgsNumRuntime >= 2) {
            field = runinfo->mArgsRuntime[1];
        }
        else {
            field = " ";
        }

        if(vector_count(SFD(nextin).mLines) > 0) {
            int i;
            for(i=0; i<vector_count(SFD(nextin).mLines)-1; i++) {
                sObject* line = STRING_NEW_STACK(vector_item(SFD(nextin).mLines, i));
                string_chomp(line);
                if(!fd_write(nextout, string_c_str(line), string_length(line))) {
                    err_msg("signal interrupt", runinfo->mSName, runinfo->mSLine, runinfo->mArgs[0]);
                    runinfo->mRCode = RCODE_SIGNAL_INTERRUPT;
                    return FALSE;
                }
                if(!fd_write(nextout, field, strlen(field))) {
                    err_msg("signal interrupt", runinfo->mSName, runinfo->mSLine, runinfo->mArgs[0]);
                    runinfo->mRCode = RCODE_SIGNAL_INTERRUPT;
                    return FALSE;
                }
            }

            sObject* line = STRING_NEW_STACK(vector_item(SFD(nextin).mLines, i));
            string_chomp(line);
            if(!fd_write(nextout, string_c_str(line), string_length(line))) {
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

        if(SFD(nextin).mBufLen == 0) {
            runinfo->mRCode = RCODE_NFUN_NULL_INPUT;
        }
        else {
            runinfo->mRCode = 0;
        }
    }

    return TRUE;
}

BOOL cmd_lines(sObject* nextin, sObject* nextout, sRunInfo* runinfo)
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

    if(runinfo->mArgsNumRuntime > 1 && runinfo->mBlocksNum <= runinfo->mArgsNumRuntime-1 && runinfo->mFilter) {
        if(SFD(nextin).mBufLen == 0) {
            runinfo->mRCode = RCODE_NFUN_NULL_INPUT;
        }
        else {
            runinfo->mRCode = 0;
        }

        fd_split(nextin, lf);

        if(vector_count(SFD(nextin).mLines) > 0) {
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

                    int first = atoi(buf);
                    int second = atoi(buf2);

                    if(first < 0) {
                        first += vector_count(SFD(nextin).mLines);
                        if(first < 0) first = 0;
                    }
                    if(second < 0) {
                        second += vector_count(SFD(nextin).mLines);
                        if(second < 0) second = 0;
                    }
                    if(first >= vector_count(SFD(nextin).mLines)) {
                        first = vector_count(SFD(nextin).mLines) -1;
                        if(first < 0) first = 0;
                    }
                    if(second >= vector_count(SFD(nextin).mLines)) {
                        second = vector_count(SFD(nextin).mLines) -1;
                        if(second < 0) second = 0;
                    }

                    if(first < second) {
                        sObject* nextin2 = FD_NEW_STACK();

                        int j;
                        for(j=first; j<=second; j++) {
                            fd_clear(nextin2);

                            char* str = vector_item(SFD(nextin).mLines, j);
                            if(!fd_write(nextin2,str,strlen(str))) {
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
                    else {
                        sObject* nextin2 = FD_NEW_STACK();

                        int j;
                        for(j=first; j>=second; j--) {
                            fd_clear(nextin2);

                            char* str = vector_item(SFD(nextin).mLines, j);
                            if(!fd_write(nextin2, str, strlen(str))) {
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
                else {
                    int num = atoi(arg);

                    if(num < 0) {
                        num += vector_count(SFD(nextin).mLines);
                        if(num < 0) num = 0;
                    }
                    if(num >= vector_count(SFD(nextin).mLines)) {
                        num = vector_count(SFD(nextin).mLines) -1;
                        if(num < 0) num = 0;
                    }

                    sObject* nextin2 = FD_NEW_STACK();

                    char* str = vector_item(SFD(nextin).mLines, num);
                    if(!fd_write(nextin2, str, strlen(str))){
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
    }

    return TRUE;
}

typedef int (*sort_if_cancelable)(void* left, void* right, sRunInfo* runinfo, sObject* nextin, sObject* nextout);

static int sort_fun(void* left, void* right, sRunInfo* runinfo, sObject* nextin2, sObject* nextout)
{
    char* left2 = left;
    char* right2 = right;

    fd_clear(nextin2);
    if(!fd_write(nextin2, left2, strlen(left2))) {
        return -1;
    }
    if(!fd_write(nextin2, right2, strlen(right2))) {
        return -1;
    }

    int rcode = 0;
    if(!run(runinfo->mBlocks[0], nextin2, nextout, &rcode, runinfo->mCurrentObject, runinfo->mRunningObject)) {
        runinfo->mRCode = rcode;
        return -1;
    }

    if(rcode < 0) {
        return -1;
    }

    return rcode == 0;
}

static BOOL quick_sort(sObject* self, int left, int right, sort_if_cancelable fun, sRunInfo* runinfo, sObject* nextin, sObject* nextout)
{
    int i;
    int j;
    void* center_item;

    if(left < right) {
        center_item = SVECTOR(self).mTable[(left+right) / 2];

        i = left;
        j = right;

        do { 
            while(1) {
                int ret = fun(SVECTOR(self).mTable[i], center_item, runinfo, nextin, nextout);
                if(ret < 0) return FALSE;
                if(SVECTOR(self).mTable[i]==center_item || !ret)
                {
                    break;
                }
                i++;
            }
                     
            while(1) {
                int ret = fun(center_item, SVECTOR(self).mTable[j], runinfo, nextin, nextout);
                if(ret < 0) return FALSE;
                if(center_item==SVECTOR(self).mTable[j] || !ret)
                {
                    break;
                }
                j--;
            }

            if(i <= j) {
                void* tmp = SVECTOR(self).mTable[i]; // swap
                SVECTOR(self).mTable[i] = SVECTOR(self).mTable[j];
                SVECTOR(self).mTable[j] = tmp;

                i++;
                j--;
            }
        } while(i <= j);

        if(!quick_sort(self, left, j, fun, runinfo, nextin, nextout)) {
            return FALSE;
        }
        if(!quick_sort(self, i, right, fun, runinfo, nextin, nextout)) {
            return FALSE;
        }
    }

    return TRUE;
}

BOOL vector_sort_cancelable(sObject* self, sort_if_cancelable fun, sRunInfo* runinfo, sObject* nextin, sObject* nextout)
{
    return quick_sort(self, 0, SVECTOR(self).mCount-1, fun, runinfo, nextin, nextout);
}

typedef int (*sort_if_cancelable_shuffle)(int left, int right, int* shuffle_num);

static int sort_fun_shuffle(int left, int right, int* shuffle_num)
{
    return shuffle_num[left] < shuffle_num[right];
}

static BOOL quick_sort_shuffle(sObject* self, int left, int right, sort_if_cancelable_shuffle fun, int* shuffle_num)
{
    int i;
    int j;
    void* center_item;

    if(left < right) {
        center_item = SVECTOR(self).mTable[(left+right) / 2];

        i = left;
        j = right;

        do { 
            while(1) {
                int ret = fun(i, (left+right) / 2, shuffle_num);
                if(ret < 0) return FALSE;
                if(SVECTOR(self).mTable[i]==center_item || !ret)
                {
                    break;
                }
                i++;
            }
                     
            while(1) {
                int ret = fun((left+right) / 2, j, shuffle_num);
                if(ret < 0) return FALSE;
                if(center_item==SVECTOR(self).mTable[j] || !ret)
                {
                    break;
                }
                j--;
            }

            if(i <= j) {
                void* tmp = SVECTOR(self).mTable[i]; // swap
                SVECTOR(self).mTable[i] = SVECTOR(self).mTable[j];
                SVECTOR(self).mTable[j] = tmp;

                i++;
                j--;
            }
        } while(i <= j);

        if(!quick_sort_shuffle(self, left, j, fun, shuffle_num)) {
            return FALSE;
        }
        if(!quick_sort_shuffle(self, i, right, fun, shuffle_num)) {
            return FALSE;
        }
    }

    return TRUE;
}

BOOL vector_sort_cancelable_shuffle(sObject* self, sort_if_cancelable_shuffle fun, int* shuffle_num)
{
    return quick_sort_shuffle(self, 0, SVECTOR(self).mCount-1, fun, shuffle_num);
}

BOOL cmd_sort(sObject* nextin, sObject* nextout, sRunInfo* runinfo)
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
        if(sRunInfo_option(runinfo, "-shuffle")) {
            fd_split(nextin, lf);

            if(vector_count(SFD(nextin).mLines) > 0) {
                int* shuffle_num = MALLOC(sizeof(int)*vector_count(SFD(nextin).mLines));
                int i;
                for(i=0;i<vector_count(SFD(nextin).mLines); i++) {
                    shuffle_num[i] = rand() % 2560;
                }

                sObject* nextin2 = FD_NEW_STACK();
                if(!vector_sort_cancelable_shuffle(SFD(nextin).mLines, sort_fun_shuffle, shuffle_num)) {
                    FREE(shuffle_num);
                    return FALSE;
                }

                FREE(shuffle_num);

                for(i=0; i<vector_count(SFD(nextin).mLines); i++) {
                    char* line = vector_item(SFD(nextin).mLines, i);
                    if(!fd_write(nextout, line, strlen(line))) {
                        err_msg("interrupt", runinfo->mSName, runinfo->mSLine, runinfo->mArgs[0]);
                        runinfo->mRCode = RCODE_SIGNAL_INTERRUPT;
                        return FALSE;
                    }
                }
            }

            if(SFD(nextin).mBufLen == 0) {
                runinfo->mRCode = RCODE_NFUN_NULL_INPUT;
            }
            else {
                runinfo->mRCode = 0;
            }
        } 
        else if(runinfo->mBlocksNum == 1) {
            fd_split(nextin, lf);

            if(vector_count(SFD(nextin).mLines) > 0) {
                sObject* nextin2 = FD_NEW_STACK();
                if(!vector_sort_cancelable(SFD(nextin).mLines, sort_fun, runinfo, nextin2, nextout)) {
                    return FALSE;
                }

                int i;
                for(i=0; i<vector_count(SFD(nextin).mLines); i++) {
                    char* line = vector_item(SFD(nextin).mLines, i);
                    if(!fd_write(nextout, line, strlen(line))) {
                        err_msg("interrupt", runinfo->mSName, runinfo->mSLine, runinfo->mArgs[0]);
                        runinfo->mRCode = RCODE_SIGNAL_INTERRUPT;
                        return FALSE;
                    }
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
