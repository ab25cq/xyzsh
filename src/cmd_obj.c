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
#include <ctype.h>

#if defined(HAVE_CURSES_H)
#include <curses.h>
#elif defined(HAVE_NCURSES_H)
#include <ncurses.h>
#elif defined(HAVE_NCURSES_NCURSES_H)
#include <ncurses/ncurses.h>
#endif

#include "xyzsh.h"

BOOL cmd_mshow(sObject* nextin, sObject* nextout, sRunInfo* runinfo)
{
    sObject* object = runinfo->mRecieverObject;
    uobject_it* it = uobject_loop_begin(object);
    while(it) {
        sObject* object2 = uobject_loop_item(it);
        char* key = uobject_loop_key(it);

        char buf[BUFSIZ];
        char*obj_kind[T_TYPE_MAX] = {
            NULL, "var", "array", "hash", "list", "native function", "block", "file dicriptor", "file dicriptor2", "job", "object", "function", "class", "external program", "completion", "external object", "alias", "int"
        };
        int size = snprintf(buf, BUFSIZ, "%s: %s\n", key, obj_kind[STYPE(object2)]);

        if(!fd_write(nextout, buf, size)) {
            err_msg("signal interrupt", runinfo->mSName, runinfo->mSLine);
            runinfo->mRCode = RCODE_SIGNAL_INTERRUPT;
            return FALSE;
        }

        it = uobject_loop_next(it);
    }

    runinfo->mRCode = 0;
    return TRUE;
}

BOOL cmd_defined(sObject* nextin, sObject* nextout, sRunInfo* runinfo)
{
    if(runinfo->mArgsNumRuntime > 1) {
        int i;
        for(i=1; i<runinfo->mArgsNumRuntime; i++) {
            sObject* object;
            (void)get_object_from_argument(&object, runinfo->mArgsRuntime[i], runinfo->mCurrentObject, runinfo->mRunningObject, runinfo);

            if(object) {
                runinfo->mRCode = 0;
            }
            else {
                runinfo->mRCode = RCODE_NFUN_FALSE;
                break;
            }
        }
    }

    return TRUE;
}

BOOL object_info(sObject* object, sObject* nextin, sObject* nextout, sRunInfo* runinfo)
{
    char buf[128];

    switch(STYPE(object)) {
        case T_STRING :
            snprintf(buf, 128, "Type: string\nLength: %d\nMallocLen: %d\n", SSTRING(object).mLen, SSTRING(object).mMallocLen);

            if(!fd_write(nextout, buf, strlen(buf))) {
                err_msg("signal interrupt", runinfo->mSName, runinfo->mSLine);
                runinfo->mRCode = RCODE_SIGNAL_INTERRUPT;
                return FALSE;
            }
            break;

        case T_VECTOR:
            snprintf(buf, 128, "Type: vector\nTable Size: %d\nCount: %d\n", SVECTOR(object).mTableSize, SVECTOR(object).mCount);

            if(!fd_write(nextout, buf, strlen(buf))) {
                err_msg("signal interrupt", runinfo->mSName, runinfo->mSLine);
                runinfo->mRCode = RCODE_SIGNAL_INTERRUPT;
                return FALSE;
            }
            break;

        case T_HASH:
            snprintf(buf, 128, "Type: hash\nTable Size: %d\nCount: %d\n", SHASH(object).mTableSize, SHASH(object).mCounter);

            if(!fd_write(nextout, buf, strlen(buf))) {
                err_msg("signal interrupt", runinfo->mSName, runinfo->mSLine);
                runinfo->mRCode = RCODE_SIGNAL_INTERRUPT;
                return FALSE;
            }
            break;

        case T_LIST:
            snprintf(buf, 128, "Type: list\nCount: %d\n", SLIST(object).mCount);

            if(!fd_write(nextout, buf, strlen(buf))) {
                err_msg("signal interrupt", runinfo->mSName, runinfo->mSLine);
                runinfo->mRCode = RCODE_SIGNAL_INTERRUPT;
                return FALSE;
            }
            break;

        case T_NFUN:
            snprintf(buf, 128, "Type: native function\nParent: %s\n", SNFUN(object).mParent ? "exists" : "no parent");

            if(!fd_write(nextout, buf, strlen(buf))) {
                err_msg("signal interrupt", runinfo->mSName, runinfo->mSLine);
                runinfo->mRCode = RCODE_SIGNAL_INTERRUPT;
                return FALSE;
            }
            break;

        case T_BLOCK:
            snprintf(buf, 128, "Type: block\nStatment Number: %d\nStatment Size %d\n", SBLOCK(object).mStatmentsNum, SBLOCK(object).mStatmentsSize);

            if(!fd_write(nextout, buf, strlen(buf))) {
                err_msg("signal interrupt", runinfo->mSName, runinfo->mSLine);
                runinfo->mRCode = RCODE_SIGNAL_INTERRUPT;
                return FALSE;
            }
            break;

        case T_FD:
            snprintf(buf, 128, "Type: file discriptor\n\nBuffer Size %d\nBuffer Length: %d\nLine Number %d\nLine Field %d\nReaded Line Number %d\n", SFD(object).mBufSize, SFD(object).mBufLen, vector_count(SFD(object).mLines), SFD(object).mLinesLineField, SFD(object).mReadedLineNumber);

            if(!fd_write(nextout, buf, strlen(buf))) {
                err_msg("signal interrupt", runinfo->mSName, runinfo->mSLine);
                runinfo->mRCode = RCODE_SIGNAL_INTERRUPT;
                return FALSE;
            }
            break;

        case T_FD2:
            snprintf(buf, 128, "Type: file discriptor2\nFile Discriptor %d\n", SFD2(object).mFD);

            if(!fd_write(nextout, buf, strlen(buf))) {
                err_msg("signal interrupt", runinfo->mSName, runinfo->mSLine);
                runinfo->mRCode = RCODE_SIGNAL_INTERRUPT;
                return FALSE;
            }
            break;

        case T_JOB:
            snprintf(buf, 128, "Type: job\nName %s\nProcess Group %d\nSuspended %d\n", SJOB(object).mName, SJOB(object).mPGroup, SJOB(object).mSuspended);

            if(!fd_write(nextout, buf, strlen(buf))) {
                err_msg("signal interrupt", runinfo->mSName, runinfo->mSLine);
                runinfo->mRCode = RCODE_SIGNAL_INTERRUPT;
                return FALSE;
            }
            break;

        case T_UOBJECT: 
            snprintf(buf, 128, "Type: user object\nTable Size %d\nCount %d\nName %s\nParent %s", SUOBJECT(object).mTableSize, SUOBJECT(object).mCounter, SUOBJECT(object).mName, SUOBJECT(object).mParent ? SUOBJECT(SUOBJECT(object).mParent).mName: "no parent");

            if(!fd_write(nextout, buf, strlen(buf))) {
                err_msg("signal interrupt", runinfo->mSName, runinfo->mSLine);
                runinfo->mRCode = RCODE_SIGNAL_INTERRUPT;
                return FALSE;
            }
            break;

        case T_FUN:
            snprintf(buf, 128, "Type: function\nParent: %s\nNo StackFrame: %d\n", SFUN(object).mParent ? "exists": "no parent", SFUN(object).mFlags & FUN_FLAGS_NO_STACKFRAME);

            if(!fd_write(nextout, buf, strlen(buf))) {
                err_msg("signal interrupt", runinfo->mSName, runinfo->mSLine);
                runinfo->mRCode = RCODE_SIGNAL_INTERRUPT;
                return FALSE;
            }
            break;

        case T_CLASS:
            snprintf(buf, 128, "Type: class\nParent: %s\nNo StackFrame: %d\n", SCLASS(object).mParent ? "exists": "no parent", SCLASS(object).mFlags & FUN_FLAGS_NO_STACKFRAME);

            if(!fd_write(nextout, buf, strlen(buf))) {
                err_msg("signal interrupt", runinfo->mSName, runinfo->mSLine);
                runinfo->mRCode = RCODE_SIGNAL_INTERRUPT;
                return FALSE;
            }
            break;

        case T_EXTPROG:
            snprintf(buf, 128, "Type: external program\nPath: %s\n", SEXTPROG(object).mPath);

            if(!fd_write(nextout, buf, strlen(buf))) {
                err_msg("signal interrupt", runinfo->mSName, runinfo->mSLine);
                runinfo->mRCode = RCODE_SIGNAL_INTERRUPT;
                return FALSE;
            }
            break;

        case T_COMPLETION: {
            sObject* block = SCOMPLETION(object).mBlock;
            snprintf(buf, 128, "Type: completion\nStatment Number: %d\nStatment Size %d\n", SBLOCK(block).mStatmentsNum, SBLOCK(block).mStatmentsSize);

            if(!fd_write(nextout, buf, strlen(buf))) {
                err_msg("signal interrupt", runinfo->mSName, runinfo->mSLine);
                runinfo->mRCode = RCODE_SIGNAL_INTERRUPT;
                return FALSE;
            }
            }
            break;

        case T_EXTOBJ:
            if(!fd_write(nextout, "no info", strlen(buf))) {
                err_msg("signal interrupt", runinfo->mSName, runinfo->mSLine);
                runinfo->mRCode = RCODE_SIGNAL_INTERRUPT;
                return FALSE;
            }
            break;

        default:
            fprintf(stderr,"unexpected err in object_info\n");
            exit(1);
    }
}

BOOL cmd_objinfo(sObject* nextin, sObject* nextout, sRunInfo* runinfo)
{
    if(runinfo->mArgsNumRuntime > 1) {
        int i;
        for(i=1; i<runinfo->mArgsNumRuntime; i++) {
            sObject* object;
            if(!get_object_from_argument(&object, runinfo->mArgsRuntime[i], runinfo->mCurrentObject, runinfo->mRunningObject, runinfo)) {
                return FALSE;
            }

            if(object) {
                if(!object_info(object, nextin, nextout, runinfo)) {
                    return FALSE;
                }
                runinfo->mRCode = 0;
            }
        }
    }

    return TRUE;
}

BOOL cmd_typeof(sObject* nextin, sObject* nextout, sRunInfo* runinfo)
{
    if(runinfo->mArgsNumRuntime > 1) {
        int i;
        for(i=1; i<runinfo->mArgsNumRuntime; i++) {
            sObject* object;
            if(!get_object_from_argument(&object, runinfo->mArgsRuntime[i], runinfo->mCurrentObject, runinfo->mRunningObject, runinfo)) {
                return FALSE;
            }

            if(object) {
                runinfo->mRCode = 0;

                char buf[BUFSIZ];
                char* obj_kind[T_TYPE_MAX] = {
                    NULL, "var", "array", "hash", "list", "native function", "block", "file dicriptor", "file dicriptor2", "job", "object", "function", "class", "external program", "completion", "external object", "alias", "int"
                };
                int size = snprintf(buf, BUFSIZ, "%s\n", obj_kind[STYPE(object)]);

                if(!fd_write(nextout, buf, size)) {
                    err_msg("signal interrupt", runinfo->mSName, runinfo->mSLine);
                    runinfo->mRCode = RCODE_SIGNAL_INTERRUPT;
                    return FALSE;
                }
            }
        }
    }

    return TRUE;
}

BOOL cmd_mrun(sObject* nextin, sObject* nextout, sRunInfo* runinfo)
{
    if(runinfo->mBlocksNum == 1) {
        sObject* klass = class_clone_on_stack_from_stack_block(runinfo->mBlocks[0], NULL, sRunInfo_option(runinfo, "-copy-stackframe"));

        int rcode = 0;
        if(!run(SCLASS(klass).mBlock, nextin, nextout, &rcode, runinfo->mRecieverObject, runinfo->mRunningObject)) {
            runinfo->mRCode = rcode;
            return FALSE;
        }

        runinfo->mRCode = rcode;
    }
    else if(runinfo->mArgsNumRuntime >= 2) {
        sObject* current = runinfo->mCurrentObject;
        sObject* klass = access_object(runinfo->mArgsRuntime[1], &current, runinfo->mRunningObject);

        if(klass && STYPE(klass) == T_CLASS) {
            runinfo->mCurrentObject = runinfo->mRecieverObject;
            if(!run_function(klass, nextin, nextout, runinfo, runinfo->mArgsRuntime + 2, runinfo->mArgsNumRuntime -2, runinfo->mBlocks, runinfo->mBlocksNum)) {
                return FALSE;
            }

            //runinfo->mRCode = 0;
        }
        else {
            err_msg("not found variable", runinfo->mSName, runinfo->mSLine);
            return FALSE;
        }
    }

    return TRUE;
}

BOOL output_cwo(sObject* nextout, sObject* object, sRunInfo* runinfo) 
{
    if(object) {
        if(object == gRootObject) {
            if(!fd_write(nextout, SUOBJECT(object).mName, strlen(SUOBJECT(object).mName))) {
                err_msg("signal interrupt", runinfo->mSName, runinfo->mSLine);
                runinfo->mRCode = RCODE_SIGNAL_INTERRUPT;
                return FALSE;
            }
            if(!fd_write(nextout, "::", 2)) {
                err_msg("signal interrupt", runinfo->mSName, runinfo->mSLine);
                runinfo->mRCode = RCODE_SIGNAL_INTERRUPT;
                return FALSE;
            }
        }
        else {
            if(!output_cwo(nextout, SUOBJECT(object).mParent, runinfo)) {
                return FALSE;
            }
            if(!fd_write(nextout, SUOBJECT(object).mName, strlen(SUOBJECT(object).mName))) {
                err_msg("signal interrupt", runinfo->mSName, runinfo->mSLine);
                runinfo->mRCode = RCODE_SIGNAL_INTERRUPT;
                return FALSE;
            }
            if(!fd_write(nextout, "::", 2)) {
                err_msg("signal interrupt", runinfo->mSName, runinfo->mSLine);
                runinfo->mRCode = RCODE_SIGNAL_INTERRUPT;
                return FALSE;
            }
        }
    }

    return TRUE;
}

BOOL cmd_pwo(sObject* nextin, sObject* nextout, sRunInfo* runinfo)
{
    if(!output_cwo(nextout, runinfo->mCurrentObject, runinfo)) {
        return FALSE;
    }

    fd_trunc(nextout, SFD(nextout).mBufLen-2);  // delete last ::

    if(!fd_write(nextout, "\n", 1)) {
        err_msg("signal interrupt", runinfo->mSName, runinfo->mSLine);
        runinfo->mRCode = RCODE_SIGNAL_INTERRUPT;
        return FALSE;
    }
    runinfo->mRCode = 0;

    return TRUE;
}

/*
BOOL cmd_co(sObject* nextin, sObject* nextout, sRunInfo* runinfo)
{
    if(runinfo->mArgsNumRuntime == 2) {
        sObject* new_current;
        if(!get_object_from_argument(&new_current, runinfo->mArgsRuntime[1], runinfo->mCurrentObject, runinfo->mRunningObject, runinfo)) {
            return FALSE;
        }

        if(new_current && STYPE(new_current) == T_UOBJECT) {
            gCurrentObject = new_current;
            runinfo->mRCode = 0;
        }
        else {
            err_msg("can't change the current object", runinfo->mSName, runinfo->mSLine);
            return FALSE;
        }
    }

    return TRUE;
}
*/

BOOL cmd_inherit(sObject* nextin, sObject* nextout, sRunInfo* runinfo)
{
    sObject* running_object = runinfo->mRunningObject;
    if(runinfo->mRunningObject) {
        switch(STYPE(running_object)) {
        case T_FUN:  {
            sObject* parent = SFUN(running_object).mParent;

            if(parent) {
                if(STYPE(parent) == T_NFUN) {
                    if(gAppType == kATConsoleApp)
                    {
                        if(tcsetpgrp(gTtyFD, getpgid(0)) < 0) {
                            perror("tcsetpgrp inner command");
                            exit(1);
                        }
                    }

                    runinfo->mRCode = RCODE_NFUN_INVALID_USSING;
                    if(!SNFUN(parent).mNativeFun(nextin, nextout, runinfo)) {
                        return FALSE;
                    }
                    if(runinfo->mRCode == RCODE_NFUN_INVALID_USSING) {
                        err_msg("invalid command using", runinfo->mSName, runinfo->mSLine);
                        return FALSE;
                    }
                    //runinfo->mRCode = 0;
                }
                else {  // T_FUN
                    if(!run_function(parent, nextin, nextout, runinfo, runinfo->mArgsRuntime + 1, runinfo->mArgsNumRuntime -1, runinfo->mBlocks, runinfo->mBlocksNum))
                    {
                        return FALSE;
                    }
                    //runinfo->mRCode = 0;
                }
            }
            }
            break;

        case T_CLASS: {
            sObject* parent = SFUN(running_object).mParent;

            if(parent) {
                if(!run_function(parent, nextin, nextout, runinfo, runinfo->mArgsRuntime + 1, runinfo->mArgsNumRuntime -1, runinfo->mBlocks, runinfo->mBlocksNum))
                {
                    return FALSE;
                }
                //runinfo->mRCode = 0;
            }
            }
            break;

        case T_NFUN:
        case T_BLOCK:
            err_msg("There is not a parent object", runinfo->mSName, runinfo->mSLine);
            return FALSE;

        default:
            fprintf(stderr, "unexpected error in cmd_inherit\n");
            exit(1);
        }
    }

    return TRUE;
}

static BOOL entry_fun(sObject* nextin, sObject* nextout, sRunInfo* runinfo, sObject* block, int type, sObject* object, char* name)
{
    BOOL no_stackframe = sRunInfo_option(runinfo, "-copy-stackframe");

    if(type == T_CLASS) 
        block = class_clone_on_gc_from_stack_block(block, TRUE, NULL, no_stackframe); // block is a stack object, so change the memory manager from stack to gc
    else
        block = fun_clone_on_gc_from_stack_block(block, TRUE, NULL, no_stackframe); // block is a stack object, so change the memory manager from stack to gc

    if(sRunInfo_option(runinfo, "-inherit")) {
        sObject* parent = uobject_item(object, name);
        if(parent) {
            if(type == T_FUN && (STYPE(parent) == T_FUN || STYPE(parent) == T_NFUN) 
                || type == T_CLASS && STYPE(parent) == T_CLASS) 
            {
                SFUN(block).mParent = parent;
            }
            else {
                err_msg("can't inherit a parent function beacuse of the different type", runinfo->mSName, runinfo->mSLine);
                return FALSE;
            }
        }
        else {
            err_msg("can't inherit. There is not a parent", runinfo->mSName, runinfo->mSLine);
            return FALSE;
        }
    }
    else {
        SFUN(block).mParent = NULL;
    }

    char* argument;
    if(argument = sRunInfo_option_with_argument(runinfo, "-option-with-argument")) {
        char option[BUFSIZ+1];
        char* p = option;
        *p++ = '-';
        while(*argument) {
            if(*argument == ',') {
                argument++;
                *p = 0;
                if(strlen(option) > 1) {
                    if(!fun_put_option_with_argument(block, STRDUP(option))) {
                        err_msg("option number is full.", runinfo->mSName, runinfo->mSLine);
                        return FALSE;
                    }
                }
                p = option;
                *p++ = '-';
            }
            else {
                if(isdigit(*argument) || isalpha(*argument) || *argument ==  '_') {
                    if(p - option < BUFSIZ) {
                        *p++ = *argument++;
                    }
                    else {
                        argument++;
                    }
                }
                else {
                    err_msg("invalid option name", runinfo->mSName, runinfo->mSLine);
                    return FALSE;
                }
            }
        }
        *p = 0;
        if(strlen(option) > 1) {
            if(!fun_put_option_with_argument(block, STRDUP(option))) {
                err_msg("option number is full.", runinfo->mSName, runinfo->mSLine);
                return FALSE;
            }
        }
    }

    uobject_put(object, name, block);

    return TRUE;
}

BOOL cmd_def(sObject* nextin, sObject* nextout, sRunInfo* runinfo)
{
    if(runinfo->mArgsNumRuntime == 2) {
        /// define ///
        if(runinfo->mBlocksNum >= 1) {
            sObject* object;
            sObject* name = STRING_NEW_STACK("");
            if(!get_object_prefix_and_name_from_argument(&object, name, runinfo->mArgsRuntime[1], runinfo->mCurrentObject, runinfo->mRunningObject, runinfo)) {
                return FALSE;
            }
            if(!entry_fun(nextin, nextout, runinfo, runinfo->mBlocks[0], T_FUN, object, string_c_str(name))) {
                return FALSE;
            }
            
            runinfo->mRCode = 0;
        }
        /// input ///
        else if(runinfo->mFilter) {
            stack_start_stack();
            int rcode = 0;
            sObject* block = BLOCK_NEW_STACK();
            int sline = 1;
            if(!parse(SFD(nextin).mBuf, "def", &sline, block, NULL)) {
                err_msg("parser error", runinfo->mSName, runinfo->mSLine);
                stack_end_stack();
                return FALSE;
            }

            sObject* object;
            sObject* name = STRING_NEW_STACK("");
            if(!get_object_prefix_and_name_from_argument(&object, name, runinfo->mArgsRuntime[1], runinfo->mCurrentObject, runinfo->mRunningObject, runinfo)) {
                stack_end_stack();
                return FALSE;
            }
            if(!entry_fun(nextin, nextout, runinfo, block, T_FUN, object, string_c_str(name))) {
                stack_end_stack();
                return FALSE;
            }

            stack_end_stack();
            
            if(SFD(nextin).mBufLen == 0) {
                runinfo->mRCode = RCODE_NFUN_NULL_INPUT;
            }
            else {
                runinfo->mRCode = 0;
            }
        }
        /// output ///
        else {
            sObject* fun;
            if(!get_object_from_argument(&fun, runinfo->mArgsRuntime[1], runinfo->mCurrentObject, runinfo->mRunningObject, runinfo)) {
                return FALSE;
            }

            if(fun && STYPE(fun) == T_FUN) {
                char* source = SBLOCK(SFUN(fun).mBlock).mSource;
                if(!fd_write(nextout, source, strlen(source))) {
                    err_msg("signal interrupt", runinfo->mSName, runinfo->mSLine);
                    runinfo->mRCode = RCODE_SIGNAL_INTERRUPT;
                    return FALSE;
                }
                if(source[strlen(source)-1] != '\n') {   /// pomch
                    if(!fd_write(nextout, "\n", 1)) {
                        err_msg("signal interrupt", runinfo->mSName, runinfo->mSLine);
                        runinfo->mRCode = RCODE_SIGNAL_INTERRUPT;
                        return FALSE;
                    }
                }
                runinfo->mRCode = 0;
            }
            else {
                err_msg("not found variable", runinfo->mSName, runinfo->mSLine);
                return FALSE;
            }
        }
    }

    return TRUE;
}

BOOL cmd_class(sObject* nextin, sObject* nextout, sRunInfo* runinfo)
{
    if(runinfo->mArgsNumRuntime == 2) {
        /// define ///
        if(runinfo->mBlocksNum >= 1) {
            sObject* object;
            sObject* name = STRING_NEW_STACK("");
            if(!get_object_prefix_and_name_from_argument(&object, name, runinfo->mArgsRuntime[1], runinfo->mCurrentObject, runinfo->mRunningObject, runinfo)) {
                return FALSE;
            }
            if(!entry_fun(nextin, nextout, runinfo, runinfo->mBlocks[0], T_CLASS, object, string_c_str(name))) {
                return FALSE;
            }
            
            runinfo->mRCode = 0;
        }
        /// input ///
        else if(runinfo->mFilter) {
            stack_start_stack();
            int rcode = 0;
            sObject* block = BLOCK_NEW_STACK();
            int sline = 1;
            if(!parse(SFD(nextin).mBuf, "def", &sline, block, NULL)) {
                err_msg("parser error", runinfo->mSName, runinfo->mSLine);
                stack_end_stack();
                return FALSE;
            }

            sObject* object;
            sObject* name = STRING_NEW_STACK("");
            if(!get_object_prefix_and_name_from_argument(&object, name, runinfo->mArgsRuntime[1], runinfo->mCurrentObject, runinfo->mRunningObject, runinfo)) {
                stack_end_stack();
                return FALSE;
            }
            if(!entry_fun(nextin, nextout, runinfo, block, T_CLASS, object, string_c_str(name))) {
                stack_end_stack();
                return FALSE;
            }
            stack_end_stack();
            
            if(SFD(nextin).mBufLen == 0) {
                runinfo->mRCode = RCODE_NFUN_NULL_INPUT;
            }
            else {
                runinfo->mRCode = 0;
            }
        }
        /// output ///
        else {
            sObject* klass;
            if(!get_object_from_argument(&klass, runinfo->mArgsRuntime[1], runinfo->mCurrentObject, runinfo->mRunningObject, runinfo)) {
                return FALSE;
            }

            if(klass && STYPE(klass) == T_CLASS) {
                char* source = SBLOCK(SCLASS(klass).mBlock).mSource;
                if(!fd_write(nextout, source, strlen(source))) {
                    err_msg("signal interrupt", runinfo->mSName, runinfo->mSLine);
                    runinfo->mRCode = RCODE_SIGNAL_INTERRUPT;
                    return FALSE;
                }
                if(source[strlen(source)-1] != '\n') {   /// pomch
                    if(!fd_write(nextout, "\n", 1)) {
                        err_msg("signal interrupt", runinfo->mSName, runinfo->mSLine);
                        runinfo->mRCode = RCODE_SIGNAL_INTERRUPT;
                        return FALSE;
                    }
                }
                runinfo->mRCode = 0;
            }
            else {
                err_msg("not found variable", runinfo->mSName, runinfo->mSLine);
                return FALSE;
            }
        }
    }

    return TRUE;
}

BOOL cmd_var(sObject* nextin, sObject* nextout, sRunInfo* runinfo)
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

    /// input
    if(runinfo->mFilter) {
        fd_split(nextin, lf, TRUE, FALSE, FALSE);

        if(sRunInfo_option(runinfo, "-new")) {
            int max = vector_count(SFD(nextin).mLines);
            if(max > 0) {
                sObject* new_var = STRING_NEW_GC(vector_item(SFD(nextin).mLines, 0), TRUE);
                string_chomp(new_var);

                if(!add_object_to_objects_in_pipe(new_var, runinfo, runinfo->mCommand))
                {
                    return FALSE;
                }

                char buf[128];
                int size = snprintf(buf, 128, "%p", new_var);
                if(!fd_write(nextout, buf, size)) {
                    err_msg("signal interrupt", runinfo->mSName, runinfo->mSLine);
                    runinfo->mRCode = RCODE_SIGNAL_INTERRUPT;
                    return FALSE;
                }
                if(!fd_write(nextout, "\n", 1)) {
                    err_msg("signal interrupt", runinfo->mSName, runinfo->mSLine);
                    runinfo->mRCode = RCODE_SIGNAL_INTERRUPT;
                    return FALSE;
                }
            }
            else {
                sObject* new_var = STRING_NEW_GC("", TRUE);

                if(!add_object_to_objects_in_pipe(new_var, runinfo, runinfo->mCommand))
                {
                    return FALSE;
                }

                char buf[128];
                int size = snprintf(buf, 128, "%p", new_var);
                if(!fd_write(nextout, buf, size)) {
                    err_msg("signal interrupt", runinfo->mSName, runinfo->mSLine);
                    runinfo->mRCode = RCODE_SIGNAL_INTERRUPT;
                    return FALSE;
                }
                if(!fd_write(nextout, "\n", 1)) {
                    err_msg("signal interrupt", runinfo->mSName, runinfo->mSLine);
                    runinfo->mRCode = RCODE_SIGNAL_INTERRUPT;
                    return FALSE;
                }
            }

            if(sRunInfo_option(runinfo, "-shift")) {
                int i;
                for(i=1;i<max; i++) {
                    char* str = vector_item(SFD(nextin).mLines, i);
                    if(!fd_write(nextout, str, strlen(str))) {
                        err_msg("signal interrupt", runinfo->mSName, runinfo->mSLine);
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
        else if(runinfo->mArgsNumRuntime > 1) {
            int i;
            int max = vector_count(SFD(nextin).mLines);
            for(i=1; i<runinfo->mArgsNumRuntime; i++) {
                if(i-1 < max) {
                    sObject* new_var = STRING_NEW_GC(vector_item(SFD(nextin).mLines, i-1), TRUE);
                    string_chomp(new_var);

                    if(sRunInfo_option(runinfo, "-local")) {
                        uobject_put(SFUN(runinfo->mRunningObject).mLocalObjects, runinfo->mArgsRuntime[i], new_var);
                    }
                    else {
                        sObject* object;
                        sObject* name = STRING_NEW_STACK("");
                        if(!get_object_prefix_and_name_from_argument(&object, name, runinfo->mArgsRuntime[i], runinfo->mCurrentObject, runinfo->mRunningObject, runinfo)) {
                            return FALSE;
                        }

                        uobject_put(object, string_c_str(name), new_var);
                    }
                }
                else {
                    sObject* new_var = STRING_NEW_GC("", TRUE);

                    if(sRunInfo_option(runinfo, "-local")) {
                        uobject_put(SFUN(runinfo->mRunningObject).mLocalObjects, runinfo->mArgsRuntime[i], new_var);
                    }
                    else {
                        sObject* object;
                        sObject* name = STRING_NEW_STACK("");
                        if(!get_object_prefix_and_name_from_argument(&object, name, runinfo->mArgsRuntime[i], runinfo->mCurrentObject, runinfo->mRunningObject, runinfo)) {
                            return FALSE;
                        }

                        uobject_put(object, string_c_str(name), new_var);
                    }
                }
            }

            if(sRunInfo_option(runinfo, "-shift")) {
                for(;i-1<max; i++) {
                    char* str = vector_item(SFD(nextin).mLines, i-1);
                    if(!fd_write(nextout, str, strlen(str))) {
                        err_msg("signal interrupt", runinfo->mSName, runinfo->mSLine);
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
    /// output ///
    else {
        if(runinfo->mArgsNumRuntime > 1) {
            int i;
            for(i=1; i<runinfo->mArgsNumRuntime; i++) {
                sObject* var;
                if(!get_object_from_argument(&var, runinfo->mArgsRuntime[i], runinfo->mCurrentObject, runinfo->mRunningObject, runinfo)) {
                    return FALSE;
                }

                if(var && STYPE(var) == T_STRING) {
                    if(!fd_write(nextout, string_c_str(var), string_length(var))) {
                        err_msg("signal interrupt", runinfo->mSName, runinfo->mSLine);
                        runinfo->mRCode = RCODE_SIGNAL_INTERRUPT;
                        return FALSE;
                    }
                    if(!fd_write(nextout, field, strlen(field))) {
                        err_msg("signal interrupt", runinfo->mSName, runinfo->mSLine);
                        runinfo->mRCode = RCODE_SIGNAL_INTERRUPT;
                        return FALSE;
                    }
                    runinfo->mRCode = 0;
                }
                else {
                    err_msg("not found variable", runinfo->mSName, runinfo->mSLine);
                    return FALSE;
                }
            }
        }
    }

    return TRUE;
}

BOOL cmd_ary(sObject* nextin, sObject* nextout, sRunInfo* runinfo)
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

    /// size
    if(sRunInfo_option(runinfo, "-size")) {
        if(runinfo->mArgsNumRuntime > 1) {
            sObject* object = runinfo->mCurrentObject;
            int i;
            for(i=1; i<runinfo->mArgsNumRuntime; i++) {
                sObject* ary;
                if(!get_object_from_argument(&ary, runinfo->mArgsRuntime[i], runinfo->mCurrentObject, runinfo->mRunningObject, runinfo)) {
                    return FALSE;
                }

                if(ary && STYPE(ary) == T_VECTOR) {
                    char buf[128];
                    int size = snprintf(buf, 128, "%d\n", vector_count(ary));

                    if(!fd_write(nextout, buf, size)) {
                        err_msg("signal interrupt", runinfo->mSName, runinfo->mSLine);
                        runinfo->mRCode = RCODE_SIGNAL_INTERRUPT;
                        return FALSE;
                    }

                    runinfo->mRCode = 0;
                }
                else {
                    err_msg("not found variable", runinfo->mSName, runinfo->mSLine);
                    return FALSE;
                }
            }
        }
    }
    /// output
    else if(!runinfo->mFilter && runinfo->mArgsNumRuntime > 1) {
        /// index
        char* argument;
        if(argument = sRunInfo_option_with_argument(runinfo, "-index")) {
            sObject* object = runinfo->mCurrentObject;
            int i;
            for(i=1; i<runinfo->mArgsNumRuntime; i++) {
                sObject* ary;
                if(!get_object_from_argument(&ary, runinfo->mArgsRuntime[i], runinfo->mCurrentObject, runinfo->mRunningObject, runinfo)) {
                    return FALSE;
                }

                if(ary && STYPE(ary) == T_VECTOR) {
                    if(vector_count(ary) > 0) {
                        int num = atoi(argument);
                        if(num < 0) {
                            num += vector_count(ary);
                        }

                        if(num >= 0 && num < vector_count(ary)) {
                            sObject* var = vector_item(ary, num);

                            if(!fd_write(nextout, string_c_str(var), string_length(var))) {
                                err_msg("signal interrupt", runinfo->mSName, runinfo->mSLine);
                                runinfo->mRCode = RCODE_SIGNAL_INTERRUPT;
                                return FALSE;
                            }
                        }
                        if(!fd_write(nextout, field, strlen(field))) {
                            err_msg("signal interrupt", runinfo->mSName, runinfo->mSLine);
                            runinfo->mRCode = RCODE_SIGNAL_INTERRUPT;
                            return FALSE;
                        }
                    }

                    runinfo->mRCode = 0;
                }
                else {
                    err_msg("not found variable", runinfo->mSName, runinfo->mSLine);
                    return FALSE;
                }
            }
        }
        else {
            sObject* object = runinfo->mCurrentObject;
            int i;
            for(i=1; i<runinfo->mArgsNumRuntime; i++) {
                sObject* ary;
                if(!get_object_from_argument(&ary, runinfo->mArgsRuntime[i], runinfo->mCurrentObject, runinfo->mRunningObject, runinfo)) {
                    return FALSE;
                }

                if(ary && STYPE(ary) == T_VECTOR) {
                    int j;
                    for(j=0; j<vector_count(ary); j++) {
                        sObject* var = vector_item(ary, j);

                        if(!fd_write(nextout, string_c_str(var), string_length(var))) {
                            err_msg("signal interrupt", runinfo->mSName, runinfo->mSLine);
                            runinfo->mRCode = RCODE_SIGNAL_INTERRUPT;
                            return FALSE;
                        }

                        if(!fd_write(nextout, field, strlen(field))) {
                            err_msg("signal interrupt", runinfo->mSName, runinfo->mSLine);
                            runinfo->mRCode = RCODE_SIGNAL_INTERRUPT;
                            return FALSE;
                        }
                    }
                    runinfo->mRCode = 0;
                }
                else {
                    err_msg("not found variable", runinfo->mSName, runinfo->mSLine);
                    return FALSE;
                }
            }
        }
    }
    /// input
    else if(runinfo->mFilter) {
        fd_split(nextin, lf, TRUE, FALSE, TRUE);

        /// -new
        if(sRunInfo_option(runinfo, "-new")) {
            int i;
            int max = vector_count(SFD(nextin).mLines);
            sObject* new_ary = VECTOR_NEW_GC(16, TRUE);

            if(!add_object_to_objects_in_pipe(new_ary, runinfo, runinfo->mCommand))
            {
                return FALSE;
            }

            char buf[128];
            int size = snprintf(buf, 128, "%p", new_ary);
            if(!fd_write(nextout, buf, size)) {
                err_msg("signal interrupt", runinfo->mSName, runinfo->mSLine);
                runinfo->mRCode = RCODE_SIGNAL_INTERRUPT;
                return FALSE;
            }
            if(!fd_write(nextout, "\n", 1)) {
                err_msg("signal interrupt", runinfo->mSName, runinfo->mSLine);
                runinfo->mRCode = RCODE_SIGNAL_INTERRUPT;
                return FALSE;
            }

            for(i=0; i<max; i++) {
                sObject* new_var = STRING_NEW_GC(vector_item(SFD(nextin).mLines, i), FALSE);
                vector_add(new_ary, new_var);
            }
            
            if(SFD(nextin).mBufLen == 0) {
                runinfo->mRCode = RCODE_NFUN_NULL_INPUT;
            }
            else {
                runinfo->mRCode = 0;
            }
        }
        else if(runinfo->mArgsNumRuntime == 2) {
            /// number
            char* argument = sRunInfo_option_with_argument(runinfo, "-insert");
            if(argument != NULL || sRunInfo_option(runinfo, "-append")) {
                sObject* ary;
                if(sRunInfo_option(runinfo, "-local")) {
                    ary = uobject_item(SFUN(runinfo->mRunningObject).mLocalObjects, runinfo->mArgsRuntime[1]);
                    if(ary == NULL || STYPE(ary) != T_VECTOR) {
                        uobject_put(SFUN(runinfo->mRunningObject).mLocalObjects, runinfo->mArgsRuntime[1], ary = VECTOR_NEW_GC(16, TRUE));
                    }
                }
                else {
                    sObject* object;
                    sObject* name = STRING_NEW_STACK("");
                    if(!get_object_prefix_and_name_from_argument(&object, name, runinfo->mArgsRuntime[1], runinfo->mCurrentObject, runinfo->mRunningObject, runinfo)) {
                        return FALSE;
                    }

                    ary = uobject_item(object, string_c_str(name));
                    if(ary == NULL || STYPE(ary) != T_VECTOR) {
                        if(object == runinfo->mCurrentObject) {
                            uobject_put(runinfo->mCurrentObject, runinfo->mArgsRuntime[1], ary = VECTOR_NEW_GC(16, TRUE));
                        }
                        else {
                            err_msg("not found the array", runinfo->mSName, runinfo->mSLine);
                            return FALSE;
                        }
                    }
                }

                int append;
                if(sRunInfo_option(runinfo, "-append")) {
                    append = -1;
                }
                else {
                    append = atoi(argument);
                }

                if(append < 0) append += vector_count(ary) + 1;
                if(append < 0) append = 0;
                if(append > vector_count(ary)) append = vector_count(ary);

                int i;
                int max = vector_count(SFD(nextin).mLines);
                if(max == 0) {
                    sObject* new_var = STRING_NEW_GC("", FALSE);
                    vector_insert(ary, append, new_var);
                }
                else {
                    void** appended_ary = MALLOC(sizeof(sObject*)*max);
                    for(i=0; i<max; i++) {
                        sObject* new_var = STRING_NEW_GC(vector_item(SFD(nextin).mLines, i), FALSE);
                        appended_ary[i] = new_var;
                    }
                    vector_mass_insert(ary, append, appended_ary, max);
                    FREE(appended_ary);
                }
                
                if(SFD(nextin).mBufLen == 0) {
                    runinfo->mRCode = RCODE_NFUN_NULL_INPUT;
                }
                else {
                    runinfo->mRCode = 0;
                }
            }
            else {
                int i;
                int max = vector_count(SFD(nextin).mLines);
                sObject* new_ary = VECTOR_NEW_GC(16, TRUE);

                if(sRunInfo_option(runinfo, "-local")) {
                    sObject* object = SFUN(runinfo->mRunningObject).mLocalObjects;
                    uobject_put(object, runinfo->mArgsRuntime[1], new_ary);
                }
                else {
                    sObject* object;
                    sObject* name = STRING_NEW_STACK("");
                    if(!get_object_prefix_and_name_from_argument(&object, name, runinfo->mArgsRuntime[1], runinfo->mCurrentObject, runinfo->mRunningObject, runinfo)) {
                        return FALSE;
                    }

                    uobject_put(object, string_c_str(name), new_ary);
                }

                for(i=0; i<max; i++) {
                    sObject* new_var = STRING_NEW_GC(vector_item(SFD(nextin).mLines, i), FALSE);
                    vector_add(new_ary, new_var);
                }
                
                if(SFD(nextin).mBufLen == 0) {
                    runinfo->mRCode = RCODE_NFUN_NULL_INPUT;
                }
                else {
                    runinfo->mRCode = 0;
                }
            }
        }
    }

    return TRUE;
}

BOOL cmd_hash(sObject* nextin, sObject* nextout, sRunInfo* runinfo)
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

    /// size
    if(sRunInfo_option(runinfo, "-size")) {
        if(runinfo->mArgsNumRuntime > 1) {
            sObject* object = runinfo->mCurrentObject;
            int i;
            for(i=1; i<runinfo->mArgsNumRuntime; i++) {
                sObject* hash;
                if(!get_object_from_argument(&hash, runinfo->mArgsRuntime[i], runinfo->mCurrentObject, runinfo->mRunningObject, runinfo)) {
                    return FALSE;
                }

                if(hash && STYPE(hash) == T_HASH) {
                    char buf[128];
                    int size = snprintf(buf, 128, "%d\n", hash_count(hash));

                    if(!fd_write(nextout, buf, size)) {
                        err_msg("signal interrupt", runinfo->mSName, runinfo->mSLine);
                        runinfo->mRCode = RCODE_SIGNAL_INTERRUPT;
                        return FALSE;
                    }

                    runinfo->mRCode = 0;
                }
                else {
                    err_msg("not found variable", runinfo->mSName, runinfo->mSLine);
                    return FALSE;
                }
            }
        }
    }
    /// output
    else if(!runinfo->mFilter && runinfo->mArgsNumRuntime > 1) {
        /// key
        char* argument;
        if(argument = sRunInfo_option_with_argument(runinfo, "-key")) {
            sObject* object = runinfo->mCurrentObject;
            int i;
            for(i=1; i<runinfo->mArgsNumRuntime; i++) {
                sObject* hash;
                if(!get_object_from_argument(&hash, runinfo->mArgsRuntime[i], runinfo->mCurrentObject, runinfo->mRunningObject, runinfo)) {
                    return FALSE;
                }

                if(hash && STYPE(hash) == T_HASH) {
                    sObject* var = hash_item(hash, argument);

                    if(var) {
                        if(!fd_write(nextout, string_c_str(var), string_length(var))) {
                            err_msg("signal interrupt", runinfo->mSName, runinfo->mSLine);
                            runinfo->mRCode = RCODE_SIGNAL_INTERRUPT;
                            return FALSE;
                        }
                    }

                    if(!fd_write(nextout, field, strlen(field))) {
                        err_msg("signal interrupt", runinfo->mSName, runinfo->mSLine);
                        runinfo->mRCode = RCODE_SIGNAL_INTERRUPT;
                        return FALSE;
                    }

                    runinfo->mRCode = 0;
                }
                else {
                    err_msg("not found variable", runinfo->mSName, runinfo->mSLine);
                    return FALSE;
                }
            }
        }
        else {
            sObject* object = runinfo->mCurrentObject;
            int i;
            for(i=1; i<runinfo->mArgsNumRuntime; i++) {
                sObject* hash;
                if(!get_object_from_argument(&hash, runinfo->mArgsRuntime[i], runinfo->mCurrentObject, runinfo->mRunningObject, runinfo)) {
                    return FALSE;
                }
                if(hash && STYPE(hash) == T_HASH) {
                    hash_it* it = hash_loop_begin(hash);
                    while(it) {
                        char* key = hash_loop_key(it);
                        sObject* var = hash_loop_item(it);

                        if(!fd_write(nextout, key, strlen(key))) {
                            err_msg("signal interrupt", runinfo->mSName, runinfo->mSLine);
                            runinfo->mRCode = RCODE_SIGNAL_INTERRUPT;
                            return FALSE;
                        }
                        if(!fd_write(nextout, field, strlen(field))) {
                            err_msg("signal interrupt", runinfo->mSName, runinfo->mSLine);
                            runinfo->mRCode = RCODE_SIGNAL_INTERRUPT;
                            return FALSE;
                        }
                        if(!fd_write(nextout, string_c_str(var), string_length(var))) {
                            err_msg("signal interrupt", runinfo->mSName, runinfo->mSLine);
                            runinfo->mRCode = RCODE_SIGNAL_INTERRUPT;
                            return FALSE;
                        }
                        if(!fd_write(nextout, field, strlen(field))) {
                            err_msg("signal interrupt", runinfo->mSName, runinfo->mSLine);
                            runinfo->mRCode = RCODE_SIGNAL_INTERRUPT;
                            return FALSE;
                        }

                        it = hash_loop_next(it);
                    }
                    runinfo->mRCode = 0;
                }
                else {
                    err_msg("not found variable", runinfo->mSName, runinfo->mSLine);
                    return FALSE;
                }
            }
        }
    }
    /// input ///
    else if(runinfo->mFilter) {
        fd_split(nextin, lf, TRUE, FALSE, TRUE);

        if(sRunInfo_option(runinfo, "-new")) {
            int i;
            sObject* new_hash = HASH_NEW_GC(16, TRUE);

            if(!add_object_to_objects_in_pipe(new_hash, runinfo, runinfo->mCommand))
            {
                return FALSE;
            }

            char buf[128];
            int size = snprintf(buf, 128, "%p", new_hash);
            if(!fd_write(nextout, buf, size)) {
                err_msg("signal interrupt", runinfo->mSName, runinfo->mSLine);
                runinfo->mRCode = RCODE_SIGNAL_INTERRUPT;
                return FALSE;
            }
            if(!fd_write(nextout, "\n", 1)) {
                err_msg("signal interrupt", runinfo->mSName, runinfo->mSLine);
                runinfo->mRCode = RCODE_SIGNAL_INTERRUPT;
                return FALSE;
            }

            sObject* key = STRING_NEW_STACK("");
            int max = vector_count(SFD(nextin).mLines);
            for(i=0; i<max; i++) {
                if((i % 2) == 0) {
                    string_put(key, vector_item(SFD(nextin).mLines, i));
                }
                else {
                    sObject* new_var = STRING_NEW_GC(vector_item(SFD(nextin).mLines, i), FALSE);
                    hash_put(new_hash, string_c_str(key), new_var);
                }
            }
            if((i % 2) == 1) {
                hash_put(new_hash, string_c_str(key), STRING_NEW_GC("", FALSE));
            }

            if(SFD(nextin).mBufLen == 0) {
                runinfo->mRCode = RCODE_NFUN_NULL_INPUT;
            }
            else {
                runinfo->mRCode = 0;
            }
        }
        else if(runinfo->mArgsNumRuntime == 2) {
            /// number
            if(sRunInfo_option(runinfo, "-append")) {
                sObject* hash;
                if(sRunInfo_option(runinfo, "-local")) {
                    hash = uobject_item(SFUN(runinfo->mRunningObject).mLocalObjects, runinfo->mArgsRuntime[1]);

                    if(hash == NULL || STYPE(hash) != T_HASH) {
                        uobject_put(SFUN(runinfo->mRunningObject).mLocalObjects, runinfo->mArgsRuntime[1], hash = HASH_NEW_GC(16, TRUE));
                    }
                }
                else {
                    sObject* object;
                    sObject* name = STRING_NEW_STACK("");
                    if(!get_object_prefix_and_name_from_argument(&object, name, runinfo->mArgsRuntime[1], runinfo->mCurrentObject, runinfo->mRunningObject, runinfo)) {
                        return FALSE;
                    }

                    hash = uobject_item(object, string_c_str(name));

                    if(hash == NULL || STYPE(hash) != T_HASH) {
                        if(object == runinfo->mCurrentObject) {
                            uobject_put(runinfo->mCurrentObject, runinfo->mArgsRuntime[1], hash = HASH_NEW_GC(16, TRUE));
                        }
                        else {
                            err_msg("not found the hash", runinfo->mSName, runinfo->mSLine);
                            return FALSE;
                        }
                    }
                }

                int i;
                int max = vector_count(SFD(nextin).mLines);
                sObject* key = STRING_NEW_STACK("");
                for(i=0; i<max; i++) {
                    if((i % 2) == 0) {
                        string_put(key, vector_item(SFD(nextin).mLines, i));
                    }
                    else {
                        sObject* new_var = STRING_NEW_GC(vector_item(SFD(nextin).mLines, i), FALSE);
                        hash_put(hash, string_c_str(key), new_var);
                    }
                }
                if((i % 2) == 1) {
                    hash_put(hash, string_c_str(key), STRING_NEW_GC("", FALSE));
                }
                    
                if(SFD(nextin).mBufLen == 0) {
                    runinfo->mRCode = RCODE_NFUN_NULL_INPUT;
                }
                else {
                    runinfo->mRCode = 0;
                }
            }
            else {
                int i;
                int max = vector_count(SFD(nextin).mLines);
                sObject* new_hash = HASH_NEW_GC(16, TRUE);

                if(sRunInfo_option(runinfo, "-local")) {
                    sObject* object = SFUN(runinfo->mRunningObject).mLocalObjects;
                    uobject_put(object, runinfo->mArgsRuntime[1], new_hash);
                }
                else {
                    sObject* object;
                    sObject* name = STRING_NEW_STACK("");
                    if(!get_object_prefix_and_name_from_argument(&object, name, runinfo->mArgsRuntime[1], runinfo->mCurrentObject, runinfo->mRunningObject, runinfo)) {
                        return FALSE;
                    }

                    uobject_put(object, string_c_str(name), new_hash);
                }

                sObject* key = STRING_NEW_STACK("");
                for(i=0; i<max; i++) {
                    if((i % 2) == 0) {
                        string_put(key, vector_item(SFD(nextin).mLines, i));
                    }
                    else {
                        sObject* new_var = STRING_NEW_GC(vector_item(SFD(nextin).mLines, i), FALSE);
                        hash_put(new_hash, string_c_str(key), new_var);
                    }
                }
                if((i % 2) == 1) {
                    hash_put(new_hash, string_c_str(key), STRING_NEW_GC("", FALSE));
                }
                    
                if(SFD(nextin).mBufLen == 0) {
                    runinfo->mRCode = RCODE_NFUN_NULL_INPUT;
                }
                else {
                    runinfo->mRCode = 0;
                }
            }
        }
    }

    return TRUE;
}


BOOL cmd_object(sObject* nextin, sObject* nextout, sRunInfo* runinfo)
{
    if(sRunInfo_option(runinfo, "-new")) {
        if(runinfo->mBlocksNum >= 1) {
            sObject* object = runinfo->mCurrentObject;

            sObject* new_object = UOBJECT_NEW_GC(8, object, "anonymous", TRUE);
            uobject_init(new_object);

            int rcode = 0;
            if(!run(runinfo->mBlocks[0], nextin, nextout, &rcode, new_object, runinfo->mRunningObject)) {
                runinfo->mRCode = rcode;
                return FALSE;
            }

            if(!add_object_to_objects_in_pipe(new_object, runinfo, runinfo->mCommand))
            {
                return FALSE;
            }

            char buf[128];
            int size = snprintf(buf, 128, "%p", new_object);
            if(!fd_write(nextout, buf, size)) {
                err_msg("signal interrupt", runinfo->mSName, runinfo->mSLine);
                runinfo->mRCode = RCODE_SIGNAL_INTERRUPT;
                return FALSE;
            }
            if(!fd_write(nextout, "\n", 1)) {
                err_msg("signal interrupt", runinfo->mSName, runinfo->mSLine);
                runinfo->mRCode = RCODE_SIGNAL_INTERRUPT;
                return FALSE;
            }

            runinfo->mRCode = rcode;
        }
        else {
            sObject* object = runinfo->mCurrentObject;

            sObject* new_object = UOBJECT_NEW_GC(8, object, "anonymous", TRUE);
            uobject_init(new_object);

            if(!add_object_to_objects_in_pipe(new_object, runinfo, runinfo->mCommand))
            {
                return FALSE;
            }

            char buf[128];
            int size = snprintf(buf, 128, "%p", new_object);
            if(!fd_write(nextout, buf, size)) {
                err_msg("signal interrupt", runinfo->mSName, runinfo->mSLine);
                runinfo->mRCode = RCODE_SIGNAL_INTERRUPT;
                return FALSE;
            }
            if(!fd_write(nextout, "\n", 1)) {
                err_msg("signal interrupt", runinfo->mSName, runinfo->mSLine);
                runinfo->mRCode = RCODE_SIGNAL_INTERRUPT;
                return FALSE;
            }

            runinfo->mRCode = 0;
        }
    }
    else if(runinfo->mArgsNumRuntime > 1) {
        if(runinfo->mBlocksNum >= 1) {
            sObject* parent_object = runinfo->mCurrentObject;
            int i;
            for(i=1; i<runinfo->mArgsNumRuntime; i++) {
                sObject* new_object = UOBJECT_NEW_GC(8, parent_object, runinfo->mArgsRuntime[i], TRUE);

                if(sRunInfo_option(runinfo, "-local")) {
                    sObject* object = SFUN(runinfo->mRunningObject).mLocalObjects;
                    uobject_put(object, runinfo->mArgsRuntime[i], new_object);
                }
                else {
                    sObject* object;
                    sObject* name = STRING_NEW_STACK("");
                    if(!get_object_prefix_and_name_from_argument(&object, name, runinfo->mArgsRuntime[i], runinfo->mCurrentObject, runinfo->mRunningObject, runinfo)) {
                        return FALSE;
                    }

                    uobject_put(object, string_c_str(name), new_object);
                }

                uobject_init(new_object);

                int rcode = 0;
                if(!run(runinfo->mBlocks[0], nextin, nextout, &rcode, new_object, runinfo->mRunningObject)) {
                    runinfo->mRCode = rcode;
                    return FALSE;
                }

                runinfo->mRCode = rcode;
            }
        }
        else {
            sObject* parent_object = runinfo->mCurrentObject;

            int i;
            for(i=1; i<runinfo->mArgsNumRuntime; i++) {
                sObject* new_object = UOBJECT_NEW_GC(8, parent_object, runinfo->mArgsRuntime[i], TRUE);

                if(sRunInfo_option(runinfo, "-local")) {
                    sObject* object = SFUN(runinfo->mRunningObject).mLocalObjects;
                    uobject_put(object, runinfo->mArgsRuntime[i], new_object);
                }
                else {
                    sObject* object;
                    sObject* name = STRING_NEW_STACK("");
                    if(!get_object_prefix_and_name_from_argument(&object, name, runinfo->mArgsRuntime[i], runinfo->mCurrentObject, runinfo->mRunningObject, runinfo)) {
                        return FALSE;
                    }

                    uobject_put(object, string_c_str(name), new_object);
                }

                uobject_init(new_object);
                runinfo->mRCode = 0;
            }
        }
    }

    return TRUE;
}

BOOL cmd_alias(sObject* nextin, sObject* nextout, sRunInfo* runinfo)
{
    if(runinfo->mArgsNumRuntime == 2) {
        if(runinfo->mBlocksNum == 1) {
            char* name = runinfo->mArgsRuntime[1];
            sObject* block = runinfo->mBlocks[0];

            if(sRunInfo_option(runinfo, "-local")) {
                sObject* object = SFUN(runinfo->mRunningObject).mLocalObjects;
                sObject* new_alias = ALIAS_NEW_GC(block, TRUE);
                uobject_put(object, name, new_alias);
            }
            else {
                sObject* object;
                sObject* name2 = STRING_NEW_STACK("");
                if(!get_object_prefix_and_name_from_argument(&object, name2, name, runinfo->mCurrentObject, runinfo->mRunningObject, runinfo)) {
                    return FALSE;
                }

                sObject* new_alias = ALIAS_NEW_GC(block, TRUE);

                uobject_put(object, string_c_str(name2), new_alias);
            }
        }
        else {
            /// input ///
            if(runinfo->mFilter) {
                char* name = runinfo->mArgsRuntime[1];
                char* body = SFD(nextin).mBuf;

                sObject* block = BLOCK_NEW_STACK();

                int sline = 1;
                if(!parse(body, "alias", &sline, block, NULL)) {
                    return FALSE;
                }

                if(sRunInfo_option(runinfo, "-local")) {
                    sObject* object = SFUN(runinfo->mRunningObject).mLocalObjects;
                    sObject* new_alias = ALIAS_NEW_GC(block, TRUE);
                    uobject_put(object, name, new_alias);
                }
                else {
                    sObject* object;
                    sObject* name2 = STRING_NEW_STACK("");
                    if(!get_object_prefix_and_name_from_argument(&object, name2, name, runinfo->mCurrentObject, runinfo->mRunningObject, runinfo)) {
                        return FALSE;
                    }

                    sObject* new_alias = ALIAS_NEW_GC(block, TRUE);
                    uobject_put(object, string_c_str(name2), new_alias);
                }

                FREE(name);
            }
            /// output ///
            else {
                sObject* alias;
                if(!get_object_from_argument(&alias, runinfo->mArgsRuntime[1], runinfo->mCurrentObject, runinfo->mRunningObject, runinfo)) {
                    return FALSE;
                }

                if(alias && STYPE(alias) == T_ALIAS) {
                    sObject* block = SALIAS(alias).mBlock;
                    char* source = SBLOCK(block).mSource;
                    if(!fd_write(nextout, source, strlen(source))) {
                        err_msg("signal interrupt", runinfo->mSName, runinfo->mSLine);
                        runinfo->mRCode = RCODE_SIGNAL_INTERRUPT;
                        return FALSE;
                    }
                    if(!fd_write(nextout, "\n", 1)) {
                        err_msg("signal interrupt", runinfo->mSName, runinfo->mSLine);
                        runinfo->mRCode = RCODE_SIGNAL_INTERRUPT;
                        return FALSE;
                    }
                }
                else {
                    err_msg("not found variable", runinfo->mSName, runinfo->mSLine);
                    return FALSE;
                }
            }
        }

        runinfo->mRCode = 0;
    }

    return TRUE;
}

BOOL cmd_ref(sObject* nextin, sObject* nextout, sRunInfo* runinfo)
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

    /// output
    if(!runinfo->mFilter) {
        if(runinfo->mArgsNumRuntime > 1) {
            /// type
            if(sRunInfo_option(runinfo, "-type")) {
                int i;
                int max = runinfo->mArgsNumRuntime;
                for(i=1; i<max; i++) {
                    sObject* item;
                    if(!get_object_from_argument(&item, runinfo->mArgsRuntime[i], runinfo->mCurrentObject, runinfo->mRunningObject, runinfo)) {
                        return FALSE;
                    }

                    if(item) {
                        int type = STYPE(item);

                        char buf[128];
                        char* obj_kind[T_TYPE_MAX] = {
                            NULL, "var", "array", "hash", "list", "native function", "block", "file dicriptor", "file dicriptor", "job", "object", "function", "class", "external program", "completion", "external object", "alias", "int"
                        };
                        int size = snprintf(buf, 128, "%s\n", obj_kind[type]);

                        if(!fd_write(nextout, buf, size)) {
                            err_msg("signal interrupt", runinfo->mSName, runinfo->mSLine);
                            runinfo->mRCode = RCODE_SIGNAL_INTERRUPT;
                            return FALSE;
                        }
                    }
                }

                runinfo->mRCode = 0;
            }
            else {
                int i;
                for(i=1; i<runinfo->mArgsNumRuntime; i++) {
                    sObject* item;
                    if(!get_object_from_argument(&item, runinfo->mArgsRuntime[i], runinfo->mCurrentObject, runinfo->mRunningObject, runinfo)) {
                        return FALSE;
                    }

                    if(item) {
                        if(!add_object_to_objects_in_pipe(item, runinfo, runinfo->mCommand))
                        {
                            return FALSE;
                        }

                        char buf[128];
                        int size = snprintf(buf, 128, "%p", item);
                        if(!fd_write(nextout, buf, size)) {
                            err_msg("signal interrupt", runinfo->mSName, runinfo->mSLine);
                            runinfo->mRCode = RCODE_SIGNAL_INTERRUPT;
                            return FALSE;
                        }
                        if(!fd_write(nextout, field, strlen(field))) {
                            err_msg("signal interrupt", runinfo->mSName, runinfo->mSLine);
                            runinfo->mRCode = RCODE_SIGNAL_INTERRUPT;
                            return FALSE;
                        }
                        runinfo->mRCode = 0;
                    }
                    else {
                        err_msg("not found variable", runinfo->mSName, runinfo->mSLine);
                        return FALSE;
                    }
                }
            }
        }
    }
    /// input
    else {
        fd_split(nextin, lf, TRUE, FALSE, TRUE);

        /// type
        if(sRunInfo_option(runinfo, "-type")) {
            int i;
            int max = vector_count(SFD(nextin).mLines);
            for(i=0; i<max; i++) {
                char* line = vector_item(SFD(nextin).mLines, i);
                void* mem = (void*)(unsigned long)strtoll(line, NULL, 16);
                if(mem == NULL || !gc_valid_object(mem) || !contained_in_pipe(mem)) {
                    err_msg("invalid address. it's not memory managed by a xyzsh gabage collecter or it doesn't exist in pipe data", runinfo->mSName, runinfo->mSLine);
                    return FALSE;
                }

                sObject* obj = mem;
                int type = STYPE(obj);

                char buf[128];
                char* obj_kind[T_TYPE_MAX] = {
                    NULL, "var", "array", "hash", "list", "native function", "block", "file dicriptor", "file dicriptor", "job", "object", "function", "class", "external program", "completion", "external object", "alias", "int"
                };
                int size = snprintf(buf, 128, "%s\n", obj_kind[type]);

                if(!fd_write(nextout, buf, size)) {
                    err_msg("signal interrupt", runinfo->mSName, runinfo->mSLine);
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
        else {
            if(runinfo->mArgsNumRuntime > 1) {
                int i;
                int max = vector_count(SFD(nextin).mLines);
                BOOL shift = sRunInfo_option(runinfo, "-shift");
                for(i=0; i<max; i++) {
                    if(i+1 < runinfo->mArgsNumRuntime) {
                        char* line = vector_item(SFD(nextin).mLines, i);
                        void* mem = (void*)(unsigned long)strtoll(line, NULL, 16);
                        if(mem == NULL || !gc_valid_object(mem) || !contained_in_pipe(mem)) {
                            err_msg("invalid address. it's not memory managed by a xyzsh gabage collecter or it doesn't exist in pipe data", runinfo->mSName, runinfo->mSLine);
                            return FALSE;
                        }

                        if(sRunInfo_option(runinfo, "-local")) {
                            sObject* object = SFUN(runinfo->mRunningObject).mLocalObjects;
                            uobject_put(object, runinfo->mArgsRuntime[i+1], mem);
                        }
                        else {
                            sObject* object;
                            sObject* name = STRING_NEW_STACK("");
                            if(!get_object_prefix_and_name_from_argument(&object, name, runinfo->mArgsRuntime[i+1], runinfo->mCurrentObject, runinfo->mRunningObject, runinfo)) {
                                return FALSE;
                            }

                            uobject_put(object, string_c_str(name), mem);
                        }
                    }
                    else if(shift) {
                        char* str = vector_item(SFD(nextin).mLines, i);
                        if(!fd_write(nextout, str, strlen(str))) {
                            err_msg("signal interrupt", runinfo->mSName, runinfo->mSLine);
                            runinfo->mRCode = RCODE_SIGNAL_INTERRUPT;
                            return FALSE;
                        }
                    }
                    else {
                        break;
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
    }

    return TRUE;
}


BOOL cmd_export(sObject* nextin, sObject* nextout, sRunInfo* runinfo)
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

    /// output
    if(!runinfo->mFilter) {
        if(runinfo->mArgsNumRuntime > 1) {
            sObject* object = runinfo->mCurrentObject;
            int i;
            for(i=1; i<runinfo->mArgsNumRuntime; i++) {
                char* env = getenv(runinfo->mArgsRuntime[i]);
                if(env) {
                    if(!fd_write(nextout, env, strlen(env))) {
                        err_msg("signal interrupt", runinfo->mSName, runinfo->mSLine);
                        runinfo->mRCode = RCODE_SIGNAL_INTERRUPT;
                        return FALSE;
                    }
                    if(!fd_write(nextout, field, strlen(field))) {
                        err_msg("signal interrupt", runinfo->mSName, runinfo->mSLine);
                        runinfo->mRCode = RCODE_SIGNAL_INTERRUPT;
                        return FALSE;
                    }
                }

                runinfo->mRCode = 0;  // If there is not evironment, don't occure error
            }
        }
    }
    /// input
    else {
        fd_split(nextin, lf, TRUE, FALSE, FALSE);

        if(runinfo->mArgsNumRuntime > 1) {
            int i;
            int max = vector_count(SFD(nextin).mLines);
            for(i=1; i<runinfo->mArgsNumRuntime; i++) {
                if(i-1 < max) {
                    sObject* new_var = STRING_NEW_STACK(vector_item(SFD(nextin).mLines, i-1));
                    string_chomp(new_var);
                    setenv(runinfo->mArgsRuntime[i], string_c_str(new_var), 1);
                }
                else {
                    setenv(runinfo->mArgsRuntime[i], "", 1);
                }
            }

            if(sRunInfo_option(runinfo, "-shift")) {
                for(;i-1<max; i++) {
                    char* str = vector_item(SFD(nextin).mLines, i-1);
                    if(!fd_write(nextout, str, strlen(str))) {
                        err_msg("signal interrupt", runinfo->mSName, runinfo->mSLine);
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

BOOL cmd_unset(sObject* nextin, sObject* nextout, sRunInfo* runinfo)
{
    /// output
    if(!runinfo->mFilter && runinfo->mArgsNumRuntime >= 2) {
        int i;
        for(i=1; i<runinfo->mArgsNumRuntime; i++) {
            char* name = runinfo->mArgsRuntime[i];
            if(unsetenv(name) != 0) {
                err_msg("invalid env name", runinfo->mSName, runinfo->mSLine);
                return FALSE;
            }
        }

        runinfo->mRCode = 0;
    }

    return TRUE;
}

