#include <stdio.h>

int main()
{
    printf("~~~~~~~~~~~~> Child (debuggee) process: before breakpoint\n");
    // The breakpoint
    printf("~~~~~~~~~~~~> Child (debuggee) process: after breakpoint\n");

    return 0;
}