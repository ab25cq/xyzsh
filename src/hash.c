#include "config.h"
#include <stdio.h>
#include <string.h>
#include "xyzsh/xyzsh.h"

hash_it* hash_it_new(char* key, void* item, hash_it* coll_it, hash_it* next_it, sObject* hash) 
{
   hash_it* it = (hash_it*)MALLOC(sizeof(hash_it));

   it->mKey = STRDUP(key);

   it->mItem = item;
   it->mCollisionIt = coll_it;

   it->mNextIt = next_it;   

   return it;
}
   
void hash_it_release(hash_it* it, sObject* hash)
{
    FREE(it->mKey);
      
    FREE(it);
}
         
#ifndef MDEBUG
sObject* hash_new_on_malloc(int size)
{
   sObject* self = MALLOC(sizeof(sObject));

   self->mFlags = T_HASH;
   
   SHASH(self).mTableSize = size;
   SHASH(self).mTable = (hash_it**)MALLOC(sizeof(hash_it*) * size);
   memset(SHASH(self).mTable, 0, sizeof(hash_it*)*size);

   SHASH(self).mEntryIt = NULL;

   SHASH(self).mCounter = 0;

   return self;
}
#else
sObject* hash_new_on_malloc_debug(int size, const char* fname, int line, const char* func_name)
{
   sObject* self = CheckMemLeak_Malloc(sizeof(sObject), fname, line, func_name);

   self->mFlags = T_HASH;
   
   SHASH(self).mTableSize = size;
   SHASH(self).mTable = (hash_it**)MALLOC(sizeof(hash_it*) * size);
   memset(SHASH(self).mTable, 0, sizeof(hash_it*)*size);

   SHASH(self).mEntryIt = NULL;

   SHASH(self).mCounter = 0;

   return self;
}
#endif

sObject* hash_new_on_gc(int size, BOOL user_object)
{
   sObject* self = gc_get_free_object(T_HASH, user_object);
   
   SHASH(self).mTableSize = size;
   SHASH(self).mTable = (hash_it**)MALLOC(sizeof(hash_it*) * size);
   memset(SHASH(self).mTable, 0, sizeof(hash_it*)*size);

   SHASH(self).mEntryIt = NULL;

   SHASH(self).mCounter = 0;

   return self;
}

sObject* hash_new_on_stack(int size)
{
   sObject* self = stack_get_free_object(T_HASH);
   
   SHASH(self).mTableSize = size;
   SHASH(self).mTable = (hash_it**)MALLOC(sizeof(hash_it*) * size);
   memset(SHASH(self).mTable, 0, sizeof(hash_it*)*size);

   SHASH(self).mEntryIt = NULL;

   SHASH(self).mCounter = 0;

   return self;
}

void hash_delete_on_gc(sObject* self)
{
   ASSERT(TYPE(self) == T_HASH);

   hash_it* it = SHASH(self).mEntryIt;

   while(it) {
      hash_it* next_it = it->mNextIt;
      hash_it_release(it, self);
      it = next_it;
   }
   
   FREE(SHASH(self).mTable);
}

void hash_delete_on_stack(sObject* self)
{
   ASSERT(TYPE(self) == T_HASH);

   hash_it* it = SHASH(self).mEntryIt;

   while(it) {
      hash_it* next_it = it->mNextIt;
      hash_it_release(it, self);
      it = next_it;
   }
   
   FREE(SHASH(self).mTable);
}

void hash_delete_on_malloc(sObject* self)
{
   ASSERT(TYPE(self) == T_HASH);

   hash_it* it = SHASH(self).mEntryIt;

   while(it) {
      hash_it* next_it = it->mNextIt;
      hash_it_release(it, self);
      it = next_it;
   }
   
   FREE(SHASH(self).mTable);
   
   FREE(self);
}

static unsigned int get_hash_value(sObject* self, char* key)
{
   ASSERT(TYPE(self) == T_HASH);

   unsigned int i = 0;
   while(*key) {
      i += *key;
      key++;
   }

   return i % SHASH(self).mTableSize;
}

static void resize(sObject* self) 
{
   ASSERT(TYPE(self) == T_HASH);

    const int table_size = SHASH(self).mTableSize;

    SHASH(self).mTableSize *= 5;
    FREE(SHASH(self).mTable);
    SHASH(self).mTable = (hash_it**)MALLOC(sizeof(hash_it*) * SHASH(self).mTableSize);
    memset(SHASH(self).mTable, 0, sizeof(hash_it*)*SHASH(self).mTableSize);

    hash_it* it = SHASH(self).mEntryIt;
    while(it) {
        unsigned int hash_key = get_hash_value(self, it->mKey);

        it->mCollisionIt = SHASH(self).mTable[hash_key];
        
        SHASH(self).mTable[hash_key] = it;

        it = it->mNextIt;
    }
}

void hash_put(sObject* self, char* key, void* item)
{
   ASSERT(TYPE(self) == T_HASH);

    if(SHASH(self).mCounter >= SHASH(self).mTableSize) {
        resize(self);
    }
    
    unsigned int hash_key = get_hash_value(self, key);
    
    hash_it* it = SHASH(self).mTable[ hash_key ];
    while(it) {
        if(strcmp(key, it->mKey) == 0) {
            it->mItem = item;
            return;
        }
        it = it->mCollisionIt;
    }
    
    hash_it* new_it
        = hash_it_new(key, item, SHASH(self).mTable[hash_key], SHASH(self).mEntryIt, self);
    
    SHASH(self).mTable[hash_key] = new_it;
    SHASH(self).mEntryIt = new_it;
    SHASH(self).mCounter++;
}

static void erase_from_list(sObject* self, hash_it* rit)
{
   ASSERT(TYPE(self) == T_HASH);

   if(rit == SHASH(self).mEntryIt) {
      SHASH(self).mEntryIt = rit->mNextIt;
   }
   else {
      hash_it* it = SHASH(self).mEntryIt;

      while(it->mNextIt) {
         if(it->mNextIt == rit) {
            it->mNextIt = it->mNextIt->mNextIt;
            break;
         }
            
         it = it->mNextIt;
      }
   }
}

BOOL hash_erase(sObject* self, char* key)
{
   ASSERT(TYPE(self) == T_HASH);

   const unsigned int hash_value = get_hash_value(self, key);
   hash_it* it = SHASH(self).mTable[hash_value];
   
   if(it == NULL)
      ;
   else if(strcmp(it->mKey, key) == 0) {
      hash_it* it2 = SHASH(self).mEntryIt;   
      SHASH(self).mTable[ hash_value ] = it->mCollisionIt;

      erase_from_list(self, it);
      
      hash_it_release(it, self);

      SHASH(self).mCounter--;

      return TRUE;
   }
   else {
      hash_it* prev_it = it;
      it = it->mCollisionIt;
      while(it) {
         if(strcmp(it->mKey, key) == 0) {
            prev_it->mCollisionIt = it->mCollisionIt;
            erase_from_list(self, it);
            hash_it_release(it, self);
            SHASH(self).mCounter--;
            
            return TRUE;
         }
         
         prev_it = it;
         it = it->mCollisionIt;
      }
   }

   return FALSE;
}

void hash_clear(sObject* self)
{
   ASSERT(TYPE(self) == T_HASH);

   int i;
   int max;
   hash_it* it = SHASH(self).mEntryIt;

   while(it) {
      hash_it* next_it = it->mNextIt;
      hash_it_release(it, self);
      it = next_it;
   }

   memset(SHASH(self).mTable, 0, sizeof(hash_it*)*SHASH(self).mTableSize);
   
   SHASH(self).mEntryIt = NULL;
   SHASH(self).mCounter = 0;
}

void hash_replace(sObject* self, char* key, void* item)
{
   ASSERT(TYPE(self) == T_HASH);

   hash_it* it = SHASH(self).mTable[get_hash_value(self, key)];

   if(it) {
      do {
         if(strcmp(it->mKey, key) == 0) {
            it->mItem = item;
            break;
         }
   
         it = it->mCollisionIt;
      } while(it);
   }
}

void hash_show(sObject* self, char* fname)
{
   ASSERT(TYPE(self) == T_HASH);

   char tmp[8096];
   int i;
   int max;
   hash_it* it;
   xstrncpy(tmp, "", 8096);
   
   max = SHASH(self).mTableSize;
   for(i=0; i<max; i++) {
      snprintf(tmp + strlen(tmp), 8096-strlen(tmp), "table[%d]: ", i);
      
      it = SHASH(self).mTable[i];
      while(it) {
         snprintf(tmp + strlen(tmp), 8096-strlen(tmp), "item[\"%s\"], ", it->mKey);
         it = it->mCollisionIt;
      }
    
    xstrncat(tmp, "\n", 8096);
   }

    
    FILE* f = fopen(fname, "a");
    fprintf(f, "-------------------------------------\n");
    fprintf(f, "%s", tmp);
    fprintf(f, "-------------------------------------\n");
    it = SHASH(self).mEntryIt;
    while(it) {
        fprintf(f, "%s\n", it->mKey);
        it = it->mNextIt;
    }
    fprintf(f, "-------------------------------------\n");
    fclose(f);
}

void* hash_item(sObject* self, char* key)
{
   ASSERT(TYPE(self) == T_HASH);

   hash_it* it = SHASH(self).mTable[ get_hash_value(self, key) ];

   while(it) {
      if(strcmp(key, it->mKey) == 0) return it->mItem;
      it = it->mCollisionIt;
   }

   return NULL;
}

void* hash_item_addr(sObject* self, char* key)
{
   ASSERT(TYPE(self) == T_HASH);

   hash_it* it = SHASH(self).mTable[ get_hash_value(self, key) ];
   while(it) {
      if(strcmp(key, it->mKey) == 0) return &it->mItem;
      it = it->mCollisionIt;
   }

   return NULL;
}
   
char* hash_key(sObject* self, void* item)
{
   ASSERT(TYPE(self) == T_HASH);

   hash_it* it = SHASH(self).mEntryIt;

   while(it) {
      if(it->mItem == item) return it->mKey;
      it = it->mNextIt;
   }

   return NULL;
}

int hash_count(sObject* self)
{
   ASSERT(TYPE(self) == T_HASH);

   return SHASH(self).mCounter;
}   

hash_it* hash_loop_begin(sObject* self) 
{ 
   ASSERT(TYPE(self) == T_HASH);

    return SHASH(self).mEntryIt; 
}

void* hash_loop_item(hash_it* it) 
{ 
    return it->mItem; 
}

char* hash_loop_key(hash_it* it)
{ 
    return it->mKey; 
}

hash_it* hash_loop_next(hash_it* it) 
{ 
    return it->mNextIt; 
}

int hash_gc_children_mark(sObject* self)
{
   ASSERT(TYPE(self) == T_HASH);

    int count = 0;

    hash_it* it = SHASH(self).mEntryIt;
    while(it) {
        sObject* item = it->mItem;

        if(item && IS_MARKED(item) == 0) {
            SET_MARK(item);
            count++;
            count += object_gc_children_mark(item);
        }

        it = it->mNextIt;
    }

    return count;
}

