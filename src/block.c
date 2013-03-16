#include "config.h"

#include "xyzsh/xyzsh.h"
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
            FREE(runinfo->mRedirectsFileNamesRuntime[i]);
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

static BOOL sRunInfo_put_option(sRunInfo* self, MANAGED char* key)
{
    int hash_value = options_hash_fun(key);

    option_hash_it* p = self->mOptions + hash_value;
    while(1) {
        if(p->mKey) {
            p++;
            if(p == self->mOptions + hash_value) {
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

static BOOL sRunInfo_put_option_with_argument(sRunInfo* self, MANAGED char* key, MANAGED char* argument)
{
    int hash_value = options_hash_fun(key);

    option_hash_it* p = self->mOptions + hash_value;
    while(1) {
        if(p->mKey) {
            p++;
            if(p == self->mOptions + hash_value) {
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

static void sRunInfo_add_arg(sRunInfo* self, MANAGED char* buf)
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
    dest->mKind = src->mKind;

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
            dest->mEnvs[i].mKeyEnv = src->mEnvs[i].mKeyEnv;
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
    dest->mKind = src->mKind;

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
            dest->mEnvs[i].mKeyEnv = src->mEnvs[i].mKeyEnv;
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

BOOL sCommand_add_arg(sCommand* self, MANAGED char* buf, int env, int glob, char* sname, int sline)
{
    if(self->mArgsNum+1 >= self->mArgsSize) {
        const int arg_size = self->mArgsSize;

        int new_size = self->mArgsSize * 2;
    
        if(new_size >= XYZSH_ARG_SIZE_MAX) {
            err_msg("argument size overflow", sname, sline, self->mArgs[0]);
            return FALSE;
        }

        self->mArgs = REALLOC(self->mArgs, sizeof(char*)*new_size);
        self->mArgsFlags = REALLOC(self->mArgsFlags,sizeof(int)*new_size);
        memset(self->mArgs + self->mArgsSize, 0, sizeof(char) * (new_size - self->mArgsSize));
        memset(self->mArgsFlags + self->mArgsSize, 0, sizeof(int)* (new_size - self->mArgsSize));
        self->mArgsSize = new_size;

    }

    self->mArgs[self->mArgsNum] = buf;
    self->mArgsFlags[self->mArgsNum++] = (env ? XYZSH_ARGUMENT_ENV:0) | (glob ? XYZSH_ARGUMENT_GLOB:0);
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
                err_msg("block size overflow", sname, sline, self->mArgs[0]);
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
                err_msg("message size overflow", sname, sline, self->mArgs[0]);
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

BOOL sCommand_add_env(sCommand* self, MANAGED char* name, MANAGED char* key, BOOL key_env, BOOL double_dollar, BOOL option, char* sname, int sline)
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
                err_msg("environment size overflow", sname, sline, self->mArgs[0]);
                return FALSE;
            }
            self->mEnvs = REALLOC(self->mEnvs, sizeof(sEnv)*self->mEnvsSize);
        }
    }

    self->mEnvs[self->mEnvsNum].mFlags = ENV_FLAGS_KIND_ENV | (double_dollar ? ENV_FLAGS_DOUBLE_DOLLAR: 0) | (option ? ENV_FLAGS_OPTION:0);
    self->mEnvs[self->mEnvsNum].mName = MANAGED name;
    self->mEnvs[self->mEnvsNum].mKey = MANAGED key;
    self->mEnvs[self->mEnvsNum].mKeyEnv = key_env;
    self->mEnvsNum++;

    return TRUE;
}

BOOL sCommand_add_env_block(sCommand* self, sObject* block, BOOL double_dollar, BOOL option, eLineField lf, char* sname, int sline)
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
                err_msg("environment size overflow", sname, sline, self->mArgs[0]);
                return FALSE;
            }
            self->mEnvs = REALLOC(self->mEnvs, sizeof(sEnv)*self->mEnvsSize);

        }
    }

    self->mEnvs[self->mEnvsNum].mFlags = ENV_FLAGS_KIND_BLOCK | (double_dollar ? ENV_FLAGS_DOUBLE_DOLLAR:0) | (option ? ENV_FLAGS_OPTION:0);
    self->mEnvs[self->mEnvsNum].mBlock = block_clone_on_stack(block, T_BLOCK);
    self->mEnvs[self->mEnvsNum].mLineField = lf;

    self->mEnvsNum++;

    return TRUE;
}

BOOL sCommand_add_redirect(sCommand* self, MANAGED char* name, BOOL env, BOOL glob, int redirect, char* sname, int sline)
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
                err_msg("redirect size overflow", sname, sline, self->mArgs[0]);
                return FALSE;
            }

            self->mRedirectsSize *= 2;
            self->mRedirectsFileNames = REALLOC(self->mRedirectsFileNames, sizeof(char*)*self->mRedirectsSize);
            self->mRedirects = REALLOC(self->mRedirects, sizeof(int)*self->mRedirectsSize);
        }
    }

    self->mRedirectsFileNames[self->mRedirectsNum] = MANAGED name;
    self->mRedirects[self->mRedirectsNum++] = redirect | (env ? REDIRECT_ENV:0) | (glob ? REDIRECT_GLOB: 0);
    
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

    return self;
}

static void sStatment_copy_deeply(sStatment* dest, sStatment* source)
{
    dest->mCommandsNum = source->mCommandsNum;

    int i;
    for(i=0; i<source->mCommandsNum; i++) {
        sCommand_copy_deeply(dest->mCommands + i, source->mCommands + i);
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
    dest->mCommandsNum = source->mCommandsNum;

    int i;
    for(i=0; i<source->mCommandsNum; i++) {
        sCommand_copy_deeply_stack(dest->mCommands + i, source->mCommands + i);
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
    int i;
    for(i=0; i<self->mCommandsNum; i++) {
        sCommand_delete(self->mCommands + i);
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

//////////////////////////////////////////////////
// Expand variable and glob
//////////////////////////////////////////////////
static BOOL expand_env(ALLOC char** result, BOOL* option, char* str, sCommand* command, sRunInfo* runinfo, sObject* nextin, BOOL* expand_double_dollar, sObject* fun);

static BOOL expand_variable(sCommand* command, sObject* object, sBuf* buf, sRunInfo* runinfo, sEnv* env, sObject* nextin, sObject* fun)
{
    /// expand key ///
    char* key;
    if(env->mKeyEnv) {
        BOOL expand_double_dollar;
        BOOL option;
        if(!expand_env(ALLOC &key, &option, env->mKey, command, runinfo, nextin, &expand_double_dollar, fun)) {
            return FALSE;
        }

        if(expand_double_dollar) {
            err_msg("invalid expanding varibale block", runinfo->mSName, runinfo->mSLine, command->mArgs[0]);
            FREE(key);
            return FALSE;
        }
    }
    else {
        key = STRDUP(env->mKey);
    }

    switch(TYPE(object)) {
        case T_STRING: {
            add_str_to_buf(buf, string_c_str(object), string_length(object));
            }
            break;

        case T_VECTOR:
            if(*key == 0) {
                int i;
                for(i=0; i<vector_count(object); i++) {
                    sObject* object2 = vector_item(object, i);
                    add_str_to_buf(buf, string_c_str(object2), string_length(object2));
                    if(i+1<vector_count(object)) add_str_to_buf(buf, " ", 1);
                }
            }
            else {
                int index = atoi(key);
                if(index < 0) {
                    index += vector_count(object);
                }

                if(index >= 0 && index < vector_count(object)) {
                    object = vector_item(object, index);
                    add_str_to_buf(buf, string_c_str(object), string_length(object));
                }
            }
            break;

        case T_HASH:
            if(*key == 0) {
                char buf[128];
                snprintf(buf, 128, "need hash key(%s)", env->mName);
                err_msg(buf, runinfo->mSName, runinfo->mSLine, command->mArgs[0]);
                FREE(key);
                return FALSE;
            }
            else {
                object = hash_item(object, key);
                if(object) {
                    add_str_to_buf(buf, string_c_str(object), string_length(object));
                }
            }
            break;

        default: {
            char buf[128];
            snprintf(buf, 128, "can't expand variable because of type(%s)", env->mName);
            err_msg(buf, runinfo->mSName, runinfo->mSLine, command->mArgs[0]);
            FREE(key);
            return FALSE;
            }
    }

    FREE(key);
    return TRUE;
}

static BOOL expand_glob(char* buf, ALLOC char** head, ALLOC char*** tails, sCommand* command, char* sname, int sline)
{
    glob_t result;
    int rc = glob(buf, 0, NULL, &result);

    if(rc == GLOB_NOSPACE) {
        err_msg("read_one_argument: out of space during glob operation", sname, sline, buf);
        globfree(&result);
        return FALSE;
    }
    else if(rc == GLOB_NOMATCH) {
        *head = STRDUP(buf);

        *tails = MALLOC(sizeof(char*)*result.gl_pathc);
        **tails = NULL;
    }
    else {
        if(result.gl_pathc > 0) {
            *head = STRDUP(result.gl_pathv[0]);

            *tails = MALLOC(sizeof(char*)*result.gl_pathc);

            int i;
            for(i=1; i<result.gl_pathc; i++) {
                (*tails)[i-1] = STRDUP(result.gl_pathv[i]);
            }
            (*tails)[i-1] = NULL;
        }
        else {
            *head = STRDUP(buf);

            *tails = MALLOC(sizeof(char*)*result.gl_pathc);
            **tails = NULL;
        }
    }

    globfree(&result);

    return TRUE;
}

// if return is FALSE, no allocated result
static BOOL expand_env(ALLOC char** result, BOOL* option, char* str, sCommand* command, sRunInfo* runinfo, sObject* nextin, BOOL* expand_double_dollar, sObject* fun)
{
    *expand_double_dollar = FALSE;
    *option = FALSE;

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

            if(!*option && env->mFlags & ENV_FLAGS_OPTION) *option = TRUE;

            if(env->mFlags & ENV_FLAGS_KIND_ENV) {
                sObject* object;
                if(!get_object_from_str(&object, env->mName, runinfo->mCurrentObject, runinfo->mRunningObject, runinfo)) {
                    return FALSE;
                }

                if(object == NULL) {
                    char* env2 = getenv(env->mName);

                    if(env2) {
                        add_str_to_buf(&buf, env2, strlen(env2));
                    }
                    else {
                        FREE(buf.mBuf);
                        char buf[128];
                        snprintf(buf, 128, "no such as object(%s)", env->mName);
                        err_msg(buf, runinfo->mSName, runinfo->mSLine, command->mArgs[0]);
                        return FALSE;
                    }
                }
                else {
                    if(!expand_variable(command, object, &buf, runinfo, env,nextin, fun)) {
                        FREE(buf.mBuf);
                        return FALSE;
                    }
                }
            }
            /// block ///
            else {
                int rcode;
                sObject* nextout = FD_NEW_STACK();

                if(!run(env->mBlock, nextin, nextout, &rcode, runinfo->mCurrentObject, runinfo->mRunningObject))
                {
                    err_msg_adding("run time error", runinfo->mSName, runinfo->mSLine, command->mArgs[0]);
                    runinfo->mRCode = rcode;
                    FREE(buf.mBuf);
                    return FALSE;
                }

                if(env->mFlags & ENV_FLAGS_DOUBLE_DOLLAR) {
                    *expand_double_dollar = TRUE;

                    if(buf.mBuf[0] != 0) {
                        sRunInfo_add_arg(runinfo, MANAGED buf.mBuf);
                        memset(&buf, 0, sizeof(sBuf));
                        buf.mBuf = MALLOC(64);
                        buf.mBuf[0] = 0;
                        buf.mSize = 64;
                    }

                    fd_split(nextout, env->mLineField);
                    
                    int j;
                    for(j=0; j<vector_count(SFD(nextout).mLines); j++) 
                    {
                        char* item = vector_item(SFD(nextout).mLines, j);
                        sObject* item2 = STRING_NEW_STACK(item);
                        string_chomp(item2);

                        if(string_length(item2) > 0) {
                            if((env->mFlags & ENV_FLAGS_OPTION) && string_c_str(item2)[0] == '-') {
                                if(fun && (TYPE(fun) == T_NFUN && nfun_option_with_argument(fun, string_c_str(item2)) || TYPE(fun) == T_CLASS && class_option_with_argument(fun, string_c_str(item2)) || TYPE(fun) == T_FUN && fun_option_with_argument(fun, string_c_str(item2)) ))
                                {
                                    if(j + 1 >= vector_count(SFD(nextout).mLines)) {
                                        FREE(buf.mBuf);
                                        char buf[BUFSIZ];
                                        snprintf(buf, BUFSIZ, "invalid option with argument(%s)", string_c_str(item2));
                                        err_msg(buf, runinfo->mSName, runinfo->mSLine, command->mArgs[0]);
                                        return FALSE;
                                    }

                                    char* option = STRDUP(string_c_str(item2));
                                    char* next_item = vector_item(SFD(nextout).mLines, j+1);
                                    sObject* next_item2 = STRING_NEW_STACK(next_item);
                                    string_chomp(next_item2);
                                    char* arg = STRDUP(string_c_str(next_item2));

                                    if(!sRunInfo_put_option_with_argument(runinfo, MANAGED option, MANAGED arg))
                                    {
                                        FREE(buf.mBuf);
                                        char buf[BUFSIZ];
                                        snprintf(buf, BUFSIZ, "option number max (%s)\n", arg);
                                        err_msg(buf, runinfo->mSName, runinfo->mSLine, command->mArgs[0]);
                                        FREE(arg);
                                        FREE(option);
                                        return FALSE;
                                    }
                                    j++;
                                }
                                else {
                                    char* option = STRDUP(string_c_str(item2));
                                    if(!sRunInfo_put_option(runinfo, MANAGED option)) {
                                        FREE(buf.mBuf);
                                        char buf[BUFSIZ];
                                        snprintf(buf, BUFSIZ, "option number max (%s)\n", option);
                                        err_msg(buf, runinfo->mSName, runinfo->mSLine, command->mArgs[0]);
                                        FREE(option);
                                        return FALSE;
                                    }
                                }
                            }
                            else {
                                sRunInfo_add_arg(runinfo, MANAGED STRDUP(string_c_str(item2)));
                            }
                        }
                    }
                }
                else {
                    add_str_to_buf(&buf, SFD(nextout).mBuf, SFD(nextout).mBufLen);
                }
            }
        }
        else {
            add_char_to_buf(&buf, *p++);
        }
    }

    *result = buf.mBuf;

    return TRUE;
}

static BOOL put_option(MANAGED char* arg, sCommand* command, sObject* fun, sObject* nextin, sObject* nextout, sRunInfo* runinfo, int* i)
{
    if(fun && (TYPE(fun) == T_NFUN && nfun_option_with_argument(fun, arg) || TYPE(fun) == T_CLASS && class_option_with_argument(fun,arg) || TYPE(fun) == T_FUN && fun_option_with_argument(fun, arg) ))
    {
        if(*i + 1 >= command->mArgsNum) {
            char buf[BUFSIZ];
            snprintf(buf, BUFSIZ, "invalid option with argument(%s)", arg);
            err_msg(buf, runinfo->mSName, runinfo->mSLine, command->mArgs[0]);
            FREE(arg);
            return FALSE;
        }

        /// environment or environment and glob ///
        if(command->mArgsFlags[*i+1] & XYZSH_ARGUMENT_GLOB) {
            err_msg("can't expand glob with option", runinfo->mSName, runinfo->mSLine, command->mArgs[0]);
            FREE(arg);
            return FALSE;
        }
        else if(command->mArgsFlags[*i+1] & XYZSH_ARGUMENT_ENV) {
            char* expanded_str;
            BOOL expand_double_dollar;
            BOOL option;
            if(!expand_env(ALLOC &expanded_str, &option, command->mArgs[*i+1], command, runinfo, nextin, &expand_double_dollar, fun)) {
                FREE(arg);
                return FALSE;
            }

            if(expand_double_dollar) {
                err_msg("invalid expanding varibale block", runinfo->mSName, runinfo->mSLine, command->mArgs[0]);
                FREE(expanded_str);
                FREE(arg);
                return FALSE;
            }

            if(!sRunInfo_put_option_with_argument(runinfo, MANAGED arg, MANAGED expanded_str))
            {
                char buf[BUFSIZ];
                snprintf(buf, BUFSIZ, "option number max (%s)\n", arg);
                err_msg(buf, runinfo->mSName, runinfo->mSLine, command->mArgs[0]);
                FREE(expanded_str);
                FREE(arg);
                return FALSE;
            }
        }
        else {
            if(command->mArgs[*i+1][0] == PARSER_MAGIC_NUMBER_OPTION) {
                char buf[BUFSIZ];
                snprintf(buf, BUFSIZ, "invalid option with argument(%s)", arg);
                err_msg(buf, runinfo->mSName, runinfo->mSLine, command->mArgs[0]);
                FREE(arg);
                return FALSE;
            }

            if(!sRunInfo_put_option_with_argument(runinfo, MANAGED arg, MANAGED STRDUP(command->mArgs[*i+1])))
            {
                char buf[BUFSIZ];
                snprintf(buf, BUFSIZ, "option number max (%s)\n", arg);
                err_msg(buf, runinfo->mSName, runinfo->mSLine, command->mArgs[0]);
                FREE(arg);
                return FALSE;
            }
        }
        (*i)++;
    }
    else {
        if(!sRunInfo_put_option(runinfo, MANAGED arg)) {
            char buf[BUFSIZ];
            snprintf(buf, BUFSIZ, "option number max (%s)\n", arg);
            err_msg(buf, runinfo->mSName, runinfo->mSLine, command->mArgs[0]);
            FREE(arg);
            return FALSE;
        }
    }

    return TRUE;
}

BOOL sCommand_expand_env(sCommand* command, sObject* fun, sObject* nextin, sObject* nextout, sRunInfo* runinfo)
{
    BOOL external_command = fun == NULL || TYPE(fun) == T_EXTPROG;

    int i;
    for(i=0; i < command->mArgsNum; i++) {
        /// options ///
        if(command->mArgs[i][0] == PARSER_MAGIC_NUMBER_OPTION) {
            if(command->mArgsFlags[i] & XYZSH_ARGUMENT_GLOB) {
                err_msg("can't expand glob in option", runinfo->mSName, runinfo->mSLine, command->mArgs[0]);
                return FALSE;
            }

            if(command->mArgsFlags[i] & XYZSH_ARGUMENT_ENV) {
                char* expanded_str;
                BOOL expand_double_dollar;
                BOOL option;
                if(!expand_env(ALLOC &expanded_str, &option, command->mArgs[i], command, runinfo, nextin, &expand_double_dollar, fun)) {
                    return FALSE;
                }

                if(expand_double_dollar) {
                    err_msg("invalid expanding varibale block", runinfo->mSName, runinfo->mSLine, command->mArgs[0]);
                    FREE(expanded_str);
                    return FALSE;
                }

                expanded_str[0] = '-';

                if(external_command) {
                    sRunInfo_add_arg(runinfo, MANAGED expanded_str);
                }
                else {
                    if(!put_option(MANAGED expanded_str, command, fun, nextin, nextout, runinfo, &i)) {
                        return FALSE;
                    }
                }
            }
            else {
                if(external_command) {
                    char* arg = MALLOC(strlen(command->mArgs[i]) + 1);
                    arg[0] = '-';
                    strcpy(arg + 1, command->mArgs[i] + 1);
                    sRunInfo_add_arg(runinfo, MANAGED arg);
                }
                else {
                    char* arg = MALLOC(strlen(command->mArgs[i]) + 1);
                    arg[0] = '-';
                    strcpy(arg + 1, command->mArgs[i] + 1);
                    if(!put_option(MANAGED arg, command, fun, nextin, nextout, runinfo, &i)) {
                        return FALSE;
                    }
                }
            }
        }
        /// variable ///
        else if(command->mArgsFlags[i] & XYZSH_ARGUMENT_ENV) {
            /// variable and glob ///
            if(command->mArgsFlags[i] & XYZSH_ARGUMENT_GLOB) {
                char* expanded_str;
                BOOL expand_double_dollar;
                BOOL option;
                if(!expand_env(ALLOC &expanded_str, &option, command->mArgs[i], command, runinfo, nextin, &expand_double_dollar, fun)) {
                    return FALSE;
                }

                if(expand_double_dollar) {
                    FREE(expanded_str);
                    err_msg("can't expand glob with double dollar environment block", runinfo->mSName, runinfo->mSLine, command->mArgs[0]);
                    return FALSE;
                }
                else {
                    char* head;
                    char** tails;
                    if(!expand_glob(expanded_str, ALLOC &head, ALLOC &tails, command, runinfo->mSName, runinfo->mSLine))
                    {
                        FREE(expanded_str);
                        return FALSE;
                    }
                    sRunInfo_add_arg(runinfo, MANAGED head);
                    char** p = tails;
                    while(*p) {
                        sRunInfo_add_arg(runinfo, MANAGED *p);
                        p++;
                    }
                    FREE(tails);
                }

                FREE(expanded_str);
            }
            else {
                char* expanded_str;
                BOOL expand_double_dollar;
                BOOL option;
                if(!expand_env(ALLOC &expanded_str, &option, command->mArgs[i], command, runinfo, nextin, &expand_double_dollar, fun)) {
                    return FALSE;
                }

                if(expand_double_dollar) {
                    if(expanded_str[0] == 0) {
                        FREE(expanded_str);
                    }
                    else {
                        sRunInfo_add_arg(runinfo, MANAGED expanded_str);
                    }
                }
                else {
                    if(option && expanded_str[0] == '-') {
                        if(fun && (TYPE(fun) == T_NFUN && nfun_option_with_argument(fun, expanded_str) || TYPE(fun) == T_CLASS && class_option_with_argument(fun, expanded_str) || TYPE(fun) == T_FUN && fun_option_with_argument(fun, expanded_str)))
                        {
                            if(i + 1 >= command->mArgsNum || command->mArgsFlags[i+1] & XYZSH_ARGUMENT_GLOB || command->mArgs[i+1][0] == PARSER_MAGIC_NUMBER_OPTION)
                            {
                                FREE(expanded_str);
                                char buf[BUFSIZ];
                                snprintf(buf, BUFSIZ, "invalid option with argument(%s)", expanded_str);
                                err_msg(buf, runinfo->mSName, runinfo->mSLine, command->mArgs[0]);
                                return FALSE;
                            }

                            char* arg;

                            if(command->mArgsFlags[i+1] & XYZSH_ARGUMENT_ENV) {
                                char* expanded_str2;
                                BOOL expand_double_dollar2;
                                BOOL option;
                                if(!expand_env(ALLOC &expanded_str2, &option, command->mArgs[i+1], command, runinfo, nextin, &expand_double_dollar2, fun)) 
                                {
                                    return FALSE;
                                }

                                if(expand_double_dollar2) {
                                    FREE(expanded_str);
                                    FREE(expanded_str2);
                                    char buf[BUFSIZ];
                                    snprintf(buf, BUFSIZ, "invalid option with argument(%s)", expanded_str);
                                    err_msg(buf, runinfo->mSName, runinfo->mSLine, command->mArgs[0]);
                                    return FALSE;
                                }

                                arg = expanded_str2;
                            }
                            else {
                                arg = STRDUP(command->mArgs[i+1]);
                            }

                            if(!sRunInfo_put_option_with_argument(runinfo, expanded_str, MANAGED arg))
                            {
                                FREE(expanded_str);
                                FREE(arg);
                                char buf[BUFSIZ];
                                snprintf(buf, BUFSIZ, "option number max (%s)\n", arg);
                                err_msg(buf, runinfo->mSName, runinfo->mSLine, command->mArgs[0]);
                                return FALSE;
                            }
                            i++;
                        }
                        else {
                            if(!sRunInfo_put_option(runinfo, MANAGED expanded_str)) {
                                FREE(expanded_str);
                                char buf[BUFSIZ];
                                snprintf(buf, BUFSIZ, "option number max (%s)\n", expanded_str);
                                err_msg(buf, runinfo->mSName, runinfo->mSLine, command->mArgs[0]);
                                return FALSE;
                            }
                        }
                    }
                    else {
                        sRunInfo_add_arg(runinfo, MANAGED expanded_str);
                    }
                }
            }
        }
        /// glob
        else if(command->mArgsFlags[i] & XYZSH_ARGUMENT_GLOB) {
            char* head;
            char** tails;
            if(!expand_glob(command->mArgs[i], ALLOC &head, ALLOC &tails, command, runinfo->mSName, runinfo->mSLine))
            {
                return FALSE;
            }
            sRunInfo_add_arg(runinfo, MANAGED head);
            char** p = tails;
            while(*p) {
                sRunInfo_add_arg(runinfo, MANAGED *p);
                p++;
            }
            FREE(tails);
        }
        /// normal ///
        else {
            sRunInfo_add_arg(runinfo, MANAGED STRDUP(command->mArgs[i]));
        }
    }

    return TRUE;
}

BOOL sCommand_expand_env_of_redirect(sCommand* command, sObject* nextin, sRunInfo* runinfo)
{
    if(command->mRedirectsNum > 0) {
        runinfo->mRedirectsFileNamesRuntime = MALLOC(sizeof(char*)*command->mRedirectsNum);

        int i;
        for(i=0; i<command->mRedirectsNum; i++) {
            if(command->mRedirects[i] & REDIRECT_ENV) {
                char* expanded_str;
                BOOL expand_double_dollar;
                BOOL option;
                if(!expand_env(ALLOC &expanded_str, &option, command->mRedirectsFileNames[i], command, runinfo, nextin, &expand_double_dollar, NULL)) {
                    return FALSE;
                }

                if(expand_double_dollar) {
                    err_msg("invalid expanding varibale block", runinfo->mSName, runinfo->mSLine, command->mArgs[0]);
                    FREE(expanded_str);
                    return FALSE;
                }

                if(command->mRedirects[i] & REDIRECT_GLOB) {
                    char* head;
                    char** tails;
                    if(!expand_glob(expanded_str, ALLOC &head, ALLOC &tails, command, runinfo->mSName, runinfo->mSLine))
                    {
                        FREE(expanded_str);
                        return FALSE;
                    }
                    runinfo->mRedirectsFileNamesRuntime[i] = MANAGED head;
                    runinfo->mRedirectsNumRuntime++;
                    char** p = tails;
                    while(*p) {
                        FREE(*p);
                        p++;
                    }
                    FREE(tails);

                    FREE(expanded_str);
                }
                else {
                    runinfo->mRedirectsFileNamesRuntime[i] = MANAGED expanded_str;
                    runinfo->mRedirectsNumRuntime++;
                }
            }
            else if(command->mRedirects[i] & REDIRECT_GLOB) {
                char* head;
                char** tails;
                if(!expand_glob(command->mRedirectsFileNames[i], ALLOC &head, ALLOC &tails, command, runinfo->mSName, runinfo->mSLine))
                {
                    return FALSE;
                }
                runinfo->mRedirectsFileNamesRuntime[i] = MANAGED head;
                runinfo->mRedirectsNumRuntime++;
                char** p = tails;
                while(*p) {
                    FREE(*p);
                    p++;
                }
                FREE(tails);
            }
            else {
                runinfo->mRedirectsFileNamesRuntime[i] = STRDUP(command->mRedirectsFileNames[i]);
                runinfo->mRedirectsNumRuntime++;
            }
        }
    }
    else {
        runinfo->mRedirectsFileNamesRuntime = NULL;
    }

    return TRUE;
}

