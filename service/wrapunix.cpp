#include "unp.h"

void sig_chld(int signo) //signal action
{
    MSG_INFO("signal received");
    pid_t pid;
    int stat;
    //waitpid 用于获取子进程的退出状态。-1 表示等待任意子进程，WNOHANG 表示非阻塞模式，即如果没有子进程退出，则立即返回
    while((pid = waitpid(-1, &stat, WNOHANG)) > 0)
    {
        if(WIFEXITED(stat))
        {
            MSG_INFO("child %d normal termination, exit status = %d",pid,  WEXITSTATUS(stat));
        }
        else if(WIFSIGNALED(stat))
        {
            MSG_INFO("child %d abnormal termination, singal number  = %d%s", pid, WTERMSIG(stat),
            #ifdef WCOREDUMP
                   WCOREDUMP(stat)? " (core file generated) " : "");
            #else
                   "");
            #endif
        }
        else
        {
            MSG_ERR("child %d terminated abnormally", pid);
        }
    }
    if (pid == -1) {
        perror("waitpid error");
    }

    return;
}

void * Calloc(size_t n, size_t size)
{
    void *ptr;
    if( (ptr = calloc(n, size)) == NULL)
    {
        errno = ENOMEM;
        QUIT_ERR("Calloc error");
    }
    return ptr;
}


Sigfunc *Signal(int signo, Sigfunc * func)
{
    struct sigaction act, oact;
    act.sa_handler = func;
    sigemptyset(&act.sa_mask);
    act.sa_flags = 0;
    if(signo == SIGALRM){
#ifdef SA_INTERRUPT
        act.sa_flags |= SA_INTERRUPT;
#endif
    } else {
#ifdef SA_RESTART
        act.sa_flags |= SA_RESTART;
#endif
    }
    if(sigaction(signo, &act, &oact) < 0)
    {
        return SIG_ERR;
    }
    return oact.sa_handler;
}
