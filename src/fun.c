#include "config.h"
#include "xyzsh/xyzsh.h"
#include <string.h>
#include <stdio.h>

static int options_hash_fun(char* key)
{
    int value = 0;
    while(*key) {
        value += *key;
        key++;
    }
    return value % XYZSH_OPTION_MAX;
}

sObject* fun_new_on_gc(sObject* parent, BOOL user_object, BOOL no_stackframe)
{
   sObject* self = gc_get_free_object(T_FUN, user_object);
   
   SFUN(self).mBlock = NULL;

   SFUN(self).mParent = parent;

   SFUN(self).mLocalObjects = NULL;

   SFUN(self).mArgBlocks = NULL;

   SFUN(self).mOptions = MALLOC(sizeof(option_hash_it)*XYZSH_OPTION_MAX);
   memset(SFUN(self).mOptions, 0, sizeof(option_hash_it)*XYZSH_OPTION_MAX);

   SFUN(self).mFlags = no_stackframe ? FUN_FLAGS_NO_STACKFRAME : 0;

   return self;
}

sObject* fun_new_on_stack(sObject* parent)
{
   sObject* self = stack_get_free_object(T_FUN);
   
   SFUN(self).mBlock = NULL;

   SFUN(self).mParent = parent;

   SFUN(self).mLocalObjects = NULL;
   SFUN(self).mArgBlocks = NULL;

   SFUN(self).mOptions = MALLOC(sizeof(option_hash_it)*XYZSH_OPTION_MAX);
   memset(SFUN(self).mOptions, 0, sizeof(option_hash_it)*XYZSH_OPTION_MAX);

   SFUN(self).mFlags = 0;

   return self;
}

sObject* fun_clone_on_gc_from_stack_block(sObject* block, BOOL user_object, sObject* parent, BOOL no_stackframe)
{
   sObject* self = gc_get_free_object(T_FUN, user_object);
   
   SFUN(self).mBlock = block_clone_on_gc(block, T_BLOCK, FALSE);

   SFUN(self).mParent = parent;

   SFUN(self).mLocalObjects = NULL;
   SFUN(self).mArgBlocks = NULL;

   SFUN(self).mOptions = MALLOC(sizeof(option_hash_it)*XYZSH_OPTION_MAX);
   memset(SFUN(self).mOptions, 0, sizeof(option_hash_it)*XYZSH_OPTION_MAX);

   SFUN(self).mFlags = no_stackframe ? FUN_FLAGS_NO_STACKFRAME : 0;

   return self;
}

sObject* fun_clone_on_stack_from_stack_block(sObject* block, sObject* parent, BOOL no_stackframe)
{
   sObject* self = stack_get_free_object(T_FUN);
   
   SFUN(self).mBlock = block_clone_on_stack(block, T_BLOCK);

   SFUN(self).mParent = parent;

   SFUN(self).mLocalObjects = NULL;
   SFUN(self).mArgBlocks = NULL;

   SFUN(self).mOptions = MALLOC(sizeof(option_hash_it)*XYZSH_OPTION_MAX);
   memset(SFUN(self).mOptions, 0, sizeof(option_hash_it)*XYZSH_OPTION_MAX);

   SFUN(self).mFlags = no_stackframe ? FUN_FLAGS_NO_STACKFRAME : 0;

   return self;
}

void fun_delete_on_gc(sObject* self)
{
    int i;
    for(i=0; i<XYZSH_OPTION_MAX; i++) {
        if(SFUN(self).mOptions[i].mKey) { FREE(SFUN(self).mOptions[i].mKey); }
        if(SFUN(self).mOptions[i].mArg) { FREE(SFUN(self).mOptions[i].mArg); }
    }
    FREE(SFUN(self).mOptions);
}

void fun_delete_on_stack(sObject* self)
{
    int i;
    for(i=0; i<XYZSH_OPTION_MAX; i++) {
        if(SFUN(self).mOptions[i].mKey) { FREE(SFUN(self).mOptions[i].mKey); }
        if(SFUN(self).mOptions[i].mArg) { FREE(SFUN(self).mOptions[i].mArg); }
    }
    FREE(SFUN(self).mOptions);
}

int fun_gc_children_mark(sObject* self)
{
    int count = 0;

    sObject* block = SFUN(self).mBlock;
    if(block) {
        if(IS_MARKED(block) == 0) {
            SET_MARK(block);
            count++;
            count += object_gc_children_mark(block);
        }
    }

    sObject* parent = SFUN(self).mParent;
    if(parent) {
        if(IS_MARKED(parent) == 0) {
            SET_MARK(parent);
            count++;
            count += object_gc_children_mark(parent);
        }
    }

    sObject* lobjects = SFUN(self).mLocalObjects;
    if(lobjects) {
        if(IS_MARKED(lobjects) == 0) {
            SET_MARK(lobjects);
            count++;
            count += object_gc_children_mark(lobjects);
        }
    }

    sObject* arg_blocks = SFUN(self).mArgBlocks;
    if(arg_blocks) {
        if(IS_MARKED(arg_blocks) == 0) {
            SET_MARK(arg_blocks);
            count++;
            count += object_gc_children_mark(arg_blocks);
        }
    }

    return count;
}

BOOL fun_put_option_with_argument(sObject* self, MANAGED char* key)
{
    int hash_value = options_hash_fun(key);

    option_hash_it* p = SFUN(self).mOptions + hash_value;
    while(1) {
        if(p->mKey) {
            p++;
            if(p == SFUN(self).mOptions + hash_value) {
                return FALSE;
            }
            else if(p == SFUN(self).mOptions + XYZSH_OPTION_MAX) {
                p = SFUN(self).mOptions;
            }
        }
        else {
            p->mKey = MANAGED key;
            return TRUE;
        }
    }
}

BOOL fun_option_with_argument(sObject* self, char* key)
{
    int hash_value = options_hash_fun(key);
    option_hash_it* p = SFUN(self).mOptions + hash_value;

    while(1) {
        if(p->mKey) {
            if(strcmp(p->mKey, key) == 0) {
                return TRUE;
            }
            else {
                p++;
                if(p == SFUN(self).mOptions + hash_value) {
                    return FALSE;
                }
                else if(p == SFUN(self).mOptions + XYZSH_OPTION_MAX) {
                    p = SFUN(self).mOptions;
                }
            }
        }
        else {
            return FALSE;
        }
    }
}

