#include "config.h"
#include <time.h>
#include <stdio.h>
#include <string.h>
#include <readline/readline.h>
#include <readline/history.h>
#include <signal.h>
#include <limits.h>
#include <sys/stat.h>
#include <sys/types.h>
#include "xyzsh/xyzsh.h"

static void sig_int_readline(int signo)
{
    rl_kill_full_line(0,0);
    rl_reset_line_state();
    rl_crlf();
    rl_redisplay();
}

int readline_signal()
{
    xyzsh_set_signal();

    struct sigaction sa2;
    memset(&sa2, 0, sizeof(sa2));
    sa2.sa_handler = sig_int_readline;
    sa2.sa_flags |= SA_RESTART;
    if(sigaction(SIGINT, &sa2, NULL) < 0) {
        perror("sigaction2");
        exit(1);
    }
    memset(&sa2, 0, sizeof(sa2));
    sa2.sa_handler = SIG_IGN;
    sa2.sa_flags = 0;
    if(sigaction(SIGTSTP, &sa2, NULL) < 0) {
        perror("sigaction2");
        exit(1);
    }
    memset(&sa2, 0, sizeof(sa2));
    sa2.sa_handler = SIG_IGN;
    sa2.sa_flags = 0;
    if(sigaction(SIGUSR1, &sa2, NULL) < 0) {
        perror("sigaction2");
        exit(1);
    }
    sa2.sa_handler = SIG_IGN;
    sa2.sa_flags = 0;
    if(sigaction(SIGQUIT, &sa2, NULL) < 0) {
        perror("sigaction2");
        exit(1);
    }
}

static void main_xyzsh_job_done(int job_num, char* job_title)
{
    printf("%d: %s done.\n", job_num+1, job_title);
}

static char* prompt()
{
    stack_start_stack();
    xyzsh_set_signal();

    sObject* fun = FUN_NEW_STACK(NULL);
    sObject* stackframe = UOBJECT_NEW_GC(8, gXyzshObject, "_stackframe", FALSE);
    vector_add(gStackFrames, stackframe);
    //uobject_init(stackframe);
    SFUN(fun).mLocalObjects = stackframe;

    sObject* nextout = FD_NEW_STACK();

    int rcode;
    if(!run(gPrompt, gStdin, nextout, &rcode, gCurrentObject, fun)) {
        if(rcode == RCODE_BREAK) {
            fprintf(stderr, "invalid break. Not in a loop\n");
        }
        else if(rcode == RCODE_RETURN) {
            fprintf(stderr, "invalid return. Not in a function\n");
        }
        else if(rcode == RCODE_EXIT) {
            fprintf(stderr, "invalid exit. In the prompt\n");
        }
        else {
            fprintf(stderr, "run time error\n");
            fprintf(stderr, "%s", string_c_str(gErrMsg));
        }
    }
    readline_signal();

    mreset_tty();
    readline_signal();
    char* buf = readline(SFD(nextout).mBuf);

    (void)vector_pop_back(gStackFrames);

    stack_end_stack();

    return buf;
}

BOOL xyzsh_run(int* rcode, sObject* block, char* source_name, fXyzshJobDone xyzsh_job_done_, sObject* nextin, sObject* nextout, int argc, char** argv, sObject* current_object)
{
    string_put(gErrMsg, "");

    xyzsh_job_done = xyzsh_job_done_;

    stack_start_stack();
    xyzsh_set_signal();

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

    if(!run(block, nextin, nextout, rcode, current_object, fun)) {
        xyzsh_restore_signal_default();
        (void)vector_pop_back(gStackFrames);

        stack_end_stack();

        /// wait background job
        xyzsh_wait_background_job();

        return FALSE;
    }
    xyzsh_restore_signal_default();

    (void)vector_pop_back(gStackFrames);
    stack_end_stack();

    /// wait background job
    xyzsh_wait_background_job();

    return TRUE;
}

static void readline_insert_text(char* cmdline, int cursor_point)
{
    (void)rl_replace_line(cmdline, 0);
    //(void)rl_insert_text(cmdline);

    int n = cursor_point;

    if(n < 0) n += strlen(rl_line_buffer) + 1;
    if(n < 0) n = 0;
    if(n > strlen(rl_line_buffer)) n = strlen(rl_line_buffer);

    rl_point = n;
}

static char* gCmdLine;
static int gCursorPoint;

int readline_init_text()
{
    readline_insert_text(gCmdLine, gCursorPoint);
}

static int gHistSize = 1000;

void readline_read_history()
{
    /// set history size ///
    char* histsize_env = getenv("XYZSH_HISTSIZE");
    if(histsize_env) {
        gHistSize = atoi(histsize_env);
        if(gHistSize < 0) gHistSize = 1000;
        if(gHistSize > 50000) gHistSize = 50000;
        char buf[256];
        snprintf(buf, 256, "%d", gHistSize);
        setenv("XYZSH_HISTSIZE", buf, 1);
    }
    else {
        gHistSize = 1000;
        char buf[256];
        snprintf(buf, 256, "%d", gHistSize);
        setenv("XYZSH_HISTSIZE", buf, 1);
    }

    /// set history file name ///
    char* histfile = getenv("XYZSH_HISTFILE");
    if(histfile == NULL) {
        char* home = getenv("HOME");

        if(home) {
            char path[PATH_MAX];
            snprintf(path, PATH_MAX, "%s/.xyzsh/history", home);
            setenv("XYZSH_HISTFILE", path, 1);
        }
        else {
            fprintf(stderr, "HOME evironment path is NULL. exited\n");
            exit(1);
        }
    }

    char rc_path[PATH_MAX];
    snprintf(rc_path, PATH_MAX, "%sread_history.xyzsh", SYSCONFDIR);
    
    xyzsh_read_rc_core(rc_path);
}

void readline_write_history()
{
    char* histfile = getenv("XYZSH_HISTFILE");

    if(histfile) {
        write_history(histfile);
    }
    else {
        char* home = getenv("HOME");

        if(home) {
            char path[PATH_MAX];
            snprintf(path, PATH_MAX, "%s/.xyzsh/history", home);
            write_history(path);
        }
        else {
            fprintf(stderr, "HOME evironment path is NULL. exited\n");
            exit(1);
        }
    }
}

BOOL xyzsh_readline_interface_onetime(int* rcode, char* cmdline, int cursor_point, char* source_name, char** argv, int argc, fXyzshJobDone xyzsh_job_done_)
{
    /// start interactive shell ///
    mreset_tty();
    xyzsh_job_done = xyzsh_job_done_;

    char* buf;
    HISTORY_STATE* history_state = history_get_history_state();
    int history_num = history_state->length;

    /// prompt ///
    if(gPrompt) {
        gCmdLine = cmdline;
        gCursorPoint = cursor_point;
        rl_startup_hook = readline_init_text;
        buf = prompt();
    }
    else {
        gCmdLine = cmdline;
        gCursorPoint = cursor_point;
        rl_startup_hook = readline_init_text;
        mreset_tty();
        readline_signal();
        buf = readline("> ");
    }

    /// run ///
    if(buf == NULL) {
    }
    else if(*buf) {
        stack_start_stack();
        sObject* block = BLOCK_NEW_STACK();
        int sline = 1;
        if(parse(buf, "xyzsh", &sline, block, NULL)) {
            xyzsh_set_signal();

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

            if(!run(block, gStdin, gStdout, rcode, gCurrentObject, fun)) {
                xyzsh_restore_signal_default();
                
                if(*rcode == RCODE_BREAK) {
                    fprintf(stderr, "invalid break. Not in a loop\n");
                }
                else if(*rcode == RCODE_RETURN) {
                    fprintf(stderr, "invalid return. Not in a function\n");
                }
                else if(*rcode == RCODE_EXIT) {
                }
                else {
                    fprintf(stderr, "run time error\n");
                    fprintf(stderr, "%s", string_c_str(gErrMsg));
                }

                /// add history ///
                add_history(buf);
                history_num++;
                if(history_num > gHistSize) {
                    HIST_ENTRY* history = remove_history(0);
                    free(history);
                }

                if(*rcode != 0) {
                    fprintf(stderr, "return code is %d\n", *rcode);
                }
                (void)vector_pop_back(gStackFrames);
                stack_end_stack();

                /// wait background job
                xyzsh_wait_background_job();

                free(buf);

                return FALSE;
            }
            (void)vector_pop_back(gStackFrames);
            readline_signal();
        }
        else {
            xyzsh_restore_signal_default();

            fprintf(stderr, "parser error\n");
            fprintf(stderr, "%s", string_c_str(gErrMsg));

            /// add history ///
            add_history(buf);
            history_num++;
            if(history_num > gHistSize) {
                HIST_ENTRY* history = remove_history(0);
                free(history);
            }

            if(*rcode != 0) {
                fprintf(stderr, "return code is %d\n", *rcode);
            }
            stack_end_stack();

            /// wait background job
            xyzsh_wait_background_job();

            free(buf);

            return FALSE;
        }

        /// add history ///
        add_history(buf);
        history_num++;
        if(history_num > gHistSize) {
            HIST_ENTRY* history = remove_history(0);
            free(history);
        }

        if(*rcode != 0) {
            fprintf(stderr, "return code is %d\n", *rcode);
        }
        stack_end_stack();
    }

    /// wait background job
    xyzsh_wait_background_job();

    free(buf);

    return TRUE;
}

void xyzsh_readline_interface(char* cmdline, int cursor_point, char** argv, int argc)
{
    /// start interactive shell ///
    mreset_tty();
    xyzsh_job_done = main_xyzsh_job_done;

    char* buf;
    HISTORY_STATE* history_state = history_get_history_state();
    int history_num = history_state->length;
    BOOL first = TRUE;
    while(1) {
        /// prompt ///
        if(gPrompt) {
            if(first) {
                gCmdLine = cmdline;
                gCursorPoint = cursor_point;
                rl_startup_hook = readline_init_text;
                first = FALSE;
            }
            else {
                rl_startup_hook = NULL;
            }
            buf = prompt();
        }
        else {
            mreset_tty();
            if(first) {
                gCmdLine = cmdline;
                gCursorPoint = cursor_point;
                rl_startup_hook = readline_init_text;
                first = FALSE;
            }
            else {
                rl_startup_hook = NULL;
            }
            readline_signal();
            buf = readline("> ");
        }

        /// run ///
        if(buf == NULL) {
            if(vector_count(gJobs) > 0) {
                fprintf(stderr,"\njobs exist\n");
            }
            else {
                break;
            }
        }
        else if(*buf) {
            stack_start_stack();
            int rcode = 0;
            sObject* block = BLOCK_NEW_STACK();
            int sline = 1;
            if(parse(buf, "xyzsh", &sline, block, NULL)) {
                xyzsh_set_signal();

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

                if(!run(block, gStdin, gStdout, &rcode, gCurrentObject, fun)) {
                    readline_signal();
                    if(rcode == RCODE_BREAK) {
                        fprintf(stderr, "invalid break. Not in a loop\n");
                    }
                    else if(rcode == RCODE_RETURN) {
                        fprintf(stderr, "invalid return. Not in a function\n");
                    }
                    else if(rcode == RCODE_EXIT) {
                        stack_end_stack();
                        break;
                    }
                    else {
                        fprintf(stderr, "run time error\n");
                        fprintf(stderr, "%s", string_c_str(gErrMsg));
                    }
                }
                (void)vector_pop_back(gStackFrames);
                readline_signal();
            }
            else {
                fprintf(stderr, "parser error\n");
                fprintf(stderr, "%s", string_c_str(gErrMsg));
            }

            /// add history ///
            add_history(buf);
            history_num++;
            if(history_num > gHistSize) {
                HIST_ENTRY* history = remove_history(0);
                free(history);
            }

            if(rcode != 0) {
                fprintf(stderr, "return code is %d\n", rcode);
            }
            stack_end_stack();
        }

        /// wait background job
        xyzsh_wait_background_job();

        free(buf);
    }
}

BOOL xyzsh_load_file(char* fname, char** argv, int argc, sObject* current_object)
{
    xyzsh_set_signal_optc();
    sRunInfo runinfo;
    memset(&runinfo, 0, sizeof(sRunInfo));
    runinfo.mSName = "xyzsh";
    runinfo.mCurrentObject = current_object;
    if(!load_file(fname, gStdin, gStdout, &runinfo, argv, argc))
    {
        if(runinfo.mRCode == RCODE_BREAK) {
            fprintf(stderr, "invalid break. Not in a loop\n");
            xyzsh_restore_signal_default();
            return FALSE;
        }
        else if(runinfo.mRCode == RCODE_RETURN) {
            fprintf(stderr, "invalid return. Not in a function\n");
            xyzsh_restore_signal_default();
            return FALSE;
        }
        else if(runinfo.mRCode == RCODE_EXIT) {
        }
        else {
            fprintf(stderr, "run time error\n");
            fprintf(stderr, "%s", string_c_str(gErrMsg));

            xyzsh_restore_signal_default();

            return FALSE;
        }
    }

    if(runinfo.mRCode != 0) {
        fprintf(stderr, "return code is %d\n", runinfo.mRCode);
    }

    xyzsh_restore_signal_default();

    return TRUE;
}

void xyzsh_opt_c(char* cmd, char** argv, int argc)
{
    stack_start_stack();
    int rcode = 0;
    sObject* block = BLOCK_NEW_STACK();
    int sline = 1;
    if(parse(cmd, "xyzsh", &sline, block, NULL)) {
        xyzsh_set_signal_optc();

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

        if(!run(block, gStdin, gStdout, &rcode, gRootObject, fun)) {
            if(rcode == RCODE_BREAK) {
                fprintf(stderr, "invalid break. Not in a loop\n");
            }
            else if(rcode == RCODE_RETURN) {
                fprintf(stderr, "invalid return. Not in a function\n");
            }
            else if(rcode == RCODE_EXIT) {
            }
            else {
                fprintf(stderr, "run time error\n");
                fprintf(stderr, "%s", string_c_str(gErrMsg));
            }
        }
        (void)vector_pop_back(gStackFrames);
        xyzsh_restore_signal_default();
    }
    else {
        fprintf(stderr, "parser error\n");
        fprintf(stderr, "%s", string_c_str(gErrMsg));
    }

    if(rcode != 0) {
        fprintf(stderr, "return code is %d\n", rcode);
    }
    stack_end_stack();
}

char* xyzsh_job_title(int n)
{
    if(n < vector_count(gJobs) && n >= 0) {
        sObject* job = vector_item(gJobs, n);
        return SJOB(job).mName;
    }
    else {
        return NULL;
    }
}

int xyzsh_job_num()
{
    return vector_count(gJobs);
}

void xyzsh_add_inner_command(sObject* object, char* name, fXyzshNativeFun command, int arg_num, ...)
{
    sObject* nfun = NFUN_NEW_GC(command, NULL, TRUE);

    va_list args;
    va_start(args, arg_num);

    int i;
    for(i=0; i<arg_num; i++) {
        char* arg = va_arg(args, char*);
        (void)nfun_put_option_with_argument(nfun, STRDUP(arg));
    }
    va_end(args);

    uobject_put(object, name, nfun);
}

