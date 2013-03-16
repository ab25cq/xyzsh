#include "config.h"
#include "xyzsh/xyzsh.h"
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
#include <readline/readline.h>
#include <readline/history.h>

#if defined(HAVE_CURSES_H)
#include <curses.h>
#elif defined(HAVE_NCURSES_H)
#include <ncurses.h>
#elif defined(HAVE_NCURSES_NCURSES_H)
#include <ncurses/ncurses.h>
#endif

static void str_cut(enum eKanjiCode code, char* mbs, int termsize, char* dest_mbs, int dest_byte)
{
    if(code == kUtf8) {
        wchar_t* wcs;
        wchar_t* wcs_tmp;
        int i;

        wcs = (wchar_t*)MALLOC(sizeof(wchar_t)*(termsize+1)*MB_CUR_MAX);
        wcs_tmp = (wchar_t*)MALLOC(sizeof(wchar_t)*(termsize+1)*MB_CUR_MAX);
        if(mbstowcs(wcs, mbs, (termsize+1)*MB_CUR_MAX) == -1) {
            mbstowcs(wcs, "?????", (termsize+1)*MB_CUR_MAX);
        }

        for(i=0; i<wcslen(wcs); i++) {
            wcs_tmp[i] = wcs[i];
            wcs_tmp[i+1] = 0;

            if(wcswidth(wcs_tmp, wcslen(wcs_tmp)) > termsize) {
                wcs_tmp[i] = 0;
                break;
            }
        }

        wcstombs(dest_mbs, wcs_tmp, dest_byte);

        FREE(wcs);
        FREE(wcs_tmp);
    }
    else {
        int n;
        BOOL kanji = FALSE;
        for(n=0; n<termsize && n<dest_byte-1; n++) {
            if(!kanji && is_kanji(code, mbs[n])) {
                kanji = TRUE;
            }
            else {
                kanji = FALSE;
            }

            dest_mbs[n] = mbs[n];
        }
        
        if(kanji)
            dest_mbs[n-1] = 0;
        else
            dest_mbs[n] = 0;
    }
}

BOOL cmd_selector(sObject* nextin, sObject* nextout, sRunInfo* runinfo)
{
    if(isatty(0) == 0 || isatty(1) == 0) {
        err_msg("selector: stdin or stdout is not a tty", runinfo->mSName, runinfo->mSLine, runinfo->mArgs[0]);
        return FALSE;
    }
    if(tcgetpgrp(0) != getpgid(0)) {
        err_msg("selector: not forground process group", runinfo->mSName, runinfo->mSLine, runinfo->mArgs[0]);
        return FALSE;
    }
    char* term_env = getenv("TERM");
    if(term_env == NULL || strcmp(term_env, "") == 0) {
        err_msg("selector: not TERM environment variable setting", runinfo->mSName, runinfo->mSLine, runinfo->mArgs[0]);
        return FALSE;
    }

    eLineField lf = gLineField;
    if(sRunInfo_option(runinfo, "-Lw")) {
        lf = kCRLF;
    }
    else if(sRunInfo_option(runinfo, "-Lm")) {
        lf = kCR;
    }
    else if(sRunInfo_option(runinfo, "-Lu")) {
        lf = kLF;
    }
    else if(sRunInfo_option(runinfo, "-La")) {
        lf = kBel;
    }

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


    BOOL multiple = sRunInfo_option(runinfo, "-multiple");

    if(runinfo->mFilter) {
        fd_split(nextin, lf);

        if(vector_count(SFD(nextin).mLines) > 0) {
            msave_ttysettings();
            msave_screen();
            initscr();
            raw();
            keypad(stdscr, TRUE);

            const int maxx = mgetmaxx();
            const int maxy = mgetmaxy();

            int* markfiles = MALLOC(sizeof(int)*vector_count(SFD(nextin).mLines));
            memset(markfiles, 0, sizeof(int)*vector_count(SFD(nextin).mLines));

            static int scrolltop = 0;
            static int cursor = 0;

            if(!sRunInfo_option(runinfo, "-preserve-position")) {
                scrolltop = 0;
                cursor = 0;
            }

            if(cursor < 0) {
                cursor = 0;
            }
            if(cursor >= vector_count(SFD(nextin).mLines)) {
                cursor = vector_count(SFD(nextin).mLines)-1;
                if(cursor < 0) cursor = 0;
            }
            if(cursor < scrolltop) {
                int i = cursor;
                int width_sum = 0;
                while(width_sum < maxy) {
                    char* line = vector_item(SFD(nextin).mLines, i);
                    int width = str_termlen(code, line) / maxx + 1;
                    width_sum += width;
                    i--;
                    if(i < 0) {
                        i = -2;
                        break;
                    }
                }
                
                scrolltop = i +2;
            }

            while(1) {
                /// view ///
                clear();
                int n = scrolltop;
                int y = 0;
                while(y < maxy && n < vector_count(SFD(nextin).mLines)) {
                    int attrs = 0;
                    if(n == cursor) {
                        attrs = A_REVERSE;
                    }
                    else if(markfiles[n]) {
                        attrs = A_BOLD;
                    }
                    if(attrs) attron(attrs);

                    char* line = vector_item(SFD(nextin).mLines, n);

                    if(str_termlen(code, line) <= maxx) {
                        mvprintw(y, 0, "%s", line);
                        y++;
                    }
                    else {
                        char* p = line;
                        while(p < line + strlen(line)) {
                            char one_line[BUFSIZ];
                            str_cut(code, p, maxx, one_line, BUFSIZ);
                            mvprintw(y, 0, "%s", one_line);
                            y++;

                            p+=strlen(one_line);
                        }
                    }

                    if(attrs) attroff(attrs);

                    n++;
                }
                refresh();

                /// input ///
                int key = getch();

                if(key == 14 || key == KEY_DOWN) {
                    cursor++;
                }
                else if(key == 16 || key == KEY_UP) {
                    cursor--;
                }
                else if(key == 4 || key == KEY_NPAGE) {
                    cursor += (maxy / 2);
                }
                else if(key == 21 || key == KEY_PPAGE) {
                    cursor -= (maxy /2);
                }
                else if(key == 12) {
                    clear();
                    refresh();
                }
                else if(key == 'q' || key == 3 || key == 7) {
                    err_msg("canceled", runinfo->mSName, runinfo->mSLine, runinfo->mArgs[0]);
                    endwin();
                    mrestore_screen();
                    mrestore_ttysettings();
                    FREE(markfiles);
                    return FALSE;
                }
                else if(key == 'a' && multiple) {
                    int i;
                    for(i=0; i<vector_count(SFD(nextin).mLines); i++) {
                        markfiles[i] = !markfiles[i];
                    }
                }
                else if(key == ' ' && multiple) {
                    markfiles[cursor] = !markfiles[cursor];
                    cursor++;
                }
                else if(key == 10 || key == 13) {
                    if(multiple) {
                        BOOL flg_mark = FALSE;
                        int k;
                        for(k=0; k<vector_count(SFD(nextin).mLines); k++) {
                            if(markfiles[k]) {
                                char* str = vector_item(SFD(nextin).mLines, k);
                                if(!fd_write(nextout,str, strlen(str))) {
                                    err_msg("interrupt", runinfo->mSName, runinfo->mSLine, runinfo->mArgs[0]);
                                    runinfo->mRCode = RCODE_SIGNAL_INTERRUPT;
                                    FREE(markfiles);
                                    return FALSE;
                                }

                                flg_mark = TRUE;
                            }
                        }

                        if(!flg_mark) {
                            char* str = vector_item(SFD(nextin).mLines, cursor);
                            if(!fd_write(nextout, str, strlen(str))) {
                                err_msg("interrupt", runinfo->mSName, runinfo->mSLine, runinfo->mArgs[0]);
                                runinfo->mRCode = RCODE_SIGNAL_INTERRUPT;
                                FREE(markfiles);
                                return FALSE;
                            }
                        }
                    }
                    else {
                        char* str = vector_item(SFD(nextin).mLines, cursor);
                        if(!fd_write(nextout, str, strlen(str))) {
                            err_msg("interrupt", runinfo->mSName, runinfo->mSLine, runinfo->mArgs[0]);
                            runinfo->mRCode = RCODE_SIGNAL_INTERRUPT;
                            FREE(markfiles);
                            return FALSE;
                        }
                    }
                    if(SFD(nextin).mBufLen == 0) {
                        runinfo->mRCode = RCODE_NFUN_NULL_INPUT;
                    }
                    else {
                        runinfo->mRCode = 0;
                    }
                    break;
                }

                if(cursor < 0) {
                    cursor = 0;
                }
                if(cursor >= vector_count(SFD(nextin).mLines)) {
                    cursor = vector_count(SFD(nextin).mLines)-1;
                    if(cursor < 0) cursor = 0;
                }
                if(cursor >= n) {
                    scrolltop = n;
                }
                if(cursor < scrolltop) {
                    int i = cursor;
                    int width_sum = 0;
                    while(width_sum < maxy) {
                        char* line = vector_item(SFD(nextin).mLines, i);
                        int width = str_termlen(code, line) / maxx + 1;
                        width_sum += width;
                        i--;
                        if(i < 0) {
                            i = -2;
                            break;
                        }
                    }
                    
                    scrolltop = i +2;
                }
            }

            FREE(markfiles);

            endwin();
            mrestore_screen();
            mrestore_ttysettings();
        }
        else {
            runinfo->mRCode = RCODE_NFUN_NULL_INPUT;
        }
    }

    return TRUE;
}

BOOL cmd_p(sObject* nextin, sObject* nextout, sRunInfo* runinfo)
{
    if(isatty(0) == 0 || isatty(1) == 0) {
        err_msg("p: stdin or stdout is not a tty", runinfo->mSName, runinfo->mSLine, runinfo->mArgs[0]);
        return FALSE;
    }
    if(tcgetpgrp(0) != getpgid(0)) {
        err_msg("p: not forground process group", runinfo->mSName, runinfo->mSLine, runinfo->mArgs[0]);
        return FALSE;
    }
    char* term_env = getenv("TERM");
    if(term_env == NULL || strcmp(term_env, "") == 0) {
        err_msg("p: not TERM environment variable setting", runinfo->mSName, runinfo->mSLine, runinfo->mArgs[0]);
        return FALSE;
    }

    eLineField lf = kLF;

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

    if(runinfo->mFilter) {
        sObject* nextin2 = FD_NEW_STACK();

        /// make control characters visible ///
        char* p = SFD(nextin).mBuf;
        while(p - SFD(nextin).mBuf < SFD(nextin).mBufLen) {
            if(*p == '\n') {
                if(!fd_writec(nextin2, *p)) {
                    err_msg("interrupt", runinfo->mSName, runinfo->mSLine, runinfo->mArgs[0]);
                    runinfo->mRCode = RCODE_SIGNAL_INTERRUPT;
                    return FALSE;
                }
                p++;
            }
            else if(*p >= 0 && *p <= 31) {
                if(!fd_writec(nextin2, '^')) {
                    err_msg("interrupt", runinfo->mSName, runinfo->mSLine, runinfo->mArgs[0]);
                    runinfo->mRCode = RCODE_SIGNAL_INTERRUPT;
                    return FALSE;
                }
                if(!fd_writec(nextin2, *p+'@')) {
                    err_msg("interrupt", runinfo->mSName, runinfo->mSLine, runinfo->mArgs[0]);
                    runinfo->mRCode = RCODE_SIGNAL_INTERRUPT;
                    return FALSE;
                }
                p++;
            }
            else {
                if(!fd_writec(nextin2, *p)) {
                    err_msg("interrupt", runinfo->mSName, runinfo->mSLine, runinfo->mArgs[0]);
                    runinfo->mRCode = RCODE_SIGNAL_INTERRUPT;
                    return FALSE;
                }
                p++;
            }
        }

        /// go ///
        fd_split(nextin2, lf);

        msave_ttysettings();
        msave_screen();
        initscr();
        raw();
        keypad(stdscr, TRUE);

        const int maxx = mgetmaxx();
        const int maxy = mgetmaxy();

        int* markfiles = MALLOC(sizeof(int)*vector_count(SFD(nextin2).mLines));
        memset(markfiles, 0, sizeof(int)*vector_count(SFD(nextin2).mLines));

        static int scrolltop = 0;
        static int cursor = 0;

        if(!sRunInfo_option(runinfo, "-prereserve-position")) {
            scrolltop = 0;
            cursor = 0;
        }

        if(cursor < 0) {
            cursor = 0;
        }
        if(cursor >= vector_count(SFD(nextin2).mLines)) {
            cursor = vector_count(SFD(nextin2).mLines)-1;
            if(cursor < 0) cursor = 0;
        }
        if(cursor < scrolltop) {
            int i = cursor;
            int width_sum = 0;
            while(width_sum < maxy) {
                char* line = vector_item(SFD(nextin2).mLines, i);
                int width = str_termlen(code, line) / maxx + 1;
                width_sum += width;
                i--;
                if(i < 0) {
                    i = -2;
                    break;
                }
            }
            
            scrolltop = i +2;
        }

        while(1) {
            /// view ///
            clear();
            int n = scrolltop;
            int y = 0;
            while(y < maxy && n < vector_count(SFD(nextin2).mLines)) {
                int attrs = 0;
                if(n == cursor) {
                    attrs = A_REVERSE;
                }
                else if(markfiles[n]) {
                    attrs = A_BOLD;
                }
                if(attrs) attron(attrs);

                char* line = vector_item(SFD(nextin2).mLines, n);

                if(str_termlen(code, line) <= maxx) {
                    mvprintw(y, 0, "%s", line);
                    y++;
                }
                else {
                    char* p = line;
                    while(p < line + strlen(line)) {
                        char one_line[BUFSIZ];
                        str_cut(code, p, maxx, one_line, BUFSIZ);
                        mvprintw(y, 0, "%s", one_line);
                        y++;

                        p+=strlen(one_line);
                    }
                }

                if(attrs) attroff(attrs);

                n++;
            }
            refresh();

            /// input ///
            int key = getch();

            if(key == 14 || key == KEY_DOWN) {
                cursor++;
            }
            else if(key == 16 || key == KEY_UP) {
                cursor--;
            }
            else if(key == 4 || key == KEY_NPAGE) {
                cursor += (maxy / 2);
            }
            else if(key == 21 || key == KEY_PPAGE) {
                cursor -= (maxy /2);
            }
            else if(key == 12) {
                clear();
                refresh();
            }
            else if(key == 'q' || key == 3 || key == 7) {
                err_msg("p: canceled", runinfo->mSName, runinfo->mSLine, runinfo->mArgs[0]);
                endwin();
                mrestore_screen();
                mrestore_ttysettings();
                FREE(markfiles);
                return FALSE;
            }
            else if(key == 10 || key == 13) {
                if(!fd_write(nextout, SFD(nextin).mBuf, SFD(nextin).mBufLen)) {
                    err_msg("interrupt", runinfo->mSName, runinfo->mSLine, runinfo->mArgs[0]);
                    runinfo->mRCode = RCODE_SIGNAL_INTERRUPT;
                    FREE(markfiles);
                    return FALSE;
                }
                if(SFD(nextin).mBufLen == 0) {
                    runinfo->mRCode = RCODE_NFUN_NULL_INPUT;
                }
                else {
                    runinfo->mRCode = 0;
                }
                break;
            }

            if(cursor < 0) {
                cursor = 0;
            }
            if(cursor >= vector_count(SFD(nextin2).mLines)) {
                cursor = vector_count(SFD(nextin2).mLines)-1;
                if(cursor < 0) cursor = 0;
            }
            if(cursor >= n) {
                scrolltop = n;
            }
            if(cursor < scrolltop) {
                int i = cursor;
                int width_sum = 0;
                while(width_sum < maxy) {
                    char* line = vector_item(SFD(nextin2).mLines, i);
                    int width = str_termlen(code, line) / maxx + 1;
                    width_sum += width;
                    i--;
                    if(i < 0) {
                        i = -2;
                        break;
                    }
                }
                
                scrolltop = i +2;
            }
        }

        FREE(markfiles);

        endwin();
        mrestore_screen();
        mrestore_ttysettings();
    }

    return TRUE;
}

BOOL cmd_readline(sObject* nextin, sObject* nextout, sRunInfo* runinfo)
{
    if(isatty(0) == 0 || isatty(1) == 0) {
        err_msg("readline: stdin or stdout is not a tty", runinfo->mSName, runinfo->mSLine, runinfo->mArgs[0]);
        return FALSE;
    }
    if(tcgetpgrp(0) != getpgid(0)) {
        err_msg("readline: not forground process group", runinfo->mSName, runinfo->mSLine, runinfo->mArgs[0]);
        return FALSE;
    }
    char* term_env = getenv("TERM");
    if(term_env == NULL || strcmp(term_env, "") == 0) {
        err_msg("readline: not TERM environment variable setting", runinfo->mSName, runinfo->mSLine, runinfo->mArgs[0]);
        return FALSE;
    }

    char* prompt;
    if(runinfo->mArgsNumRuntime >= 2) {
        prompt = runinfo->mArgsRuntime[1];
    }
    else {
        prompt = "> ";
    }

    if(runinfo->mBlocksNum == 1) {
        readline_completion_user_castamized(runinfo->mBlocks[0]);

        char* buf = readline(prompt);

        if(buf) {
            if(!fd_write(nextout, buf, strlen(buf))) {
                err_msg("interrupt", runinfo->mSName, runinfo->mSLine, runinfo->mArgs[0]);
                runinfo->mRCode = RCODE_SIGNAL_INTERRUPT;
                readline_completion();
                return FALSE;
            }
        }

        readline_completion();
    }
    else {
        if(sRunInfo_option(runinfo, "-no-completion")) {
            readline_no_completion();
        }

        char* buf = readline(prompt);

        if(buf) {
            if(!fd_write(nextout, buf, strlen(buf))) {
                err_msg("interrupt", runinfo->mSName, runinfo->mSLine, runinfo->mArgs[0]);
                runinfo->mRCode = RCODE_SIGNAL_INTERRUPT;
                readline_completion();
                return FALSE;
            }
        }
        
        readline_completion();
    }

    runinfo->mRCode = 0;

    return TRUE;
}

typedef struct {
    sObject* mName;

    struct stat mStat;
} sFile;

static sFile* sFile_new(char* name, struct stat* stat_)
{
    sFile* self = MALLOC(sizeof(sFile));

    self->mName = STRING_NEW_STACK(name);
    self->mStat = *stat_;

    return self;
}

static void sFile_delete(sFile* self)
{
    FREE(self);
}

static BOOL sort_name(void* left, void* right)
{
    sFile* l = (sFile*) left;
    sFile* r = (sFile*) right;

    char* lfname = string_c_str(l->mName);
    char* rfname = string_c_str(r->mName);

    if(strcmp(lfname, ".") == 0) return TRUE;
    if(strcmp(lfname, "..") == 0) {
        if(strcmp(rfname, ".") == 0) return FALSE;

        return TRUE;          
    }
    if(strcmp(rfname, ".") == 0) return FALSE;
    if(strcmp(rfname, "..") == 0) return FALSE;

    if(!S_ISDIR(l->mStat.st_mode) && !S_ISDIR(r->mStat.st_mode)
        || S_ISDIR(l->mStat.st_mode) && S_ISDIR(r->mStat.st_mode) )
    {
        return strcasecmp(lfname, rfname) < 0;
    }

    if(S_ISDIR(l->mStat.st_mode))
        return TRUE;
    else
        return FALSE;
}


// add / to tail of path
static BOOL select_file_read(sObject* v, char* path)
{
    DIR* dir = opendir(path);
    if(dir == NULL) {
        return FALSE;
    }
    
    struct dirent* entry;
    while(entry = readdir(dir)) {
        char fpath[PATH_MAX];
        xstrncpy(fpath, path, PATH_MAX);
        xstrncat(fpath, entry->d_name, PATH_MAX);

        struct stat stat_;
        memset(&stat_, 0, sizeof(struct stat));
        if(stat(fpath, &stat_) < 0) {
            continue;
        }

        sFile* file = sFile_new(entry->d_name, &stat_);

        vector_add(v, file);
    }
    
    closedir(dir);

    vector_sort(v, sort_name);

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

static void str_cut2(enum eKanjiCode code, char* mbs, int termsize, char* dest_mbs, int dest_byte)
{
    if(code == kUtf8) {
        int i;
        int n;

        wchar_t* wcs 
            = (wchar_t*)MALLOC(sizeof(wchar_t)*(termsize+1)*MB_CUR_MAX);
        wchar_t* tmp 
            = (wchar_t*)MALLOC(sizeof(wchar_t)*(termsize+1)*MB_CUR_MAX);

        if(mbstowcs(wcs, mbs, (termsize+1)*MB_CUR_MAX) == -1) {
            mbstowcs(wcs, "?????", (termsize+1)*MB_CUR_MAX);
        }

        i = 0;
        while(1) {
            if(i < wcslen(wcs)) {
                tmp[i] = wcs[i];
                tmp[i+1] = 0;
            }
            else {
                tmp[i] = ' ';
                tmp[i+1] = 0;
            }

            n = wcswidth(tmp, wcslen(tmp));
            if(n < 0) {
                xstrncpy(dest_mbs, "?????", dest_byte);

                FREE(wcs);
                FREE(tmp);

                return;
            }
            else {
                if(n > termsize) {
                    tmp[i] = 0;

                    if(wcswidth(tmp, wcslen(tmp)) != termsize) {
                        tmp[i] = ' ';
                        tmp[i+1] = 0;
                    }
                    break;
                }
            }

            i++;
        }

        wcstombs(dest_mbs, tmp, dest_byte);

        FREE(wcs);
        FREE(tmp);
    }
    else {
        int n;
        BOOL tmpend = FALSE;
        BOOL kanji = FALSE;
        for(n=0; n<termsize && n<dest_byte-1; n++) {
            if(kanji)
                kanji = FALSE;
            else if(!tmpend && is_kanji(code, mbs[n])) {
                kanji = TRUE;
            }

            if(!tmpend && mbs[n] == 0) tmpend = TRUE;
                        
            if(tmpend)
                dest_mbs[n] = ' ';
            else
                dest_mbs[n] = mbs[n];
        }
        
        if(kanji) dest_mbs[n-1] = ' ';
        dest_mbs[n] = 0;
    }
}

static int correct_path(char* current_path, char* path, char* path2, int path2_size)
{
    char tmp[PATH_MAX];
    char tmp2[PATH_MAX];
    char tmp3[PATH_MAX];
    char tmp4[PATH_MAX];

    /// 先頭にカレントパスを加える ///
    {
        if(path[0] == '/') {      /// 絶対パスなら必要ない
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

    /// .を削除する ///
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

    /// ..を削除する ///
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

static void parentname(char* result, int result_size, char* path)
{
    char* p;

    if(*path == 0) {
        xstrncpy(result, "", result_size);
        return;
    }
    
    if(strcmp(path, "/") == 0) {
        xstrncpy(result, "/", result_size);
        return;
    }

    for(p=path + strlen(path)-2; p != path-1; p--) {
        if(*p == '/' && (p-path + 2) < result_size) {
            memcpy(result, path, p-path);
            result[p-path] = '/';
            result[p-path+1] = 0;
            
            return;
        }
    }

    xstrncpy(result, "", result_size);
}


BOOL cmd_fselector(sObject* nextin, sObject* nextout, sRunInfo* runinfo)
{
    BOOL multiple = sRunInfo_option(runinfo, "-multiple");

    if(isatty(0) == 0 || isatty(1) == 0) {
        err_msg("fselector: stdin or stdout is not a tty", runinfo->mSName, runinfo->mSLine, runinfo->mArgs[0]);
        return FALSE;
    }
    if(tcgetpgrp(0) != getpgid(0)) {
        err_msg("fselector: not forground process group", runinfo->mSName, runinfo->mSLine, runinfo->mArgs[0]);
        return FALSE;
    }
    char* term_env = getenv("TERM");
    if(term_env == NULL || strcmp(term_env, "") == 0) {
        err_msg("fselector: not TERM environment variable setting", runinfo->mSName, runinfo->mSLine, runinfo->mArgs[0]);
        return FALSE;
    }

    sObject* v = VECTOR_NEW_STACK(10);
    char cwd_path[PATH_MAX];
    mygetcwd(cwd_path, PATH_MAX);
    xstrncat(cwd_path, "/", PATH_MAX);

    char current_dir[PATH_MAX];
    xstrncpy(current_dir, cwd_path, PATH_MAX);
    select_file_read(v, current_dir);

    sObject* mark_files = VECTOR_NEW_STACK(10);

    if(vector_count(v) > 0) {
        msave_ttysettings();       // 端末の設定の保存
        msave_screen();
        initscr();
        raw();
        keypad(stdscr, TRUE);

        const int maxx = mgetmaxx();
        const int maxy = mgetmaxy();

        int cursor = 0;
        while(1) {
            /// 描写 ///
            clear();
            mvprintw(0,0,"%s", current_dir);

            int page = cursor/((maxy-1)*4);
            int n = page*4*(maxy-1);
            while(n < page*4*(maxy-1)+(maxy-1)*4 && n < vector_count(v)) {
                sFile* file = vector_item(v, n);

                int attrs = 0;
                if(n == cursor) {
                    attrs |= A_REVERSE;
                }
                if(S_ISDIR(file->mStat.st_mode)) {
                    attrs |= A_BOLD;
                }
                if(attrs) attron(attrs);

                char file2[PATH_MAX];
                str_cut2(kUtf8, string_c_str(file->mName), maxx/4-2, file2, PATH_MAX);

                int y = (n-page*4*(maxy-1)) / 4 + 1;
                int x = ((n-page*4*(maxy-1)) % 4)*maxx/4;

                char full_path[PATH_MAX];
                xstrncpy(full_path, current_dir, PATH_MAX);
                xstrncat(full_path, string_c_str(file->mName), PATH_MAX);

                BOOL marked = FALSE;
                int i;
                for(i=0; i<vector_count(mark_files); i++) {
                    sObject* mark_file = vector_item(mark_files, i);

                    if(strcmp(string_c_str(mark_file), full_path) == 0) {
                        marked = TRUE;
                        break;
                    }
                }

                if(marked) 
                    mvprintw(y, x, "*%s", file2);
                else
                    mvprintw(y, x, " %s", file2);

                if(attrs) attroff(attrs);

                n++;
            }
            refresh();

            /// インプット ///
            int key = getch();

            if(key == 14 || key == KEY_DOWN) {
                cursor+=4;
            }
            else if(key == 16 || key == KEY_UP) {
                cursor-=4;
            }
            else if(key == 6 || key == KEY_RIGHT) {
                cursor++;
            }
            else if(key == 2 || key == KEY_LEFT) {
                cursor--;
            }
            else if(key == 4 || key == KEY_NPAGE) {
                cursor += (maxy-1)*4;
            }
            else if(key == 21 || key == KEY_PPAGE) {
                cursor -= (maxy-1)*4;
            }
            else if(key == 12) {
                clear();
                refresh();
            }
            else if(key == ' ' && multiple) {
                sFile* cursor_file = vector_item(v, cursor);

                sObject* cursor_mfile = NULL;

                char cursor_path[PATH_MAX];
                xstrncpy(cursor_path, current_dir, PATH_MAX);
                xstrncat(cursor_path, string_c_str(cursor_file->mName), PATH_MAX);

                int i;
                for(i=0; i<vector_count(mark_files); i++) {
                    sObject* mfile = vector_item(mark_files, i);

                    if(strcmp(string_c_str(mfile), cursor_path) == 0) {
                        cursor_mfile = mfile;
                        break;
                    }
                }

                if(!cursor_mfile) {
                    vector_add(mark_files, STRING_NEW_STACK(cursor_path));
                }
                else {
                    vector_erase(mark_files
                        , vector_index(mark_files, cursor_mfile));
                }

                cursor++;
            }
            else if(key == '\\') {
                xstrncpy(current_dir, "/", PATH_MAX);

                int i;
                for(i=0; i<vector_count(v); i++) {
                    sFile_delete(vector_item(v, i));
                }
                vector_clear(v);

                select_file_read(v, current_dir);
                cursor = 0;
            }
            else if(key == '\t' || key == 'w') {
                break;
            }
            else if(key == 'q' || key == 3 || key == 7) {
                err_msg("canceled", runinfo->mSName, runinfo->mSLine, runinfo->mArgs[0]);
                endwin();
                mrestore_screen();
                mrestore_ttysettings();    // 端末の設定の復帰
                int i;
                for(i=0; i<vector_count(v); i++) {
                    sFile_delete(vector_item(v, i));
                }
                return FALSE;
            }
            else if(key == 10 || key == 13) {
                sFile* cursor_file = vector_item(v, cursor);

                if(strcmp(string_c_str(cursor_file->mName), ".") != 0
                    && S_ISDIR(cursor_file->mStat.st_mode)) 
                {
                    char path2[PATH_MAX];
                    if(correct_path(current_dir
                            , string_c_str(cursor_file->mName), path2, PATH_MAX)) 
                    {
                        xstrncpy(current_dir, path2, PATH_MAX);
                        if(current_dir[strlen(current_dir)-1] != '/') {
                            xstrncat(current_dir, "/", PATH_MAX);
                        }

                        int i;
                        for(i=0; i<vector_count(v); i++) {
                            sFile_delete(vector_item(v, i));
                        }
                        vector_clear(v);

                        select_file_read(v, current_dir);
                        cursor = 0;
                    }
                }
            }
            else if(key == 8 || key == KEY_BACKSPACE) {    // CTRL-H
                char parent_path[PATH_MAX];
                parentname(parent_path, PATH_MAX, current_dir);
                xstrncpy(current_dir, parent_path, PATH_MAX);

                int i;
                for(i=0; i<vector_count(v); i++) {
                    sFile_delete(vector_item(v, i));
                }
                vector_clear(v);

                select_file_read(v, current_dir);
                cursor = 0;
            }

            /// 修正 ///
            if(cursor < 0) {
                cursor = 0;
            }
            if(cursor >= vector_count(v)) {
                cursor = vector_count(v)-1;
            }
        }

//clear();
//refresh();
        endwin();
        mrestore_screen();
        mrestore_ttysettings();    // 端末の設定の復帰

        if(vector_count(mark_files) == 0) {
            sFile* cursor_file = vector_item(v, cursor);

            /// 現在のカレントディレクトリのパスなら
            /// ディレクトリ名を取る
            char cursor_path[PATH_MAX+1];
            xstrncpy(cursor_path, current_dir, PATH_MAX);
            xstrncat(cursor_path, string_c_str(cursor_file->mName), PATH_MAX);

            /// カレントのファイル、子ディレクトリのファイルなら
            /// 相対ディレクトリ
            if(memcmp(cursor_path
                    , cwd_path, strlen(cwd_path)) == 0 )
            {
                char cursor_path2[PATH_MAX+1];
                memcpy(cursor_path2
                    , cursor_path+strlen(cwd_path)
                    , strlen(cursor_path) - strlen(cwd_path)+1);
                xstrncat(cursor_path2, "\n", PATH_MAX+1);

                if(!fd_write(nextout, cursor_path2, strlen(cursor_path2)))
                {
                    err_msg("signal interrupt", runinfo->mSName, runinfo->mSLine, runinfo->mArgs[0]);
                    int i;
                    for(i=0; i<vector_count(v); i++) {
                        sFile_delete(vector_item(v, i));
                    }
                    return FALSE;
                }
            }
            else {
                xstrncat(cursor_path, "\n", PATH_MAX+1);
                if(!fd_write(nextout, cursor_path, strlen(cursor_path)))
                {
                    err_msg("signal interrupt", runinfo->mSName, runinfo->mSLine, runinfo->mArgs[0]);
                    int i;
                    for(i=0; i<vector_count(v); i++) {
                        sFile_delete(vector_item(v, i));
                    }
                    return FALSE;
                }
            }
        }
        else {
            int j;
            for(j=0; j<vector_count(mark_files); j++) {
                char full_path[PATH_MAX+1];
                xstrncpy(full_path
                    , string_c_str((sObject*)vector_item(mark_files, j))
                    , PATH_MAX);

                if(strstr(full_path, cwd_path) == full_path) {
                    char full_path2[PATH_MAX+PATH_MAX];
                    memcpy(full_path2
                        , full_path + strlen(cwd_path) 
                        , strlen(full_path) - strlen(cwd_path));
                    full_path2[strlen(full_path) - strlen(cwd_path)] = '\n';
                    full_path2[strlen(full_path) - strlen(cwd_path) + 1] = 0;

                    if(!fd_write(nextout, full_path2, strlen(full_path2)))
                    {
                        err_msg("signal interrupt", runinfo->mSName, runinfo->mSLine, runinfo->mArgs[0]);
                        int i;
                        for(i=0; i<vector_count(v); i++) {
                            sFile_delete(vector_item(v, i));
                        }
                        return FALSE;
                    }
                }
                else {
                    xstrncat(full_path, "\n", PATH_MAX+1);
                    if(!fd_write(nextout, full_path, strlen(full_path)))
                    {
                        err_msg("signal interrupt", runinfo->mSName, runinfo->mSLine, runinfo->mArgs[0]);
                        int i;
                        for(i=0; i<vector_count(v); i++) {
                            sFile_delete(vector_item(v, i));
                        }
                        return FALSE;
                    }
                }
            }
        }
    }

    int i;
    for(i=0; i<vector_count(v); i++) {
        sFile_delete(vector_item(v, i));
    }

    runinfo->mRCode = 0;

    return TRUE;
}

BOOL cmd_curses_initscr(sObject* nextin, sObject* nextout, sRunInfo* runinfo)
{
    initscr();
    raw();
    keypad(stdscr, TRUE);
    noecho();
    runinfo->mRCode = 0;

    return TRUE;
}

BOOL cmd_curses_endwin(sObject* nextin, sObject* nextout, sRunInfo* runinfo)
{
    if(mis_raw_mode()) {
        endwin();
        runinfo->mRCode = 0;
    }

    return TRUE;
}

BOOL cmd_curses_getch(sObject* nextin, sObject* nextout, sRunInfo* runinfo)
{
    if(mis_raw_mode()) {
        int key = getch();
        char buf[BUFSIZ];
        int n = snprintf(buf, BUFSIZ, "%d\n", key);
        if(!fd_write(nextout, buf, n)) {
            err_msg("interrupt", runinfo->mSName, runinfo->mSLine, runinfo->mArgs[0]);
            runinfo->mRCode = RCODE_SIGNAL_INTERRUPT;
            return FALSE;
        }
        runinfo->mRCode = 0;
    }

    return TRUE;
}

BOOL cmd_curses_move(sObject* nextin, sObject* nextout, sRunInfo* runinfo)
{
    if(mis_raw_mode()) {
        if(runinfo->mArgsNumRuntime == 3) {
            int y = atoi(runinfo->mArgsRuntime[1]);
            int x = atoi(runinfo->mArgsRuntime[2]);
            move(y, x);
            runinfo->mRCode = 0;
        }
    }

    return TRUE;
}

BOOL cmd_curses_refresh(sObject* nextin, sObject* nextout, sRunInfo* runinfo)
{
    if(mis_raw_mode()) {
        refresh();
        runinfo->mRCode = 0;
    }

    return TRUE;
}

BOOL cmd_curses_clear(sObject* nextin, sObject* nextout, sRunInfo* runinfo)
{
    if(mis_raw_mode()) {
        clear();
        runinfo->mRCode = 0;
    }

    return TRUE;
}

BOOL cmd_curses_printw(sObject* nextin, sObject* nextout, sRunInfo* runinfo)
{
    eLineField lf = gLineField;
    if(sRunInfo_option(runinfo, "-Lw")) {
        lf = kCRLF;
    }
    else if(sRunInfo_option(runinfo, "-Lm")) {
        lf = kCR;
    }
    else if(sRunInfo_option(runinfo, "-Lu")) {
        lf = kLF;
    }
    else if(sRunInfo_option(runinfo, "-La")) {
        lf = kBel;
    }

    if(mis_raw_mode() && runinfo->mArgsNumRuntime == 2) {
        if(runinfo->mFilter) {
            fd_split(nextin, lf);
        }

        /// go ///
        char* p = runinfo->mArgsRuntime[1];

        int strings_num = 0;
        while(*p) {
            if(*p == '%') {
                p++;

                if(*p == '%') {
                    p++;
                    printw("%%");
                }
                else {
                    if(*p == 'd' || *p == 'i' || *p == 'o' || *p == 'u' || *p == 'x' || *p == 'X' || *p == 'c') 
                    {
                        char aformat[16];
                        snprintf(aformat, 16, "%%%c", *p);
                        p++;

                        char* arg;
                        if(runinfo->mFilter && strings_num < vector_count(SFD(nextin).mLines)) {
                            arg = vector_item(SFD(nextin).mLines, strings_num);
                        }
                        else {
                            arg = "0";
                        }
                        strings_num++;

                        char* buf;
                        int size = asprintf(&buf, aformat, atoi(arg));

                        printw("%s", buf);
                        free(buf);
                    }
                    else if(*p == 'e' || *p == 'E' || *p == 'f' || *p == 'F' || *p == 'g' || *p == 'G' || *p == 'a' || *p == 'A')
                    {
                        char aformat[16];
                        snprintf(aformat, 16, "%%%c", *p);
                        p++;

                        char* arg;
                        if(runinfo->mFilter && strings_num < vector_count(SFD(nextin).mLines)) {
                            arg = vector_item(SFD(nextin).mLines, strings_num);
                        }
                        else {
                            arg = "0";
                        }
                        strings_num++;

                        char* buf;
                        int size = asprintf(&buf, aformat, atof(arg));

                        printw("%s", buf);
                        free(buf);
                    }
                    else if(*p == 's') {
                        char aformat[16];
                        snprintf(aformat, 16, "%%%c", *p);
                        p++;

                        sObject* arg;
                        if(runinfo->mFilter && strings_num < vector_count(SFD(nextin).mLines)) {
                            arg = STRING_NEW_STACK(vector_item(SFD(nextin).mLines, strings_num));
                        }
                        else {
                            arg = STRING_NEW_STACK("");
                        }
                        strings_num++;

                        string_chomp(arg);

                        char* buf;
                        int size = asprintf(&buf, aformat, string_c_str(arg));

                        printw("%s", buf);
                        free(buf);
                    }
                    else {
                        err_msg("invalid format at printf", runinfo->mSName, runinfo->mSLine, runinfo->mArgs[0]);
                        return FALSE;
                    }
                }
            }
            else {
                printw("%c", *p++);
            }
        }

        if(runinfo->mFilter) {
            if(strings_num > vector_count(SFD(nextin).mLines)) {
                if(SFD(nextin).mBufLen == 0) {
                    runinfo->mRCode = RCODE_NFUN_NULL_INPUT;
                }
                else {
                    runinfo->mRCode = 1;
                }
            }
            else {
                if(SFD(nextin).mBufLen == 0) {
                    runinfo->mRCode = RCODE_NFUN_NULL_INPUT;
                }
                else {
                    runinfo->mRCode = 0;
                }
            }
        }
        else {
            runinfo->mRCode = 0;
        }
    }

    return TRUE;
}

BOOL cmd_curses_is_raw_mode(sObject* nextin, sObject* nextout, sRunInfo* runinfo)
{
    if(mis_raw_mode()) {
        runinfo->mRCode = 0;
    }

    return TRUE;
}

void curses_object_init(sObject* self)
{
    sObject* curses = UOBJECT_NEW_GC(8, self, "curses", TRUE);
    uobject_init(curses);
    uobject_put(self, "curses", curses);

    uobject_put(curses, "initscr", NFUN_NEW_GC(cmd_curses_initscr, NULL, TRUE));
    uobject_put(curses, "endwin", NFUN_NEW_GC(cmd_curses_endwin, NULL, TRUE));
    uobject_put(curses, "getch", NFUN_NEW_GC(cmd_curses_getch, NULL, TRUE));
    uobject_put(curses, "move", NFUN_NEW_GC(cmd_curses_move, NULL, TRUE));
    uobject_put(curses, "refresh", NFUN_NEW_GC(cmd_curses_refresh, NULL, TRUE));
    uobject_put(curses, "clear", NFUN_NEW_GC(cmd_curses_clear, NULL, TRUE));
    uobject_put(curses, "printw", NFUN_NEW_GC(cmd_curses_printw, NULL, TRUE));
    uobject_put(curses, "is_raw_mode", NFUN_NEW_GC(cmd_curses_is_raw_mode, NULL, TRUE));
}

