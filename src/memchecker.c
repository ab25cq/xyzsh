#include "config.h"
#include "xyzsh/xyzsh.h"
#include <string.h>
#include <stdio.h>
#include <signal.h>

memchecker_it* memchecker_it_new(void* key, int item, memchecker_it* coll_it, memchecker_it* next_it, sObject* memchecker) 
{
   memchecker_it* it = (memchecker_it*)MALLOC(sizeof(memchecker_it));

   it->mKey = key;

   it->mItem = item;
   it->mCollisionIt = coll_it;

   it->mNextIt = next_it;   

   return it;
}
   
void memchecker_it_release(memchecker_it* it, sObject* memchecker)
{
   FREE(it);
}

sObject* memchecker_new_from_stack(int size)
{
   sObject* self = stack_get_free_object(T_MEMCHECKER);
   
   SMEMCHECKER(self).mTableSize = size;
   SMEMCHECKER(self).mTable = (memchecker_it**)MALLOC(sizeof(memchecker_it*) * size);
   memset(SMEMCHECKER(self).mTable, 0, sizeof(memchecker_it*)*size);

   SMEMCHECKER(self).mEntryIt = NULL;

   SMEMCHECKER(self).mCounter = 0;

   return self;
}

void memchecker_delete_stack(sObject* self)
{
   memchecker_it* it = SMEMCHECKER(self).mEntryIt;

   while(it) {
      memchecker_it* next_it = it->mNextIt;
      memchecker_it_release(it, self);
      it = next_it;
   }
   
   FREE(SMEMCHECKER(self).mTable);
}

static unsigned int get_hash_value(sObject* self, void* key)
{
    return ((unsigned long)key) % SMEMCHECKER(self).mTableSize;
}

static void resize(sObject* self) 
{
    const int table_size = SMEMCHECKER(self).mTableSize;

    SMEMCHECKER(self).mTableSize *= 5;
    FREE(SMEMCHECKER(self).mTable);
    SMEMCHECKER(self).mTable = (memchecker_it**)MALLOC(sizeof(memchecker_it*) * SMEMCHECKER(self).mTableSize);
    memset(SMEMCHECKER(self).mTable, 0, sizeof(memchecker_it*)*SMEMCHECKER(self).mTableSize);

    memchecker_it* it = SMEMCHECKER(self).mEntryIt;
    while(it) {
        unsigned int hash_key = get_hash_value(self, it->mKey);

        it->mCollisionIt = SMEMCHECKER(self).mTable[hash_key];
        
        SMEMCHECKER(self).mTable[hash_key] = it;

        it = it->mNextIt;
    }
}

void memchecker_put(sObject* self, void* key, int item)
{
    if(SMEMCHECKER(self).mCounter >= SMEMCHECKER(self).mTableSize) {
        resize(self);
    }
    
    unsigned int hash_key = get_hash_value(self, key);
    
    memchecker_it* it = SMEMCHECKER(self).mTable[ hash_key ];
//printf("counter %d table size %d hash_key %u\n", SMEMCHECKER(self).mCounter, SMEMCHECKER(self).mTableSize, hash_key);
    while(it) {
        if(key == it->mKey) {
            it->mItem = item;
            return;
        }
        it = it->mCollisionIt;
    }
    
    memchecker_it* new_it
        = memchecker_it_new(key, item, SMEMCHECKER(self).mTable[hash_key], SMEMCHECKER(self).mEntryIt, self);
    
    SMEMCHECKER(self).mTable[hash_key] = new_it;
    SMEMCHECKER(self).mEntryIt = new_it;
    SMEMCHECKER(self).mCounter++;
}

static void erase_from_list(sObject* self, memchecker_it* rit)
{
   if(rit == SMEMCHECKER(self).mEntryIt) {
      SMEMCHECKER(self).mEntryIt = rit->mNextIt;
   }
   else {
      memchecker_it* it = SMEMCHECKER(self).mEntryIt;

      while(it->mNextIt) {
         if(it->mNextIt == rit) {
            it->mNextIt = it->mNextIt->mNextIt;
            break;
         }
            
         it = it->mNextIt;
      }
   }
}

BOOL memchecker_erase(sObject* self, void* key)
{
   const unsigned int hash_value = get_hash_value(self, key);
   memchecker_it* it = SMEMCHECKER(self).mTable[hash_value];
   
   if(it == NULL)
      ;
   else if(it->mKey == key) {
      memchecker_it* it2 = SMEMCHECKER(self).mEntryIt;   
      SMEMCHECKER(self).mTable[ hash_value ] = it->mCollisionIt;

      erase_from_list(self, it);
      
      memchecker_it_release(it, self);

      SMEMCHECKER(self).mCounter--;

      return TRUE;
   }
   else {
      memchecker_it* prev_it = it;
      it = it->mCollisionIt;
      while(it) {
         if(it->mKey == key) {
            prev_it->mCollisionIt = it->mCollisionIt;
            erase_from_list(self, it);
            memchecker_it_release(it, self);
            SMEMCHECKER(self).mCounter--;
            
            return TRUE;
         }
         
         prev_it = it;
         it = it->mCollisionIt;
      }
   }

   return FALSE;
}

int memchecker_item(sObject* self, void* key)
{
   memchecker_it* it = SMEMCHECKER(self).mTable[ get_hash_value(self, key) ];

   while(it) {
      if(key == it->mKey) return it->mItem;
      it = it->mCollisionIt;
   }

   return -1;
}
