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

sObject* class_new_on_gc(sObject* parent, BOOL user_object, BOOL no_stackframe)
{
   sObject* self = gc_get_free_object(T_CLASS, user_object);
   
   SCLASS(self).mBlock = NULL;

   SCLASS(self).mParent = parent;

   SCLASS(self).mLocalObjects = NULL;
   SCLASS(self).mArgBlocks = NULL;
   SCLASS(self).mOptions = MALLOC(sizeof(option_hash_it)*XYZSH_OPTION_MAX);
   memset(SCLASS(self).mOptions, 0, sizeof(option_hash_it)*XYZSH_OPTION_MAX);

   SCLASS(self).mFlags = no_stackframe ? FUN_FLAGS_NO_STACKFRAME : 0;

   return self;
}


sObject* class_clone_on_gc_from_stack_block(sObject* block, BOOL user_object, sObject* parent, BOOL no_stackframe)
{
   sObject* self = gc_get_free_object(T_CLASS, user_object);
   
   SCLASS(self).mBlock = block_clone_on_gc(block, T_BLOCK, FALSE);

   SCLASS(self).mParent = parent;

   SCLASS(self).mLocalObjects = NULL;
   SCLASS(self).mArgBlocks = NULL;
   SCLASS(self).mOptions = MALLOC(sizeof(option_hash_it)*XYZSH_OPTION_MAX);
   memset(SCLASS(self).mOptions, 0, sizeof(option_hash_it)*XYZSH_OPTION_MAX);
   SCLASS(self).mFlags = no_stackframe ? FUN_FLAGS_NO_STACKFRAME : 0;

   return self;
}

sObject* class_clone_on_stack_from_stack_block(sObject* block, sObject* parent, BOOL no_stackframe)
{
   sObject* self = stack_get_free_object(T_CLASS);
   
   SCLASS(self).mBlock = block_clone_on_stack(block, T_BLOCK);

   SCLASS(self).mParent = parent;

   SCLASS(self).mLocalObjects = NULL;
   SCLASS(self).mArgBlocks = NULL;
   SCLASS(self).mOptions = MALLOC(sizeof(option_hash_it)*XYZSH_OPTION_MAX);
   memset(SCLASS(self).mOptions, 0, sizeof(option_hash_it)*XYZSH_OPTION_MAX);

   SCLASS(self).mFlags = no_stackframe ? FUN_FLAGS_NO_STACKFRAME : 0;

   return self;
}

void class_delete_on_gc(sObject* self)
{
    int i;
    for(i=0; i<XYZSH_OPTION_MAX; i++) {
        if(SCLASS(self).mOptions[i].mKey) { FREE(SCLASS(self).mOptions[i].mKey); }
        if(SCLASS(self).mOptions[i].mArg) { FREE(SCLASS(self).mOptions[i].mArg); }
    }
    FREE(SCLASS(self).mOptions);
}

BOOL class_put_option_with_argument(sObject* self, MANAGED char* key)
{
    int hash_value = options_hash_fun(key);

    option_hash_it* p = SCLASS(self).mOptions + hash_value;
    while(1) {
        if(p->mKey) {
            p++;
            if(p == SCLASS(self).mOptions + hash_value) {
                return FALSE;
            }
            else if(p == SCLASS(self).mOptions + XYZSH_OPTION_MAX) {
                p = SCLASS(self).mOptions;
            }
        }
        else {
            p->mKey = MANAGED key;
            return TRUE;
        }
    }
}

BOOL class_option_with_argument(sObject* self, char* key)
{
    int hash_value = options_hash_fun(key);
    option_hash_it* p = SCLASS(self).mOptions + hash_value;

    while(1) {
        if(p->mKey) {
            if(strcmp(p->mKey, key) == 0) {
                return TRUE;
            }
            else {
                p++;
                if(p == SCLASS(self).mOptions + hash_value) {
                    return FALSE;
                }
                else if(p == SCLASS(self).mOptions + XYZSH_OPTION_MAX) {
                    p = SCLASS(self).mOptions;
                }
            }
        }
        else {
            return FALSE;
        }
    }
}

void class_delete_on_stack(sObject* self)
{
    int i;
    for(i=0; i<XYZSH_OPTION_MAX; i++) {
        if(SCLASS(self).mOptions[i].mKey) { FREE(SCLASS(self).mOptions[i].mKey); }
        if(SCLASS(self).mOptions[i].mArg) { FREE(SCLASS(self).mOptions[i].mArg); }
    }
    FREE(SCLASS(self).mOptions);
}

int class_gc_children_mark(sObject* self)
{
    int count = 0;

    sObject* block = SCLASS(self).mBlock;
    if(block) {
        if(IS_MARKED(block) == 0) {
            SET_MARK(block);
            count++;
            count += object_gc_children_mark(block);
        }
    }

    sObject* parent = SCLASS(self).mParent;
    if(parent) {
        if(IS_MARKED(parent) == 0) {
            SET_MARK(parent);
            count++;
            count += object_gc_children_mark(parent);
        }
    }

    sObject* lobjects = SCLASS(self).mLocalObjects;
    if(lobjects) {
        if(IS_MARKED(lobjects) == 0) {
            SET_MARK(lobjects);
            count++;
            count += object_gc_children_mark(lobjects);
        }
    }

    sObject* arg_blocks = SCLASS(self).mArgBlocks;
    if(arg_blocks) {
        if(IS_MARKED(arg_blocks) == 0) {
            SET_MARK(arg_blocks);
            count++;
            count += object_gc_children_mark(arg_blocks);
        }
    }

    return count;
}
