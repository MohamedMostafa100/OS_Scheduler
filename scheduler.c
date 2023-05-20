#include "headers.h"
void clearResources(int signum);
void passTime(int signum);
int msgqID;
int currentTime = 0;
bool running = false;
float Each_WTA[1000];
int Each_WTA_counter=0;
void Algorithm_SRTN(struct Queue_Log **const logQueue,struct Queue_MemoryLog **const MemoryLogQueue, int *const processorIdleTime, int *const processWaitingTime, float *const processWeightedTurnaround,  float *const deviation, int *const processNumber);
void Algorithm_HPF(struct Queue_Log **const logQueue, struct Queue_MemoryLog **const MemoryLogQueue, int *const processorIdleTime, int *const processWaitingTime, float *const processWeightedTurnaround, float *const deviation, int *const processNumber);
void Algorithm_RR(int *const quantum, struct Queue_Log **const logQueue,struct Queue_MemoryLog **const MemoryLogQueue, int *const processorIdleTime, int *const processWaitingTime, float *const processWeightedTurnaround, float *const deviation, int *const processNumber);

int main(int argc, char *argv[])
{
    signal(SIGINT, clearResources);
    initClk();

    //TODO: implement the scheduler.
   // create memory log queue
    struct Queue_MemoryLog * MemoryLogQueue=createQueue_MemoryLog();
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
        Algorithm_SRTN(&logQueue,&MemoryLogQueue, &processorIdleTime, &processWaitingTime, &processWeightedTurnaround, &deviation, &processNumber);
        break;
    case ALGO_HPF:
        Algorithm_HPF(&logQueue,&MemoryLogQueue, &processorIdleTime, &processWaitingTime, &processWeightedTurnaround, &deviation, &processNumber);
        break;
    case ALGO_RR:
        Algorithm_RR(&quantum, &logQueue,&MemoryLogQueue, &processorIdleTime, &processWaitingTime, &processWeightedTurnaround, &deviation, &processNumber);
    default:
        break;
    }

    //Prepare log file
    FILE *schedulerLog;
    schedulerLog = fopen("scheduler.log", "w");
    fprintf(schedulerLog, "#At time x process y state arr w total z remain y wait k\n");
    while (logQueue->front)
    {
        char event[10];

        switch (logQueue->front->event)
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
        fprintf(schedulerLog, "At time %d process %d %s arr %d total %d remain %d wait %d", logQueue->front->time, logQueue->front->id, event, logQueue->front->arrivalTime, logQueue->front->totalTime, logQueue->front->remainingTime, logQueue->front->waitingTime);
        if (strcmp(event, "finished") == 0)
        {
            fprintf(schedulerLog, " TA %d WTA %0.2f\n", logQueue->front->totalTime + logQueue->front->waitingTime, ((float)(logQueue->front->totalTime + logQueue->front->waitingTime) / logQueue->front->totalTime));
        }
        else
        {
            fprintf(schedulerLog, "\n");//terminate the line
        }

        free(dequeue_Log(logQueue));
    }

    fclose(schedulerLog);

    free(logQueue);


     FILE * memoryLog;
    memoryLog = fopen("memory.log", "w");
    fprintf(schedulerLog, "#At time x Allocated y bytes for process z from i to j\n");
    while (MemoryLogQueue->start)
    {
        char m[10];
        switch (MemoryLogQueue->start->m)
        {
        case Allocated:
            strcpy(m, "Allocated");
            break;

        case freed:
            strcpy(m, "freed");
            break;
        default:
            break;
        }
        fprintf(memoryLog, "At time %d %s %d bytes for process %d from %d to %d", MemoryLogQueue->start->time, m, MemoryLogQueue->start->memsize, MemoryLogQueue->start->processId,MemoryLogQueue->start->i,MemoryLogQueue->start->j - 1);
        fprintf(memoryLog, "\n");
        free(dequeue_MemoryLog(MemoryLogQueue));
    }
    fclose(memoryLog);


    //Prepare performance file for writing the performance info
    FILE *schedulerPerf;
    schedulerPerf = fopen("scheduler.perf", "w");

    // scanf("\n");
    fprintf(schedulerPerf, "CPU utilization = %.2f%%\n", ((float)(currentTime - processorIdleTime) * 100 / currentTime));
    fprintf(schedulerPerf, "Avg WTA = %.2f\n", ((float)processWeightedTurnaround / processNumber));
    fprintf(schedulerPerf, "Avg Waiting = %.2f\n", ((float)processWaitingTime / processNumber));
    float storesum=0;
    for(int i=0;i<Each_WTA_counter;i++)storesum+=Each_WTA[i];
    float averageWTA=storesum/processNumber;
    float sumSquared=0;
    for(int i=0;i<Each_WTA_counter;i++)sumSquared+=pow(Each_WTA[i]-averageWTA,2);
    fprintf(schedulerPerf, "Std WTA = %.2f\n", sqrt((sumSquared / processNumber)));

    fclose(schedulerPerf);

    //Teminate process generator
    kill(getppid(),SIGUSR2);


    //TODO: upon termination release the clock resources.

    destroyClk(true);
    exit(0);

}
/*---------------------------------------------------------------SRTN----------------------------------------------------------------*/
void Algorithm_SRTN(struct Queue_Log **const logQueue,struct Queue_MemoryLog **const MemoryLogQueue, int *const processorIdleTime, int *const processWaitingTime, float *const processWeightedTurnaround, float *const deviation, int *const processNumber)
{
        // Create a new queue of processes
    struct Queue_PCB *queue = createQueue_PCB();
        // The process currently running on the processor
    struct Node_PCB *runningPCB = NULL;

    struct Queue_PCB *Waitqueue = createQueue_PCB();

  // The messages received from the message queue
    struct Message_Action receivedAction; // has action and time
    struct Message_Process receivedProcess; //has attached process
    struct Message_Generator receivedStat; //has status

   // memory part
    struct Memory_Node *root = CreateMemoryNode(1024, 0);
    struct Memory_Node *node = NULL;

    //Scheduling started
    msgrcv(msgqID, &receivedAction, sizeof(receivedAction.time) + sizeof(receivedAction.action), getppid(), !IPC_NOWAIT);
    running = true;
    // Keep running while there are still processes in the queue or processes from the generator
    while(running || queue->front){
        //Check if no more processes from process generator
        msgrcv(msgqID, &receivedAction, sizeof(receivedAction.time) + sizeof(receivedAction.action), getppid(), !IPC_NOWAIT);
        if(receivedAction.action==ACT_STOP){
            running = false;
        }
        //Get current time
        currentTime = receivedAction.time;
        printf("Time now: %d \n",currentTime);
        fflush(stdout);

        // Receive the status message from the generator
        msgrcv(msgqID, &receivedStat, sizeof(receivedStat.status), getppid(), !IPC_NOWAIT);
        while(receivedStat.status == GEN_SEND){
                        // Receive the process message from the generator
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

            struct Node_PCB *newPCB = createPCB(processPID, createProcess(receivedProcess.attachedProcess.id, receivedProcess.attachedProcess.arrivalTime, receivedProcess.attachedProcess.runTime, receivedProcess.attachedProcess.priority, receivedProcess.attachedProcess.memsize));
            //timeEnqueue_PCB(queue, newPCB, newPCB->process->runTime);
            node = mem_insert(root, newPCB->process);
            if(node){
                timeEnqueue_PCB(queue, newPCB, newPCB->process->runTime);
                newPCB->memoryNode = node;
                enqueue_MemoryLog(*MemoryLogQueue, createMemoryLog(currentTime, Allocated, node, newPCB));
            }
            else{
                normalEnqueue_PCB(Waitqueue, newPCB);
            }
            // allocate in Memory
           // newPCB->memoryNode = node;
          //  enqueue_MemoryLog(*MemoryLogQueue, createMemoryLog(currentTime, Allocated, node, newPCB));
            msgrcv(msgqID, &receivedStat, sizeof(receivedStat.status), getppid(), !IPC_NOWAIT);
        }

        // Create a new action message to send to the process
        struct Message_Action sendAction;
        //3 cases 
        // 1.No process currently scheduled 
        // 2.The next process isn't the current one 
        // 3.Stick to the current process
        if(!runningPCB){
            runningPCB = queue->front;
            if(runningPCB){
                sendAction.mType = runningPCB->pid;
                sendAction.action = ACT_RUN; // sending process front action
                sendAction.time = currentTime;
                // schedular send front info to running process
                msgsnd(msgqID, &sendAction, sizeof(sendAction.time) + sizeof(sendAction.action), !IPC_NOWAIT);
                // waiting for process to send a front action
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
        else if(runningPCB != queue->front){
            runningPCB->stoppageTime= currentTime;
            enqueue_Log(*logQueue, createLog(currentTime, EV_STOPPED, runningPCB));
            runningPCB = queue->front;
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
            //runningPCB = queue->front;
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
                mem_delete(runningPCB->memoryNode);                    
                enqueue_MemoryLog(*MemoryLogQueue, createMemoryLog(currentTime, freed, runningPCB->memoryNode, runningPCB));
                (*processWaitingTime) += runningPCB->waitingTime;
                (*processWeightedTurnaround) += ((float)((runningPCB->waitingTime + runningPCB->process->runTime)) / runningPCB->process->runTime);
                Each_WTA[Each_WTA_counter++]=((float)((runningPCB->waitingTime + runningPCB->process->runTime)) / runningPCB->process->runTime);
                (*deviation) += pow(((float)(runningPCB->waitingTime + runningPCB->process->runTime) / runningPCB->process->runTime) - (float)((runningPCB->waitingTime + runningPCB->process->runTime)),2);
                (*processNumber)++;
                // current PCB is not pointing to the current process anymore
                free(runningPCB->process);
                // remove current pcb from queue
                free(remove_PCB(queue, runningPCB));
                // running pcb will be the next PCB in queue
                while(Waitqueue->front){
                    struct Node_PCB *newPCB = Waitqueue->front;
                    node = mem_insert(root, newPCB->process);
                    if(node){
                        dequeue_PCB(Waitqueue);
                        timeEnqueue_PCB(queue, newPCB, newPCB->process->runTime);
                        newPCB->memoryNode = node;
                        enqueue_MemoryLog(*MemoryLogQueue, createMemoryLog(currentTime, Allocated, node, newPCB));
                    }
                    else{
                        break;
                    }
                }
                runningPCB = queue->front;
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

                if(running && !runningPCB){ //No processes to schedule but the generator still not finished
                    (*processorIdleTime)++;
                }
            }
        }
    }
    free(queue);
}
/*---------------------------------------------------------------Non-Prempetive HPF--------------------------------------------------*/
void Algorithm_HPF(struct Queue_Log **const logQueue,struct Queue_MemoryLog **const MemoryLogQueue, int *const processorIdleTime, int *const processWaitingTime, float *const processWeightedTurnaround, float *const deviation, int *const processNumber)
{
    struct Queue_PCB *queue = createQueue_PCB();
    struct Node_PCB *runningPCB = NULL;

    struct Queue_PCB *Waitqueue = createQueue_PCB();

    struct Message_Action receivedAction;
    struct Message_Process receivedProcess;
    struct Message_Generator receivedStat;
   // memory part
    struct Memory_Node *root = CreateMemoryNode(1024, 0);
    struct Memory_Node *node = NULL;
    //Start Scheduling
    msgrcv(msgqID, &receivedAction, sizeof(receivedAction.time) + sizeof(receivedAction.action), getppid(), !IPC_NOWAIT);
    running = true;

    while(running || queue->front){
        //Check if no more processes from process generator
        msgrcv(msgqID, &receivedAction, sizeof(receivedAction.time) + sizeof(receivedAction.action), getppid(), !IPC_NOWAIT);
        if(receivedAction.action==ACT_STOP){
            running = false;
        }
        //Get current time
        currentTime = receivedAction.time;
        printf("Time now: %d \n",currentTime);
        fflush(stdout);
        // functions for testing
        // printf("jimmy %d",currentTime);

        //we now receive the status that's reported on the message queue between the process_generator and the scheduler
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

            struct Node_PCB *newPCB = createPCB(processPID, createProcess(receivedProcess.attachedProcess.id, receivedProcess.attachedProcess.arrivalTime, receivedProcess.attachedProcess.runTime, receivedProcess.attachedProcess.priority, receivedProcess.attachedProcess.memsize));
            //priorityEnqueue_PCB(queue, newPCB, newPCB->process->priority);
            node = mem_insert(root, newPCB->process);
            if(node){
                priorityEnqueue_PCB(queue, newPCB, newPCB->process->priority);
                newPCB->memoryNode = node;
                enqueue_MemoryLog(*MemoryLogQueue, createMemoryLog(currentTime, Allocated, node, newPCB));
            }
            else{
                normalEnqueue_PCB(Waitqueue, newPCB);
            }
            // allocate in Memory
            //newPCB->memoryNode = node;
            //enqueue_MemoryLog(*MemoryLogQueue, createMemoryLog(currentTime, Allocated, node, newPCB));
            msgrcv(msgqID, &receivedStat, sizeof(receivedStat.status), getppid(), !IPC_NOWAIT);
        }

        struct Message_Action sendAction;
        //2 cases 
        // 1.No process currently scheduled 
        // 2.Stick to the current process
        //allocating to a new process
        if(!runningPCB){
            runningPCB = queue->front;
            if(runningPCB){
                sendAction.mType = runningPCB->pid;
                sendAction.action = ACT_RUN; // sending process start action
                sendAction.time = currentTime;
                // schedular send start info to running process
                msgsnd(msgqID, &sendAction, sizeof(sendAction.time) + sizeof(sendAction.action), !IPC_NOWAIT);
                // waiting for process to send a start action
                msgrcv(msgqID, &receivedAction, sizeof(receivedAction.time) + sizeof(receivedAction.action), getpid(), !IPC_NOWAIT);
                // we update the waiting time of the process
                //by jimmy
                runningPCB->waitingTime = currentTime - runningPCB->process->arrivalTime;
                enqueue_Log(*logQueue, createLog(currentTime, EV_STARTED, runningPCB));
                //we update the remaining time
                runningPCB->remainingTime = receivedAction.time; 
            }
            else{
                (*processorIdleTime)++;
            }
             
        }
        else{
            //runningPCB = queue->front;
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
                //now we compute the statistics 
                enqueue_Log(*logQueue, createLog(currentTime, EV_FINISHED, runningPCB));
                mem_delete(runningPCB->memoryNode);                    
                enqueue_MemoryLog(*MemoryLogQueue, createMemoryLog(currentTime, freed, runningPCB->memoryNode, runningPCB));
                (*processWaitingTime) += runningPCB->waitingTime;
                (*processWeightedTurnaround) += ((float)((runningPCB->waitingTime + runningPCB->process->runTime)) / runningPCB->process->runTime);
                Each_WTA[Each_WTA_counter++]=((float)((runningPCB->waitingTime + runningPCB->process->runTime)) / runningPCB->process->runTime);
                (*deviation) += pow(((float)(runningPCB->waitingTime + runningPCB->process->runTime) / runningPCB->process->runTime) - (float)((runningPCB->waitingTime + runningPCB->process->runTime)),2);

                (*processNumber)++;
                // current PCB is not pointing to the current process anymore
                free(runningPCB->process);
                // remove current pcb from queue
                free(remove_PCB(queue, runningPCB));
                // running pcb will be the next PCB in queue
                while(Waitqueue->front){
                    struct Node_PCB *newPCB = Waitqueue->front;
                    node = mem_insert(root, newPCB->process);
                    if(node){
                        dequeue_PCB(Waitqueue);
                        priorityEnqueue_PCB(queue, newPCB, newPCB->process->priority);
                        newPCB->memoryNode = node;
                        enqueue_MemoryLog(*MemoryLogQueue, createMemoryLog(currentTime, Allocated, node, newPCB));
                    }
                    else{
                        break;
                    }
                }
                runningPCB = queue->front;
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

                if(running && !runningPCB){ //No processes to schedule but the generator still not finished
                    (*processorIdleTime)++;
                } 
            }
        }
    }
    free(queue);
}
/*---------------------------------------------------------------RR------------------------------------------------------------------*/
void Algorithm_RR(int *const quantum, struct Queue_Log **const logQueue,struct Queue_MemoryLog **const MemoryLogQueue, int *const processorIdleTime, int *const processWaitingTime, float *const processWeightedTurnaround, float *const deviation, int *const processNumber){
    struct Queue_PCB *queue = createQueue_PCB();
    struct Node_PCB *runningPCB = NULL;

    struct Queue_PCB *Waitqueue = createQueue_PCB();

    struct Message_Action receivedAction;
    struct Message_Process receivedProcess;
    struct Message_Generator receivedStat;
    int q = 0;
   // memory part
    struct Memory_Node *root = CreateMemoryNode(1024, 0);
    struct Memory_Node *node = NULL;

    //Start Scheduling
    msgrcv(msgqID, &receivedAction, sizeof(receivedAction.time) + sizeof(receivedAction.action), getppid(), !IPC_NOWAIT);
    running = true;

    while(running || queue->front){
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

            struct Node_PCB *newPCB = createPCB(processPID, createProcess(receivedProcess.attachedProcess.id, receivedProcess.attachedProcess.arrivalTime, receivedProcess.attachedProcess.runTime, receivedProcess.attachedProcess.priority, receivedProcess.attachedProcess.memsize));
            //normalEnqueue_PCB(queue, newPCB);
            node = mem_insert(root, newPCB->process);
            if(node){
                normalEnqueue_PCB(queue, newPCB);
                newPCB->memoryNode = node;
                enqueue_MemoryLog(*MemoryLogQueue, createMemoryLog(currentTime, Allocated, node, newPCB));
            }
            else{
                normalEnqueue_PCB(Waitqueue, newPCB);
            }
            // allocate in Memory
            //newPCB->memoryNode = node;
            //enqueue_MemoryLog(*MemoryLogQueue, createMemoryLog(currentTime, Allocated, node, newPCB));
            msgrcv(msgqID, &receivedStat, sizeof(receivedStat.status), getppid(), !IPC_NOWAIT);
        }

        struct Message_Action sendAction;
        //2 cases 1.No process currently scheduled 2.Stick to the current process(check if finished or quantum passed)
        if(!runningPCB){
            runningPCB = queue->front;
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
                while(Waitqueue->front){
                    struct Node_PCB *newPCB = Waitqueue->front;
                    node = mem_insert(root, newPCB->process);
                    if(node){
                        dequeue_PCB(Waitqueue);
                        normalEnqueue_PCB(queue, newPCB);
                        newPCB->memoryNode = node;
                        enqueue_MemoryLog(*MemoryLogQueue, createMemoryLog(currentTime, Allocated, node, newPCB));
                    }
                    else{
                        break;
                    }
                }
                struct Node_PCB *nextPCB = NULL;
                if(runningPCB==queue->rear){
                    nextPCB = queue->front;
                    //points to the next of runningprocess as I'll free runningPCB 
                }
                else{
                    nextPCB = runningPCB->next;
                }
                enqueue_Log(*logQueue, createLog(currentTime, EV_FINISHED, runningPCB));
                mem_delete(runningPCB->memoryNode);                    
                enqueue_MemoryLog(*MemoryLogQueue, createMemoryLog(currentTime, freed, runningPCB->memoryNode, runningPCB));
                (*processWaitingTime) += runningPCB->waitingTime;
                (*processWeightedTurnaround) += ((float)((currentTime)-runningPCB->process->arrivalTime) / runningPCB->process->runTime);
                Each_WTA[Each_WTA_counter++]=((float)((currentTime)-runningPCB->process->arrivalTime) / runningPCB->process->runTime);
                // Each_WTA[Each_WTA_counter++]=(*processWeightedTurnaround);
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
                if(runningPCB && queue->front){ //To ensure the next process is in the queue
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
                else{
                    runningPCB = NULL; //Free current process
                    //nothing works now
                }

                if(running && !runningPCB){ //No processes to schedule but the generator still not finished
                    (*processorIdleTime)++;
                }
            }
            else if(q == *quantum){
                struct Node_PCB *nextPCB = NULL;
                if(runningPCB==queue->rear){
                    nextPCB = queue->front;
                }
                else{
                    nextPCB = runningPCB->next;
                }
                // if quantum has finished but the current process is the only one in the queue
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
                //neither quantum nor the process had finished
                sendAction.mType = runningPCB->pid;
                sendAction.action = ACT_RUN; // sending process start action
                sendAction.time = currentTime;
                // schedular send start info to running process
                msgsnd(msgqID, &sendAction, sizeof(sendAction.time) + sizeof(sendAction.action), !IPC_NOWAIT);
                // waiting for process to send a front action
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

