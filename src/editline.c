#include "config.h"
#include "xyzsh.h"

#if !defined(__CYGWIN__)
#include <term.h>
#endif

#include <termios.h>
#include <string.h>
#include <strings.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <libgen.h>
#include <dirent.h>
#include <oniguruma.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <pwd.h>
#include <limits.h>

#if defined(HAVE_CURSES_H)
#include <curses.h>
#elif defined(HAVE_NCURSES_H)
#include <ncurses.h>
#elif defined(HAVE_NCURSES_NCURSES_H)
#include <ncurses/ncurses.h>
#endif

#include "el.h"
#include "tty.h"
#include "refresh.h"

#include "histedit.h"

static EditLine* gEditLine;
static HistoryW* gHistory;
static sObject* gCmdLineStack;

static void sig_int_editline(int signo)
{
    printf("\r\033[0K");
    tty_rawmode(gEditLine);  // I don't know why tty settings is changed by CTRL-C signal handler
/*

    //ed_tty_sigint(gEditLine, 0);

    //ed_redisplay(gEditLine, 0);
    //ed_newline(gEditLine, 0);
    //el_deletestr(gEditLine, 10);
    //ch_reset(gEditLine, 0);
    //re_refresh_cursor(gEditLine);

    //el_reset(gEditLine);

    ed_move_to_beg(gEditLine, 0);
    ed_kill_line(gEditLine, 0);
    prompt_print(gEditLine, 0);
    //ch_reset(gEditLine, 0);
    //re_clear_display(gEditLine);

    re_refresh(gEditLine);

    //if (gEditLine->el_flags & UNBUFFERED)
//        terminal__flush(gEditLine);

    //el_wset(gEditLine, EL_REFRESH);

    //read_prepare(gEditLine);

    el_resize();
    re_clear_display(el);
    ch_reset(el, 0);
    re_refresh(el);

if (el->el_flags & UNBUFFERED)
        terminal__flush(el);
//        re_clear_display(el);
//        re_refresh(el);
//        terminal__flush(el);
*/
}

static int editline_signal()
{
    xyzsh_set_signal();

    struct sigaction sa2;
    memset(&sa2, 0, sizeof(sa2));
    sa2.sa_handler = sig_int_editline;
    //sa2.sa_flags |= SA_RESTART;
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

static wchar_t* gEditlinePrompt = NULL;
static wchar_t* gEditlineRPrompt = NULL;

static wchar_t* editline_prompt(EditLine* el)
{
    return gEditlinePrompt;
}

static wchar_t* editline_rprompt(EditLine* el)
{
    return gEditlineRPrompt;
}

static sObject* gCompletionArray;
static sObject* gReadlineCurrentObject;
static sObject* gReadlineBlock;

static BOOL name_sort(void* left, void* right)
{
    char* lfname = left;
    char* rfname = right;

    if(strcmp(lfname, ".") == 0) return TRUE;
    if(strcmp(lfname, "..") == 0) {
        if(strcmp(rfname, ".") == 0) return FALSE;

        return TRUE;          
    }
    if(strcmp(rfname, ".") == 0) return FALSE;
    if(strcmp(rfname, "..") == 0) return FALSE;

    return strcmp(lfname, rfname) < 0;
}

static void all_program_completion()
{
    sObject* hash = HASH_NEW_STACK(16);

    sObject* current = gReadlineCurrentObject;
    while(1) {
        uobject_it* it = uobject_loop_begin(current);
        while(it) {
            char* key = uobject_loop_key(it);
            sObject* object = uobject_loop_item(it);

            if(hash_item(hash, key) == NULL) {
                hash_put(hash, key, object);

                sObject* candidate = STRING_NEW_STACK(uobject_loop_key(it));
                if(STYPE(object) == T_UOBJECT) {
                    if(uobject_item(object, "main")) {
                        vector_add(gCompletionArray, string_c_str(candidate));
                    }
                    else {
                        sObject* candidate2 = STRING_NEW_STACK(uobject_loop_key(it));
                        string_push_back(candidate2, "::");
                        vector_add(gCompletionArray, string_c_str(candidate2));
                    }
                }
                else {
                    vector_add(gCompletionArray, string_c_str(candidate));
                }
            }

            it = uobject_loop_next(it);
        }
        
        if(current == gRootObject) break;

        current = SUOBJECT(current).mParent;

        if(current == NULL) break;
    }
    vector_sort(gCompletionArray, name_sort);
}

static void message_completion()
{
//puts("message_completion");
    sStatment* statment = SBLOCK(gReadlineBlock).mStatments + SBLOCK(gReadlineBlock).mStatmentsNum - 1;
    
    sCommand* command = statment->mCommands + statment->mCommandsNum-1;

    sObject* current = gReadlineCurrentObject;
    sObject* object;

    sObject* messages = STRING_NEW_STACK("");

    if(command->mMessages[0][0] == 0) {
        object = gRootObject;
    }
    else {
        while(1) {
            object = uobject_item(current, command->mMessages[0]);
            if(object || current == gRootObject) break;
            current = SUOBJECT((current)).mParent;
            if(current == NULL) break;
        }
    }

    string_put(messages, command->mMessages[0]);
    string_push_back(messages, "::");

    if(object && STYPE(object) == T_UOBJECT) {
        int i;
        for(i=1; i<command->mMessagesNum; i++) {
            object = uobject_item(object, command->mMessages[i]);
            if(object == NULL || STYPE(object) != T_UOBJECT) break;
            string_push_back(messages, command->mMessages[i]);
            string_push_back(messages, "::");
        }

        if(object && STYPE(object) == T_UOBJECT) {
            uobject_it* it = uobject_loop_begin(object);
            while(it) {
                char* key = uobject_loop_key(it);
                sObject* item = uobject_loop_item(it);

                sObject* candidate = STRING_NEW_STACK(string_c_str(messages));
                string_push_back(candidate, key);

                if(STYPE(item) == T_UOBJECT) {
                    if(uobject_item(item, "main")) {
                        vector_add(gCompletionArray, string_c_str(candidate));
                    }
                    else {
                        sObject* candidate2 = STRING_NEW_STACK(string_c_str(messages));
                        string_push_back(candidate2, key);
                        string_push_back(candidate2, "::");

                        vector_add(gCompletionArray, string_c_str(candidate2));
                    }
                }
                else {
                    vector_add(gCompletionArray, string_c_str(candidate));
                }

                it = uobject_loop_next(it);
            }

            vector_sort(gCompletionArray, name_sort);
        }
    }
}

static void program_completion()
{
//puts("program_completion");
    sObject* hash = HASH_NEW_STACK(16);

    sObject* current = gReadlineCurrentObject;
    while(1) {
        uobject_it* it = uobject_loop_begin(current);
        while(it) {
            char* key = uobject_loop_key(it);
            sObject* object = uobject_loop_item(it);

            if(hash_item(hash, key) == NULL) {
                hash_put(hash, key, object);

                sObject* candidate = STRING_NEW_STACK(uobject_loop_key(it));
                if(STYPE(object) == T_UOBJECT) {
                    if(uobject_item(object, "main")) {
                        vector_add(gCompletionArray, string_c_str(candidate));
                    }
                    else {
                        sObject* candidate2 = STRING_NEW_STACK(uobject_loop_key(it));
                        string_push_back(candidate2, "::");
                        vector_add(gCompletionArray, string_c_str(candidate2));
                    }
                }
                else {
                    vector_add(gCompletionArray, string_c_str(candidate));
                }
            }

            it = uobject_loop_next(it);
        }
        
        if(current == gRootObject) break;

        current = SUOBJECT(current).mParent;

        if(current == NULL) break;
    }
    vector_sort(gCompletionArray, name_sort);
}

static sObject* gUserCompletionNextout;

static void editline_get_last_word(EditLine* el, sObject* last_word, BOOL* squote, BOOL* dquote)
{
    const LineInfoW *lf = el_wline(el);

    wchar_t* cmdline = MALLOC(sizeof(wchar_t)*(lf->cursor - lf->buffer + 1));
    int i;
    for(i=0; i<lf->cursor - lf->buffer; i++) {
        cmdline[i] = lf->buffer[i];
    }
    cmdline[i] = 0;

    const int size = (wcslen(cmdline) + 1) * MB_LEN_MAX;
    char* buf = MALLOC(size);
    wcstombs(buf, cmdline, size);

    FREE(cmdline);

    const char* word_break_character = " \t\n\"'|!&;()<>=";

    char* p = buf;

    *squote = FALSE;
    *dquote = FALSE;

    BOOL squote2 = FALSE;
    BOOL dquote2 = FALSE;
    while(*p) {
        if(!squote2 && *p == '"') {
            dquote2 = !dquote2;
            string_push_back2(last_word, *p++);
        }
        else if(!dquote2 && *p == '\'') {
            squote2 = !squote2;
            string_push_back2(last_word, *p++);
        }
        else if(squote2 || dquote2) {
            string_push_back2(last_word, *p++);
        }
        else if(*p == '\\' && *(p+1) != 0) {
            string_push_back2(last_word, *p++);
            string_push_back2(last_word, *p++);
        }
        else {
            BOOL found = FALSE;
            char* p2 = (char*)word_break_character;
            while(*p2) {
                if(*p == *p2) {
                    found = TRUE;
                    break;
                }
                else {
                    p2++;
                }
            }

            if(found) {
                p++;

                string_put(last_word, "");

                if(*p == '\'') {
                    *squote = TRUE;
                    *dquote = FALSE;
                    p++;
                    squote2 = TRUE;
                }
                else if(*p == '"') {
                    *squote = FALSE;
                    *dquote = TRUE;
                    dquote2 = TRUE;
                    p++;
                }
                else {
                    *squote = FALSE;
                    *dquote = FALSE;
                }
            }
            else {
                string_push_back2(last_word, *p++);
            }
        }
    }

    FREE(buf);
}

static BOOL user_completion(sObject* ucompletion, sObject* cmdline, sCommand* command)
{
//puts("user_completion");
    sObject* nextin = FD_NEW_STACK();
    if(!fd_write(nextin, string_c_str(cmdline), string_length(cmdline))) {
        return FALSE;
    }
    sObject* nextout = FD_NEW_STACK();

    sObject* fun = FUN_NEW_STACK(NULL);
    sObject* stackframe = UOBJECT_NEW_GC(8, gXyzshObject, "_stackframe", FALSE);
    vector_add(gStackFrames, stackframe);
    //uobject_init(stackframe);
    SFUN(fun).mLocalObjects = stackframe;

    sObject* argv = VECTOR_NEW_GC(16, FALSE);
    if(command->mArgsNum == 1) {
        vector_add(argv, STRING_NEW_GC(command->mArgs[0], FALSE));

        sObject* last_word = STRING_NEW_GC("", FALSE);
        BOOL squote;
        BOOL dquote;
        editline_get_last_word(gEditLine, last_word, &squote, &dquote);
        vector_add(argv, last_word);
    }
    else if(command->mArgsNum > 1) {
        vector_add(argv, STRING_NEW_GC(command->mArgs[0], FALSE));

        sObject* last_word = STRING_NEW_GC("", FALSE);
        BOOL squote;
        BOOL dquote;
        editline_get_last_word(gEditLine, last_word, &squote, &dquote);
        vector_add(argv, last_word);
    }
    else {
        vector_add(argv, STRING_NEW_GC("", FALSE));
        vector_add(argv, STRING_NEW_GC("", FALSE));
    }
    uobject_put(SFUN(fun).mLocalObjects, "ARGV", argv);

    int rcode = 0;
    xyzsh_set_signal();
    if(!run(SCOMPLETION(ucompletion).mBlock, nextin, nextout, &rcode, gReadlineCurrentObject, fun)) {
        editline_signal();
        (void)vector_pop_back(gStackFrames);
        fprintf(stderr, "\nrun time error\n");
        fprintf(stderr, "%s", string_c_str(gErrMsg));
        return FALSE;
    }
    (void)vector_pop_back(gStackFrames);
    editline_signal();

    eLineField lf;
    if(fd_guess_lf(nextout, &lf)) {
        fd_split(nextout, lf, TRUE, FALSE, TRUE);
    } else {
        fd_split(nextout, kLF, TRUE, FALSE, TRUE);
    }

    int i;
    for(i=0; i<vector_count(SFD(nextout).mLines); i++) {
        char* candidate = vector_item(SFD(nextout).mLines, i);
//printf("candidate (%s)\n", candidate);
        if(candidate[0]) vector_add(gCompletionArray, candidate);
    }

    vector_sort(gCompletionArray, name_sort);

    return TRUE;
}

static void split_prefix_of_object_and_name(sObject** object, sObject* prefix, sObject* name, char* str, sObject* current_object)
{
    BOOL first = TRUE;
    char* p = str;
    while(*p) {
        if(*p == ':' && *(p+1) == ':') {
            p+=2;

            if(string_c_str(name)[0] == 0) {
                if(first) {
                    first = FALSE;

                    *object = gRootObject;
                    string_push_back(prefix, string_c_str(name));
                    string_push_back(prefix, "::");
                }
            }
            else {
                string_push_back(prefix, string_c_str(name));
                string_push_back(prefix, "::");

                if(*object && STYPE(*object) == T_UOBJECT) {
                    if(first) {
                        first = FALSE;

                        *object = access_object3(string_c_str(name), &current_object);
                    }
                    else {
                        *object = uobject_item(*object, string_c_str(name));
                    }
                    string_put(name, "");
                }
                else {
                    string_put(name, "");
                    break;
                }
            }
        }
        else {
            string_push_back2(name, *p);
            p++;
        }
    }
}

#if !HAVE_DECL_ENVIRON
            extern char **environ;
#endif

static void env_completion(EditLine* el)
{
//puts("env_completion");
    sObject* last_word = STRING_NEW_STACK("");
    BOOL squote;
    BOOL dquote;
    editline_get_last_word(el, last_word, &squote, &dquote);

    /// go ///
    sObject* hash = HASH_NEW_STACK(16);

    char** p;
    for(p = environ; *p; p++) {
        char env_name[PATH_MAX];

        char* p2 = env_name;
        char* p3 = *p;

        while(*p3 != 0 && *p3 != '=') {
            *p2++ = *p3++;
        }

        char* env = getenv(*p);
        struct stat estat;
        if(stat(env, &estat) >= 0) {
            if(S_ISDIR(estat.st_mode)) {
                *p2++ = '/';
            }
        }
        *p2 = 0;

        sObject* string = STRING_NEW_STACK("$");
        string_push_back(string, env_name);
        vector_add(gCompletionArray, string_c_str(string));
    }

    if(strstr(string_c_str(last_word), "::")) {
        sObject* current = gReadlineCurrentObject;
        sObject* prefix = STRING_NEW_STACK("");
        sObject* name = STRING_NEW_STACK("");

        split_prefix_of_object_and_name(&current, prefix, name, string_c_str(last_word), gReadlineCurrentObject);

        if(current && STYPE(current) == T_UOBJECT) {
            uobject_it* it = uobject_loop_begin(current);
            while(it) {
                char* key = uobject_loop_key(it);
                sObject* object = uobject_loop_item(it);

                if(STYPE(object) == T_UOBJECT) {
                    sObject* candidate = STRING_NEW_STACK("$");
                    string_push_back(candidate, string_c_str(prefix));
                    string_push_back(candidate, key);
                    string_push_back(candidate, "::");

                    vector_add(gCompletionArray, string_c_str(candidate));
                }
                else if(STYPE(object) == T_STRING || STYPE(object) == T_VECTOR || STYPE(object) == T_HASH) {
                    sObject* candidate = STRING_NEW_STACK("$");
                    string_push_back(candidate, string_c_str(prefix));
                    string_push_back(candidate, key);

                    vector_add(gCompletionArray, string_c_str(candidate));
                }

                it = uobject_loop_next(it);
            }
        }
    }
    else {
        sObject* current = gReadlineCurrentObject;

        while(1) {
            uobject_it* it = uobject_loop_begin(current);
            while(it) {
                char* key = uobject_loop_key(it);
                sObject* object = uobject_loop_item(it);

                if(STYPE(object) == T_UOBJECT) {
                    sObject* candidate = STRING_NEW_STACK("$");
                    string_push_back(candidate, key);
                    string_push_back(candidate, "::");

//printf("candidate %s\n", string_c_str(candidate));
                    vector_add(gCompletionArray, string_c_str(candidate));
                }
                else if(STYPE(object) == T_STRING || STYPE(object) == T_VECTOR || STYPE(object) == T_HASH) {
                    sObject* candidate = STRING_NEW_STACK("$");
                    string_push_back(candidate, key);

                    vector_add(gCompletionArray, string_c_str(candidate));
//printf("candidate %s\n", string_c_str(candidate));
                }

                it = uobject_loop_next(it);
            }
            
            if(current == gRootObject) break;

            current = SUOBJECT(current).mParent;

            if(current == NULL) break;
        }
    }

    vector_sort(gCompletionArray, name_sort);
}

static BOOL filename_completion(sObject* cmdline)
{
//puts("filename_completion");
    int sline = 1;
    sObject* block = BLOCK_NEW_STACK();
    sObject* current_object = gCurrentObject;
    if(!parse("|::file_completion $ARGV[0]", "file_completion", &sline, block, &current_object)) {
        return FALSE;
    }

    sObject* nextin = FD_NEW_STACK();
    if(!fd_write(nextin, string_c_str(cmdline), string_length(cmdline))) {
        return FALSE;
    }
    sObject* nextout = FD_NEW_STACK();

    sObject* fun = FUN_NEW_STACK(NULL);
    sObject* stackframe = UOBJECT_NEW_GC(8, gXyzshObject, "_stackframe", FALSE);
    vector_add(gStackFrames, stackframe);
    //uobject_init(stackframe);
    SFUN(fun).mLocalObjects = stackframe;

    sObject* argv = VECTOR_NEW_GC(16, FALSE);

    sObject* last_word = STRING_NEW_GC("", FALSE);
    BOOL squote;
    BOOL dquote;
    editline_get_last_word(gEditLine, last_word, &squote, &dquote);
    vector_add(argv, last_word);

    uobject_put(SFUN(fun).mLocalObjects, "ARGV", argv);

    int rcode = 0;
    xyzsh_set_signal();
    if(!run(block, nextin, nextout, &rcode, gCurrentObject, fun)) {
        editline_signal();
        (void)vector_pop_back(gStackFrames);
        fprintf(stderr, "\nrun time error\n");
        fprintf(stderr, "%s", string_c_str(gErrMsg));
        return FALSE;
    }
    (void)vector_pop_back(gStackFrames);
    editline_signal();

    eLineField lf;
    if(fd_guess_lf(nextout, &lf)) {
        fd_split(nextout, lf, TRUE, FALSE, TRUE);
    } else {
        fd_split(nextout, kLF, TRUE, FALSE, TRUE);
    }

    int i;
    for(i=0; i<vector_count(SFD(nextout).mLines); i++) {
        char* candidate = vector_item(SFD(nextout).mLines, i);
//printf("candidate (%s)\n", candidate);
        if(candidate[0]) vector_add(gCompletionArray, candidate);
    }

    vector_sort(gCompletionArray, name_sort);

    return TRUE;
}

static BOOL null_completion()
{
    vector_clear(gCompletionArray);

    return TRUE;
}

static BOOL castamized_user_completion(sObject* ucompletion, sObject* cmdline)
{
    sObject* nextin = FD_NEW_STACK();
    if(!fd_write(nextin, string_c_str(cmdline), string_length(cmdline))) {
        return FALSE;
    }
    sObject* nextout = FD_NEW_STACK();

    sObject* fun = FUN_NEW_STACK(NULL);
    sObject* stackframe = UOBJECT_NEW_GC(8, gXyzshObject, "_stackframe", FALSE);
    vector_add(gStackFrames, stackframe);
    //uobject_init(stackframe);
    SFUN(fun).mLocalObjects = stackframe;

    sObject* argv = VECTOR_NEW_GC(16, FALSE);
    sObject* last_word = STRING_NEW_GC("", FALSE);
    BOOL squote;
    BOOL dquote;
    editline_get_last_word(gEditLine, last_word, &squote, &dquote);
    vector_add(argv, last_word);

    uobject_put(SFUN(fun).mLocalObjects, "ARGV", argv);

    int rcode = 0;
    xyzsh_set_signal();
    if(!run(SCOMPLETION(ucompletion).mBlock, nextin, nextout, &rcode, gReadlineCurrentObject, fun)) {
        editline_signal();
        (void)vector_pop_back(gStackFrames);
        fprintf(stderr, "\nrun time error\n");
        fprintf(stderr, "%s", string_c_str(gErrMsg));
        return FALSE;
    }
    (void)vector_pop_back(gStackFrames);
    editline_signal();

    eLineField lf;
    if(fd_guess_lf(nextout, &lf)) {
        fd_split(nextout, lf, TRUE, FALSE, TRUE);
    } else {
        fd_split(nextout, kLF, TRUE, FALSE, TRUE);
    }

    int i;
    for(i=0; i<vector_count(SFD(nextout).mLines); i++) {
        char* candidate = vector_item(SFD(nextout).mLines, i);
//printf("candidate (%s)\n", candidate);
        if(candidate[0]) vector_add(gCompletionArray, candidate);
    }

    vector_sort(gCompletionArray, name_sort);

    return TRUE;
}

static BOOL gPressingTabTwice = FALSE;

static BOOL gCompletionViewLines = -1;

static void completion_view_clear(EditLine* el, BOOL after_control_c)
{
    if(gCompletionViewLines > 0) {
        /// clear //
        const int maxx = mgetmaxx();
        char buf[1024];
        int j;
        for(j=0; j<maxx && j<1024; j++) {
            buf[j] = ' ';
        }
        buf[j] = 0;

        puts("");

        for(j=0; j<gCompletionViewLines; j++) {
            printf("%s\n", buf);
        }

        if(after_control_c) {
            /// delete lines and back to previous position ///
            const LineInfoW* lf = el_wline(el);
            const int prompt_h = el->el_prompt.p_pos.h;
            int v = (prompt_h + wcswidth(lf->buffer, wcslen(lf->buffer))) / maxx + 1;

            int i;
            for(i=0; i<gCompletionViewLines + v; i++) {
                printf("\033[%dA\r\033[0K", 1);
            }
        }
        else {
            /// back to previous position ///
            const LineInfoW* lf = el_wline(el);
            const int prompt_h = el->el_prompt.p_pos.h;
            int v = (prompt_h + wcswidth(lf->buffer, wcslen(lf->buffer))) / maxx + 1;

            int i;
            for(i=0; i<gCompletionViewLines + v; i++) {
                printf("\033[%dA", 1);
                //printf("\033[%dA\r\033[0K", 1);
            }

            /// redraw prompt ///
            printf("\r\033[0K");
            el_wset(gEditLine, EL_REFRESH);
        }

        gCompletionViewLines = -1;
    }
}

static void completion_view(EditLine* el, sObject* candidates, int omit_head_of_completion_display_matches, int cursor, int scroll_top, int* lines_len, int* cols_len, int* scrolling_lines)
{
    /// get max length of a candidate ///
    int i;
    int max_width = 0;
    for(i=0; i<vector_count(candidates); i++) {
        char* candidate = vector_item(candidates, i) + omit_head_of_completion_display_matches;
        int width = str_termlen(kUtf8, candidate);
        if(width > max_width) {
            max_width = width;
        }
    }
    
    max_width += 2;

    /// get lines length ///
    const int maxx = mgetmaxx();
    *cols_len = maxx / max_width;
    if(*cols_len == 0) *cols_len = 1;
    *lines_len = vector_count(candidates) / *cols_len;
    if(vector_count(candidates) % *cols_len > 0) (*lines_len)++;
    int c = 0;
    int l = 0;

    puts("");

    /// get height of cmdline ///
    const LineInfoW* lf = el_wline(el);
    const int prompt_h = el->el_prompt.p_pos.h;
    const int height_of_cmdline = (prompt_h + wcswidth(lf->buffer, wcslen(lf->buffer))) / maxx + 1;

    const int maxy = mgetmaxy();
    *scrolling_lines = maxy -1 -height_of_cmdline;

    /// view with scrolling ///
    if(*lines_len > *scrolling_lines) {
        /// draw ///
        for(l=scroll_top; l <scroll_top+*scrolling_lines && l<*lines_len; l++) {
            for(c=0; c<*cols_len; c++) {
                i = *lines_len * c + l;
                if(i < vector_count(candidates)) {
                    char* candidate = vector_item(candidates, i) + omit_head_of_completion_display_matches;
                    const int termlen = str_termlen(kUtf8, candidate);
                    char space[1024];
                    space[0] = 0;
                    int j;
                    for(j=0; j<max_width-termlen; j++) {
                        xstrncat(space, " ", 1024);
                    }
                    if(i == cursor) {
                        printf("\033[7m");
                        printf("%s", candidate);
                        printf("\033[0m");
                        printf("%s", space);
                    }
                    else {
                        printf("%s", candidate);
                        printf("%s", space);
                    }
                }
                else {
                    char space[1024];
                    space[0] = 0;
                    int j;
                    for(j=0; j<max_width; j++) {
                        xstrncat(space, " ", 1024);
                    }
                    printf("%s", space);
                }
            }
            puts("");
        }

        printf("\033[%dA", *scrolling_lines + height_of_cmdline);
        gCompletionViewLines = *scrolling_lines;  // used by completion_view_clear
    }
    /// view to only one screen ///
    else {
        /// draw ///
        for(l=0; l<*lines_len; l++) {
            for(c=0; c<*cols_len; c++) {
                i = *lines_len * c + l;
                if(i < vector_count(candidates)) {
                    char* candidate = vector_item(candidates, i) + omit_head_of_completion_display_matches;
                    const int termlen = str_termlen(kUtf8, candidate);
                    char space[1024];
                    space[0] = 0;
                    int j;
                    for(j=0; j<max_width-termlen; j++) {
                        xstrncat(space, " ", 1024);
                    }
                    if(i == cursor) {
                        printf("\033[7m");
                        printf("%s", candidate);
                        printf("\033[0m");
                        printf("%s", space);
                    }
                    else {
                        printf("%s", candidate);
                        printf("%s", space);
                    }
                }
            }
            puts("");
        }

        printf("\033[%dA", *lines_len + height_of_cmdline);
        gCompletionViewLines = *lines_len; // used by completion_view_clear after this
    }

    /// must be refresh cmdline ///
    el_wset(gEditLine, EL_REFRESH);
}

struct termios gSavedTty;

static void my_initscr()
{
    struct termios t;
    int x,y;
    int i, j;
    struct sKeyMap* item;

    tcgetattr(STDIN_FILENO, &gSavedTty);

    tcgetattr(STDIN_FILENO, &t);

    t.c_lflag &= ~(ICANON | ECHO | ISIG | ECHOE);

# if defined(IEXTEN) && !defined(__MINT__)
    t.c_lflag &= ~IEXTEN;
# endif

/*
# ifdef ONLCR
   t.c_oflag &= ~ONLCR;
# endif
    t.c_iflag &= ~ICRNL;
*/

/*
#ifdef ECHOPRT
    t.c_lflag &= ~(ICANON | ISIG | ECHO | ECHOCTL | ECHOE| ECHOK | ECHOKE
                         | ECHONL | ECHOPRT);
#else
    t.c_lflag &= ~(ICANON | ISIG | ECHO | ECHOCTL | ECHOE | ECHOK | ECHOKE
                  | ECHONL);
#endif
*/
    t.c_iflag |= IGNBRK;
    t.c_iflag &= ~(IXOFF|IXON);

    t.c_cc[VMIN] = 0;
    t.c_cc[VTIME] = 0;
    t.c_cc[VLNEXT] = 0;
    t.c_cc[VDISCARD] = 0;

    tcsetattr(STDIN_FILENO, TCSANOW, &t);
}

static void my_endwin()
{
    tcsetattr(STDIN_FILENO, TCSANOW, &gSavedTty);
}

static unsigned char my_getch()
{
    fd_set mask, read_ok;

    FD_ZERO(&mask);
    FD_SET(0, &mask);

    unsigned char key = 0;
    while(1) {
        read_ok = mask;

        select(1, &read_ok, NULL, NULL, NULL);

        if(FD_ISSET(0, &read_ok)) {
            unsigned char buf[128];
            int n;
            if((n = read(0, buf, 1)) == 1) {
                key = buf[0];
                break;
            }
        }
    }

    return key;
}

static el_action_t completion_core(EditLine* el, int ch, char* non_append_space_characters, int omit_head_of_completion_display_matches)
{
    unsigned char res = 0;

    sObject* last_word = STRING_NEW_STACK("");
    BOOL squote, dquote;
    editline_get_last_word(el, last_word, &squote, &dquote);

    const int mblen = string_length(last_word);

    // Scan completion array for matching name
    const int common_head_point_max = 9999;
    int common_head_point = common_head_point_max;
    sObject* candidates = VECTOR_NEW_STACK(10);
    char* last_match = NULL;
    int i;
    for(i=0; i<vector_count(gCompletionArray); i++) {
        char* candidate = vector_item(gCompletionArray, i);

        if(mblen == 0 || mblen <= strlen(candidate) && strncmp(candidate, string_c_str(last_word), mblen) == 0) {
            int chp = -1;
            int an_end = 0;
            int an_end_before = 0;
            int j;
            for(j=0; mblen != 0 && candidate && j<strlen(candidate) && last_match && j<strlen(last_match); j++) {
                if(an_end == j) {
                    const unsigned char c = candidate[j];
                    if(c >= 128) {
                        const int size = ((c & 0x80) >> 7) + ((c & 0x40) >> 6) + ((c & 0x20) >> 5) + ((c & 0x10) >> 4);
                        an_end_before = an_end;
                        an_end += size;
                    }
                    else {
                        an_end_before = an_end;
                        an_end++;
                    }
                }

                if(candidate[j] == last_match[j]) {
                    chp = j;
                }
                else {
                    chp = an_end_before -1;
                    break;
                }
            }
//printf(" chp %d\n", chp);
            if(chp >= 0 && chp < common_head_point) common_head_point = chp;

            last_match = candidate;
            vector_add(candidates, candidate);
        }
    }
//printf("common_head_point %d\n", common_head_point);

    wchar_t dir[1024];

    /// matching count == 1 completion
    if(vector_count(candidates) == 1) {
//puts("matched_count == 1");
        mbstowcs(dir, &last_match[mblen], sizeof(dir) / sizeof(*dir)-1);
        if(strlen(last_match) > 0) {
            char last_character = last_match[strlen(last_match)-1];
            BOOL appended_space = TRUE;
            if(non_append_space_characters) {
                char* p = non_append_space_characters;
                while(*p) {
                    if(*p == last_character) {
                        appended_space = FALSE;
                        break;
                    }
                    else {
                        p++;
                    }
                }
            }
            if(wcslen(dir) < 1024-5) {
                if(appended_space) {
                    if(squote) wcscat(dir, L"'");
                    else if(dquote) wcscat(dir, L"\"");

                    wcscat(dir, L" ");
                }

            }
        }

        if (el_winsertstr(el, dir) == -1) {
            res = CC_ERROR;
        }
        else {
            res = CC_REFRESH;
        }
    }
    else if(vector_count(candidates) > 1) {
        char* menu_select = getenv("XYZSH_MENU_SELECT");

        /// menu select ///
        if(gPressingTabTwice && menu_select && strcasecmp(menu_select, "on") == 0) {
            my_initscr();
            int cursor = 0;
            const int maxy = mgetmaxy();
            int scroll_top = 0;

            const int cleft = 0;
            const int cup = 1;
            const int cright = 2;
            const int cdown = 3;

            const int cleft2 = 4;
            const int cup2 = 5;
            const int cright2 = 6;
            const int cdown2 = 7;

            const int cursor_max = 8;

            char ckeycode[cursor_max][32];

            if(setupterm(NULL, fileno(stdout), (int*)0) == OK) {
                xstrncpy(ckeycode[cleft], tigetstr("kcub1"), 32);
                xstrncpy(ckeycode[cup], tigetstr("kcuu1"), 32);
                xstrncpy(ckeycode[cright], tigetstr("kcuf1"), 32);
                xstrncpy(ckeycode[cdown], tigetstr("kcud1"), 32);

                xstrncpy(ckeycode[cleft2], tparm(tigetstr("cub"), 0), 32);
                xstrncpy(ckeycode[cup2] , tigetstr("cuu1"), 32);
                xstrncpy(ckeycode[cright2] , tigetstr("cuf1"), 32);
                xstrncpy(ckeycode[cdown2] , tparm(tigetstr("cud"), 0), 32);
            }
            else {
                memset(ckeycode, 0, sizeof(char)*32*cursor_max);
            }

            while(1) {
                int lines_len;
                int cols_len;
                int scrolling_lines;
                completion_view(el, candidates, omit_head_of_completion_display_matches, cursor, scroll_top, &lines_len, &cols_len, &scrolling_lines);

                int key;

                const int buf_max = 32;
                char buf[buf_max];

                int i = 0;
                buf[i] = my_getch();
//printf("i %d buf[i] %d (%c)\n", i, buf[i], buf[i]);

                if(buf[i] == '\t' || buf[i] == 'm'-'a'+1 || buf[i] == 'j'-'a'+1 || buf[i] == 'd'-'a'+1 || buf[i] == 'u'-'a'+1) {
                    key = buf[i];
                }
                else {
                    do {
                        key = -1;
                        BOOL continue_ = FALSE;
                        int j;
                        for(j=0; j<cursor_max; j++) {
                            if(ckeycode[j][0] && buf[i] == ckeycode[j][i]) {
                                if(ckeycode[j][i+1] == 0) {
//puts("get key");
                                    key = j + 128;
                                }
                                else {
//puts("continue");
                                    continue_ = TRUE;
                                }
                                break;
                            }
                        }

                        if(key != -1 || !continue_) {
                            break;
                        }

                        i++;
                        buf[i] = my_getch();
//printf("i %d buf[i] %d (%c)\n", i, buf[i], buf[i]);
                    } while(i < buf_max);
                }
                
                /// next ///
                if(key == '\t') {
                    cursor++;
                }
                /// left ///
                else if(key == 128+cleft || key == 128+cleft2) {
                    cursor-=lines_len;

                    if(cursor >= vector_count(candidates)) {
                        cursor -= lines_len * cols_len;

                        if(cursor <0) {
                            cursor += lines_len;
                        }
                    }
                    if(cursor < 0) {
                        cursor += lines_len * cols_len;

                        if(cursor >= vector_count(candidates)) {
                            cursor -= lines_len;
                        }
                    }
                }
                /// up ///
                else if(key == 128+cup || key == 128+cup2) {
                    cursor--;
                }
                /// right ///
                else if(key == 128+cright || key == 128+cright2) {
                    cursor+=lines_len;

                    if(cursor >= vector_count(candidates)) {
                        cursor -= lines_len * cols_len;

                        if(cursor <0) {
                            cursor += lines_len;
                        }
                    }
                    if(cursor < 0) {
                        cursor += lines_len * cols_len;

                        if(cursor >= vector_count(candidates)) {
                            cursor -= lines_len;
                        }
                    }
                }
                /// down ///
                else if(key == 128+cdown || key == 128+cdown2) {
                    cursor++;
                }
                /// control-m control-j ///
                else if(key == 'm'-'a'+1 || key == 'j'-'a'+1) {
                    break;
                }
                /// control-d ///
                else if(key == 'd'-'a'+1) {
                    cursor += 10;
                }
                /// control-u ///
                else if(key == 'u'-'a'+1) {
                    cursor -= 10;
                }
                else {
                    cursor = -1;
                    break;
                }

                /// corect the values ///
                if(cursor >= vector_count(candidates)) {
                    cursor = 0;
                }
                if(cursor < 0) {
                    cursor = vector_count(candidates) -1;
                }
                if(scroll_top < 0) {
                    scroll_top = 0;
                }
                if(scroll_top > lines_len) {
                    scroll_top = lines_len - 1;
                }
                if(cursor % lines_len < scroll_top) {
                    scroll_top = 0;
                }
                if(cursor % lines_len > scroll_top + scrolling_lines-1) {
                    scroll_top = (cursor % lines_len) - scrolling_lines+1;
                }
            }
            my_endwin();

            if(cursor >= 0) {
                char* candidate = vector_item(candidates, cursor);

                if(candidate) {
                    const int len = strlen(candidate) - string_length(last_word) + 1;
                    char* str = MALLOC(len);
                    memcpy(str, candidate + string_length(last_word), len);
                    
                    mbstowcs(dir, str, sizeof(dir) / sizeof(*dir)-1);

                    char last_character = candidate[strlen(candidate)-1];
                    BOOL appended_space = TRUE;
                    if(non_append_space_characters) {
                        char* p = non_append_space_characters;
                        while(*p) {
                            if(*p == last_character) {
                                appended_space = FALSE;
                                break;
                            }
                            else {
                                p++;
                            }
                        }
                    }
                    if(wcslen(dir) < 1024-5) {
                        if(appended_space) {
                            if(squote) wcscat(dir, L"'");
                            else if(dquote) wcscat(dir, L"\"");

                            wcscat(dir, L" ");
                        }
                    }

                    FREE(str);

                    if (el_winsertstr(el, dir) == -1) {
                        res = CC_ERROR;
                    }
                    else {
                        res = CC_REFRESH;
                    }
                }
            }

            completion_view_clear(el, FALSE);
            //el_wset(gEditLine, EL_REFRESH);
            gPressingTabTwice = FALSE;
        }
        else {
            /// common head of candidates completion ///
            if(common_head_point != common_head_point_max && common_head_point >= string_length(last_word)) {
                char* common_head = MALLOC(common_head_point -mblen+2);
                memcpy(common_head, last_match + mblen, common_head_point - mblen+1);
                common_head[common_head_point - mblen+1] = 0;

                mbstowcs(dir, common_head, sizeof(dir) / sizeof(*dir));

                FREE(common_head);

                if (el_winsertstr(el, dir) == -1)
                    res = CC_ERROR;
                else
                    res = CC_REFRESH;
            }
            /// view ///
            else {
                int lines_len;
                int cols_len;
                int scrolling_lines;
                completion_view(el, candidates, omit_head_of_completion_display_matches, -1, 0, &lines_len, &cols_len, &scrolling_lines);

                gPressingTabTwice = TRUE;
            }
        }
    }

    return res;
}

static void get_current_completion_object(sObject** completion_object, sObject** current_object)
{
    if(*current_object && *current_object != gRootObject) {
        sObject* parent_object = SUOBJECT(*current_object).mParent;
        get_current_completion_object(completion_object, &parent_object);
        if(*completion_object) *completion_object = uobject_item(*completion_object, SUOBJECT(*current_object).mName);
    }
}

static sObject* access_object_compl(char* name, sObject** current)
{
    sObject* object;

    while(1) {
        object = uobject_item(*current, name);

        if(object || *current == gCompletionObject) { return object; }

        *current = SUOBJECT((*current)).mParent;

        if(*current == NULL) return NULL;;
    }
}

static unsigned char call_filename_completion(EditLine* el, int ch, sObject* cmdline)
{
//puts("call_filename_completion");
    sObject* el_obj = uobject_item(gRootObject, "el");
    sObject* var;
    if(el_obj && STYPE(el_obj) == T_UOBJECT) {
        uobject_put(el_obj, "omit_head_of_completion_display_matches", STRING_NEW_GC("0", TRUE));
        uobject_put(el_obj, "non_append_space_characters", STRING_NEW_GC("", TRUE));
    }

    filename_completion(cmdline);

    char* non_append_space_characters;
    int omit_head_of_completion_display_matches;
    if(el_obj && STYPE(el_obj) == T_UOBJECT) {
        var = uobject_item(el_obj, "non_append_space_characters");
        if(var && STYPE(var) == T_STRING) {
            non_append_space_characters = string_c_str(var);
        }
        else {
            non_append_space_characters = NULL;
        }

        var = uobject_item(el_obj, "omit_head_of_completion_display_matches");
        if(var && STYPE(var) == T_STRING) {
            omit_head_of_completion_display_matches = atoi(string_c_str(var));
        }
        else {
            omit_head_of_completion_display_matches = 0;
        }
    }
    else {
        non_append_space_characters = NULL;
        omit_head_of_completion_display_matches = 0;
    }

    return completion_core(el, ch, non_append_space_characters, omit_head_of_completion_display_matches);
}

static BOOL gInhibitCompletion = FALSE;

static el_action_t complete(EditLine *el, int ch)
{
    int res;
    stack_start_stack();

    gReadlineBlock = BLOCK_NEW_STACK();

    const LineInfoW *lf = el_wline(el);
    wchar_t* line = MALLOC(sizeof(wchar_t)*(lf->cursor - lf->buffer + 1));
    int i;
    for(i=0; i<lf->cursor - lf->buffer; i++) {
        line[i] = lf->buffer[i];
    }
    line[i] = 0;

    const int size = MB_LEN_MAX * (wcslen(line) + 1);
    char* buf = MALLOC(size);
    wcstombs(buf, line, size);
    FREE(line);

    sObject* cmdline = STRING_NEW_STACK("");
    string_push_back(cmdline, buf);
    FREE(buf);

//printf("cmdline (%s)\n", string_c_str(cmdline));

    int sline = 1;
    gReadlineCurrentObject = gCurrentObject;
    BOOL result = parse(string_c_str(cmdline), "readline", &sline, gReadlineBlock, &gReadlineCurrentObject);

    /// in the block? get the block
    if(!result && (SBLOCK(gReadlineBlock).mCompletionFlags & (COMPLETION_FLAGS_BLOCK|COMPLETION_FLAGS_ENV_BLOCK))) {
        while(1) {
            if(SBLOCK(gReadlineBlock).mStatmentsNum > 0) {
                sStatment* statment = SBLOCK(gReadlineBlock).mStatments + SBLOCK(gReadlineBlock).mStatmentsNum - 1;

                if(statment->mCommandsNum > 0) {
                    sCommand* command = statment->mCommands + statment->mCommandsNum-1;

                    int num = SBLOCK(gReadlineBlock).mCompletionFlags & COMPLETION_FLAGS_BLOCK_OR_ENV_NUM;

                    if(num > 0) {
                        if(SBLOCK(gReadlineBlock).mCompletionFlags & COMPLETION_FLAGS_ENV_BLOCK) {
                            sEnv* env = command->mEnvs + num -1;
                            gReadlineBlock = env->mBlock;
                        }
                        else {
                            gReadlineBlock = *(command->mBlocks + num -1);
                        }
                    }
                    else {
                        break;
                    }
                }
                else {
                    break;
                }
            }
            else {
                break;
            }
        }
    }

    gCompletionArray = VECTOR_NEW_STACK(16);

    if(SBLOCK(gReadlineBlock).mStatmentsNum == 0) {
//puts("all_program_completion1");
        all_program_completion();
        res = completion_core(el, ch, ":", 0);
        stack_end_stack();
        return res;
    }
    else if(SBLOCK(gReadlineBlock).mCompletionFlags & COMPLETION_FLAGS_ENV) {
//puts("env_completion");
        env_completion(el);
        res = completion_core(el, ch, ":/", 0);
        stack_end_stack();
        return res;
    }
    else if(SBLOCK(gReadlineBlock).mCompletionFlags & (COMPLETION_FLAGS_AFTER_REDIRECT|COMPLETION_FLAGS_AFTER_EQUAL)) {
//puts("redirect equal");
        res = call_filename_completion(el, ch, cmdline);
        stack_end_stack();
        return res;
    }
    else {
        sStatment* statment = SBLOCK(gReadlineBlock).mStatments + SBLOCK(gReadlineBlock).mStatmentsNum - 1;

        if(statment->mCommandsNum == 0 || SBLOCK(gReadlineBlock).mCompletionFlags & (COMPLETION_FLAGS_COMMAND_END|COMPLETION_FLAGS_STATMENT_END|COMPLETION_FLAGS_STATMENT_HEAD)) {
//puts("all_program_completion2");
            all_program_completion();
            res = completion_core(el, ch, ":", 0);
            stack_end_stack();
            return res;
        }
        else {
            if(statment->mCommandsNum == 1 && statment->mCommands[0].mArgsNum == 1 && !(SBLOCK(gReadlineBlock).mCompletionFlags & COMPLETION_FLAGS_AFTER_SPACE)) {
//puts("call_filename_completion");
                char* arg = statment->mCommands[0].mArgs[0];
                if(strstr(arg, "/")) {
                    res = call_filename_completion(el, ch, cmdline);
                    stack_end_stack();
                    return res;
                }
            }

            sCommand* command = statment->mCommands + statment->mCommandsNum-1;

            /// get user completion ///
            sObject* ucompletion;
            if(command->mArgsNum > 0) {
                sObject* completion_object = gCompletionObject;
                sObject* current_object = gCurrentObject;

                get_current_completion_object(&completion_object, &current_object);

                if(completion_object == NULL) completion_object = gCompletionObject;

                sObject* object;
                if(command->mMessagesNum > 0) {
                    sObject* reciever = completion_object;
                    object = access_object_compl(command->mMessages[0], &reciever);

                    int i;
                    for(i=1; i<command->mMessagesNum; i++) {
                        if(object && STYPE(object) == T_UOBJECT) {
                            object = uobject_item(object, command->mMessages[i]);
                        }
                        else {
                            break;
                        }
                    }

                    if(object && STYPE(object) == T_UOBJECT) {
                        ucompletion = uobject_item(object, command->mArgs[0]);

                        if(ucompletion == NULL) {
                            ucompletion = uobject_item(object, "__all__");;
                        }
                    }
                    else {
                        ucompletion = NULL;
                    }
                }
                else {
                    sObject* reciever = completion_object;
                    ucompletion = access_object_compl(command->mArgs[0], &reciever);

                    if(ucompletion == NULL) {
                        reciever = completion_object;
                        ucompletion = access_object_compl("__all__", &reciever);
                    }
                }
            }
            else {
                ucompletion = NULL;
            }

            /// go ///
            if(command->mArgsNum == 0 
                || command->mArgsNum == 1 && SBLOCK(gReadlineBlock).mCompletionFlags & COMPLETION_FLAGS_INPUTING_COMMAND_NAME)
            {
                if(command->mMessagesNum > 0) {
//puts("message_completion");
                    message_completion();
                    res = completion_core(el, ch, ":", 0);
                    stack_end_stack();
                    return res;
                }
                else {
//puts("program_completion");
                    program_completion();
                    if(command->mArgsNum > 0) {
                        res = completion_core(el, ch, ":", 0);
                    }
                    else {
                        res = completion_core(el, ch, ":", 0);
                    }
                    stack_end_stack();
                    return res;
                }
            }
            else if(ucompletion && STYPE(ucompletion) == T_COMPLETION) {
                sObject* el_obj = uobject_item(gRootObject, "el");
                sObject* var;
                if(el_obj && STYPE(el_obj) == T_UOBJECT) {
                    uobject_put(el_obj, "omit_head_of_completion_display_matches", STRING_NEW_GC("0", TRUE));
                    uobject_put(el_obj, "non_append_space_characters", STRING_NEW_GC("", TRUE));
                }

                gInhibitCompletion = FALSE;
//puts("user_completion");
                if(!user_completion(ucompletion, cmdline, command)) {
                    //res = call_filename_completion(el, ch, cmdline);
                    stack_end_stack();
                    return res;
                }
                
                if(gInhibitCompletion) {
                    res = CC_REFRESH;
                }
                else {
                    char* non_append_space_characters;
                    int omit_head_of_completion_display_matches;
                    if(el_obj && STYPE(el_obj) == T_UOBJECT) {
                        var = uobject_item(el_obj, "non_append_space_characters");
                        if(var && STYPE(var) == T_STRING) {
                            non_append_space_characters = string_c_str(var);
                        }
                        else {
                            non_append_space_characters = NULL;
                        }

                        var = uobject_item(el_obj, "omit_head_of_completion_display_matches");
                        if(var && STYPE(var) == T_STRING) {
                            omit_head_of_completion_display_matches = atoi(string_c_str(var));
                        }
                        else {
                            omit_head_of_completion_display_matches = 0;
                        }
                    }
                    else {
                        non_append_space_characters = NULL;
                        omit_head_of_completion_display_matches = 0;
                    }
                    res = completion_core(el, ch, non_append_space_characters, omit_head_of_completion_display_matches);
                }

                stack_end_stack();
                return res;
            }
        }
    }

    stack_end_stack();

    return 0;
}

static el_action_t null_complete(EditLine *el, int ch)
{
    (void)null_completion();
    return completion_core(el, ch, "", 0);
}

static el_action_t cmdline_macro(EditLine *el, int ch)
{
    el_action_t res;
    stack_start_stack();

    sObject* nextout2 = FD_NEW_STACK();

    int rcode = 0;
    sObject* block = BLOCK_NEW_STACK();
    int sline = 1;
    if(parse("::cmdline_macro", "cmdline_macro", &sline, block, NULL)) {
        xyzsh_set_signal();

        sObject* fun = FUN_NEW_STACK(NULL);
        sObject* stackframe = UOBJECT_NEW_GC(8, gXyzshObject, "_stackframe", FALSE);
        vector_add(gStackFrames, stackframe);
        //uobject_init(stackframe);
        SFUN(fun).mLocalObjects = stackframe;

        sObject* nextin2 = FD_NEW_STACK();

        const LineInfoW *lf = el_wline(el);
        const int size = MB_LEN_MAX * (wcslen(lf->buffer) + 1);
        char* buffer = MALLOC(size);
        wcstombs(buffer, lf->buffer, size);
        (void)fd_write(nextin2, buffer, strlen(buffer));
        FREE(buffer);

        if(!run(block, nextin2, nextout2, &rcode, gRootObject, fun)) {
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
        editline_signal();
        //xyzsh_restore_signal_default();
    }
    else {
        fprintf(stderr, "parser error\n");
        fprintf(stderr, "%s", string_c_str(gErrMsg));
    }

    const int len = strlen(SFD(nextout2).mBuf) + 1;
    wchar_t* wcs = MALLOC(sizeof(wchar_t)*len);
    mbstowcs(wcs, SFD(nextout2).mBuf, len);

    el_winsertstr(el, wcs);
    res = CC_REFRESH;
    FREE(wcs);
    stack_end_stack();

    return res;
}

static sObject* gUserCompleteBlock = NULL;

static el_action_t castamized_user_complete(EditLine *el, int ch)
{
    el_action_t res = 0;

    /// get user completion ///
    sObject* ucompletion = gUserCompleteBlock;

    sObject* el_obj = uobject_item(gRootObject, "el");
    sObject* var;
    if(el_obj && STYPE(el_obj) == T_UOBJECT) {
        uobject_put(el_obj, "omit_head_of_completion_display_matches", STRING_NEW_GC("0", TRUE));
        uobject_put(el_obj, "non_append_space_characters", STRING_NEW_GC("", TRUE));
    }

    const LineInfoW *lf = el_wline(el);
    wchar_t* line = MALLOC(sizeof(wchar_t)*(lf->cursor - lf->buffer + 1));
    int i;
    for(i=0; i<lf->cursor - lf->buffer; i++) {
        line[i] = lf->buffer[i];
    }
    line[i] = 0;

    const int size = MB_LEN_MAX * (wcslen(line) + 1);
    char* buf = MALLOC(size);
    wcstombs(buf, line, size);
    FREE(line);

    sObject* cmdline = STRING_NEW_STACK("");
    string_push_back(cmdline, buf);
    FREE(buf);

//printf("cmdline (%s)\n", string_c_str(cmdline));

    gInhibitCompletion = FALSE;
    if(!castamized_user_completion(ucompletion, cmdline)) {
        //res = call_filename_completion(el, ch, cmdline);
        return res;
    }
    
    if(gInhibitCompletion) {
        res = CC_REFRESH;
    }
    else {
        char* non_append_space_characters;
        int omit_head_of_completion_display_matches;
        if(el_obj && STYPE(el_obj) == T_UOBJECT) {
            var = uobject_item(el_obj, "non_append_space_characters");
            if(var && STYPE(var) == T_STRING) {
                non_append_space_characters = string_c_str(var);
            }
            else {
                non_append_space_characters = NULL;
            }

            var = uobject_item(el_obj, "omit_head_of_completion_display_matches");
            if(var && STYPE(var) == T_STRING) {
                omit_head_of_completion_display_matches = atoi(string_c_str(var));
            }
            else {
                omit_head_of_completion_display_matches = 0;
            }
        }
        else {
            non_append_space_characters = NULL;
            omit_head_of_completion_display_matches = 0;
        }
        res = completion_core(el, ch, non_append_space_characters, omit_head_of_completion_display_matches);
    }

    return res;
}


static void transform_lf_to_semicolon(wchar_t* buf, ALLOC wchar_t** result)
{
    *result = MALLOC(sizeof(wchar_t)*wcslen(buf)+1);
    wchar_t* p = buf;
    wchar_t* p2 = *result;
    while(*p) {
        if(*p == '\n') {
            *p2++ = ';';
            p++;
        }
        else if(*p == '<' && *(p+1) == '<' && *(p+2) == '<') {
            *p2++ = *p++;
            *p2++ = *p++;
            *p2++ = *p++;

            while(*p == ' ' || *p == '\t') {
                *p2++ = *p++;
            }

            while(*p) {
                if(*p >='A' && * p <= 'Z' || *p >='a' && *p <='z' || *p == '_' || *p >= '0' && *p <= '9')
                {
                    *p2++ = *p++;
                }
                else {
                    break;
                }
            }

            break;
        }
        else {
            *p2++ = *p++;
        }
    }
    *p2 = 0;
}

#define OKCMD    -1    /* must be -1! */
public int read_getcmd(EditLine *el, el_action_t *cmdnum, Char *ch);

/* value to leave unused in line buffer */
#define	EL_LEAVE	2

public const Char * el_my_wgets(EditLine* el, int* nread, wchar_t* init_buffer, int cursor_pos)
{
    int retval;
    el_action_t cmdnum = 0;
    int num;        /* how many chars we have read at NL */
    Char ch, *cp;
    int crlf = 0;
    int nrb;
#ifdef FIONREAD
    c_macro_t *ma = &el->el_chared.c_macro;
#endif /* FIONREAD */

// --- this is hack by ab25cq -- //
// clear buffer 
    const int sz = (size_t)(gEditLine->el_line.limit - gEditLine->el_line.buffer + EL_LEAVE);
    (void) memset(gEditLine->el_line.buffer, 0, sz);
    gEditLine->el_line.lastchar = gEditLine->el_line.cursor = gEditLine->el_line.buffer;
// -- end --

    if (nread == NULL)
        nread = &nrb;
    *nread = 0;

    if (el->el_flags & NO_TTY) {
        size_t idx;

        cp = el->el_line.buffer;
        while ((num = (*el->el_read.read_char)(el, cp)) == 1) {
            /* make sure there is space for next character */
            if (cp + 1 >= el->el_line.limit) {
                idx = (size_t)(cp - el->el_line.buffer);
                if (!ch_enlargebufs(el, (size_t)2))
                    break;
                cp = &el->el_line.buffer[idx];
            }
            cp++;
            if (el->el_flags & UNBUFFERED)
                break;
            if (cp[-1] == '\r' || cp[-1] == '\n')
                break;
        }
        if (num == -1) {
            if (errno == EINTR)
                cp = el->el_line.buffer;
            el->el_errno = errno;
        }

        goto noedit;
    }


#ifdef FIONREAD
    if (el->el_tty.t_mode == EX_IO && ma->level < 0) {
        long chrs = 0;

        (void) ioctl(el->el_infd, FIONREAD, &chrs);
        if (chrs == 0) {
            if (tty_rawmode(el) < 0) {
                errno = 0;
                *nread = 0;
                return NULL;
            }
        }
    }
#endif /* FIONREAD */

    if ((el->el_flags & UNBUFFERED) == 0) {
        read_prepare(el);

// -- this is hacked by ab25cq ---
if (init_buffer) {
el_winsertstr(el, init_buffer);

const LineInfoW* li = el_wline(el);

const int len = wcslen(li->buffer);

if(cursor_pos < 0) cursor_pos += len + 1;
if(cursor_pos < 0) cursor_pos = 0;
if(cursor_pos > len) cursor_pos = len;

el->el_line.cursor = el->el_line.buffer + cursor_pos;

re_refresh(el);
}
// --- end ---
    }

    if (el->el_flags & EDIT_DISABLED) {
        size_t idx;

        if ((el->el_flags & UNBUFFERED) == 0)
            cp = el->el_line.buffer;
        else
            cp = el->el_line.lastchar;

        terminal__flush(el);

        while ((num = (*el->el_read.read_char)(el, cp)) == 1) {
            /* make sure there is space next character */
            if (cp + 1 >= el->el_line.limit) {
                idx = (size_t)(cp - el->el_line.buffer);
                if (!ch_enlargebufs(el, (size_t)2))
                    break;
                cp = &el->el_line.buffer[idx];
            }
            cp++;
            crlf = cp[-1] == '\r' || cp[-1] == '\n';
            if (el->el_flags & UNBUFFERED)
                break;
            if (crlf)
                break;
        }

        if (num == -1) {
            if (errno == EINTR)
                cp = el->el_line.buffer;
            el->el_errno = errno;
        }

        goto noedit;
    }

    for (num = OKCMD; num == OKCMD;) {    /* while still editing this
                         * line */
#ifdef DEBUG_EDIT
        read_debug(el);
#endif /* DEBUG_EDIT */
        /* if EOF or error */
        if ((num = read_getcmd(el, &cmdnum, &ch)) != OKCMD) {
            num = -1;

// --- this is hacked by ab25cq -- //
completion_view_clear(el, TRUE);
// -- end --

#ifdef DEBUG_READ
            (void) fprintf(el->el_errfile,
                "Returning from el_gets %d\n", num);
#endif /* DEBUG_READ */
            break;
        }
// --- this is hacked by ab25cq --- //
if(ch != '\t') {
gPressingTabTwice = FALSE;
completion_view_clear(el, FALSE);
}
/// ----------------------------- ///

        if (el->el_errno == EINTR) {
            el->el_line.buffer[0] = '\0';
            el->el_line.lastchar =
                el->el_line.cursor = el->el_line.buffer;
            break;
        }
        if ((unsigned int)cmdnum >= (unsigned int)el->el_map.nfunc) {    /* BUG CHECK command */
#ifdef DEBUG_EDIT
            (void) fprintf(el->el_errfile,
                "ERROR: illegal command from key 0%o\r\n", ch);
#endif /* DEBUG_EDIT */
            continue;    /* try again */
        }
        /* now do the real command */
#ifdef DEBUG_READ
        {
            el_bindings_t *b;
            for (b = el->el_map.help; b->name; b++)
                if (b->func == cmdnum)
                    break;
            if (b->name)
                (void) fprintf(el->el_errfile,
                    "Executing %s\n", b->name);
            else
                (void) fprintf(el->el_errfile,
                    "Error command = %d\n", cmdnum);
        }
#endif /* DEBUG_READ */
        /* vi redo needs these way down the levels... */
        el->el_state.thiscmd = cmdnum;
        el->el_state.thisch = ch;
        if (el->el_map.type == MAP_VI &&
            el->el_map.current == el->el_map.key &&
            el->el_chared.c_redo.pos < el->el_chared.c_redo.lim) {
            if (cmdnum == VI_DELETE_PREV_CHAR &&
                el->el_chared.c_redo.pos != el->el_chared.c_redo.buf
                && Isprint(el->el_chared.c_redo.pos[-1]))
                el->el_chared.c_redo.pos--;
            else
                *el->el_chared.c_redo.pos++ = ch;
        }
        retval = (*el->el_map.func[cmdnum]) (el, ch);
#ifdef DEBUG_READ
        (void) fprintf(el->el_errfile,
            "Returned state %d\n", retval );
#endif /* DEBUG_READ */

        /* save the last command here */
        el->el_state.lastcmd = cmdnum;

        /* use any return value */
        switch (retval) {
        case CC_CURSOR:
            re_refresh_cursor(el);
            break;

        case CC_REDISPLAY:
            re_clear_lines(el);
            re_clear_display(el);
            /* FALLTHROUGH */

        case CC_REFRESH:
            re_refresh(el);
            break;

        case CC_REFRESH_BEEP:
            re_refresh(el);
            terminal_beep(el);
            break;

        case CC_NORM:    /* normal char */
            break;

        case CC_ARGHACK:    /* Suggested by Rich Salz */
            /* <rsalz@pineapple.bbn.com> */
            continue;    /* keep going... */

        case CC_EOF:    /* end of file typed */
            if ((el->el_flags & UNBUFFERED) == 0)
                num = 0;
            else if (num == -1) {
                *el->el_line.lastchar++ = CONTROL('d');
                el->el_line.cursor = el->el_line.lastchar;
                num = 1;
            }
            break;

        case CC_NEWLINE:    /* normal end of line */
            num = (int)(el->el_line.lastchar - el->el_line.buffer);
            break;

        case CC_FATAL:    /* fatal error, reset to known state */
#ifdef DEBUG_READ
            (void) fprintf(el->el_errfile,
                "*** editor fatal ERROR ***\r\n\n");
#endif /* DEBUG_READ */
            /* put (real) cursor in a known place */
            re_clear_display(el);    /* reset the display stuff */
            ch_reset(el, 1);    /* reset the input pointers */
            re_refresh(el); /* print the prompt again */
            break;

        case CC_ERROR:
        default:    /* functions we don't know about */
#ifdef DEBUG_READ
            (void) fprintf(el->el_errfile,
                "*** editor ERROR ***\r\n\n");
#endif /* DEBUG_READ */
            terminal_beep(el);
            terminal__flush(el);
            break;
        }

/// --- this is hacked by ab25cq --- ///
// visualize checking for right paren ///
char* blink_matching_paren = getenv("XYZSH_BLINK_MATCHING_PAREN");
if(blink_matching_paren && (strcasecmp(blink_matching_paren, "on") == 0 || strcasecmp(blink_matching_paren, "true") == 0)) 
{
    if(ch == ')') {
        const int left_paren_pos_max = 128;
        Char* lparen_pos = NULL;
        Char* left_paren_pos[left_paren_pos_max];
        memset(left_paren_pos, 0, sizeof(Char)*left_paren_pos_max);
        int left_paren_count = 0;
        Char* p = el->el_line.buffer;
        while(p < el->el_line.cursor) {
            if(*p == '(') {
                if(left_paren_count < left_paren_pos_max) {
                    left_paren_pos[left_paren_count++] = p;
                }
                p++;
            }
            else if(*p == ')') {
                left_paren_count--;
                if(left_paren_count < 0) {
                    lparen_pos = NULL;
                }
                else {
                    lparen_pos = left_paren_pos[left_paren_count];
                }
                p++;

            }
            else {
                p++;
            }
        }

        /// move cursor to previus ( point ///
        if(lparen_pos) {
            const int prompt_h = el->el_prompt.p_pos.h;
            const int prompt_v = el->el_prompt.p_pos.v;

            const int len = lparen_pos - el->el_line.buffer;
            wchar_t* tmp = MALLOC(sizeof(wchar_t)* (len + 1));
            memcpy(tmp, el->el_line.buffer, sizeof(wchar_t)*len);
            tmp[len] = 0;

            const int term_len = wcswidth(tmp, wcslen(tmp));

            FREE(tmp);
            
            const int termsz = mgetmaxx();
            const int h = (term_len + prompt_h) % termsz;
            const int v = (term_len + prompt_h) / termsz;

            terminal_move_to_line(el, v);
            terminal_move_to_char(el, h);
            terminal__flush(el);
            
            /// sleep and move cursor back at previous point ///
#if HAVE_NANOSLEEP
            char* xyzsh_blink_matching_paren_milli = getenv("XYZSH_BLINK_MATCHING_PAREN_MILLI_SEC");
            if(xyzsh_blink_matching_paren_milli) {
                struct timespec req = { 0, 1000*1000*atoi(xyzsh_blink_matching_paren_milli) };

                (void)nanosleep(&req, NULL);
            }
            else {
                sleep(1);
            }
#else
            sleep(1);
#endif
            re_refresh_cursor(el);

        }
    }
}
/// ---------------------------

        el->el_state.argument = 1;
        el->el_state.doingarg = 0;
        el->el_chared.c_vcmd.action = NOP;
        if (el->el_flags & UNBUFFERED)
            break;
    }

    terminal__flush(el);        /* flush any buffered output */
    /* make sure the tty is set up correctly */
    if ((el->el_flags & UNBUFFERED) == 0) {
        read_finish(el);
        *nread = num != -1 ? num : 0;
    } else {
        *nread = (int)(el->el_line.lastchar - el->el_line.buffer);
    }
    goto done;
noedit:
    el->el_line.cursor = el->el_line.lastchar = cp;
    *cp = '\0';
    *nread = (int)(el->el_line.cursor - el->el_line.buffer);
done:
    if (*nread == 0) {
        if (num == -1) {
            *nread = -1;
            errno = el->el_errno;
        }
        return NULL;
    } else
        return el->el_line.buffer;
}

static BOOL gEditlineEditing = FALSE;

ALLOC char* editline(char* prompt, char* rprompt, char* text, int cursor_pos)
{
    if(gEditlineEditing) { // inhibit duplex activation
        return NULL;
    }
    editline_signal();

    if(prompt == NULL) prompt = " > ";

    const int len = strlen(prompt) + 1;
    wchar_t* wprompt = MALLOC(sizeof(wchar_t)*len);
    mbstowcs(wprompt, prompt, len);
    gEditlinePrompt = wprompt;

    wchar_t* wrprompt;
    if(rprompt) {
        const int len2 = strlen(rprompt) + 1;
        wrprompt = MALLOC(sizeof(wchar_t)*len2);
        mbstowcs(wrprompt, rprompt, len);
        gEditlineRPrompt = wrprompt;
        el_wset(gEditLine, EL_RPROMPT_ESC, editline_rprompt, '\1');
    }
    else {
        wrprompt = NULL;
        el_wset(gEditLine, EL_RPROMPT_ESC, NULL);
    }

    wchar_t* text2;

    if(text) {
        const int len = strlen(text) + 1;
        text2 = MALLOC(sizeof(wchar_t)*len);
        mbstowcs(text2, text, len);
    }
    else {
        text2 = NULL;
    }

    BOOL first = TRUE;
    int numc = 0;
    const wchar_t* line;
    do {
        gEditlineEditing = TRUE;

        if(vector_count(gCmdLineStack) > 0) {
            sObject* str = vector_pop_back(gCmdLineStack);

            char* cstr = string_c_str(str);

            const int len = strlen(cstr) + 1;
            wchar_t* wstr = MALLOC(sizeof(wchar_t) * len);
            mbstowcs(wstr, cstr, len);

            line = el_my_wgets(gEditLine, &numc, wstr, -1);

            FREE(wstr);
            string_delete_on_malloc(str);
        }
        else if(first) {
            line = el_my_wgets(gEditLine, &numc, text2, cursor_pos);
            first = FALSE;
        }
        else {
            line = el_my_wgets(gEditLine, &numc, L"", -1);
        }

        gEditlineEditing = FALSE;
    } while(numc == -1);

    char* result;
    if(numc == 0 && line == NULL) {
        result = NULL;
    }
    else {
        char* ignore_history = getenv("HIST_IGNORE_SPACE");
        if(ignore_history == NULL || strcmp(ignore_history, "on") != 0 || line[0] != ' ') {
            wchar_t* line2;
            transform_lf_to_semicolon((wchar_t*)line, ALLOC &line2);
            line2[wcslen(line2)-1] = 0; // chomp
            HistEventW ev;
            history_w(gHistory, &ev, H_ENTER, line2);
            FREE(line2);
        }

        const int size = MB_LEN_MAX * (wcslen(line) + 1);
        result = MALLOC(size);
        wcstombs(result, line, size);
    }

    FREE(wprompt);
    if(wrprompt) FREE(wrprompt);

    if(text2) FREE(text2);

    xyzsh_set_signal();
    return ALLOC result;
}

ALLOC char* editline_user_castamized(char* prompt, char* rprompt, char* text, int cursor_pos, sObject* block)
{
    if(gEditlineEditing) { // inhibit duplex activation
        return NULL;
    }

    gUserCompleteBlock = block;

    editline_signal();

    if(prompt == NULL) prompt = " > ";

    const int len = strlen(prompt) + 1;
    wchar_t* wprompt = MALLOC(sizeof(wchar_t)*len);
    mbstowcs(wprompt, prompt, len);
    gEditlinePrompt = wprompt;

    wchar_t* wrprompt;
    if(rprompt) {
        const int len2 = strlen(rprompt) + 1;
        wrprompt = MALLOC(sizeof(wchar_t)*len2);
        mbstowcs(wrprompt, rprompt, len);
        gEditlineRPrompt = wrprompt;
        el_wset(gEditLine, EL_RPROMPT_ESC, editline_rprompt, '\1');
    }
    else {
        wrprompt = NULL;
        el_wset(gEditLine, EL_RPROMPT_ESC, NULL);
    }

    wchar_t* text2;

    if(text) {
        const int len = strlen(text) + 1;
        text2 = MALLOC(sizeof(wchar_t)*len);
        mbstowcs(text2, text, len);
    }
    else {
        text2 = NULL;
    }

    BOOL first = TRUE;
    int numc = 0;
    const wchar_t* line;
    do {
        gEditlineEditing = TRUE;

        if(first) {
            line = el_my_wgets(gEditLine, &numc, text2, cursor_pos);
            first = FALSE;
        }
        else {
            line = el_my_wgets(gEditLine, &numc, L"", -1);
        }

        gEditlineEditing = FALSE;
    } while(numc == -1);

    char* result;
    if(numc == 0 && line == NULL) {
        result = NULL;
    }
    else {
        char* ignore_history = getenv("HIST_IGNORE_SPACE");
        if(ignore_history == NULL || strcmp(ignore_history, "on") != 0 || line[0] != ' ') {
            wchar_t* line2;
            transform_lf_to_semicolon((wchar_t*)line, ALLOC &line2);
            line2[wcslen(line2)-1] = 0; // chomp
            HistEventW ev;
            history_w(gHistory, &ev, H_ENTER, line2);
            FREE(line2);
        }

        const int size = MB_LEN_MAX * (wcslen(line) + 1);
        result = MALLOC(size);
        wcstombs(result, line, size);
    }

    FREE(wprompt);
    if(wrprompt) FREE(wrprompt);

    if(text2) FREE(text2);

    xyzsh_set_signal();
    return ALLOC result;
}

ALLOC char* editline_no_completion(char* prompt, char* rprompt, char* text, int cursor_pos)
{
    if(gEditlineEditing) { // inhibit duplex activation
        return NULL;
    }

    editline_signal();

    if(prompt == NULL) prompt = " > ";

    const int len = strlen(prompt) + 1;
    wchar_t* wprompt = MALLOC(sizeof(wchar_t)*len);
    mbstowcs(wprompt, prompt, len);
    gEditlinePrompt = wprompt;

    wchar_t* wrprompt;
    if(rprompt) {
        const int len2 = strlen(rprompt) + 1;
        wrprompt = MALLOC(sizeof(wchar_t)*len2);
        mbstowcs(wrprompt, rprompt, len);
        gEditlineRPrompt = wrprompt;
        el_wset(gEditLine, EL_RPROMPT_ESC, editline_rprompt, '\1');
    }
    else {
        wrprompt = NULL;
        el_wset(gEditLine, EL_RPROMPT_ESC, NULL);
    }

    wchar_t* text2;

    if(text) {
        const int len = strlen(text) + 1;
        text2 = MALLOC(sizeof(wchar_t)*len);
        mbstowcs(text2, text, len);
    }
    else {
        text2 = NULL;
    }

    BOOL first = TRUE;
    int numc = 0;
    const wchar_t* line;
    do {
        gEditlineEditing = TRUE;

        if(first) {
            line = el_my_wgets(gEditLine, &numc, text2, cursor_pos);
            first = FALSE;
        }
        else {
            line = el_my_wgets(gEditLine, &numc, L"", -1);
        }

        gEditlineEditing = FALSE;
    } while(numc == -1);

    char* result;
    if(numc == 0 && line == NULL) {
        result = NULL;
    }
    else {
        char* ignore_history = getenv("HIST_IGNORE_SPACE");
        if(ignore_history == NULL || strcmp(ignore_history, "on") != 0 || line[0] != ' ') {
            wchar_t* line2;
            transform_lf_to_semicolon((wchar_t*)line, ALLOC &line2);
            line2[wcslen(line2)-1] = 0; // chomp
            HistEventW ev;
            history_w(gHistory, &ev, H_ENTER, line2);
            FREE(line2);
        }

        const int size = MB_LEN_MAX * (wcslen(line) + 1);
        result = MALLOC(size);
        wcstombs(result, line, size);
    }

    FREE(wprompt);
    if(wrprompt) FREE(wrprompt);

    if(text2) FREE(text2);

    xyzsh_set_signal();
    return ALLOC result;
}

BOOL cmd_editline_read_history(sObject* nextin, sObject* nextout, sRunInfo* runinfo)
{
    if(runinfo->mArgsNumRuntime == 2) {
        char* fname = runinfo->mArgsRuntime[1];
        if(access(fname, R_OK) == 0) {
            HistEventW ev;
            history_w(gHistory, &ev, H_LOAD, fname);
            runinfo->mRCode = 0;
        }
    }

    return TRUE;
}

BOOL cmd_editline_write_history(sObject* nextin, sObject* nextout, sRunInfo* runinfo)
{
    if(runinfo->mArgsNumRuntime == 2) {
        char* fname = runinfo->mArgsRuntime[1];
        HistEventW ev;
        history_w(gHistory, &ev, H_SAVE, fname);
        runinfo->mRCode = 0;
    }

    return TRUE;
}

BOOL cmd_editline_history(sObject* nextin, sObject* nextout, sRunInfo* runinfo)
{
    HistEventW ev;
    (void)history_w(gHistory, &ev, H_LAST);
    
    do {
        const int size = MB_LEN_MAX * (wcslen(ev.str) + 1);
        char* buf = MALLOC(size);
        wcstombs(buf, (wchar_t*)ev.str, size);

        if(!fd_write(nextout, buf, strlen(buf))) {
            err_msg("signal interrupt", runinfo->mSName, runinfo->mSLine);
            runinfo->mRCode = RCODE_SIGNAL_INTERRUPT;
            FREE(buf);
            return FALSE;
        }
        if(!fd_writec(nextout, '\n')) {
            err_msg("signal interrupt", runinfo->mSName, runinfo->mSLine);
            runinfo->mRCode = RCODE_SIGNAL_INTERRUPT;
            FREE(buf);
            return FALSE;
        }
        FREE(buf);
    } while(history_w(gHistory, &ev, H_PREV) == 0);
  
    runinfo->mRCode = 0;

    return TRUE;
}

BOOL cmd_editline_clear_screen(sObject* nextin, sObject* nextout, sRunInfo* runinfo)
{
    ed_clear_screen(gEditLine, 0);
    runinfo->mRCode = 0;

    return TRUE;
}

BOOL cmd_editline_insert_text(sObject* nextin, sObject* nextout, sRunInfo* runinfo)
{
    if(gEditlineEditing && runinfo->mArgsNumRuntime == 2) {
        char* str = runinfo->mArgsRuntime[1];

        const int len = strlen(str) + 1;
        wchar_t* wstr = MALLOC(sizeof(wchar_t)*len);

        mbstowcs(wstr, str, len);

        el_winsertstr(gEditLine, wstr);

        //re_refresh(gEditLine);

        FREE(wstr);

        runinfo->mRCode = 0;
    }

    return TRUE;
}

BOOL cmd_editline_delete_text(sObject* nextin, sObject* nextout, sRunInfo* runinfo)
{
    if(gEditlineEditing) {
        if(runinfo->mArgsNumRuntime == 3) {
            int pos = atoi(runinfo->mArgsRuntime[1]);
            int pos2 = atoi(runinfo->mArgsRuntime[2]);

            const LineInfoW* li = el_wline(gEditLine);
            const int len = wcslen(li->buffer);

            if(pos < 0) pos += len + 1;
            if(pos < 0) pos = 0;
            if(pos > len) pos = len;

            if(pos2 < 0) pos2 += len + 1;
            if(pos2 < 0) pos2 = 0;
            if(pos2 > len) pos2 = len;

            if(pos < pos2) {
                int cursor_pos = gEditLine->el_line.cursor - gEditLine->el_line.buffer;

                gEditLine->el_line.cursor = gEditLine->el_line.buffer + pos2;
                el_wdeletestr(gEditLine, pos2 - pos);

                gEditLine->el_line.cursor = gEditLine->el_line.buffer + cursor_pos - (pos2 -pos);

                //re_refresh(gEditLine);

                runinfo->mRCode = 0;
            }
        }
        else if(runinfo->mArgsNumRuntime == 2) {
            int num = atoi(runinfo->mArgsRuntime[1]);

            el_wdeletestr(gEditLine, num);

            runinfo->mRCode = 0;
        }
    }

    return TRUE;
}

BOOL cmd_editline_point_move(sObject* nextin, sObject* nextout, sRunInfo* runinfo)
{
    if(runinfo->mArgsNumRuntime == 2 && gEditlineEditing) {
        int cursor_pos = atoi(runinfo->mArgsRuntime[1]);

        const LineInfoW* li = el_wline(gEditLine);
        const int len = wcslen(li->buffer);

        if(cursor_pos < 0) cursor_pos += len + 1;
        if(cursor_pos < 0) cursor_pos = 0;
        if(cursor_pos > len) cursor_pos = len;

        gEditLine->el_line.cursor = gEditLine->el_line.buffer + cursor_pos;

        //re_refresh(gEditLine);

        runinfo->mRCode = 0;
    }

    return TRUE;
}

BOOL cmd_editline_line_buffer(sObject* nextin, sObject* nextout, sRunInfo* runinfo)
{
    if(!runinfo->mFilter) {
        const LineInfoW* li = el_wline(gEditLine);
        const int size = MB_LEN_MAX * (wcslen(li->buffer) + 1);
        char* mbs = MALLOC(size);
        wcstombs(mbs, li->buffer, size);

        if(!fd_write(nextout, mbs, strlen(mbs))) {
            err_msg("signal interrupt", runinfo->mSName, runinfo->mSLine);
            runinfo->mRCode = RCODE_SIGNAL_INTERRUPT;
            FREE(mbs);
            return FALSE;
        }
        if(!fd_write(nextout, "\n", 1)) {
            err_msg("signal interrupt", runinfo->mSName, runinfo->mSLine);
            runinfo->mRCode = RCODE_SIGNAL_INTERRUPT;
            FREE(mbs);
            return FALSE;
        }

        runinfo->mRCode = 0;

        FREE(mbs);
    }

    return TRUE;
}

#define    EL_LEAVE    2

static el_action_t ed_my_newline(EditLine *el, int ch)
{
    el_action_t res = 0;

    stack_start_stack();

    sObject* block = BLOCK_NEW_STACK();

    const LineInfoW *lf = el_wline(el);

    const int size = MB_LEN_MAX * (wcslen(lf->buffer) + 1);
    char* buf = MALLOC(size);
    wcstombs(buf, lf->buffer, size);

    sObject* cmdline = STRING_NEW_STACK("");
    string_push_back(cmdline, buf);
    FREE(buf);

//printf("cmdline (%s)\n", string_c_str(cmdline));

    int sline = 1;
    sObject* current_object = gCurrentObject;
    BOOL result = parse(string_c_str(cmdline), "editline", &sline, block, &current_object);

    /// in the block? get the block
    if(!result && (SBLOCK(block).mCompletionFlags & (COMPLETION_FLAGS_BLOCK|COMPLETION_FLAGS_ENV_BLOCK))) {
        el_winsertstr(el, L"\n");
        res = CC_REFRESH;
    }
    else if(!result && (SBLOCK(block).mCompletionFlags & COMPLETION_FLAGS_HERE_DOCUMENT)) {
        el_winsertstr(el, L"\n");
        res = CC_REFRESH;
    }
    else {
        res = ed_newline(el, 0);
    }

    stack_end_stack();

    return res;
}

static el_action_t ed_commandline_stack(EditLine *el, int ch)
{
    el_action_t res = 0;

    const LineInfoW *lf = el_wline(el);
    const int size = MB_LEN_MAX * (wcslen(lf->buffer) + 1);
    char* buf = MALLOC(size);
    wcstombs(buf, lf->buffer, size);

    vector_add(gCmdLineStack, STRING_NEW_MALLOC(buf));

    FREE(buf);

    ch_reset(el, 0);
    re_refresh(gEditLine);

    return res;
}

static void editline_additional_keybind_vi()
{
    el_wset(gEditLine, EL_ADDFN, L"ed-complete", L"Complete argument", complete);
    el_wset(gEditLine, EL_BIND, L"^I", L"ed-complete", NULL);
    el_wset(gEditLine, EL_ADDFN, L"ed-cmdline-macro", L"Commandlien macro", cmdline_macro);
    el_wset(gEditLine, EL_BIND, L"^X", L"ed-cmdline-macro", NULL);

    el_wset(gEditLine, EL_ADDFN, L"ed-my-newline", L"Pressing Enter", ed_my_newline);
    el_wset(gEditLine, EL_BIND, L"^J", L"ed-my-newline", NULL);
    el_wset(gEditLine, EL_BIND, L"^M", L"ed-my-newline", NULL);
    el_wset(gEditLine, EL_BIND, L"^[q", L"ed-commandline-stack", NULL);
}

static void editline_additional_keybind_emacs()
{
    el_wset(gEditLine, EL_ADDFN, L"ed-complete", L"Complete argument", complete);
    el_wset(gEditLine, EL_BIND, L"^I", L"ed-complete", NULL);
    el_wset(gEditLine, EL_ADDFN, L"ed-cmdline-macro", L"Commandlien macro", cmdline_macro);
    el_wset(gEditLine, EL_BIND, L"^X", L"ed-cmdline-macro", NULL);

    el_wset(gEditLine, EL_ADDFN, L"ed-my-newline", L"Pressing Enter", ed_my_newline);
    el_wset(gEditLine, EL_BIND, L"^J", L"ed-my-newline", NULL);
    el_wset(gEditLine, EL_BIND, L"^M", L"ed-my-newline", NULL);
    el_wset(gEditLine, EL_ADDFN, L"ed-commandline-stack", L"Commandline stack", ed_commandline_stack);
    el_wset(gEditLine, EL_BIND, L"^[q", L"ed-commandline-stack", NULL);
//el_wset(gEditLine, EL_BIND, NULL);
}

BOOL cmd_editline_keybind(sObject* nextin, sObject* nextout, sRunInfo* runinfo)
{
    char* mode;
    
    if(mode = sRunInfo_option_with_argument(runinfo, "-mode")) {
        if(strcmp(mode, "vi") == 0) {
            el_wset(gEditLine, EL_EDITOR, L"vi");
            editline_additional_keybind_vi();
            runinfo->mRCode = 0;
        }
        else if(strcmp(mode, "emacs") == 0) {
            el_wset(gEditLine, EL_EDITOR, L"emacs");
            editline_additional_keybind_emacs();
            runinfo->mRCode = 0;
        }
    }
    else if(runinfo->mArgsNumRuntime == 1) {
        char* home = getenv("HOME");
        if(home == NULL) {
            err_msg("$HOME is NULL", runinfo->mSName, runinfo->mSLine);
            return FALSE;
        }

        char path[PATH_MAX];
        snprintf(path, PATH_MAX, "%s/.xyzsh/tmpfile", home);
        FILE* file = fopen(path, "w");
        if(file == NULL) {
            err_msg("can't ceate tmpfile", runinfo->mSName, runinfo->mSLine);
            return FALSE;
        }

        FILE* el_outfile = gEditLine->el_outfile;       // save prev el_outfile
        gEditLine->el_outfile = file;
        el_wset(gEditLine, EL_BIND, NULL, NULL, NULL);
        gEditLine->el_outfile = el_outfile;
        fclose(file);

        sObject* str = STRING_NEW_STACK("");

        int fd = open(path, O_RDONLY);

        char buf[BUFSIZ];
        while(1) {
            int size = read(fd, buf, BUFSIZ);

            if(size < 0) {
                err_msg("can't read temporaly file", runinfo->mSName, runinfo->mSLine);
                close(fd);
                return FALSE;
            }
            else if(size == 0) {
                break;
            }
            else {
                if(!fd_write(nextout, buf, size)) {
                    err_msg("signal interrupt", runinfo->mSName, runinfo->mSLine);
                    runinfo->mRCode = RCODE_SIGNAL_INTERRUPT;
                    fclose(file);
                    return FALSE;
                }
            }
        }

        close(fd);

        runinfo->mRCode = 0;
    }
    else if(runinfo->mArgsNumRuntime == 3) {
        char* key = runinfo->mArgsRuntime[1];
        char* func = runinfo->mArgsRuntime[2];

        const int len = strlen(key) + 1;
        wchar_t* wcskey = MALLOC(sizeof(wchar_t) * len);
        mbstowcs(wcskey, key, len);

        const int len2 = strlen(func) + 1;
        wchar_t* wcsfunc = MALLOC(sizeof(wchar_t) * len2);
        mbstowcs(wcsfunc, func, len2);
        
        if(el_wset(gEditLine, EL_BIND, wcskey, wcsfunc, NULL) < 0) {
            err_msg("error on bind function to a key", runinfo->mSName, runinfo->mSLine);

            FREE(wcskey);
            FREE(wcsfunc);
            return FALSE;
        }

        FREE(wcskey);
        FREE(wcsfunc);

        runinfo->mRCode = 0;
    }

    return TRUE;
}

BOOL cmd_editline_inhibit_completion(sObject* nextin, sObject* nextout, sRunInfo* runinfo)
{
    if(gEditlineEditing && runinfo->mArgsNumRuntime == 2) {
        if(atoi(runinfo->mArgsRuntime[1]) != 0) {
            gInhibitCompletion = TRUE;
        }
        runinfo->mRCode = 0;
    }

    return TRUE;
}

BOOL cmd_editline_point(sObject* nextin, sObject* nextout, sRunInfo* runinfo)
{
    if(!runinfo->mFilter) {
        const int cursor_pos = gEditLine->el_line.cursor - gEditLine->el_line.buffer;

        char buf[BUFSIZ];
        int n = snprintf(buf, BUFSIZ, "%d\n", cursor_pos);
        if(!fd_write(nextout, buf, n)) {
            sCommand* command = runinfo->mCommand;
            err_msg("signal interrupt", runinfo->mSName, runinfo->mSLine);
            runinfo->mRCode = RCODE_SIGNAL_INTERRUPT;
            return FALSE;
        }

        runinfo->mRCode = 0;
    }

    return TRUE;
}

BOOL cmd_editline_replace_line(sObject* nextin, sObject* nextout, sRunInfo* runinfo)
{
    if(gEditlineEditing && (runinfo->mArgsNumRuntime == 2 || runinfo->mArgsNumRuntime == 3)) {
        /// clear buffer ///
        const int sz = (size_t)(gEditLine->el_line.limit - gEditLine->el_line.buffer + EL_LEAVE);
        (void) memset(gEditLine->el_line.buffer, 0, sz);
        gEditLine->el_line.lastchar = gEditLine->el_line.cursor = gEditLine->el_line.buffer;

        /// insert ///
        char* str = runinfo->mArgsRuntime[1];

        const int len = strlen(str) + 1;
        wchar_t* wstr = MALLOC(sizeof(wchar_t)*len);

        mbstowcs(wstr, str, len);

        el_winsertstr(gEditLine, wstr);

        FREE(wstr);

        /// cursor position ///
        int cursor_pos;
        if(runinfo->mArgsNumRuntime == 3) {
            cursor_pos = atoi(runinfo->mArgsRuntime[2]);
        }
        else {
            cursor_pos = -1;
        }

        const LineInfoW* li = el_wline(gEditLine);
        const int len2 = wcslen(li->buffer);

        if(cursor_pos < 0) cursor_pos += len2 + 1;
        if(cursor_pos < 0) cursor_pos = 0;
        if(cursor_pos > len2) cursor_pos = len2;

        gEditLine->el_line.cursor = gEditLine->el_line.buffer + cursor_pos;

        runinfo->mRCode = 0;
    }

    return TRUE;
}

BOOL cmd_editline_forced_update_display(sObject* nextin, sObject* nextout, sRunInfo* runinfo)
{
    if(gEditlineEditing) {
        re_refresh(gEditLine);
        runinfo->mRCode = 0;
    }

    return TRUE;
}

BOOL cmd_completion(sObject* nextin, sObject* nextout, sRunInfo* runinfo)
{
    if(runinfo->mArgsNumRuntime >= 2) {
        /// input ///
        if(runinfo->mBlocksNum == 1) {
            sObject* block = runinfo->mBlocks[0];

            int i;
            for(i=1; i<runinfo->mArgsNumRuntime; i++) {
                sObject* object;
                sObject* name = STRING_NEW_STACK("");
                if(!get_object_prefix_and_name_from_argument(&object, name, runinfo->mArgsRuntime[i], gCompletionObject, runinfo->mRunningObject, runinfo)) {
                    return FALSE;
                }

                uobject_put(object, string_c_str(name), COMPLETION_NEW_GC(block, FALSE));
            }

            runinfo->mRCode = 0;
        }
        /// output ///
        else if(runinfo->mBlocksNum == 0) {
            int i;
            for(i=1; i<runinfo->mArgsNumRuntime; i++) {
                sObject* compl;
                if(!get_object_from_argument(&compl, runinfo->mArgsRuntime[i], gCompletionObject, runinfo->mRunningObject, runinfo)) {
                    return FALSE;
                }

                if(compl && STYPE(compl) == T_COMPLETION) {
                    if(sRunInfo_option(runinfo, "-source")) {
                        sObject* block = SCOMPLETION(compl).mBlock;
                        if(!fd_write(nextout, SBLOCK(block).mSource, strlen(SBLOCK(block).mSource))) 
                        {
                            err_msg("signal interrupt", runinfo->mSName, runinfo->mSLine);
                            runinfo->mRCode = RCODE_SIGNAL_INTERRUPT;
                            return FALSE;
                        }
                    }
                    else {
                        if(!run_object(compl, nextin, nextout, runinfo)) {
                            return FALSE;
                        }
                    }
                }
                else {
                    err_msg("There is no object", runinfo->mSName, runinfo->mSLine);

                    return FALSE;
                }
            }

            runinfo->mRCode = 0;
        }
    }

    return TRUE;
}

BOOL run_completion(sObject* compl, sObject* nextin, sObject* nextout, sRunInfo* runinfo) 
{
    stack_start_stack();

    sObject* nextin2 = FD_NEW_STACK();
    const LineInfoW* lf = el_wline(gEditLine);
    const int size = MB_LEN_MAX * (lf->cursor - lf->buffer + 1);
    char* buf = MALLOC(size);
    wcstombs(buf, lf->buffer, size);
    if(!fd_write(nextin2, buf, strlen(buf))) {
        stack_end_stack();
        FREE(buf);
        return FALSE;
    }
    FREE(buf);

    sObject* fun = FUN_NEW_STACK(NULL);
    sObject* stackframe = UOBJECT_NEW_GC(8, gXyzshObject, "_stackframe", FALSE);
    vector_add(gStackFrames, stackframe);
    //uobject_init(stackframe);
    SFUN(fun).mLocalObjects = stackframe;

    sObject* argv = VECTOR_NEW_GC(16, FALSE);
    if(runinfo->mArgsNum == 1) {
        vector_add(argv, STRING_NEW_GC(runinfo->mArgs[0], FALSE));
        vector_add(argv, STRING_NEW_GC("", FALSE));
    }
    else if(runinfo->mArgsNum > 1) {
        vector_add(argv, STRING_NEW_GC(runinfo->mArgs[0], FALSE));

        /// if parser uses PARSER_MAGIC_NUMBER_OPTION, convert it
        char* str = runinfo->mArgs[runinfo->mArgsNum-1];
        vector_add(argv, STRING_NEW_GC(str, FALSE));
    }
    else {
        vector_add(argv, STRING_NEW_GC("", FALSE));
        vector_add(argv, STRING_NEW_GC("", FALSE));
    }
    uobject_put(SFUN(fun).mLocalObjects, "ARGV", argv);

    xyzsh_set_signal();
    int rcode = 0;
    if(!run(SCOMPLETION(compl).mBlock, nextin2, nextout, &rcode, gRootObject, fun)) {
        if(rcode == RCODE_BREAK) {
        }
        else if(rcode & RCODE_RETURN) {
        }
        else if(rcode == RCODE_EXIT) {
        }
        else {
            err_msg_adding("run time error\n", runinfo->mSName, runinfo->mSLine);
        }

        (void)vector_pop_back(gStackFrames);
        stack_end_stack();

        return FALSE;
    }
    (void)vector_pop_back(gStackFrames);
    stack_end_stack();

    return TRUE;
}

void editline_object_init(sObject* self)
{
    sObject* editline = UOBJECT_NEW_GC(8, self, "rl", TRUE);
    uobject_init(editline);
    uobject_put(self, "rl", editline);
    uobject_put(self, "el", editline);

    uobject_put(editline, "read_history", NFUN_NEW_GC(cmd_editline_read_history, NULL, TRUE));
    uobject_put(editline, "write_history", NFUN_NEW_GC(cmd_editline_write_history, NULL, TRUE));
    uobject_put(editline, "history", NFUN_NEW_GC(cmd_editline_history, NULL, TRUE));
    uobject_put(editline, "clear_screen", NFUN_NEW_GC(cmd_editline_clear_screen, NULL, TRUE));
    uobject_put(editline, "insert_text", NFUN_NEW_GC(cmd_editline_insert_text, NULL, TRUE));
    uobject_put(editline, "delete_text", NFUN_NEW_GC(cmd_editline_delete_text, NULL, TRUE));
    uobject_put(editline, "point_move", NFUN_NEW_GC(cmd_editline_point_move, NULL, TRUE));
    uobject_put(editline, "line_buffer", NFUN_NEW_GC(cmd_editline_line_buffer, NULL, TRUE));
    uobject_put(editline, "inhibit_completion", NFUN_NEW_GC(cmd_editline_inhibit_completion, NULL, TRUE));
    uobject_put(editline, "point", NFUN_NEW_GC(cmd_editline_point, NULL, TRUE));
    uobject_put(editline, "replace_line", NFUN_NEW_GC(cmd_editline_replace_line, NULL, TRUE));
    uobject_put(editline, "forced_update_display", NFUN_NEW_GC(cmd_editline_forced_update_display, NULL, TRUE));

    sObject* nfun = NFUN_NEW_GC(cmd_editline_keybind, NULL, TRUE);
    (void)nfun_put_option_with_argument(nfun, STRDUP("-mode"));
    uobject_put(editline, "keybind", nfun);
}

static void xyzsh_editline_history_init()
{
    /// set history size ///
    int history_size;
    char* histsize_env = getenv("XYZSH_HISTSIZE");
    if(histsize_env) {
        history_size = atoi(histsize_env);
        if(history_size < 0) history_size = 1000;
        if(history_size > 50000) history_size = 50000;
        char buf[256];
        snprintf(buf, 256, "%d", history_size);
        setenv("XYZSH_HISTSIZE", buf, 1);
    }
    else {
        history_size = 1000;
        char buf[256];
        snprintf(buf, 256, "%d", history_size);
        setenv("XYZSH_HISTSIZE", buf, 1);
    }

    /// set history file name ///
    char histfile[PATH_MAX]; 
    char* histfile_env = getenv("XYZSH_HISTFILE");
    if(histfile_env == NULL) {
        char* home = getenv("HOME");

        if(home) {
            snprintf(histfile, PATH_MAX, "%s/.xyzsh/history", home);
        }
        else {
            fprintf(stderr, "HOME evironment path is NULL. exited\n");
            exit(1);
        }
    }
    else {
        xstrncpy(histfile, histfile_env, PATH_MAX);
    }

    HistEventW ev;
    history_w(gHistory, &ev, H_SETSIZE, history_size);
    history_w(gHistory, &ev, H_LOAD, histfile);
}

void xyzsh_editline_init()
{
    /// editline init ///
    gEditLine = el_init("xyzsh", stdin, stdout, stderr);

    el_wset(gEditLine, EL_EDITOR, L"emacs");
    el_wset(gEditLine, EL_SIGNAL, 1);
    el_wset(gEditLine, EL_PROMPT_ESC, editline_prompt, '\1');

    el_source(gEditLine, NULL);

    editline_additional_keybind_emacs();
    gCmdLineStack = VECTOR_NEW_MALLOC(10);

    gHistory = history_winit();
    el_wset(gEditLine, EL_HIST, history_w, gHistory);

    xyzsh_editline_history_init();
}

void xyzsh_editline_final()
{
    int i;
    for(i=0; i<vector_count(gCmdLineStack); i++) {
        string_delete_on_malloc(vector_item(gCmdLineStack, i));
    }
    vector_delete_on_malloc(gCmdLineStack);

    //if(gHistorySuggestionTail) FREE(gHistorySuggestionTail);

    /// write history ///
    char* histfile = getenv("XYZSH_HISTFILE");

    if(histfile) {
        HistEventW ev;
        history_w(gHistory, &ev, H_SAVE, histfile);
    }
    else {
        char* home = getenv("HOME");

        if(home) {
            char path[PATH_MAX];
            snprintf(path, PATH_MAX, "%s/.xyzsh/history", home);
            HistEventW ev;
            history_w(gHistory, &ev, H_SAVE, path);
        }
        else {
            fprintf(stderr, "HOME evironment path is NULL. exited\n");
            exit(1);
        }
    }

    /// history end ///
    history_wend(gHistory);

    /// Editline end ///
    el_end(gEditLine);
}
