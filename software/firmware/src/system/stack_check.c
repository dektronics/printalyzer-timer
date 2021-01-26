#include <stdio.h>
#include <stddef.h>
#include <stdlib.h>

void *__stack_chk_guard = (void *)0xdeadbeef;

void __stack_chk_fail(void)
{
    printf("\nStack check failure!\n\n");
    abort();
}
