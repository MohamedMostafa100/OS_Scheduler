#include <stdio.h>
/*====================================================================================================*/

struct Node_Process
{
    int id;
    int arrivalTime;
    int runTime;
    int priority;
    int memsize;
    struct Node_Process *next;
};

struct Node_Process *createProcess(int _id, int arrTime, int _runTime, int _priority,int _memsize)
{
    struct Node_Process *newProcess = malloc(sizeof(struct Node_Process));
    newProcess->id = _id;
    newProcess->arrivalTime = arrTime;
    newProcess->runTime = _runTime;
    newProcess->priority = _priority;
    newProcess->memsize=_memsize;
    newProcess->next = NULL;
    return newProcess;
}

/*====================================================================================================*/
//a linked list implementation of a queue of processes
struct Queue_Process
{
    struct Node_Process *front;
    struct Node_Process *rear;
};

struct Queue_Process *createQueue_Process()
{
    struct Queue_Process *newQueue = malloc(sizeof(struct Queue_Process));
    newQueue->front = NULL;
    newQueue->rear = NULL;
    return newQueue;
}

void enqueue_Process(struct Queue_Process *const q, struct Node_Process *const process)
{
    if (q->rear == NULL)
    {
        q->front = process;
        q->rear = process;
    }
    else
    {
        q->rear->next = process;
        q->rear = process;
    }
}

struct Node_Process *dequeue_Process(struct Queue_Process *const q)
{
    struct Node_Process *process = NULL;
    if (q->front)
    {
        process = q->front;
        q->front = q->front->next;
        if (q->front == NULL)
            q->rear = NULL;
    }
    return process;
}

// needed enums used elsewhere in the project
/*====================================================================================================*/
enum Algorithm
{
    ALGO_SRTN = 1,
    ALGO_HPF,
    ALGO_RR
};
enum Action
{
    ACT_RUN = 1,
    ACT_STOP
};
enum gStatus
{
    GEN_SEND = 1,
    GEN_STALL
};

struct Message_Action
{
    long mType;
    int time;
    enum Action action;
};

struct Message_Process
{
    long mType;
    struct Node_Process attachedProcess;
};

struct Message_Generator
{
    long mType;
    enum gStatus status; 
};
/*====================================================================================================*/

struct Node_PCB
{
    struct Node_Process *process;
    struct Memory_Node *memoryNode;
    pid_t pid;
    int priority;
    int remainingTime;
    int executionTime;
    int waitingTime;
    int stoppageTime;
    int memsize;
    struct Node_PCB *next;
};

struct Node_PCB *createPCB(pid_t _pid, struct Node_Process *const _process)
{
    struct Node_PCB *newPCB = malloc(sizeof(struct Node_PCB));
    newPCB->process = _process;
    newPCB->pid = _pid;
    newPCB->priority = _process->priority;
    newPCB->remainingTime = _process->runTime;
    newPCB->executionTime = 0;
    newPCB->waitingTime = 0;
    newPCB->stoppageTime = 0;
    newPCB->memsize=_process->memsize;
    newPCB->next = NULL;
    return newPCB;
}

/*====================================================================================================*/

struct Queue_PCB
{
    struct Node_PCB *front;
    struct Node_PCB *rear;
};

struct Queue_PCB *createQueue_PCB()
{
    struct Queue_PCB *newQueue = malloc(sizeof(struct Queue_PCB));
    newQueue->front = NULL;
    newQueue->rear = NULL;
    return newQueue;
}

void normalEnqueue_PCB(struct Queue_PCB *const q, struct Node_PCB *const PCB)
{
    if (q->rear == NULL)
    {
        q->front = PCB;
        q->rear = PCB;
    }
    else
    {
        q->rear->next = PCB;
        q->rear = PCB;
    }
}

void priorityEnqueue_PCB(struct Queue_PCB *const q, struct Node_PCB *const PCB, int priority)
{
    if (q->rear == NULL)
    {
        q->front = PCB;
        q->rear = PCB;
    }
    else if(q->front->priority > priority)
    {
        PCB->next = q->front;
        q->front = PCB;
    }
    else
    {
        struct Node_PCB *current = q->front;
        while (current->next && current->next->priority <= priority)
        {
            current = current->next;
        }

        if (!current->next)
        {
            q->rear = PCB;
        }

        PCB->next = current->next;
        current->next = PCB;
    }
}

void timeEnqueue_PCB(struct Queue_PCB *const q, struct Node_PCB *const PCB, int time)
{
    if (q->rear == NULL)
    {
        q->front = PCB;
        q->rear = PCB;
    }
    else if(q->front->remainingTime > time)
    {
        PCB->next = q->front;
        q->front = PCB;
    }
    else
    {
        struct Node_PCB *current = q->front;
        while (current->next && current->next->remainingTime <= time)
        {
            current = current->next;
        }

        if (!current->next)
        {
            q->rear = PCB;
        }

        PCB->next = current->next;
        current->next = PCB;
    }
}



struct Node_PCB *remove_PCB(struct Queue_PCB *const q, struct Node_PCB *const PCB)
{
    if (q->front == PCB)
    {
        q->front = q->front->next;
        if (q->front == NULL)
            q->rear = NULL;
    }
    else
    {
        struct Node_PCB *current = q->front;
        while (current->next && current->next != PCB)
        {
            current = current->next;
        }
        if (!current->next)
            return NULL;
        if (current->next == q->rear)
            q->rear = current;
        current->next = current->next->next;
    }

    PCB->next = NULL;
    return PCB;
}
struct Node_PCB *dequeue_PCB(struct Queue_PCB *const q)
{
    struct Node_PCB *PCB = NULL;
    if (q->front)
    {
        PCB = q->front;
        q->front = q->front->next;
        if (q->front == NULL)
            q->rear = NULL;
    }
    
    PCB->next = NULL;
    return PCB;
}
/*====================================================================================================*/
/*====================================================================================================*/
/*====================================================================================================*/
enum Event
{
    EV_STARTED = 1,
    EV_RESUMED,
    EV_STOPPED,
    EV_FINISHED
};


struct Node_Log
{
    int time;
    enum Event event;
    int id;
    int arrivalTime;
    int totalTime;
    int remainingTime;
    int waitingTime;
    struct Node_Log *next;
};

struct Node_Log *createLog(int _time, enum Event _event, struct Node_PCB *const _PCB)
{
    struct Node_Log *newLog = malloc(sizeof(struct Node_Log));
    newLog->time = _time;
    newLog->event = _event;
    newLog->id = _PCB->process->id;
    newLog->arrivalTime = _PCB->process->arrivalTime;
    newLog->totalTime = _PCB->process->runTime;
    newLog->remainingTime = _PCB->remainingTime;
    newLog->waitingTime = _PCB->waitingTime;
    newLog->next = NULL;
    return newLog;
}
/*====================================================================================================*/

struct Queue_Log
{
    struct Node_Log *front;
    struct Node_Log *rear;
};

struct Queue_Log *createQueue_Log()
{
    struct Queue_Log *newQueue = malloc(sizeof(struct Queue_Log));
    newQueue->front = NULL;
    newQueue->rear = NULL;
    return newQueue;
}

void enqueue_Log(struct Queue_Log *const q, struct Node_Log *const log)
{
    if (q->rear == NULL)
    {
        q->front = log;
        q->rear = log;
    }
    else
    {
        q->rear->next = log;
        q->rear = log;
    }
}

struct Node_Log *dequeue_Log(struct Queue_Log *const q)
{
    struct Node_Log *log = NULL;
    if (q->front)
    {
        log = q->front;
        q->front = q->front->next;
        if (q->front == NULL)
            q->rear = NULL;
    }
    return log;
}
/*====================================================================================================*/

enum Memory_Event
{
    Allocated=1,
    freed
};
struct Memory_Node
{
    int memsizeOccupied;
    int memsize;
    int processId;
    enum Memory_Event m;
    int i; //start
    int j; //end
    int isfull;
    int isLeftChild;
    int isRightChild;
    struct Memory_Node* parent;
    struct Memory_Node *left;
    struct Memory_Node *right;
    struct Memory_Node *LeftSibling;
    struct Memory_Node *RightSibling;
};

/*====================================================================================================*/

///////////////////////////////////////////////////Memoryy///////////////////////////////////////////////////////

struct Memory_Node* CreateMemoryNode(int value,int start)
{
    struct Memory_Node *newNode = malloc(sizeof(struct Memory_Node));
    if(newNode!=NULL)
    {
        newNode->memsize=value;
        newNode->left=NULL;
        newNode->right=NULL;
        newNode->LeftSibling=NULL;
        newNode->RightSibling=NULL;
        newNode->i=start;
        newNode->isfull=0;
        newNode->j=newNode->memsize+newNode->i;
        newNode->isLeftChild=0;
        newNode->isRightChild=0;
    }
    return newNode;
}

//Insert new node into the memory
struct Memory_Node* mem_insert(struct Memory_Node*  Node,struct Node_Process* process) 
{
    int value=process->memsize;
    
    //If process is bigger than current memory node, return null
    if (Node->memsize < value)
    {
        return NULL;
    }

    //If the Node doesn't have any children (Is not split)
    if (Node->left == NULL && Node->right == NULL)
    {
        //If this is the correct level (Memory Node size >= Process Size > 1/2 Memory Node Size) and this is free memory (not a process)
        if (Node->memsize >= value && (Node->memsize/2) < value && Node->isfull !=1 )
        {
            //Add the process to memory
            Node->isfull=1;
            Node->processId=process->id;
            return Node;
        }
        //Else if this is free memory (not a process), Split it
        else if (Node->isfull != 1) {
            if ((Node->memsize / 2) != 0)
            {
                Node->left = CreateMemoryNode((Node->memsize) / 2, (Node->i));
                Node->right = CreateMemoryNode((Node->memsize) / 2, (Node->i) + (Node->memsize) / 2);
                Node->left->isLeftChild=1;
                Node->right->isRightChild=1;
                Node->left->parent=Node->right->parent=Node;
                Node->left->RightSibling=Node->right;
                Node->right->LeftSibling=Node->left;
            }
        }
        //Else this is a process and cannot be split, So it cannot be placed here
        else 
        {
            return NULL;
        }
    }

    //Check both children of the current Memory Node (If they exist)
    struct Memory_Node* check=NULL;
    check = mem_insert(Node->left, process);   
    if (check == NULL)
        check = mem_insert(Node->right, process);
    return check;
}

//still working on it
void mem_delete(struct Memory_Node*  Node)
{
    //Set the node to be free space
    Node->isfull=0;

    //If this is a left child
    if(Node->isLeftChild==1)
    {
        //If it's right sibling is free memory (not a process) and doesn't have any children (is not split), merge the two by deleting them
        if(Node->RightSibling->isfull==0 && (Node->RightSibling->left == NULL && Node->RightSibling->right == NULL))
        {
            Node->parent->right=NULL;
            Node->parent->left=NULL;
            mem_delete(Node->parent);
        }
    
    }
    //If this is a right child
    else if(Node->isRightChild==1)
    {
        //If it's left sibling is free memory (not a process) and doesn't have any children (is not split), merge the two by deleting them
        if(Node->LeftSibling->isfull==0 && (Node->LeftSibling->left == NULL && Node->LeftSibling->right == NULL))
        {
            Node->parent->left=NULL;
            Node->parent->right=NULL;
            mem_delete(Node->parent);
        }
    }
}
struct Queue_Memory
{
    struct Memory_Node *start;
    struct Memory_Node *end;
};

struct Queue_Memory *createQueue_Memory()
{
    struct Queue_Memory *newQueue = malloc(sizeof(struct Queue_Memory));
    newQueue->start = NULL;
    newQueue->end = NULL;
    return newQueue;
}

void allocate_process_in_memory(struct Memory_Node* node,struct Node_Process* newProcess)
{    
}
struct Memory_Log
{
    int i;
    int j;
    int memsize;
    int time;
    int processId;
    enum Memory_Event m;
   // int remainingMemory;
    struct Memory_Log * Next;
};
struct Memory_Log *createMemoryLog(int _time, enum Memory_Event _m,struct Memory_Node* node, struct Node_PCB *const _PCB)
{
    struct Memory_Log *newLog = malloc(sizeof(struct Memory_Log));
    newLog->time = _time;
    newLog->processId = _PCB->process->id;
    newLog->i=node->i;
    newLog->j=node->j;
    newLog->memsize=_PCB->memsize;
    newLog->m=_m;
    newLog->Next=NULL;
    return newLog;
}
struct Queue_MemoryLog
{
    struct Memory_Log *start;
    struct Memory_Log *end;
};
struct Queue_MemoryLog *createQueue_MemoryLog()
{
    struct Queue_MemoryLog *newQueue = malloc(sizeof(struct Queue_MemoryLog));
    newQueue->start = NULL;
    newQueue->end = NULL;
    return newQueue;
}
void enqueue_MemoryLog(struct Queue_MemoryLog *const queue, struct Memory_Log *const log)
{
    if (queue->end == NULL)
    {
        queue->start = log;
        queue->end = log;
    }
    else
    {
        queue->end->Next = log;
        queue->end = log;
    }
}

struct Memory_Log *dequeue_MemoryLog(struct Queue_MemoryLog *const queue)
{
    struct Memory_Log *log = NULL;
    if (queue->start)
    {
        log = queue->start;
        queue->start = queue->start->Next;
        if (queue->start == NULL)
            queue->end = NULL;
    }
    return log;
}
/*====================================================================================================*/