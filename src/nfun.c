#include "config.h"
#include "xyzsh/xyzsh.h"
#include <string.h>
#include <stdio.h>

sObject* nfun_new_from_gc(fXyzshNativeFun fun, sObject* parent, BOOL user_object)
{
   sObject* self = gc_get_free_object(T_NFUN, user_object);
   
   SNFUN(self).mNativeFun = fun;
   SNFUN(self).mParent = parent;

   SNFUN(self).mOptions = MALLOC(sizeof(option_hash_it)*XYZSH_OPTION_MAX);
   memset(SNFUN(self).mOptions,0, sizeof(option_hash_it)*XYZSH_OPTION_MAX);

   return self;
}

void nfun_delete_gc(sObject* self)
{
    int i;
    for(i=0; i<XYZSH_OPTION_MAX; i++) {
        if(SNFUN(self).mOptions[i].mKey) { FREE(SNFUN(self).mOptions[i].mKey); }
        if(SNFUN(self).mOptions[i].mArg) { FREE(SNFUN(self).mOptions[i].mArg); }
    }
    FREE(SNFUN(self).mOptions);
}

static int options_hash_fun(char* key)
{
    int value = 0;
    while(*key) {
        value += *key;
        key++;
    }
    return value % XYZSH_OPTION_MAX;
}

BOOL nfun_put_option_with_argument(sObject* self, MANAGED char* key)
{
    int hash_value = options_hash_fun(key);

    option_hash_it* p = SNFUN(self).mOptions + hash_value;
    while(1) {
        if(p->mKey) {
            p++;
            if(p == SNFUN(self).mOptions + hash_value) {
                return FALSE;
            }
            else if(p == SNFUN(self).mOptions + XYZSH_OPTION_MAX) {
                p = SNFUN(self).mOptions;
            }
        }
        else {
            p->mKey = MANAGED key;
            return TRUE;
        }
    }
}

BOOL nfun_option_with_argument_item(sObject* self, char* key)
{
    int hash_value = options_hash_fun(key);
    option_hash_it* p = SNFUN(self).mOptions + hash_value;

    while(1) {
        if(p->mKey) {
            if(strcmp(p->mKey, key) == 0) {
                return TRUE;
            }
            else {
                p++;
                if(p == SNFUN(self).mOptions + hash_value) {
                    return FALSE;
                }
                else if(p == SNFUN(self).mOptions + XYZSH_OPTION_MAX) {
                    p = SNFUN(self).mOptions;
                }
            }
        }
        else {
            return FALSE;
        }
    }
}

int nfun_gc_children_mark(sObject* self)
{
    int count = 0;

    sObject* parent = SNFUN(self).mParent;
    if(parent) {
        if(IS_MARKED(parent) == 0) {
            SET_MARK(parent);
            count++;
            count += object_gc_children_mark(parent);
        }
    }

    return count;
}

