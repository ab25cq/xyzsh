#include "config.h"

#include "xyzsh/xyzsh.h"
#include <string.h>
#include <stdio.h>
#include <glob.h>

sCommand* sCommand_new(sStatment* statment)
{
    sCommand* self = statment->mCommands + statment->mCommandsNum;

    self->mArgs = MALLOC(sizeof(char*)*3);
    self->mArgsFlags = MALLOC(sizeof(int)*3);
    self->mArgsSize = 3;
    self->mArgsNum = 0;

    self->mArgsRuntime = MALLOC(sizeof(char*)*3);
    self->mArgsSizeRuntime = 3;
    self->mArgsNumRuntime = 0;

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

    self->mRedirectsFileNamesRuntime = NULL;

    memset(self->mMessages, 0, sizeof(char*)*XYZSH_MESSAGES_MAX);
    self->mMessagesNum = 0;

    memset(self->mOptions, 0, sizeof(option_hash_it)*XYZSH_OPTION_MAX);

    statment->mCommandsNum++;

    return self;
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


BOOL sCommand_put_option(sCommand* self, MANAGED char* key)
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

BOOL sCommand_put_option_with_argument(sCommand* self, MANAGED char* key, MANAGED char* argument)
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

BOOL sCommand_option_item(sCommand* self, char* key)
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

void sCommand_sweep_runtime_info(sCommand* self)
{
    int i;
    for(i=0; i<self->mArgsNumRuntime; i++) {
        FREE(self->mArgsRuntime[i]);
    }
    self->mArgsNumRuntime = 0;

    for(i=0; i<XYZSH_OPTION_MAX; i++) {
        if(self->mOptions[i].mKey) { FREE(self->mOptions[i].mKey); }
        if(self->mOptions[i].mArg) { FREE(self->mOptions[i].mArg); }
    }

    memset(self->mOptions, 0, sizeof(option_hash_it)*XYZSH_OPTION_MAX);

    if(self->mRedirectsFileNamesRuntime) {
        for(i=0; i<self->mRedirectsNum; i++) {
            FREE(self->mRedirectsFileNamesRuntime[i]);
        }
        FREE(self->mRedirectsFileNamesRuntime);

        self->mRedirectsFileNamesRuntime = NULL;
    }
}

void sStatment_sweep_runtime_info(sStatment* self)
{
    int j;
    for(j=0; j<self->mCommandsNum; j++) {
        sCommand* command = self->mCommands + j;
        
        int i;
        for(i=0; i<command->mArgsNumRuntime; i++) {
            FREE(command->mArgsRuntime[i]);
        }
        command->mArgsNumRuntime = 0;

        for(i=0; i<XYZSH_OPTION_MAX; i++) {
            if(command->mOptions[i].mKey) { FREE(command->mOptions[i].mKey); }
            if(command->mOptions[i].mArg) { FREE(command->mOptions[i].mArg); }
        }

        memset(command->mOptions, 0, sizeof(option_hash_it)*XYZSH_OPTION_MAX);

        if(command->mRedirectsFileNamesRuntime) {
            for(i=0; i<command->mRedirectsNum; i++) {
                FREE(command->mRedirectsFileNamesRuntime[i]);
            }
            FREE(command->mRedirectsFileNamesRuntime);

            command->mRedirectsFileNamesRuntime = NULL;
        }
    }
}

void sCommand_add_redirect_to_command(sCommand* self, MANAGED char* name, BOOL env, BOOL glob, int redirect)
{
    if(self->mRedirectsNum >= self->mRedirectsSize) {
        if(self->mRedirectsFileNames == NULL) {
            self->mRedirectsSize = 1;
            self->mRedirectsFileNames = MALLOC(sizeof(char*)*self->mRedirectsSize);
            self->mRedirects = MALLOC(sizeof(int)*self->mRedirectsSize);

        }
        else {
            const int redirect_size = self->mRedirectsSize;

            self->mRedirectsSize *= 2;
            self->mRedirectsFileNames = REALLOC(self->mRedirectsFileNames, sizeof(char*)*self->mRedirectsSize);
            self->mRedirects = REALLOC(self->mRedirects, sizeof(int)*self->mRedirectsSize);

        }
    }

    self->mRedirectsFileNames[self->mRedirectsNum] = MANAGED name;
    self->mRedirects[self->mRedirectsNum++] = redirect | (env ? REDIRECT_ENV:0) | (glob ? REDIRECT_GLOB: 0);
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

char* sCommand_option_with_argument_item(sCommand* self, char* key)
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
        dest->mEnvs[i].mKind = src->mEnvs[i].mKind;

        if(src->mEnvs[i].mKind == kEnv) {
            dest->mEnvs[i].mName = STRDUP(src->mEnvs[i].mName);
            dest->mEnvs[i].mKey = STRDUP(src->mEnvs[i].mKey);
            dest->mEnvs[i].mKeyEnv = src->mEnvs[i].mKeyEnv;
            dest->mEnvs[i].mDoubleDollar = src->mEnvs[i].mDoubleDollar;
        }
        else {
            dest->mEnvs[i].mBlock = block_clone_gc(src->mEnvs[i].mBlock, T_BLOCK, FALSE);
            dest->mEnvs[i].mDoubleDollar = src->mEnvs[i].mDoubleDollar;
            dest->mEnvs[i].mLineField = src->mEnvs[i].mLineField;
        }
    }
    dest->mEnvsNum = src->mEnvsNum;
    dest->mEnvsSize = src->mEnvsSize;

/*
    dest->mArgsRuntime = MALLOC(sizeof(char*)*src->mArgsSizeRuntime);
    dest->mArgsNumRuntime = src->mArgsNumRuntime;
    dest->mArgsSizeRuntime = src->mArgsSizeRuntime;

    for(i=0; i<src->mArgsNumRuntime; i++) {
        dest->mArgsRuntime[i] = STRDUP(src->mArgsRuntime[i]);
    }
    dest->mArgsRuntime[i] = NULL;
*/
    dest->mArgsRuntime = MALLOC(sizeof(char*)*3);
    dest->mArgsSizeRuntime = 3;
    dest->mArgsNumRuntime = 0;

    dest->mBlocks = MALLOC(sizeof(sObject*)*src->mBlocksSize);
    dest->mBlocksNum = src->mBlocksNum;
    dest->mBlocksSize = src->mBlocksSize;

    for(i=0; i<src->mBlocksNum; i++) {
        dest->mBlocks[i] = block_clone_gc(src->mBlocks[i], T_BLOCK, FALSE);
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

/*
    memcpy(dest->mOptions, src->mOptions, sizeof(option_hash_it)*XYZSH_OPTION_MAX);
    for(i=0; i<XYZSH_OPTION_MAX; i++) {
        if(src->mOptions[i].mKey) { dest->mOptions[i].mKey = STRDUP(src->mOptions[i].mKey); }
        if(src->mOptions[i].mArg) { dest->mOptions[i].mArg = STRDUP(src->mOptions[i].mArg); }
    }
*/
    memset(dest->mOptions, 0, sizeof(option_hash_it)*XYZSH_OPTION_MAX);

    if(src->mMessagesNum > 0) {
        for(i=0; i<src->mMessagesNum; i++) {
            dest->mMessages[i] = STRDUP(src->mMessages[i]);
        }
        dest->mMessagesNum = src->mMessagesNum;
    }

    dest->mRedirectsFileNamesRuntime = NULL;
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
        dest->mEnvs[i].mKind = src->mEnvs[i].mKind;

        if(src->mEnvs[i].mKind == kEnv) {
            dest->mEnvs[i].mName = STRDUP(src->mEnvs[i].mName);
            dest->mEnvs[i].mKey = STRDUP(src->mEnvs[i].mKey);
            dest->mEnvs[i].mKeyEnv = src->mEnvs[i].mKeyEnv;
            dest->mEnvs[i].mDoubleDollar = src->mEnvs[i].mDoubleDollar;
        }
        else {
            dest->mEnvs[i].mBlock = block_clone_stack(src->mEnvs[i].mBlock, T_BLOCK);
            dest->mEnvs[i].mDoubleDollar = src->mEnvs[i].mDoubleDollar;
            dest->mEnvs[i].mLineField = src->mEnvs[i].mLineField;
        }
    }
    dest->mEnvsNum = src->mEnvsNum;
    dest->mEnvsSize = src->mEnvsSize;

/*
    dest->mArgsRuntime = MALLOC(sizeof(char*)*src->mArgsSizeRuntime);
    dest->mArgsNumRuntime = src->mArgsNumRuntime;
    dest->mArgsSizeRuntime = src->mArgsSizeRuntime;

    for(i=0; i<src->mArgsNumRuntime; i++) {
        dest->mArgsRuntime[i] = STRDUP(src->mArgsRuntime[i]);
    }
    dest->mArgsRuntime[i] = NULL;
*/
    dest->mArgsRuntime = MALLOC(sizeof(char*)*3);
    dest->mArgsSizeRuntime = 3;
    dest->mArgsNumRuntime = 0;

    dest->mBlocks = MALLOC(sizeof(sObject*)*src->mBlocksSize);
    dest->mBlocksNum = src->mBlocksNum;
    dest->mBlocksSize = src->mBlocksSize;

    for(i=0; i<src->mBlocksNum; i++) {
        dest->mBlocks[i] = block_clone_stack(src->mBlocks[i], T_BLOCK);
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

/*
    memcpy(dest->mOptions, src->mOptions, sizeof(option_hash_it)*XYZSH_OPTION_MAX);

    for(i=0; i<XYZSH_OPTION_MAX; i++) {
        if(src->mOptions[i].mKey) { dest->mOptions[i].mKey = STRDUP(src->mOptions[i].mKey); }
        if(src->mOptions[i].mArg) { dest->mOptions[i].mArg = STRDUP(src->mOptions[i].mArg); }
    }
*/
    memset(dest->mOptions, 0, sizeof(option_hash_it)*XYZSH_OPTION_MAX);

    if(src->mMessagesNum > 0) {
        for(i=0; i<src->mMessagesNum; i++) {
            dest->mMessages[i] = STRDUP(src->mMessages[i]);
        }
        dest->mMessagesNum = src->mMessagesNum;
    }

    dest->mRedirectsFileNamesRuntime = NULL;
}

void sCommand_delete(sCommand* self)
{
    int i;
    for(i=0; i<self->mArgsNum; i++) {
        FREE(self->mArgs[i]);
    }
    FREE(self->mArgs);
    FREE(self->mArgsFlags);
    for(i=0; i<self->mArgsNumRuntime; i++) {
        FREE(self->mArgsRuntime[i]);
    }
    FREE(self->mArgsRuntime);

    for(i=0; i<self->mEnvsNum; i++) {
        if(self->mEnvs[i].mKind == kEnv) {
            FREE(self->mEnvs[i].mName);
            FREE(self->mEnvs[i].mKey);
        }
    }
    if(self->mEnvs) FREE(self->mEnvs);

    for(i=0; i<XYZSH_OPTION_MAX; i++) {
        if(self->mOptions[i].mKey) { FREE(self->mOptions[i].mKey); }
        if(self->mOptions[i].mArg) { FREE(self->mOptions[i].mArg); }
    }
    for(i=0; i<self->mMessagesNum; i++) {
        FREE(self->mMessages[i]);
    }

    if(self->mBlocks) FREE(self->mBlocks);

    if(self->mRedirectsFileNames) {
        for(i=0; i<self->mRedirectsNum; i++) {
            FREE(self->mRedirectsFileNames[i]);
        }
        FREE(self->mRedirectsFileNames);
    }

    if(self->mRedirects) FREE(self->mRedirects);

    if(self->mRedirectsFileNamesRuntime) {
        for(i=0; i<self->mRedirectsNum; i++) {
            FREE(self->mRedirectsFileNamesRuntime[i]);
        }
        FREE(self->mRedirectsFileNamesRuntime);
    }
}

void sCommand_view(sCommand* self)
{
    printf("mArgsSize: %d\n", self->mArgsSize);
    printf("mArgsNum: %d\n", self->mArgsNum);
    int i;
    for(i=0; i<self->mArgsNum; i++) {
        printf("%d: %s\n", i, self->mArgs[i]);
    }
}

void sCommand_add_arg_to_command(sCommand* self, MANAGED char* buf, int env, int glob)
{
    if(self->mArgsNum+1 >= self->mArgsSize) {
        const int arg_size = self->mArgsSize;

        int new_size = self->mArgsSize * 2;
        self->mArgs = REALLOC(self->mArgs, sizeof(char*)*new_size);
        self->mArgsFlags = REALLOC(self->mArgsFlags,sizeof(int)*new_size);
        memset(self->mArgs + self->mArgsSize, 0, sizeof(char) * (new_size - self->mArgsSize));
        memset(self->mArgsFlags + self->mArgsSize, 0, sizeof(int)* (new_size - self->mArgsSize));
        self->mArgsSize = new_size;

    }

    self->mArgs[self->mArgsNum] = buf;
    self->mArgsFlags[self->mArgsNum++] = (env ? ARG_ENV:0) | (glob ? ARG_GLOB:0);
    self->mArgs[self->mArgsNum] = NULL;
}

void sCommand_add_block_to_command(sCommand* self, sObject* block)
{
    if(self->mBlocksNum >= self->mBlocksSize) {
        if(self->mBlocks == NULL) {
            const int block_size = self->mBlocksSize;
            int new_block_size = 1;
            self->mBlocks = MALLOC(sizeof(sObject*)*new_block_size);
            memset(self->mBlocks + self->mBlocksSize, 0, sizeof(sObject*)*(new_block_size - self->mBlocksSize));

            self->mBlocksSize = new_block_size;

        }
        else {
            const int block_size = self->mBlocksSize;
            int new_block_size = self->mBlocksSize * 2;
            self->mBlocks = REALLOC(self->mBlocks, sizeof(sObject*)*new_block_size);
            memset(self->mBlocks + self->mBlocksSize, 0, sizeof(sObject*)*(new_block_size - self->mBlocksSize));

            self->mBlocksSize = new_block_size;

        }
    }

    self->mBlocks[self->mBlocksNum++] = block;
}

BOOL sCommand_add_message(sCommand* self, MANAGED char* message)
{
    if(self->mMessagesNum > XYZSH_MESSAGES_MAX) {
        return FALSE;
    }
    else {
        self->mMessages[self->mMessagesNum++] = message;
        return TRUE;
    }
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

void sStatment_view(sStatment* self)
{
    printf("mFlags ");
    if(self->mFlags & STATMENT_REVERSE) {
        printf("Reverse ");
    }
    else if(self->mFlags & STATMENT_BACKGROUND) {
        printf("Background ");
    }
    else if(self->mFlags & STATMENT_GLOBALPIPEIN) {
        printf("GlobalPipeIn ");
    }
    else if(self->mFlags & STATMENT_GLOBALPIPEOUT) {
        printf("GlobalPipeOut ");
    }
    else if(self->mFlags & STATMENT_CONTEXTPIPE) {
        printf("ContextPipe ");
    }
    else if(self->mFlags & STATMENT_OROR) {
        printf("OrOr ");
    }
    else if(self->mFlags & STATMENT_ANDAND) {
        printf("AndAnd ");
    }
    printf("\n");
    printf("mCommandsNum %d\n", self->mCommandsNum);

    int i;
    for(i=0; i<self->mCommandsNum; i++) {
        sCommand_view(self->mCommands + i);
    }
}

sObject* block_new_from_gc(BOOL user_object)
{
   sObject* self = gc_get_free_object(T_BLOCK, user_object);
   
   SBLOCK(self).mStatments = MALLOC(sizeof(sStatment)*1);
   memset(SBLOCK(self).mStatments, 0, sizeof(sStatment)*1);
   SBLOCK(self).mStatmentsNum = 0;
   SBLOCK(self).mStatmentsSize = 1;
   SBLOCK(self).mSource = NULL;

   return self;
}

sObject* block_clone_gc(sObject* source, int type, BOOL user_object)
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

sObject* block_clone_stack(sObject* source, int type)
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

sObject* block_new_from_stack()
{
   sObject* self = stack_get_free_object(T_BLOCK);
   
   SBLOCK(self).mStatments = MALLOC(sizeof(sStatment)*1);
   memset(SBLOCK(self).mStatments, 0, sizeof(sStatment)*1);
   SBLOCK(self).mStatmentsNum = 0;
   SBLOCK(self).mStatmentsSize = 1;
   SBLOCK(self).mSource = NULL;
    
   return self;
}

void block_delete_gc(sObject* self)
{
    int i;
    for(i=0; i<SBLOCK(self).mStatmentsNum; i++) {
        sStatment_delete(SBLOCK(self).mStatments + i);
    }
    FREE(SBLOCK(self).mStatments);

    if(SBLOCK(self).mSource) FREE(SBLOCK(self).mSource);
}

void block_delete_stack(sObject* self)
{
    int i;
    for(i=0; i<SBLOCK(self).mStatmentsNum; i++) {
        sStatment_delete(SBLOCK(self).mStatments + i);
    }
    FREE(SBLOCK(self).mStatments);

    if(SBLOCK(self).mSource) FREE(SBLOCK(self).mSource);
}

////////////////////////////////////////////////////
// some functions
////////////////////////////////////////////////////
void block_view(sObject* self)
{
    printf("mStatmentsNum %d\n", SBLOCK(self).mStatmentsNum);
    printf("mStatmentsSize %d\n", SBLOCK(self).mStatmentsSize);

    int i;
    for(i=0; i<SBLOCK(self).mStatmentsNum; i++) {
        sStatment_view(SBLOCK(self).mStatments + i);
    }
}

static void sCommand_add_arg_to_command2(sCommand* self, MANAGED char* buf)
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

static BOOL expand_env_to_command(ALLOC char** result, char* str, sCommand* command, sRunInfo* runinfo, sObject* nextin, BOOL* expand_double_dollar);

static BOOL expand_variable(sCommand* command, sObject* object, sBuf* buf, sRunInfo* runinfo, sEnv* env, sObject* nextin)
{
    /// expand key ///
    char* key;
    if(env->mKeyEnv) {
        BOOL expand_double_dollar;
        if(!expand_env_to_command(ALLOC &key, env->mKey, command, runinfo, nextin, &expand_double_dollar)) {
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
                    if(index < 0) index = 0;
                }
                if(index >= vector_count(object)) {
                    index = vector_count(object) -1;
                }

                if(vector_count(object) > 0) {
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
                if(object == NULL) {
                    char buf[128];
                    snprintf(buf, 128, "invalid key(%s)", key);
                    err_msg(buf, runinfo->mSName, runinfo->mSLine, command->mArgs[0]);
                    FREE(key);
                    return FALSE;
                }

                add_str_to_buf(buf, string_c_str(object), string_length(object));
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
            (*tails)[i-1] = 0;
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

// if return is FALSE, no allocate result
static BOOL expand_env_to_command(ALLOC char** result, char* str, sCommand* command, sRunInfo* runinfo, sObject* nextin, BOOL* expand_double_dollar)
{
    *expand_double_dollar = FALSE;

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

            if(env->mKind == kEnv) {
                sObject* current = runinfo->mCurrentObject;
                sObject* object = access_object(env->mName, &current, runinfo->mRunningObject);
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
                    if(!expand_variable(command, object, &buf, runinfo, env,nextin)) {
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

                if(env->mDoubleDollar) {
                    *expand_double_dollar = TRUE;

                    if(buf.mBuf[0] != 0) {
                        sCommand_add_arg_to_command2(command, MANAGED buf.mBuf);
                        memset(&buf, 0, sizeof(sBuf));
                        buf.mBuf = MALLOC(64);
                        buf.mBuf[0] = 0;
                        buf.mSize = 64;
                    }

                    fd_split(nextout, env->mLineField);
                    
                    int i;
                    for(i=0; i<vector_count(SFD(nextout).mLines); i++) 
                    {
                        char* item = vector_item(SFD(nextout).mLines, i);
                        sObject* item2 = STRING_NEW_STACK(item);
                        string_chomp(item2);

                        if(string_length(item2) > 0) {
                            sCommand_add_arg_to_command2(command, MANAGED STRDUP(string_c_str(item2)));
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

static BOOL get_option(MANAGED char* arg, sCommand* command, sObject* fun, sObject* nextin, sObject* nextout, sRunInfo* runinfo, int* i)
{
    if(fun && (TYPE(fun) == T_NFUN && nfun_option_with_argument_item(fun, arg) || (TYPE(fun) == T_CLASS || TYPE(fun) == T_FUN) && fun_option_with_argument_item(fun, arg)) )
    {
        if(*i + 1 >= command->mArgsNum) {
            char buf[BUFSIZ];
            snprintf(buf, BUFSIZ, "invalid option with argument(%s)", arg);
            err_msg(buf, runinfo->mSName, runinfo->mSLine, command->mArgs[0]);
            FREE(arg);
            return FALSE;
        }

        /// environment or environment and glob ///
        if(command->mArgsFlags[*i+1] & ARG_GLOB) {
            err_msg("can't expand glob with option", runinfo->mSName, runinfo->mSLine, command->mArgs[0]);
            FREE(arg);
            return FALSE;
        }
        else if(command->mArgsFlags[*i+1] & ARG_ENV) {
            char* expanded_str;
            BOOL expand_double_dollar;
            if(!expand_env_to_command(ALLOC &expanded_str, command->mArgs[*i+1], command, runinfo, nextin, &expand_double_dollar)) {
                FREE(arg);
                return FALSE;
            }

            if(expand_double_dollar) {
                err_msg("invalid expanding varibale block", runinfo->mSName, runinfo->mSLine, command->mArgs[0]);
                FREE(expanded_str);
                FREE(arg);
                return FALSE;
            }

            if(!sCommand_put_option_with_argument(command, MANAGED arg, MANAGED expanded_str))
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

            if(!sCommand_put_option_with_argument(command, MANAGED arg, MANAGED STRDUP(command->mArgs[*i+1])))
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
        if(!sCommand_put_option(command, MANAGED arg)) {
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

    if(command->mArgsNumRuntime > 0) {
        int i;
        for(i=0; i<command->mArgsNumRuntime; i++) {
            FREE(command->mArgsRuntime[i]);
        }
        command->mArgsNumRuntime = 0;
    }

    int i;
    for(i=0; i < command->mArgsNum; i++) {
        /// options ///
        if(command->mArgs[i][0] == PARSER_MAGIC_NUMBER_OPTION) {
            if(command->mArgsFlags[i] & ARG_GLOB) {
                err_msg("can't expand glob in option", runinfo->mSName, runinfo->mSLine, command->mArgs[0]);
                return FALSE;
            }

            if(command->mArgsFlags[i] & ARG_ENV) {
                char* expanded_str;
                BOOL expand_double_dollar;
                if(!expand_env_to_command(ALLOC &expanded_str, command->mArgs[i], command, runinfo, nextin, &expand_double_dollar)) {
                    return FALSE;
                }

                if(expand_double_dollar) {
                    err_msg("invalid expanding varibale block", runinfo->mSName, runinfo->mSLine, command->mArgs[0]);
                    FREE(expanded_str);
                    return FALSE;
                }

                expanded_str[0] = '-';

                if(external_command) {
                    sCommand_add_arg_to_command2(command, MANAGED expanded_str);
                }
                else {
                    if(!get_option(MANAGED expanded_str, command, fun, nextin, nextout, runinfo, &i)) {
                        return FALSE;
                    }
                }
            }
            else {
                if(external_command) {
                    char* arg = MALLOC(strlen(command->mArgs[i]) + 1);
                    arg[0] = '-';
                    strcpy(arg + 1, command->mArgs[i] + 1);
                    sCommand_add_arg_to_command2(command, MANAGED arg);
                }
                else {
                    char* arg = MALLOC(strlen(command->mArgs[i]) + 1);
                    arg[0] = '-';
                    strcpy(arg + 1, command->mArgs[i] + 1);
                    if(!get_option(MANAGED arg, command, fun, nextin, nextout, runinfo, &i)) {
                        return FALSE;
                    }
                }
            }
        }
        /// variable ///
        else if(command->mArgsFlags[i] & ARG_ENV) {
            /// variable and glob ///
            if(command->mArgsFlags[i] & ARG_GLOB) {
                char* expanded_str;
                BOOL expand_double_dollar;
                if(!expand_env_to_command(ALLOC &expanded_str, command->mArgs[i], command, runinfo, nextin, &expand_double_dollar)) {
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
                    sCommand_add_arg_to_command2(command, MANAGED head);
                    char** p = tails;
                    while(*p) {
                        sCommand_add_arg_to_command2(command, MANAGED *p);
                        p++;
                    }
                    FREE(tails);
                }

                FREE(expanded_str);
            }
            else {
                char* expanded_str;
                BOOL expand_double_dollar;
                if(!expand_env_to_command(ALLOC &expanded_str, command->mArgs[i], command, runinfo, nextin, &expand_double_dollar)) {
                    return FALSE;
                }

                if(expand_double_dollar) {
                    if(expanded_str[0] == 0) {
                        FREE(expanded_str);
                    }
                    else {
                        sCommand_add_arg_to_command2(command, MANAGED expanded_str);
                    }
                }
                else {
                    sCommand_add_arg_to_command2(command, MANAGED expanded_str);
                }
            }
        }
        /// glob
        else if(command->mArgsFlags[i] & ARG_GLOB) {
            char* head;
            char** tails;
            if(!expand_glob(command->mArgs[i], ALLOC &head, ALLOC &tails, command, runinfo->mSName, runinfo->mSLine))
            {
                return FALSE;
            }
            sCommand_add_arg_to_command2(command, MANAGED head);
            char** p = tails;
            while(*p) {
                sCommand_add_arg_to_command2(command, MANAGED *p);
                p++;
            }
            FREE(tails);
        }
        /// normal ///
        else {
            sCommand_add_arg_to_command2(command, MANAGED STRDUP(command->mArgs[i]));
        }
    }

    return TRUE;
}

BOOL sCommand_expand_env_redirect(sCommand* command, sObject* nextin, sRunInfo* runinfo)
{
    if(command->mRedirectsNum > 0) {
        command->mRedirectsFileNamesRuntime = MALLOC(sizeof(char*)*command->mRedirectsNum);

        int i;
        for(i=0; i<command->mRedirectsNum; i++) {
            if(command->mRedirects[i] & REDIRECT_ENV) {
                char* expanded_str;
                BOOL expand_double_dollar;
                if(!expand_env_to_command(ALLOC &expanded_str, command->mRedirectsFileNames[i], command, runinfo, nextin, &expand_double_dollar)) {
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
                    command->mRedirectsFileNamesRuntime[i] = MANAGED head;
                    char** p = tails;
                    while(*p) {
                        FREE(*p);
                        p++;
                    }
                    FREE(tails);

                    FREE(expanded_str);
                }
                else {
                    command->mRedirectsFileNamesRuntime[i] = MANAGED expanded_str;
                }
            }
            else if(command->mRedirects[i] & REDIRECT_GLOB) {
                char* head;
                char** tails;
                if(!expand_glob(command->mRedirectsFileNames[i], ALLOC &head, ALLOC &tails, command, runinfo->mSName, runinfo->mSLine))
                {
                    return FALSE;
                }
                command->mRedirectsFileNamesRuntime[i] = MANAGED head;
                char** p = tails;
                while(*p) {
                    FREE(*p);
                    p++;
                }
                FREE(tails);
            }
            else {
                command->mRedirectsFileNamesRuntime[i] = STRDUP(command->mRedirectsFileNames[i]);
            }
        }
    }
    else {
        command->mRedirectsFileNamesRuntime = NULL;
    }

    return TRUE;
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
                if(SBLOCK(self).mStatments[i].mCommands[j].mEnvs[k].mKind == kEnvBlock) {
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

void sCommand_add_env(sCommand* self, MANAGED char* name, MANAGED char* key, BOOL key_env, BOOL double_dollar)
{
    if(self->mEnvsNum >= self->mEnvsSize) {
        if(self->mEnvs == NULL) {
            self->mEnvsSize = 1;
            self->mEnvs = MALLOC(sizeof(sEnv)*self->mEnvsSize);
        }
        else {
            const int env_size = self->mEnvsSize;
            self->mEnvsSize *= 2;
            self->mEnvs = REALLOC(self->mEnvs, sizeof(sEnv)*self->mEnvsSize);

        }
    }

    self->mEnvs[self->mEnvsNum].mKind = kEnv;
    self->mEnvs[self->mEnvsNum].mName = MANAGED name;
    self->mEnvs[self->mEnvsNum].mKey = MANAGED key;
    self->mEnvs[self->mEnvsNum].mKeyEnv = key_env;
    self->mEnvs[self->mEnvsNum].mDoubleDollar = double_dollar;
    self->mEnvsNum++;
}

void sCommand_add_env_block(sCommand* self, sObject* block, BOOL double_dollar, eLineField lf)
{
    if(self->mEnvsNum >= self->mEnvsSize) {
        if(self->mEnvs == NULL) {
            self->mEnvsSize = 1;
            self->mEnvs = MALLOC(sizeof(sEnv)*self->mEnvsSize);
        }
        else {
            const int env_size = self->mEnvsSize;
            self->mEnvsSize *= 2;
            self->mEnvs = REALLOC(self->mEnvs, sizeof(sEnv)*self->mEnvsSize);

        }
    }

    self->mEnvs[self->mEnvsNum].mKind = kEnvBlock;
    self->mEnvs[self->mEnvsNum].mBlock = block_clone_stack(block, T_BLOCK);
    self->mEnvs[self->mEnvsNum].mDoubleDollar = double_dollar;
    self->mEnvs[self->mEnvsNum].mLineField = lf;

    self->mEnvsNum++;
}

