#include "config.h"
#include "xyzsh.h"
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

        if(*current == NULL) { return NULL; }
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

        if(*current == NULL) return NULL;
    }
}

BOOL get_object_from_argument(sObject** object, char* str, sObject* current_object, sObject* running_object, sRunInfo* runinfo) 
{
    *object = current_object;

    char* p = str;
    BOOL first = TRUE;
    sObject* name = STRING_NEW_STACK("");
    while(*p) {
        if(*p == ':' && *(p+1) == ':') {
            p+=2;

            if(string_c_str(name)[0] == 0) {
                if(first) {
                    first = FALSE;

                    *object = gRootObject;
                }
                else {
                    char buf[128];
                    snprintf(buf, 128,"invalid object name(%s)", name);
                    err_msg(buf, runinfo->mSName, runinfo->mSLine);
                    return FALSE;
                }
            }
            else {
                if(first) {
                    first = FALSE;

                    *object = access_object(string_c_str(name), &current_object, running_object);

                    if(*object == NULL || STYPE(*object) != T_UOBJECT) {
                        char buf[128];
                        snprintf(buf, 128,"invalid object name(%s)", name);
                        err_msg(buf, runinfo->mSName, runinfo->mSLine);
                        return FALSE;
                    }
                }
                else {
                    *object = uobject_item(*object, string_c_str(name));

                    if(*object == NULL || STYPE(*object) != T_UOBJECT) {
                        char buf[128];
                        snprintf(buf, 128,"invalid object name(%s)", name);
                        err_msg(buf, runinfo->mSName, runinfo->mSLine);
                        return FALSE;
                    }
                }

                string_put(name, "");
            }
        }
        else {
            string_push_back2(name, *p);
            p++;
        }
    }

    if(string_c_str(name) != 0) {
        if(first) {
            *object = access_object(string_c_str(name), &current_object, running_object);
        }
        else {
            *object = uobject_item(*object, string_c_str(name));
        }
    }

    if(*object == NULL) {
        char buf[128];
        snprintf(buf, 128,"invalid object name(%s)", name);
        err_msg(buf, runinfo->mSName, runinfo->mSLine);
        return FALSE;
    }

    return TRUE;
}

BOOL get_object_prefix_and_name_from_argument(sObject** object, sObject* name, char* str, sObject* current_object, sObject* running_object, sRunInfo* runinfo) 
{
    *object = current_object;

    char* p = str;
    BOOL first = TRUE;
    while(*p) {
        if(*p == ':' && *(p+1) == ':') {
            p+=2;

            if(string_c_str(name)[0] == 0) {
                if(first) {
                    first = FALSE;

                    *object = gRootObject;
                }
                else {
                    err_msg("invalid object name5", runinfo->mSName, runinfo->mSLine);
                    return FALSE;
                }
            }
            else {
                if(first) {
                    first = FALSE;

                    *object = access_object(string_c_str(name), &current_object, running_object);

                    if(*object == NULL || STYPE(*object) != T_UOBJECT) {
                        err_msg("invalid object name6", runinfo->mSName, runinfo->mSLine);
                        return FALSE;
                    }
                }
                else {
                    *object = uobject_item(*object, string_c_str(name));

                    if(*object == NULL || STYPE(*object) != T_UOBJECT) {
                        err_msg("invalid object name7", runinfo->mSName, runinfo->mSLine);
                        return FALSE;
                    }
                }
            }

            string_put(name, "");
        }
        else {
            string_push_back2(name, *p);
            p++;
        }
    }

    if(*object == NULL || string_c_str(name) == 0) {
        err_msg("invalid object name8", runinfo->mSName, runinfo->mSLine);
        return FALSE;
    }

    return TRUE;
}
