#include "config.h"
#include "xyzsh.h"
#include <string.h>
#include <stdio.h>

sObject* external_prog_new_on_gc(char* path, BOOL user_object)
{
   sObject* self = gc_get_free_object(T_EXTPROG, user_object);
   
   SEXTPROG(self).mPath = STRDUP(path);

   return self;
}

void external_prog_delete_on_gc(sObject* self)
{
    FREE(SEXTPROG(self).mPath);
}

sObject* extobj_new_on_gc(void* object, fExtObjMarkFun mark_fun, fExtObjFreeFun free_fun, fExtObjMainFun main_fun, BOOL user_object)
{
   sObject* self = gc_get_free_object(T_EXTOBJ, user_object);
   
   SEXTOBJ(self).mObject = object;
   SEXTOBJ(self).mMarkFun = mark_fun;
   SEXTOBJ(self).mFreeFun = free_fun;
   SEXTOBJ(self).mMainFun = main_fun;

   return self;
}


