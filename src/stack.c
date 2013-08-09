#include "config.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "xyzsh.h"

static int gStackPageSize;
static sObject** gStack;

static int gStackPage;
static int gStackSlot;

#define SLOT_SIZE 32
#define MAX_STACK 256

static int gStackFrame;
static int gStackFramePage[MAX_STACK];
static int gStackFrameSlot[MAX_STACK];

static void object_delete(sObject* obj) 
{
    switch(STYPE(obj)) {
        case T_STRING:
            string_delete_on_stack(obj);
            break;

        case T_VECTOR:
            vector_delete_on_stack(obj);
            break;

        case T_LIST:
            list_delete_on_stack(obj);
            break;

        case T_HASH:
            hash_delete_on_stack(obj);
            break;

        case T_BLOCK:
            block_delete_on_stack(obj);
            break;

        case T_COMPLETION:
            break;

        case T_FD:
            fd_delete_on_stack(obj);
            break;

        case T_FD2:
             break;

        case T_FUN:
            fun_delete_on_stack(obj);
            break;

        case T_CLASS:
            class_delete_on_stack(obj);
            break;

        case T_UOBJECT:
            uobject_delete_on_stack(obj);
            break;

        case T_EXTOBJ:
            SEXTOBJ(obj).mFreeFun(SEXTOBJ(obj).mObject);
            break;

        case T_INT:
            break;

        default:
            fprintf(stderr,"unexpected err in object_delete stack\n");
            exit(1);
    }
}
/*
BOOL stack_valid_object(sObject* object)
{
    int i;
    for(i=0; i<gStackPageSize; i++) {
        if(object >= gStack[i] && object < gStack[i] + SLOT_SIZE) {
            return TRUE;
        }
    }

    return FALSE;
}
*/

void stack_init(int stack_page_size)
{
    gStackPageSize = stack_page_size;
    gStack = (sObject**)MALLOC(sizeof(sObject*)*gStackPageSize);
    int i;
    for(i=0; i<gStackPageSize; i++) {
        gStack[i] = MALLOC(sizeof(sObject)*SLOT_SIZE);
        memset(gStack[i], 0, sizeof(sObject)*SLOT_SIZE);
    }

    gStackSlot = 0;
    gStackPage = 0;

    gStackFrame = 0;
}

void stack_final()
{
    if(gStackFrame != 0) { fprintf(stderr, "stackframe level is %d\n", gStackFrame); }
    int i;
    for(i=0; i<gStackPageSize; i++) {
        FREE(gStack[i]);
    }
    FREE(gStack);
}

void stack_start_stack()
{
    gStackFramePage[gStackFrame] = gStackPage;
    gStackFrameSlot[gStackFrame] = gStackSlot;
    gStackFrame++;
}

void stack_end_stack()
{
    gStackFrame--;

    const int stack_frame_page = gStackFramePage[gStackFrame];
    const int stack_frame_slot = gStackFrameSlot[gStackFrame];

    if(stack_frame_page == gStackPage) {
        int i;
        for(i=stack_frame_slot; i<gStackSlot; i++) {
            sObject* obj = gStack[stack_frame_page] + i;
            object_delete(obj);
        }
        memset(gStack[stack_frame_page] + stack_frame_slot, 0, sizeof(sObject)*(gStackSlot-stack_frame_slot));

        gStackSlot = stack_frame_slot;
    }
    else {
        int i;
        for(i=0; i<gStackSlot; i++) {
            sObject* obj = gStack[gStackPage] + i;
            object_delete(obj);
        }
        memset(gStack[gStackPage], 0, sizeof(sObject)*gStackSlot);

        int j;
        for(j=stack_frame_page + 1; j<gStackPage; j++) {
            for(i=0; i<SLOT_SIZE; i++) {
                sObject* obj = gStack[j] + i;
                object_delete(obj);
            }
            memset(gStack[j], 0, sizeof(sObject)*SLOT_SIZE);
        }
        for(i=stack_frame_slot; i<SLOT_SIZE; i++) {
            sObject* obj = gStack[stack_frame_page] + i;
            object_delete(obj);
        }
        memset(gStack[stack_frame_page] + stack_frame_slot, 0, sizeof(sObject)*(SLOT_SIZE-stack_frame_slot));

        gStackPage = stack_frame_page;
        gStackSlot = stack_frame_slot;
    }
}

static void make_new_stack()
{
    int new_stack_page_size = gStackPageSize * 2;

    gStack = (sObject**)REALLOC(gStack, sizeof(sObject*)*new_stack_page_size);
    int i;
    for(i=gStackPageSize; i<new_stack_page_size; i++) {
        gStack[i] = MALLOC(sizeof(sObject)*SLOT_SIZE);
        memset(gStack[i], 0, sizeof(sObject)*SLOT_SIZE);
    }

    gStackPageSize = new_stack_page_size;
}

sObject* stack_get_free_object(int kind)
{
    if(gStackSlot >= SLOT_SIZE) {
        gStackPage++;
        gStackSlot = 0;

        if(gStackPage == gStackPageSize) {
            make_new_stack();
        }
    }

    sObject* result = gStack[gStackPage] + gStackSlot++;

    result->mFlags = kind;

    return result;
}

int stack_slot_size()
{
    return SLOT_SIZE;
}

int stack_page_size()
{
    return gStackPageSize;
}
