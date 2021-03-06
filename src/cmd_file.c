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
#include <pwd.h>

#if defined(HAVE_CURSES_H)
#include <curses.h>
#elif defined(HAVE_NCURSES_H)
#include <ncurses.h>
#elif defined(HAVE_NCURSES_NCURSES_H)
#include <ncurses/ncurses.h>
#endif

#include "xyzsh.h"

BOOL cmd_write(sObject* nextin, sObject* nextout, sRunInfo* runinfo)
{
    if(runinfo->mArgsNumRuntime == 2 && runinfo->mFilter) {
        char* fname = runinfo->mArgsRuntime[1];

        int fd;
        if(sRunInfo_option(runinfo, "-force")) {
            fd = open(fname, O_WRONLY|O_CREAT|O_TRUNC, 0644);
        }
        else if(sRunInfo_option(runinfo, "-append")) {
            fd = open(fname, O_WRONLY|O_APPEND);
        }
        else {
            if(access(fname, F_OK) == 0) {
                err_msg("The file exists. If you want to override, add -force option to \"write\" runinfo", runinfo->mSName, runinfo->mSLine);
                return FALSE;
            }

            fd = open(fname, O_WRONLY|O_CREAT|O_TRUNC, 0644);
        }

        if(sRunInfo_option(runinfo, "-error")) {
            if(!bufsiz_write(fd, SFD(gStderr).mBuf, SFD(gStderr).mBufLen)) {
                err_msg("signal interrupt", runinfo->mSName, runinfo->mSLine);
                runinfo->mRCode = RCODE_SIGNAL_INTERRUPT;
                return FALSE;
            }
            fd_clear(gStderr);

            if(!fd_write(nextout, SFD(nextin).mBuf, SFD(nextin).mBufLen)) {
                err_msg("signal interrupt", runinfo->mSName, runinfo->mSLine);
                runinfo->mRCode = RCODE_SIGNAL_INTERRUPT;
                return FALSE;
            }
        }
        else {
            if(!bufsiz_write(fd, SFD(nextin).mBuf, SFD(nextin).mBufLen)) {
                err_msg("signal interrupt", runinfo->mSName, runinfo->mSLine);
                runinfo->mRCode = RCODE_SIGNAL_INTERRUPT;
                return FALSE;
            }
        }

        (void)close(fd);

        if(SFD(nextin).mBufLen == 0) {
            runinfo->mRCode = RCODE_NFUN_NULL_INPUT;
        }
        else {
            runinfo->mRCode = 0;
        }
    }

    return TRUE;
}

static void mygetcwd(char* result, int result_size)
{
    int l;
    
    l = 50;
    while(!getcwd(result, l)) {
         l += 50;
         if(l+1 >= result_size) {
            break;
         }
    }
}

static int correct_path(char* current_path, char* path, char* path2, int path2_size)
{
    char tmp[PATH_MAX];
    char tmp2[PATH_MAX];
    char tmp3[PATH_MAX];
    char tmp4[PATH_MAX];

    /// add current path to head ///
    {
        if(path[0] == '/') {
            xstrncpy(tmp, path, PATH_MAX);
        }
        else {
            if(current_path == NULL) {
                char cwd[PATH_MAX];
                mygetcwd(cwd, PATH_MAX);
                
                xstrncpy(tmp, cwd, PATH_MAX);
                xstrncat(tmp, "/", PATH_MAX);
                xstrncat(tmp, path, PATH_MAX);
            }
            else {
                xstrncpy(tmp, current_path, PATH_MAX);
                if(current_path[strlen(current_path)-1] != '/') {
                    xstrncat(tmp, "/", PATH_MAX);
                }
                xstrncat(tmp, path, PATH_MAX);
            }
        }

    }

    xstrncpy(tmp2, tmp, PATH_MAX);

    /// delete . ///
    {
        char* p;
        char* p2;
        int i;

        p = tmp3;
        p2 = tmp2;
        while(*p2) {
            if(*p2 == '/' && *(p2+1) == '.' && *(p2+2) != '.' && ((*p2+3)=='/' || *(p2+3)==0)) {
                p2+=2;
            }
            else {
                *p++ = *p2++;
            }
        }
        *p = 0;

        if(*tmp3 == 0) {
            *tmp3 = '/';
            *(tmp3+1) = 0;
        }

    }

    /// delete .. ///
    {
        char* p;
        char* p2;

        p = tmp4;
        p2 = tmp3;

        while(*p2) {
            if(*p2 == '/' && *(p2+1) == '.' && *(p2+2) == '.' 
                && *(p2+3) == '/')
            {
                p2 += 3;

                do {
                    p--;

                    if(p < tmp4) {
                        xstrncpy(path2, "/", path2_size);
                        return FALSE;
                    }
                } while(*p != '/');

                *p = 0;
            }
            else if(*p2 == '/' && *(p2+1) == '.' && *(p2+2) == '.' 
                && *(p2+3) == 0) 
            {
                do {
                    p--;

                    if(p < tmp4) {
                        xstrncpy(path2, "/", path2_size);
                        return FALSE;
                    }
                } while(*p != '/');

                *p = 0;
                break;
            }
            else {
                *p++ = *p2++;
            }
        }
        *p = 0;
    }

    if(*tmp4 == 0) {
        xstrncpy(path2, "/", path2_size);
    }
    else {
        xstrncpy(path2, tmp4, path2_size);
    }

    return TRUE;
}

BOOL cmd_cd(sObject* nextin, sObject* nextout, sRunInfo* runinfo)
{
    if(runinfo->mArgsNumRuntime == 1) {
        char* path = getenv("HOME");
        if(path) {
            struct stat stat_;
            if(stat(path, &stat_) == 0) {
                if(S_ISDIR(stat_.st_mode)) {
                    setenv("PWD", path, 1);
                    if(chdir(path) >= 0) {
                        runinfo->mRCode = 0;
                    }
                }
            }
        }
    }
    else if(runinfo->mArgsNumRuntime == 2) {
        char* path = runinfo->mArgsRuntime[1];
        char cwd[PATH_MAX];
        char path2[PATH_MAX];

        mygetcwd(cwd, PATH_MAX);

        if(correct_path(cwd, path, path2, PATH_MAX)) {
            struct stat stat_;
            if(stat(path2, &stat_) == 0) {
                if(S_ISDIR(stat_.st_mode)) {
                    setenv("PWD", path2, 1);
                    if(chdir(path2) >= 0) {
                        runinfo->mRCode = 0;
                    }
                }
            }
        }
    }

    return TRUE;
}

BOOL cmd_popd(sObject* nextin, sObject* nextout, sRunInfo* runinfo)
{
    if(vector_count(gDirStack) > 0) {
        sObject* pwd = vector_pop_back(gDirStack);

        if(pwd) {
            setenv("PWD", string_c_str(pwd), 1);
            if(chdir(string_c_str(pwd)) >= 0) {
                runinfo->mRCode = 0;
            }
        }
    }

    return TRUE;
}

BOOL cmd_pushd(sObject* nextin, sObject* nextout, sRunInfo* runinfo)
{
    if(runinfo->mArgsNumRuntime == 2) {
        char* path = runinfo->mArgsRuntime[1];
        char cwd[PATH_MAX];
        char path2[PATH_MAX];

        mygetcwd(cwd, PATH_MAX);
        if(correct_path(cwd, path, path2, PATH_MAX)) {
            struct stat stat_;

            if(stat(path2, &stat_) == 0) {
                if(S_ISDIR(stat_.st_mode)) {
                    vector_add(gDirStack, STRING_NEW_GC(path2, FALSE));

                    if(!fd_write(nextout, path2, strlen(path2))) {
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
        }
    }

    return TRUE;
}

BOOL cmd_expand_tilda(sObject* nextin, sObject* nextout, sRunInfo* runinfo)
{
    if(runinfo->mFilter) {
        char* target = SFD(nextin).mBuf;
        if(target[0] == '~') {
            struct passwd* pw;

            sObject* pw_dirs = HASH_NEW_STACK(10);
            pw = getpwuid(getuid());
            hash_put(pw_dirs, "~/", STRING_NEW_STACK(pw->pw_dir));

            while((pw = getpwent()) != NULL) {
                char home_dir[128];
                snprintf(home_dir, 128, "~%s/", pw->pw_name);
                hash_put(pw_dirs, home_dir, STRING_NEW_STACK(pw->pw_dir));
            }

            endpwent();

            BOOL found = FALSE;

            hash_it* it = hash_loop_begin(pw_dirs);
            while(it) {
                char* key = hash_loop_key(it);

                if(strstr(target, key) == target) {
                    sObject* item = hash_loop_item(it);

                    if(!fd_write(nextout, string_c_str(item), string_length(item))) {
                        err_msg("signal interrupt", runinfo->mSName, runinfo->mSLine);
                        runinfo->mRCode = RCODE_SIGNAL_INTERRUPT;
                        return FALSE;
                    }
                    if(!fd_writec(nextout, '/')) {
                        err_msg("signal interrupt", runinfo->mSName, runinfo->mSLine);
                        runinfo->mRCode = RCODE_SIGNAL_INTERRUPT;
                        return FALSE;
                    }
                    char* p = target + strlen(key);
                    if(!fd_write(nextout, p, strlen(p))) {
                        err_msg("signal interrupt", runinfo->mSName, runinfo->mSLine);
                        runinfo->mRCode = RCODE_SIGNAL_INTERRUPT;
                        return FALSE;
                    }

                    found = TRUE;
                    break;
                }

                it = hash_loop_next(it);
            }

            if(!found) {
                if(!fd_write(nextout, target, SFD(nextin).mBufLen)) {
                    err_msg("signal interrupt", runinfo->mSName, runinfo->mSLine);
                    runinfo->mRCode = RCODE_SIGNAL_INTERRUPT;
                    return FALSE;
                }
            }
        }
        else {
            if(!fd_write(nextout, target, SFD(nextin).mBufLen)) {
                err_msg("signal interrupt", runinfo->mSName, runinfo->mSLine);
                runinfo->mRCode = RCODE_SIGNAL_INTERRUPT;
                return FALSE;
            }
        }

        runinfo->mRCode = 0;
    }

    return TRUE;
}


