#include "config.h"
#include "xyzsh/xyzsh.h"
#include <string.h>
#include <stdio.h>

sObject* access_object(char* name, sObject** current, sObject* running_object)
{
    sObject* object;
    if(running_object) {
        object = uobject_item(SFUN(running_object).mLocalObjects, name);
        if(object) { return object; }
    }

    while(1) {
        object = uobject_item(*current, name);

        if(object || *current == gRootObject) { return object; }

        *current = SUOBJECT((*current)).mParent;
    }
}

sObject* access_object2(char* name, sObject* current, sObject* running_object)
{
    sObject* object = uobject_item(SFUN(running_object).mLocalObjects, name);
    if(object) return object;

    return uobject_item(current, name);
}

sObject* access_object3(char* name, sObject** current)
{
    sObject* object;

    while(1) {
        object = uobject_item(*current, name);

        if(object || *current == gRootObject) { return object; }

        *current = SUOBJECT((*current)).mParent;
    }
}
