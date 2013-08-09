#include "config.h"
#include "xyzsh.h"
#include <string.h>
#include <stdio.h>

sObject* completion_new_on_gc(sObject* block, BOOL user_object)
{
   sObject* self = gc_get_free_object(T_COMPLETION, user_object);
   
   SCOMPLETION(self).mBlock = block_clone_on_gc(block, T_BLOCK, user_object);

   return self;
}

int completion_gc_children_mark(sObject* self)
{
    int count = 0;

    sObject* block = SCOMPLETION(self).mBlock;
    if(block) {
        if(IS_MARKED(block) == 0) {
            SET_MARK(block);
            count++;
            count += object_gc_children_mark(block);
        }
    }

    return count;
}


