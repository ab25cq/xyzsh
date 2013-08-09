#include "config.h"

#include <dlfcn.h>
#include <errno.h>
#include <stdlib.h>
#include <assert.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <termios.h>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <signal.h>
#include <dirent.h>
#include <limits.h>
#include "xyzsh.h"

static sObject* gDynamicLibraryFinals;

BOOL load_file(char* fname, sObject* nextin, sObject* nextout, sRunInfo* runinfo, char** argv, int argc)
{
    int fd = open(fname, O_RDONLY);
    if(fd < 0) {
        err_msg("open error", runinfo->mSName, runinfo->mSLine);
        return FALSE;
    }

    char* buf;
    int buf_len;
    int buf_size;
    if(!bufsiz_read(fd, ALLOC &buf, &buf_len, &buf_size)) {
        sCommand* command = runinfo->mCommand;
        err_msg("signal interrupt16", runinfo->mSName, runinfo->mSLine);
        runinfo->mRCode = RCODE_SIGNAL_INTERRUPT;
        close(fd);
        return FALSE;
    }
    (void)close(fd);

    stack_start_stack();
    int rcode = 0;
    sObject* block = BLOCK_NEW_STACK();
    int sline = 1;
    if(parse(buf, fname, &sline, block, NULL)) {
        FREE(buf);

        sObject* fun = FUN_NEW_STACK(NULL);
        sObject* stackframe = UOBJECT_NEW_GC(8, gXyzshObject, "_stackframe", FALSE);
        vector_add(gStackFrames, stackframe);
        //uobject_init(stackframe);
        SFUN(fun).mLocalObjects = stackframe;

        sObject* argv2 = VECTOR_NEW_GC(16, FALSE);
        int i;
        for(i=0; i<argc; i++) {
            vector_add(argv2, STRING_NEW_GC(argv[i], FALSE));
        }
        uobject_put(SFUN(fun).mLocalObjects, "ARGV", argv2);

        if(!run(block, nextin, nextout, &rcode, runinfo->mCurrentObject, fun))
        {
            if(rcode == RCODE_EXIT) {
            }
            else {
                runinfo->mRCode = rcode;

                (void)vector_pop_back(gStackFrames);
                stack_end_stack();
                return FALSE;
            }
        }
        (void)vector_pop_back(gStackFrames);
    }
    else {
        FREE(buf);
        sCommand* command = runinfo->mCommand;
        err_msg_adding("parser error", runinfo->mSName, runinfo->mSLine);
        stack_end_stack();
        return FALSE;
    }

    stack_end_stack();

    return TRUE;
}

BOOL load_so_file(char* fname, sObject* nextin, sObject* nextout, sRunInfo* runinfo)
{
    sCommand* command = runinfo->mCommand;

    char path[PATH_MAX];
    snprintf(path, PATH_MAX, "%s%s", EXTDIR, fname);

    if(access(path, R_OK) != 0) {
        char* home = getenv("HOME");
        if(home) {
            snprintf(path, PATH_MAX, "%s/.xyzsh/lib/%s", home, fname);
            if(access(path, R_OK) != 0) {
                xstrncpy(path, fname, PATH_MAX);
            }
        }
        else {
            xstrncpy(path, fname, PATH_MAX);
        }
    }

    if(hash_item(gDynamicLibraryFinals, path) == NULL) {
        void* handle = dlopen(path, RTLD_LAZY);
        if(handle == NULL) {
            err_msg((char*)dlerror(), runinfo->mSName, runinfo->mSLine);
            return FALSE;
        }

        int (*init_func)() = dlsym(handle, "dl_init");
        int (*final_func)() = dlsym(handle, "dl_final");
        if(init_func == NULL || final_func == NULL) {
            err_msg("not found dl_init or dl_final", runinfo->mSName, runinfo->mSLine);
            dlclose(handle);
            return FALSE;
        }

        if(init_func(NULL) != 0) {
            err_msg("false in initialize the dynamic library", runinfo->mSName, runinfo->mSLine);
            dlclose(handle);
            return FALSE;
        }

        hash_put(gDynamicLibraryFinals, path, final_func);

        //dlclose(handle);

        /// remove extension name ///
        char* p = path + strlen(path);

        while(p >= path) {
            if(*p == '.') {
                *p = 0;
                break;
            }
            else {
                p--;
            }
        }

        if(p == path) {
            err_msg("can't load initilize xyzsh script", runinfo->mSName, runinfo->mSLine);
            dlclose(handle);
            return FALSE;
        }

        xstrncat(path, ".xyzsh", PATH_MAX);
        if(access(path, F_OK) == 0) {
            if(!load_file(path, nextin, nextout, runinfo, NULL, 0)) {
                return FALSE;
            }
        }
    }

    return TRUE;
}

void load_init()
{
    gDynamicLibraryFinals = HASH_NEW_MALLOC(10);
}

void load_final()
{
    hash_it* it = hash_loop_begin(gDynamicLibraryFinals);
    while(it) {
        int (*func)() = hash_loop_item(it);
        if(func() != 0) {
            fprintf(stderr, "false in finalize");
        }

        it = hash_loop_next(it);
    }
    hash_delete_on_malloc(gDynamicLibraryFinals);
}

