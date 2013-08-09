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
#include <migemo.h>

#include "xyzsh.h"

static migemo* gMigemo;
static sObject* gMigemoCache;

static void migemo_init()
{
    char buf[PATH_MAX];
    char migemodir[PATH_MAX];
    gMigemo = migemo_open(NULL);

    snprintf(migemodir, PATH_MAX, "%s", SYSTEM_MIGEMODIR);

    snprintf(buf, PATH_MAX, "%s/utf-8/migemo-dict", migemodir);
    if(migemo_load(gMigemo, MIGEMO_DICTID_MIGEMO, buf) == MIGEMO_DICTID_INVALID) {
        fprintf(stderr, "%s is not found\n", buf);
        exit(1);
    }
    snprintf(buf, PATH_MAX, "%s/utf-8/roma2hira.dat", migemodir);
    if(migemo_load(gMigemo, MIGEMO_DICTID_ROMA2HIRA, buf) == MIGEMO_DICTID_INVALID) {
        fprintf(stderr, "%s is not found\n", buf);
        exit(1);
    }
    snprintf(buf, PATH_MAX, "%s/utf-8/hira2kata.dat", migemodir);
    if(migemo_load(gMigemo, MIGEMO_DICTID_HIRA2KATA, buf) == MIGEMO_DICTID_INVALID) {
        fprintf(stderr, "%s is not found\n", buf);
        exit(1);
    }
    snprintf(buf, PATH_MAX, "%s/utf-8/han2zen.dat", migemodir);
    if(migemo_load(gMigemo, MIGEMO_DICTID_HAN2ZEN, buf) == MIGEMO_DICTID_INVALID) {
        fprintf(stderr, "%s is not found\n", buf);
        exit(1);
    }
}

static void migemo_final()
{
    onig_end();
    migemo_close(gMigemo);

    hash_it* it = hash_loop_begin(gMigemoCache);
    while(it) {
        regex_t* reg = hash_loop_item(it);
        onig_free(reg);

        it = hash_loop_next(it);
    }
    hash_delete_on_malloc(gMigemoCache);
}

BOOL cmd_migemo_querry(sObject* nextin, sObject* nextout, sRunInfo* runinfo)
{
    if(runinfo->mArgsNumRuntime == 2) {
        char* regex = runinfo->mArgsRuntime[1];

        OnigUChar * p = migemo_query(gMigemo, regex);
        if(p == NULL) {
            err_msg("migemo query failed", runinfo->mSName, runinfo->mSLine);
            migemo_release(gMigemo, (unsigned char*) p);
            return FALSE;
        }

        if(!fd_write(nextout, p, strlen(p))) {
            err_msg("signal interrupt", runinfo->mSName, runinfo->mSLine);
            runinfo->mRCode = RCODE_SIGNAL_INTERRUPT;
            migemo_release(gMigemo, (unsigned char*) p);
            return FALSE;
        }

        if(!fd_write(nextout, "\n", 1)) {
            err_msg("signal interrupt", runinfo->mSName, runinfo->mSLine);
            runinfo->mRCode = RCODE_SIGNAL_INTERRUPT;
            migemo_release(gMigemo, (unsigned char*) p);
            return FALSE;
        }

        migemo_release(gMigemo, (unsigned char*) p);

        runinfo->mRCode = 0;
    }

    return TRUE;
}

BOOL cmd_migemo_match(sObject* nextin, sObject* nextout, sRunInfo* runinfo)
{
    BOOL quiet = sRunInfo_option(runinfo, "-quiet");

    if(runinfo->mFilter && runinfo->mArgsNumRuntime == 2) {
        runinfo->mRCode = RCODE_NFUN_FALSE;
        char* target = SFD(nextin).mBuf;
        char* regex = runinfo->mArgsRuntime[1];

        if(regex[0] == 0) {
            runinfo->mRCode = 0;
            if(!quiet) {
                char buf[1024];
                int n = snprintf(buf, 1024, "0\n%d\n", (int)strlen(target));
                if(!fd_write(nextout, buf, n)) {
                    err_msg("signal interrupt", runinfo->mSName, runinfo->mSLine);
                    runinfo->mRCode = RCODE_SIGNAL_INTERRUPT;
                    return FALSE;
                }
            }
        }
        else {
            regex_t* reg = hash_item(gMigemoCache, regex);

            if(reg == NULL) {
                OnigUChar * p = migemo_query(gMigemo, regex);
                if(p == NULL) {
                    err_msg("migemo query failed", runinfo->mSName, runinfo->mSLine);
                    migemo_release(gMigemo, (unsigned char*) p);
                    return FALSE;
                }

                /// modify query ///
                char* p2 = MALLOC(strlen(p)*2 + 1);

                char* _p = p;
                char* _p2 = p2;

                while(*_p) {
                    if(*_p == '+') {
                        *_p2++ = '\\';
                        *_p2++ = *_p++;
                    }
                    else {
                        *_p2++ = *_p++;
                    }
                }
                *_p2 = 0;

                /// make regex ///
                OnigErrorInfo err_info;

                int r = onig_new(&reg, p2, p2 + strlen(p2), ONIG_OPTION_DEFAULT, ONIG_ENCODING_UTF8, ONIG_SYNTAX_DEFAULT,  &err_info);

                if(r != ONIG_NORMAL && r != 0) {
                    err_msg("regex of migemo query failed", runinfo->mSName, runinfo->mSLine);
                    onig_free(reg);
                    FREE(p2);
                    migemo_release(gMigemo, (unsigned char*) p);
                    return FALSE;
                }

                FREE(p2);
                migemo_release(gMigemo, (unsigned char*) p);
            }

            OnigRegion* region = onig_region_new();
            int r2 = onig_search(reg, target, target + strlen(target), target, target + strlen(target), region, ONIG_OPTION_NONE);

            if(r2 >= 0) {
                runinfo->mRCode = 0;
                if(!quiet) {
                    char buf[1024];
                    int n = snprintf(buf, 1024, "%d\n%d\n", region->beg[0], region->end[0]);
                    if(!fd_write(nextout, buf, n)) {
                        err_msg("signal interrupt", runinfo->mSName, runinfo->mSLine);
                        runinfo->mRCode = RCODE_SIGNAL_INTERRUPT;
                        onig_region_free(region, 1);
                        onig_free(reg);
                        return FALSE;
                    }
                }
            }
            else {
                if(!quiet) {
                    char buf[1024];
                    int n = snprintf(buf, 1024, "-1\n-1\n");
                    if(!fd_write(nextout, buf, n)) {
                        err_msg("signal interrupt", runinfo->mSName, runinfo->mSLine);
                        runinfo->mRCode = RCODE_SIGNAL_INTERRUPT;
                        onig_region_free(region, 1);
                        onig_free(reg);
                        return FALSE;
                    }
                }
            }

            onig_region_free(region, 1);

            /// Migemo querry which is only one character is very heavy, so get cache.
            if(strlen(regex) <= 2) {
                hash_put(gMigemoCache, regex, reg);
            }
            else {
                onig_free(reg);
            }
    
        }
    }

    return TRUE;
}

#if defined(__CYGWIN__)
int migemo_dl_init()
#else
int dl_init()
#endif
{
    migemo_init();
    sObject* migemo_object = UOBJECT_NEW_GC(8, gRootObject, "migemo", TRUE);
    uobject_init(migemo_object);
    uobject_put(gRootObject, "migemo", migemo_object);
    uobject_put(migemo_object, "match", NFUN_NEW_GC(cmd_migemo_match, NULL, TRUE));
    uobject_put(migemo_object, "querry", NFUN_NEW_GC(cmd_migemo_querry, NULL, TRUE));
    gMigemoCache = HASH_NEW_MALLOC(100);

    return 0;
}

#if defined(__CYGWIN__)
int migemo_dl_final()
#else
int dl_final()
#endif
{
    migemo_final();

    return 0;
}

