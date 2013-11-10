#include "config.h"
#include <time.h>
#include <stdio.h>
#include <string.h>
#include <signal.h>
#include <limits.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <stdlib.h>
#include <pthread.h>
#include <sys/time.h>
#include "xyzsh.h"
//#include "temulator.h"

#if defined(HAVE_CURSES_H)
#include <curses.h>
#elif defined(HAVE_NCURSES_H)
#include <ncurses.h>
#elif defined(HAVE_NCURSES_NCURSES_H)
#include <ncurses/ncurses.h>
#endif

static void main_xyzsh_job_done(int job_num, char* job_title)
{
    printf("%d: %s done.\n", job_num+1, job_title);
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

BOOL xyzsh_eval(int* rcode, char* cmd, char* source_name, fXyzshJobDone xyzsh_job_done_, sObject* nextin, sObject* nextout, int argc, char** argv, sObject* current_object)
{
    string_put(gErrMsg, "");

    xyzsh_job_done = xyzsh_job_done_;

    stack_start_stack();

    sObject* block = BLOCK_NEW_STACK();
    int sline = 1;
    if(parse(cmd, source_name, &sline, block, NULL)) {
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
    }
    else {
        stack_end_stack();

        /// wait background job
        xyzsh_wait_background_job();

        return FALSE;
    }

    /// wait background job
    xyzsh_wait_background_job();

    return TRUE;
}

static ALLOC char* run_editline(char* text, int cursor_pos)
{
    stack_start_stack();

    char* buf;

    char* prompt;
    if(gPrompt) {
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
            else if(rcode & RCODE_RETURN) {
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

        (void)vector_pop_back(gStackFrames);

        prompt = SFD(nextout).mBuf;
    }
    else {
        prompt = " > ";
    }

    char* rprompt;
    if(gRPrompt) {
        sObject* fun = FUN_NEW_STACK(NULL);
        sObject* stackframe = UOBJECT_NEW_GC(8, gXyzshObject, "_stackframe", FALSE);
        vector_add(gStackFrames, stackframe);
        //uobject_init(stackframe);
        SFUN(fun).mLocalObjects = stackframe;

        sObject* nextout2 = FD_NEW_STACK();

        int rcode;
        if(!run(gRPrompt, gStdin, nextout2, &rcode, gCurrentObject, fun)) {
            if(rcode == RCODE_BREAK) {
                fprintf(stderr, "invalid break. Not in a loop\n");
            }
            else if(rcode & RCODE_RETURN) {
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
        (void)vector_pop_back(gStackFrames);

        rprompt = SFD(nextout2).mBuf;
    }
    else {
        rprompt = NULL;
    }

    mreset_tty();
    buf = ALLOC editline(prompt, rprompt, text, cursor_pos);

    stack_end_stack();

    return ALLOC buf;
}


// EOF --> rcode == -2
// errors --> rcode == -1
BOOL xyzsh_readline_interface_onetime(int* rcode, char* cmdline, int cursor_pos, char* source_name, char** argv, int argc, fXyzshJobDone xyzsh_job_done_)
{
    /// start interactive shell ///
    xyzsh_job_done = xyzsh_job_done_;

    /// edit line ///
    char* buf = ALLOC run_editline(cmdline, cursor_pos);

    /// run ///
    if(buf == NULL) {
        *rcode = -2;

        /// wait background job
        xyzsh_wait_background_job();
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
                else if(*rcode & RCODE_RETURN) {
                    fprintf(stderr, "invalid return. Not in a function\n");
                }
                else if(*rcode == RCODE_EXIT) {
                }
                else {
                    fprintf(stderr, "run time error\n");
                    fprintf(stderr, "%s", string_c_str(gErrMsg));
                }

                if(*rcode != 0) {
                    fprintf(stderr, "return code is %d\n", *rcode);
                }
                (void)vector_pop_back(gStackFrames);
                stack_end_stack();

                /// wait background job
                xyzsh_wait_background_job();

                FREE(buf);

                return FALSE;
            }
            (void)vector_pop_back(gStackFrames);
        }
        else {
            xyzsh_restore_signal_default();

            fprintf(stderr, "parser error\n");
            fprintf(stderr, "%s", string_c_str(gErrMsg));

            if(*rcode != 0) {
                fprintf(stderr, "return code is %d\n", *rcode);
            }
            stack_end_stack();

            /// wait background job
            xyzsh_wait_background_job();

            FREE(buf);

            return FALSE;
        }

        if(*rcode != 0) {
            fprintf(stderr, "return code is %d\n", *rcode);
        }
        stack_end_stack();

        /// wait background job
        xyzsh_wait_background_job();

        FREE(buf);
    }

    return TRUE;
}

struct sTEmulatorFunArg {
    char* cmdline;
    int cursor_point;
    char** argv;
    int argc;
    BOOL exit_in_spite_ofjob_exist;
    BOOL welcome_msg;
};

static void temulator_fun(void* arg)
{
    struct sTEmulatorFunArg* targ = arg;

    /// start interactive shell ///
    xyzsh_job_done = main_xyzsh_job_done;

    if(targ->welcome_msg) {
        char* version = getenv("XYZSH_VERSION");
        printf("-+- Welcome to xyzsh %s -+-\n", version);
        printf("run \"help\" command to see usage\n");
    }

    char* buf;
    BOOL first = TRUE;
    while(1) {
        /// prompt ///
        if(first) {
            buf = ALLOC run_editline(targ->cmdline, targ->cursor_point);
            first = FALSE;
        }
        else {
            buf = ALLOC run_editline(NULL, -1);
        }

        /// run ///
        if(buf == NULL) {
            if(targ->exit_in_spite_ofjob_exist) {
                break;
            }
            else {
                if(vector_count(gJobs) > 0) {
                    fprintf(stderr,"\njobs exist\n");
                }
                else {
                    break;
                }
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
                for(i=0; i<targ->argc; i++) {
                    vector_add(argv2, STRING_NEW_GC(targ->argv[i], FALSE));
                }
                uobject_put(SFUN(fun).mLocalObjects, "ARGV", argv2);

                if(!run(block, gStdin, gStdout, &rcode, gCurrentObject, fun)) {
                    if(rcode == RCODE_BREAK) {
                        fprintf(stderr, "invalid break. Not in a loop\n");
                    }
                    else if(rcode & RCODE_RETURN) {
                        fprintf(stderr, "invalid return. Not in a function\n");
                    }
                    else if(rcode == RCODE_EXIT) {
                        (void)vector_pop_back(gStackFrames);
                        stack_end_stack();
                        FREE(buf);
                        break;
                    }
                    else {
                        fprintf(stderr, "run time error\n");
                        fprintf(stderr, "%s", string_c_str(gErrMsg));
                    }
                }
                (void)vector_pop_back(gStackFrames);
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

        /// wait background job
        xyzsh_wait_background_job();

        FREE(buf);
    }
}

static BOOL gSigChld = FALSE;
static BOOL gSigWinch = FALSE;

static void handler(int signo)
{
    switch (signo) {
      case SIGCHLD:
        gSigChld = TRUE;
        break;
      case SIGWINCH:
        gSigWinch = TRUE;
        break;
    }
}

#include "temulator.h"

static int is_expired(struct timeval now, struct timeval expiry)
{
    return now.tv_sec > expiry.tv_sec
        || (now.tv_sec == expiry.tv_sec && now.tv_usec > expiry.tv_usec);
}

static struct timeval const slice = { 0, 1000 * 1000 / 100 };

static struct timeval timeval_add(struct timeval a, struct timeval b)
{
    int usec = a.tv_usec + b.tv_usec;
    a.tv_sec += b.tv_sec;
    while (usec > 1000 * 1000) {
        a.tv_sec += 1;
        usec -= 1000 * 1000;
    }
    a.tv_usec = usec;
    return a;
}


void xyzsh_readline_interface_on_curses(char* cmdline, int cursor_point, char** argv, int argc, BOOL exit_in_spite_ofjob_exist, BOOL welcome_msg)
{
    gSigChld = FALSE;
    gSigWinch = FALSE;

    signal(SIGCHLD, handler);
    signal(SIGWINCH, handler);

    const int maxx = mgetmaxx();
    const int maxy = mgetmaxy();

    int temulator_y = 0;
    int temulator_x = 0;
    int temulator_height = maxy;
    int temulator_width = maxx;

    sTEmulator* temulator = temulator_init(temulator_height, temulator_width);

    struct sTEmulatorFunArg arg;

    arg.cmdline = cmdline;
    arg.cursor_point = cursor_point;
    arg.argv = argv;
    arg.argc = argc;
    arg.exit_in_spite_ofjob_exist = exit_in_spite_ofjob_exist;
    arg.welcome_msg = welcome_msg;

    temulator_open(temulator, temulator_fun, &arg);

    initscr();
    start_color();
    noecho();
    raw();
    nodelay(stdscr, TRUE);
    keypad(stdscr, TRUE);
    curs_set(0);
#if !defined(__CYGWIN__)
    ESCDELAY=50;
#endif

    temulator_init_colors();

    WINDOW* term_win = newwin(temulator_height, temulator_width, temulator_y, temulator_x);

    int pty = temulator->mFD;

    fd_set mask, read_ok;
    FD_ZERO(&mask);
    FD_SET(0, &mask);
    FD_SET(pty, &mask);

    int dirty = 0;
    struct timeval next;

    gettimeofday(&next, NULL);
    while(1) {
        struct timeval tv = { 0, 1000 * 1000 / 100 };
        read_ok = mask;

        if(select(pty+1, &read_ok, NULL, NULL, &tv) > 0) {
            if(FD_ISSET(pty, &read_ok)) {
                temulator_read(temulator);
                dirty = 1;
            }
        }

        int key;
        while((key = getch()) != ERR) {
            temulator_write(temulator, key);
            dirty = 1;
        }

        gettimeofday(&tv, NULL);
        if(dirty && is_expired(tv, next)) {
            temulator_draw_on_curses(temulator, term_win, temulator_y, temulator_x);
            wrefresh(term_win);
            dirty = 0;
            next = timeval_add(tv, slice);
        }

        if(gSigChld) {
            gSigChld = FALSE;
            break;
        }

        if(gSigWinch) {
            gSigWinch = 0;

            temulator_height = mgetmaxy();
            temulator_width = mgetmaxx();

            if(temulator_width >= 10 && temulator_height >= 10) {
                resizeterm(temulator_height, temulator_width);

                wresize(term_win, temulator_height, temulator_width);
                temulator_resize(temulator, temulator_height, temulator_width);

                dirty = 1;
            }
        }
    }

    endwin();

    temulator_final(temulator);
}


void xyzsh_readline_interface(char* cmdline, int cursor_point, char** argv, int argc, BOOL exit_in_spite_ofjob_exist, BOOL welcome_msg)
{
    /// start interactive shell ///
    xyzsh_job_done = main_xyzsh_job_done;

    if(welcome_msg) {
        char* version = getenv("XYZSH_VERSION");
        printf("-+- Welcome to xyzsh %s -+-\n", version);
        printf("run \"help\" command to see usage\n");
    }

    char* buf;
    BOOL first = TRUE;
    while(1) {
        /// prompt ///
        if(first) {
            buf = ALLOC run_editline(cmdline, cursor_point);
            first = FALSE;
        }
        else {
            buf = ALLOC run_editline(NULL, -1);
        }

        /// run ///
        if(buf == NULL) {
            if(exit_in_spite_ofjob_exist) {
                break;
            }
            else {
                if(vector_count(gJobs) > 0) {
                    fprintf(stderr,"\njobs exist\n");
                }
                else {
                    break;
                }
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
                    if(rcode == RCODE_BREAK) {
                        fprintf(stderr, "invalid break. Not in a loop\n");
                    }
                    else if(rcode & RCODE_RETURN) {
                        fprintf(stderr, "invalid return. Not in a function\n");
                    }
                    else if(rcode == RCODE_EXIT) {
                        (void)vector_pop_back(gStackFrames);
                        stack_end_stack();
                        FREE(buf);
                        break;
                    }
                    else {
                        fprintf(stderr, "run time error\n");
                        fprintf(stderr, "%s", string_c_str(gErrMsg));
                    }
                }
                (void)vector_pop_back(gStackFrames);
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

        /// wait background job
        xyzsh_wait_background_job();

        FREE(buf);
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
        else if(runinfo.mRCode & RCODE_RETURN) {
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
            else if(rcode & RCODE_RETURN) {
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

