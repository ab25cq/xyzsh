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

void split_prefix_of_object_and_name(sObject** object, sObject* prefix, sObject* name, char* str)
{
    char* p = str;
    while(*p) {
        if(*p == ':' && *(p+1) == ':') {
            p+=2;

            string_push_back(prefix, string_c_str(name));
            string_push_back(prefix, "::");

            if(*object && TYPE(*object) == T_UOBJECT) {
                *object = uobject_item(*object, string_c_str(name));
                string_put(name, "");
            }
            else {
                string_put(name, "");
                break;
            }
        }
        else {
            string_push_back2(name, *p);
            p++;
        }
    }
}

void split_prefix_of_object_and_name2(sObject** object, sObject* prefix, sObject* name, char* str, sObject* current_object)
{
    BOOL first = TRUE;
    char* p = str;
    while(*p) {
        if(*p == ':' && *(p+1) == ':') {
            p+=2;

            string_push_back(prefix, string_c_str(name));
            string_push_back(prefix, "::");

            if(*object && TYPE(*object) == T_UOBJECT) {
                if(first) {
                    first = FALSE;

                    *object = access_object3(string_c_str(name), &current_object);
                }
                else {
                    *object = uobject_item(*object, string_c_str(name));
                }
                string_put(name, "");
            }
            else {
                string_put(name, "");
                break;
            }
        }
        else {
            string_push_back2(name, *p);
            p++;
        }
    }
}

BOOL get_object_from_str(sObject** object, char* str, sObject* current_object, sObject* running_object, sRunInfo* runinfo) 
{
    *object = current_object;

    char* p = str;
    BOOL first = TRUE;
    sObject* name = STRING_NEW_STACK("");
    while(*p) {
        if(*p == ':' && *(p+1) == ':') {
            if(string_c_str(name)[0] == 0) {
                err_msg("invalid object name", runinfo->mSName, runinfo->mSLine, runinfo->mArgs[0]);
                return FALSE;
            }
            p+=2;

            if(first) {
                first = FALSE;

                *object = access_object(string_c_str(name), &current_object, running_object);

                if(*object == NULL) {
                    err_msg("invalid object name", runinfo->mSName, runinfo->mSLine, runinfo->mArgs[0]);
                    return TRUE;
                }
            }
            else {
                if(TYPE(*object) != T_UOBJECT) {
                    err_msg("invalid object name", runinfo->mSName, runinfo->mSLine, runinfo->mArgs[0]);
                    return TRUE;
                }

                *object = uobject_item(*object, string_c_str(name));

                if(*object == NULL) {
                    err_msg("invalid object name", runinfo->mSName, runinfo->mSLine, runinfo->mArgs[0]);
                    return TRUE;
                }
            }

            string_put(name, "");
        }
        else {
            string_push_back2(name, *p);
            p++;
        }
    }

    if(string_c_str(name) != 0) {
        if(first) {
            *object = access_object(string_c_str(name), &current_object, running_object);

            if(*object == NULL) {
                err_msg("invalid object name", runinfo->mSName, runinfo->mSLine, runinfo->mArgs[0]);
                return TRUE;
            }
        }
        else {
            if(TYPE(*object) != T_UOBJECT) {
                err_msg("invalid object name", runinfo->mSName, runinfo->mSLine, runinfo->mArgs[0]);
                return TRUE;
            }

            *object = uobject_item(*object, string_c_str(name));
        }
    }

    if(*object == NULL) {
        err_msg("invalid object name", runinfo->mSName, runinfo->mSLine, runinfo->mArgs[0]);
        return TRUE;
    }

    return TRUE;
}
