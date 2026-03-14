#include <stdio.h>
#include "Interface/mem_manager.h"


int main() {
    mm_init();
    printf("success init\n");
    mm_destroy();
    printf("success destroy\n");
    return 0;
}