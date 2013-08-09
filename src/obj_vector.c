#include "config.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "xyzsh.h"

sObject* vector_new_on_gc(int first_size, BOOL user_object)
{
    sObject* self = (sObject*)gc_get_free_object(T_VECTOR, user_object);

    SVECTOR(self).mTable = (void**)MALLOC(sizeof(void*) * first_size);
    SVECTOR(self).mTableSize = first_size;
    SVECTOR(self).mCount = 0;
 
    return self;
}

sObject* vector_new_on_stack(int first_size)
{
    sObject* self = (sObject*)stack_get_free_object(T_VECTOR);
 
    SVECTOR(self).mTable = (void**)MALLOC(sizeof(void*) * first_size);
    SVECTOR(self).mTableSize = first_size;
    SVECTOR(self).mCount = 0;
 
    return self;
}

#ifndef MDEBUG

sObject* vector_new_on_malloc(int first_size)
{
    sObject* self = (sObject*)MALLOC(sizeof(sObject));

    self->mFlags = T_VECTOR;
 
    SVECTOR(self).mTable = (void**)MALLOC(sizeof(void*) * first_size);
    SVECTOR(self).mTableSize = first_size;
    SVECTOR(self).mCount = 0;
 
    return self;
}

#else

sObject* vector_new_on_malloc_debug(int first_size, const char* fname, int line, const char* func_name)
{
    sObject* self = debug_malloc(sizeof(sObject), fname, line, func_name);

    self->mFlags = T_VECTOR;
 
    SVECTOR(self).mTable = (void**)MALLOC(sizeof(void*) * first_size);
    SVECTOR(self).mTableSize = first_size;
    SVECTOR(self).mCount = 0;
 
    return self;
}

#endif

void vector_delete_on_malloc(sObject* self)
{
   FREE(SVECTOR(self).mTable);
   FREE(self);
}

void vector_delete_on_gc(sObject* self)
{
   FREE(SVECTOR(self).mTable);
}

void vector_delete_on_stack(sObject* self)
{
   FREE(SVECTOR(self).mTable);
}

void vector_exchange(sObject* self, int n, void* item)
{
   SVECTOR(self).mTable[n] = item;
}

void vector_add(sObject* self, void* item)
{
   if(SVECTOR(self).mCount == SVECTOR(self).mTableSize) {
      SVECTOR(self).mTableSize = SVECTOR(self).mTableSize * 2;

      SVECTOR(self).mTable = (void**)REALLOC(SVECTOR(self).mTable
                                , sizeof(void*)*SVECTOR(self).mTableSize);
   }

   SVECTOR(self).mTable[SVECTOR(self).mCount] = item;
   SVECTOR(self).mCount++;
}

void vector_insert(sObject* self, int n, void* item)
{
   if(SVECTOR(self).mCount == SVECTOR(self).mTableSize) {
      SVECTOR(self).mTableSize = SVECTOR(self).mTableSize * 2;

      SVECTOR(self).mTable = (void**)REALLOC(SVECTOR(self).mTable
                              , sizeof(void*)*SVECTOR(self).mTableSize);
   }

   memmove(SVECTOR(self).mTable +n +1, SVECTOR(self).mTable +n
                           , sizeof(void*)*(SVECTOR(self).mCount - n));
   
   SVECTOR(self).mTable[n] = item;
   SVECTOR(self).mCount++;
}

void vector_mass_insert(sObject* self, int n, void** items, int items_size)
{
   if(SVECTOR(self).mCount+items_size >= SVECTOR(self).mTableSize) {
      SVECTOR(self).mTableSize = (SVECTOR(self).mTableSize+items_size) * 2;

      SVECTOR(self).mTable = (void**)REALLOC(SVECTOR(self).mTable
                              , sizeof(void*)*SVECTOR(self).mTableSize);
   }

   memmove(SVECTOR(self).mTable +n +items_size, SVECTOR(self).mTable +n
                           , sizeof(void*)*(SVECTOR(self).mCount - n));
   
   memmove(SVECTOR(self).mTable + n, items, sizeof(void*)*items_size);
   SVECTOR(self).mCount+=items_size;
}

int vector_index(sObject* self, void* item)
{
    int i;
    for(i=0; i<SVECTOR(self).mCount; i++) {
        if(SVECTOR(self).mTable[i] == item) {
            return i;
        }
    }

    return -1;
}

void* vector_erase(sObject* self, int n)
{
    void* item = NULL;

    if(n>=0 && n<SVECTOR(self).mCount) {
        item = SVECTOR(self).mTable[n];
        memmove(SVECTOR(self).mTable + n, SVECTOR(self).mTable + n + 1, sizeof(void*)*(SVECTOR(self).mCount -n-1));

        SVECTOR(self).mCount--;
    }

    return item;
}

void* vector_pop_back(sObject* self)
{
    return vector_erase(self, SVECTOR(self).mCount-1);
}

void* vector_pop_front(sObject* self)
{
    return vector_erase(self, 0);
}

static BOOL quick_sort(sObject* self, int left, int right, sort_if fun)
{
    int i;
    int j;
    void* center_item;

    if(left < right) {
        center_item = SVECTOR(self).mTable[(left+right) / 2];

        i = left;
        j = right;

        do { 
            while(1) {
                int ret = fun(SVECTOR(self).mTable[i], center_item);
                if(ret < 0) return FALSE;
                if(SVECTOR(self).mTable[i]==center_item || !ret)
                {
                    break;
                }
                i++;
            }
                     
            while(1) {
                int ret = fun(center_item, SVECTOR(self).mTable[j]);
                if(ret < 0) return FALSE;
                if(center_item==SVECTOR(self).mTable[j] || !ret)
                {
                    break;
                }
                j--;
            }

            if(i <= j) {
                void* tmp = SVECTOR(self).mTable[i]; // swap
                SVECTOR(self).mTable[i] = SVECTOR(self).mTable[j];
                SVECTOR(self).mTable[j] = tmp;

                i++;
                j--;
            }
        } while(i <= j);

        if(!quick_sort(self, left, j, fun)) {
            return FALSE;
        }
        if(!quick_sort(self, i, right, fun)) {
            return FALSE;
        }
    }

    return TRUE;
}

void vector_sort(sObject* self, sort_if fun)
{
    quick_sort(self, 0, SVECTOR(self).mCount-1, fun);
}

void vector_clear(sObject* self)
{
    SVECTOR(self).mCount = 0;
}

int vector_gc_children_mark(sObject* self)
{
    int count = 0;
    int i;
    for(i=0; i<SVECTOR(self).mCount; i++) {
        sObject* item = SVECTOR(self).mTable[i];

        if(item && IS_MARKED(item) == 0) {
            SET_MARK(item);
            count++;

            count += object_gc_children_mark(item);
        }
    }

    return count;
}

