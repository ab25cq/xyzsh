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

#include "xyzsh.h"

BOOL cmd_while(sObject* nextin, sObject* nextout, sRunInfo* runinfo)
{
    if(runinfo->mBlocksNum == 2) {
        runinfo->mRCode = 0;

        while(1) {
            /// condition ///
            int rcode;
            if(!run(runinfo->mBlocks[0], nextin, nextout, &rcode, runinfo->mCurrentObject, runinfo->mRunningObject)) {
                runinfo->mRCode = rcode;
                return FALSE;
            }

            if(rcode != 0) {
                break;
            }

            /// inner loop ///
            if(!run(runinfo->mBlocks[1], nextin, nextout, &rcode, runinfo->mCurrentObject, runinfo->mRunningObject)) {
                if(rcode == RCODE_BREAK) {
                    rcode = 0;
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

    return TRUE;
}

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
        fd_split(nextin, lf, FALSE, FALSE, FALSE);

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
                        err_msg("interrupt", runinfo->mSName, runinfo->mSLine);
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
                    err_msg("interrupt", runinfo->mSName, runinfo->mSLine);
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

BOOL cmd_break(sObject* nextin, sObject* nextout, sRunInfo* runinfo)
{
    runinfo->mRCode = RCODE_BREAK;
    return FALSE;
}

BOOL cmd_for(sObject* nextin, sObject* nextout, sRunInfo* runinfo)
{
    if(runinfo->mBlocksNum == 1 && runinfo->mArgsNumRuntime >= 4) {
        char* arg_name = runinfo->mArgsRuntime[1];
        char* in = runinfo->mArgsRuntime[2];

        if(strcmp(in, "in") != 0) {
            err_msg("for command needs 'in' word at 3th argument.", runinfo->mSName, runinfo->mSLine);
            return FALSE;
        }

        runinfo->mRCode = 0;

        sObject* object = SFUN(runinfo->mRunningObject).mLocalObjects;

        int i;
        for(i=3; i<runinfo->mArgsNumRuntime; i++) {
            /// condition ///
            uobject_put(object, arg_name, STRING_NEW_GC(runinfo->mArgsRuntime[i], TRUE));

            /// inner loop ///
            int rcode = 0;
            if(!run(runinfo->mBlocks[0], nextin, nextout, &rcode, runinfo->mCurrentObject, runinfo->mRunningObject)) {
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

    return TRUE;
}

BOOL cmd_times(sObject* nextin, sObject* nextout, sRunInfo* runinfo)
{
    if(runinfo->mArgsNumRuntime == 2 && runinfo->mBlocksNum == 1) {
        int number = atoi(runinfo->mArgsRuntime[1]);
        int i;
        for(i=0; i<number; i++) {
            int rcode = 0;
            if(!run(runinfo->mBlocks[0], nextin, nextout, &rcode, runinfo->mCurrentObject, runinfo->mRunningObject)) {
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

    return TRUE;
}
