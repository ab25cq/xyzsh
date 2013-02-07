#include "config.h"
#include "xyzsh/xyzsh.h"

#include <term.h>
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
            if(current == NULL) break;
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

            if(current == NULL) break;
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

            if(current == NULL) break;
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
            int l = strlen(candidate);
            if((l > 2 && candidate[l-2] == ':' && candidate[l-1] == ':' )
                || (l > 1 && candidate[l-1] == '/') )
            {
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

        if(strstr((char*)text, "::")) {
            sObject* current = gReadlineCurrentObject;
            sObject* prefix = STRING_NEW_STACK("");
            sObject* name = STRING_NEW_STACK("");

            split_prefix_of_object_and_name2(&current, prefix, name, (char*)text, gReadlineCurrentObject);

            if(current && TYPE(current) == T_UOBJECT) {
                uobject_it* it = uobject_loop_begin(current);
                while(it) {
                    char* key = uobject_loop_key(it);
                    sObject* object = uobject_loop_item(it);

                    if(TYPE(object) == T_UOBJECT) {
                        sObject* candidate = STRING_NEW_STACK("");
                        string_push_back(candidate, string_c_str(prefix));
                        string_push_back(candidate, key);
                        string_push_back(candidate, "::");

                        vector_add(gCompletionArray, string_c_str(candidate));
                    }
                    else if(TYPE(object) == T_STRING || TYPE(object) == T_VECTOR || TYPE(object) == T_HASH) {
                        sObject* candidate = STRING_NEW_STACK("");
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

                    if(TYPE(object) == T_UOBJECT) {
                        sObject* candidate = STRING_NEW_STACK("");
                        string_push_back(candidate, key);
                        string_push_back(candidate, "::");

                        vector_add(gCompletionArray, string_c_str(candidate));
                    }
                    else if(TYPE(object) == T_STRING || TYPE(object) == T_VECTOR || TYPE(object) == T_HASH) {
                        sObject* candidate = STRING_NEW_STACK("");
                        string_push_back(candidate, key);

                        vector_add(gCompletionArray, string_c_str(candidate));
                    }

                    it = uobject_loop_next(it);
                }
                
                if(current == gRootObject) break;

                current = SUOBJECT(current).mParent;

                if(current == NULL) break;
            }
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

char* readline_filename_completion_null_generator(const char* a, int b)
{
    return NULL;
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

    sObject* readline = uobject_item(gRootObject, "rl");
    if(readline && TYPE(readline) == T_UOBJECT) {
        uobject_put(readline, "omit_head_of_completion_display_matches", STRING_NEW_GC("0", TRUE));
    }

    rl_completion_entry_function = readline_filename_completion_null_generator;

/*
    if(SBLOCK(gReadlineBlock).mCompletionFlags & COMPLETION_FLAGS_TILDA) {
        char** result = rl_completion_matches(text, rl_username_completion_function);
        stack_end_stack();
        return result;
    }
    else 
*/
    if(SBLOCK(gReadlineBlock).mStatmentsNum == 0) {
        char** result = rl_completion_matches(text, all_program_completion);
        stack_end_stack();
        return result;
    }
    else if(SBLOCK(gReadlineBlock).mCompletionFlags & (COMPLETION_FLAGS_AFTER_REDIRECT|COMPLETION_FLAGS_AFTER_EQUAL)) {
        rl_completion_entry_function = NULL;
        stack_end_stack();
        return NULL;
    }
    else {
        sStatment* statment = SBLOCK(gReadlineBlock).mStatments + SBLOCK(gReadlineBlock).mStatmentsNum - 1;

        if(statment->mCommandsNum == 0 || SBLOCK(gReadlineBlock).mCompletionFlags & (COMPLETION_FLAGS_COMMAND_END|COMPLETION_FLAGS_STATMENT_END|COMPLETION_FLAGS_STATMENT_HEAD)) {
            char** result = rl_completion_matches(text, all_program_completion);
            stack_end_stack();
            return result;
        }
        else {
            if(statment->mCommandsNum == 1 && statment->mCommands[0].mArgsNum == 1) {
                char* arg = statment->mCommands[0].mArgs[0];
                if(strstr(arg, "/")) {
                    rl_completion_entry_function = NULL;
                    stack_end_stack();
                    return NULL;
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
    if(runinfo->mArgsNumRuntime >= 2) {
        /// input ///
        if(runinfo->mBlocksNum == 1) {
            sObject* block = runinfo->mBlocks[0];

            int i;
            for(i=1; i<runinfo->mArgsNumRuntime; i++) {
                sObject* object = gCompletionObject;
                sObject* prefix = STRING_NEW_STACK("");
                sObject* name = STRING_NEW_STACK("");

                split_prefix_of_object_and_name(&object, prefix, name, runinfo->mArgsRuntime[i]);

                if(object && TYPE(object) == T_UOBJECT && string_c_str(name)[0] != 0) {
                    uobject_put(object, string_c_str(name), COMPLETION_NEW_GC(block, FALSE));
                }
                else {
                    err_msg("invalid variable name", runinfo->mSName, runinfo->mSLine, runinfo->mArgs[0]);
                    return FALSE;
                }
            }

            runinfo->mRCode = 0;
        }
        /// output ///
        else if(runinfo->mBlocksNum == 0) {
            if(sRunInfo_option(runinfo, "-source")) {
                int i;
                for(i=1; i<runinfo->mArgsNumRuntime; i++) {
                    sObject* compl;
                    if(!get_object_from_str(&compl, runinfo->mArgsRuntime[i], runinfo->mCurrentObject, runinfo->mRunningObject, runinfo)) {
                        return FALSE;
                    }

                    if(compl && TYPE(compl) == T_COMPLETION) {
                        sObject* block = SCOMPLETION(compl).mBlock;
                        if(!fd_write(nextout, SBLOCK(block).mSource, strlen(SBLOCK(block).mSource))) 
                        {
                            err_msg("signal interrupt", runinfo->mSName, runinfo->mSLine, runinfo->mArgs[0]);
                            runinfo->mRCode = RCODE_SIGNAL_INTERRUPT;
                            return FALSE;
                        }
                        
                    }
                    else {
                        err_msg("There is no object", runinfo->mSName, runinfo->mSLine, runinfo->mArgsRuntime[i]);

                        return FALSE;
                    }
                }

                runinfo->mRCode = 0;
            }
            else {
                int i;
                for(i=1; i<runinfo->mArgsNumRuntime; i++) {
                    sObject* compl;
                    if(!get_object_from_str(&compl, runinfo->mArgsRuntime[i], runinfo->mCurrentObject, runinfo->mRunningObject, runinfo)) {
                        return FALSE;
                    }

                    if(compl && TYPE(compl) == T_COMPLETION) {
                        if(!run_object(compl, nextin, nextout, runinfo)) {
                            return FALSE;
                        }
                    }
                    else {
                        err_msg("There is no object", runinfo->mSName, runinfo->mSLine, runinfo->mArgsRuntime[i]);

                        return FALSE;
                    }
                }

                runinfo->mRCode = 0;
            }
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

BOOL cmd_readline_line_buffer(sObject* nextin, sObject* nextout, sRunInfo* runinfo)
{
    if(!runinfo->mFilter) {
        if(!fd_write(nextout, rl_line_buffer, strlen(rl_line_buffer))) {
            err_msg("signal interrupt", runinfo->mSName, runinfo->mSLine, runinfo->mArgs[0]);
            runinfo->mRCode = RCODE_SIGNAL_INTERRUPT;
            return FALSE;
        }
        if(!fd_write(nextout, "\n", 1)) {
            err_msg("signal interrupt", runinfo->mSName, runinfo->mSLine, runinfo->mArgs[0]);
            runinfo->mRCode = RCODE_SIGNAL_INTERRUPT;
            return FALSE;
        }

        runinfo->mRCode = 0;
    }

    return TRUE;
}

BOOL cmd_readline_point(sObject* nextin, sObject* nextout, sRunInfo* runinfo)
{
    if(!runinfo->mFilter) {
        char buf[BUFSIZ];
        int n = snprintf(buf, BUFSIZ, "%d\n", rl_point);
        if(!fd_write(nextout, buf, n)) {
            sCommand* command = runinfo->mCommand;
            err_msg("signal interrupt", runinfo->mSName, runinfo->mSLine, command->mArgs[0]);
            runinfo->mRCode = RCODE_SIGNAL_INTERRUPT;
            return FALSE;
        }

        runinfo->mRCode = 0;
    }

    return TRUE;
}

BOOL cmd_readline_point_move(sObject* nextin, sObject* nextout, sRunInfo* runinfo)
{
    if(runinfo->mArgsNumRuntime == 2) {
        int n = atoi(runinfo->mArgsRuntime[1]);

        if(n < 0) n += strlen(rl_line_buffer) + 1;
        if(n < 0) n = 0;
        if(n > strlen(rl_line_buffer)) n = strlen(rl_line_buffer);

        rl_point = n;

        runinfo->mRCode = 0;
    }

    return TRUE;
}

BOOL cmd_readline_forced_update_display(sObject* nextin, sObject* nextout, sRunInfo* runinfo)
{
    puts("");
    rl_forced_update_display();
    runinfo->mRCode = 0;

    return TRUE;
}

BOOL cmd_readline_insert_text(sObject* nextin, sObject* nextout, sRunInfo* runinfo)
{
    if(runinfo->mArgsNumRuntime == 2) {
        (void)rl_insert_text(runinfo->mArgsRuntime[1]);

        runinfo->mRCode = 0;
    }

    return TRUE;
}

BOOL cmd_readline_delete_text(sObject* nextin, sObject* nextout, sRunInfo* runinfo)
{
    if(runinfo->mArgsNumRuntime == 3) {
        int n = atoi(runinfo->mArgsRuntime[1]);
        int m = atoi(runinfo->mArgsRuntime[2]);
        if(n < 0) n += strlen(rl_line_buffer) + 1;
        if(n < 0) n= 0;
        if(m < 0) m += strlen(rl_line_buffer) + 1;
        if(m < 0) m = 0;
        rl_point -= rl_delete_text(n, m);
        if(rl_point < 0) {
            rl_point = 0;
        }

        runinfo->mRCode = 0;
    }

    return TRUE;
}

BOOL cmd_readline_replace_line(sObject* nextin, sObject* nextout, sRunInfo* runinfo)
{
    if(runinfo->mArgsNumRuntime == 3) {
        (void)rl_replace_line(runinfo->mArgsRuntime[1], 0);

        int n = atoi(runinfo->mArgsRuntime[2]);

        if(n < 0) n += strlen(rl_line_buffer) + 1;
        if(n < 0) n = 0;
        if(n > strlen(rl_line_buffer)) n = strlen(rl_line_buffer);

        rl_point = n;
        runinfo->mRCode = 0;
    }

    return TRUE;
}

BOOL cmd_readline_read_history(sObject* nextin, sObject* nextout, sRunInfo* runinfo)
{
    if(runinfo->mArgsNumRuntime == 2) {
        char* fname = runinfo->mArgsRuntime[1];
        read_history(fname);
        runinfo->mRCode = 0;
    }

    return TRUE;
}

BOOL cmd_readline_write_history(sObject* nextin, sObject* nextout, sRunInfo* runinfo)
{
    if(runinfo->mArgsNumRuntime == 2) {
        char* fname = runinfo->mArgsRuntime[1];
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
   *r++ = '\\'; // insert escaped single quote
   *r++ = '\'';
   *r++ = '\''; // start new quoted string
 }
    }

  *r++ = '\'';
  *r = '\0';

  return (result);
}

static BOOL quote_glob;
//static BOOL inhibit_tilde = 0;

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
  case '{': case '}':   // reserved words
  case '^':
  case '$': case '`':   // expansion chars
  case '*': case '[': case '?': case ']': //globbing chars
 case ' ': case '\t': case '\n':  // IFS white space
 case '"': case '\'': case '\\':  // quoting chars
 case '|': case '&': case ';':  // shell metacharacters
 case '<': case '>': case '!':
 case '%':
 case '#':
   *r++ = '\\';
   *r++ = c;
   break;
 case '~':    // tilde expansion
    //*r++ = '\\';
    *r++ = c;
    break;
/*
   if (s == string) {
      goto def;

   }
*/
   break;
/*
 case '#':    // comment char
   if(!shell_cmd)
     goto def;
   if (s == string)
     *r++ = '\\';
*/
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

static char*
bash_tilde_expand(char *s)
{
//puts("bash_tilde_expand");
//sleep(1);
    char* mtext = malloc(1024);
    char* p = mtext;

    s++; // ~

    if(*s == '/') {   // ~/
        s++;

        char* home = getenv("HOME");
        while(*home) {
            *p++ = *home++;
        }
        *p++ = '/';

        while(*s) {
            *p++ = *s++;
        }
        *p = 0;
    }
    else if(*s == 0) {  // this may be never runned, paranoia
        *p++ = '~';
        *p = 0;
    }
    /// ~user/
    else {
        char* point = strstr(s, "/");
        if(point) {
            char* user_name = malloc(point -s + 1);
            memcpy(user_name, s, point -s);
            user_name[point -s] = 0;
    
            struct passwd* pwd = getpwnam(user_name);

            char* p2 = pwd->pw_dir;

            while(*p2) {
                *p++ = *p2++;
            }
            *p++ = '/';

            s = point;
            s++;
            while(*s) {
                *p++ = *s++;
            }
            *p = 0;

            free(user_name);
        }
        else {
           *p++ = '~';
           while(*s) {
            *p++ = *s++;
           }
           *p = 0;
        }
    }

    return mtext;
}

static char *
bash_quote_filename (char *s, int rtype, char *qcp)
{
//puts("bash_quote_filename");
  char *rtext, *mtext, *ret;
  int rlen, cs;

  rtext = (char *)NULL;

  mtext = s;
  if (mtext[0] == '~') // && rtype == SINGLE_MATCH)
    mtext = bash_tilde_expand (s);

  cs = completion_quoting_style;
  if (*qcp == '"')
    cs = COMPLETE_DQUOTE;
  else if (*qcp == '\'')
    cs = COMPLETE_SQUOTE;

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
          if (i >= eindex) {
            return 1;
          }
          continue;
        }
      else if (string[i] == '"' || string[i] == '\'')
        {
          char quote = string[i];
          i = skip_quoted (string, ++i, quote);
          if (i > eindex) {
            return 1;
          }
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
void completion_display_matches_hook(char** matches, int num_matches, int max_length)
{
puts("");
    int omit_head_of_string = 0;
    sObject* readline = uobject_item(gRootObject, "rl");
    if(readline && TYPE(readline) == T_UOBJECT) {
        sObject* item = uobject_item(readline, "omit_head_of_completion_display_matches");
        if(item && TYPE(item) == T_STRING) {
            omit_head_of_string = atoi(string_c_str(item));
        }
    }

    int my_max_length = 0;
    int i;
    for(i=1; i<=num_matches; i++) {
        char* p = matches[i] + omit_head_of_string;
        int length = str_termlen(kUtf8, p);
        if(my_max_length < length) {
            my_max_length = length;
        }
    }

    my_max_length++;

    const int maxx = mgetmaxx();
    const int rows = maxx / my_max_length;

    int x = 0;
    for(i=1; i<=num_matches; i++) {
        int x;
        for(x=0; x<rows; x++) {
            char* p = matches[i] + omit_head_of_string;
            printf("%s", p);

            int space_len = my_max_length - str_termlen(kUtf8, p);
            int j;
            for(j=0; j<space_len; j++) {
                printf(" ");
            }

            i++;
            if(i>num_matches) break;
        }
        puts("");
    }
    rl_forced_update_display();
}

void xyzsh_readline_init(BOOL runtime_script)
{
    rl_attempted_completion_function = readline_on_complete;
    rl_completion_entry_function = readline_filename_completion_null_generator;

    rl_completer_quote_characters = "\"'";
    rl_completer_word_break_characters = " \t\n\"'|!&;()$<>=";
    rl_completion_append_character= ' ';
    rl_filename_quote_characters = " \t\n\"'|!&;()$%<>[]~";
    rl_filename_quoting_function = bash_quote_filename;
    rl_filename_dequoting_function = (rl_dequote_func_t*)bash_dequote_filename;
    rl_char_is_quoted_p = (rl_linebuf_func_t*)lftp_char_is_quoted;

    rl_completion_display_matches_hook = completion_display_matches_hook;

    rl_bind_key('x'-'a'+1, readline_macro);
}

void xyzsh_readline_final()
{
}

void readline_object_init(sObject* self)
{
    sObject* readline = UOBJECT_NEW_GC(8, self, "rl", TRUE);
    uobject_init(readline);
    uobject_put(self, "rl", readline);

    uobject_put(readline, "forced_update_display", NFUN_NEW_GC(cmd_readline_forced_update_display, NULL, TRUE));
    uobject_put(readline, "insert_text", NFUN_NEW_GC(cmd_readline_insert_text, NULL, TRUE));
    uobject_put(readline, "delete_text", NFUN_NEW_GC(cmd_readline_delete_text, NULL, TRUE));
    uobject_put(readline, "clear_screen", NFUN_NEW_GC(cmd_readline_clear_screen, NULL, TRUE));
    uobject_put(readline, "point_move", NFUN_NEW_GC(cmd_readline_point_move, NULL, TRUE));
    uobject_put(readline, "read_history", NFUN_NEW_GC(cmd_readline_read_history, NULL, TRUE));
    uobject_put(readline, "write_history", NFUN_NEW_GC(cmd_readline_write_history, NULL, TRUE));
    uobject_put(readline, "replace_line", NFUN_NEW_GC(cmd_readline_replace_line, NULL, TRUE));
    uobject_put(readline, "point", NFUN_NEW_GC(cmd_readline_point, NULL, TRUE));
    uobject_put(readline, "line_buffer", NFUN_NEW_GC(cmd_readline_line_buffer, NULL, TRUE));
}

