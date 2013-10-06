#include "config.h"
#include "xyzsh.h"
#include <string.h>
#include <stdio.h>

sObject* alias_new_on_gc(sObject* block, BOOL user_object)
{
   sObject* self = gc_get_free_object(T_ALIAS, user_object);
   
   SALIAS(self).mBlock = block_clone_on_gc(block, T_BLOCK, FALSE);
   SALIAS(self).mLocalObjects = NULL;
   SALIAS(self).mArgBlocks = NULL;
   SALIAS(self).mOptions = NULL;
   SALIAS(self).mFlags = 0;

   return self;
}

int alias_gc_children_mark(sObject* self)
{
    int count = 0;

    sObject* block = SALIAS(self).mBlock;
    if(block) {
        if(IS_MARKED(block) == 0) {
            SET_MARK(block);
            count++;
            count += object_gc_children_mark(block);
        }
    }

    sObject* lobjects = SALIAS(self).mLocalObjects;
    if(lobjects) {
        if(IS_MARKED(lobjects) == 0) {
            SET_MARK(lobjects);
            count++;
            count += object_gc_children_mark(lobjects);
        }
    }

    sObject* arg_blocks = SALIAS(self).mArgBlocks;
    if(arg_blocks) {
        if(IS_MARKED(arg_blocks) == 0) {
            SET_MARK(arg_blocks);
            count++;
            count += object_gc_children_mark(arg_blocks);
        }
    }

    return count;
}


