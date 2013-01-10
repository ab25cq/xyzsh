#include "config.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "xyzsh/xyzsh.h"

static list_it* list_it_release(list_it* it)
{
   FREE(it);
}

static list_it* list_it_allocate(void* item, list_it* prev_it, list_it* next_it)
{
   list_it* it = (list_it*)MALLOC(sizeof(list_it));
      
   it->mItem = item;
   it->mPrevIt = prev_it;
   it->mNextIt = next_it;
   
   return it;
}

#if defined(MDEBUG)

sObject* list_new_debug_from_malloc(const char* fname, int line, const char* func_name)
{
   sObject* self = (sObject*)CheckMemLeak_Malloc(sizeof(sObject), fname, line, func_name);
   
   SLIST(self).mEntryIt = NULL;
   SLIST(self).mLastIt = NULL;
   SLIST(self).mCount = 0;

   return self;
}

#else

sObject* list_new_from_malloc()
{
    sObject* self = (sObject*)MALLOC(sizeof(sObject));
    
    SLIST(self).mEntryIt = NULL;
    SLIST(self).mLastIt = NULL;
    SLIST(self).mCount = 0;
 
    return self;
}
#endif

sObject* list_new_from_gc(BOOL user_object)
{
    sObject* self = gc_get_free_object(T_LIST, user_object);
    
    SLIST(self).mEntryIt = NULL;
    SLIST(self).mLastIt = NULL;
    SLIST(self).mCount = 0;
 
    return self;
}

sObject* list_new_from_stack()
{
    sObject* self = stack_get_free_object(T_LIST);
    
    SLIST(self).mEntryIt = NULL;
    SLIST(self).mLastIt = NULL;
    SLIST(self).mCount = 0;
 
    return self;
}

void list_delete_malloc(sObject* self)
{
   list_it* it = SLIST(self).mEntryIt;
   while(it) {
      list_it* next_it = it->mNextIt;
      list_it_release(it);
      it = next_it;
   }

   FREE(self);
}

void list_delete_gc(sObject* self)
{
   list_it* it = SLIST(self).mEntryIt;
   while(it) {
      list_it* next_it = it->mNextIt;
      list_it_release(it);
      it = next_it;
   }
}

void list_delete_stack(sObject* self)
{
   list_it* it = SLIST(self).mEntryIt;
   while(it) {
      list_it* next_it = it->mNextIt;
      list_it_release(it);
      it = next_it;
   }
}

int list_count(sObject* self)
{
    return SLIST(self).mCount;
}

BOOL list_empty(sObject* self)
{
    return SLIST(self).mEntryIt == NULL;
}

int list_index_of(sObject* self, void* item)
{
   int i = 0;
   list_it* it = SLIST(self).mEntryIt;
   
   while(it) {
      if(it->mItem == item) return i;
      it = it->mNextIt;
      i++;
   }
   
   return -1;
}

void list_push_front(sObject* self, void* item)
{
   list_it* new_it = list_it_allocate(item, NULL, SLIST(self).mEntryIt);
   if(SLIST(self).mEntryIt) SLIST(self).mEntryIt->mPrevIt = new_it;
   if(SLIST(self).mEntryIt == NULL) SLIST(self).mLastIt = new_it;
   SLIST(self).mEntryIt = new_it;
   
   SLIST(self).mCount++;
}
         
void* list_pop_front(sObject* self)
{
   void* result;
   list_it* new_entry;

   if(SLIST(self).mEntryIt == NULL) return NULL;

   if(SLIST(self).mLastIt == SLIST(self).mEntryIt) SLIST(self).mLastIt = NULL;
   
   result = SLIST(self).mEntryIt->mItem;
   
   new_entry = SLIST(self).mEntryIt->mNextIt;
   if(new_entry) new_entry->mPrevIt = NULL;
   
   list_it_release(SLIST(self).mEntryIt);
   SLIST(self).mEntryIt = new_entry;
   
   SLIST(self).mCount--;
   
   return result;
}

void list_push_back(sObject* self, void* item)
{
    list_it* new_it = list_it_allocate(item, SLIST(self).mLastIt, NULL);
    if(SLIST(self).mLastIt) SLIST(self).mLastIt->mNextIt = new_it;
    if(SLIST(self).mLastIt == NULL) SLIST(self).mEntryIt = new_it;
    SLIST(self).mLastIt = new_it;

    SLIST(self).mCount++;
}
         
void* list_pop_back(sObject* self)
{
    void* result;
    list_it* new_last;

    if(SLIST(self).mLastIt == NULL) return NULL;

    if(SLIST(self).mEntryIt == SLIST(self).mLastIt) SLIST(self).mEntryIt = NULL;

    result = SLIST(self).mLastIt->mItem;

    new_last = SLIST(self).mLastIt->mPrevIt;
    if(new_last) new_last->mNextIt = NULL;

    list_it_release(SLIST(self).mLastIt);
    SLIST(self).mLastIt = new_last;

    SLIST(self).mCount--;

    return result;
}

void list_clear(sObject* self)
{
   list_it* it = SLIST(self).mEntryIt;

   while(it) {
      list_it* next_it = it->mNextIt;
      list_it_release(it);
      it = next_it;
      }
   
   SLIST(self).mEntryIt = NULL;
   SLIST(self).mLastIt = NULL;
   SLIST(self).mCount = 0;
}

list_it* list_begin(sObject* self)
{
    return SLIST(self).mEntryIt;
}

list_it* list_last(sObject* self)
{
    return SLIST(self).mLastIt;
}

list_it* list_at(sObject* self, int index)
{
   int i = 0;
   list_it* it = SLIST(self).mEntryIt;
   while(it) {
      if(i == index) return it;
         
      it = it->mNextIt;
      i++;
   }
   
   return NULL;
}

list_it* list_find(sObject* self, void* item)
{
   list_it* it = SLIST(self).mEntryIt;
   while(it) {
      if(it->mItem == item) return it;
      it = it->mNextIt;
   }
   return NULL;
}

void* list_item(list_it* it)
{
    return it->mItem;
}

list_it* list_next(list_it* it)
{
    return it->mNextIt;
}

list_it* list_prev(list_it* it)
{
    return it->mPrevIt;
}

void list_replace(list_it* it, void* item)
{
    it->mItem = item;
}

list_it* list_erase(list_it* it, sObject* owner)
{
   if(it->mPrevIt) it->mPrevIt->mNextIt = it->mNextIt;
   if(it->mNextIt) it->mNextIt->mPrevIt = it->mPrevIt;
   
   if(SLIST(owner).mEntryIt == it) SLIST(owner).mEntryIt = it->mNextIt;
   if(SLIST(owner).mLastIt == it) SLIST(owner).mLastIt = it->mPrevIt;
   SLIST(owner).mCount--;

   list_it* ret = it->mNextIt;
   list_it_release(it);

   return ret;
}
      
void list_insert_front(list_it* it, void* item, sObject* owner)
{
   list_it* new_it = list_it_allocate(item, it->mPrevIt, it);
   if(it->mPrevIt) it->mPrevIt->mNextIt = new_it;
   it->mPrevIt = new_it;

   if(it == SLIST(owner).mEntryIt) SLIST(owner).mEntryIt = new_it;
   SLIST(owner).mCount++;
}

void list_insert_back(list_it* it, void* item, sObject* owner)
{
   list_it* new_it = list_it_allocate(item, it, it->mNextIt);
   if(it->mNextIt) it->mNextIt->mPrevIt = new_it;
   it->mNextIt = new_it;

   if(it == SLIST(owner).mLastIt) SLIST(owner).mLastIt = new_it;      
   SLIST(owner).mCount++;
}

int list_gc_children_mark(sObject* self)
{
    int count = 0;

    list_it* it;
    for(it= SLIST(self).mEntryIt; it; it=it->mNextIt) {
        sObject* item = it->mItem;

        if(item && IS_MARKED(item) == 0) {
            SET_MARK(item);
            count ++;
            count += object_gc_children_mark(item);
        }
    }

    return count;
}

