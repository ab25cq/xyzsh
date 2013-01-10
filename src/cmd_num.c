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

BOOL cmd_plusplus(sObject* nextin, sObject* nextout, sRunInfo* runinfo)
{
    sCommand* command = runinfo->mCommand;

    if(!runinfo->mFilter && command->mArgsNumRuntime == 2) {
        sObject* current = runinfo->mCurrentObject;
        sObject* var = access_object(command->mArgsRuntime[1], &current, runinfo->mRunningObject);

        if(var && TYPE(var) == T_STRING) {
            int num = atoi(string_c_str(var));
            num++;
            char buf[128];
            snprintf(buf, 128, "%d", num);
            string_put(var, buf);

            runinfo->mRCode = 0;
        }
        else {
            err_msg("not found variable", runinfo->mSName, runinfo->mSLine, command->mArgs[0]);
            return FALSE;
        }
    }

    return TRUE;
}

BOOL cmd_minusminus(sObject* nextin, sObject* nextout, sRunInfo* runinfo)
{
    sCommand* command = runinfo->mCommand;

    if(!runinfo->mFilter && command->mArgsNumRuntime == 2) {
        sObject* current = runinfo->mCurrentObject;
        sObject* var = access_object(command->mArgsRuntime[1], &current, runinfo->mRunningObject);

        if(var && TYPE(var) == T_STRING) {
            int num = atoi(string_c_str(var));
            num--;
            char buf[128];
            snprintf(buf, 128, "%d", num);
            string_put(var, buf);

            runinfo->mRCode = 0;
        }
        else {
            err_msg("not found variable", runinfo->mSName, runinfo->mSLine, command->mArgs[0]);
            return FALSE;
        }
    }

    return TRUE;
}

BOOL cmd_plus(sObject* nextin, sObject* nextout, sRunInfo* runinfo)
{
    sCommand* command = runinfo->mCommand;

    /// output
    if(runinfo->mFilter && command->mArgsNumRuntime == 2) {
        char* arg = SFD(nextin).mBuf;
        int num = atoi(arg);

        num += atoi(command->mArgsRuntime[1]);

        char buf[128];
        int size = snprintf(buf, 128, "%d\n", num);

        if(!fd_write(nextout, buf, size)) {
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

BOOL cmd_minus(sObject* nextin, sObject* nextout, sRunInfo* runinfo)
{
    sCommand* command = runinfo->mCommand;

    /// output
    if(runinfo->mFilter && command->mArgsNumRuntime == 2) {
        char* arg = SFD(nextin).mBuf;
        int num = atoi(arg);

        num -= atoi(command->mArgsRuntime[1]);

        char buf[128];
        int size = snprintf(buf, 128, "%d\n", num);

        if(!fd_write(nextout, buf, size)) {
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

BOOL cmd_mult(sObject* nextin, sObject* nextout, sRunInfo* runinfo)
{
    sCommand* command = runinfo->mCommand;

    /// output
    if(runinfo->mFilter && command->mArgsNumRuntime == 2) {
        char* arg = SFD(nextin).mBuf;
        int num = atoi(arg);

        num *= atoi(command->mArgsRuntime[1]);

        char buf[128];
        int size = snprintf(buf, 128, "%d\n", num);

        if(!fd_write(nextout, buf, size)) {
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

BOOL cmd_div(sObject* nextin, sObject* nextout, sRunInfo* runinfo)
{
    sCommand* command = runinfo->mCommand;

    /// output
    if(runinfo->mFilter && command->mArgsNumRuntime == 2) {
        char* arg = SFD(nextin).mBuf;
        int num = atoi(arg);

        int num2 = atoi(command->mArgsRuntime[1]);

        if(num2 == 0) {
            err_msg("zero div", runinfo->mSName, runinfo->mSLine, command->mArgs[0]);
            return FALSE;
        }

        num /= num2;

        char buf[128];
        int size = snprintf(buf, 128, "%d\n", num);

        if(!fd_write(nextout, buf, size)) {
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

BOOL cmd_mod(sObject* nextin, sObject* nextout, sRunInfo* runinfo)
{
    sCommand* command = runinfo->mCommand;

    /// output
    if(runinfo->mFilter && command->mArgsNumRuntime == 2) {
        char* arg = SFD(nextin).mBuf;
        int num = atoi(arg);

        num = num % atoi(command->mArgsRuntime[1]);

        char buf[128];
        int size = snprintf(buf, 128, "%d\n", num);

        if(!fd_write(nextout, buf, size)) {
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

BOOL cmd_pow(sObject* nextin, sObject* nextout, sRunInfo* runinfo)
{
    sCommand* command = runinfo->mCommand;

    /// output
    if(runinfo->mFilter && command->mArgsNumRuntime == 2) {
        char* arg = SFD(nextin).mBuf;
        int num = atoi(arg);

        num = pow(num, atoi(command->mArgsRuntime[1]));

        char buf[128];
        int size = snprintf(buf, 128, "%d\n", num);

        if(!fd_write(nextout, buf, size)) {
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

BOOL cmd_abs(sObject* nextin, sObject* nextout, sRunInfo* runinfo)
{
    sCommand* command = runinfo->mCommand;

    /// output
    if(runinfo->mFilter) {
        char* arg = SFD(nextin).mBuf;
        int num = abs(atoi(arg));

        char buf[128];
        int size = snprintf(buf, 128, "%d\n", num);

        if(!fd_write(nextout, buf, size)) {
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

