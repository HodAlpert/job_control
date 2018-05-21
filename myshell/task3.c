#include <stdio.h>
#include <linux/limits.h>
#include <unistd.h>
#include <malloc.h>
#include <wait.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>
#include "LineParser.h"
#include "job_control.h"

pid_t parent;
char pathName[PATH_MAX];
pid_t execute(cmdLine *pCmdLine,int* fdin, job** jobList, job*currentJob, struct termios* shellAttr);

void handleSignal(int signal){
    char* nameOfSignal=strsignal(signal);
    dprintf(STDIN_FILENO,"signal %s was ignored\n",nameOfSignal);
}
void handleSignal2(int signal){
    printf("\n");

}


void connectFdInPipe(int* fdin){
    if (fdin!=NULL) {
        assert(close(fdin[1]) != -1);
        assert(dup2(fdin[0], STDIN_FILENO) != -1);
        assert(close(fdin[0]) != -1);
    }
}
void connectFdOutPipe(int* fdout){
    if(fdout!=NULL){
        assert(close(fdout[0])!=-1);
        assert(dup2(fdout[1],STDOUT_FILENO)!=-1);
        assert(close(fdout[1])!=-1);
    }
}


void childSignalSetter() {
    signal(SIGTTIN, SIG_DFL);
    signal(SIGTTOU, SIG_DFL);
    signal(SIGTSTP, SIG_DFL);
    signal(SIGQUIT, SIG_DFL);
    signal(SIGCHLD, SIG_DFL);

}

void childProccess(cmdLine *pCmdLine, int* fdout){
    childSignalSetter();
    if (pCmdLine->inputRedirect!=NULL) {
        assert(close(0)!=-1);
        assert(dup2(fileno(fopen(pCmdLine->inputRedirect, "r")), 0) != -1);
    }
    if (pCmdLine->outputRedirect!=NULL) {
        assert(close(1)!=-1);
        assert(dup2(fileno(fopen(pCmdLine->outputRedirect, "w")), 1) != -1);
    }
    if(pCmdLine->next!=NULL) {
        execute(pCmdLine->next, fdout,NULL,NULL,NULL);
        connectFdOutPipe(fdout);
    }
    assert(execvp(pCmdLine->arguments[0],pCmdLine->arguments)!=-1);
}

void ParentProccess(int childId, cmdLine *pCmdLine,int* fdout,job** jobList,job*currentJob,struct termios* shellAttr){
    if (fdout!=NULL) {
        assert(close(fdout[0]) != -1);
        assert(close(fdout[1]) != -1);
    }
    if (parent==getpid()&&pCmdLine->blocking=='\1') {
        int status;
        pid_t temp;
        run_job_in_foreground(jobList,currentJob,0,shellAttr,parent);
        if ((temp = waitpid(-getpgid(childId), &status, WUNTRACED))!=-1){
        }
    }
    else{
//        run_job_in_background(currentJob,0);
    }
}

void childPreProccess( job*currentJob, int isFirst){
    if(isFirst) {
        setpgid(getpid(), getpid());
        if(tcgetattr(STDIN_FILENO,currentJob->tmodes)==-1){
            perror("tcgetattr failed");
        }
    }

}
pid_t execute(cmdLine *pCmdLine,int* fdin, job** jobList, job*currentJob, struct termios* shellAttr){
    int fdout[2];
    if(pCmdLine->next!=NULL){
        assert(pipe(fdout)!=-1);
    }
    int isFirstChild =(getpid()==parent);
    int child_pid;
    switch (child_pid=fork()){
        case -1:
            perror("fork failed");
            exit(-1);
        case 0:
            connectFdInPipe(fdin);
            childPreProccess(currentJob,isFirstChild);
            childProccess(pCmdLine,(pCmdLine->next!=NULL)?fdout:NULL);
            break;
        default:
            if(isFirstChild)
                setpgid(child_pid,child_pid);
            ParentProccess(child_pid,pCmdLine,(pCmdLine->next!=NULL)?fdout:NULL,jobList,currentJob,shellAttr);
    }
    return child_pid;
}
void assignHandlers(){
    struct sigaction sa;
    sa.sa_handler=handleSignal;
    assert(sigaction(SIGQUIT,&sa,NULL)!=-1);
    assert(sigaction(SIGCHLD,&sa,NULL)!=-1);
    sa.sa_handler=handleSignal2;
    assert(sigaction(SIGTSTP,&sa,NULL)!=-1);

}

void signalInit() {
    signal(SIGTTIN, SIG_IGN);
    signal(SIGTTOU, SIG_IGN);
    assignHandlers();
    setpgid(getpid(),getpid());
}

int main() {
    signalInit();
    job** job_list = calloc(1, sizeof(long));
    *job_list=NULL;
    int i=1;
    struct termios* attr = malloc(sizeof(struct termios));
    if(tcgetattr(STDIN_FILENO,attr)==-1){
        perror("tcgetattr failed");
    }
    parent = getpid();
    while (i){
        fflush(STDIN_FILENO);
        sync();
        tcsetpgrp (STDIN_FILENO, getpgid(getpid()));
        tcsetattr(STDIN_FILENO,TCSADRAIN,attr);
        size_t size = PATH_MAX;
        assert(getcwd(pathName,size)!=NULL);
        printf("%s",pathName);
        printf(">");
        char line[2048];
        while(fgets(line,2048,stdin)==NULL){}
        cmdLine *pCmdLine;
        pCmdLine = parseCmdLines(line);
        job* jobToAdd;
        if(strcmp(pCmdLine->arguments[0],"quit")==0){
            freeCmdLines(pCmdLine);
            break;
        }
        else{
            line[strlen(line)-1]='\0';
            if(strcmp(line, "jobs")==0){
                print_jobs(job_list);
            }
            else if(strncmp(line,"fg",2)==0){
                run_job_in_foreground(job_list,find_job_by_index(*job_list,atoi(line+3)),1,attr,getpgid(getpid()));
            }
            else if(strncmp(line,"bg",2)==0){
                run_job_in_background(find_job_by_index(*job_list,atoi(line+3)),1);
            }
            else {
                jobToAdd = add_job(job_list,line);
                jobToAdd->status=RUNNING;
                jobToAdd->pgid = execute(pCmdLine, NULL,job_list,jobToAdd,attr);
            }

        }
        freeCmdLines(pCmdLine);
    }
    free(attr);
    free_job_list(job_list);
    free(job_list);

}