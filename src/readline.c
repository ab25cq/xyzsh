#include "config.h"
#include "xyzsh/xyzsh.h"

#include <string.h>
#include <strings.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <libgen.h>
#include <dirent.h>
#include <readline/readline.h>
#include <readline/history.h>
#include <oniguruma.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <pwd.h>
#include <limits.h>

static sObject* gReadlineBlock;
static sObject* gCompletionArray;

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

    return strcasecmp(lfname, rfname) < 0;
}

static sObject* gUserCompletionNextout;
static sObject* gReadlineCurrentObject;

static char* message_completion(const char* text, int stat)
{
    static int index, wordlen;

    if(stat == 0) {
        sStatment* statment = SBLOCK(gReadlineBlock).mStatments + SBLOCK(gReadlineBlock).mStatmentsNum - 1;
        
        sCommand* command = statment->mCommands + statment->mCommandsNum-1;

        gCompletionArray = VECTOR_NEW_STACK(16);

        sObject* current = gReadlineCurrentObject;
        sObject* object;

        sObject* messages = STRING_NEW_STACK("");

        while(1) {
            object = uobject_item(current, command->mMessages[0]);
            if(object || current == gRootObject) break;
            current = SUOBJECT((current)).mParent;
        }

        string_put(messages, command->mMessages[0]);
        string_push_back(messages, "::");

        if(object && TYPE(object) == T_UOBJECT) {
            int i;
            for(i=1; i<command->mMessagesNum; i++) {
                object = uobject_item(object, command->mMessages[i]);
                if(object == NULL || TYPE(object) != T_UOBJECT) break;
                string_push_back(messages, command->mMessages[i]);
                string_push_back(messages, "::");
            }

            if(object && TYPE(object) == T_UOBJECT) {
                uobject_it* it = uobject_loop_begin(object);
                while(it) {
                    char* key = uobject_loop_key(it);
                    sObject* item = uobject_loop_item(it);

                    sObject* candidate = STRING_NEW_STACK(string_c_str(messages));
                    string_push_back(candidate, key);

                    if(TYPE(item) == T_UOBJECT) {
                        vector_add(gCompletionArray, string_c_str(candidate));

                        sObject* candidate2 = STRING_NEW_STACK(string_c_str(messages));
                        string_push_back(candidate2, key);
                        string_push_back(candidate2, "::");

                        vector_add(gCompletionArray, string_c_str(candidate2));
                    }
                    else {
                        vector_add(gCompletionArray, string_c_str(candidate));
                    }

                    it = uobject_loop_next(it);
                }

                vector_sort(gCompletionArray, name_sort);
            }
        }

        wordlen = strlen(text);
        index = 0;
    }

    while(index < vector_count(gCompletionArray)) {
        char* candidate = vector_item(gCompletionArray, index);
        index++;

        if(!strncmp(text, candidate, wordlen)) {
            int len = strlen(candidate);
            if(len > 2 && candidate[len-2] == ':' && candidate[len-1] == ':') {
                rl_completion_append_character = 0;
            }
            else {
                rl_completion_append_character = ' ';
            }
            return strdup(candidate);
        }
    }

    return NULL;
}

static char* all_program_completion(const char* text, int stat)
{
    static int index, wordlen;

    if(stat == 0) {
        gCompletionArray = VECTOR_NEW_STACK(16);
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
                    if(TYPE(object) == T_UOBJECT) {
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
        }
        vector_sort(gCompletionArray, name_sort);

        wordlen = strlen(text);
        index = 0;
    }

    while(index < vector_count(gCompletionArray)) {
        char* candidate = vector_item(gCompletionArray, index);
        index++;

        return strdup(candidate);
    }

    return NULL;
}

static char* program_completion(const char* text, int stat)
{
    static int index, wordlen;

    if(stat == 0) {
        gCompletionArray = VECTOR_NEW_STACK(16);
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
                    if(TYPE(object) == T_UOBJECT) {
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
        }
        vector_sort(gCompletionArray, name_sort);

        wordlen = strlen(text);
        index = 0;
    }

    while(index < vector_count(gCompletionArray)) {
        char* candidate = vector_item(gCompletionArray, index);
        index++;

        if(!strncmp(text, candidate, wordlen)) {
            int len = strlen(candidate);
            if(len > 2 && candidate[len-2] == ':' && candidate[len-1] == ':') {
                rl_completion_append_character = 0;
            }
            else {
                rl_completion_append_character = ' ';
            }
            return strdup(candidate);
        }
    }

    return NULL;
}

static char* user_completion(const char* text, int stat)
{
    static int index, wordlen;
    static char text2[1024];

    if(stat == 0) {
        gCompletionArray = VECTOR_NEW_STACK(16);

        int i;
        for(i=0; i<vector_count(SFD(gUserCompletionNextout).mLines); i++) {
            sObject* candidate = STRING_NEW_STACK(vector_item(SFD(gUserCompletionNextout).mLines, i));
            string_chomp(candidate);
            vector_add(gCompletionArray, string_c_str(candidate));
        }

        vector_sort(gCompletionArray, name_sort);

        wordlen = strlen(text);
        index = 0;
    }

    while(index < vector_count(gCompletionArray)) {
        char* candidate = vector_item(gCompletionArray, index);
        index++;

        if(!strncmp(text, candidate, wordlen)) {
            rl_completion_append_character = ' ';
            return strdup(candidate);
        }
    }

    return NULL;
}

static char* redirect_completion(const char* text, int stat)
{
    static int index, wordlen;

    if(stat == 0) {
        gCompletionArray = VECTOR_NEW_STACK(16);

        vector_add(gCompletionArray, STRING_NEW_STACK("%>"));
        vector_add(gCompletionArray, STRING_NEW_STACK("%>>"));

        vector_sort(gCompletionArray, name_sort);

        wordlen = strlen(text);
        index = 0;
    }

    while(index < vector_count(gCompletionArray)) {
        char* candidate = string_c_str((sObject*)vector_item(gCompletionArray, index));
        index++;

        if(!strncmp(text, candidate, wordlen)) {
            rl_completion_append_character = 0;
            return strdup(candidate);
        }
    }

    return NULL;
}

#if !HAVE_DECL_ENVIRON
            extern char **environ;
#endif

static char* env_completion(const char* text, int stat_)
{
    static int index, wordlen;

    if(stat_ == 0) {
        gCompletionArray = VECTOR_NEW_STACK(16);
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

            sObject* string = STRING_NEW_STACK(env_name);
            vector_add(gCompletionArray, string_c_str(string));
        }

        sObject* current = gReadlineCurrentObject;
        while(1) {
            uobject_it* it = uobject_loop_begin(current);
            while(it) {
                char* key = uobject_loop_key(it);
                sObject* object = uobject_loop_item(it);

                if(hash_item(hash, key) == NULL) {
                    if(TYPE(object) == T_STRING || TYPE(object) == T_VECTOR || TYPE(object) == T_HASH) {
                        hash_put(hash, key, object);

                        sObject* candidate = STRING_NEW_STACK("");
                        string_push_back(candidate, uobject_loop_key(it));

                        vector_add(gCompletionArray, string_c_str(candidate));
                    }
                }

                it = uobject_loop_next(it);
            }
            
            if(current == gRootObject) break;

            current = SUOBJECT(current).mParent;
        }

        vector_sort(gCompletionArray, name_sort);

        wordlen = strlen(text);
        index = 0;
    }

    while(index < vector_count(gCompletionArray)) {
        char* candidate = vector_item(gCompletionArray, index);
        index++;

        if(!strncmp(text, candidate, wordlen)) {
            int len = strlen(candidate);
            if(len > 2 && candidate[len-2] == ':' && candidate[len-1] == ':') {
                rl_completion_append_character = 0;
            }
            else {
                rl_completion_append_character = ' ';
            }
            return strdup(candidate);
        }
    }

    return NULL;
}

static void get_current_completion_object(sObject** completion_object, sObject** current_object)
{
    if(*current_object != gRootObject) {
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
    }
}


char** readline_on_complete(const char* text, int start, int end)
{
    stack_start_stack();

    gReadlineBlock = BLOCK_NEW_STACK();

    sObject* cmdline = STRING_NEW_STACK("");
    string_push_back3(cmdline, rl_line_buffer, end);

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

                    int num = SBLOCK(gReadlineBlock).mCompletionFlags & 0xFF;

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

    if(SBLOCK(gReadlineBlock).mCompletionFlags & COMPLETION_FLAGS_TILDA) {
        char** result = rl_completion_matches(text, rl_username_completion_function);
        stack_end_stack();
        return result;
    }
    else if(SBLOCK(gReadlineBlock).mStatmentsNum == 0) {
        char** result = rl_completion_matches(text, all_program_completion);
        stack_end_stack();
        return result;
    }
    else {
        sStatment* statment = SBLOCK(gReadlineBlock).mStatments + SBLOCK(gReadlineBlock).mStatmentsNum - 1;

        if(statment->mCommandsNum == 0 || SBLOCK(gReadlineBlock).mCompletionFlags & (COMPLETION_FLAGS_COMMAND_END|COMPLETION_FLAGS_STATMENT_END|COMPLETION_FLAGS_STATMENT_HEAD)) {
            char** result = rl_completion_matches(text, all_program_completion);
            stack_end_stack();
            return result;
        }
        else {
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
                        if(object && TYPE(object) == T_UOBJECT) {
                            object = uobject_item(object, command->mMessages[i]);
                        }
                        else {
                            break;
                        }
                    }

                    if(object && TYPE(object) == T_UOBJECT) {
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
            if(SBLOCK(gReadlineBlock).mCompletionFlags & COMPLETION_FLAGS_ENV) {
                char** result = rl_completion_matches(text, env_completion);
                stack_end_stack();
                return result;
            }
            else if(command->mArgsNum == 0 
                || command->mArgsNum == 1 && SBLOCK(gReadlineBlock).mCompletionFlags & COMPLETION_FLAGS_INPUTING_COMMAND_NAME)
            {
                if(command->mMessagesNum > 0) {
                    char** result = rl_completion_matches(text, message_completion);
                    stack_end_stack();
                    return result;
                }
                else {
                    char** result = rl_completion_matches(text, program_completion);
                    stack_end_stack();
                    return result;
                }
            }
            else if(ucompletion && TYPE(ucompletion) == T_COMPLETION) {
                sObject* nextin = FD_NEW_STACK();
                if(!fd_write(nextin, string_c_str(cmdline), string_length(cmdline))) {
                    stack_end_stack();
                    return NULL;
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
                    vector_add(argv, STRING_NEW_GC("", FALSE));
                }
                else if(command->mArgsNum > 1) {
                    vector_add(argv, STRING_NEW_GC(command->mArgs[0], FALSE));

                    /// if parser uses PARSER_MAGIC_NUMBER_OPTION, convert it
                    char* str = command->mArgs[command->mArgsNum-1];
                    char* new_str = MALLOC(strlen(str) + 1);
                    xstrncpy(new_str, str, strlen(str) + 1);
                    if(new_str[0] == PARSER_MAGIC_NUMBER_OPTION) {
                        new_str[0] = '-';
                    }
                    vector_add(argv, STRING_NEW_GC(new_str, FALSE));
                    FREE(new_str);
                }
                else {
                    vector_add(argv, STRING_NEW_GC("", FALSE));
                    vector_add(argv, STRING_NEW_GC("", FALSE));
                }
                uobject_put(SFUN(fun).mLocalObjects, "ARGV", argv);

                int rcode = 0;
                xyzsh_set_signal();
                if(!run(SCOMPLETION(ucompletion).mBlock, nextin, nextout, &rcode, gReadlineCurrentObject, fun)) {
                    readline_signal();
                    fprintf(stderr, "\nrun time error\n");
                    fprintf(stderr, "%s", string_c_str(gErrMsg));
                    (void)vector_pop_back(gStackFrames);
                    stack_end_stack();
                    return NULL;
                }
                (void)vector_pop_back(gStackFrames);
                readline_signal();

                eLineField lf;
                if(fd_guess_lf(nextout, &lf)) {
                    fd_split(nextout, lf);
                } else {
                    fd_split(nextout, kLF);
                }

                gUserCompletionNextout = nextout;
                char** result = rl_completion_matches(text, user_completion);
                stack_end_stack();
                return result;
            }
        }
    }

    stack_end_stack();
    return NULL;
}

BOOL cmd_completion(sObject* nextin, sObject* nextout, sRunInfo* runinfo)
{
    sCommand* command = runinfo->mCommand;

    if(command->mArgsNumRuntime >= 2) {
        if(command->mBlocksNum == 1) {
            sObject* block = command->mBlocks[0];

            int i;
            for(i=1; i<command->mArgsNumRuntime; i++) {
                sObject* object = gCompletionObject;

                sObject* str = STRING_NEW_GC("", FALSE);
                char* p = command->mArgsRuntime[i];
                while(*p) {
                    if(*p == ':' && *(p+1) == ':') {
                        if(string_c_str(str)[0] == 0) {
                            err_msg("invalid object name", runinfo->mSName, runinfo->mSLine, command->mArgs[0]);
                            return FALSE;
                        }
                        p+=2;

                        sObject* object2 = uobject_item(object, string_c_str(str));
                        if(object2 == NULL || TYPE(object2) !=T_UOBJECT) {
                            sObject* new_object = UOBJECT_NEW_GC(8, object, string_c_str(str), TRUE);
                            uobject_put(object, string_c_str(str), new_object);
                            uobject_init(new_object);

                            object = new_object;
                        }
                        else {
                            object = object2;
                        }
                        string_put(str, "");
                    }
                    else {
                        string_push_back2(str, *p);
                        p++;
                    }
                }
                if(object && TYPE(object) == T_UOBJECT && string_c_str(str)[0] != 0) {
                    uobject_put(object, string_c_str(str), COMPLETION_NEW_GC(block, FALSE));
                }
                else {
                    err_msg("There is no object", runinfo->mSName, runinfo->mSLine, command->mArgsRuntime[i]);

                    return FALSE;
                }
            }

            runinfo->mRCode = 0;
        }
        else if(command->mBlocksNum == 0) {
            int i;
            for(i=1; i<command->mArgsNumRuntime; i++) {
                sObject* object = gCompletionObject;

                sObject* str = STRING_NEW_GC("", FALSE);
                char* p = command->mArgsRuntime[i];
                while(*p) {
                    if(*p == ':' && *(p+1) == ':') {
                        if(string_c_str(str)[0] == 0) {
                            err_msg("invalid object name", runinfo->mSName, runinfo->mSLine, command->mArgs[0]);
                            return FALSE;
                        }
                        p+=2;

                        sObject* object2 = uobject_item(object, string_c_str(str));
                        if(object2 == NULL || TYPE(object2) !=T_UOBJECT) {
                            sObject* new_object = UOBJECT_NEW_GC(8, object, string_c_str(str), TRUE);
                            uobject_put(object, string_c_str(str), new_object);
                            uobject_init(new_object);

                            object = new_object;
                        }
                        else {
                            object = object2;
                        }
                        string_put(str, "");
                    }
                    else {
                        string_push_back2(str, *p);
                        p++;
                    }
                }
                if(object && TYPE(object) == T_UOBJECT && string_c_str(str)[0] != 0) {
                    sObject* completion = uobject_item(object, string_c_str(str));
                    if(completion) {
                        stack_start_stack();

                        sObject* fun = FUN_NEW_STACK(NULL);
                        sObject* stackframe = UOBJECT_NEW_GC(8, gXyzshObject, "_stackframe", FALSE);
                        vector_add(gStackFrames, stackframe);
                        //uobject_init(stackframe);
                        SFUN(fun).mLocalObjects = stackframe;

                        xyzsh_set_signal();
                        int rcode = 0;
                        if(!run(SCOMPLETION(completion).mBlock, nextin, nextout, &rcode, gRootObject, fun)) {
                            if(rcode == RCODE_BREAK) {
                            }
                            else if(rcode == RCODE_RETURN) {
                            }
                            else if(rcode == RCODE_EXIT) {
                            }
                            else {
                                err_msg_adding("run time error\n", runinfo->mSName, runinfo->mSLine, command->mArgs[0]);
                            }

                            (void)vector_pop_back(gStackFrames);
                            readline_signal();
                            stack_end_stack();

                            return FALSE;
                        }
                        (void)vector_pop_back(gStackFrames);
                        readline_signal();
                        stack_end_stack();
                    }
                    else {
                        err_msg("There is no object", runinfo->mSName, runinfo->mSLine, command->mArgsRuntime[i]);

                        return FALSE;
                    }
                }
                else {
                    err_msg("There is no object", runinfo->mSName, runinfo->mSLine, command->mArgsRuntime[i]);

                    return FALSE;
                }
            }

            runinfo->mRCode = 0;
        }
    }

    return TRUE;
}

BOOL cmd_readline_clear_screen(sObject* nextin, sObject* nextout, sRunInfo* runinfo)
{
    mclear_immediately();
    rl_forced_update_display();

    runinfo->mRCode = 0;

    return TRUE;
}

BOOL cmd_readline_point_move(sObject* nextin, sObject* nextout, sRunInfo* runinfo)
{
    sCommand* command = runinfo->mCommand;

    if(command->mArgsNumRuntime == 2) {
        int n = atoi(command->mArgsRuntime[1]);

        if(n < 0) n += strlen(rl_line_buffer) + 1;
        if(n < 0) n = 0;
        if(n > strlen(rl_line_buffer)) n = strlen(rl_line_buffer);

        rl_point = n;

        runinfo->mRCode = 0;
    }

    return TRUE;
}

BOOL cmd_readline_insert_text(sObject* nextin, sObject* nextout, sRunInfo* runinfo)
{
    sCommand* command = runinfo->mCommand;

    if(runinfo->mFilter) {
        (void)rl_insert_text(SFD(nextin).mBuf);
        puts("");
        rl_forced_update_display();

        runinfo->mRCode = 0;
    }

    return TRUE;
}

BOOL cmd_readline_delete_text(sObject* nextin, sObject* nextout, sRunInfo* runinfo)
{
    sCommand* command = runinfo->mCommand;

    if(command->mArgsNumRuntime == 3) {
        int n = atoi(command->mArgsRuntime[1]);
        int m = atoi(command->mArgsRuntime[2]);
        if(n < 0) n += strlen(rl_line_buffer) + 1;
        if(n < 0) n= 0;
        if(m < 0) m += strlen(rl_line_buffer) + 1;
        if(m < 0) m = 0;
        rl_point -= rl_delete_text(n, m);
        if(rl_point < 0) {
            rl_point = 0;
        }
        puts("");
        rl_forced_update_display();

        runinfo->mRCode = 0;
    }

    return TRUE;
}

BOOL cmd_readline_read_history(sObject* nextin, sObject* nextout, sRunInfo* runinfo)
{
    sCommand* command = runinfo->mCommand;

    if(command->mArgsNumRuntime == 2) {
        char* fname = command->mArgsRuntime[1];
        read_history(fname);
        runinfo->mRCode = 0;
    }

    return TRUE;
}

BOOL cmd_readline_write_history(sObject* nextin, sObject* nextout, sRunInfo* runinfo)
{
    sCommand* command = runinfo->mCommand;

    if(command->mArgsNumRuntime == 2) {
        char* fname = command->mArgsRuntime[1];
        write_history(fname);
        runinfo->mRCode = 0;
    }

    return TRUE;
}

static int readline_macro(int count, int key)
{
    stack_start_stack();

    sObject* nextout2 = FD_NEW_STACK();

    int rcode = 0;
    sObject* block = BLOCK_NEW_STACK();
    int sline = 1;
    if(parse("root::macro", "macro", &sline, block, NULL)) {
        xyzsh_set_signal();

        sObject* fun = FUN_NEW_STACK(NULL);
        sObject* stackframe = UOBJECT_NEW_GC(8, gXyzshObject, "_stackframe", FALSE);
        vector_add(gStackFrames, stackframe);
        //uobject_init(stackframe);
        SFUN(fun).mLocalObjects = stackframe;

        sObject* nextin2 = FD_NEW_STACK();

        (void)fd_write(nextin2, rl_line_buffer, rl_point);

        if(!run(block, nextin2, nextout2, &rcode, gRootObject, fun)) {
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
        readline_signal();
        //xyzsh_restore_signal_default();
    }
    else {
        fprintf(stderr, "parser error\n");
        fprintf(stderr, "%s", string_c_str(gErrMsg));
    }

    rl_insert_text(SFD(nextout2).mBuf);
    puts("");
    rl_forced_update_display();
    stack_end_stack();

    return 0;
}

static int skip_quoted(const char *s, int i, char q)
{
   while(s[i] && s[i]!=q) 
   {
      if(s[i]=='\\' && s[i+1]) i++;
      i++;
   }
   if(s[i]) i++;
   return i;
}

static int lftp_char_is_quoted(const char *string, int eindex)
{
  int i, pass_next;

  for (i = pass_next = 0; i <= eindex; i++)
    {
      if (pass_next)
        {
          pass_next = 0;
          if (i >= eindex)
            return 1;
          continue;
        }
      else if (string[i] == '"' || string[i] == '\'')
        {
          char quote = string[i];
          i = skip_quoted (string, ++i, quote);
          if (i > eindex)
            return 1;
          i--;
        }
      else if (string[i] == '\\')
        {
          pass_next = 1;
          continue;
        }
    }
  return (0);
}

char* readline_filename_completion_null_generator(const char* a, int b)
{
    return NULL;
}


enum { COMPLETE_DQUOTE,COMPLETE_SQUOTE,COMPLETE_BSQUOTE };
#define completion_quoting_style COMPLETE_BSQUOTE

static BOOL shell_cmd;

static char *
double_quote (char *string)
{
  register int c;
  char *result, *r, *s;

  result = (char *)malloc (3 + (2 * strlen (string)));
  r = result;
  *r++ = '"';

  for (s = string; s && (c = *s); s++)
    {
      switch (c)
        {
	case '$':
	case '`':
	  if(!shell_cmd)
	     goto def;
	case '"':
	case '\\':
	  *r++ = '\\';
	default: def:
	  *r++ = c;
	  break;
        }
    }

  *r++ = '"';
  *r = '\0';

  return (result);
}

static char *
single_quote (char *string)
{
  register int c;
  char *result, *r, *s;

  result = (char *)malloc (3 + (4 * strlen (string)));
  r = result;
  *r++ = '\'';

  for (s = string; s && (c = *s); s++)
    {
      *r++ = c;

      if (c == '\'')
	{
	  *r++ = '\\';	// insert escaped single quote
	  *r++ = '\'';
	  *r++ = '\'';	// start new quoted string
	}
    }

  *r++ = '\'';
  *r = '\0';

  return (result);
}

static BOOL quote_glob;
static BOOL inhibit_tilde;

static char *
backslash_quote (char *string)
{
  int c;
  char *result, *r, *s;

  result = (char*)malloc (2 * strlen (string) + 1);

  for (r = result, s = string; s && (c = *s); s++)
    {
      switch (c)
	{
 	case '(': case ')':
 	case '{': case '}':			// reserved words
 	case '^':
 	case '$': case '`':			// expansion chars
	  if(!shell_cmd)
	    goto def;
 	case '*': case '[': case '?': case ']':	//globbing chars
	  if(!shell_cmd && !quote_glob)
	    goto def;
	case ' ': case '\t': case '\n':		// IFS white space
	case '"': case '\'': case '\\':		// quoting chars
	case '|': case '&': case ';':		// shell metacharacters
	case '<': case '>': case '!':
	  *r++ = '\\';
	  *r++ = c;
	  break;
	case '~':				// tilde expansion
	  if (s == string && inhibit_tilde)
	    *r++ = '.', *r++ = '/';
	  goto def;
	case '#':				// comment char
	  if(!shell_cmd)
	    goto def;
	  if (s == string)
	    *r++ = '\\';
	default: def:
	  *r++ = c;
	  break;
	}
    }

  *r = '\0';
  return (result);
}

static char *
quote_word_break_chars (char *text)
{
  char *ret, *r, *s;
  int l;

  l = strlen (text);
  ret = (char*)malloc ((2 * l) + 1);
  for (s = text, r = ret; *s; s++)
    {
      if (*s == '\\')
	{
	  *r++ = '\\';
	  *r++ = *++s;
	  if (*s == '\0')
	    break;
	  continue;
	}
      if (strchr (rl_completer_word_break_characters, *s))
        *r++ = '\\';
      *r++ = *s;
    }
  *r = '\0';
  return ret;
}

static char *
bash_quote_filename (char *s, int rtype, char *qcp)
{
  char *rtext, *mtext, *ret;
  int rlen, cs;

  rtext = (char *)NULL;

  mtext = s;
#if 0
  if (mtext[0] == '~' && rtype == SINGLE_MATCH)
    mtext = bash_tilde_expand (s);
#endif

  cs = completion_quoting_style;
  if (*qcp == '"')
    cs = COMPLETE_DQUOTE;
  else if (*qcp == '\'')
    cs = COMPLETE_SQUOTE;
#if defined (BANG_HISTORY)
  else if (*qcp == '\0' && history_expansion && cs == COMPLETE_DQUOTE &&
	   history_expansion_inhibited == 0 && strchr (mtext, '!'))
    cs = COMPLETE_BSQUOTE;

  if (*qcp == '"' && history_expansion && cs == COMPLETE_DQUOTE &&
        history_expansion_inhibited == 0 && strchr (mtext, '!'))
    {
      cs = COMPLETE_BSQUOTE;
      *qcp = '\0';
    }
#endif

  switch (cs)
    {
    case COMPLETE_DQUOTE:
      rtext = double_quote (mtext);
      break;
    case COMPLETE_SQUOTE:
      rtext = single_quote (mtext);
      break;
    case COMPLETE_BSQUOTE:
      rtext = backslash_quote (mtext);
      break;
    }

  if (mtext != s)
    free (mtext);

  if (rtext && cs == COMPLETE_BSQUOTE)
    {
      mtext = quote_word_break_chars (rtext);
      free (rtext);
      rtext = mtext;
    }

  rlen = strlen (rtext);
  ret = (char*)malloc (rlen + 1);
  strcpy (ret, rtext);

  if (rtype == MULT_MATCH && cs != COMPLETE_BSQUOTE)
    ret[rlen - 1] = '\0';
  free (rtext);
  return ret;
}

static char *
bash_dequote_filename (const char *text, int quote_char)
{
  char *ret;
  const char *p;
  char *r;
  int l, quoted;

  l = strlen (text);
  ret = (char*)malloc (l + 1);
  for (quoted = quote_char, p = text, r = ret; p && *p; p++)
    {
      if (*p == '\\')
	{
	  *r++ = *++p;
	  if (*p == '\0')
	    break;
	  continue;
	}
      if (quoted && *p == quoted)
        {
          quoted = 0;
          continue;
        }
      if (quoted == 0 && (*p == '\'' || *p == '"'))
        {
          quoted = *p;
          continue;
        }
      *r++ = *p;
    }
  *r = '\0';
  return ret;
}

void xyzsh_readline_init(BOOL runtime_script)
{
    rl_attempted_completion_function = readline_on_complete;
//    rl_completion_entry_function = readline_filename_completion_null_generator;
    rl_completer_quote_characters = "\"'";
    rl_completer_word_break_characters = " \t\n\"'|!&;()$%<>=";
    rl_completion_append_character= ' ';
    rl_filename_quote_characters = " \t\n\"'|!&;()$%<>:";
    rl_filename_quoting_function = bash_quote_filename;
    rl_filename_dequoting_function = (rl_dequote_func_t*)bash_dequote_filename;

    rl_char_is_quoted_p = (rl_linebuf_func_t*)lftp_char_is_quoted;

/*
    rl_comrl_filename_quote_characters = " \t\n\\'\"()$&|>";
    rl_completer_quote_characters = " \t\n\\'\"()$&|>";
    rl_basic_quote_characters = " \t\n\"'|!&;()$";
*/

    rl_bind_key('x'-'a'+1, readline_macro);
}

void xyzsh_readline_final()
{
}
