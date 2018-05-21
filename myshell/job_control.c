#include "job_control.h"
#include <wait.h>
#include <assert.h>

/**
* Receive a pointer to a job list and a new command to add to the job list and adds it to it.
* Create a new job list if none exists.
**/
job* add_job(job** job_list, char* cmd){
    job* job_to_add = initialize_job(cmd);

    if (*job_list == NULL){
        *job_list = job_to_add;
        job_to_add -> idx = 1;
    }
    else{
        int counter = 2;
        job* list = *job_list;
        while (list -> next !=NULL){
//            printf("adding %d\n", list->idx);
            list = list -> next;
            counter++;
        }
        job_to_add ->idx = counter;
        list -> next = job_to_add;
    }
    return job_to_add;
}


/**
* Receive a pointer to a job list and a pointer to a job and removes the job from the job list 
* freeing its memory.
**/
void remove_job(job** job_list, job* tmp){
    if (*job_list == NULL)
        return;
    job* tmp_list = *job_list;
    if (tmp_list == tmp){
        *job_list = tmp_list -> next;
        free_job(tmp);
        return;
    }

    while (tmp_list->next != tmp){
        tmp_list = tmp_list -> next;
    }
    tmp_list -> next = tmp -> next;
    free_job(tmp);

}

/**
* receives a status and prints the string it represents.
**/
char* status_to_str(int status)
{
    static char* strs[] = {"Done", "Suspended", "Running"};
    return strs[status + 1];
}


/**
*   Receive a job list, and print it in the following format:<code>[idx] \t status \t\t cmd</code>, where:
    cmd: the full command as typed by the user.
    status: Running, Suspended, Done (for jobs that have completed but are not yet removed from the list).
  
**/
void print_jobs(job** job_list){

    job* tmp = *job_list;
    update_job_list(job_list, FALSE);
    while (tmp != NULL){
        printf("[%d]\t %s \t\t %s", tmp->idx, status_to_str(tmp->status),tmp -> cmd);

        if (tmp -> cmd[strlen(tmp -> cmd)-1]  != '\n')
            printf("\n");
        job* job_to_remove = tmp;
        tmp = tmp -> next;
        if (job_to_remove->status == DONE)
            remove_job(job_list, job_to_remove);

    }

}


/**
* Receive a pointer to a list of jobs, and delete all of its nodes and the memory allocated for each of them.
*/
void free_job_list(job** job_list){
    while(*job_list != NULL){
        job* tmp = *job_list;
        *job_list = (*job_list) -> next;
        free_job(tmp);
    }

}


/**
* receives a pointer to a job, and frees it along with all memory allocated for its fields.
**/
void free_job(job* job_to_remove){
    free(job_to_remove->tmodes);
    free(job_to_remove->cmd);
    free(job_to_remove);
}



/**
* Receive a command (string) and return a job pointer. 
* The function needs to allocate all required memory for: job, cmd, tmodes
* to copy cmd, and to initialize the rest of the fields to NULL: next, pigd, status 
**/


job* initialize_job(char* cmd){
    job * createdJob = malloc(sizeof(job));
    createdJob->cmd = malloc(strlen(cmd+1)*sizeof(char));
    strcpy(createdJob->cmd,cmd);
    createdJob->tmodes = malloc(sizeof(struct termios));
    createdJob->next=NULL;
    return createdJob;
}


/**
* Receive a job list and and index and return a pointer to a job with the given index, according to the idx field.
* Print an error message if no job with such an index exists.
**/
job* find_job_by_index(job* job_list, int idx){
    if(job_list==NULL){
        dprintf(STDERR_FILENO,"find job by index: no job of index %d was found\n",idx);
        return NULL;
    }
    job* tmp = job_list;
    job* next;
    do {
        if (tmp->idx == idx) {
            return tmp;
        }
        next = tmp->next;
        tmp = next;
    }
        while(next != NULL);
    dprintf(STDERR_FILENO,"find job by index: no job of index %d was found\n",idx);
    return NULL;
}


/**
* Receive a pointer to a job list, and a boolean to decide whether to remove done
* jobs from the job list or not. 
**/

pid_t checkIfDone(job *jobToCheck){
    int status;
    pid_t result =waitpid(-jobToCheck->pgid, &status, WNOHANG|WUNTRACED);
    switch (result){
        case -1://procces not exist
            jobToCheck->status=DONE;
            break;
        case 0://still running
            if (WIFSIGNALED(status)) {
                jobToCheck->status=SUSPENDED;
            }
            break;
        default://done
            jobToCheck->status=DONE;
            break;
    }
    return result;
}

void update_job_list(job **job_list, int remove_done_jobs){
    if(*job_list==NULL)
        return;
    job* tmp = *job_list;
    job* next;
    do {
        checkIfDone(tmp);
        if (remove_done_jobs==TRUE&&tmp->status==DONE){
            printf("[%d]\t %s \t\t %s", tmp->idx, status_to_str(tmp->status),tmp -> cmd);
            if (tmp -> cmd[strlen(tmp -> cmd)-1]  != '\n')
                printf("\n");
            remove_job(job_list,tmp);
        }
        next = tmp->next;
        tmp = next;
    }
    while(next != NULL);
}

/** 
* Put job j in the foreground.  If cont is nonzero, restore the saved terminal modes and send the process group a
* SIGCONT signal to wake it up before we block.  Run update_job_list to print DONE jobs.
**/

void run_job_in_foreground (job** job_list, job *j, int cont, struct termios* shell_tmodes, pid_t shell_pgid){
    if(j==NULL){
        return;
    }
    if(cont) {
        int status;
        pid_t DoneTest;
        switch (DoneTest = waitpid(-j->pgid, &status, WNOHANG)) {
            case -1:// job is done- print and remove
                j->status = DONE;
                printf("[%d]\t %s \t\t %s", j->idx, status_to_str(j->status), j->cmd);
                if (j->cmd[strlen(j->cmd) - 1] != '\n')
                    printf("\n");
                remove_job(job_list, j);
                break;
            case 0:
                tcsetpgrp(STDIN_FILENO, j->pgid);
                if (cont == 1 && j->status == SUSPENDED) {
                    tcsetattr(STDIN_FILENO, TCSADRAIN, j->tmodes);

                }
                assert(kill(-j->pgid, SIGCONT) != -1);
                if (waitpid(-j->pgid, &status, WUNTRACED) != -1) {
                    if (WIFSTOPPED(status)||WIFSIGNALED(status)) {
                        j->status = SUSPENDED;
                    } else if (WIFSIGNALED(status) && WTERMSIG(status) == SIGINT) {
                        j->status = DONE;
                    }
                }
                break;
            default://job is done
                j->status = DONE;
                printf("[%d]\t %s \t\t %s", j->idx, status_to_str(j->status), j->cmd);
                if (j->cmd[strlen(j->cmd) - 1] != '\n')
                    printf("\n");
                remove_job(job_list, j);
                return;
        }
    }
    tcsetpgrp (STDIN_FILENO,shell_pgid);
    tcgetattr(STDIN_FILENO,j->tmodes);
    tcsetattr(STDIN_FILENO,TCSADRAIN,shell_tmodes);
    update_job_list(job_list,0);

}

/** 
* Put a job in the background.  If the cont argument is nonzero, send
* the process group a SIGCONT signal to wake it up.  
**/

void run_job_in_background (job *j, int cont){
    j->status=RUNNING;
    if(cont){
        kill(-j->pgid,SIGCONT);
    }
}