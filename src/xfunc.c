#include <string.h>

char* xstrncpy(char* des, char* src, int size)
{
    des[size-1] = 0;
    return strncpy(des, src, size-1);
}

char* xstrncat(char* des, char* str, int size)
{
    des[size-1] = 0;
    return strncat(des, str, size-1);
}
