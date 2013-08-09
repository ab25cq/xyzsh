#include "config.h"
#include "xyzsh.h"
#include <string.h>
#include <stdio.h>

sObject* int_new_on_stack(int value)
{
   sObject* self = stack_get_free_object(T_INT);
   
   SINT(self) = value;

   return self;
}
