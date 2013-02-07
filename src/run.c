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
#include "xyzsh/xyzsh.h"

enum eAppType gAppType;
sObject* gStdin;
sObject* gStdout;
sObject* gStderr;

sObject* gGlobalPipe;

static sObject* gObjectsInPipe;

static sObject* gRunningObjects;

sRunInfo* gRunInfoOfRunningObject;

sObject* gDynamicLibraryFinals;

BOOL contained_in_pipe(sObject* object)
{
    BOOL found = FALSE;
    int j;
    for(j=0; j<vector_count(gObjectsInPipe); j++) {
        if(object == vector_item(gObjectsInPipe, j)) {
            found = TRUE;
        }
    }

    return found;
}

BOOL add_object_to_objects_in_pipe(sObject* object, sRunInfo* runinfo, sCommand* command)
{
    vector_add(gObjectsInPipe, object);

    if(vector_count(gObjectsInPipe) > XYZSH_OBJECTS_IN_PIPE_MAX) {
        err_msg("overflow objects in pipe", runinfo->mSName, runinfo->mSLine, command->mArgs[0]);
        return FALSE;
    }

    return TRUE;
}

eLineField gLineField;
enum eKanjiCode gKanjiCode;

void (*xyzsh_job_done)(int job_num, char* job_title);

sObject* gJobs;

void run_init(enum eAppType app_type)
{
    gAppType = app_type;
    gStdin = FD_NEW_STACK();
    gStdout = FD_NEW_STACK();
    gStderr = FD_NEW_STACK();
    gJobs = VECTOR_NEW_GC(16, FALSE);
    uobject_put(gXyzshObject, "_jobs", gJobs);

    xyzsh_job_done = NULL;

    gLineField = kLF;
    gKanjiCode = kByte;

    gRunInfoOfRunningObject = NULL;

    gGlobalPipe = FD_NEW_STACK();

    gObjectsInPipe = VECTOR_NEW_GC(10, FALSE);
    uobject_put(gXyzshObject, "_objects_in_pipe", gObjectsInPipe);

    gRunningObjects = VECTOR_NEW_GC(10, FALSE);
    uobject_put(gXyzshObject, "_running_block", gRunningObjects);

    gDynamicLibraryFinals = HASH_NEW_MALLOC(10);
}

void run_final()
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

sObject* job_new_on_gc(char* name, pid_t pgroup, struct termios tty)
{
    sObject* self = gc_get_free_object(T_JOB, FALSE);

    SJOB(self).mName = STRDUP(name);
    SJOB(self).mPGroup = pgroup;
    SJOB(self).mTty = MALLOC(sizeof(struct termios));
    memcpy(SJOB(self).mTty, &tty, sizeof(struct termios));

    vector_add(gJobs, self);
}

void job_delete_on_gc(sObject* self)
{
    FREE(SJOB(self).mTty);
    if(SJOB(self).mName) FREE(SJOB(self).mName);
}

static BOOL nextout_reader(sObject* nextout, int* pipeoutfds, sRunInfo* runinfo, char* program)
{
    char buf[BUFSIZ+1];
    while(1) {
        if(gXyzshSigInt) {
            err_msg("signal interrupt1", runinfo->mSName, runinfo->mSLine, program);
            gXyzshSigInt = FALSE;
            runinfo->mRCode = RCODE_SIGNAL_INTERRUPT;
            return FALSE;
        }
        int size = read(pipeoutfds[0], buf, BUFSIZ);
        if(size < 0) {
            if(errno == EINTR) {
                break;
            }
            else {
                perror("read pipe");
                exit(1);
            }
        }
        else if(size == 0) {
            break;
        }

        if(!fd_write(nextout, buf, size)) {
            err_msg("signal interrupt3", runinfo->mSName, runinfo->mSLine, program);
            runinfo->mRCode = RCODE_SIGNAL_INTERRUPT;
            return FALSE;
        }
    }

    (void)close(pipeoutfds[0]);

    return TRUE;
}

static void nextin_writer(pid_t pid, sObject* nextin, int* pipeinfds, int* pipeoutfds, int* pipeerrfds, sRunInfo* runinfo, char* program, pid_t* nextin_reader_pid)
{
    int pid2 = fork();
    if(pid2 < 0) {
        perror("fork2");
        exit(1);
    }
    /// writer process ///
    if(pid2 == 0) {
        if(pipeoutfds[0] != -1) {
            close(pipeoutfds[0]);
            close(pipeoutfds[1]);
        }
        if(pipeerrfds[0] != -1) {
            close(pipeerrfds[0]);
            close(pipeerrfds[1]);
        }

        pid2 = getpid();

        if(gAppType != kATOptC) {
            (void)setpgid(pid2, pid);
        }
        (void)close(pipeinfds[0]);

        if(!bufsiz_write(pipeinfds[1], SFD(nextin).mBuf, SFD(nextin).mBufLen)) {
            if(errno != EPIPE ) {
                perror("write memory pipe");
                exit(1);
            }
        }
        (void)close(pipeinfds[1]);

        exit(0);
    }
    /// the parent process ///
    else {
        if(gAppType != kATOptC) {
            (void)setpgid(pid2, pid);
        }

        (void)close(pipeinfds[0]);
        (void)close(pipeinfds[1]);
    }

    *nextin_reader_pid = pid2;
}

static void make_job_title(sRunInfo* runinfo, char* title, int title_num)
{
    title[0] = 0;

    sStatment* statment = runinfo->mStatment;
    sCommand* command_ = runinfo->mCommand;

    int i;
    for(i=0; i<statment->mCommandsNum; i++) {
        sCommand* command = statment->mCommands + i;

        if(command == command_) {
            int j;
            for(j=0; j<runinfo->mArgsNumRuntime; j++) {
                char* arg = runinfo->mArgsRuntime[j];

                xstrncat(title, arg, title_num);
                if(j+1 < runinfo->mArgsNumRuntime) {
                    xstrncat(title, " ", title_num);
                }
            }
            break;
        }
        else {
            int j;
            for(j=0; j<command->mArgsNum; j++) {
                char* arg = command->mArgs[j];

                char buf[16];
                buf[0] = PARSER_MAGIC_NUMBER_ENV;
                buf[1] = 0;

                if(arg[0] == PARSER_MAGIC_NUMBER_OPTION || strstr(arg, buf)) {
                    if(j+1 < command->mArgsNum) {
                        xstrncat(title, " ", title_num);
                    }
                }
                else {
                    xstrncat(title, arg, title_num);
                    if(j+1 < command->mArgsNum) {
                        xstrncat(title, " ", title_num);
                    }
                }
            }
        }

        if(i+1 < statment->mCommandsNum) {
            xstrncat(title, "|", title_num);
        }
    }
}

static BOOL wait_child_program(pid_t pid, pid_t nextin_reader_pid, int nextout2, sRunInfo* runinfo, char* program)
{
    if(nextin_reader_pid != -1) {
        if(gAppType == kATOptC) {
            while(1) {
                int status;
                nextin_reader_pid = waitpid(nextin_reader_pid, &status, WUNTRACED);

                if(nextin_reader_pid < 0 && errno == EINTR) {
                }
                else if(WIFSTOPPED(status)) {
                    kill(nextin_reader_pid, SIGCONT);
                }
                else if(WIFSIGNALED(status)) {
                    err_msg("signal interrupt4", runinfo->mSName, runinfo->mSLine, program);
                    return FALSE;
                }
                else {
                    break;
                }
            }
        }
        else {
            int status;
            nextin_reader_pid = waitpid(nextin_reader_pid, &status, WUNTRACED);

            if(WIFSIGNALED(status) || WIFSTOPPED(status))
            {
                err_msg("signal interrupt5", runinfo->mSName, runinfo->mSLine, program);
                return FALSE;
            }
        }
    }

    if(!runinfo->mLastProgram) {
        if(gAppType == kATOptC) {
            while(1) {
                int status;
                pid = waitpid(pid, &status, WUNTRACED);

                if(pid < 0 && errno == EINTR) {
                }
                else if(WIFSTOPPED(status)) {
                    kill(pid, SIGCONT);
                }
                else if(WIFSIGNALED(status)) {
                    err_msg("signal interrupt6", runinfo->mSName, runinfo->mSLine, program);
                    return FALSE;
                }
                else {
                    break;
                }
            }
        }
        else {
            int status;
            pid = waitpid(pid, &status, WUNTRACED);

            if(WIFSTOPPED(status)) {
                err_msg("signal interrupt7", runinfo->mSName, runinfo->mSLine, program);

                kill(pid, SIGKILL);
                pid = waitpid(pid, &status, WUNTRACED);
                if(tcsetpgrp(0, getpgid(0)) < 0) {
                    perror("tcsetpgrp(xyzsh)");
                    exit(1);
                }

                return FALSE;
            }
            /// exited normally ///
            else if(WIFEXITED(status)) {
                /// command not found ///
                if(WEXITSTATUS(status) == 127) {
                    if(gAppType == kATConsoleApp) // && nextout2 == 1)
                    {
                        if(tcsetpgrp(0, getpgid(0)) < 0) {
                            perror("tcsetpgrp(xyzsh)");
                            exit(1);
                        }
                    }

                    char buf[BUFSIZ];
                    snprintf(buf, BUFSIZ, "command not found");
                    err_msg(buf, runinfo->mSName, runinfo->mSLine, program);
                    return FALSE;
                }
            }
            else if(WIFSIGNALED(status)) {  // a xyzsh external program which is not a last command can't be stopped by CTRL-Z
                err_msg("signal interrupt8", runinfo->mSName, runinfo->mSLine, program);

                if(tcsetpgrp(0, getpgid(0)) < 0) {
                    perror("tcsetpgrp(xyzsh)");
                    exit(1);
                }
                return FALSE;
            }
            if(tcsetpgrp(0, getpgid(0)) < 0) {
                perror("tcsetpgrp(xyzsh)");
                exit(1);
            }
        }
    }
    /// last program ///
    else {
        /// a background job ///
        if(runinfo->mStatment->mFlags & STATMENT_FLAGS_BACKGROUND)
        {
            struct termios tty;
            tcgetattr(STDIN_FILENO, &tty);

            char title[BUFSIZ];
            make_job_title(runinfo, title, BUFSIZ);

            sObject* job = JOB_NEW_GC(title, pid, tty);
        }

        /// a forground job ///
        else {
            sigchld_block(0);       // don't block sigchild
            int status = 0;

            if(gAppType == kATOptC) {
                while(1) {
                    pid_t pid2 = waitpid(pid, &status, WUNTRACED);
                    if(pid2 < 0 && errno == EINTR) {
                    }
                    else if(WIFSTOPPED(status)) {
                        kill(pid2, SIGCONT);
                    }
                    else {
                        break;
                    }
                }
            }
            else {
                pid_t pid2 = waitpid(pid, &status, WUNTRACED);

                /// exited from signal ///
                if(WIFSIGNALED(status)) {
                    runinfo->mRCode = WTERMSIG(status) + 128;
                }
                /// exited normally ///
                else if(WIFEXITED(status)) {
                    runinfo->mRCode = WEXITSTATUS(status);

                    /// command not found ///
                    if(runinfo->mRCode == 127) {
                        if(gAppType == kATConsoleApp) // && nextout2 == 1)
                        {
                            if(tcsetpgrp(0, getpgid(0)) < 0) {
                                perror("tcsetpgrp(xyzsh)");
                                exit(1);
                            }
                        }

                        char buf[BUFSIZ];
                        snprintf(buf, BUFSIZ, "command not found");
                        err_msg(buf, runinfo->mSName, runinfo->mSLine, program);
                        return FALSE;
                    }
                }
                /// stopped from signal ///
                else if(WIFSTOPPED(status)) {
                    runinfo->mRCode = WSTOPSIG(status) + 128;

                    /// make a job ///
                    struct termios tty;
                    tcgetattr(STDIN_FILENO, &tty);

                    char title[BUFSIZ];
                    make_job_title(runinfo, title, BUFSIZ);

                    sObject* job = JOB_NEW_GC(title, pid, tty);
                }

                if(gAppType == kATConsoleApp) // && nextout2 == 1)
                {
                    if(tcsetpgrp(0, getpgid(0)) < 0) {
                        perror("tcsetpgrp(xyzsh)");
                        exit(1);
                    }
                }
            }
        }
    }

    return TRUE;
}

static BOOL run_exec_cprog(sRunInfo* runinfo, char* program, int nextin, int nextout, int nexterr)
{
    /// pipe ///
    if(nextin != 0) {
        if(dup2(nextin, 0) < 0) {
            char buf[32];
            snprintf(buf, 32, "dup2 3 nextin %d", nextin);
            perror(buf);
            exit(1);
        }
        if(close(nextin) < 0) { return FALSE; }
    }
    
    if(nextout != 1) {
        if(dup2(nextout, 1) < 0) {
            char buf[128];
            snprintf(buf, 128, "dup2 nextout (%d)", nextout);
            perror(buf);
            exit(1);
        }
        
        if(close(nextout) < 0) { return FALSE; }
    }
    
    if(nexterr != 2) {
        if(dup2(nexterr, 2) < 0) {
            perror("dup2 5");
            exit(1);
        }
        
        if(close(nexterr) < 0) { return FALSE; }
    }

    /// exec ///
    BOOL program_at_cd = TRUE;
    char* p = program;
    while(*p) {
        if(*p == '/') {
            program_at_cd = FALSE;
            break;
        }
        else {
            p++;
        }
    }

    if(program_at_cd) {
        if(access(program, X_OK) == 0) {
            fprintf(stderr, "This is program at current directory\n");
            exit(127);
        }
    }

    execv(program, runinfo->mArgsRuntime);
    //fprintf(stderr, "exec('%s') error\n", command->mArgsRuntime[0]);  // comment out for try
    exit(127);
}

static BOOL run_external_command(char* program, sObject* nextin, sObject* nextout, sRunInfo* runinfo)
{
    int pipeinfds[2] = { -1, -1};
    int pipeoutfds[2] = { -1, -1 };
    int pipeerrfds[2] = { -1 , -1};
    int nextin2;
    int nextout2;
    int nexterr2;

    if(nextin == gStdin && SFD(gStdin).mBufLen == 0) {
        nextin2 = 0;
    }
    else if(TYPE(nextin) == T_FD2) {
        nextin2 = SFD2(nextin).mFD;
    }
    else {
        if(pipe(pipeinfds) < 0) {
            perror("pipe");
            exit(1);
        }
        nextin2 = pipeinfds[0];
    }
    if(nextout == gStdout) {
        nextout2 = 1;
        nexterr2 = 2;
    }
    else if(TYPE(nextout) == T_FD2) {
        nextout2 = SFD2(nextout).mFD;

        if(pipe(pipeerrfds) < 0) {
            perror("pipe");
            exit(1);
        }
        nexterr2 = pipeerrfds[1];
    }
    else {
        if(pipe(pipeoutfds) < 0) {
            perror("pipe");
            exit(1);
        }
        nextout2 = pipeoutfds[1];

        if(pipe(pipeerrfds) < 0) {
            perror("pipe");
            exit(1);
        }
        nexterr2 = pipeerrfds[1];
    }

    /// fork ///
    pid_t pid = fork();
    if(pid < 0) {
        perror("fork");
        exit(1);
    }

    /// a child process ///
    if(pid == 0) {
        if(nextin2 != 0) { (void)close(pipeinfds[1]); }
        if(nextout2 != 1) { (void)close(pipeoutfds[0]); }
        if(nexterr2 != 2) {(void)close(pipeerrfds[0]); }

        pid = getpid();

//        if(nextout2 != pipeoutfds[1]) {
if(1) {
            // a child process has a own process group
            if(gAppType != kATOptC) { (void)setpgid(pid, pid); }

            /// move the child program to forground
            if(gAppType == kATConsoleApp
                && !(runinfo->mStatment->mFlags & STATMENT_FLAGS_BACKGROUND))
            {
                if(tcsetpgrp(0, pid) < 0) {
                    perror("tcsetpgrp(child)");
                    exit(1);
                }
            }
        }
        else {
            if(gAppType != kATOptC) { (void)setpgid(pid, getpgid(getpid())); }
        }

        xyzsh_restore_signal_default();
        sigchld_block(0);

        if(!run_exec_cprog(runinfo, program, nextin2, nextout2, nexterr2))
        {
            return FALSE;
        }

        return TRUE;
    }
    /// the parent process 
    else {
        pid_t pgroup;
if(1) {
//        if(nextout2 != pipeoutfds[1]) {
            pgroup = pid; // a child process has a own process group
        }
        else {
            pgroup = getpgid(getpid());
        }

        if(gAppType != kATOptC) {
            (void)setpgid(pid, pgroup);
        }

        /// If a child process gets big data in xyzsh, it makes the xyzsh blocked.
        /// Therefore xyzsh can't waitpid and deadlock.
        /// Avoid this, xyzsh is ready for a writer process to write buffer to the xyzsh.
        pid_t nextin_reader_pid = -1;
        if(TYPE(nextin) != T_FD2 && nextin2 != 0) {
            nextin_writer(pgroup, nextin, pipeinfds, pipeoutfds, pipeerrfds, runinfo, program, &nextin_reader_pid);
        }
        if(TYPE(nextout) != T_FD2 && nextout2 != 1) {
            (void)close(pipeoutfds[1]);
            if(!nextout_reader(nextout, pipeoutfds, runinfo, program)) {
                // wait everytime
                (void)wait_child_program(pid, nextin_reader_pid, nextout2, runinfo, program);
                return FALSE;
            }
        }
        if(nexterr2 != 2) {
            (void)close(pipeerrfds[1]);
            if(!nextout_reader(gStderr, pipeerrfds, runinfo, program)) {
                (void)wait_child_program(pid, nextin_reader_pid, nextout2, runinfo, program);
                return FALSE;
            }
        }

        // wait everytime
        if(!wait_child_program(pid, nextin_reader_pid, nextout2, runinfo, program)) {
            return FALSE;
        }

        return TRUE;
    }
}

BOOL run_function(sObject* fun, sObject* nextin, sObject* nextout, sRunInfo* runinfo, char** argv, int argc, sObject** blocks, int blocks_num)
{
    sObject* local_objects = SFUN(fun).mLocalObjects;
    sRunInfo* runinfo2 = gRunInfoOfRunningObject;
    gRunInfoOfRunningObject = runinfo;

    BOOL no_stackframe = SFUN(fun).mFlags & FUN_FLAGS_NO_STACKFRAME;

    if(!no_stackframe) {
        sObject* stackframe = UOBJECT_NEW_GC(8, gXyzshObject, "_stackframe", FALSE);
        vector_add(gStackFrames, stackframe);
        //uobject_init(stackframe);
        SFUN(fun).mLocalObjects = stackframe;
    } else {
        sObject* stackframe_before = vector_item(gStackFrames, vector_count(gStackFrames)-1);
        sObject* stackframe = UOBJECT_NEW_GC(8, gXyzshObject, "_stackframe", FALSE);
        vector_add(gStackFrames, stackframe);
        //uobject_init(stackframe);
        SFUN(fun).mLocalObjects = stackframe;

        uobject_it* it = uobject_loop_begin(stackframe_before);
        while(it) {
             sObject* object = uobject_loop_item(it);
             char* key = uobject_loop_key(it);
             uobject_put(stackframe, key, object);
             it = uobject_loop_next(it);
        }
    }

    sObject* argv2 = VECTOR_NEW_GC(16, FALSE);
    int i;
    for(i=0; i<argc; i++) {
        vector_add(argv2, STRING_NEW_GC(argv[i], FALSE));
    }
    uobject_put(SFUN(fun).mLocalObjects, "ARGV", argv2);

    sCommand* command = runinfo->mCommand;
    sObject* options = HASH_NEW_GC(16, FALSE);
    for(i=0; i<XYZSH_OPTION_MAX; i++) {
        if(runinfo->mOptions[i].mKey) {
            if(runinfo->mOptions[i].mArg) {
                hash_put(options, SFUN(fun).mOptions[i].mKey, STRING_NEW_GC(runinfo->mOptions[i].mArg, FALSE));
            }
            else {
                hash_put(options, runinfo->mOptions[i].mKey, STRING_NEW_GC(runinfo->mOptions[i].mKey, FALSE));
            }
        }
    }
    uobject_put(SFUN(fun).mLocalObjects, "OPTIONS", options);

    sObject* arg_blocks = SFUN(fun).mArgBlocks;
    SFUN(fun).mArgBlocks = VECTOR_NEW_GC(16, FALSE);

    for(i=0; i<blocks_num; i++) {
        sObject* block = blocks[i];
        vector_add(SFUN(fun).mArgBlocks, block);
        //vector_add(SFUN(fun).mArgBlocks, block_clone_on_stack(block, T_BLOCK));
    }

    int rcode = 0;

    if(!run(SFUN(fun).mBlock, nextin, nextout, &rcode, runinfo->mCurrentObject, fun)) {
        if(rcode & RCODE_RETURN) {
            rcode = rcode & 0xff;
        }
        else {
            char buf[BUFSIZ];
            err_msg_adding("run time error", runinfo->mSName, runinfo->mSLine, command->mArgs[0]);
            runinfo->mRCode = rcode;
            SFUN(fun).mLocalObjects = local_objects;
            (void)vector_pop_back(gStackFrames);
            gRunInfoOfRunningObject = runinfo2;
            return FALSE;
        }
    }

    (void)vector_pop_back(gStackFrames);

    runinfo->mRCode = rcode;

    SFUN(fun).mLocalObjects = local_objects;
    SFUN(fun).mArgBlocks = arg_blocks;
    gRunInfoOfRunningObject = runinfo2;

    return TRUE;
}

static BOOL run_block(sObject* block, sObject* nextin, sObject* nextout, sRunInfo* runinfo)
{
    int rcode = 0;
    if(!run(block, nextin, nextout, &rcode, runinfo->mCurrentObject, runinfo->mRunningObject)) {
        sCommand* command = runinfo->mCommand;
        err_msg_adding("run time error", runinfo->mSName, runinfo->mSLine, command->mArgs[0]);
        runinfo->mRCode = rcode;
        return FALSE;
    }

    return TRUE;
}

static BOOL run_completion(sObject* compl, sObject* nextin, sObject* nextout, sRunInfo* runinfo) 
{
    stack_start_stack();

    sObject* fun = FUN_NEW_STACK(NULL);
    sObject* stackframe = UOBJECT_NEW_GC(8, gXyzshObject, "_stackframe", FALSE);
    vector_add(gStackFrames, stackframe);
    //uobject_init(stackframe);
    SFUN(fun).mLocalObjects = stackframe;

    xyzsh_set_signal();
    int rcode = 0;
    if(!run(SCOMPLETION(compl).mBlock, nextin, nextout, &rcode, gRootObject, fun)) {
        if(rcode == RCODE_BREAK) {
        }
        else if(rcode & RCODE_RETURN) {
        }
        else if(rcode == RCODE_EXIT) {
        }
        else {
            err_msg_adding("run time error\n", runinfo->mSName, runinfo->mSLine, runinfo->mArgs[0]);
        }

        (void)vector_pop_back(gStackFrames);
        readline_signal();
        stack_end_stack();

        return FALSE;
    }
    (void)vector_pop_back(gStackFrames);
    readline_signal();
    stack_end_stack();

    return TRUE;
}

BOOL run_object(sObject* object, sObject* nextin, sObject* nextout, sRunInfo* runinfo)
{
    switch(TYPE(object)) {
        case T_NFUN: 
            if(gAppType == kATConsoleApp)
            {
                if(tcsetpgrp(0, getpgid(0)) < 0) {
                    perror("tcsetpgrp inner command a");
                    exit(1);
                }
            }

            runinfo->mRCode = RCODE_NFUN_INVALID_USSING;

            if(!SNFUN(object).mNativeFun(nextin, nextout, runinfo)) {
                return FALSE;
            }
            if(runinfo->mRCode == RCODE_NFUN_INVALID_USSING) {
                sCommand* command = runinfo->mCommand;
                err_msg("invalid command using", runinfo->mSName, runinfo->mSLine, runinfo->mArgs[0]);
                return FALSE;
            }
            break;

        case T_FUN: {
            sCommand* command = runinfo->mCommand;
            runinfo->mCurrentObject = runinfo->mRecieverObject;
            if(!run_function(object, nextin, nextout, runinfo, runinfo->mArgsRuntime + 1, runinfo->mArgsNumRuntime -1, runinfo->mBlocks, runinfo->mBlocksNum)){
                return FALSE;
            }
            }
            break;

        case T_CLASS: {
            sCommand* command = runinfo->mCommand;
            if(!run_function(object, nextin, nextout, runinfo, runinfo->mArgsRuntime + 1, runinfo->mArgsNumRuntime -1, runinfo->mBlocks, runinfo->mBlocksNum)) {
                return FALSE;
            }
            }
            break;

        case T_EXTPROG:
            if(!run_external_command(SEXTPROG(object).mPath, nextin, nextout, runinfo)) {
                return FALSE;
            }
            break;

        case T_BLOCK:
            if(!run_block(object, nextin, nextout, runinfo)) {
                return FALSE;
            }
            break;

        case T_STRING: {
            char* str = string_c_str(object);
            int size = strlen(str);
            if(!fd_write(nextout, str, size)) {
                sCommand* command = runinfo->mCommand;
                if(command) {
                    err_msg("signal interrupt9", runinfo->mSName, runinfo->mSLine, command->mArgs[0]);
                }
                else {
                    err_msg("signal interrupt9", runinfo->mSName, runinfo->mSLine, "");
                }
                runinfo->mRCode = RCODE_SIGNAL_INTERRUPT;
                return FALSE;
            }
            if(!fd_write(nextout, "\n", 1)) {
                sCommand* command = runinfo->mCommand;
                if(command) {
                    err_msg("signal interrupt10", runinfo->mSName, runinfo->mSLine, command->mArgs[0]);
                }
                else {
                    err_msg("signal interrupt10", runinfo->mSName, runinfo->mSLine, "");
                }
                runinfo->mRCode = RCODE_SIGNAL_INTERRUPT;
                return FALSE;
            }
            }
            break;

        case T_UOBJECT: {
            sObject* main_obj = uobject_item(object, "main");
            runinfo->mRecieverObject = object;

            if(main_obj) {
                if(!run_object(main_obj, nextin, nextout, runinfo)) {
                    return FALSE;
                }
            }
            else {
                if(!cmd_mshow(nextin, nextout, runinfo)) {
                    return FALSE;
                }
            }
            }
            break;

        case T_VECTOR: {
            int j;
            for(j=0; j<vector_count(object); j++) {
                sObject* item = vector_item(object, j);

                if(!run_object(item, nextin, nextout, runinfo)) {
                    return FALSE;
                }
            }
            }
            break;

        case T_HASH: { 
            hash_it* it = hash_loop_begin(object);
            while(it) {
                char* key = hash_loop_key(it);
                if(!fd_write(nextout, key, strlen(key))) {
                    sCommand* command = runinfo->mCommand;
                    if(command) {
                        err_msg("signal interrupt9", runinfo->mSName, runinfo->mSLine, command->mArgs[0]);
                    }
                    else {
                        err_msg("signal interrupt9", runinfo->mSName, runinfo->mSLine, "");
                    }
                    runinfo->mRCode = RCODE_SIGNAL_INTERRUPT;
                    return FALSE;
                }
                if(!fd_write(nextout, "\n", 1)) {
                    sCommand* command = runinfo->mCommand;
                    if(command) {
                        err_msg("signal interrupt10", runinfo->mSName, runinfo->mSLine, command->mArgs[0]);
                    }
                    else {
                        err_msg("signal interrupt10", runinfo->mSName, runinfo->mSLine, "");
                    }
                    runinfo->mRCode = RCODE_SIGNAL_INTERRUPT;
                    return FALSE;
                }

                sObject* item = hash_loop_item(it);

                if(!run_object(item, nextin, nextout, runinfo)) {
                    return FALSE;
                }

                it = hash_loop_next(it);
            }
            }
            break;

        case T_EXTOBJ: { 
            if(SEXTOBJ(object).mMainFun) {
                if(!SEXTOBJ(object)
                      .mMainFun(SEXTOBJ(object).mObject, nextin, nextout, runinfo))
                {
                    return FALSE;
                }   
            }
            }
            break;

        case T_COMPLETION:
            if(!run_completion(object, nextin, nextout, runinfo)) {
                return FALSE;
            }
            break;

//        case T_JOB:
//        case T_FD:
//        case T_FD2:
        default: {
            char buf[BUFSIZ];
            sCommand* command = runinfo->mCommand;
            if(command) {
                err_msg("can't run this object", runinfo->mSName, runinfo->mSLine, command->mArgs[0]);
            }
            else {
                err_msg("can't run this object", runinfo->mSName, runinfo->mSLine, "");
            }
            }
            return FALSE;
    }

    return TRUE;
}

static BOOL stdin_read_out()
{
    char buf[BUFSIZ+1];
    while(TRUE) {
        int size = read(0, buf, BUFSIZ);
        if(size < 0) {
            return FALSE;
        }
        if(size == 0) {
            break;
        }
        else {
            if(!fd_write(gStdin, buf, size)) {
                return FALSE;
            }
        }
    }

    return TRUE;
}

static BOOL ready_for_redirect(sObject** nextout, sObject** nextin, int* opened_fd, sObject* pipeout, BOOL last_program, sStatment* statment, sCommand* command, sRunInfo* runinfo)
{
    int i;
    for(i=0; i<command->mRedirectsNum; i++) {
        int fd;
        char* result;
        int result_len;
        int result_size;

        switch(command->mRedirects[i] & REDIRECT_KIND) {
            case REDIRECT_IN:
                fd = open(runinfo->mRedirectsFileNamesRuntime[i], O_RDONLY);
                if(fd < 0) {
                    char buf[BUFSIZ];
                    snprintf(buf, BUFSIZ, "redirect: error with openning %s file", runinfo->mRedirectsFileNamesRuntime[i]);
                    err_msg(buf, runinfo->mSName, runinfo->mSLine, "");
                    return FALSE;
                }

                if(!bufsiz_read(fd, ALLOC &result, &result_len, &result_size)) {
                    char buf[BUFSIZ];
                    snprintf(buf, BUFSIZ, "redirect: error with reading %s file", runinfo->mRedirectsFileNamesRuntime[i]);
                    err_msg(buf, runinfo->mSName, runinfo->mSLine, "");
                    return FALSE;
                }
                (void)close(fd);

                ASSERT(TYPE(*nextin) == T_FD);
                fd_put(*nextin, MANAGED result, result_size, result_len);
                break;

            case REDIRECT_APPEND:
                if(*nextout == NULL) {
                    fd = open(runinfo->mRedirectsFileNamesRuntime[i], O_WRONLY|O_APPEND|O_CREAT, 0644);
                    if(fd < 0) {
                        char buf[BUFSIZ];
                        snprintf(buf, BUFSIZ, "redirect: error with openning %s file", runinfo->mRedirectsFileNamesRuntime[i]);
                        err_msg(buf, runinfo->mSName, runinfo->mSLine, "");
                        return FALSE;
                    }
                    *nextout = FD2_NEW_STACK(fd);

                    *opened_fd = fd;
                }
                break;

            case REDIRECT_OUT:
                if(*nextout == NULL) {
                    fd = open(runinfo->mRedirectsFileNamesRuntime[i], O_WRONLY|O_CREAT|O_TRUNC, 0644);
                    if(fd < 0) {
                        char buf[BUFSIZ];
                        snprintf(buf, BUFSIZ, "redirect: error with openning %s file", runinfo->mRedirectsFileNamesRuntime[i]);
                        err_msg(buf, runinfo->mSName, runinfo->mSLine, "");
                        return FALSE;
                    }
                    *nextout = FD2_NEW_STACK(fd);

                    *opened_fd = fd;
                }
                break;
        }
    }

    return TRUE;
}

static void redirect_finish(int opend_fd)
{
    (void)close(opend_fd);
}

static BOOL statment_tree(sStatment* statment, sObject* pipein, sObject* pipeout, sRunInfo* runinfo, sObject* current_object)
{
    /// error check ///
    if(statment->mFlags & STATMENT_FLAGS_BACKGROUND) {
        if(gAppType == kATOptC) {
            err_msg("can't make a job background on script mode. you can make a job background on interactive shell", runinfo->mSName, runinfo->mSLine, NULL);
            return FALSE;
        }
    }

    /// head of statment ///
    sObject* nextin;
    if(statment->mFlags & STATMENT_FLAGS_CONTEXT_PIPE) {
        /// read stdin ///
        if(pipein == gStdin && SFD(gStdin).mBufLen == 0 && !isatty(0)) {
            if(!stdin_read_out()) {
                sCommand* command = runinfo->mCommand;
                if(command) {
                    err_msg("signal interrupt11", runinfo->mSName, runinfo->mSLine, command->mArgs[0]);
                }
                else {
                    err_msg("signal interrupt11", runinfo->mSName, runinfo->mSLine, "");
                }
                runinfo->mRCode = RCODE_SIGNAL_INTERRUPT;
                return FALSE;
            }
        }

        short line_number = statment->mFlags & STATMENT_FLAGS_CONTEXT_PIPE_NUMBER;

        /// context pipe ///
        if(line_number == 0) {
            nextin = pipein;
        }
        /// line context pipe ///
        else {
            nextin = FD_NEW_STACK();
            eLineField lf;
            if(fd_guess_lf(pipein, &lf)) {
                fd_split(pipein, lf);
            } else {
                fd_split(pipein, kLF);
            }
            if(line_number < 0) line_number += vector_count(SFD(pipein).mLines)+1;
            if(line_number < 0) line_number = 0;
            if(line_number > vector_count(SFD(pipein).mLines)) line_number = vector_count(SFD(pipein).mLines);
            int i;
            for(i=SFD(pipein).mReadedLineNumber; i<vector_count(SFD(pipein).mLines) && i-SFD(pipein).mReadedLineNumber<line_number; i++) {
                char* line = vector_item(SFD(pipein).mLines, i);
                if(!fd_write(nextin, line, strlen(line))) {
                    sCommand* command = runinfo->mCommand;
                    if(command) {
                        err_msg("signal interrupt12", runinfo->mSName, runinfo->mSLine, command->mArgs[0]);
                    }
                    else {
                        err_msg("signal interrupt12", runinfo->mSName, runinfo->mSLine, "");
                    }
                    runinfo->mRCode = RCODE_SIGNAL_INTERRUPT;
                    return FALSE;
                }
            }
            SFD(pipein).mReadedLineNumber += line_number;
        }
    }
    else if(statment->mFlags & STATMENT_FLAGS_GLOBAL_PIPE_IN) {
        nextin = FD_NEW_STACK();
        if(!fd_write(nextin, SFD(gGlobalPipe).mBuf, SFD(gGlobalPipe).mBufLen)) {
            sCommand* command = runinfo->mCommand;
            if(command) {
                err_msg("signal interrupt12", runinfo->mSName, runinfo->mSLine, command->mArgs[0]);
            }
            else {
                err_msg("signal interrupt12", runinfo->mSName, runinfo->mSLine, "");
            }
            runinfo->mRCode = RCODE_SIGNAL_INTERRUPT;
            return FALSE;
        }
    }
    else {
        nextin = pipein;
    }

    /// command loop ///
    int i;
    for(i=0; i<statment->mCommandsNum; i++) {
        sCommand* command = runinfo->mCommand = statment->mCommands + i;

        const BOOL last_program = runinfo->mLastProgram = i == statment->mCommandsNum-1;
        runinfo->mFilter = i != 0 || (statment->mFlags & (STATMENT_FLAGS_CONTEXT_PIPE|STATMENT_FLAGS_GLOBAL_PIPE_IN));

        runinfo->mCurrentObject = current_object;

        sRunInfo_command_new(runinfo);

        /// expand env ///
        if(!sCommand_expand_env_of_redirect(command, pipein, runinfo))
        {
            sRunInfo_command_delete(runinfo);
            return FALSE;
        }

        sObject* nextout = NULL;

        int opened_fd = -1;
        if(!ready_for_redirect(&nextout, &nextin, &opened_fd, pipeout, last_program, statment, command, runinfo)) {
            sRunInfo_command_delete(runinfo);
            return FALSE;;
        }
        
        ASSERT(TYPE(nextin) == T_FD);

        if(nextout == NULL) {
            if(last_program) {
                if(statment->mFlags & STATMENT_FLAGS_GLOBAL_PIPE_OUT) {
                    fd_clear(gGlobalPipe);
                    nextout = gGlobalPipe;
                }
                else if(statment->mFlags & STATMENT_FLAGS_GLOBAL_PIPE_APPEND) {
                    nextout = gGlobalPipe;
                }
                else {
                    nextout = pipeout;
                }
            }
            else {
                nextout = FD_NEW_STACK();
            }
        }

        if(command->mArgsNum > 0) {
            if(command->mArgsFlags[0] & XYZSH_ARGUMENT_ENV) {
                err_msg("a command name must be determined staticaly. Instead of this, use eval inner command.", runinfo->mSName, runinfo->mSLine, NULL);
                sRunInfo_command_delete(runinfo);
                return FALSE;
            }

            /// message passing ///
            if(command->mMessagesNum > 0) {
                sObject* reciever = current_object;
                sObject* object = access_object(command->mMessages[0], &reciever, runinfo->mRunningObject);
                int i = 1;
                while(i < command->mMessagesNum) {
                    if(object && TYPE(object) == T_UOBJECT) {
                        reciever = object;
                        object = uobject_item(object, command->mMessages[i++]);
                    }
                    else {
                        err_msg("invalid message passing", runinfo->mSName, runinfo->mSLine, command->mArgs[0]);
                        sRunInfo_command_delete(runinfo);
                        return FALSE;
                    }
                }

                if(object && TYPE(object) == T_UOBJECT) {
                    reciever = object;
                    object = uobject_item(object, command->mArgs[0]);

                    if(object == gInheritObject) {
                        sObject* running_object = runinfo->mRunningObject;

                        if(runinfo->mRunningObject) {
                            switch(TYPE(running_object)) {
                            case T_FUN:  {
                                sObject* parent = SFUN(running_object).mParent;

                                if(parent) {
                                    if(TYPE(parent) == T_NFUN || TYPE(parent) == T_FUN) {
                                        object = parent;
                                    }
                                }
                                }
                                break;

                            case T_CLASS: {
                                sObject* parent = SFUN(running_object).mParent;

                                if(parent) {
                                    if(TYPE(parent) == T_CLASS) {
                                        object = parent;
                                    }
                                }
                                }
                                break;

                            case T_NFUN:
                            case T_BLOCK:
                                err_msg("There is not a parent object", runinfo->mSName, runinfo->mSLine, command->mArgs[0]);
                                sRunInfo_command_delete(runinfo);
                                return FALSE;

                            default:
                                fprintf(stderr, "unexpected error in cmd_inherit\n");
                                exit(1);
                            }
                        }
                    }

                    /// expand env ///
                    if(!sCommand_expand_env(command, object, pipein, nextout, runinfo))
                    {
                        sRunInfo_command_delete(runinfo);
                        return FALSE;
                    }

                    if(object) {
                        runinfo->mRecieverObject = reciever;
                        if(!run_object(object, nextin, nextout, runinfo)) {
                            sRunInfo_command_delete(runinfo);
                            return FALSE;
                        }
                    }
                    else {
                        err_msg("there is not this object", runinfo->mSName, runinfo->mSLine, command->mArgs[0]);
                        sRunInfo_command_delete(runinfo);
                        return FALSE;
                    }
                }
                else {
                    err_msg("invalid message passing", runinfo->mSName, runinfo->mSLine, command->mArgs[0]);
                    sRunInfo_command_delete(runinfo);
                    return FALSE;
                }
            }
            /// normal function ///
            else {
                sObject* reciever = current_object;
                sObject* object = access_object(command->mArgs[0], &reciever, runinfo->mRunningObject);

                if(object == gInheritObject) {
                    sObject* running_object = runinfo->mRunningObject;

                    if(runinfo->mRunningObject) {
                        switch(TYPE(running_object)) {
                        case T_FUN:  {
                            sObject* parent = SFUN(running_object).mParent;

                            if(parent) {
                                if(TYPE(parent) == T_NFUN || TYPE(parent) == T_FUN) {
                                    object = parent;
                                }
                            }
                            }
                            break;

                        case T_CLASS: {
                            sObject* parent = SFUN(running_object).mParent;

                            if(parent) {
                                if(TYPE(parent) == T_CLASS) {
                                    object = parent;
                                }
                            }
                            }
                            break;

                        case T_NFUN:
                        case T_BLOCK:
                            err_msg("There is not a parent object", runinfo->mSName, runinfo->mSLine, command->mArgs[0]);
                            sRunInfo_command_delete(runinfo);
                            return FALSE;

                        default:
                            fprintf(stderr, "unexpected error in cmd_inherit\n");
                            exit(1);
                        }
                    }
                }

                if(object) {
                    /// expand env ///
                    if(!sCommand_expand_env(command, object, pipein, nextout, runinfo))
                    {
                        sRunInfo_command_delete(runinfo);
                        return FALSE;
                    }

                    runinfo->mRecieverObject = reciever;
                    if(!run_object(object, nextin, nextout, runinfo)) {
                        sRunInfo_command_delete(runinfo);
                        return FALSE;
                    }
                }
                else {
                    /// expand env ///
                    if(!sCommand_expand_env(command, NULL, pipein, nextout, runinfo))
                    {
                        sRunInfo_command_delete(runinfo);
                        return FALSE;
                    }

                    if(!run_external_command(runinfo->mArgsRuntime[0], nextin, nextout, runinfo)) {
                        sRunInfo_command_delete(runinfo);
                        return FALSE;
                    }
                }
            }
        }

        if(opened_fd != -1) redirect_finish(opened_fd);

        if(TYPE(nextout) == T_FD2) {
            nextin = FD_NEW_STACK();
        }
        else {
            nextin = nextout;
        }

        sRunInfo_command_delete(runinfo);
    }

    /// reverse the return code ///
    if(statment->mFlags & STATMENT_FLAGS_REVERSE) {
        runinfo->mRCode = !runinfo->mRCode;
    }

    return TRUE;
}

BOOL run(sObject* block, sObject* pipein, sObject* pipeout, int* rcode, sObject* current_object, sObject* running_object)
{
    vector_add(gRunningObjects, running_object); // for marking on gc
    vector_add(gRunningObjects, block); // for marking on gc

    static unsigned int n = 0;
    static int nest_level = 0;
    nest_level++;
    if(nest_level == 50) {
        if(SBLOCK(block).mStatmentsNum > 0) {
            sStatment* statment = SBLOCK(block).mStatments + 0;
            err_msg("stack over flow", statment->mFName, statment->mLine, "system");
        }
        else {
            err_msg("stack over flow", "xyzsh", 0, "system");
        }
        nest_level--;

        (void)vector_pop_back(gRunningObjects);
        (void)vector_pop_back(gRunningObjects);
        return FALSE;
    }

    stack_start_stack();

    sRunInfo runinfo;
    memset(&runinfo, 0, sizeof(sRunInfo));

    runinfo.mRunningObject = running_object;

    int i;
    for(i=0; i<SBLOCK(block).mStatmentsNum; i++) {
        sStatment* statment = SBLOCK(block).mStatments + i;

        runinfo.mSName = statment->mFName;
        runinfo.mSLine = statment->mLine;
        runinfo.mRCode = 0;
        runinfo.mStatment = statment;
        runinfo.mNestLevel = nest_level;

        if(!statment_tree(statment, pipein, pipeout, &runinfo, current_object)) {
            fd_clear(gStdout);
            fd_clear(gStderr);

            *rcode = runinfo.mRCode;

            stack_end_stack();
            nest_level--;

            if(nest_level == 0) {
                vector_clear(gObjectsInPipe);
            }

            if(!(n++ % 32)) (void)gc_sweep();

            (void)vector_pop_back(gRunningObjects);
            (void)vector_pop_back(gRunningObjects);
            return FALSE;
        }

        /// flash stdout and stderr ///
        if(pipeout == gStdout) {
            if(!fd_flash(gStdout, 1)) {
                fd_clear(gStdout);
                fd_clear(gStderr);

                sCommand* command = runinfo.mCommand;
                err_msg("signal interrupt13", runinfo.mSName, runinfo.mSLine, command->mArgs[0]);
                *rcode = RCODE_SIGNAL_INTERRUPT;
                stack_end_stack();
                nest_level--;
                if(nest_level == 0) {
                    vector_clear(gObjectsInPipe);
                }
                if(!(n++ % 32)) (void)gc_sweep();
                (void)vector_pop_back(gRunningObjects);
                (void)vector_pop_back(gRunningObjects);
                return FALSE;
            }
        }

        if(nest_level == 1) {
            if(!fd_flash(gStderr, 2)) {
                fd_clear(gStdout);
                fd_clear(gStderr);

                sCommand* command = runinfo.mCommand;
                err_msg("signal interrupt14", runinfo.mSName, runinfo.mSLine, command->mArgs[0]);
                *rcode = RCODE_SIGNAL_INTERRUPT;
                stack_end_stack();
                nest_level--;
                if(nest_level == 0) {
                    vector_clear(gObjectsInPipe);
                }
                if(!(n++ % 32)) (void)gc_sweep();
                (void)vector_pop_back(gRunningObjects);
                (void)vector_pop_back(gRunningObjects);
                return FALSE;
            }
        }

        /// the end of statment ///
        if(gXyzshSigUser) {
            gXyzshSigUser = FALSE;
            sCommand* command = runinfo.mCommand;
            err_msg("command not found", runinfo.mSName, runinfo.mSLine, ""); //command->mArgs[0]);
            stack_end_stack();
            nest_level--;
            fd_clear(gStdout);
            fd_clear(gStderr);
            if(nest_level == 0) {
                vector_clear(gObjectsInPipe);
            }
            if(!(n++ % 32)) (void)gc_sweep();
            (void)vector_pop_back(gRunningObjects);
            (void)vector_pop_back(gRunningObjects);
            return FALSE;
        }
        else if(runinfo.mRCode == 128+SIGINT || gXyzshSigInt) {
            gXyzshSigInt = FALSE;
            sCommand* command = runinfo.mCommand;
            err_msg("signal interrupt15", runinfo.mSName, runinfo.mSLine, command->mArgs[0]);
            *rcode = RCODE_SIGNAL_INTERRUPT;
            stack_end_stack();
            nest_level--;
            fd_clear(gStdout);
            fd_clear(gStderr);
            if(nest_level == 0) {
                vector_clear(gObjectsInPipe);
            }
            if(!(n++ % 32)) (void)gc_sweep();
            (void)vector_pop_back(gRunningObjects);
            (void)vector_pop_back(gRunningObjects);
            return FALSE;
        }
        else if((statment->mFlags & STATMENT_FLAGS_OROR) && runinfo.mRCode == 0) {
            while(i<SBLOCK(block).mStatmentsNum
                    && !(statment->mFlags & STATMENT_FLAGS_NORMAL))
            {
                i++;
                statment = SBLOCK(block).mStatments + i;
            }
        }
        else if((statment->mFlags & STATMENT_FLAGS_ANDAND) && runinfo.mRCode != 0) {
            while(i<SBLOCK(block).mStatmentsNum
                    && !(statment->mFlags & STATMENT_FLAGS_NORMAL))
            {
                i++;
                statment = SBLOCK(block).mStatments + i;
            }
        }
    }

    *rcode = runinfo.mRCode;

    stack_end_stack();
    nest_level--;

    if(nest_level == 0) {
        vector_clear(gObjectsInPipe);
    }
    if(!(n++ % 32)) (void)gc_sweep();

    (void)vector_pop_back(gRunningObjects);
    (void)vector_pop_back(gRunningObjects);
    return TRUE;
}

BOOL forground_job(int num)
{
    if(num>=0 && num < vector_count(gJobs)) {
        sObject* job = vector_item(gJobs, num);

        tcsetattr(STDIN_FILENO, TCSANOW, SJOB(job).mTty);
        if(tcsetpgrp(0, SJOB(job).mPGroup) < 0) {
            perror("tcsetpgrp(fg)");
            return FALSE;
        }
        
        if(kill(SJOB(job).mPGroup, SIGCONT) < 0) {
            perror("kill(fg)");
            return FALSE;
        }

        /// wait ///
        int status = 0;
        pid_t pid = waitpid(SJOB(job).mPGroup, &status, WUNTRACED);

        if(pid < 0) {
            perror("waitpid(run_wait_cprogs)");
            return FALSE;
        }

        /// exited normally ///
        if(WIFEXITED(status)) {
            if(xyzsh_job_done) xyzsh_job_done(num, SJOB(job).mName);

            vector_erase(gJobs, num);

            /// forground xyzsh ///
            mreset_tty();
            if(tcsetpgrp(0, getpgid(0)) < 0) {
                perror("tcsetpgrp");
                return FALSE;
            }
        }
        /// exited from signal ///
        else if(WIFSIGNALED(status)) {
            if(xyzsh_job_done) xyzsh_job_done(num, SJOB(job).mName);

            vector_erase(gJobs, num);

            /// froground xyzsh ///
            if(tcsetpgrp(0, getpgid(0)) < 0) {
                perror("tcsetpgrp");
                return FALSE;
            }
        }
        /// stopped from signal ///
        else if(WIFSTOPPED(status)) {
            tcgetattr(STDIN_FILENO, SJOB(job).mTty);

            /// forground xyzsh ///
            if(tcsetpgrp(0, getpgid(0)) < 0) {
                perror("tcsetpgrp");
                return FALSE;
            }
        }

        return TRUE;
    }

    err_msg("invalid job number", "fg", 1, "fg");
    return FALSE;
}

BOOL background_job(int num)
{
    if(num>=0 && num < vector_count(gJobs)) {
        sObject* job = vector_item(gJobs, num);
        
        if(kill(SJOB(job).mPGroup, SIGCONT) < 0) {
            perror("kill(bg)");
            return FALSE;
        }
    }

    return TRUE;
}

void xyzsh_wait_background_job()
{
    int status;
    pid_t pid = waitpid(-1, &status, WNOHANG);

    if(pid >= 0) {
        int i;
        for(i=0; i<vector_count(gJobs); i++) {
            sObject* job = vector_item(gJobs, i);

            if(pid == SJOB(job).mPGroup) {
                if(xyzsh_job_done) xyzsh_job_done(i, SJOB(job).mName);

                vector_erase(gJobs, i);
                break;
            }
        }
    }
}

void xyzsh_kill_all_jobs()
{
    int i;
    for(i=0; i<vector_count(gJobs); i++) {
        sObject* job = vector_item(gJobs, i);

        kill(SJOB(job).mPGroup, SIGKILL);
    }

    vector_clear(gJobs);
}

// TRUE: Success. result is malloced.
// FALSE: signal interrupt. set RCODE_SIGNAL_INTERRUPT on rcode and err message. result is not malloced.
BOOL bufsiz_read(int fd, ALLOC char** result, int* result_len, int* result_size)
{
    int source_size = BUFSIZ * 8;
    int source_len = 0;
    char* source = MALLOC(BUFSIZ*8);
    source[0] = 0;

    char buf[BUFSIZ+1];
    while(TRUE) {
        int size = read(fd, buf, BUFSIZ);
        if(size < 0) {
            FREE(source);
            return FALSE;
        }
        buf[size] = 0;
        if(size < BUFSIZ) {
            if(source_len + size + 1 >= source_size) {
                source_size *= 2;
                source = REALLOC(source, source_size);
            }
            strcat(source, buf);
            source_len += size;
            break;
        }
        else {
            if(source_len + size + 1 >= source_size) {
                source_size *= 2;
                source = REALLOC(source, source_size);
            }
            strcat(source, buf);
            source_len += size;
        }
    }

    *result = source;
    *result_len = source_len;
    *result_size = source_size;
    
    return TRUE;
}

BOOL load_file(char* fname, sObject* nextin, sObject* nextout, sRunInfo* runinfo, char** argv, int argc)
{
    int fd = open(fname, O_RDONLY);
    if(fd < 0) {
        err_msg("open error", runinfo->mSName, runinfo->mSLine, fname);
        return FALSE;
    }

    char* buf;
    int buf_len;
    int buf_size;
    if(!bufsiz_read(fd, ALLOC &buf, &buf_len, &buf_size)) {
        sCommand* command = runinfo->mCommand;
        err_msg("signal interrupt16", runinfo->mSName, runinfo->mSLine, command->mArgs[0]);
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
        err_msg_adding("parser error", runinfo->mSName, runinfo->mSLine, "parser");
        stack_end_stack();
        return FALSE;
    }

    stack_end_stack();

    return TRUE;
}

#if defined(__CYGWIN__)
int migemo_dl_final();
int migemo_dl_init();
#endif

BOOL load_so_file(char* fname, sObject* nextin, sObject* nextout, sRunInfo* runinfo)
{
    sCommand* command = runinfo->mCommand;

    char path[PATH_MAX];
    snprintf(path, PATH_MAX, "%s%s", EXTDIR, fname);

    if(access(path, X_OK) != 0) {
        char* home = getenv("HOME");
        if(home) {
            snprintf(path, PATH_MAX, "%s/.xyzsh/lib/%s", home, fname);
            if(access(path, X_OK) != 0) {
                xstrncpy(path, fname, PATH_MAX);
            }
        }
        else {
            xstrncpy(path, fname, PATH_MAX);
        }
    }

#if defined(__CYGWIN__)
    if(hash_item(gDynamicLibraryFinals, path) == NULL) {
        if(strstr(path, "migemo")) {
            if(migemo_dl_init() != 0) {
                err_msg("false in initialize the dynamic library", runinfo->mSName, runinfo->mSLine, command->mArgs[0]);
                return FALSE;
            }
            hash_put(gDynamicLibraryFinals, path, migemo_dl_final);
        }

        char path2[PATH_MAX];
        snprintf(path2, PATH_MAX, "%s.xyzsh", path);
        if(access(path2, F_OK) == 0) {
            if(!load_file(path2, nextin, nextout, runinfo, NULL, 0)) {
                return FALSE;
            }
        }
    }
#else
    if(hash_item(gDynamicLibraryFinals, path) == NULL) {
        void* handle = dlopen(path, RTLD_LAZY);
        if(handle == NULL) {
            err_msg(dlerror(), runinfo->mSName, runinfo->mSLine, command->mArgs[0]);
            return FALSE;
        }

        int (*init_func)() = dlsym(handle, "dl_init");
        int (*final_func)() = dlsym(handle, "dl_final");
        if(init_func == NULL || final_func == NULL) {
            err_msg("not found dl_init or dl_final", runinfo->mSName, runinfo->mSLine, command->mArgs[0]);
            dlclose(handle);
            return FALSE;
        }

        if(init_func(NULL) != 0) {
            err_msg("false in initialize the dynamic library", runinfo->mSName, runinfo->mSLine, command->mArgs[0]);
            dlclose(handle);
            return FALSE;
        }

        hash_put(gDynamicLibraryFinals, path, final_func);

        //dlclose(handle);

        char path2[PATH_MAX];
        snprintf(path2, PATH_MAX, "%s.xyzsh", path);
        if(access(path2, F_OK) == 0) {
            if(!load_file(path2, nextin, nextout, runinfo, NULL, 0)) {
                return FALSE;
            }
        }
    }
#endif

    return TRUE;
}

