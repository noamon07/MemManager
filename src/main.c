#include <stdio.h>
#include "Interface/mem_manager.h"
#include <string.h>

int mm_huge_check()
{
    Handle h2 =mm_malloc(5000);
    if(!MEM_MANAGER_VALID(h2))
    {
        printf("fail malloc\n");
        return 1;
    }
    printf("success malloc\n");
    char* s = mm_get_ptr(h2);
    strcpy(s,"hello world");
    printf("%s\n",s);
    mm_realloc(h2,10000);
    printf("%s\n",s);
    mm_free(h2);

    return 0;
}


int main() {
    Handle h1 = mm_malloc(5);
    if(MEM_MANAGER_VALID(h1))
    {
        printf("success malloc\n");
        mm_free(h1);
    }
    else
    {
        printf("fail malloc\n");
    }
    if(mm_huge_check())
        return 1;
    mm_destroy();
    printf("success destroy\n");
    return 0;
}