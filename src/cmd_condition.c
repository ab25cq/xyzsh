#include "config.h"
#include <errno.h>
#include <time.h>
#include <stdlib.h>
#include <fcntl.h>
#include <libgen.h>
#include <stdio.h>
//#define _GNU_SOURCE 1
//#define __USE_GNU 1
#include <string.h>
#include <unistd.h>
#include <math.h>
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

BOOL cmd_condition_n(sObject* nextin, sObject* nextout, sRunInfo* runinfo)
{
    if(runinfo->mFilter) {
        runinfo->mRCode = RCODE_NFUN_FALSE;
        if(SFD(nextin).mBufLen > 0) {
            runinfo->mRCode = 0;
        }
    }

    return TRUE;
}

BOOL cmd_condition_z(sObject* nextin, sObject* nextout, sRunInfo* runinfo)
{
    if(runinfo->mFilter) {
        runinfo->mRCode = RCODE_NFUN_FALSE;
        if(SFD(nextin).mBufLen == 0) {
            runinfo->mRCode = 0;
        }
    }

    return TRUE;
}

BOOL cmd_condition_b(sObject* nextin, sObject* nextout, sRunInfo* runinfo)
{
    if(runinfo->mFilter) {
        runinfo->mRCode = RCODE_NFUN_FALSE;
        struct stat stat_;
        if(stat(SFD(nextin).mBuf, &stat_) == 0) {
            if(S_ISBLK(stat_.st_mode)) {
                runinfo->mRCode = 0;
            }
        }
    }

    return TRUE;
}

BOOL cmd_condition_c(sObject* nextin, sObject* nextout, sRunInfo* runinfo)
{
    if(runinfo->mFilter) {
        runinfo->mRCode = RCODE_NFUN_FALSE;
        struct stat stat_;
        if(stat(SFD(nextin).mBuf, &stat_) == 0) {
            if(S_ISCHR(stat_.st_mode)) {
                runinfo->mRCode = 0;
            }
        }
    }

    return TRUE;
}

BOOL cmd_condition_d(sObject* nextin, sObject* nextout, sRunInfo* runinfo)
{
    if(runinfo->mFilter) {
        runinfo->mRCode = RCODE_NFUN_FALSE;
        struct stat stat_;
        if(stat(SFD(nextin).mBuf, &stat_) == 0) {
            if(S_ISDIR(stat_.st_mode)) {
                runinfo->mRCode = 0;
            }
        }
    }

    return TRUE;
}

BOOL cmd_condition_f(sObject* nextin, sObject* nextout, sRunInfo* runinfo)
{
    if(runinfo->mFilter) {
        runinfo->mRCode = RCODE_NFUN_FALSE;
        struct stat stat_;
        if(stat(SFD(nextin).mBuf, &stat_) == 0) {
            if(S_ISREG(stat_.st_mode)) {
                runinfo->mRCode = 0;
            }
        }
    }

    return TRUE;
}

BOOL cmd_condition_h(sObject* nextin, sObject* nextout, sRunInfo* runinfo)
{
    if(runinfo->mFilter) {
        runinfo->mRCode = RCODE_NFUN_FALSE;
        struct stat stat_;
        if(stat(SFD(nextin).mBuf, &stat_) == 0) {
            if(S_ISLNK(stat_.st_mode)) {
                runinfo->mRCode = 0;
            }
        }
    }

    return TRUE;
}

BOOL cmd_condition_l(sObject* nextin, sObject* nextout, sRunInfo* runinfo)
{
    if(runinfo->mFilter) {
        runinfo->mRCode = RCODE_NFUN_FALSE;
        struct stat stat_;
        if(stat(SFD(nextin).mBuf, &stat_) == 0) {
            if(S_ISLNK(stat_.st_mode)) {
                runinfo->mRCode = 0;
            }
        }
    }

    return TRUE;
}

BOOL cmd_condition_p(sObject* nextin, sObject* nextout, sRunInfo* runinfo)
{
    if(runinfo->mFilter) {
        runinfo->mRCode = RCODE_NFUN_FALSE;
        struct stat stat_;
        if(stat(SFD(nextin).mBuf, &stat_) == 0) {
            if(S_ISFIFO(stat_.st_mode)) {
                runinfo->mRCode = 0;
            }
        }
    }

    return TRUE;
}

BOOL cmd_condition_t(sObject* nextin, sObject* nextout, sRunInfo* runinfo)
{
    runinfo->mRCode = RCODE_NFUN_FALSE;
    return TRUE;
}

BOOL cmd_condition_s2(sObject* nextin, sObject* nextout, sRunInfo* runinfo)
{
    if(runinfo->mFilter) {
        runinfo->mRCode = RCODE_NFUN_FALSE;
        struct stat stat_;
        if(stat(SFD(nextin).mBuf, &stat_) == 0) {
            if(S_ISSOCK(stat_.st_mode)) {
                runinfo->mRCode = 0;
            }
        }
    }

    return TRUE;
}

BOOL cmd_condition_g(sObject* nextin, sObject* nextout, sRunInfo* runinfo)
{
    if(runinfo->mFilter) {
        runinfo->mRCode = RCODE_NFUN_FALSE;
        struct stat stat_;
        if(stat(SFD(nextin).mBuf, &stat_) == 0) {
            if(stat_.st_mode & S_ISGID) {
                runinfo->mRCode = 0;
            }
        }
    }

    return TRUE;
}

BOOL cmd_condition_k(sObject* nextin, sObject* nextout, sRunInfo* runinfo)
{
    if(runinfo->mFilter) {
        runinfo->mRCode = RCODE_NFUN_FALSE;
        struct stat stat_;
        if(stat(SFD(nextin).mBuf, &stat_) == 0) {
#if defined(S_ISTXT)
            if(stat_.st_mode & S_ISTXT) {
                runinfo->mRCode = 0;
            }
#endif
#if defined(S_ISVTX)
            if(stat_.st_mode & S_ISVTX) {
                runinfo->mRCode = 0;
            }
#endif
        }
    }

    return TRUE;
}

BOOL cmd_condition_u(sObject* nextin, sObject* nextout, sRunInfo* runinfo)
{
    if(runinfo->mFilter) {
        runinfo->mRCode = RCODE_NFUN_FALSE;
        struct stat stat_;
        if(stat(SFD(nextin).mBuf, &stat_) == 0) {
            if(stat_.st_mode & S_ISUID) {
                runinfo->mRCode = 0;
            }
        }
    }

    return TRUE;
}

BOOL cmd_condition_r(sObject* nextin, sObject* nextout, sRunInfo* runinfo)
{
    if(runinfo->mFilter) {
        runinfo->mRCode = RCODE_NFUN_FALSE;
        if(access(SFD(nextin).mBuf, R_OK) == 0) {
            runinfo->mRCode = 0;
        }
    }

    return TRUE;
}

BOOL cmd_condition_w(sObject* nextin, sObject* nextout, sRunInfo* runinfo)
{
    if(runinfo->mFilter) {
        runinfo->mRCode = RCODE_NFUN_FALSE;
        if(access(SFD(nextin).mBuf, W_OK) == 0) {
            runinfo->mRCode = 0;
        }
    }

    return TRUE;
}

BOOL cmd_condition_x(sObject* nextin, sObject* nextout, sRunInfo* runinfo)
{
    if(runinfo->mFilter) {
        runinfo->mRCode = RCODE_NFUN_FALSE;
        if(access(SFD(nextin).mBuf, X_OK) == 0) {
            runinfo->mRCode = 0;
        }
    }

    return TRUE;
}

BOOL cmd_condition_o(sObject* nextin, sObject* nextout, sRunInfo* runinfo)
{
    if(runinfo->mFilter) {
        runinfo->mRCode = RCODE_NFUN_FALSE;
        struct stat stat_;
        if(stat(SFD(nextin).mBuf, &stat_) == 0) {
            if(stat_.st_uid == getuid()) {
                runinfo->mRCode = 0;
            }
        }
    }

    return TRUE;
}

BOOL cmd_condition_g2(sObject* nextin, sObject* nextout, sRunInfo* runinfo)
{
    if(runinfo->mFilter) {
        runinfo->mRCode = RCODE_NFUN_FALSE;
        struct stat stat_;
        if(stat(SFD(nextin).mBuf, &stat_) == 0) {
            if(stat_.st_gid == getgid()) {
                runinfo->mRCode = 0;
            }
        }
    }

    return TRUE;
}

BOOL cmd_condition_e(sObject* nextin, sObject* nextout, sRunInfo* runinfo)
{
    if(runinfo->mFilter) {
        runinfo->mRCode = RCODE_NFUN_FALSE;
        struct stat stat_;
        if(stat(SFD(nextin).mBuf, &stat_) == 0) {
            runinfo->mRCode = 0;
        }
    }

    return TRUE;
}

BOOL cmd_condition_s(sObject* nextin, sObject* nextout, sRunInfo* runinfo)
{
    if(runinfo->mFilter) {
        runinfo->mRCode = RCODE_NFUN_FALSE;
        struct stat stat_;
        if(stat(SFD(nextin).mBuf, &stat_) == 0) {
            if(stat_.st_size > 0) {
                runinfo->mRCode = 0;
            }
        }
    }

    return TRUE;
}

BOOL cmd_condition_eq(sObject* nextin, sObject* nextout, sRunInfo* runinfo)
{
    if(runinfo->mFilter && runinfo->mArgsNumRuntime == 2) {
        runinfo->mRCode = RCODE_NFUN_FALSE;
        if(sRunInfo_option(runinfo, "-ignore-case")) {
            if(strcasecmp(SFD(nextin).mBuf, runinfo->mArgsRuntime[1]) == 0) {
                runinfo->mRCode = 0;
            }
        }
        else {
            if(strcmp(SFD(nextin).mBuf, runinfo->mArgsRuntime[1]) == 0) {
                runinfo->mRCode = 0;
            }
        }
    }

    return TRUE;
}

BOOL cmd_condition_neq(sObject* nextin, sObject* nextout, sRunInfo* runinfo)
{
    if(runinfo->mFilter && runinfo->mArgsNumRuntime == 2) {
        runinfo->mRCode = RCODE_NFUN_FALSE;
        if(sRunInfo_option(runinfo, "-ignore-case")) {
            if(strcasecmp(SFD(nextin).mBuf, runinfo->mArgsRuntime[1]) != 0) {
                runinfo->mRCode = 0;
            }
        }
        else {
            if(strcmp(SFD(nextin).mBuf, runinfo->mArgsRuntime[1]) != 0) {
                runinfo->mRCode = 0;
            }
        }
    }

    return TRUE;
}

BOOL cmd_condition_slt(sObject* nextin, sObject* nextout, sRunInfo* runinfo)
{
    if(runinfo->mFilter && runinfo->mArgsNumRuntime == 2) {
        runinfo->mRCode = RCODE_NFUN_FALSE;
        if(sRunInfo_option(runinfo, "-ignore-case")) {
            if(strcasecmp(SFD(nextin).mBuf, runinfo->mArgsRuntime[1]) < 0) {
                runinfo->mRCode = 0;
            }
        }
        else {
            if(strcmp(SFD(nextin).mBuf, runinfo->mArgsRuntime[1]) < 0) {
                runinfo->mRCode = 0;
            }
        }
    }

    return TRUE;
}

BOOL cmd_condition_sgt(sObject* nextin, sObject* nextout, sRunInfo* runinfo)
{
    if(runinfo->mFilter && runinfo->mArgsNumRuntime == 2) {
        runinfo->mRCode = RCODE_NFUN_FALSE;
        if(sRunInfo_option(runinfo, "-ignore-case")) {
            if(strcasecmp(SFD(nextin).mBuf, runinfo->mArgsRuntime[1]) > 0) {
                runinfo->mRCode = 0;
            }
        }
        else {
            if(strcmp(SFD(nextin).mBuf, runinfo->mArgsRuntime[1]) > 0) {
                runinfo->mRCode = 0;
            }
        }
    }

    return TRUE;
}

BOOL cmd_condition_sle(sObject* nextin, sObject* nextout, sRunInfo* runinfo)
{
    if(runinfo->mFilter && runinfo->mArgsNumRuntime == 2) {
        runinfo->mRCode = RCODE_NFUN_FALSE;
        if(sRunInfo_option(runinfo, "-ignore-case")) {
            if(strcasecmp(SFD(nextin).mBuf, runinfo->mArgsRuntime[1]) <= 0) {
                runinfo->mRCode = 0;
            }
        }
        else {
            if(strcmp(SFD(nextin).mBuf, runinfo->mArgsRuntime[1]) <= 0) {
                runinfo->mRCode = 0;
            }
        }
    }

    return TRUE;
}

BOOL cmd_condition_sge(sObject* nextin, sObject* nextout, sRunInfo* runinfo)
{
    if(runinfo->mFilter && runinfo->mArgsNumRuntime == 2) {
        runinfo->mRCode = RCODE_NFUN_FALSE;
        if(sRunInfo_option(runinfo, "-ignore-case")) {
            if(strcasecmp(SFD(nextin).mBuf, runinfo->mArgsRuntime[1]) >= 0) {
                runinfo->mRCode = 0;
            }
        }
        else {
            if(strcmp(SFD(nextin).mBuf, runinfo->mArgsRuntime[1]) >= 0) {
                runinfo->mRCode = 0;
            }
        }
    }

    return TRUE;
}

BOOL cmd_condition_eq2(sObject* nextin, sObject* nextout, sRunInfo* runinfo)
{
    if(runinfo->mFilter && runinfo->mArgsNumRuntime == 2) {
        runinfo->mRCode = RCODE_NFUN_FALSE;
        if(atoi(SFD(nextin).mBuf) == atoi(runinfo->mArgsRuntime[1])) {
            runinfo->mRCode = 0;
        }
    }

    return TRUE;
}

BOOL cmd_condition_ne(sObject* nextin, sObject* nextout, sRunInfo* runinfo)
{
    if(runinfo->mFilter && runinfo->mArgsNumRuntime == 2) {
        runinfo->mRCode = RCODE_NFUN_FALSE;
        if(atoi(SFD(nextin).mBuf) != atoi(runinfo->mArgsRuntime[1])) {
            runinfo->mRCode = 0;
        }
    }

    return TRUE;
}

BOOL cmd_condition_lt(sObject* nextin, sObject* nextout, sRunInfo* runinfo)
{
    if(runinfo->mFilter && runinfo->mArgsNumRuntime == 2) {
        runinfo->mRCode = RCODE_NFUN_FALSE;
        if(atoi(SFD(nextin).mBuf) < atoi(runinfo->mArgsRuntime[1])) {
            runinfo->mRCode = 0;
        }
    }

    return TRUE;
}

BOOL cmd_condition_le(sObject* nextin, sObject* nextout, sRunInfo* runinfo)
{
    if(runinfo->mFilter && runinfo->mArgsNumRuntime == 2) {
        runinfo->mRCode = RCODE_NFUN_FALSE;
        if(atoi(SFD(nextin).mBuf) <= atoi(runinfo->mArgsRuntime[1])) {
            runinfo->mRCode = 0;
        }
    }

    return TRUE;
}

BOOL cmd_condition_gt(sObject* nextin, sObject* nextout, sRunInfo* runinfo)
{
    if(runinfo->mFilter && runinfo->mArgsNumRuntime == 2) {
        runinfo->mRCode = RCODE_NFUN_FALSE;
        if(atoi(SFD(nextin).mBuf) > atoi(runinfo->mArgsRuntime[1])) {
            runinfo->mRCode = 0;
        }
    }

    return TRUE;
}

BOOL cmd_condition_ge(sObject* nextin, sObject* nextout, sRunInfo* runinfo)
{
    if(runinfo->mFilter && runinfo->mArgsNumRuntime == 2) {
        runinfo->mRCode = RCODE_NFUN_FALSE;
        if(atoi(SFD(nextin).mBuf) >= atoi(runinfo->mArgsRuntime[1])) {
            runinfo->mRCode = 0;
        }
    }

    return TRUE;
}

BOOL cmd_condition_nt(sObject* nextin, sObject* nextout, sRunInfo* runinfo)
{
    if(runinfo->mFilter && runinfo->mArgsNumRuntime == 2) {
        runinfo->mRCode = RCODE_NFUN_FALSE;
        struct stat lstat_;
        struct stat rstat_;
        if(lstat(SFD(nextin).mBuf, &lstat_) == 0) {
            if(lstat(runinfo->mArgsRuntime[1], &rstat_) == 0) {
                if(lstat_.st_mtime > rstat_.st_mtime) 
                {
                    runinfo->mRCode = 0;
                }
            }
        }
    }

    return TRUE;
}

BOOL cmd_condition_ot(sObject* nextin, sObject* nextout, sRunInfo* runinfo)
{
    if(runinfo->mFilter && runinfo->mArgsNumRuntime == 2) {
        runinfo->mRCode = RCODE_NFUN_FALSE;
        struct stat lstat_;
        struct stat rstat_;
        if(lstat(SFD(nextin).mBuf, &lstat_) == 0) {
            if(lstat(runinfo->mArgsRuntime[1], &rstat_) == 0) {
                if(lstat_.st_mtime < rstat_.st_mtime) 
                {
                    runinfo->mRCode = 0;
                }
            }
        }
    }

    return TRUE;
}

BOOL cmd_condition_ef(sObject* nextin, sObject* nextout, sRunInfo* runinfo)
{
    runinfo->mRCode = RCODE_NFUN_FALSE;

    return TRUE;
}

void clear_matching_info_variable()
{
    char buf[128];
    snprintf(buf, 128, "");
    uobject_put(gRootObject, "MATCH", STRING_NEW_GC(buf, FALSE));

    snprintf(buf, 128, "");
    uobject_put(gRootObject, "PREMATCH", STRING_NEW_GC(buf, FALSE));

    snprintf(buf, 128, "");
    uobject_put(gRootObject, "POSTMATCH", STRING_NEW_GC(buf, FALSE));

    snprintf(buf, 128, "");
    uobject_put(gRootObject, "MATCH_NUMBER", STRING_NEW_GC(buf, FALSE));

    snprintf(buf, 128, "");
    uobject_put(gRootObject, "LAST_MATCH", STRING_NEW_GC(buf, FALSE));

    int i;
    for(i=0; i<10; i++) {
        char name[128];
        snprintf(name, 128, "%d", i);
        uobject_put(gRootObject, name, STRING_NEW_GC(buf, FALSE));
    }
}

BOOL cmd_condition_re(sObject* nextin, sObject* nextout, sRunInfo* runinfo)
{
    enum eKanjiCode code = gKanjiCode;
    if(sRunInfo_option(runinfo, "-byte")) {
        code = kByte;
    }
    else if(sRunInfo_option(runinfo, "-utf8")) {
        code = kUtf8;
    }
    else if(sRunInfo_option(runinfo, "-sjis")) {
        code = kSjis;
    }
    else if(sRunInfo_option(runinfo, "-eucjp")) {
        code = kEucjp;
    }

    BOOL verbose = sRunInfo_option(runinfo, "-verbose");
    BOOL offsets = sRunInfo_option(runinfo, "-offsets");

    if(runinfo->mFilter && runinfo->mArgsNumRuntime == 2) {
        clear_matching_info_variable();

        //BOOL preserve = sRunInfo_option(runinfo, "-preserve");

        runinfo->mRCode = RCODE_NFUN_FALSE;
        char* target = SFD(nextin).mBuf;
        char* regex = runinfo->mArgsRuntime[1];

        regex_t* reg;
        int r = get_onig_regex(&reg, runinfo, regex);

        if(r == ONIG_NORMAL) {
            //sObject* preserved_data = STRING_NEW_STACK();

            OnigRegion* region = onig_region_new();
            int r2 = onig_search(reg, target
               , target + strlen(target)
               , target, target + strlen(target)
               , region, ONIG_OPTION_NONE);

            if(r2 >= 0) {
                if(region->beg[0] > 0) {
                    uobject_put(gRootObject, "PREMATCH", STRING_NEW_GC3(target, region->beg[0], FALSE));
                }

                const int size = region->end[0] - region->beg[0];

                uobject_put(gRootObject, "MATCH", STRING_NEW_GC3(target + region->beg[0], size, FALSE));
                uobject_put(gRootObject, "0", STRING_NEW_GC3(target + region->beg[0], size, FALSE));

                const int n = strlen(target)-region->end[0];
                if(n > 0) {
                    uobject_put(gRootObject, "POSTMATCH", STRING_NEW_GC3(target + region->end[0], n, FALSE));
                }

                int i;
                for (i=1; i<region->num_regs; i++) {
                    const int size = region->end[i] - region->beg[i];

                    char name[16];
                    snprintf(name, 16, "%d", i);

                    uobject_put(gRootObject, name, STRING_NEW_GC3(target + region->beg[i], size, FALSE));
                }

                if(region->num_regs > 0) {
                    const int n = region->num_regs -1;

                    const int size = region->end[n] - region->beg[n];

                    uobject_put(gRootObject, "LAST_MATCH", STRING_NEW_GC3(target + region->beg[n], size, FALSE));
                }

                char buf[128];
                snprintf(buf, 128, "%d", region->num_regs);
                uobject_put(gRootObject, "MATCH_NUMBER", STRING_NEW_GC(buf, FALSE));

                if(verbose) {
                    int point = str_pointer2kanjipos(code, target, target + r2);

                    char buf[1024];
                    int size = snprintf(buf, 1024, "%d\n", point);
                    if(!fd_write(nextout, buf, size)) {
                        err_msg("signal interrupt", runinfo->mSName, runinfo->mSLine, runinfo->mArgs[0]);
                        runinfo->mRCode = RCODE_SIGNAL_INTERRUPT;
                        onig_region_free(region, 1);
                        onig_free(reg);
                        return FALSE;
                    }
                }

                if(offsets) {
                    int i;
                    for (i=0; i<region->num_regs; i++) {
                        int point = str_pointer2kanjipos(code, target, target + region->beg[i]);

                        char buf[1024];
                        int size = snprintf(buf, 1024, "%d\n", point);
                        if(!fd_write(nextout, buf, size)) {
                            err_msg("signal interrupt", runinfo->mSName, runinfo->mSLine, runinfo->mArgs[0]);
                            runinfo->mRCode = RCODE_SIGNAL_INTERRUPT;
                            onig_region_free(region, 1);
                            onig_free(reg);
                            return FALSE;
                        }

                        point = str_pointer2kanjipos(code, target, target + region->end[i]);

                        size = snprintf(buf, 1024, "%d\n", point);
                        if(!fd_write(nextout, buf, size)) {
                            err_msg("signal interrupt", runinfo->mSName, runinfo->mSLine, runinfo->mArgs[0]);
                            runinfo->mRCode = RCODE_SIGNAL_INTERRUPT;
                            onig_region_free(region, 1);
                            onig_free(reg);
                            return FALSE;
                        }
                    }
                }

                runinfo->mRCode = 0;
            }

            onig_region_free(region, 1);
            onig_free(reg);
        }
        else {
            onig_free(reg);
            err_msg("=~: invalid regex", runinfo->mSName, runinfo->mSLine, runinfo->mArgs[0]);
            return FALSE;
        }
    }

    return TRUE;
}

