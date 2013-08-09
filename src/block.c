#include "config.h"

#include "xyzsh.h"
#include <string.h>
#include <stdio.h>
#include <glob.h>

void sRunInfo_command_new(sRunInfo* runinfo)
{
    runinfo->mArgsRuntime = MALLOC(sizeof(char*)*3);
    runinfo->mArgsSizeRuntime = 3;
    runinfo->mArgsNumRuntime = 0;

    memset(runinfo->mOptions, 0, sizeof(option_hash_it)*XYZSH_OPTION_MAX);

    runinfo->mRedirectsFileNamesRuntime = NULL;
    runinfo->mRedirectsNumRuntime = 0;

    sCommand* command = runinfo->mCommand;

    runinfo->mBlocks = command->mBlocks;
    runinfo->mBlocksNum = command->mBlocksNum;

    runinfo->mArgs = command->mArgs;
    runinfo->mArgsNum = command->mArgsNum;
}

void sRunInfo_command_delete(sRunInfo* runinfo)
{
    int i;
    for(i=0; i<runinfo->mArgsNumRuntime; i++) {
        FREE(runinfo->mArgsRuntime[i]);
    }
    FREE(runinfo->mArgsRuntime);

    for(i=0; i<XYZSH_OPTION_MAX; i++) {
        if(runinfo->mOptions[i].mKey) { FREE(runinfo->mOptions[i].mKey); }
        if(runinfo->mOptions[i].mArg) { FREE(runinfo->mOptions[i].mArg); }
    }

    if(runinfo->mRedirectsFileNamesRuntime) {
        int i;
        for(i=0; i<runinfo->mRedirectsNumRuntime; i++) {
            if(runinfo->mRedirectsFileNamesRuntime[i]) FREE(runinfo->mRedirectsFileNamesRuntime[i]);
        }
        FREE(runinfo->mRedirectsFileNamesRuntime);
    }
}

static int options_hash_fun(char* key)
{
    int value = 0;
    while(*key) {
        value += *key;
        key++;
    }
    return value % XYZSH_OPTION_MAX;
}

BOOL sRunInfo_option(sRunInfo* self, char* key)
{
    int hash_value = options_hash_fun(key);
    option_hash_it* p = self->mOptions + hash_value;

    while(1) {
        if(p->mKey) {
            if(strcmp(p->mKey, key) == 0) {
                return TRUE;
            }
            else {
                p++;
                if(p == self->mOptions + hash_value) {
                    return FALSE;
                }
                else if(p == self->mOptions + XYZSH_OPTION_MAX) {
                    p = self->mOptions;
                }
            }
        }
        else {
            return FALSE;
        }
    }
}

BOOL sRunInfo_put_option(sRunInfo* self, MANAGED char* key)
{
    int hash_value = options_hash_fun(key);

    option_hash_it* p = self->mOptions + hash_value;
    while(1) {
        if(p->mKey) {
            p++;
            if(p == self->mOptions + hash_value) {
                FREE(key);
                return FALSE;
            }
            else if(p == self->mOptions + XYZSH_OPTION_MAX) {
                p = self->mOptions;
            }
        }
        else {
            p->mKey = MANAGED key;
            p->mArg = NULL;
            return TRUE;
        }
    }
}

char* sRunInfo_option_with_argument(sRunInfo* self, char* key)
{
    int hash_value = options_hash_fun(key);
    option_hash_it* p = self->mOptions + hash_value;

    while(1) {
        if(p->mKey) {
            if(strcmp(p->mKey, key) == 0) {
                return p->mArg;
            }
            else {
                p++;
                if(p == self->mOptions + hash_value) {
                    return NULL;
                }
                else if(p == self->mOptions + XYZSH_OPTION_MAX) {
                    p = self->mOptions;
                }
            }
        }
        else {
            return NULL;
        }
    }
}

BOOL sRunInfo_put_option_with_argument(sRunInfo* self, MANAGED char* key, MANAGED char* argument)
{
    int hash_value = options_hash_fun(key);

    option_hash_it* p = self->mOptions + hash_value;
    while(1) {
        if(p->mKey) {
            p++;
            if(p == self->mOptions + hash_value) {
                FREE(key);
                FREE(argument);
                return FALSE;
            }
            else if(p == self->mOptions + XYZSH_OPTION_MAX) {
                p = self->mOptions;
            }
        }
        else {
            p->mKey = MANAGED key;
            p->mArg = MANAGED argument;
            return TRUE;
        }
    }
}

void sRunInfo_add_arg(sRunInfo* self, MANAGED char* buf)
{
    if(self->mArgsNumRuntime+1 >= self->mArgsSizeRuntime) {
        const int arg_runtime_size = self->mArgsSizeRuntime;

        int new_size = self->mArgsSizeRuntime * 2;
        self->mArgsRuntime = REALLOC(self->mArgsRuntime, sizeof(char*)*new_size);
        memset(self->mArgsRuntime + self->mArgsSizeRuntime, 0, sizeof(char) * (new_size - self->mArgsSizeRuntime));
        self->mArgsSizeRuntime = new_size;
    }

    self->mArgsRuntime[self->mArgsNumRuntime++] = buf;
    self->mArgsRuntime[self->mArgsNumRuntime] = NULL;
}

void sRunInfo_init_redirect_file_name(sRunInfo* self, int redirect_number)
{
    if(self->mRedirectsFileNamesRuntime == NULL) {
        const int size = sizeof(char*)*redirect_number;
        self->mRedirectsFileNamesRuntime = MALLOC(size);
        memset(self->mRedirectsFileNamesRuntime, 0, size);
        self->mRedirectsNumRuntime = redirect_number;
    }
}

void sRunInfo_add_redirect_file_name(sRunInfo* self, int redirect_index, MANAGED char* fname)
{
    self->mRedirectsFileNamesRuntime[redirect_index] = MANAGED fname;
}

sCommand* sCommand_new(sStatment* statment)
{
    sCommand* self = statment->mCommands + statment->mCommandsNum;

    self->mArgs = MALLOC(sizeof(char*)*3);
    self->mArgsFlags = MALLOC(sizeof(int)*3);
    self->mArgsSize = 3;
    self->mArgsNum = 0;

    self->mEnvs = NULL;
    self->mEnvsSize = 0;
    self->mEnvsNum = 0;

    self->mBlocks = NULL;
    self->mBlocksSize = 0;
    self->mBlocksNum = 0;

    self->mRedirectsFileNames = NULL;
    self->mRedirects = NULL;
    self->mRedirectsSize = 0;
    self->mRedirectsNum = 0;

    self->mMessages = NULL;
    self->mMessagesNum = 0;
    self->mMessagesSize = 0;

    statment->mCommandsNum++;

    return self;
}


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

void add_str_to_buf(sBuf* buf, char* str, int len)
{
    if(buf->mSize <= buf->mLen + len) {
        buf->mSize = (buf->mSize + len) * 2;
        buf->mBuf = REALLOC(buf->mBuf, buf->mSize);
    }
    memcpy(buf->mBuf + buf->mLen, str, len+1);
    buf->mLen += len;
}

static void sCommand_copy_deeply(sCommand* dest, sCommand* src)
{
    dest->mArgs = MALLOC(sizeof(char*)*src->mArgsSize);
    dest->mArgsFlags = MALLOC(sizeof(int)*src->mArgsSize);
    dest->mArgsNum = src->mArgsNum;
    dest->mArgsSize = src->mArgsSize;

    int i;
    for(i=0; i<src->mArgsNum; i++) {
        dest->mArgs[i] = STRDUP(src->mArgs[i]);
        dest->mArgsFlags[i] = src->mArgsFlags[i];
    }
    dest->mArgs[i] = NULL;

    dest->mEnvs = MALLOC(sizeof(sEnv)*src->mEnvsSize);
    for(i=0; i<src->mEnvsNum; i++) {
        dest->mEnvs[i].mFlags = src->mEnvs[i].mFlags;

        if(src->mEnvs[i].mFlags & ENV_FLAGS_KIND_ENV) {
            dest->mEnvs[i].mName = STRDUP(src->mEnvs[i].mName);
            dest->mEnvs[i].mKey = STRDUP(src->mEnvs[i].mKey);
            dest->mEnvs[i].mInitValue = STRDUP(src->mEnvs[i].mInitValue);
        }
        else {
            dest->mEnvs[i].mBlock = block_clone_on_gc(src->mEnvs[i].mBlock, T_BLOCK, FALSE);
            dest->mEnvs[i].mLineField = src->mEnvs[i].mLineField;
        }
    }
    dest->mEnvsNum = src->mEnvsNum;
    dest->mEnvsSize = src->mEnvsSize;

    dest->mBlocks = MALLOC(sizeof(sObject*)*src->mBlocksSize);
    dest->mBlocksNum = src->mBlocksNum;
    dest->mBlocksSize = src->mBlocksSize;

    for(i=0; i<src->mBlocksNum; i++) {
        dest->mBlocks[i] = block_clone_on_gc(src->mBlocks[i], T_BLOCK, FALSE);
    }

    dest->mRedirectsFileNames = MALLOC(sizeof(char*)*src->mRedirectsSize);
    for(i=0; i<src->mRedirectsNum; i++) {
        dest->mRedirectsFileNames[i] = STRDUP(src->mRedirectsFileNames[i]);
    }
    dest->mRedirects = MALLOC(sizeof(int)*src->mRedirectsSize);
    for(i=0; i<src->mRedirectsNum; i++) {
        dest->mRedirects[i] = src->mRedirects[i];
    }

    dest->mRedirectsSize = src->mRedirectsSize;
    dest->mRedirectsNum = src->mRedirectsNum;

    dest->mMessages = MALLOC(sizeof(char*)*src->mMessagesSize);
    if(src->mMessagesNum > 0) {
        for(i=0; i<src->mMessagesNum; i++) {
            dest->mMessages[i] = STRDUP(src->mMessages[i]);
        }
        dest->mMessagesNum = src->mMessagesNum;
    }
}

static void sCommand_copy_deeply_stack(sCommand* dest, sCommand* src)
{
    dest->mArgs = MALLOC(sizeof(char*)*src->mArgsSize);
    dest->mArgsFlags = MALLOC(sizeof(int)*src->mArgsSize);
    dest->mArgsNum = src->mArgsNum;
    dest->mArgsSize = src->mArgsSize;

    int i;
    for(i=0; i<src->mArgsNum; i++) {
        dest->mArgs[i] = STRDUP(src->mArgs[i]);
        dest->mArgsFlags[i] = src->mArgsFlags[i];
    }
    dest->mArgs[i] = NULL;

    dest->mEnvs = MALLOC(sizeof(sEnv)*src->mEnvsSize);
    for(i=0; i<src->mEnvsNum; i++) {
        dest->mEnvs[i].mFlags = src->mEnvs[i].mFlags;

        if(src->mEnvs[i].mFlags & ENV_FLAGS_KIND_ENV) {
            dest->mEnvs[i].mName = STRDUP(src->mEnvs[i].mName);
            dest->mEnvs[i].mKey = STRDUP(src->mEnvs[i].mKey);
            dest->mEnvs[i].mInitValue = STRDUP(src->mEnvs[i].mInitValue);
        }
        else {
            dest->mEnvs[i].mBlock = block_clone_on_stack(src->mEnvs[i].mBlock, T_BLOCK);
        }
    }
    dest->mEnvsNum = src->mEnvsNum;
    dest->mEnvsSize = src->mEnvsSize;

    dest->mBlocks = MALLOC(sizeof(sObject*)*src->mBlocksSize);
    dest->mBlocksNum = src->mBlocksNum;
    dest->mBlocksSize = src->mBlocksSize;

    for(i=0; i<src->mBlocksNum; i++) {
        dest->mBlocks[i] = block_clone_on_stack(src->mBlocks[i], T_BLOCK);
    }

    dest->mRedirectsFileNames = MALLOC(sizeof(char*)*src->mRedirectsSize);
    for(i=0; i<src->mRedirectsNum; i++) {
        dest->mRedirectsFileNames[i] = STRDUP(src->mRedirectsFileNames[i]);
    }
    dest->mRedirects = MALLOC(sizeof(int)*src->mRedirectsSize);
    for(i=0; i<src->mRedirectsNum; i++) {
        dest->mRedirects[i] = src->mRedirects[i];
    }
    dest->mRedirectsSize = src->mRedirectsSize;
    dest->mRedirectsNum = src->mRedirectsNum;

    dest->mMessages = MALLOC(sizeof(char*)*src->mMessagesSize);
    if(src->mMessagesNum > 0) {
        for(i=0; i<src->mMessagesNum; i++) {
            dest->mMessages[i] = STRDUP(src->mMessages[i]);
        }
        dest->mMessagesNum = src->mMessagesNum;
    }
}

void sCommand_delete(sCommand* self)
{
    int i;
    for(i=0; i<self->mArgsNum; i++) {
        FREE(self->mArgs[i]);
    }
    FREE(self->mArgs);
    FREE(self->mArgsFlags);

    if(self->mEnvs) {
        for(i=0; i<self->mEnvsNum; i++) {
            if(self->mEnvs[i].mFlags & ENV_FLAGS_KIND_ENV) {
                FREE(self->mEnvs[i].mName);
                FREE(self->mEnvs[i].mKey);
                FREE(self->mEnvs[i].mInitValue);
            }
        }
        FREE(self->mEnvs);
    }

    if(self->mMessages) {
        for(i=0; i<self->mMessagesNum; i++) {
            FREE(self->mMessages[i]);
        }
        FREE(self->mMessages);
    }

    if(self->mBlocks) FREE(self->mBlocks);
    if(self->mRedirects) FREE(self->mRedirects);
    if(self->mRedirectsFileNames) {
        for(i=0; i<self->mRedirectsNum; i++) {
            FREE(self->mRedirectsFileNames[i]);
        }
        FREE(self->mRedirectsFileNames);
    }
}

BOOL sCommand_add_arg(sCommand* self, MANAGED char* buf, int env, int quoted_string, int quoted_head, int glob, int option, char* sname, int sline)
{
    if(self->mArgsNum+1 >= self->mArgsSize) {
        const int arg_size = self->mArgsSize;

        int new_size = self->mArgsSize * 2;
    
        if(new_size >= XYZSH_ARG_SIZE_MAX) {
            err_msg("argument size overflow", sname, sline);
            return FALSE;
        }

        self->mArgs = REALLOC(self->mArgs, sizeof(char*)*new_size);
        self->mArgsFlags = REALLOC(self->mArgsFlags,sizeof(int)*new_size);
        memset(self->mArgs + self->mArgsSize, 0, sizeof(char) * (new_size - self->mArgsSize));
        memset(self->mArgsFlags + self->mArgsSize, 0, sizeof(int)* (new_size - self->mArgsSize));
        self->mArgsSize = new_size;

    }

    self->mArgs[self->mArgsNum] = buf;
    self->mArgsFlags[self->mArgsNum++] = (env ? XYZSH_ARGUMENT_ENV:0) | (glob ? XYZSH_ARGUMENT_GLOB:0) | (option ? XYZSH_ARGUMENT_OPTION : 0) | (quoted_string ? XYZSH_ARGUMENT_QUOTED:0) | (quoted_head ? XYZSH_ARGUMENT_QUOTED_HEAD:0);
    self->mArgs[self->mArgsNum] = NULL;

    return TRUE;
}

BOOL sCommand_add_block(sCommand* self, sObject* block, char* sname, int sline)
{
    if(self->mBlocksNum >= self->mBlocksSize) {
        if(self->mBlocks == NULL) {
            int new_block_size = 1;
            self->mBlocks = MALLOC(sizeof(sObject*)*new_block_size);
            memset(self->mBlocks, 0, sizeof(sObject*)*new_block_size);

            self->mBlocksSize = new_block_size;

        }
        else {
            const int block_size = self->mBlocksSize;
            int new_block_size = self->mBlocksSize * 2;

            if(new_block_size >= XYZSH_BLOCK_SIZE_MAX) {
                err_msg("block size overflow", sname, sline);
                return FALSE;
            }
            self->mBlocks = REALLOC(self->mBlocks, sizeof(sObject*)*new_block_size);
            memset(self->mBlocks + self->mBlocksSize, 0, sizeof(sObject*)*(new_block_size - self->mBlocksSize));

            self->mBlocksSize = new_block_size;

        }
    }

    self->mBlocks[self->mBlocksNum++] = block;

    return TRUE;
}

BOOL sCommand_add_message(sCommand* self, MANAGED char* message, char* sname, int sline)
{
    if(self->mMessagesNum >= self->mMessagesSize) {
        if(self->mMessages == NULL) {
            int new_message_size = 2;
            self->mMessages = MALLOC(sizeof(char*)*new_message_size);
            memset(self->mMessages, 0, sizeof(char*)*new_message_size);

            self->mMessagesSize = new_message_size;
        }
        else {
            const int message_size = self->mMessagesSize;
            int new_message_size = self->mMessagesSize * 2;

            if(new_message_size >= XYZSH_MESSAGE_SIZE_MAX) {
                err_msg("message size overflow", sname, sline);
                return FALSE;
            }

            self->mMessages = REALLOC(self->mMessages, sizeof(char*)*new_message_size);
            memset(self->mMessages + self->mMessagesSize, 0, sizeof(char*)*(new_message_size - self->mMessagesSize));

            self->mMessagesSize = new_message_size;

        }
    }

    self->mMessages[self->mMessagesNum++] = message;

    return TRUE;
}

BOOL sCommand_add_env(sCommand* self, MANAGED char* name, MANAGED char* init_value, MANAGED char* key, BOOL key_env, BOOL key_quoted, char* sname, int sline)
{
    if(self->mEnvsNum >= self->mEnvsSize) {
        if(self->mEnvs == NULL) {
            self->mEnvsSize = 1;
            self->mEnvs = MALLOC(sizeof(sEnv)*self->mEnvsSize);
        }
        else {
            const int env_size = self->mEnvsSize;
            self->mEnvsSize *= 2;

            if(self->mEnvsSize >= XYZSH_ENV_SIZE_MAX) {
                err_msg("environment size overflow", sname, sline);
                return FALSE;
            }
            self->mEnvs = REALLOC(self->mEnvs, sizeof(sEnv)*self->mEnvsSize);
        }
    }

    self->mEnvs[self->mEnvsNum].mFlags = ENV_FLAGS_KIND_ENV | (key_env ? ENV_FLAGS_KEY_ENV:0) | (key_quoted ? ENV_FLAGS_KEY_QUOTED_STRING:0);
    self->mEnvs[self->mEnvsNum].mName = MANAGED name;
    self->mEnvs[self->mEnvsNum].mInitValue = MANAGED init_value;
    self->mEnvs[self->mEnvsNum].mKey = MANAGED key;
    self->mEnvsNum++;

    return TRUE;
}

BOOL sCommand_add_env_block(sCommand* self, sObject* block, eLineField lf, char* sname, int sline)
{
    if(self->mEnvsNum >= self->mEnvsSize) {
        if(self->mEnvs == NULL) {
            self->mEnvsSize = 1;
            self->mEnvs = MALLOC(sizeof(sEnv)*self->mEnvsSize);
        }
        else {
            const int env_size = self->mEnvsSize;
            self->mEnvsSize *= 2;

            if(self->mEnvsSize >= XYZSH_ENV_SIZE_MAX) {
                err_msg("environment size overflow", sname, sline);
                return FALSE;
            }
            self->mEnvs = REALLOC(self->mEnvs, sizeof(sEnv)*self->mEnvsSize);

        }
    }

    self->mEnvs[self->mEnvsNum].mFlags = ENV_FLAGS_KIND_BLOCK;
    self->mEnvs[self->mEnvsNum].mBlock = block_clone_on_stack(block, T_BLOCK);
    self->mEnvs[self->mEnvsNum].mLineField = lf;

    self->mEnvsNum++;

    return TRUE;
}

BOOL sCommand_add_redirect(sCommand* self, MANAGED char* name, BOOL env, BOOL quoted_string, BOOL glob, int redirect, char* sname, int sline)
{
    if(self->mRedirectsNum >= self->mRedirectsSize) {
        if(self->mRedirectsFileNames == NULL) {
            self->mRedirectsSize = 1;
            self->mRedirectsFileNames = MALLOC(sizeof(char*)*self->mRedirectsSize);
            self->mRedirects = MALLOC(sizeof(int)*self->mRedirectsSize);

        }
        else {
            const int redirect_size = self->mRedirectsSize;
            
            if(self->mRedirectsSize*2 >= XYZSH_REDIRECT_SIZE_MAX) {
                err_msg("redirect size overflow", sname, sline);
                return FALSE;
            }

            self->mRedirectsSize *= 2;
            self->mRedirectsFileNames = REALLOC(self->mRedirectsFileNames, sizeof(char*)*self->mRedirectsSize);
            self->mRedirects = REALLOC(self->mRedirects, sizeof(int)*self->mRedirectsSize);
        }
    }

    self->mRedirectsFileNames[self->mRedirectsNum] = MANAGED name;
    self->mRedirects[self->mRedirectsNum++] = redirect | (env ? REDIRECT_ENV:0) | (glob ? REDIRECT_GLOB: 0) | (quoted_string ? REDIRECT_QUOTED:0);
    
    return TRUE;
}

BOOL sCommand_add_command_without_command_name(sCommand* self, sCommand* command, char* sname, int sline)
{
    int i;
    for(i=1; i<command->mArgsNum; i++) {
        if(command->mArgsFlags[i] & XYZSH_ARGUMENT_ENV) {
            sBuf buf;
            memset(&buf, 0, sizeof(sBuf));
            buf.mBuf = MALLOC(64);
            buf.mBuf[0] = 0;
            buf.mSize = 64;

            char* p = command->mArgs[i];
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

                    int num = atoi(buf2) + self->mEnvsNum;
                    snprintf(buf2, 128, "%d", num);

                    add_char_to_buf(&buf, PARSER_MAGIC_NUMBER_ENV);
                    add_str_to_buf(&buf, buf2, strlen(buf2));
                    add_char_to_buf(&buf, PARSER_MAGIC_NUMBER_ENV);
                }
                else {
                    add_char_to_buf(&buf, *p++);
                }
            }
            if(!sCommand_add_arg(self, MANAGED buf.mBuf, command->mArgsFlags[i] & XYZSH_ARGUMENT_ENV, command->mArgsFlags[i] & XYZSH_ARGUMENT_QUOTED, command->mArgsFlags[i] & XYZSH_ARGUMENT_QUOTED_HEAD, command->mArgsFlags[i] & XYZSH_ARGUMENT_GLOB, command->mArgsFlags[i] & XYZSH_ARGUMENT_OPTION, sname, sline))
            {
                return FALSE;
            }
        }
        else {
            char* arg = STRDUP(command->mArgs[i]);
            if(!sCommand_add_arg(self, MANAGED arg, command->mArgsFlags[i] & XYZSH_ARGUMENT_ENV, command->mArgsFlags[i] & XYZSH_ARGUMENT_QUOTED, command->mArgsFlags[i] & XYZSH_ARGUMENT_QUOTED_HEAD, command->mArgsFlags[i] & XYZSH_ARGUMENT_GLOB, command->mArgsFlags[i] & XYZSH_ARGUMENT_OPTION, sname, sline))
            {
                return FALSE;
            }
        }
    }
    for(i=0; i<command->mEnvsNum; i++) {
        sEnv* env = command->mEnvs + i;

        if(env->mFlags & ENV_FLAGS_KIND_ENV) {
            if(!sCommand_add_env(self, MANAGED STRDUP(env->mName), MANAGED STRDUP(env->mInitValue), MANAGED STRDUP(env->mKey), env->mFlags & ENV_FLAGS_KEY_ENV, env->mFlags & ENV_FLAGS_KEY_QUOTED_STRING, sname, sline))
            {
                return FALSE;
            }
        }
        else {
            sObject* block = block_clone_on_stack(env->mBlock, T_BLOCK);
            if(!sCommand_add_env_block(self, block, env->mLineField, sname, sline)) {
                return FALSE;
            }
        }
    }
    for(i=0; i<command->mBlocksNum; i++) {
        sObject* block = block_clone_on_stack(command->mBlocks[i], T_BLOCK);
        if(!sCommand_add_block(self, block, sname, sline)) {
            return FALSE;
        }
    }
    for(i=0; i<command->mRedirectsNum; i++) {
        int flags = command->mRedirects[i];
        if(flags & REDIRECT_ENV) {
            sBuf buf;
            memset(&buf, 0, sizeof(sBuf));
            buf.mBuf = MALLOC(64);
            buf.mBuf[0] = 0;
            buf.mSize = 64;

            char* p = command->mRedirectsFileNames[i];
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

                    int num = atoi(buf2) + self->mEnvsNum;
                    snprintf(buf2, 128, "%d", num);

                    add_char_to_buf(&buf, PARSER_MAGIC_NUMBER_ENV);
                    add_str_to_buf(&buf, buf2, strlen(buf2));
                    add_char_to_buf(&buf, PARSER_MAGIC_NUMBER_ENV);
                }
                else {
                    add_char_to_buf(&buf, *p++);
                }
            }
            if(!sCommand_add_redirect(self, MANAGED buf.mBuf, flags & REDIRECT_ENV, flags & REDIRECT_QUOTED, flags & REDIRECT_GLOB, flags & REDIRECT_KIND, sname, sline)) 
            {
                return FALSE;
            }
        }
        else {
            if(!sCommand_add_redirect(self, MANAGED STRDUP(command->mRedirectsFileNames[i]), flags & REDIRECT_ENV, flags & REDIRECT_QUOTED, flags & REDIRECT_GLOB, flags & REDIRECT_KIND, sname, sline))
            {
                return FALSE;
            }
        }
    }

    return TRUE;
}

sStatment* sStatment_new(sObject* block, int sline, char* sname)
{
    if(SBLOCK(block).mStatmentsNum >= SBLOCK(block).mStatmentsSize) {
        const int statments_size = SBLOCK(block).mStatmentsSize;

        int new_statments_size = SBLOCK(block).mStatmentsSize * 2;
        SBLOCK(block).mStatments = REALLOC(SBLOCK(block).mStatments, sizeof(sStatment)*new_statments_size);
        memset(SBLOCK(block).mStatments + SBLOCK(block).mStatmentsSize, 0, sizeof(sStatment)*(new_statments_size - SBLOCK(block).mStatmentsSize));
        SBLOCK(block).mStatmentsSize = new_statments_size;
    }

    sStatment* self = SBLOCK(block).mStatments + SBLOCK(block).mStatmentsNum;
    SBLOCK(block).mStatmentsNum++;
    self->mLine = sline;
    self->mFName = STRDUP(sname);
    self->mFlags = 0;

    return self;
}

static void sStatment_copy_deeply(sStatment* dest, sStatment* source)
{
    if(source->mFlags & STATMENT_FLAGS_KIND_NODETREE) {
        dest->mNodeTreeNum = source->mNodeTreeNum;

        int i;
        for(i=0; i<source->mNodeTreeNum; i++) {
            dest->mNodeTree[i] = ALLOC sNodeTree_clone(source->mNodeTree[i]);
        }
    }
    else {
        dest->mCommandsNum = source->mCommandsNum;

        int i;
        for(i=0; i<source->mCommandsNum; i++) {
            sCommand_copy_deeply(dest->mCommands + i, source->mCommands + i);
        }
    }

    if(source->mFName) {
        dest->mFName = STRDUP(source->mFName);
    }
    else {
        dest->mFName = NULL;
    }
    dest->mLine = source->mLine;

    dest->mFlags = source->mFlags;
}

static void sStatment_copy_deeply_stack(sStatment* dest, sStatment* source)
{
    if(source->mFlags & STATMENT_FLAGS_KIND_NODETREE) {
        dest->mNodeTreeNum = source->mNodeTreeNum;

        int i;
        for(i=0; i<source->mNodeTreeNum; i++) {
            dest->mNodeTree[i] = ALLOC sNodeTree_clone(source->mNodeTree[i]);
        }
    }
    else {
        dest->mCommandsNum = source->mCommandsNum;

        int i;
        for(i=0; i<source->mCommandsNum; i++) {
            sCommand_copy_deeply_stack(dest->mCommands + i, source->mCommands + i);
        }
    }

    if(source->mFName) {
        dest->mFName = STRDUP(source->mFName);
    }
    else {
        dest->mFName = NULL;
    }
    dest->mLine = source->mLine;

    dest->mFlags = source->mFlags;
}

void sStatment_delete(sStatment* self)
{
    if(self->mFlags & STATMENT_FLAGS_KIND_NODETREE) {
        int i;
        for(i=0; i<self->mNodeTreeNum; i++) {
            sNodeTree_free(self->mNodeTree[i]);
        }
    }
    else {
        int i;
        for(i=0; i<self->mCommandsNum; i++) {
            sCommand_delete(self->mCommands + i);
        }
    }
    if(self->mFName) FREE(self->mFName);
}

sObject* block_new_on_gc(BOOL user_object)
{
   sObject* self = gc_get_free_object(T_BLOCK, user_object);
   
   SBLOCK(self).mStatments = MALLOC(sizeof(sStatment)*1);
   memset(SBLOCK(self).mStatments, 0, sizeof(sStatment)*1);
   SBLOCK(self).mStatmentsNum = 0;
   SBLOCK(self).mStatmentsSize = 1;
   SBLOCK(self).mSource = NULL;

   return self;
}

sObject* block_clone_on_gc(sObject* source, int type, BOOL user_object)
{
    sObject* self = gc_get_free_object(type, user_object);

    const int statments_size = SBLOCK(source).mStatmentsSize;
    
    SBLOCK(self).mStatments = MALLOC(sizeof(sStatment)*statments_size);
    memset(SBLOCK(self).mStatments, 0, sizeof(sStatment)*statments_size);
    SBLOCK(self).mStatmentsNum = SBLOCK(source).mStatmentsNum;
    SBLOCK(self).mStatmentsSize = statments_size;
    if(SBLOCK(source).mSource) {
        SBLOCK(self).mSource = STRDUP(SBLOCK(source).mSource);
    }
    else {
        SBLOCK(self).mSource = NULL;
    }
    SBLOCK(self).mCompletionFlags = SBLOCK(source).mCompletionFlags;

    /// copy contents of all statments
    int i;
    for(i=0; i<SBLOCK(source).mStatmentsNum; i++) {
        sStatment_copy_deeply(SBLOCK(self).mStatments + i, SBLOCK(source).mStatments + i);
    }

    return self;
}

sObject* block_clone_on_stack(sObject* source, int type)
{
    sObject* self = stack_get_free_object(type);

    const int statments_size = SBLOCK(source).mStatmentsSize;
    
    SBLOCK(self).mStatments = MALLOC(sizeof(sStatment)*statments_size);
    memset(SBLOCK(self).mStatments, 0, sizeof(sStatment)*statments_size);
    SBLOCK(self).mStatmentsNum = SBLOCK(source).mStatmentsNum;
    SBLOCK(self).mStatmentsSize = statments_size;
    if(SBLOCK(source).mSource) {
        SBLOCK(self).mSource = STRDUP(SBLOCK(source).mSource);
    }
    else {
        SBLOCK(self).mSource = NULL;
    }
    SBLOCK(self).mCompletionFlags = SBLOCK(source).mCompletionFlags;

    /// copy contents of all statments
    int i;
    for(i=0; i<SBLOCK(source).mStatmentsNum; i++) {
        sStatment_copy_deeply_stack(SBLOCK(self).mStatments + i, SBLOCK(source).mStatments + i);
    }

    return self;
}

sObject* block_new_on_stack()
{
   sObject* self = stack_get_free_object(T_BLOCK);
   
   SBLOCK(self).mStatments = MALLOC(sizeof(sStatment)*1);
   memset(SBLOCK(self).mStatments, 0, sizeof(sStatment)*1);
   SBLOCK(self).mStatmentsNum = 0;
   SBLOCK(self).mStatmentsSize = 1;
   SBLOCK(self).mSource = NULL;
    
   return self;
}

void block_delete_on_gc(sObject* self)
{
    int i;
    for(i=0; i<SBLOCK(self).mStatmentsNum; i++) {
        sStatment_delete(SBLOCK(self).mStatments + i);
    }
    FREE(SBLOCK(self).mStatments);

    if(SBLOCK(self).mSource) FREE(SBLOCK(self).mSource);
}

void block_delete_on_stack(sObject* self)
{
    int i;
    for(i=0; i<SBLOCK(self).mStatmentsNum; i++) {
        sStatment_delete(SBLOCK(self).mStatments + i);
    }
    FREE(SBLOCK(self).mStatments);

    if(SBLOCK(self).mSource) FREE(SBLOCK(self).mSource);
}


int block_gc_children_mark(sObject* self)
{
    int count = 0;

    int i;
    for(i=0; i<SBLOCK(self).mStatmentsNum; i++) {
        int j;
        for(j=0; j<SBLOCK(self).mStatments[i].mCommandsNum; j++) {
            int k;
            for(k=0; k<SBLOCK(self).mStatments[i].mCommands[j].mBlocksNum; k++) 
            {
                sObject* block = SBLOCK(self).mStatments[i].mCommands[j].mBlocks[k];
                if(IS_MARKED(block) == 0) {
                    SET_MARK(block);
                    count++;
                    count += block_gc_children_mark(block);
                }
            }
            for(k=0; k<SBLOCK(self).mStatments[i].mCommands[j].mEnvsNum; k++) {
                if(SBLOCK(self).mStatments[i].mCommands[j].mEnvs[k].mFlags & ENV_FLAGS_KIND_BLOCK) {
                    sObject* block = SBLOCK(self).mStatments[i].mCommands[j].mEnvs[k].mBlock;
                    if(IS_MARKED(block) == 0) {
                        SET_MARK(block);
                        count++;
                        count += block_gc_children_mark(block);
                    }
                }
            }
        }
    }

    return count;
}

