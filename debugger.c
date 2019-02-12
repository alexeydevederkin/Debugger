#include <stdio.h>
#include <signal.h>
#include <sys/ptrace.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <sys/user.h>
#include <time.h>


/*
    Simple debugger

http://www.alexonlinux.com/how-debugger-works
https://eli.thegreenplace.net/2011/01/23/how-debuggers-work-part-1

compile debuggee with -static flag:  
    $ gcc -static sleeper.c -o sleeper && ./sleeper

address where to put INT 3 (0xCC): 
    $ gdb sleeper
    (gdb) disassemble main
OR search in $ objdump -d sleeper

compile debugger:
    $ gcc debugger.c -o debugger

run debugger:
    $ ./debugger
*/


void signal_handler(int sig)
{
    printf("Process %ld received signal %d\n", (long)getpid(), sig);
}


void do_debugger(pid_t child)
{
    int status = 0;
    long data;
    long orig_data;
    unsigned long long addr;

    struct user_regs_struct regs;

    printf("In debugger process %ld\n", (long)getpid());

    if (signal(SIGCHLD, signal_handler) == SIG_ERR) 
    {
        perror("signal");
        exit(-1);
    }

    // Waiting for child process to stop...
    wait(&status);

    
    // Placing breakpoint...
    /*
    From the debuggee disassemble (sleeper):
        0x0000000000400b51 <+4>:     lea     0x915b0(%rip),%rdi        # 0x492108
        0x0000000000400b58 <+11>:    callq   0x410210 <puts>
        0x0000000000400b5d <+16>:    lea     0x915c4(%rip),%rdi        # 0x492128
        0x0000000000400b64 <+23>:    callq   0x410210 <puts>
    Placing breakpoint after first printf()
    */

    addr = 0x400b5d;

    data = ptrace(PTRACE_PEEKTEXT, child, (void *)addr, NULL);
    orig_data = data;
    data = (data & ~0xff) | 0xcc;  //  0xCC = INT 3 = Interrupt 3 - trap to debugger
    ptrace(PTRACE_POKETEXT, child, (void *)addr, data);
    printf("\noriginal instruction: |%lx|\n", orig_data);
    printf("\nmodified instruction: |%lx|\n\n", data);

    // Breakpoint is ready. Telling child to continue running...
    ptrace(PTRACE_CONT, child, NULL, NULL);
    child = wait(&status);

    // Restoring original data...
    ptrace(PTRACE_POKETEXT, child, (void *)addr, orig_data);

    // Changing RIP register so that it will point to the right address...
    memset(&regs, 0, sizeof(regs));
    ptrace(PTRACE_GETREGS, child, NULL, &regs);
    printf("RIP before resuming child is %llx\n", regs.rip);
    regs.rip = addr;
    ptrace(PTRACE_SETREGS, child, NULL, &regs);

    // debuggee is now ready to get resumed... Waiting 5 seconds...
    printf("Time before debugger falling asleep: %ld\n", (long)time(NULL));
    sleep(5);
    printf("Time after debugger falling asleep: %ld. Resuming debuggee...\n", (long)time(NULL));

    ptrace(PTRACE_CONT, child, NULL, NULL);

    child = wait(&status);
    if (WIFSTOPPED(status))
        printf("Debuggee stopped %d\n", WSTOPSIG(status));
    if (WIFEXITED(status))
        printf("Debuggee exited...\n");

    printf("Debugger exiting...\n");
}


void do_debuggee(void)
{
    char* argv[] = { NULL };
    char* envp[] = { NULL };
    
    printf("In debuggee process %ld\n", (long)getpid());

    if (ptrace(PTRACE_TRACEME, 0, NULL, NULL))
    {
        perror("ptrace");
        return;
    }

    execve("sleeper", argv, envp);
}


int main()
{
    pid_t child;

    // Creating child process. It will execute do_debuggee().
    // Parent process will continue to do_debugger().
    
    child = fork();

    if (child == 0)
    {
        do_debuggee();
    }
    else if (child > 0)
    {
        do_debugger(child); 
    }
    else
    {
        perror("fork");
        return -1;
    }

    return 0;
}