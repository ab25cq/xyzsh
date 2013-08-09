#include "config.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <assert.h>
#include "xyzsh.h"

static int gPoolSize;
static sObject** gPool;
static sObject* gFreeObjectHead;
sObject* gRootObject;
sObject* gCompletionObject;
sObject* gXyzshObject;
sObject* gCurrentObject;
sObject* gStackFrames;

#define SLOT_SIZE 64

static void object_delete(sObject* obj)
{
    switch(STYPE(obj)) {
    case T_STRING:
        string_delete_on_gc(obj);
        break;

    case T_VECTOR:
        vector_delete_on_gc(obj);
        break;

    case T_HASH:
        hash_delete_on_gc(obj);
        break;

    case T_LIST:
        list_delete_on_gc(obj);
        break;

    case T_BLOCK:
        block_delete_on_gc(obj);
        break;
        
    case T_COMPLETION:
        break;

    case T_CLASS:
        class_delete_on_gc(obj);
        break;

    case T_FUN:
        fun_delete_on_gc(obj);
        break;

    case T_NFUN:
        nfun_delete_on_gc(obj);
        break;

    case T_FD:
        fprintf(stderr, "unexpected err on object_delete at gc.c");
        exit(1);

    case T_JOB:
        job_delete_on_gc(obj);
        break;

    case T_UOBJECT:
        uobject_delete_on_gc(obj);
        break;

    case T_EXTOBJ:
        SEXTOBJ(obj).mFreeFun(SEXTOBJ(obj).mObject);
        break;

    case T_EXTPROG:
        external_prog_delete_on_gc(obj);
        break;

/*
    case T_ALIAS:
        alias_delete_on_gc(obj);
        break;
*/
    }
}

int object_gc_children_mark(sObject* self)
{
    int count = 0;

    switch(STYPE(self)) {
    case T_VECTOR:
        count += vector_gc_children_mark(self);
        break;

    case T_HASH:
        count += hash_gc_children_mark(self);
        break;

    case T_LIST:
        count += list_gc_children_mark(self);
        break;

    case T_UOBJECT:
        count += uobject_gc_children_mark(self);
        break;

    case T_CLASS:
        count += class_gc_children_mark(self);
        break;

    case T_FUN:
        count += fun_gc_children_mark(self);
        break;

    case T_NFUN:
        count += nfun_gc_children_mark(self);
        break;

    case T_BLOCK:
        count += block_gc_children_mark(self);
        break;

    case T_COMPLETION:
        count += completion_gc_children_mark(self);
        break;

    case T_EXTOBJ:
        count += SEXTOBJ(self).mMarkFun(self);
        break;

    case T_ALIAS:
        count += alias_gc_children_mark(self);
        break;
    }

    return count;
}

static int sweep()
{
    int count = 0;
    int i;
    for(i=0; i<gPoolSize; i++) {
        sObject* p = gPool[i];
        int j;
        for(j=0; j<SLOT_SIZE; j++) {
           if(STYPE(p + j) != 0 && !IS_MARKED(p + j)) {
                object_delete(p + j);

                memset(p + j, 0, sizeof(sObject));

                sObject* next = gFreeObjectHead;
                gFreeObjectHead = p + j;
                gFreeObjectHead->mNextFreeObject = next;
                count++;
            }
        }
    }

    return count;
}

BOOL gc_valid_object(sObject* object)
{
    int i;
    for(i=0; i<gPoolSize; i++) {
        if(object >= gPool[i] && object < gPool[i] + SLOT_SIZE) {
            unsigned long n = ((unsigned long)object - (unsigned long)gPool[i]) % (unsigned long)sizeof(sObject);
            if(n == 0) {
                if(STYPE(object) > 0 && STYPE(object) < T_TYPE_MAX) {
                    return TRUE;
                }
                else {
                    return FALSE;
                }

            }
            else {
                return FALSE;
            }
        }
    }

    return FALSE;
}

sObject* gInheritMethod2;

void gc_init(int pool_size)
{
    gPoolSize = pool_size;
    gPool = (sObject**)MALLOC(sizeof(sObject*)*gPoolSize);
    memset(gPool, 0, sizeof(sObject*)*gPoolSize);
    int i;
    for(i=0; i<gPoolSize; i++) {
        gPool[i] = (sObject*)MALLOC(sizeof(sObject)*SLOT_SIZE);
        memset(gPool[i], 0, sizeof(sObject)*SLOT_SIZE);
    }

    /// link to a next free object
    gFreeObjectHead = gPool[0];

    for(i=0; i<gPoolSize-1; i++) {
        sObject* p = gPool[i];

        int j;
        for(j=0; j<SLOT_SIZE-1; j++) {
            p[j].mNextFreeObject = p + j + 1;
        }

        p[j].mNextFreeObject = gPool[i+1];
    }

    sObject* p = gPool[i];

    int j;
    for(j=0; j<SLOT_SIZE-1; j++) {
        p[j].mNextFreeObject = p + j + 1;
    }

    p[j].mNextFreeObject = NULL;

    gRootObject = UOBJECT_NEW_GC(8, NULL, "root", FALSE);
    uobject_init(gRootObject);
    uobject_root_init(gRootObject);
    gCurrentObject = gRootObject;

    sObject* xyzsh_object = UOBJECT_NEW_GC(8, NULL, "xyzsh", TRUE);
    uobject_init(xyzsh_object);
    xyzsh_object_init(xyzsh_object);
    gInheritMethod2 = uobject_item(xyzsh_object, "inherit");
    uobject_put(gRootObject, "xyzsh", xyzsh_object);

    gCompletionObject = UOBJECT_NEW_GC(8, gRootObject, "compl", FALSE);
    uobject_init(gCompletionObject);
    uobject_put(gRootObject, "compl", gCompletionObject);

    gXyzshObject = UOBJECT_NEW_GC(8, NULL, "xyzsh", FALSE);
    gStackFrames = VECTOR_NEW_GC(8, FALSE);
    uobject_put(gXyzshObject, "_stackframes", gStackFrames);
}

void gc_final()
{
    /// sweep all user objects ///
    uobject_clear(gRootObject);
    uobject_clear(gXyzshObject);
    if(gAppType == kATConsoleApp) {
        printf("sweeped %d objects...\n", gc_sweep());
    }

    int i;
    for(i=0; i<gPoolSize; i++) {
        sObject* p = gPool[i];
        int j;
        for(j=0; j<SLOT_SIZE; j++) {
            if(STYPE(p + j)) {
                object_delete(p + j);
            }
        }
        FREE(gPool[i]);
    }
    FREE(gPool);
}

static int mark()
{
    int i;
    /// clear mark flag
    for(i=0; i<gPoolSize; i++) {
        sObject* p = gPool[i];
        int j;
        for(j=0; j<SLOT_SIZE; j++) {
            CLEAR_MARK(p + j);
        }
    }
    
    /// mark all objects
    int count = 0;
    SET_MARK(gRootObject);
    count++;
    count += uobject_gc_children_mark(gRootObject);

    SET_MARK(gXyzshObject);
    count++;
    count += uobject_gc_children_mark(gXyzshObject);

    return count;
}

int gc_sweep()
{
    (void)mark();
    return sweep();
}

static void make_new_pool()
{
    int new_pool_size = gPoolSize * 2;
    gPool = (sObject**)REALLOC(gPool, sizeof(sObject*)*new_pool_size);
    memset(gPool + gPoolSize, 0, sizeof(sObject*)*(new_pool_size-gPoolSize));
    int i;
    for(i=gPoolSize; i<new_pool_size; i++) {
        gPool[i] = (sObject*)MALLOC(sizeof(sObject)*SLOT_SIZE);
        memset(gPool[i], 0, sizeof(sObject)*SLOT_SIZE);
    }

    gFreeObjectHead = gPool[gPoolSize];

    for(i=gPoolSize; i<new_pool_size-1; i++) {
        sObject* p = gPool[i];

        int j;
        for(j=0; j<SLOT_SIZE-1; j++) {
            p[j].mNextFreeObject = p + j + 1;
        }

        p[j].mNextFreeObject = gPool[i+1];
    }

    sObject* p = gPool[i];

    int j;
    for(j=0; j<SLOT_SIZE-1; j++) {
        p[j].mNextFreeObject = p + j + 1;
    }

    p[j].mNextFreeObject = NULL;

    gPoolSize = new_pool_size;
}

sObject* gc_get_free_object(int kind, BOOL user_object)
{
    if(gFreeObjectHead == NULL) {
        if(gFreeObjectHead == NULL) {
            make_new_pool();
        }
    }

    if(gFreeObjectHead == NULL) {
        fprintf(stderr, "no object exists\n");
        exit(1);
    }

    sObject* result = gFreeObjectHead;
    gFreeObjectHead = gFreeObjectHead->mNextFreeObject;

    if(user_object) {
        SET_USER_OBJECT(result);
    }

    result->mFlags |= kind;

    return result;
}

int gc_free_objects_num()
{
    int i=0;
    sObject* p = gFreeObjectHead;
    while(p) {
        p = p->mNextFreeObject;
        i++;
    }

    return i;
}

int gc_used_objects_num()
{
    //return SLOT_SIZE*gPoolSize - gc_free_objects_num();
    return mark(NULL, NULL);
}

int gc_slot_size()
{
    return SLOT_SIZE;
}

int gc_pool_size()
{
    return gPoolSize;
}
