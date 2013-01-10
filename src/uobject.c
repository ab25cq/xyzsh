#include "config.h"
#include "xyzsh/xyzsh.h"
#include <string.h>
#include <stdio.h>

static unsigned int get_uobject_value(sObject* self, char* key)
{
   unsigned int i = 0;
   while(*key) {
      i += *key;
      key++;
   }

   return i % SUOBJECT(self).mTableSize;
}

static void resize(sObject* self) 
{
    const int table_size = SUOBJECT(self).mTableSize;

    SUOBJECT(self).mTableSize *= 5;
    FREE(SUOBJECT(self).mTable);
    SUOBJECT(self).mTable = (uobject_it**)MALLOC(sizeof(uobject_it*) * SUOBJECT(self).mTableSize);
    memset(SUOBJECT(self).mTable, 0, sizeof(uobject_it*)*SUOBJECT(self).mTableSize);

    uobject_it* it = SUOBJECT(self).mEntryIt;
    while(it) {
        unsigned int uobject_key = get_uobject_value(self, it->mKey);

        it->mCollisionIt = SUOBJECT(self).mTable[uobject_key];
        
        SUOBJECT(self).mTable[uobject_key] = it;

        it = it->mNextIt;
    }
}


uobject_it* uobject_it_new(char* key, void* item, uobject_it* coll_it, uobject_it* next_it, sObject* hash) 
{
   uobject_it* it = (uobject_it*)MALLOC(sizeof(uobject_it));

   it->mKey = STRDUP(key);

   it->mItem = item;
   it->mCollisionIt = coll_it;

   it->mNextIt = next_it;   

   return it;
}
   
void uobject_it_release(uobject_it* it, sObject* hash)
{
    FREE(it->mKey);
      
    FREE(it);
}


sObject* uobject_new_from_gc(int size, sObject* parent, char* name, BOOL user_object)
{
   sObject* self = gc_get_free_object(T_UOBJECT, user_object);
   
   SUOBJECT(self).mTableSize = size;
   SUOBJECT(self).mTable = (uobject_it**)MALLOC(sizeof(uobject_it*) * size);
   memset(SUOBJECT(self).mTable, 0, sizeof(uobject_it*)*size);

   SUOBJECT(self).mEntryIt = NULL;

   SUOBJECT(self).mCounter = 0;

   if(parent) SUOBJECT(self).mParent = parent; else SUOBJECT(self).mParent = self;

   SUOBJECT(self).mName = STRDUP(name);

   return self;
}

sObject* uobject_new_from_stack(int size, sObject* parent, char* name)
{
   sObject* self = stack_get_free_object(T_UOBJECT);
   
   SUOBJECT(self).mTableSize = size;
   SUOBJECT(self).mTable = (uobject_it**)MALLOC(sizeof(uobject_it*) * size);
   memset(SUOBJECT(self).mTable, 0, sizeof(uobject_it*)*size);

   SUOBJECT(self).mEntryIt = NULL;

   SUOBJECT(self).mCounter = 0;

   if(parent) SUOBJECT(self).mParent = parent; else SUOBJECT(self).mParent = self;

   SUOBJECT(self).mName = STRDUP(name);

   return self;
}

sObject* gInheritObject;

void uobject_root_init(sObject* self)
{
    ASSERT(TYPE(self) == T_UOBJECT);

    uobject_put(self, "unset", NFUN_NEW_GC(cmd_unset, NULL, TRUE));
    sObject* readline = UOBJECT_NEW_GC(8, self, "rl", TRUE);
    uobject_init(readline);
    uobject_put(self, "rl", readline);

    uobject_put(readline, "insert_text", NFUN_NEW_GC(cmd_readline_insert_text, NULL, TRUE));
    uobject_put(readline, "delete_text", NFUN_NEW_GC(cmd_readline_delete_text, NULL, TRUE));
    uobject_put(readline, "clear_screen", NFUN_NEW_GC(cmd_readline_clear_screen, NULL, TRUE));
    uobject_put(readline, "point_move", NFUN_NEW_GC(cmd_readline_point_move, NULL, TRUE));
    uobject_put(readline, "read_history", NFUN_NEW_GC(cmd_readline_read_history, NULL, TRUE));
    uobject_put(readline, "write_history", NFUN_NEW_GC(cmd_readline_write_history, NULL, TRUE));
    uobject_put(self, "completion", NFUN_NEW_GC(cmd_completion, NULL, TRUE));
    uobject_put(self, "p", NFUN_NEW_GC(cmd_p, NULL, TRUE));
    uobject_put(self, "jobs", NFUN_NEW_GC(cmd_jobs, NULL, TRUE));
    uobject_put(self, "gcinfo", NFUN_NEW_GC(cmd_gcinfo, NULL, TRUE));
    uobject_put(self, "stackinfo", NFUN_NEW_GC(cmd_stackinfo, NULL, TRUE));
    uobject_put(self, "fg", NFUN_NEW_GC(cmd_fg, NULL, TRUE));
    uobject_put(self, "exit", NFUN_NEW_GC(cmd_exit, NULL, TRUE));
    uobject_put(self, "fselector", NFUN_NEW_GC(cmd_fselector, NULL, TRUE));
    uobject_put(self, "sweep", NFUN_NEW_GC(cmd_sweep, NULL, TRUE));
    uobject_put(self, "subshell", NFUN_NEW_GC(cmd_subshell, NULL, TRUE));
    uobject_put(self, "print", NFUN_NEW_GC(cmd_print, NULL, TRUE));
    uobject_put(self, "if", NFUN_NEW_GC(cmd_if, NULL, TRUE));
    uobject_put(self, "for", NFUN_NEW_GC(cmd_for, NULL, TRUE));
    uobject_put(self, "break", NFUN_NEW_GC(cmd_break, NULL, TRUE));
    uobject_put(self, "while", NFUN_NEW_GC(cmd_while, NULL, TRUE));
    uobject_put(self, "return", NFUN_NEW_GC(cmd_return, NULL, TRUE));
    uobject_put(self, "rehash", NFUN_NEW_GC(cmd_rehash, NULL, TRUE));
    uobject_put(self, "true", NFUN_NEW_GC(cmd_true, NULL, TRUE));
    uobject_put(self, "false", NFUN_NEW_GC(cmd_false, NULL, TRUE));
    uobject_put(self, "write", NFUN_NEW_GC(cmd_write, NULL, TRUE));
    uobject_put(self, "quote", NFUN_NEW_GC(cmd_quote, NULL, TRUE));
    uobject_put(self, "load", NFUN_NEW_GC(cmd_load, NULL, TRUE));
    gInheritObject = NFUN_NEW_GC(cmd_inherit, NULL, TRUE);
    uobject_put(self, "inherit", gInheritObject);
    uobject_put(self, "eval", NFUN_NEW_GC(cmd_eval, NULL, TRUE));
    uobject_put(self, "object", NFUN_NEW_GC(cmd_object, NULL, TRUE));
    uobject_put(self, "pwo", NFUN_NEW_GC(cmd_pwo, NULL, TRUE));
    uobject_put(self, "co", NFUN_NEW_GC(cmd_co, NULL, TRUE));
    uobject_put(self, "ref", NFUN_NEW_GC(cmd_ref, NULL, TRUE));
    uobject_put(self, "length", NFUN_NEW_GC(cmd_length, NULL, TRUE));
    uobject_put(self, "export", NFUN_NEW_GC(cmd_export, NULL, TRUE));
    uobject_put(self, "x", NFUN_NEW_GC(cmd_x , NULL, TRUE));
    uobject_put(self, "stackframe", NFUN_NEW_GC(cmd_stackframe, NULL, TRUE));
    uobject_put(self, "msleep", NFUN_NEW_GC(cmd_msleep, NULL, TRUE));
    uobject_put(self, "raise", NFUN_NEW_GC(cmd_raise, NULL, TRUE));
    uobject_put(self, "cd", NFUN_NEW_GC(cmd_cd, NULL, TRUE));
    uobject_put(self, "popd", NFUN_NEW_GC(cmd_popd, NULL, TRUE));
    uobject_put(self, "pushd", NFUN_NEW_GC(cmd_pushd, NULL, TRUE));
    uobject_put(self, "block", NFUN_NEW_GC(cmd_block, NULL, TRUE));
    uobject_put(self, "lc", NFUN_NEW_GC(cmd_lc, NULL, TRUE));
    uobject_put(self, "uc", NFUN_NEW_GC(cmd_uc, NULL, TRUE));
    uobject_put(self, "chomp", NFUN_NEW_GC(cmd_chomp, NULL, TRUE));
    uobject_put(self, "chop", NFUN_NEW_GC(cmd_chop, NULL, TRUE));
    uobject_put(self, "pomch", NFUN_NEW_GC(cmd_pomch, NULL, TRUE));
    uobject_put(self, "funinfo", NFUN_NEW_GC(cmd_funinfo, NULL, TRUE));
    uobject_put(self, "printf", NFUN_NEW_GC(cmd_printf, NULL, TRUE));
    uobject_put(self, "join", NFUN_NEW_GC(cmd_join, NULL, TRUE));
    uobject_put(self, "lines", NFUN_NEW_GC(cmd_lines, NULL, TRUE));
    uobject_put(self, "rows", NFUN_NEW_GC(cmd_rows, NULL, TRUE));
    uobject_put(self, "scan", NFUN_NEW_GC(cmd_scan, NULL, TRUE));
    uobject_put(self, "split", NFUN_NEW_GC(cmd_split, NULL, TRUE));
    uobject_put(self, "try", NFUN_NEW_GC(cmd_try, NULL, TRUE));
    uobject_put(self, "errmsg", NFUN_NEW_GC(cmd_errmsg, NULL, TRUE));
    uobject_put(self, "prompt", NFUN_NEW_GC(cmd_prompt, NULL, TRUE));
    uobject_put(self, "defined", NFUN_NEW_GC(cmd_defined, NULL, TRUE));

    uobject_put(self, "-n", NFUN_NEW_GC(cmd_condition_n, NULL, TRUE));
    uobject_put(self, "-z", NFUN_NEW_GC(cmd_condition_z, NULL, TRUE));
    uobject_put(self, "-b", NFUN_NEW_GC(cmd_condition_b, NULL, TRUE));
    uobject_put(self, "-c", NFUN_NEW_GC(cmd_condition_c, NULL, TRUE));
    uobject_put(self, "-d", NFUN_NEW_GC(cmd_condition_d, NULL, TRUE));
    uobject_put(self, "-f", NFUN_NEW_GC(cmd_condition_f, NULL, TRUE));
    uobject_put(self, "-h", NFUN_NEW_GC(cmd_condition_h, NULL, TRUE));
    uobject_put(self, "-L", NFUN_NEW_GC(cmd_condition_l, NULL, TRUE));
    uobject_put(self, "-p", NFUN_NEW_GC(cmd_condition_p, NULL, TRUE));
    uobject_put(self, "-t", NFUN_NEW_GC(cmd_condition_t, NULL, TRUE));
    uobject_put(self, "-S", NFUN_NEW_GC(cmd_condition_s2, NULL, TRUE));
    uobject_put(self, "-g", NFUN_NEW_GC(cmd_condition_g, NULL, TRUE));
    uobject_put(self, "-k", NFUN_NEW_GC(cmd_condition_k, NULL, TRUE));
    uobject_put(self, "-u", NFUN_NEW_GC(cmd_condition_u, NULL, TRUE));
    uobject_put(self, "-r", NFUN_NEW_GC(cmd_condition_r, NULL, TRUE));
    uobject_put(self, "-w", NFUN_NEW_GC(cmd_condition_w, NULL, TRUE));
    uobject_put(self, "-x", NFUN_NEW_GC(cmd_condition_x, NULL, TRUE));
    uobject_put(self, "-O", NFUN_NEW_GC(cmd_condition_o, NULL, TRUE));
    uobject_put(self, "-G", NFUN_NEW_GC(cmd_condition_g2, NULL, TRUE));
    uobject_put(self, "-e", NFUN_NEW_GC(cmd_condition_e, NULL, TRUE));
    uobject_put(self, "-s", NFUN_NEW_GC(cmd_condition_s, NULL, TRUE));
    uobject_put(self, "=", NFUN_NEW_GC(cmd_condition_eq, NULL, TRUE));
    uobject_put(self, "!=", NFUN_NEW_GC(cmd_condition_neq, NULL, TRUE));
    uobject_put(self, "-slt", NFUN_NEW_GC(cmd_condition_slt, NULL, TRUE));
    uobject_put(self, "-sgt", NFUN_NEW_GC(cmd_condition_sgt, NULL, TRUE));
    uobject_put(self, "-sle", NFUN_NEW_GC(cmd_condition_sle, NULL, TRUE));
    uobject_put(self, "-sge", NFUN_NEW_GC(cmd_condition_sge, NULL, TRUE));
    uobject_put(self, "-eq", NFUN_NEW_GC(cmd_condition_eq2, NULL, TRUE));
    uobject_put(self, "-ne", NFUN_NEW_GC(cmd_condition_ne, NULL, TRUE));
    uobject_put(self, "-lt", NFUN_NEW_GC(cmd_condition_lt, NULL, TRUE));
    uobject_put(self, "-le", NFUN_NEW_GC(cmd_condition_le, NULL, TRUE));
    uobject_put(self, "-gt", NFUN_NEW_GC(cmd_condition_gt, NULL, TRUE));
    uobject_put(self, "-ge", NFUN_NEW_GC(cmd_condition_ge, NULL, TRUE));
    uobject_put(self, "-nt", NFUN_NEW_GC(cmd_condition_nt, NULL, TRUE));
    uobject_put(self, "-ot", NFUN_NEW_GC(cmd_condition_ot, NULL, TRUE));
    uobject_put(self, "-ef", NFUN_NEW_GC(cmd_condition_ef, NULL, TRUE));
    uobject_put(self, "=~", NFUN_NEW_GC(cmd_condition_re, NULL, TRUE));
    uobject_put(self, "++", NFUN_NEW_GC(cmd_plusplus, NULL, TRUE));
    uobject_put(self, "--", NFUN_NEW_GC(cmd_minusminus, NULL, TRUE));
    uobject_put(self, "+", NFUN_NEW_GC(cmd_plus, NULL, TRUE));
    uobject_put(self, "-", NFUN_NEW_GC(cmd_minus, NULL, TRUE));
    uobject_put(self, "*", NFUN_NEW_GC(cmd_mult, NULL, TRUE));
    uobject_put(self, "/", NFUN_NEW_GC(cmd_div, NULL, TRUE));
    uobject_put(self, "mod", NFUN_NEW_GC(cmd_mod, NULL, TRUE));
    uobject_put(self, "pow", NFUN_NEW_GC(cmd_pow, NULL, TRUE));
    uobject_put(self, "abs", NFUN_NEW_GC(cmd_abs, NULL, TRUE));
    uobject_put(self, "selector", NFUN_NEW_GC(cmd_selector, NULL, TRUE));
    uobject_put(self, "sort", NFUN_NEW_GC(cmd_sort, NULL, TRUE));
    uobject_put(self, "readline", NFUN_NEW_GC(cmd_readline, NULL, TRUE));
    uobject_put(self, "kanjicode", NFUN_NEW_GC(cmd_kanjicode, NULL, TRUE));
#if defined(HAVE_MIGEMO_H)
    uobject_put(self, "migemo_match", NFUN_NEW_GC(cmd_migemo_match, NULL, TRUE));
#endif

    uobject_put(self, "sub", NFUN_NEW_GC(cmd_sub, NULL, TRUE));
    uobject_put(self, "time", NFUN_NEW_GC(cmd_time, NULL, TRUE));

    sObject* nfun = NFUN_NEW_GC(cmd_hash, NULL, TRUE);
    (void)nfun_put_option_with_argument(nfun, STRDUP("-key"));
    uobject_put(self, "hash", nfun);
    nfun = NFUN_NEW_GC(cmd_ary, NULL, TRUE);
    (void)nfun_put_option_with_argument(nfun, STRDUP("-index"));
    (void)nfun_put_option_with_argument(nfun, STRDUP("-append"));
    uobject_put(self, "ary", nfun);

    nfun = NFUN_NEW_GC(cmd_var, NULL, TRUE);
    (void)nfun_put_option_with_argument(nfun, STRDUP("-index"));
    uobject_put(self, "var", nfun);

    nfun = NFUN_NEW_GC(cmd_add, NULL, TRUE);
    (void)nfun_put_option_with_argument(nfun, STRDUP("-number"));
    (void)nfun_put_option_with_argument(nfun, STRDUP("-index"));
    uobject_put(self, "add", nfun);

    nfun = NFUN_NEW_GC(cmd_del, NULL, TRUE);
    (void)nfun_put_option_with_argument(nfun, STRDUP("-number"));
    (void)nfun_put_option_with_argument(nfun, STRDUP("-index"));
    uobject_put(self, "del", nfun);

    nfun = NFUN_NEW_GC(cmd_index, NULL, TRUE);
    (void)nfun_put_option_with_argument(nfun, STRDUP("-number"));
    (void)nfun_put_option_with_argument(nfun, STRDUP("-count"));
    uobject_put(self, "index", nfun);

    nfun = NFUN_NEW_GC(cmd_rindex, NULL, TRUE);
    (void)nfun_put_option_with_argument(nfun, STRDUP("-number"));
    (void)nfun_put_option_with_argument(nfun, STRDUP("-count"));
    uobject_put(self, "rindex", nfun);

    nfun = NFUN_NEW_GC(cmd_each, NULL, TRUE);
    (void)nfun_put_option_with_argument(nfun, STRDUP("-number"));
    uobject_put(self, "each", nfun);

    nfun = NFUN_NEW_GC(cmd_def, NULL, TRUE);
    (void)nfun_put_option_with_argument(nfun, STRDUP("-option-with-argument"));
    uobject_put(self, "def", nfun);

    nfun = NFUN_NEW_GC(cmd_class, NULL, TRUE);
    (void)nfun_put_option_with_argument(nfun, STRDUP("-option-with-argument"));
    uobject_put(self, "class", nfun);

    clear_matching_info_variable();
}

void uobject_init(sObject* self)
{
    ASSERT(TYPE(self) == T_UOBJECT);

    uobject_put(self, "run", NFUN_NEW_GC(cmd_mrun, NULL, FALSE));
    uobject_put(self, "self", self);
    uobject_put(self, "root", gRootObject);
    uobject_put(self, "parent", SUOBJECT(self).mParent);
    uobject_put(self, "show", NFUN_NEW_GC(cmd_mshow, NULL, FALSE));
}

void uobject_delete_gc(sObject* self)
{
    ASSERT(TYPE(self) == T_UOBJECT);

   uobject_it* it = SUOBJECT(self).mEntryIt;

   while(it) {
      uobject_it* next_it = it->mNextIt;
      uobject_it_release(it, self);
      it = next_it;
   }
   
   FREE(SUOBJECT(self).mTable);
   FREE(SUOBJECT(self).mName);
}

void uobject_delete_stack(sObject* self)
{
    ASSERT(TYPE(self) == T_UOBJECT);

   uobject_it* it = SUOBJECT(self).mEntryIt;

   while(it) {
      uobject_it* next_it = it->mNextIt;
      uobject_it_release(it, self);
      it = next_it;
   }
   
   FREE(SUOBJECT(self).mTable);
   FREE(SUOBJECT(self).mName);
}

int uobject_gc_children_mark(sObject* self)
{
    ASSERT(TYPE(self) == T_UOBJECT);

    int count = 0;

    uobject_it* it = SUOBJECT(self).mEntryIt;
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

void uobject_put(sObject* self, char* key, void* item)
{
    ASSERT(TYPE(self) == T_UOBJECT);

    if(SUOBJECT(self).mCounter >= SUOBJECT(self).mTableSize) {
        resize(self);
    }
    
    unsigned int uobject_key = get_uobject_value(self, key);
    
    uobject_it* it = SUOBJECT(self).mTable[ uobject_key ];
    while(it) {
        if(strcmp(key, it->mKey) == 0) {
            it->mItem = item;
            return;
        }
        it = it->mCollisionIt;
    }
    
    uobject_it* new_it
        = uobject_it_new(key, item, SUOBJECT(self).mTable[uobject_key], SUOBJECT(self).mEntryIt, self);
    
    SUOBJECT(self).mTable[uobject_key] = new_it;
    SUOBJECT(self).mEntryIt = new_it;
    SUOBJECT(self).mCounter++;
}

void* uobject_item(sObject* self, char* key)
{
    ASSERT(TYPE(self) == T_UOBJECT);

   uobject_it* it = SUOBJECT(self).mTable[ get_uobject_value(self, key) ];

   while(it) {
      if(strcmp(key, it->mKey) == 0) return it->mItem;
      it = it->mCollisionIt;
   }

   return NULL;
}

void uobject_clear(sObject* self)
{
    ASSERT(TYPE(self) == T_UOBJECT);

   int i;
   int max;
   uobject_it* it = SUOBJECT(self).mEntryIt;

   while(it) {
      uobject_it* next_it = it->mNextIt;
      uobject_it_release(it, self);
      it = next_it;
   }

   memset(SUOBJECT(self).mTable, 0, sizeof(uobject_it*)*SUOBJECT(self).mTableSize);
   
   SUOBJECT(self).mEntryIt = NULL;
   SUOBJECT(self).mCounter = 0;
}

uobject_it* uobject_loop_begin(sObject* self) 
{ 
    ASSERT(TYPE(self) == T_UOBJECT);

    return SUOBJECT(self).mEntryIt; 
}

void* uobject_loop_item(uobject_it* it) 
{ 
    return it->mItem; 
}

char* uobject_loop_key(uobject_it* it)
{ 
    return it->mKey; 
}

uobject_it* uobject_loop_next(uobject_it* it) 
{ 
    return it->mNextIt; 
}

int uobject_count(sObject* self)
{
    ASSERT(TYPE(self) == T_UOBJECT);

   return SUOBJECT(self).mCounter;
}

static void erase_from_list(sObject* self, uobject_it* rit)
{
    ASSERT(TYPE(self) == T_UOBJECT);

   if(rit == SUOBJECT(self).mEntryIt) {
      SUOBJECT(self).mEntryIt = rit->mNextIt;
   }
   else {
      uobject_it* it = SUOBJECT(self).mEntryIt;

      while(it->mNextIt) {
         if(it->mNextIt == rit) {
            it->mNextIt = it->mNextIt->mNextIt;
            break;
         }
            
         it = it->mNextIt;
      }
   }
}

BOOL uobject_erase(sObject* self, char* key)
{
    ASSERT(TYPE(self) == T_UOBJECT);
   const unsigned int hash_value = get_uobject_value(self, key);
   uobject_it* it = SUOBJECT(self).mTable[hash_value];
   
   if(it == NULL)
      ;
   else if(strcmp(it->mKey, key) == 0) {
      uobject_it* it2 = SUOBJECT(self).mEntryIt;   
      SUOBJECT(self).mTable[ hash_value ] = it->mCollisionIt;

      erase_from_list(self, it);
      
      uobject_it_release(it, self);

      SUOBJECT(self).mCounter--;

      return TRUE;
   }
   else {
      uobject_it* prev_it = it;
      it = it->mCollisionIt;
      while(it) {
         if(strcmp(it->mKey, key) == 0) {
            prev_it->mCollisionIt = it->mCollisionIt;
            erase_from_list(self, it);
            uobject_it_release(it, self);
            SUOBJECT(self).mCounter--;
            
            return TRUE;
         }
         
         prev_it = it;
         it = it->mCollisionIt;
      }
   }

   return FALSE;
}

