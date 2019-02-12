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

This prints instructions of debuggee step by step

http://www.alexonlinux.com/how-debugger-works
https://eli.thegreenplace.net/2011/01/23/how-debuggers-work-part-1

compile debuggee with -static flag:  $ gcc -static sleeper.c -o sleeper && ./sleeper

address where to put INT 3 (0xCC): 
    $ gdb sleeper
    (gdb) disassemble main
OR search in $ objdump -d sleeper

compile debugger:
    $ gcc debugger_steps.c -o debugger_steps

run debugger:
    $ ./debugger_steps
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

    printf("In debugger process %ld\n", (long)getpid());

    // Waiting for child process to stop...
    wait(&status);
    
    unsigned icounter = 0;
    unsigned instruction = 0;
    struct user_regs_struct regs;

    while (WIFSTOPPED(status)) {
        icounter++;        
        ptrace(PTRACE_GETREGS, child, 0, &regs);
        instruction = ptrace(PTRACE_PEEKTEXT, child, regs.rip, 0);

        printf("icounter = %u.  RIP = 0x%08llx.  Instr = 0x%08x\n", icounter, regs.rip, instruction);

        // Make the child execute next instruction
        if (ptrace(PTRACE_SINGLESTEP, child, 0, 0) < 0) {
            perror("ptrace");
            return;
        }

        // Wait for child to stop on its next instruction
        wait(&status);

        // sleep 0.1 sec
        // usleep(100000);
    }

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