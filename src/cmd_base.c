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

BOOL cmd_exit(sObject* nextin, sObject* nextout, sRunInfo* runinfo)
{
    if(sRunInfo_option(runinfo, "-force")) {
        runinfo->mRCode = RCODE_EXIT;
    }
    else {
        if(vector_count(gJobs) > 0) {
            err_msg("jobs exist", runinfo->mSName, runinfo->mSLine, runinfo->mArgs[0]);
            return FALSE;
        }

        runinfo->mRCode = RCODE_EXIT;
    }

    return FALSE;
}

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

BOOL cmd_for(sObject* nextin, sObject* nextout, sRunInfo* runinfo)
{
    if(runinfo->mBlocksNum == 1 && runinfo->mArgsNumRuntime >= 4) {
        char* arg_name = runinfo->mArgsRuntime[1];
        char* in = runinfo->mArgsRuntime[2];

        if(strcmp(in, "in") != 0) {
            err_msg("for command needs 'in' word at 3th argument.", runinfo->mSName, runinfo->mSLine, "for");
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

BOOL cmd_break(sObject* nextin, sObject* nextout, sRunInfo* runinfo)
{
    runinfo->mRCode = RCODE_BREAK;
    return FALSE;
}

BOOL cmd_true(sObject* nextin, sObject* nextout, sRunInfo* runinfo)
{
    runinfo->mRCode = 0;
    return TRUE;
}

BOOL cmd_false(sObject* nextin, sObject* nextout, sRunInfo* runinfo)
{
    runinfo->mRCode = RCODE_NFUN_FALSE;
    return TRUE;
}

BOOL cmd_if(sObject* nextin, sObject* nextout, sRunInfo* runinfo)
{
    if(runinfo->mBlocksNum > 1) {
        if(runinfo->mBlocksNum % 2 == 0) {
            runinfo->mRCode = 0;

            int i;
            for(i=0; i<runinfo->mBlocksNum; i+=2) {
                /// a condition ///
                int rcode = 0;
                if(!run(runinfo->mBlocks[i], nextin, nextout, &rcode, runinfo->mCurrentObject, runinfo->mRunningObject)) {
                    runinfo->mRCode = rcode;
                    return FALSE;
                }

                /// a statment ///
                if(rcode == 0) {
                    int rcode = 0;
                    if(!run(runinfo->mBlocks[i+1], nextin, nextout, &rcode, runinfo->mCurrentObject, runinfo->mRunningObject)) {
                        runinfo->mRCode = rcode;
                        return FALSE;
                    }
                    runinfo->mRCode = rcode;
                    break;
                }
            }
        }
        else {
            runinfo->mRCode = 0;

            BOOL run_else = TRUE;

            int i;
            for(i=0; i<runinfo->mBlocksNum-1; i+=2) {
                /// a condition ///
                int rcode = 0;
                if(!run(runinfo->mBlocks[i], nextin, nextout,  &rcode, runinfo->mCurrentObject, runinfo->mRunningObject)) {
                    runinfo->mRCode = rcode;
                    return FALSE;
                }

                /// a statment ///
                if(rcode == 0) {
                    run_else = FALSE;

                    int rcode = 0;
                    if(!run(runinfo->mBlocks[i+1], nextin, nextout, &rcode, runinfo->mCurrentObject, runinfo->mRunningObject)) {
                        runinfo->mRCode = rcode;
                        return FALSE;
                    }
                    runinfo->mRCode = rcode;
                    break;
                }
            }

            /// else statment ///
            if(run_else) {
                int rcode = 0;
                if(!run(runinfo->mBlocks[runinfo->mBlocksNum-1], nextin, nextout, &rcode, runinfo->mCurrentObject, runinfo->mRunningObject)) {
                    runinfo->mRCode = rcode;
                    return FALSE;
                }

                runinfo->mRCode = rcode;
            }
        }
    }

    return TRUE;
}

BOOL cmd_return(sObject* nextin, sObject* nextout, sRunInfo* runinfo)
{
    if(runinfo->mArgsNumRuntime >= 2) {
        uchar code = atoi(runinfo->mArgsRuntime[1]);
        runinfo->mRCode = RCODE_RETURN | code;

        return FALSE;
    }
    else {
        runinfo->mRCode = RCODE_RETURN;

        return FALSE;
    }
}

BOOL cmd_subshell(sObject* nextin, sObject* nextout, sRunInfo* runinfo)
{
    if(runinfo->mBlocksNum == 1) {
        int rcode = 0;
        if(!run(runinfo->mBlocks[0], nextin, nextout, &rcode, runinfo->mCurrentObject, runinfo->mRunningObject)) {
            runinfo->mRCode = rcode;
            return FALSE;
        }
        runinfo->mRCode = rcode;
    }
    
    return TRUE;
}

BOOL cmd_print(sObject* nextin, sObject* nextout, sRunInfo* runinfo)
{
    sObject* out;
    if(sRunInfo_option(runinfo, "-out-to-error")) {
        out = gStderr;
    }
    else {
        out = nextout;
    }

    if(runinfo->mArgsNumRuntime > 1) {
        int i;
        for(i=1; i<runinfo->mArgsNumRuntime-1; i++) {
            if(!fd_write(out, runinfo->mArgsRuntime[i], strlen(runinfo->mArgsRuntime[i]))) {
                err_msg("signal interrupt", runinfo->mSName, runinfo->mSLine, runinfo->mArgs[0]);
                runinfo->mRCode = RCODE_SIGNAL_INTERRUPT;
                return FALSE;
            }
            if(!fd_write(out, " ", 1)) {
                err_msg("signal interrupt", runinfo->mSName, runinfo->mSLine, runinfo->mArgs[0]);
                runinfo->mRCode = RCODE_SIGNAL_INTERRUPT;
                return FALSE;
            }
        }
        if(!fd_write(out, runinfo->mArgsRuntime[i], strlen(runinfo->mArgsRuntime[i]))) {
            err_msg("signal interrupt", runinfo->mSName, runinfo->mSLine, runinfo->mArgs[0]);
            runinfo->mRCode = RCODE_SIGNAL_INTERRUPT;
            return FALSE;
        }
        runinfo->mRCode = 0;
    }
    else if(runinfo->mFilter) {
        if(sRunInfo_option(runinfo, "-read-from-error")) {
            if(!fd_write(out, SFD(gStderr).mBuf, SFD(gStderr).mBufLen)) {
                err_msg("signal interrupt", runinfo->mSName, runinfo->mSLine, runinfo->mArgs[0]);
                runinfo->mRCode = RCODE_SIGNAL_INTERRUPT;
                return FALSE;
            }
            fd_clear(gStderr);
        }
        else {
            if(!fd_write(out, SFD(nextin).mBuf, SFD(nextin).mBufLen)) {
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

BOOL cmd_try(sObject* nextin, sObject* nextout, sRunInfo* runinfo)
{
    if(runinfo->mBlocksNum == 2) {
        int rcode = 0;
        if(!run(runinfo->mBlocks[0], nextin, nextout, &rcode, runinfo->mCurrentObject, runinfo->mRunningObject)) {
            if(rcode == RCODE_NFUN_FALSE || rcode == RCODE_BREAK || rcode & RCODE_RETURN || rcode == RCODE_EXIT) {
                runinfo->mRCode = rcode;
                return FALSE;
            }
            else {
                if(!run(runinfo->mBlocks[1], nextin, nextout, &rcode, runinfo->mCurrentObject, runinfo->mRunningObject)) {
                    runinfo->mRCode = rcode;
                    return FALSE;
                }
                else {
                    runinfo->mRCode = rcode;
                    return TRUE;
                }
            }
        }
        else {
            runinfo->mRCode = rcode;
            return TRUE;
        }
    }

    return TRUE;
}

BOOL cmd_errmsg(sObject* nextin, sObject* nextout, sRunInfo* runinfo)
{
    if(!fd_write(nextout, string_c_str(gErrMsg), string_length(gErrMsg))) {
        err_msg("signal interrupt", runinfo->mSName, runinfo->mSLine, runinfo->mArgs[0]);
        runinfo->mRCode = RCODE_SIGNAL_INTERRUPT;
        return FALSE;
    }
    runinfo->mRCode = 0;

    return TRUE;
}

BOOL cmd_raise(sObject* nextin, sObject* nextout, sRunInfo* runinfo)
{
    if(runinfo->mArgsNumRuntime == 2) {
        err_msg(runinfo->mArgsRuntime[1], runinfo->mSName, runinfo->mSLine, runinfo->mArgs[0]);
        return FALSE;
    }

    return TRUE;
}

BOOL cmd_sweep(sObject* nextin, sObject* nextout, sRunInfo* runinfo)
{
    if(runinfo->mArgsNumRuntime == 1) {
        int num = gc_sweep();

        char buf[BUFSIZ];
        int size = snprintf(buf, BUFSIZ, "%d objects deleted\n", num);

        if(!fd_write(nextout, buf, size)) {
            err_msg("signal interrupt", runinfo->mSName, runinfo->mSLine, runinfo->mArgs[0]);
            runinfo->mRCode = RCODE_SIGNAL_INTERRUPT;
            return FALSE;
        }
        runinfo->mRCode = 0;
    }
    else {
        int i;
        for(i=1; i<runinfo->mArgsNumRuntime; i++) {
            char* var_name = runinfo->mArgsRuntime[i];
            sObject* current_object = runinfo->mCurrentObject;

            sObject* item = uobject_item(current_object, var_name);

            if(!IS_USER_OBJECT(item)) {
                err_msg("it's not user object. can't delete", runinfo->mSName, runinfo->mSLine, runinfo->mArgs[0]);
                return FALSE;
            }

            if(item) {
                if(item != gCurrentObject || strcmp(var_name, SUOBJECT(item).mName) != 0) {
                    if(uobject_erase(current_object, var_name)) {
                        runinfo->mRCode = 0;
                    }
                }
            }
        }
    }

    return TRUE;
}

BOOL cmd_load(sObject* nextin, sObject* nextout, sRunInfo* runinfo)
{
    if(sRunInfo_option(runinfo, "-dynamic-library")) {
        if(!load_so_file(runinfo->mArgsRuntime[1], nextin, nextout, runinfo))
        {
            return FALSE;
        }

        runinfo->mRCode = 0;
    }
    else if(runinfo->mArgsNumRuntime >= 2) {
        runinfo->mRCode = 0;
        if(!load_file(runinfo->mArgsRuntime[1], nextin, nextout, runinfo, runinfo->mArgsRuntime + 2, runinfo->mArgsNumRuntime -2))
        {
            return FALSE;
        }
    }

    return TRUE;
}

BOOL cmd_eval(sObject* nextin, sObject* nextout, sRunInfo* runinfo)
{
    if(runinfo->mBlocksNum > 0) {
        int rcode;
        if(!run(runinfo->mBlocks[0], nextin, nextout, &rcode, runinfo->mCurrentObject, runinfo->mRunningObject)) {
            err_msg_adding("run time error", runinfo->mSName, runinfo->mSLine, runinfo->mArgs[0]);
            runinfo->mRCode = rcode;
            return FALSE;
        }
        runinfo->mRCode = rcode;
    }
    else if(runinfo->mArgsNumRuntime >= 2) {
        char* buf = runinfo->mArgsRuntime[1];

        stack_start_stack();
        int rcode = 0;
        sObject* block = BLOCK_NEW_STACK();
        int sline = 1;
        char buf2[BUFSIZ];
        snprintf(buf2, BUFSIZ, "%s %d: eval", runinfo->mSName, runinfo->mSLine);
        if(parse(buf, buf2, &sline, block, NULL)) {
            if(!run(block, nextin, nextout, &rcode, runinfo->mCurrentObject, runinfo->mRunningObject)) {
                err_msg_adding("run time error", runinfo->mSName, runinfo->mSLine, runinfo->mArgs[0]);
                runinfo->mRCode = rcode;
                stack_end_stack();
                return FALSE;
            }
        }
        else {
            err_msg_adding("parser error\n", runinfo->mSName, runinfo->mSLine, runinfo->mArgs[0]);
            stack_end_stack();
            return FALSE;
        }

        stack_end_stack();
        runinfo->mRCode = rcode;
    }
    else if(runinfo->mFilter) {
        char* buf = SFD(nextin).mBuf;

        stack_start_stack();
        int rcode = 0;
        sObject* block = BLOCK_NEW_STACK();
        int sline = 1;
        char buf2[BUFSIZ];
        snprintf(buf2, BUFSIZ, "%s %d: eval", runinfo->mSName, runinfo->mSLine);

        if(parse(buf, buf2, &sline, block, NULL)) {
            sObject* nextin2 = FD_NEW_STACK();
            if(!run(block, nextin2, nextout, &rcode, runinfo->mCurrentObject, runinfo->mRunningObject)) {
                err_msg_adding("run time error", runinfo->mSName, runinfo->mSLine, runinfo->mArgs[0]);
                runinfo->mRCode = rcode;
                stack_end_stack();
                return FALSE;
            }
        }
        else {
            err_msg_adding("parser error\n", runinfo->mSName, runinfo->mSLine, runinfo->mArgs[0]);
            stack_end_stack();
            return FALSE;
        }

        stack_end_stack();

        if(SFD(nextin).mBufLen == 0) {
            runinfo->mRCode = RCODE_NFUN_NULL_INPUT;
        }
        else {
            runinfo->mRCode = rcode;
        }
    }

    return TRUE;
}

BOOL cmd_msleep(sObject* nextin, sObject* nextout, sRunInfo* runinfo)
{
    if(runinfo->mArgsNumRuntime == 2) {
        char c [] = {'.', 'o', 'O'};
        int n = atoi(runinfo->mArgsRuntime[1]);
        int i;
        for(i=0; i < n*5; i++) {
            char buf[32];
            int size = snprintf(buf, 32, "%c\033[D", c[i % 3]);

            if(!fd_write(nextout, buf, size)) {
                err_msg("signal interrupt", runinfo->mSName, runinfo->mSLine, runinfo->mArgs[0]);
                runinfo->mRCode = RCODE_SIGNAL_INTERRUPT;
                return FALSE;
            }
            if(nextout == gStdout) {
                if(!fd_flash(nextout, 1)) {
                    err_msg("signal interrupt", runinfo->mSName, runinfo->mSLine, runinfo->mArgs[0]);
                    runinfo->mRCode = RCODE_SIGNAL_INTERRUPT;
                    return FALSE;
                }
            }

            usleep(200000);
        }

        runinfo->mRCode = 0;
    }

    return TRUE;
}

BOOL cmd_stackinfo(sObject* nextin, sObject* nextout, sRunInfo* runinfo)
{
    char buf[BUFSIZ];

    int size = snprintf(buf, BUFSIZ, "slot size %d\npool size %d\nall object number %d\n", stack_slot_size(), stack_page_size(), stack_slot_size()*stack_page_size());

    if(!fd_write(nextout, buf, size)) {
        err_msg("signal interrupt", runinfo->mSName, runinfo->mSLine, runinfo->mArgs[0]);
        runinfo->mRCode = RCODE_SIGNAL_INTERRUPT;
        return FALSE;
    }

    runinfo->mRCode = 0;
    return TRUE;
}

BOOL cmd_stackframe(sObject* nextin, sObject* nextout, sRunInfo* runinfo)
{
    char buf[BUFSIZ];

    uobject_it* it = uobject_loop_begin(SFUN(runinfo->mRunningObject).mLocalObjects);
    while(it) {
        sObject* object2 = uobject_loop_item(it);
        char* key = uobject_loop_key(it);

        char* obj_kind[T_TYPE_MAX] = {
            NULL, "var", "array", "hash", "list", "native function", "block", "file dicriptor", "file dicriptor", "job", "object", "function", "class", "external program", "completion", "external object"
        };
        int size = snprintf(buf, BUFSIZ, "%s: %s\n", key, obj_kind[TYPE(object2)]);

        if(!fd_write(nextout, buf, size)) {
            err_msg("signal interrupt", runinfo->mSName, runinfo->mSLine, runinfo->mArgs[0]);
            runinfo->mRCode = RCODE_SIGNAL_INTERRUPT;
            return FALSE;
        }

        it = uobject_loop_next(it);
    }

    if(uobject_count(SFUN(runinfo->mRunningObject).mLocalObjects) == 0) {
        runinfo->mRCode = 1;
    }
    else {
        runinfo->mRCode = 0;
    }

    return TRUE;
}

BOOL cmd_funinfo(sObject* nextin, sObject* nextout, sRunInfo* runinfo)
{
    sObject* running_object = runinfo->mRunningObject;

    if(running_object && (TYPE(running_object) == T_FUN || TYPE(running_object) == T_CLASS) && gRunInfoOfRunningObject)
    {
        sRunInfo* runinfo2 = gRunInfoOfRunningObject;

        char buf[BUFSIZ];
        sObject* object;

        char current_object[128];
        *current_object = 0;
        object = runinfo2->mCurrentObject;
        while(object && object != gRootObject) {
            strcat(current_object, SUOBJECT(object).mName);
            strcat(current_object, "<-");
            object = SUOBJECT(object).mParent;
        }
        strcat(current_object, "root");

        char reciever_object[128];
        *reciever_object = 0;
        object = runinfo2->mRecieverObject;
        while(object && object != gRootObject) {
            strcat(reciever_object, SUOBJECT(object).mName);
            strcat(reciever_object, "<-");
            object = SUOBJECT(object).mParent;
        }
        strcat(reciever_object, "root");

        sCommand* command = runinfo2->mCommand;

        char command_name[128];
        *command_name = 0;
        int i = 0;
        while(i < command->mMessagesNum) {
            strcat(command_name, command->mMessages[i++]);
            strcat(command_name, "->");
        }
        strcat(command_name, command->mArgs[0]);

        int size = snprintf(buf, BUFSIZ, "source name: %s\nsource line: %d\nrun nest level: %d\ncurrent object: %s\nreciever object: %s\ncommand name: %s\n", runinfo2->mSName, runinfo2->mSLine, runinfo2->mNestLevel, current_object, reciever_object, command_name);

        if(!fd_write(nextout, buf, size)) {
            err_msg("signal interrupt", runinfo->mSName, runinfo->mSLine, runinfo->mArgs[0]);
            runinfo->mRCode = RCODE_SIGNAL_INTERRUPT;
            return FALSE;
        }

        runinfo->mRCode = 0;
    }

    return TRUE;
}

BOOL cmd_gcinfo(sObject* nextin, sObject* nextout, sRunInfo* runinfo)
{
    char buf[BUFSIZ];

    int size = snprintf(buf, BUFSIZ, "free objects %d\nused objects %d\nall object number %d\nslot size %d\npool size %d\n", gc_free_objects_num(), gc_used_objects_num(), gc_slot_size()*gc_pool_size(), gc_slot_size(), gc_pool_size());

    if(!fd_write(nextout, buf, size)) {
        err_msg("signal interrupt", runinfo->mSName, runinfo->mSLine, runinfo->mArgs[0]);
        runinfo->mRCode = RCODE_SIGNAL_INTERRUPT;
        return FALSE;
    }

    runinfo->mRCode = 0;
    return TRUE;
}

BOOL cmd_time(sObject* nextin, sObject* nextout, sRunInfo* runinfo)
{
    if(runinfo->mBlocksNum > 0) {
        time_t timer;
        time(&timer);

        int rcode;
        if(!run(runinfo->mBlocks[0], nextin, nextout, &rcode, runinfo->mCurrentObject, runinfo->mRunningObject)) {
            runinfo->mRCode = rcode;
            return FALSE;
        }
        runinfo->mRCode = rcode;

        time_t timer2;
        time(&timer2);
       
        int sec = timer2 - timer;
        int minuts = sec / 60;
        int sec2 = sec - minuts * 60;

        char buf[1024];
        int size = snprintf(buf, 1024, "%d sec(%d minuts %d sec)\n", sec, minuts, sec2);

        if(!fd_write(nextout, buf, size)) {
            err_msg("signal interrupt", runinfo->mSName, runinfo->mSLine, runinfo->mArgs[0]);
            runinfo->mRCode = RCODE_SIGNAL_INTERRUPT;
            return FALSE;
        }
    }

    return TRUE;
}

BOOL cmd_umask(sObject* nextin, sObject* nextout, sRunInfo* runinfo)
{
    if(!runinfo->mFilter && runinfo->mArgsNumRuntime == 2) {
        int number = strtol(runinfo->mArgsRuntime[1], NULL, 8);
        umask(number);
        runinfo->mRCode = 0;
    }

    return TRUE;
}

static BOOL xyzsh_seek_external_programs(char* path, sObject* sys, char* sname, int sline)
{
    DIR* dir = opendir(path);
    if(dir) {
        struct dirent* entry;
        while(entry = readdir(dir)) {
#if defined(__CYGWIN__)
            char entry2[PATH_MAX];
            if(strstr(entry->d_name, ".exe") == entry->d_name + strlen(entry->d_name) - 4) {
                strcpy(entry2, entry->d_name);

                entry2[strlen(entry2)-4] = 0;
            }
            else {
                strcpy(entry2, entry->d_name);
            }

            char path2[PATH_MAX];
            snprintf(path2, PATH_MAX, "%s/%s", path, entry2);

            struct stat stat_;
            memset(&stat_, 0, sizeof(struct stat));
            stat(path2, &stat_);

            if(strcmp(entry2, ".") != 0
                && strcmp(entry2, "..") != 0
                &&
                (stat_.st_mode & S_IXUSR ||stat_.st_mode & S_IXGRP||stat_.st_mode & S_IXOTH)
                && uobject_item(sys, entry2) == NULL)
            {
                uobject_put(sys, entry2, EXTPROG_NEW_GC(path2, TRUE));
            }
#else
            char path2[PATH_MAX];
            snprintf(path2, PATH_MAX, "%s/%s", path, entry->d_name);
            
            struct stat stat_;
            memset(&stat_, 0, sizeof(struct stat));
            stat(path2, &stat_);

            if(strcmp(entry->d_name, ".") != 0
                && strcmp(entry->d_name, "..") != 0
                &&
                (stat_.st_mode & S_IXUSR ||stat_.st_mode & S_IXGRP||stat_.st_mode & S_IXOTH)
                && uobject_item(sys, entry->d_name) == NULL)
            {
                uobject_put(sys, entry->d_name, EXTPROG_NEW_GC(path2, TRUE));
            }
#endif
        }

        closedir(dir);
    }
    else {
        char buf[BUFSIZ];
        snprintf(buf, BUFSIZ, "%s is invalid path", path);
        err_msg(buf, sname, sline, "rehash");
        //return FALSE;
    }

    return TRUE;
}

BOOL xyzsh_rehash(char* sname, int sline)
{
    char* path = getenv("PATH");
    if(path == NULL) {
        err_msg("PATH environment variables is NULL", sname, sline, "rehash");
        return FALSE;
    }

    sObject* sys = UOBJECT_NEW_GC(8, gRootObject, "sys", FALSE);
    uobject_put(gRootObject, "sys", sys);
    uobject_init(sys);

    char buf[PATH_MAX+1];

    char* p = path;
    while(1) {
        char* p2;
        if(p2 = strstr(p, ":")) {
            int size = p2 - p;
            if(size > PATH_MAX) {
                size = PATH_MAX;
            }
            memcpy(buf, p, size);
            buf[size] = 0;

            p = p2 + 1;

            if(!xyzsh_seek_external_programs(buf, sys, sname, sline)) {
                return FALSE;
            }
        }
        else {
            strncpy(buf, p, PATH_MAX);

            if(!xyzsh_seek_external_programs(buf, sys, sname, sline)) {
                return FALSE;
            }
            break;
        }
    }

    return TRUE;
}

BOOL cmd_rehash(sObject* nextin, sObject* nextout, sRunInfo* runinfo)
{
    if(!runinfo->mFilter) {
        if(!xyzsh_rehash(runinfo->mSName, runinfo->mSLine)) {
            return FALSE;
        }
        runinfo->mRCode = 0;
    }

    return TRUE;
}


sObject* gPrompt;

BOOL cmd_prompt(sObject* nextin, sObject* nextout, sRunInfo* runinfo)
{
    if(runinfo->mBlocksNum == 1) {
        gPrompt = block_clone_on_gc(runinfo->mBlocks[0], T_BLOCK, FALSE);
        uobject_put(gXyzshObject, "_prompt", gPrompt);
        runinfo->mRCode = 0;
    }

    return TRUE;
}

BOOL cmd_block(sObject* nextin, sObject* nextout, sRunInfo* runinfo)
{
    sObject* fun = runinfo->mRunningObject;

    if(sRunInfo_option(runinfo, "-number")) {
        if(SFUN(fun).mArgBlocks) {
            char buf[32];
            int size = snprintf(buf, 32, "%d\n", vector_count(SFUN(fun).mArgBlocks));

            if(!fd_write(nextout, buf, size))
            {
                err_msg("signal interrupt", runinfo->mSName, runinfo->mSLine, runinfo->mArgs[0]);
                runinfo->mRCode = RCODE_SIGNAL_INTERRUPT;
                return FALSE;
            }

            runinfo->mRCode = 0;
        }
    }
    else if(sRunInfo_option(runinfo, "-run")) {
        int block_num;
        if(runinfo->mArgsNumRuntime > 1) {
            block_num = atoi(runinfo->mArgsRuntime[1]);
        }
        else {
            block_num = 0;
        }

        if(SFUN(fun).mArgBlocks && block_num < vector_count(SFUN(fun).mArgBlocks)) {
            sObject* block = vector_item(SFUN(fun).mArgBlocks, block_num);

            int rcode = 0;
            if(!run(block, nextin, nextout, &rcode, runinfo->mCurrentObject, runinfo->mRunningObject)) 
            {
                err_msg_adding("run time error", runinfo->mSName, runinfo->mSLine, runinfo->mArgs[0]);
                runinfo->mRCode = rcode;
                return FALSE;
            }
        }
    }
    else {
        int block_num;
        if(runinfo->mArgsNumRuntime > 1) {
            block_num = atoi(runinfo->mArgsRuntime[1]);
        }
        else {
            block_num = 0;
        }

        if(SFUN(fun).mArgBlocks && block_num < vector_count(SFUN(fun).mArgBlocks)) {
            sObject* block = vector_item(SFUN(fun).mArgBlocks, block_num);

            if(!fd_write(nextout, SBLOCK(block).mSource, strlen(SBLOCK(block).mSource)))
            {
                err_msg("signal interrupt", runinfo->mSName, runinfo->mSLine, runinfo->mArgs[0]);
                runinfo->mRCode = RCODE_SIGNAL_INTERRUPT;
                return FALSE;
            }

            runinfo->mRCode = 0;
        }
    }

    return TRUE;
}

BOOL cmd_kanjicode(sObject* nextin, sObject* nextout, sRunInfo* runinfo)
{
    if(sRunInfo_option(runinfo, "-byte")) {
        gKanjiCode = kByte;
        runinfo->mRCode = 0;
    }
    else if(sRunInfo_option(runinfo, "-utf8")) {
        gKanjiCode = kUtf8;
        runinfo->mRCode = 0;
    }
    else if(sRunInfo_option(runinfo, "-sjis")) {
        gKanjiCode = kSjis;
        runinfo->mRCode = 0;
    }
    else if(sRunInfo_option(runinfo, "-eucjp")) {
        gKanjiCode = kEucjp;
        runinfo->mRCode = 0;
    }
    else {
        if(gKanjiCode == kUtf8) {
            if(!fd_write(nextout, "utf8", 4)) {
                err_msg("interrupt", runinfo->mSName, runinfo->mSLine, runinfo->mArgs[0]);
                runinfo->mRCode = RCODE_SIGNAL_INTERRUPT;
                return FALSE;
            }
            if(!fd_write(nextout, "\n", 1)) {
                err_msg("interrupt", runinfo->mSName, runinfo->mSLine, runinfo->mArgs[0]);
                runinfo->mRCode = RCODE_SIGNAL_INTERRUPT;
                return FALSE;
            }

            runinfo->mRCode = 0;
        }
        else if(gKanjiCode == kByte) {
            if(!fd_write(nextout, "byte", 4)) {
                err_msg("interrupt", runinfo->mSName, runinfo->mSLine, runinfo->mArgs[0]);
                runinfo->mRCode = RCODE_SIGNAL_INTERRUPT;
                return FALSE;
            }
            if(!fd_write(nextout, "\n", 1)) {
                err_msg("interrupt", runinfo->mSName, runinfo->mSLine, runinfo->mArgs[0]);
                runinfo->mRCode = RCODE_SIGNAL_INTERRUPT;
                return FALSE;
            }

            runinfo->mRCode = 0;
        }
        else if(gKanjiCode == kSjis) {
            if(!fd_write(nextout, "sjis", 4)) {
                err_msg("interrupt", runinfo->mSName, runinfo->mSLine, runinfo->mArgs[0]);
                runinfo->mRCode = RCODE_SIGNAL_INTERRUPT;
                return FALSE;
            }
            if(!fd_write(nextout, "\n", 1)) {
                err_msg("interrupt", runinfo->mSName, runinfo->mSLine, runinfo->mArgs[0]);
                runinfo->mRCode = RCODE_SIGNAL_INTERRUPT;
                return FALSE;
            }

            runinfo->mRCode = 0;
        }
        else if(gKanjiCode == kEucjp) {
            if(!fd_write(nextout, "eucjp", 5)) {
                err_msg("interrupt", runinfo->mSName, runinfo->mSLine, runinfo->mArgs[0]);
                runinfo->mRCode = RCODE_SIGNAL_INTERRUPT;
                return FALSE;
            }
            if(!fd_write(nextout, "\n", 1)) {
                err_msg("interrupt", runinfo->mSName, runinfo->mSLine, runinfo->mArgs[0]);
                runinfo->mRCode = RCODE_SIGNAL_INTERRUPT;
                return FALSE;
            }

            runinfo->mRCode = 0;
        }
    }

    return TRUE;
}

BOOL cmd_jobs(sObject* nextin, sObject* nextout, sRunInfo* runinfo)
{
    char buf[BUFSIZ];
    int i;
    for(i=0; i<vector_count(gJobs); i++) {
        sObject* job = vector_item(gJobs, i);
        int size = snprintf(buf, BUFSIZ, "[%d] %s (pgrp: %d)", i+1, SJOB(job).mName, SJOB(job).mPGroup);
        if(!fd_write(nextout, buf, size)) {
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

    if(vector_count(gJobs) == 0) {
        runinfo->mRCode = 1;
    }
    else {
        runinfo->mRCode = 0;
    }

    return TRUE;
}

BOOL cmd_fg(sObject* nextin, sObject* nextout, sRunInfo* runinfo)
{
    if(gAppType != kATOptC) {
        if(runinfo->mArgsNumRuntime >= 2) {
            int num = atoi(runinfo->mArgsRuntime[1]);
            if(!forground_job(num-1)) {
                return FALSE;
            }
        }
        else {
            if(!forground_job(0)) {
                return FALSE;
            }
        }

        runinfo->mRCode = 0;
    }

    return TRUE;
}
