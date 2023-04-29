#include "headers.h"

/* Modify this file as needed*/
int msgqID;
int remainingtime;

int main(int agrc, char *argv[])
{
    initClk();

    //TODO The process needs to get the remaining time from somewhere
    remainingtime = atoi(argv[1]);

    key_t msgqKey = ftok("keyfile.txt", MQKEY);
    msgqID = msgget(msgqKey, 0666 | IPC_CREAT);
    if (msgqID == -1)
    {
        perror("Error creating message queue");
        exit(1);
    }

    struct Message_Action receivedAction;

    while (true)
    {
        msgrcv(msgqID, &receivedAction,sizeof(receivedAction.time) + sizeof(receivedAction.action), getpid(), !IPC_NOWAIT);
        if(receivedAction.action == ACT_RUN){
            remainingtime--;
        }
        struct Message_Action sentAction;
        sentAction.mType = getppid();
        sentAction.action = ACT_RUN;
        sentAction.time = remainingtime;

        msgsnd(msgqID, &sentAction, sizeof(sentAction.action) + sizeof(sentAction.time), !IPC_NOWAIT);


        if(remainingtime == 0){
            struct Message_Action sentAction;
            sentAction.mType = getppid();
            sentAction.action = ACT_STOP;
            sentAction.time = remainingtime;
            msgsnd(msgqID, &sentAction, sizeof(sentAction.action) + sizeof(sentAction.action), !IPC_NOWAIT);
            break;
        }
    }

    destroyClk(false);
    exit(0);

    return 0;
}
