#include "config.h"

#include "xyzsh.h"
#include <string.h>
#include <stdio.h>
#include <glob.h>

typedef struct {
    char* mBuf;
    int mLen;
    int mSize;
} sBuf;

static void add_char_to_buf(sBuf* buf, char c)
{
    if(buf->mSize <= buf->mLen + 1) {
        buf->mSize *= 2;
        buf->mBuf = REALLOC(buf->mBuf, buf->mSize);
    }
    (buf->mBuf)[buf->mLen] = c;
    (buf->mBuf)[buf->mLen+1] = 0;
    buf->mLen++;
}

static void add_str_to_buf(sBuf* buf, char* str, int len)
{
    if(buf->mSize <= buf->mLen + len) {
        buf->mSize = (buf->mSize + len) * 2;
        buf->mBuf = REALLOC(buf->mBuf, buf->mSize);
    }
    memcpy(buf->mBuf + buf->mLen, str, len+1);
    buf->mLen += len;
}

typedef struct {
    char** mStrings;
    int mNumber;
    int mSize;
} sStrings;

static sStrings* sStrings_new()
{
    sStrings* self = MALLOC(sizeof(sStrings));
    self->mStrings = MALLOC(sizeof(char*)*10);
    self->mNumber = 0;
    self->mSize = 10;

    return self;
}

static sStrings* sString_delete(sStrings* self)
{
    int i;
    for(i=0; i<self->mNumber; i++) {
        if(self->mStrings[i]) FREE(self->mStrings[i]);
    }
    FREE(self->mStrings);
    FREE(self);
}

static void sStrings_add_string(sStrings* self, MANAGED char* str)
{
    if(self->mNumber >= self->mSize) {
        self->mSize *= 2;
        self->mStrings = REALLOC(self->mStrings, sizeof(char*)*self->mSize);
    }

    self->mStrings[self->mNumber] = MANAGED str;
    self->mNumber++;
}

/// result: number of strings is 1 or more
static BOOL expand_parametor(sStrings* strings, char* str, sCommand* command, sObject* fun, sObject* nextin, sRunInfo* runinfo, BOOL quoted)
{
    sBuf buf;
    memset(&buf, 0, sizeof(sBuf));
    buf.mBuf = MALLOC(64);
    buf.mBuf[0] = 0;
    buf.mSize = 64;

    char* p = str;
    while(*p) {
        if(*p == PARSER_MAGIC_NUMBER_ENV) {
            p++;

            char buf2[128];
            char* p2 = buf2;
            while(*p != PARSER_MAGIC_NUMBER_ENV) {
                *p2++ = *p++;
            }
            p++;
            *p2 = 0;

            sEnv* env = command->mEnvs + atoi(buf2);

            /// variable ///
            if(env->mFlags & ENV_FLAGS_KIND_ENV) {
                sObject* object;
                (void)get_object_from_argument(&object, env->mName, runinfo->mCurrentObject, runinfo->mRunningObject, runinfo);

                if(object == NULL) {
                    char* env2 = getenv(env->mName);

                    if(env2) {
                        add_str_to_buf(&buf, env2, strlen(env2));
                    }
                    else {
                        if(*env->mInitValue != 0) {
                            sObject* new_var = STRING_NEW_GC(env->mInitValue, TRUE);
                            sObject* object = SFUN(runinfo->mRunningObject).mLocalObjects;
                            uobject_put(object, env->mName, new_var);

                            add_str_to_buf(&buf, env->mInitValue, strlen(env->mInitValue));
                        }
                        else {
                            FREE(buf.mBuf);
                            char buf[128];
                            snprintf(buf, 128, "no such as object(%s)", env->mName);
                            err_msg(buf, runinfo->mSName, runinfo->mSLine);
                            return FALSE;
                        }
                    }
                }
                else {
                    /// expand key ///
                    char* key;
                    if(env->mFlags & ENV_FLAGS_KEY_ENV) {
                        sStrings* strings = sStrings_new();
                        if(!expand_parametor(strings, env->mKey, command, fun, nextin, runinfo, env->mFlags & ENV_FLAGS_KEY_QUOTED_STRING)) {
                            FREE(buf.mBuf);
                            sString_delete(strings);
                            return FALSE;
                        }

                        key = MANAGED strings->mStrings[0];
                        strings->mStrings[0] = NULL;

                        sString_delete(strings);
                    }
                    else {
                        key = STRDUP(env->mKey);
                    }

                    switch(STYPE(object)) {
                        case T_STRING: {
                            add_str_to_buf(&buf, string_c_str(object), string_length(object));
                            }
                            break;

                        case T_VECTOR:
                            if(*key == 0) {
                                if(vector_count(object) == 1) {
                                    sObject* item = vector_item(object, 0);
                                    add_str_to_buf(&buf, string_c_str(item), string_length(item));
                                }
                                else if(vector_count(object) > 1) {
                                    if(quoted) {
                                        int j;
                                        for(j=0; j<vector_count(object); j++) 
                                        {
                                            sObject* item = vector_item(object, j);

                                            add_str_to_buf(&buf, string_c_str(item), string_length(item));
                                            if(j < vector_count(object) -1) add_str_to_buf(&buf, " ", 1);
                                        }
                                    }
                                    else {
                                        sObject* item = vector_item(object, 0);

                                        add_str_to_buf(&buf, string_c_str(item), string_length(item));
                                        sStrings_add_string(strings, MANAGED buf.mBuf);

                                        memset(&buf, 0, sizeof(sBuf));
                                        buf.mBuf = MALLOC(64);
                                        buf.mBuf[0] = 0;
                                        buf.mSize = 64;

                                        int j;
                                        for(j=1; j<vector_count(object); j++) 
                                        {
                                            sObject* item = vector_item(object, j);

                                            if(j < vector_count(object) -1) {
                                                sStrings_add_string(strings, MANAGED STRDUP(string_c_str(item)));
                                            }
                                            else {
                                                add_str_to_buf(&buf, string_c_str(item), string_length(item));
                                            }
                                        }
                                    }
                                }
                            }
                            else {
                                int index = atoi(key);
                                if(index < 0) {
                                    index += vector_count(object);
                                }

                                if(index >= 0 && index < vector_count(object)) {
                                    object = vector_item(object, index);
                                    add_str_to_buf(&buf, string_c_str(object), string_length(object));
                                }
                            }
                            break;

                        case T_HASH:
                            if(*key == 0) {
                                char buf2[128];
                                snprintf(buf2, 128, "need hash key(%s)", env->mName);
                                err_msg(buf2, runinfo->mSName, runinfo->mSLine);
                                FREE(key);
                                FREE(buf.mBuf);
                                return FALSE;
                            }
                            else {
                                object = hash_item(object, key);
                                if(object) {
                                    add_str_to_buf(&buf, string_c_str(object), string_length(object));
                                }
                            }
                            break;

                        default: {
                            char buf2[128];
                            snprintf(buf2, 128, "can't expand variable because of type(%s)", env->mName);
                            err_msg(buf2, runinfo->mSName, runinfo->mSLine);
                            FREE(key);
                            FREE(buf.mBuf);
                            return FALSE;
                            }
                    }

                    FREE(key);
                }
            }
            /// block ///
            else {
                int rcode;
                sObject* nextout2 = FD_NEW_STACK();

                if(!run(env->mBlock, nextin, nextout2, &rcode, runinfo->mCurrentObject, runinfo->mRunningObject))
                {
                    err_msg_adding("run time error", runinfo->mSName, runinfo->mSLine);
                    runinfo->mRCode = rcode;
                    FREE(buf.mBuf);
                    return FALSE;
                }
                
                if(quoted) {
                    add_str_to_buf(&buf, SFD(nextout2).mBuf, SFD(nextout2).mBufLen);
                }
                else {
                    fd_split(nextout2, env->mLineField, TRUE, FALSE, TRUE);

                    if(vector_count(SFD(nextout2).mLines) == 1) {
                        char* item = vector_item(SFD(nextout2).mLines, 0);
                        add_str_to_buf(&buf, item, strlen(item));
                    }
                    else if(vector_count(SFD(nextout2).mLines) > 1) {
                        char* item = vector_item(SFD(nextout2).mLines, 0);

                        add_str_to_buf(&buf, item, strlen(item));
                        sStrings_add_string(strings, MANAGED buf.mBuf);

                        memset(&buf, 0, sizeof(sBuf));
                        buf.mBuf = MALLOC(64);
                        buf.mBuf[0] = 0;
                        buf.mSize = 64;

                        int j;
                        for(j=1; j<vector_count(SFD(nextout2).mLines); j++) 
                        {
                            char* item = vector_item(SFD(nextout2).mLines, j);

                            if(j < vector_count(SFD(nextout2).mLines) -1) {
                                sStrings_add_string(strings, MANAGED STRDUP(item));
                            }
                            else {
                                add_str_to_buf(&buf, item, strlen(item));
                            }
                        }
                    }
                }
            }
        }
        else {
            add_char_to_buf(&buf, *p++);
        }
    }

    sStrings_add_string(strings, MANAGED buf.mBuf);

    return TRUE;
}


static BOOL expand_glob(sStrings* strings, char* str, sCommand* command, sObject* fun, sObject* nextin, sRunInfo* runinfo)
{
    glob_t result;
    int rc = glob(str, 0, NULL, &result);

    if(rc == GLOB_NOSPACE) {
        err_msg("expand_glob: out of space during glob operation", runinfo->mSName, runinfo->mSLine);
        globfree(&result);
        return FALSE;
    }
    else if(rc == GLOB_NOMATCH) {
        sStrings_add_string(strings, MANAGED STRDUP(str));
    }
    else {
        if(result.gl_pathc > 0) {
            int i;
            for(i=0; i<result.gl_pathc; i++) {
                sStrings_add_string(strings, MANAGED STRDUP(result.gl_pathv[i]));
            }
        }
        else {
            sStrings_add_string(strings, MANAGED STRDUP(str));
        }
    }

    globfree(&result);

    return TRUE;
}

BOOL sCommand_expand_env(sCommand* command, sObject* object, sObject* nextin, sRunInfo* runinfo)
{
    const BOOL external_command = object == NULL || STYPE(object) == T_EXTPROG;

    /// command name ///
    char* arg = command->mArgs[0];
    sRunInfo_add_arg(runinfo, MANAGED STRDUP(arg));

    /// arguments ///
    int i;
    for(i=1; i < command->mArgsNum; i++) {
        /// variable or variable and glob or variable and option///
        if(command->mArgsFlags[i] & XYZSH_ARGUMENT_ENV) {
            /*
            variable and globl and option are error on the parser ///
            if(command->mArgsFlags[i] & XYZSH_ARGUMENT_GLOB && command->mArgsFlags[i] & XYZSH_ARGUMENT_OPTION) {
            }
            */

            /// variable and glob ///
            if(command->mArgsFlags[i] & XYZSH_ARGUMENT_GLOB) {
                sStrings* strings = sStrings_new();
                if(!expand_parametor(strings, command->mArgs[i], command, object, nextin, runinfo, command->mArgsFlags[i] & XYZSH_ARGUMENT_QUOTED)) {
                    sString_delete(strings);
                    return FALSE;
                }

                int j;
                for(j=0; j<strings->mNumber; j++) {
                    char* arg = strings->mStrings[j];

                    sStrings* strings2 = sStrings_new();
                    if(!expand_glob(strings2, arg, command, object, nextin, runinfo)) {
                        sString_delete(strings);
                        sString_delete(strings2);
                        return FALSE;
                    }

                    int k;
                    for(k=0; k<strings2->mNumber; k++) {
                        char* arg = strings2->mStrings[k];
                        sRunInfo_add_arg(runinfo, MANAGED arg);
                        strings2->mStrings[k] = NULL;
                    }

                    sString_delete(strings2);
                }
            }
            /// variable and option ///
            else if(command->mArgsFlags[i] & XYZSH_ARGUMENT_OPTION) {
                sStrings* strings = sStrings_new();
                if(!expand_parametor(strings, command->mArgs[i], command, object, nextin, runinfo, command->mArgsFlags[i] & XYZSH_ARGUMENT_QUOTED)) {
                    sString_delete(strings);
                    return FALSE;
                }


                int j;
                for(j=0; j<strings->mNumber; j++) {
                    char* arg = strings->mStrings[j];
                    if(object && (STYPE(object) == T_NFUN && nfun_option_with_argument(object, arg) || STYPE(object) == T_CLASS && class_option_with_argument(object,arg) || STYPE(object) == T_FUN && fun_option_with_argument(object, arg)))
                    {
                        if(j + 1 >= strings->mNumber) {
                            char buf[BUFSIZ];
                            snprintf(buf, BUFSIZ, "invalid option with argument(%s)", arg);
                            err_msg(buf, runinfo->mSName, runinfo->mSLine);
                            sString_delete(strings);
                            return FALSE;
                        }

                        char* next_arg = strings->mStrings[j+1];

                        if(!sRunInfo_put_option_with_argument(runinfo, MANAGED arg, MANAGED next_arg))
                        {
                            char buf[BUFSIZ];
                            snprintf(buf, BUFSIZ, "option number max (%s)\n", arg);
                            err_msg(buf, runinfo->mSName, runinfo->mSLine);
                            sString_delete(strings);
                            return FALSE;
                        }

                        strings->mStrings[j] = NULL;
                        strings->mStrings[j+1] = NULL;
                        j++;
                    }
                    else {
                        if(external_command) {
                            sRunInfo_add_arg(runinfo, MANAGED strings->mStrings[j]);
                            strings->mStrings[j] = NULL;
                        }
                        else {
                            if(!sRunInfo_put_option(runinfo, MANAGED arg)) {
                                char buf[BUFSIZ];
                                snprintf(buf, BUFSIZ, "option number max (%s)\n", arg);
                                err_msg(buf, runinfo->mSName, runinfo->mSLine);
                                sString_delete(strings);
                                return FALSE;
                            }

                            strings->mStrings[j] = NULL;
                        }
                    }
                }

                sString_delete(strings);
            }
            /// variable ///
            else {
                sStrings* strings = sStrings_new();
                if(!expand_parametor(strings, command->mArgs[i], command, object, nextin, runinfo, command->mArgsFlags[i] & XYZSH_ARGUMENT_QUOTED)) {
                    sString_delete(strings);
                    return FALSE;
                }

                int j;
                for(j=0; j<strings->mNumber; j++) {
                    sRunInfo_add_arg(runinfo, MANAGED strings->mStrings[j]);
                    strings->mStrings[j] = NULL;
                }

                sString_delete(strings);
            }
        }
        /// option ///
        else if(command->mArgsFlags[i] & XYZSH_ARGUMENT_OPTION) {
            /*
            option with glob is error on the parser
            if(command->mArgsFlags[i] & XYZSH_ARGUMENT_GLOB) { 
                err_msg("invalid option", runinfo->mSName, runinfo->mSLine);
                return FALSE;
            }
            else {
            */
            if(external_command) {
                sRunInfo_add_arg(runinfo, MANAGED STRDUP(command->mArgs[i]));
            }
            else {
                char* arg = command->mArgs[i];

                if(object && (STYPE(object) == T_NFUN && nfun_option_with_argument(object, arg) || STYPE(object) == T_CLASS && class_option_with_argument(object,arg) || STYPE(object) == T_FUN && fun_option_with_argument(object, arg) ))
                {
                    if(i + 1 >= command->mArgsNum) {
                        char buf[BUFSIZ];
                        snprintf(buf, BUFSIZ, "invalid option with argument(%s)", arg);
                        err_msg(buf, runinfo->mSName, runinfo->mSLine);
                        return FALSE;
                    }

                    char* next_arg = command->mArgs[i+1];
                    if(command->mArgsFlags[i+1] & XYZSH_ARGUMENT_GLOB || command->mArgsFlags[i+1] & XYZSH_ARGUMENT_OPTION) {
                        char buf[BUFSIZ];
                        snprintf(buf, BUFSIZ, "invalid option with argument(%s)", arg);
                        err_msg(buf, runinfo->mSName, runinfo->mSLine);
                        return FALSE;
                    }

                    if(command->mArgsFlags[i+1] & XYZSH_ARGUMENT_ENV) {
                        sStrings* strings = sStrings_new();
                        if(!expand_parametor(strings, command->mArgs[i+1], command, object, nextin, runinfo, command->mArgsFlags[i+1] & XYZSH_ARGUMENT_QUOTED)) {
                            sString_delete(strings);
                            return FALSE;
                        }

                        if(strings->mNumber == 0) {
                            char buf[BUFSIZ];
                            snprintf(buf, BUFSIZ, "invalid option with argument(%s)", arg);
                            err_msg(buf, runinfo->mSName, runinfo->mSLine);
                            sString_delete(strings);
                            return FALSE;
                        }

                        next_arg = strings->mStrings[0];
                        if(!sRunInfo_put_option_with_argument(runinfo, MANAGED STRDUP(arg), MANAGED next_arg))
                        {
                            char buf[BUFSIZ];
                            snprintf(buf, BUFSIZ, "option number max (%s)\n", arg);
                            err_msg(buf, runinfo->mSName, runinfo->mSLine);
                            sString_delete(strings);
                            return FALSE;
                        }

                        strings->mStrings[0] = NULL;

                        sString_delete(strings);
                    }
                    else {
                        if(!sRunInfo_put_option_with_argument(runinfo, MANAGED STRDUP(arg), MANAGED STRDUP(next_arg)))
                        {
                            char buf[BUFSIZ];
                            snprintf(buf, BUFSIZ, "option number max (%s)\n", arg);
                            err_msg(buf, runinfo->mSName, runinfo->mSLine);
                            return FALSE;
                        }
                    }
                    i++;
                }
                else {
                    if(!sRunInfo_put_option(runinfo, MANAGED STRDUP(arg))) {
                        char buf[BUFSIZ];
                        snprintf(buf, BUFSIZ, "option number max (%s)\n", arg);
                        err_msg(buf, runinfo->mSName, runinfo->mSLine);
                        return FALSE;
                    }
                }
            }
        }

        /// glob ///
        else if(command->mArgsFlags[i] & XYZSH_ARGUMENT_GLOB) {
            sStrings* strings = sStrings_new();
            if(!expand_glob(strings, command->mArgs[i], command, object, nextin, runinfo)) {
                sString_delete(strings);
                return FALSE;
            }

            int j;
            for(j=0; j<strings->mNumber; j++) {
                sRunInfo_add_arg(runinfo, MANAGED strings->mStrings[j]);
                strings->mStrings[j] = NULL;
            }

            sString_delete(strings);
        }

        /// normal way to add argument or external command ///
        else {
            sRunInfo_add_arg(runinfo, MANAGED STRDUP(command->mArgs[i]));
        }
    }
    
    /// redirect ///
    if(command->mRedirectsNum > 0) { sRunInfo_init_redirect_file_name(runinfo, command->mRedirectsNum); }

    for(i=0; i<command->mRedirectsNum; i++) {
        char* fname = command->mRedirectsFileNames[i];

        if(fname) {
            /// option ///
            /*
            redirect with option is error on the parser
            if(command->mRedirects[i] & REDIRECT_OPTION) {
            }
            */

            /// env or env and glob ///
            if(command->mRedirects[i] & REDIRECT_ENV) {
                if(command->mRedirects[i] & REDIRECT_GLOB) {
                    sStrings* strings = sStrings_new();
                    if(!expand_parametor(strings, fname, command, object, nextin, runinfo, command->mRedirects[i] & REDIRECT_QUOTED)) {
                        sString_delete(strings);
                        return FALSE;
                    }

                    if(strings->mNumber == 0) {
                        err_msg("require file name by this redirect", runinfo->mSName, runinfo->mSLine);
                        sString_delete(strings);
                        return FALSE;
                    }

                    char* fname2 = strings->mStrings[0];

                    sStrings* strings2 = sStrings_new();
                    if(!expand_glob(strings2, fname2, command, object, nextin, runinfo)) {
                        sString_delete(strings);
                        sString_delete(strings2);
                        return FALSE;
                    }

                    if(strings2->mNumber == 0) {
                        err_msg("require file name by this redirect", runinfo->mSName, runinfo->mSLine);
                        sString_delete(strings);
                        sString_delete(strings2);
                        return FALSE;
                    }

                    sRunInfo_add_redirect_file_name(runinfo, i, MANAGED strings2->mStrings[0]);

                    strings2->mStrings[0] = NULL;

                    sString_delete(strings);
                    sString_delete(strings2);
                }
                else {
                    sStrings* strings = sStrings_new();
                    if(!expand_parametor(strings, fname, command, object, nextin, runinfo, command->mRedirects[i] & REDIRECT_QUOTED)) {
                        sString_delete(strings);
                        return FALSE;
                    }

                    if(strings->mNumber == 0) {
                        err_msg("require file name by this redirect", runinfo->mSName, runinfo->mSLine);
                        sString_delete(strings);
                        return FALSE;
                    }

                    char* fname2 = strings->mStrings[0];

                    sRunInfo_add_redirect_file_name(runinfo, i, MANAGED fname2);

                    strings->mStrings[0] = NULL;

                    sString_delete(strings);
                }
            }
            /// glob ///
            else if(command->mRedirects[i] & REDIRECT_GLOB) {
                sStrings* strings = sStrings_new();
                if(!expand_glob(strings, fname, command, object, nextin, runinfo)) {
                    sString_delete(strings);
                    return FALSE;
                }

                if(strings->mNumber == 0) {
                    err_msg("require file name by this redirect", runinfo->mSName, runinfo->mSLine);
                    sString_delete(strings);
                    return FALSE;
                }

                sRunInfo_add_redirect_file_name(runinfo, i, strings->mStrings[0]);

                strings->mStrings[0] = NULL;

                sString_delete(strings);
            }
            else {
                sRunInfo_add_redirect_file_name(runinfo, i, STRDUP(fname));
            }
        }
    }

    return TRUE;
}

