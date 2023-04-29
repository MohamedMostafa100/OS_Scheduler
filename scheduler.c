#include "headers.h"
void clearResources(int signum);
void passTime(int signum);
int msgqID;
int currentTime = 0;
bool running = false;

void Algorithm_SRTN(struct Queue_Log **const logQueue, int *const processorIdleTime, int *const processWaitingTime, float *const processWeightedTurnaround,  float *const deviation, int *const processNumber);
void Algorithm_HPF(struct Queue_Log **const logQueue, int *const processorIdleTime, int *const processWaitingTime, float *const processWeightedTurnaround, float *const deviation, int *const processNumber);
void Algorithm_RR(int *const quantum, struct Queue_Log **const logQueue, int *const processorIdleTime, int *const processWaitingTime, float *const processWeightedTurnaround, float *const deviation, int *const processNumber);

int main(int argc, char *argv[])
{
    signal(SIGINT, clearResources);
    initClk();

    //TODO: implement the scheduler.

    // Receive arguments
    enum Algorithm schedulingAlgorithm = (enum Algorithm)atoi(argv[1]);
    int quantum = atoi(argv[2]);


    // CPU Parameters
    struct Queue_Log *logQueue = createQueue_Log();
    int processorIdleTime = 0;
    int processWaitingTime = 0;
    float processWeightedTurnaround = 0;
    int processNumber = 0;
    float deviation = 0;

    //Intialize the message Queue
    key_t msgqKey = ftok("keyfile.txt", MQKEY);
    msgqID = msgget(msgqKey, 0666 | IPC_CREAT);
    if (msgqID == -1)
    {
        perror("Error creating message queue");
        exit(1);
    }

    //Choose an Algorithm
    switch (schedulingAlgorithm)
    {
    case ALGO_SRTN:
        Algorithm_SRTN(&logQueue, &processorIdleTime, &processWaitingTime, &processWeightedTurnaround, &deviation, &processNumber);
        break;
    case ALGO_HPF:
        Algorithm_HPF(&logQueue, &processorIdleTime, &processWaitingTime, &processWeightedTurnaround, &deviation, &processNumber);
        break;
    case ALGO_RR:
        Algorithm_RR(&quantum, &logQueue, &processorIdleTime, &processWaitingTime, &processWeightedTurnaround, &deviation, &processNumber);
    default:
        break;
    }

    //Prepare log file
    FILE *schedulerLog;
    schedulerLog = fopen("scheduler.log", "w");

    while (logQueue->start)
    {
        char event[10];

        switch (logQueue->start->event)
        {
        case EV_STARTED:
            strcpy(event, "started");
            break;

        case EV_RESUMED:
            strcpy(event, "resumed");
            break;

        case EV_STOPPED:
            strcpy(event, "stopped");
            break;

        case EV_FINISHED:
            strcpy(event, "finished");
            break;
        default:
            break;
        }
        fprintf(schedulerLog, "#At time %d process %d %s arr %d total %d remain %d wait %d", logQueue->start->time, logQueue->start->id, event, logQueue->start->arrivalTime, logQueue->start->totalTime, logQueue->start->remainingTime, logQueue->start->waitingTime);
        if (strcmp(event, "finished") == 0)
        {
            fprintf(schedulerLog, " TA %d WTA %0.2f\n", logQueue->start->totalTime + logQueue->start->waitingTime, ((float)(logQueue->start->totalTime + logQueue->start->waitingTime) / logQueue->start->totalTime));
        }
        else
        {
            fprintf(schedulerLog, "\n");
        }

        free(dequeue_Log(logQueue));
    }

    fclose(schedulerLog);

    free(logQueue);

    //Prepare perf file
    FILE *schedulerPerf;
    schedulerPerf = fopen("scheduler.perf", "w");

    // scanf("\n");
    fprintf(schedulerPerf, "CPU utilization = %.2f%%\n", ((float)(currentTime - processorIdleTime) * 100 / currentTime));
    fprintf(schedulerPerf, "Avg WTA = %.2f\n", ((float)processWeightedTurnaround / processNumber));
    fprintf(schedulerPerf, "Avg Waiting = %.2f\n", ((float)processWaitingTime / processNumber));
    fprintf(schedulerPerf, "Standard deviation = %.2f\n", sqrt((deviation / processNumber)));

    fclose(schedulerPerf);

    //Teminate process generator
    kill(getppid(),SIGUSR2);


    //TODO: upon termination release the clock resources.

    destroyClk(true);
    exit(0);

}
/*---------------------------------------------------------------SRTN----------------------------------------------------------------*/
void Algorithm_SRTN(struct Queue_Log **const logQueue, int *const processorIdleTime, int *const processWaitingTime, float *const processWeightedTurnaround, float *const deviation, int *const processNumber)
{
    struct Queue_PCB *queue = createQueue_PCB();
    struct Node_PCB *runningPCB = NULL;

    struct Message_Action receivedAction;
    struct Message_Process receivedProcess;
    struct Message_Generator receivedStat;

    //Start Scheduling
    msgrcv(msgqID, &receivedAction, sizeof(receivedAction.time) + sizeof(receivedAction.action), getppid(), !IPC_NOWAIT);
    running = true;

    while(running || queue->start){
        //Check if no more processes from process generator
        msgrcv(msgqID, &receivedAction, sizeof(receivedAction.time) + sizeof(receivedAction.action), getppid(), !IPC_NOWAIT);
        if(receivedAction.action==ACT_STOP){
            running = false;
        }
        //Get current time
        currentTime = receivedAction.time;
        printf("Time now: %d \n",currentTime);
        fflush(stdout);

        msgrcv(msgqID, &receivedStat, sizeof(receivedStat.status), getppid(), !IPC_NOWAIT);
        while(receivedStat.status == GEN_SEND){
            msgrcv(msgqID, &receivedProcess, sizeof(receivedProcess.attachedProcess), getppid(), !IPC_NOWAIT);
            printf("process %d Received at time: %d \n",receivedProcess.attachedProcess.id,currentTime);
            fflush(stdout);

            pid_t processPID = fork();
            if (processPID == -1)
            {
                perror("Error creating new process!\n");
                exit(1);
            }
            else if (processPID == 0)
            {
                char rt[5];
                sprintf(rt, "%d", receivedProcess.attachedProcess.runTime);

                execl("process.out", "process.out", rt, NULL);
                perror("Error creating new process!\n");
                exit(1);
            }

            struct Node_PCB *newPCB = createPCB(processPID, createProcess(receivedProcess.attachedProcess.id, receivedProcess.attachedProcess.arrivalTime, receivedProcess.attachedProcess.runTime, receivedProcess.attachedProcess.priority));
            timeEnqueue_PCB(queue, newPCB, newPCB->process->runTime);
            msgrcv(msgqID, &receivedStat, sizeof(receivedStat.status), getppid(), !IPC_NOWAIT);
        }

        struct Message_Action sendAction;
        //3 cases 1.No process currently scheduled 2.The next process isn't the current one 3.Stick to the current process
        if(!runningPCB){
            runningPCB = queue->start;
            if(runningPCB){
                sendAction.mType = runningPCB->pid;
                sendAction.action = ACT_RUN; // sending process start action
                sendAction.time = currentTime;
                // schedular send start info to running process
                msgsnd(msgqID, &sendAction, sizeof(sendAction.time) + sizeof(sendAction.action), !IPC_NOWAIT);
                // waiting for process to send a start action
                msgrcv(msgqID, &receivedAction, sizeof(receivedAction.time) + sizeof(receivedAction.action), getpid(), !IPC_NOWAIT);
                // update waiting time
                runningPCB->waitingTime = currentTime - runningPCB->process->arrivalTime;
                enqueue_Log(*logQueue, createLog(currentTime, EV_STARTED, runningPCB));
                //Update remaining time
                runningPCB->remainingTime = receivedAction.time; 
            }
            else{
                (*processorIdleTime)++;
            }
             
        }
        else if(runningPCB != queue->start){
            runningPCB->stoppageTime= currentTime;
            enqueue_Log(*logQueue, createLog(currentTime, EV_STOPPED, runningPCB));
            runningPCB = queue->start;
            if(runningPCB){
                sendAction.mType = runningPCB->pid;
                sendAction.action = ACT_RUN; // sending process start action
                sendAction.time = currentTime;
                // schedular send start info to running process
                msgsnd(msgqID, &sendAction, sizeof(sendAction.time) + sizeof(sendAction.action), !IPC_NOWAIT);
                // waiting for process to send a start action
                msgrcv(msgqID, &receivedAction, sizeof(receivedAction.time) + sizeof(receivedAction.action), getpid(), !IPC_NOWAIT);
                if(runningPCB->remainingTime < runningPCB->process->runTime)
                {
                    // update waiting time
                    runningPCB->waitingTime += currentTime - runningPCB->stoppageTime;
                    enqueue_Log(*logQueue, createLog(currentTime, EV_RESUMED, runningPCB));
                }
                else{
                    // update waiting time
                    runningPCB->waitingTime = currentTime - runningPCB->process->arrivalTime;
                    enqueue_Log(*logQueue, createLog(currentTime, EV_STARTED, runningPCB));
                }
                //Update remaining time
                runningPCB->remainingTime = receivedAction.time; 
            }
        }
        else{
            //runningPCB = queue->start;
            sendAction.mType = runningPCB->pid;
            sendAction.action = ACT_RUN; // sending process start action
            sendAction.time = currentTime;
            // schedular send start info to running process
            msgsnd(msgqID, &sendAction, sizeof(sendAction.time) + sizeof(sendAction.action), !IPC_NOWAIT);
            // waiting for process to send a start action
            msgrcv(msgqID, &receivedAction, sizeof(receivedAction.time) + sizeof(receivedAction.action), getpid(), !IPC_NOWAIT);
            
            if(receivedAction.action == ACT_RUN){
                //Update remaining time
                runningPCB->remainingTime = receivedAction.time; 
            }
            else{
                enqueue_Log(*logQueue, createLog(currentTime, EV_FINISHED, runningPCB));
                (*processWaitingTime) += runningPCB->waitingTime;
                (*processWeightedTurnaround) += ((float)((runningPCB->waitingTime + runningPCB->process->runTime)) / runningPCB->process->runTime);
                (*deviation) += pow(((float)(runningPCB->waitingTime + runningPCB->process->runTime) / runningPCB->process->runTime) - (float)((runningPCB->waitingTime + runningPCB->process->runTime)),2);
                (*processNumber)++;
                // current PCB is not pointing to the current process anymore
                free(runningPCB->process);
                // remove current pcb from queue
                free(remove_PCB(queue, runningPCB));
                // running pcb will be the next PCB in queue
                runningPCB = queue->start;
                if(runningPCB){
                    sendAction.mType = runningPCB->pid;
                    sendAction.action = ACT_RUN; // sending process start action
                    sendAction.time = currentTime;
                    // schedular send start info to running process
                    msgsnd(msgqID, &sendAction, sizeof(sendAction.time) + sizeof(sendAction.action), !IPC_NOWAIT);
                    // waiting for process to send a start action
                    msgrcv(msgqID, &receivedAction, sizeof(receivedAction.time) + sizeof(receivedAction.action), getpid(), !IPC_NOWAIT);
                    if(runningPCB->remainingTime < runningPCB->process->runTime)
                    {
                        // update waiting time
                        runningPCB->waitingTime += currentTime - runningPCB->stoppageTime;
                        enqueue_Log(*logQueue, createLog(currentTime, EV_RESUMED, runningPCB));
                    }
                    else{
                        // update waiting time
                        runningPCB->waitingTime = currentTime - runningPCB->process->arrivalTime;
                        enqueue_Log(*logQueue, createLog(currentTime, EV_STARTED, runningPCB));
                    }
                    //Update remaining time
                    runningPCB->remainingTime = receivedAction.time;
                } 
            }
        }
    }
    free(queue);
}
/*---------------------------------------------------------------Non-Prempetive HPF--------------------------------------------------*/
void Algorithm_HPF(struct Queue_Log **const logQueue, int *const processorIdleTime, int *const processWaitingTime, float *const processWeightedTurnaround, float *const deviation, int *const processNumber)
{
    struct Queue_PCB *queue = createQueue_PCB();
    struct Node_PCB *runningPCB = NULL;

    struct Message_Action receivedAction;
    struct Message_Process receivedProcess;
    struct Message_Generator receivedStat;

    //Start Scheduling
    msgrcv(msgqID, &receivedAction, sizeof(receivedAction.time) + sizeof(receivedAction.action), getppid(), !IPC_NOWAIT);
    running = true;

    while(running || queue->start){
        //Check if no more processes from process generator
        msgrcv(msgqID, &receivedAction, sizeof(receivedAction.time) + sizeof(receivedAction.action), getppid(), !IPC_NOWAIT);
        if(receivedAction.action==ACT_STOP){
            running = false;
        }
        //Get current time
        currentTime = receivedAction.time;
        printf("Time now: %d \n",currentTime);
        fflush(stdout);

        msgrcv(msgqID, &receivedStat, sizeof(receivedStat.status), getppid(), !IPC_NOWAIT);
        while(receivedStat.status == GEN_SEND){
            msgrcv(msgqID, &receivedProcess, sizeof(receivedProcess.attachedProcess), getppid(), !IPC_NOWAIT);
            printf("process %d Received at time: %d \n",receivedProcess.attachedProcess.id,currentTime);
            fflush(stdout);

            pid_t processPID = fork();
            if (processPID == -1)
            {
                perror("Error creating new process!\n");
                exit(1);
            }
            else if (processPID == 0)
            {
                char rt[5];
                sprintf(rt, "%d", receivedProcess.attachedProcess.runTime);

                execl("process.out", "process.out", rt, NULL);
                perror("Error creating new process!\n");
                exit(1);
            }

            struct Node_PCB *newPCB = createPCB(processPID, createProcess(receivedProcess.attachedProcess.id, receivedProcess.attachedProcess.arrivalTime, receivedProcess.attachedProcess.runTime, receivedProcess.attachedProcess.priority));
            priorityEnqueue_PCB(queue, newPCB, newPCB->process->priority);
            msgrcv(msgqID, &receivedStat, sizeof(receivedStat.status), getppid(), !IPC_NOWAIT);
        }

        struct Message_Action sendAction;
        //2 cases 1.No process currently scheduled 2.Stick to the current process
        if(!runningPCB){
            runningPCB = queue->start;
            if(runningPCB){
                sendAction.mType = runningPCB->pid;
                sendAction.action = ACT_RUN; // sending process start action
                sendAction.time = currentTime;
                // schedular send start info to running process
                msgsnd(msgqID, &sendAction, sizeof(sendAction.time) + sizeof(sendAction.action), !IPC_NOWAIT);
                // waiting for process to send a start action
                msgrcv(msgqID, &receivedAction, sizeof(receivedAction.time) + sizeof(receivedAction.action), getpid(), !IPC_NOWAIT);
                // update waiting time
                runningPCB->waitingTime = currentTime - runningPCB->process->arrivalTime;
                enqueue_Log(*logQueue, createLog(currentTime, EV_STARTED, runningPCB));
                //Update remaining time
                runningPCB->remainingTime = receivedAction.time; 
            }
            else{
                (*processorIdleTime)++;
            }
             
        }
        else{
            //runningPCB = queue->start;
            sendAction.mType = runningPCB->pid;
            sendAction.action = ACT_RUN; // sending process start action
            sendAction.time = currentTime;
            // schedular send start info to running process
            msgsnd(msgqID, &sendAction, sizeof(sendAction.time) + sizeof(sendAction.action), !IPC_NOWAIT);
            // waiting for process to send a start action
            msgrcv(msgqID, &receivedAction, sizeof(receivedAction.time) + sizeof(receivedAction.action), getpid(), !IPC_NOWAIT);
            
            if(receivedAction.action == ACT_RUN){
                //Update remaining time
                runningPCB->remainingTime = receivedAction.time; 
            }
            else{
                enqueue_Log(*logQueue, createLog(currentTime, EV_FINISHED, runningPCB));
                (*processWaitingTime) += runningPCB->waitingTime;
                (*processWeightedTurnaround) += ((float)((runningPCB->waitingTime + runningPCB->process->runTime)) / runningPCB->process->runTime);
                (*deviation) += pow(((float)(runningPCB->waitingTime + runningPCB->process->runTime) / runningPCB->process->runTime) - (float)((runningPCB->waitingTime + runningPCB->process->runTime)),2);

                (*processNumber)++;
                // current PCB is not pointing to the current process anymore
                free(runningPCB->process);
                // remove current pcb from queue
                free(remove_PCB(queue, runningPCB));
                // running pcb will be the next PCB in queue
                runningPCB = queue->start;
                if(runningPCB){
                    sendAction.mType = runningPCB->pid;
                    sendAction.action = ACT_RUN; // sending process start action
                    sendAction.time = currentTime;
                    // schedular send start info to running process
                    msgsnd(msgqID, &sendAction, sizeof(sendAction.time) + sizeof(sendAction.action), !IPC_NOWAIT);
                    // waiting for process to send a start action
                    msgrcv(msgqID, &receivedAction, sizeof(receivedAction.time) + sizeof(receivedAction.action), getpid(), !IPC_NOWAIT);
                    // update waiting time
                    runningPCB->waitingTime = currentTime - runningPCB->process->arrivalTime;
                    enqueue_Log(*logQueue, createLog(currentTime, EV_STARTED, runningPCB));
                    //Update remaining time
                    runningPCB->remainingTime = receivedAction.time;
                } 
            }
        }
    }
    free(queue);
}
/*---------------------------------------------------------------RR------------------------------------------------------------------*/
void Algorithm_RR(int *const quantum, struct Queue_Log **const logQueue, int *const processorIdleTime, int *const processWaitingTime, float *const processWeightedTurnaround, float *const deviation, int *const processNumber){
    struct Queue_PCB *queue = createQueue_PCB();
    struct Node_PCB *runningPCB = NULL;

    struct Message_Action receivedAction;
    struct Message_Process receivedProcess;
    struct Message_Generator receivedStat;
    int q = 0;

    //Start Scheduling
    msgrcv(msgqID, &receivedAction, sizeof(receivedAction.time) + sizeof(receivedAction.action), getppid(), !IPC_NOWAIT);
    running = true;

    while(running || queue->start){
        //Check if no more processes from process generator
        msgrcv(msgqID, &receivedAction, sizeof(receivedAction.time) + sizeof(receivedAction.action), getppid(), !IPC_NOWAIT);
        if(receivedAction.action==ACT_STOP){
            running = false;
        }
        //Get current time
        currentTime = receivedAction.time;
        printf("Time now: %d \n",currentTime);
        fflush(stdout);

        msgrcv(msgqID, &receivedStat, sizeof(receivedStat.status), getppid(), !IPC_NOWAIT);
        while(receivedStat.status == GEN_SEND){
            msgrcv(msgqID, &receivedProcess, sizeof(receivedProcess.attachedProcess), getppid(), !IPC_NOWAIT);
            printf("process %d Received at time: %d \n",receivedProcess.attachedProcess.id,currentTime);
            fflush(stdout);

            pid_t processPID = fork();
            if (processPID == -1)
            {
                perror("Error creating new process!\n");
                exit(1);
            }
            else if (processPID == 0)
            {
                char rt[5];
                sprintf(rt, "%d", receivedProcess.attachedProcess.runTime);

                execl("process.out", "process.out", rt, NULL);
                perror("Error creating new process!\n");
                exit(1);
            }

            struct Node_PCB *newPCB = createPCB(processPID, createProcess(receivedProcess.attachedProcess.id, receivedProcess.attachedProcess.arrivalTime, receivedProcess.attachedProcess.runTime, receivedProcess.attachedProcess.priority));
            normalEnqueue_PCB(queue, newPCB);
            msgrcv(msgqID, &receivedStat, sizeof(receivedStat.status), getppid(), !IPC_NOWAIT);
        }

        struct Message_Action sendAction;
        //2 cases 1.No process currently scheduled 2.Stick to the current process(check if finished or quantum passed)
        if(!runningPCB){
            runningPCB = queue->start;
            if(runningPCB){
                sendAction.mType = runningPCB->pid;
                sendAction.action = ACT_RUN; // sending process start action
                sendAction.time = currentTime;
                // schedular send start info to running process
                msgsnd(msgqID, &sendAction, sizeof(sendAction.time) + sizeof(sendAction.action), !IPC_NOWAIT);
                // waiting for process to send a start action
                msgrcv(msgqID, &receivedAction, sizeof(receivedAction.time) + sizeof(receivedAction.action), getpid(), !IPC_NOWAIT);
                // update waiting time
                runningPCB->waitingTime = currentTime - runningPCB->process->arrivalTime;
                enqueue_Log(*logQueue, createLog(currentTime, EV_STARTED, runningPCB));
                //Update remaining time
                runningPCB->remainingTime = receivedAction.time;
                //Update quantum 
                q++;
            }
            else{
                (*processorIdleTime)++;
            }
             
        }
        else{
            //runningPCB = queue->start;
            sendAction.mType = runningPCB->pid;
            sendAction.action = ACT_STOP; // sending process start action
            sendAction.time = currentTime;
            // schedular send start info to running process
            msgsnd(msgqID, &sendAction, sizeof(sendAction.time) + sizeof(sendAction.action), !IPC_NOWAIT);
            // waiting for process to send a start action
            msgrcv(msgqID, &receivedAction, sizeof(receivedAction.time) + sizeof(receivedAction.action), getpid(), !IPC_NOWAIT);
            
            if(receivedAction.action == ACT_STOP){
                struct Node_PCB *nextPCB = NULL;
                if(runningPCB==queue->end){

                    nextPCB = queue->start;
                }
                else{
                    nextPCB = runningPCB->next;
                }
                enqueue_Log(*logQueue, createLog(currentTime, EV_FINISHED, runningPCB));
                (*processWaitingTime) += runningPCB->waitingTime;
                (*processWeightedTurnaround) += ((float)((currentTime)-runningPCB->process->arrivalTime) / runningPCB->process->runTime);
                (*deviation) += pow((*processWeightedTurnaround)- (float)((currentTime)-runningPCB->process->arrivalTime),2);
                (*processNumber)++;
                // current PCB is not pointing to the current process anymore
                free(runningPCB->process);
                // remove current pcb from queue
                free(remove_PCB(queue, runningPCB));
                //Reset quantum
                q=0;
                // running pcb will be the next PCB in queue
                runningPCB = nextPCB;
                if(runningPCB && queue->start){ //To ensure the next process is in the queue
                    sendAction.mType = runningPCB->pid;
                    sendAction.action = ACT_RUN; // sending process start action
                    sendAction.time = currentTime;
                    // schedular send start info to running process
                    msgsnd(msgqID, &sendAction, sizeof(sendAction.time) + sizeof(sendAction.action), !IPC_NOWAIT);
                    // waiting for process to send a start action
                    msgrcv(msgqID, &receivedAction, sizeof(receivedAction.time) + sizeof(receivedAction.action), getpid(), !IPC_NOWAIT);
                    if(runningPCB->remainingTime < runningPCB->process->runTime)
                    {
                        // update waiting time
                        runningPCB->waitingTime += currentTime - runningPCB->stoppageTime;
                        enqueue_Log(*logQueue, createLog(currentTime, EV_RESUMED, runningPCB));
                    }
                    else{
                        // update waiting time
                        runningPCB->waitingTime = currentTime - runningPCB->process->arrivalTime;
                        enqueue_Log(*logQueue, createLog(currentTime, EV_STARTED, runningPCB));
                    }
                    //Update remaining time
                    runningPCB->remainingTime = receivedAction.time;
                    //Update quantum
                    q++;
                }
            }
            else if(q == *quantum){
                struct Node_PCB *nextPCB = NULL;
                if(runningPCB==queue->end){

                    nextPCB = queue->start;
                }
                else{
                    nextPCB = runningPCB->next;
                }
                if(runningPCB != nextPCB){
                    runningPCB->stoppageTime= currentTime;
                    enqueue_Log(*logQueue, createLog(currentTime, EV_STOPPED, runningPCB));
                }
                struct Node_PCB* prevPCB = runningPCB; //For printing purposes only 
                runningPCB = nextPCB;
                //Reset quantum
                q=0;
                sendAction.mType = runningPCB->pid;
                sendAction.action = ACT_RUN; // sending process start action
                sendAction.time = currentTime;
                // schedular send start info to running process
                msgsnd(msgqID, &sendAction, sizeof(sendAction.time) + sizeof(sendAction.action), !IPC_NOWAIT);
                // waiting for process to send a start action
                msgrcv(msgqID, &receivedAction, sizeof(receivedAction.time) + sizeof(receivedAction.action), getpid(), !IPC_NOWAIT);
                if(runningPCB->remainingTime < runningPCB->process->runTime)
                {
                    // update waiting time
                    runningPCB->waitingTime += currentTime - runningPCB->stoppageTime;
                    if(runningPCB != prevPCB){
                        enqueue_Log(*logQueue, createLog(currentTime, EV_RESUMED, runningPCB));
                    }

                }
                else{
                    // update waiting time
                    runningPCB->waitingTime = currentTime - runningPCB->process->arrivalTime;
                    enqueue_Log(*logQueue, createLog(currentTime, EV_STARTED, runningPCB));
                }
                //Update remaining time
                runningPCB->remainingTime = receivedAction.time;
                //Update quantum
                q++;
            }
            else{
                sendAction.mType = runningPCB->pid;
                sendAction.action = ACT_RUN; // sending process start action
                sendAction.time = currentTime;
                // schedular send start info to running process
                msgsnd(msgqID, &sendAction, sizeof(sendAction.time) + sizeof(sendAction.action), !IPC_NOWAIT);
                // waiting for process to send a start action
                msgrcv(msgqID, &receivedAction, sizeof(receivedAction.time) + sizeof(receivedAction.action), getpid(), !IPC_NOWAIT);
                //Update remaining time
                runningPCB->remainingTime = receivedAction.time;
                //Update quantum
                q++;
            }
        }
    }
    free(queue);
}
void clearResources(int signum)
{
    destroyClk(true);
    exit(0);
}

