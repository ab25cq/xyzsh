#include "config.h"
#include "xyzsh.h"
#include <string.h>
#include <stdio.h>
#include <signal.h>
#include <sys/stat.h>
#include <locale.h>
#include <fcntl.h>

sObject* gErrMsg;

volatile BOOL gXyzshSigInt;
volatile BOOL gXyzshSigUser;
volatile BOOL gXyzshSigTstp;
volatile BOOL gXyzshSigCont;

sObject* gDirStack;

void xyzsh_read_rc_core(char* path)
{
    xyzsh_set_signal_optc();

    sRunInfo runinfo;
    char** argv = NULL;
    int argc = 0;

    if(path && access(path, R_OK) == 0) {
        memset(&runinfo, 0, sizeof(sRunInfo));
        runinfo.mSName = path;
        runinfo.mCurrentObject = gRootObject;

        if(!load_file(path, gStdin, gStdout, &runinfo, argv, argc))
        {
            if(runinfo.mRCode == RCODE_BREAK) {
                fprintf(stderr, "invalid break. Not in a loop\n");
                exit(1);
            }
            else if(runinfo.mRCode & RCODE_RETURN) {
                fprintf(stderr, "invalid return. Not in a function\n");
                exit(1);
            }
            else if(runinfo.mRCode == RCODE_EXIT) {
            }
            else {
                fprintf(stderr, "run time error\n");
                fprintf(stderr, "%s", string_c_str(gErrMsg));
                exit(1);
            }
        }

        if(runinfo.mRCode != 0) {
            fprintf(stderr, "return code is %d\n", runinfo.mRCode);
            exit(1);
        }
    }

    xyzsh_restore_signal_default();
}

static void xyzsh_read_rc()
{
    char sys_rc_path[PATH_MAX];
    snprintf(sys_rc_path, PATH_MAX, "%sxyzsh.xyzsh", SYSCONFDIR);

    char help_rc_path[PATH_MAX];
    snprintf(help_rc_path, PATH_MAX, "%shelp.xyzsh", SYSCONFDIR);

    char completion_rc_path[PATH_MAX];
    snprintf(completion_rc_path, PATH_MAX, "%scompletion.xyzsh", SYSCONFDIR);

    char user_rc_path[PATH_MAX];
    char* home;
    if(home = getenv("HOME")) {
        snprintf(user_rc_path, PATH_MAX, "%s/.xyzsh/xyzsh.xyzsh", home);
    }
    else {
        strncpy(user_rc_path, "", PATH_MAX);
    }

    xyzsh_read_rc_core(sys_rc_path);
    xyzsh_read_rc_core(help_rc_path);
    xyzsh_read_rc_core(completion_rc_path);
    xyzsh_read_rc_core(user_rc_path);
}

static void xyzsh_read_rc_mini()
{
    char sys_rc_path[PATH_MAX];
    snprintf(sys_rc_path, PATH_MAX, "%sxyzsh.xyzsh", SYSCONFDIR);

    xyzsh_read_rc_core(sys_rc_path);
}

void xyzsh_init(enum eAppType app_type, BOOL no_runtime_script)
{
    setenv("XYZSH_VERSION", "1.5.8", 1);
    setenv("XYZSH_DOCDIR", DOCDIR, 1);
    setenv("XYZSH_DATAROOTDIR", DOCDIR, 1);
    setenv("XYZSH_EXT_PATH", EXTDIR, 1);
    setenv("XYZSH_SYSCONFDIR", SYSCONFDIR, 1);

    char* home = getenv("HOME");
    if(home) {
        char home_library[PATH_MAX];
        snprintf(home_library, PATH_MAX, "%s/.xyzsh/lib/", home);

        char* ld_library_path = getenv("LD_LIBRARY_PATH");
        if(ld_library_path) {
            char ld_library_path2[512];
            snprintf(ld_library_path2, 512, "%s:%s:%s", ld_library_path, EXTDIR, home_library);

            setenv("LD_LIBRARY_PATH", ld_library_path2, 1);
        }
        else {
            char ld_library_path2[512];
            snprintf(ld_library_path2, 512, "%s:%s", EXTDIR, home_library);

            setenv("LD_LIBRARY_PATH", ld_library_path2, 1);
        }
    }
    else {
        char* ld_library_path = getenv("LD_LIBRARY_PATH");

        if(ld_library_path) {
            char ld_library_path2[512];
            snprintf(ld_library_path2, 512, "%s:%s", ld_library_path, EXTDIR);

            setenv("LD_LIBRARY_PATH", ld_library_path2, 1);
        }
        else {
            char ld_library_path2[512];
            snprintf(ld_library_path2, 512, "%s", EXTDIR);
            setenv("LD_LIBRARY_PATH", ld_library_path2, 1);
        }
    }

    setlocale(LC_ALL, "");

    stack_init(1);;
    stack_start_stack();

    gErrMsg = STRING_NEW_STACK("");

    gXyzshSigInt = FALSE;
    gXyzshSigUser = FALSE;
    gXyzshSigTstp = FALSE;
    gXyzshSigCont = FALSE;

    xyzsh_set_signal_other = NULL;

    gc_init(1);
    run_init(app_type);
    load_init();
    xyzsh_editline_init();

    gDirStack = VECTOR_NEW_GC(10, FALSE);
    uobject_put(gXyzshObject, "_dir_stack", gDirStack);

    char* term_env = getenv("TERM");
    if(term_env != NULL && strcmp(term_env, "") != 0) {
        mcurses_init();
    }

    if(!xyzsh_rehash("init", 0)) {
        fprintf(stderr, "run time error\n");
        fprintf(stderr, "%s", string_c_str(gErrMsg));
        exit(1);
    }

    if(!no_runtime_script) {
        xyzsh_read_rc();
    }
    else {
        xyzsh_read_rc_mini();
    }
}

void xyzsh_final()
{
    xyzsh_editline_final();

    mcurses_final();
    load_final();
    run_final();
    gc_final();

    stack_end_stack();
    stack_final();
}

void err_msg(char* msg, char* sname, int line)
{
    char* tmp = MALLOC(strlen(sname) + 128 + strlen(msg));
    snprintf(tmp, strlen(sname) + 128 + strlen(msg), "%s %d: %s\n", sname,  line, msg);

    string_put(gErrMsg, tmp);
    FREE(tmp);
}

void err_msg_adding(char* msg, char* sname, int line)
{
    char* tmp = MALLOC(strlen(sname) + 128 + strlen(msg));
    snprintf(tmp, strlen(sname) + 128 + strlen(msg), "%s %d: %s\n", sname,  line, msg);

    string_push_back(gErrMsg, tmp);
    FREE(tmp);
}

void sig_int()
{
    gXyzshSigInt = TRUE; 

#ifdef MDEBUG
fprintf(stderr, "SIGINT!!\n");
#endif
}

void sig_tstp()
{
    gXyzshSigInt = TRUE; 
#ifdef MDEBUG
fprintf(stderr, "SIGTSTP!!\n");
#endif
}

void sig_int_optc()
{
    sigchld_block(1);
    gXyzshSigInt = TRUE; 
    //killpg(0, SIGINT);
#ifdef MDEBUG
//fprintf(stderr, "SIGINT!!\n");
#endif
    sigchld_block(0);
}

void sig_tstp_optc()
{
    sigchld_block(1);
#ifdef MDEBUG
fprintf(stderr, "SIGTSTP!!\n");
#endif
    //msave_ttysettings();
    killpg(0, SIGSTOP);
    sigchld_block(0);
}

void sig_cont_optc()
{
    sigchld_block(1);
    //mrestore_ttysettings();
    sigchld_block(0);
}

void sig_user()
{
    gXyzshSigUser = TRUE;
}

void sigchld_action(int signum, siginfo_t* info, void* ctx)
{
}

void sigchld_block(int block)
{
    sigset_t sigset;

    sigemptyset(&sigset);
    sigaddset(&sigset, SIGCHLD);

    if(sigprocmask(block?SIG_BLOCK:SIG_UNBLOCK, &sigset, NULL) != 0)
    {
        fprintf(stderr, "error\n");
        exit(1);
    }
}

void sigttou_block(int block)
{
    sigset_t sigset;

    sigemptyset(&sigset);
    sigaddset(&sigset, SIGTTOU);

    if(sigprocmask(block?SIG_BLOCK:SIG_UNBLOCK, &sigset, NULL) != 0)
    {
        fprintf(stderr, "error\n");
        exit(1);
    }
}

void xyzsh_restore_signal_default()
{
    struct sigaction sa;

    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = SIG_DFL;
    if(sigaction(SIGPIPE, &sa, NULL) < 0) {
        perror("sigaction1");
        exit(1);
    }
    if(sigaction(SIGCHLD, &sa, NULL) < 0) {
        perror("sigaction1");
        exit(1);
    }
    if(sigaction(SIGINT, &sa, NULL) < 0) {
        perror("sigaction2");
        exit(1);
    }
    if(sigaction(SIGCONT, &sa, NULL) < 0) {
        perror("sigaction3");
        exit(1);
    }
    if(sigaction(SIGWINCH, &sa, NULL) < 0) {
        perror("sigaction4");
        exit(1);
    }

    if(sigaction(SIGPROF, &sa, NULL) < 0) {
        perror("sigaction5");
        exit(1);
    }

    if(sigaction(SIGTTIN, &sa, NULL) < 0) {
        perror("sigaction6");
        exit(1);
    }
    if(sigaction(SIGTTOU, &sa, NULL) < 0) {
        perror("sigaction7");
        exit(1);
    }
    if(sigaction(SIGTSTP, &sa, NULL) < 0) {
        perror("sigaction8");
        exit(1);
    }
    if(sigaction(SIGQUIT, &sa, NULL) < 0) {
        perror("sigaction9");
        exit(1);
    }
    if(sigaction(SIGUSR1, &sa, NULL) < 0) {
        perror("sigaction10");
        exit(1);
    }
}

void (*xyzsh_set_signal_other)();

void xyzsh_set_signal()
{
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_sigaction = sigchld_action;
    sa.sa_flags = SA_RESTART|SA_SIGINFO;
    if(sigaction(SIGCHLD, &sa, NULL) < 0) {
        perror("sigaction1");
        exit(1);
    }

    memset(&sa, 0, sizeof(sa));
    sa.sa_flags = SA_SIGINFO;
    sa.sa_handler = sig_int;
    if(sigaction(SIGINT, &sa, NULL) < 0) {
        perror("sigaction2");
        exit(1);
    }

    memset(&sa, 0, sizeof(sa));
    if(sigaction(SIGCONT, &sa, NULL) < 0) {
        perror("sigaction3");
        exit(1);
    }

    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = SIG_DFL;
    if(sigaction(SIGWINCH, &sa, NULL) < 0) {
        perror("sigaction4");
        exit(1);
    }

    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = SIG_IGN;
    sa.sa_flags = 0;
    if(sigaction(SIGTTOU, &sa, NULL) < 0) {
        perror("sigaction5");
        exit(1);
    }

    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = SIG_IGN;
    sa.sa_flags = 0;
    if(sigaction(SIGTTIN, &sa, NULL) < 0) {
        perror("sigaction6");
        exit(1);
    }

    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = sig_tstp;
    sa.sa_flags = 0;
    if(sigaction(SIGTSTP, &sa, NULL) < 0) {
        perror("sigaction7");
        exit(1);
    }

    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = SIG_IGN;
    sa.sa_flags = 0;
    if(sigaction(SIGQUIT, &sa, NULL) < 0) {
        perror("sigaction8");
        exit(1);
    }

    memset(&sa, 0, sizeof(sa));
    sa.sa_sigaction = sig_user;
    sa.sa_flags = SA_RESTART;
    if(sigaction(SIGUSR1, &sa, NULL) < 0) {
        perror("sigaction9");
        exit(1);
    }
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = SIG_IGN;
    sa.sa_flags = 0;
    if(sigaction(SIGPIPE, &sa, NULL) < 0) {
        perror("sigaction10");
        exit(1);
    }

/*
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = SIG_DFL;
    sa.sa_flags = SA_RESTART;
    if(sigaction(SIGSTOP, &sa, NULL) < 0) {
        perror("sigaction11");
        exit(1);
    }
*/

    if(xyzsh_set_signal_other) xyzsh_set_signal_other();
}

void xyzsh_set_signal_optc()
{
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_sigaction = sigchld_action;
    sa.sa_flags = SA_SIGINFO|SA_RESTART;
    if(sigaction(SIGCHLD, &sa, NULL) < 0) {
        perror("sigaction1");
        exit(1);
    }

    memset(&sa, 0, sizeof(sa));
    sa.sa_flags = SA_SIGINFO;
    sa.sa_handler = sig_int_optc;
    if(sigaction(SIGINT, &sa, NULL) < 0) {
        perror("sigaction2");
        exit(1);
    }

    memset(&sa, 0, sizeof(sa));
    sa.sa_sigaction = sig_tstp_optc;
    sa.sa_flags = SA_RESTART;
    if(sigaction(SIGTSTP, &sa, NULL) < 0) {
        perror("sigaction7");
        exit(1);
    }

    memset(&sa, 0, sizeof(sa));
    sa.sa_sigaction = sig_cont_optc;
    sa.sa_flags = SA_RESTART;
    if(sigaction(SIGCONT, &sa, NULL) < 0) {
        perror("sigaction3");
        exit(1);
    }

    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = SIG_DFL;
    if(sigaction(SIGWINCH, &sa, NULL) < 0) {
        perror("sigaction4");
        exit(1);
    }

    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = SIG_IGN;
    sa.sa_flags = 0;
    if(sigaction(SIGTTOU, &sa, NULL) < 0) {
        perror("sigaction5");
        exit(1);
    }

    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = SIG_IGN;
    sa.sa_flags = 0;
    if(sigaction(SIGTTIN, &sa, NULL) < 0) {
        perror("sigaction6");
        exit(1);
    }

    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = SIG_IGN;
    sa.sa_flags = 0;
    if(sigaction(SIGQUIT, &sa, NULL) < 0) {
        perror("sigaction8");
        exit(1);
    }

    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = sig_user;
    sa.sa_flags = 0;
    if(sigaction(SIGUSR1, &sa, NULL) < 0) {
        perror("sigaction9");
        exit(1);
    }
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = SIG_IGN;
    sa.sa_flags = 0;
    if(sigaction(SIGPIPE, &sa, NULL) < 0) {
        perror("sigaction10");
        exit(1);
    }

/*
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = SIG_DFL;
    sa.sa_flags = SA_RESTART;
    if(sigaction(SIGSTOP, &sa, NULL) < 0) {
        perror("sigaction11");
        exit(1);
    }
*/

    if(xyzsh_set_signal_other) xyzsh_set_signal_other();
}
