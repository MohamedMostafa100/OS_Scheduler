#include "headers.h"

void clearResources(int);
void terminate(int);
int msgqID;
int currentTime = 0;
int clkTime = 0;
bool finish = false;

int main(int argc, char *argv[])
{
    signal(SIGINT, clearResources);
    signal(SIGUSR2,terminate);
    // TODO Initialization
    // 1. Read the input files.
    struct Queue_Process *processQueue = createQueue_Process();

    if (argc < 2)
    {
        perror("Error! Please make sure you added a correct input file name as your first argument\n");
        exit(1);
    }
    //printf("%s \n",argv[1]);
    FILE *inputFile = fopen(argv[1], "r");
    if (!inputFile)
    {
        perror("Error! Please make sure you added a correct input file name as your first argument\n");
        exit(1);
    }
    int id;
    int arrivalTime;
    int runTime;
    int priority;
    while(true) //While loop to read from the file
    {
        int result = fscanf(inputFile, "%d\t%d\t%d\t%d", &id, &arrivalTime, &runTime, &priority);

        if (result == EOF)  //If at end oof file, break out of loop
        {
            break;
        }
        else if (result == 4)   //If line is correct (found 4 integers), enqueue a new process
        {
            struct Node_Process * newProcess=createProcess(id, arrivalTime, runTime, priority);
            enqueue_Process(processQueue, newProcess);
        }
        else    //Skip this line
        {
            while (fgetc(inputFile) != '\n');
        }
    }
    if (processQueue->start == NULL)
    {
        perror("Error! Please make sure you added a correct input file name as your first argument\n");
        exit(1);
    }
    // 2. Read the chosen scheduling algorithm and its parameters, if there are any from the argument list.
    enum Algorithm schedulingAlgorithm;
    int quantum = -1;
    {
        if (argc < 4)
        {
            perror("Error! Please make sure you chose a scheduling algorithm\n");
            exit(1);
        }
        if (strcmp(argv[2], "-sch") != 0)
        {
            perror("Error! Please make sure you chose a scheduling algorithm\n");
            exit(1);
        }

        int sch = atoi(argv[3]);
        if (sch < (int)ALGO_SRTN || sch > (int)ALGO_RR)
        {
            perror("Error! Please make sure you chose a correct scheduling algorithm\n");
            exit(1);
        }

        schedulingAlgorithm = (enum Algorithm)sch;
        if (schedulingAlgorithm == ALGO_RR)
        {
            if (argc < 6)
            {
                perror("Error! Please specify a quantum size for Round Robin Algorithm\n");
                exit(1);
            }
            if (strcmp(argv[4], "-q") != 0)
            {
                perror("Error! Please specify a quantum size for Round Robin Algorithm\n");
                exit(1);
            }

            int q = atoi(argv[5]);
            if (q <= 0)
            {
                perror("Error! Please specify a correct quantum size for Round Robin Algorithm\n");
                exit(1);
            }
            quantum = q;
        }
        else
        {
            if (argc > 4)
            {
                perror("Error! This scheduling algorithm doesn't take any parameters\n");
                exit(1);
            }
        }

        if (argc > 6)
        {
            perror("Error! Too many arguments\n");
            exit(1);
        }
    }
    // 3. Initiate and create the scheduler and clock processes.
    pid_t clockPID = fork();
    if (clockPID == -1)
    {
        perror("Error creating clock process!\n");
        exit(1);
    }
    else if (clockPID == 0)
    {
        execl("clk.out", "clk.out", NULL);
        perror("Error creating clock process!\n");
        exit(1);
    }

    pid_t schedulerPID = fork();
    if (schedulerPID == -1)
    {
        perror("Error creating scheduler process!\n");
        exit(1);
    }
    if (schedulerPID == 0)
    {
        char sch[5];
        sprintf(sch, "%d", schedulingAlgorithm);
        char q[5];
        sprintf(q, "%d", quantum);

        execl("scheduler.out", "scheduler.out", sch, q, NULL);
        perror("Error creating scheduler process!\n");
        exit(1);
    }

    // 4. Use this function after creating the clock process to initialize clock.
    initClk();
    // To get time use this function. 
    //int x = getClk();
    //printf("Current Time is %d\n", x);
    // TODO Generation Main Loop
    // 5. Create a data structure for processes and provide it with its parameters.
    // 6. Send the information to the scheduler at the appropriate time.
    key_t msgqKey = ftok("keyfile.txt", MQKEY);
    msgqID = msgget(msgqKey, 0666 | IPC_CREAT);
    if (msgqID == -1)
    {
        perror("Error creating message queue");
        exit(1);
    }


    struct Message_Process sentProcess;
    struct Message_Generator stat; 

    struct Message_Action sentAction;

    sentAction.mType = getpid();
    sentAction.action = ACT_RUN;
    sentAction.time = currentTime;

    //Send "start" action to scheduler
    msgsnd(msgqID, &sentAction, sizeof(sentAction.time) + sizeof(sentAction.action), !IPC_NOWAIT);
    //bool sending = true;


    while (true) //If process queue is not empty
    {
        if (currentTime < clkTime)  //If the process generator is late and should execute it's code
        {
            if(!processQueue->start){
                sentAction.mType = getpid();
                sentAction.action = ACT_STOP;
                sentAction.time = currentTime;
                //Send "stop" action to scheduler
                msgsnd(msgqID, &sentAction, sizeof(sentAction.time) + sizeof(sentAction.action), !IPC_NOWAIT);
            }
            else{
                sentAction.mType = getpid();
                sentAction.action = ACT_RUN;
                sentAction.time = currentTime; 
                msgsnd(msgqID, &sentAction, sizeof(sentAction.time) + sizeof(sentAction.action), !IPC_NOWAIT);
            }

            while (processQueue->start && processQueue->start->arrivalTime <= currentTime)  //If the current process should be sent out (Arrival time <= Current time)
            {
                sentProcess.mType = getpid();
                sentProcess.attachedProcess.id = processQueue->start->id;
                sentProcess.attachedProcess.arrivalTime = processQueue->start->arrivalTime;
                sentProcess.attachedProcess.runTime = processQueue->start->runTime;
                sentProcess.attachedProcess.priority = processQueue->start->priority;
                free(dequeue_Process(processQueue));
                stat.mType = getpid();
                stat.status = GEN_SEND;
                msgsnd(msgqID, &stat, sizeof(stat.status), !IPC_NOWAIT);
                msgsnd(msgqID, &sentProcess, sizeof(sentProcess.attachedProcess), !IPC_NOWAIT);
            }
            stat.mType = getpid();
            stat.status = GEN_STALL;
            msgsnd(msgqID, &stat, sizeof(stat.status), !IPC_NOWAIT);

            currentTime++;
        }
        clkTime = getClk();
        //kill(schedulerPID,SIGUSR1);
        if(finish){
            break;
        }
    }
    // 7. Clear clock resources
    destroyClk(true);
}

void terminate(int signum){
    finish=true;
}

void clearResources(int signum)
{
    //TODO Clears all resources in case of interruption
    msgctl(msgqID, IPC_RMID, (struct msqid_ds *)0);
    destroyClk(true);
    exit(0);
}
