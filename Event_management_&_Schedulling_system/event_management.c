#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_EVENTS 100

/* ================================== PARTICIPANT LINKED LIST ================================== */

typedef struct Participant {        //typedef is basically using to not write "struct" all the time
    int participantID;
    char name[50];
    struct Participant* next;
} Participant;                      //Here Participant becomes alias, so we can use it without "struct"

/* ======================================== EVENT STRUCTURE =================================================== */

typedef struct Event {
    int eventID;
    char name[50];
    int priority;      // 1 = High, 2 = Medium, 3 = Low
    int startTime;     // Example: 1300 = 1:00 PM
    int endTime;
    int resourceID;
    int Cancelled;

    //Used linked list to manage participants
    Participant* participantHead;
    int participantCount;
    int maxParticipants;
} Event;

/* ================================== RESOURCE BST =================================================== */

typedef struct Resource {
    int resourceID;
    char name[50];
    struct Resource* left;
    struct Resource* right;
} Resource;

/* ================================== PRIORITY QUEUE =================================================== */

typedef struct {
    Event* events[MAX_EVENTS];
    int size;
} PriorityQueue;

/* ========================================= GLOBAL VARIABLES =================================================== */

Event eventList[MAX_EVENTS];
int totalEvents = 0;

PriorityQueue pq;               // pq is a structure for managing event priorities
Resource* root = NULL;

void freeParticipants(Event* e);

/* ============================================================================================================================= */
/* =================================================== RESOURCE BST FUNCTIONS =================================================== */

Resource* createResource(int id, char name[]) {                     //Why "Resource*" instead of void. Because we will use this function later. It must give return a value. "void" doesn't return anything.
    Resource* newNode = (Resource*)malloc(sizeof(Resource));
    newNode->resourceID = id;
    strcpy(newNode->name, name);        //"strcpy" copies character by character until \0
    newNode->left = NULL;
    newNode->right = NULL;
    return newNode;
}

Resource* insertResource(Resource* root, int id, char name[]) {         
    if (root == NULL) {
        return createResource(id, name);
    }

    if (id < root->resourceID) {
        root->left = insertResource(root->left, id, name);
    }
    else if (id > root->resourceID) {
        root->right = insertResource(root->right, id, name);
    }

    return root;
}

Resource* searchResource(Resource* root, int id) {
    if (root == NULL || root->resourceID == id)
        return root;

    if (id < root->resourceID) {
        return searchResource(root->left, id);
    } else {
        return searchResource(root->right, id);
    }
}

void searchResourceMenu(){
    int id;
    printf("Enter Resource ID: ");
    scanf("%d",&id);

    Resource* r = searchResource(root,id);

    if(r!=NULL)
        printf("Found: %d | %s\n", r->resourceID, r->name);
    else
        printf("Resource not found\n");
}

void inorderResources(Resource* root) {
    if (root != NULL) {
        inorderResources(root->left);
        printf("Resource ID: %d | Name: %s\n", root->resourceID, root->name);
        inorderResources(root->right);
    }
}

void freeResource(Resource* root){
    if(root==NULL) return;

    freeResource(root->left);
    freeResource(root->right);

    free(root);
}

/* ============================================================================================================================= */
/* ========================================================= PRIORITY QUEUE FUNCTIONS ================================================== */

//Used Array based Heap Functions to maximize efficiency. It's time complexity is lowest compared to other implementations. It is suggested.
//Heap allows O(log n) insertion and removal while always keeping the highest priority event accessible at the root.

void swap(Event** a, Event** b) {           //We can't change pointers without double pointer
    Event* temp = *a;
    *a = *b;
    *b = temp;
}

void heapifyUp(PriorityQueue* pq, int i) {                  //Fix the heap property by moving a node upward.
    while (i>0) {
        int parent=(i-1)/2;

        if (pq->events[parent]->priority > pq->events[i]->priority) {       //Less priority value means higher urgency.If child has higher priority than parent, swap them. This function basically sorts the events based on their priority.
            swap(&pq->events[parent], &pq->events[i]);
            i=parent;
        }
        else break;
    }
}

void heapifyDown(PriorityQueue* pq, int i) {                //Fix the heap property by moving a node downward. When "removeFromPQ" is called, this function is used to fix the priorities.
    int smallest=i;
    int left=2*i+1;
    int right=2*i+2;

    if (left < pq->size && pq->events[left]->priority < pq->events[smallest]->priority) {
        smallest = left;
    }

    if (right < pq->size && pq->events[right]->priority < pq->events[smallest]->priority) {
        smallest = right;
    }

    if (smallest != i) {
        swap(&pq->events[smallest], &pq->events[i]);        //swapping the parent with the smallest child.
        heapifyDown(pq, smallest);                        //recursively fix the heap property
    }
}

void insertIntoPQ(PriorityQueue* pq, Event* event) {
    if (pq->size >= MAX_EVENTS) {
        printf("Priority Queue Full!\n");
        return;
    }

    pq->events[pq->size] = event;
    heapifyUp(pq, pq->size);
    pq->size++;
}

Event* removeFromPQ(PriorityQueue* pq) {
    if (pq->size == 0)
        return NULL;

    Event* top = pq->events[0];                     //Save the root before overwritting it
    pq->events[0] = pq->events[pq->size - 1];       //Move the last element to the root
    pq->size--;                                     //last element removed. Logically the top element now is gone.
    heapifyDown(pq, 0);                           //fix the heap property

    return top;                 //return the top
}

/* ================================================================================================================================================================ */
/* =================================================== EVENT FUNCTIONS =========================================================================================== */

Event* searchEventByID(int id) {
    for (int i = 0; i < totalEvents; i++) {
        if (eventList[i].eventID == id && eventList[i].Cancelled == 0)
            return &eventList[i];
    }
    return NULL;
}

int validTime(int t){

    int hour = t / 100;
    int min = t % 100;

    if(hour < 0 || hour > 23)
        return 0;

    if(min < 0 || min > 59)
        return 0;

    return 1;
}

void printTime(int t){
    int hour = t / 100;
    int min = t % 100;

    printf("%02d:%02d", hour, min);
}

int checkTimeConflict(int start, int end, int resourceID) {
    for (int i = 0; i < totalEvents; i++) {
        if (eventList[i].Cancelled == 0 && eventList[i].resourceID == resourceID) {

            if (eventList[i].startTime < end &&
                start < eventList[i].endTime)
                return 1;       // conflict
        }
    }
    return 0;
}

void addEvent() {
    if (totalEvents >= MAX_EVENTS) {
        printf("Event limit reached!\n");
        return;
    }

    Event* e = &eventList[totalEvents];

    printf("Enter Event ID: ");
    scanf("%d", &e->eventID);

    /* Check duplicate ID immediately */
    if(searchEventByID(e->eventID)!=NULL) {
    printf("Event ID already exists!\n");
    return;
    }

    printf("Enter Event Name: ");
    scanf(" %[^\n]", e->name);                      //" %[^\n]" is used to read strings with spaces

    printf("Enter Priority (1=High,2=Medium,3=Low): ");
    scanf("%d", &e->priority);


    printf("Enter Start Time (e.g., 1300): ");
    scanf("%d", &e->startTime);

    printf("Enter End Time: ");
    scanf("%d", &e->endTime);

    printf("Enter Resource ID: ");
    scanf("%d", &e->resourceID);

    if (searchResource(root, e->resourceID) == NULL) {
        printf("Resource does not exist!\n");
        return;
    }

    if(e->priority <1 || e->priority >3){
    printf("Invalid priority!\n");
    return;
    }

    if (checkTimeConflict(e->startTime, e->endTime, e->resourceID)) {
        printf("Time conflict detected! Event not added.\n");
        return;
    }

    if(!validTime(e->startTime)){
    printf("Invalid Start Time!\n");
    return;
    }

    if(!validTime(e->endTime)){
    printf("Invalid End Time!\n");
    return;
    }

    if(e->startTime >= e->endTime){
    printf("Invalid time range!\n");
    return;
    }

    printf("Enter Maximum Participants: ");
    scanf("%d", &e->maxParticipants);

    e->participantHead = NULL;
    e->participantCount = 0;
    e->Cancelled = 0;

    insertIntoPQ(&pq, e);

    totalEvents++;

    printf("Event Added & Scheduled Successfully!\n");
}

void searchEventMenu(){
    int id;
    printf("Enter Event ID: ");
    scanf("%d",&id);

    Event* e = searchEventByID(id);

    if(e==NULL){
        printf("Event not found!\n");
        return;
    }

    printf("Event Found: %s | Priority:%d | Time:%d-%d\n",
            e->name,e->priority,e->startTime,e->endTime);
}

void displayAllEvents() {

    printf("\n---------- Scheduled Events (By Priority) -----------\n");

    PriorityQueue temp = pq;   //copy the heap

    Event* e;

    while ((e = removeFromPQ(&temp)) != NULL) {

        if (!e->Cancelled) {

            Resource* r = searchResource(root, e->resourceID);

            printf("\n------ EVENT LIST ------\n");
            printf("ID:%d | %s | Priority:%d | Time:",
                   e->eventID, e->name, e->priority);

            printTime(e->startTime);
            printf(" - ");
            printTime(e->endTime);

            printf(" | Resource ID:%d ", e->resourceID);

            printf(" | Resource:%s | Participants:%d/%d\n",
                   r ? r->name : "Unknown", e->participantCount, e->maxParticipants);
        }
    }
}

void cancelEvent(int id) {
    Event* e = searchEventByID(id);
    if (e == NULL) {
        printf("Event not found!\n");
        return;
    }

    freeParticipants(e);

    e->Cancelled = 1;
    printf("Event Cancelled Successfully!\n");
}


/* ========================================================================================================================================================= */
/* ============================================ PARTICIPANT FUNCTIONS ===================================================================================== */

void registerParticipant(int eventID) {

    Event* e = searchEventByID(eventID);

    if (e == NULL) {
        printf("Event not found!\n");
        return;
    }

    if (e->participantCount >= e->maxParticipants) {
        printf("Participant limit reached!\n");
        return;
    }

    Participant* newP = (Participant*)malloc(sizeof(Participant));
    if (newP == NULL) {
        printf("Memory allocation failed!\n");
        return;
    }

    printf("Enter Participant ID: ");
    scanf("%d", &newP->participantID);

    printf("Enter Participant Name: ");
    scanf(" %[^\n]", newP->name);

    Participant* temp = e->participantHead;
    while(temp != NULL){
    if(temp->participantID == newP->participantID) {
        printf("Participant ID already exists!\n");
        free(newP);
        return;
    }
    temp = temp->next;
    }

    newP->next = e->participantHead;
    e->participantHead = newP;
    e->participantCount++;

    printf("Participant Registered Successfully!\n");
}

void displayParticipants(int eventID) {
    Event* e = searchEventByID(eventID);

    if (e == NULL) {
        printf("Event not found!\n");
        return;
    }

    Participant* temp = e->participantHead;

    printf("Participants for Event %s:\n", e->name);

    while (temp != NULL) {
        printf("ID:%d | Name:%s\n",
               temp->participantID, temp->name);
        temp = temp->next;
    }
}

void freeParticipants(Event* e){
    Participant* temp = e->participantHead;
    while(temp != NULL){
        Participant* next = temp->next;
        free(temp);
        temp = next;
    }

    e->participantHead = NULL;
    e->participantCount = 0;
}

/* ================================================================================================================================================================ */
/* SYSTEM SUMMARY */

void systemSummary(){

    int active=0;
    int cancelled=0;
    int totalParticipants = 0;

    for(int i=0;i<totalEvents;i++){
        if(eventList[i].Cancelled)
            cancelled++;
        else
            active++;
        totalParticipants += eventList[i].participantCount;
    }

    printf("\n===== SYSTEM SUMMARY =====\n");
    printf("Total Events Created: %d\n", totalEvents);
    printf("Active Events: %d\n", active);
    printf("Cancelled Events: %d\n", cancelled);
    printf("Events in Queue: %d\n", pq.size);
    printf("Total Participants Registered: %d\n", totalParticipants);
}

/* =============================================== MAIN MENU ================================================================================================= */

void menu() {
    printf("\n===== Event Management System =====\n");
    printf("1. Add Event\n");
    printf("2. Register Participant\n");
    printf("3. Display Events\n");
    printf("4. Display Participants\n");
    printf("5. Cancel Event\n");
    printf("6. Add Resource\n");
    printf("7. Display Resources\n");
    printf("8. Search Resource\n");
    printf("9. Process Event\n");
    printf("10. System Summary\n");
    printf("11. Exit\n");
    printf("Enter choice: ");
}

int main() {
    pq.size = 0;

    int choice, id;
    char name[50];

    while (1) {
        menu();
        scanf("%d", &choice);

        switch(choice) {

        case 1:
            addEvent();
            break;

        case 2:
            printf("Enter Event ID: ");
            scanf("%d", &id);
            registerParticipant(id);
            break;

        case 3:
            displayAllEvents();
            break;

        case 4:
            printf("Enter Event ID: ");
            scanf("%d", &id);
            displayParticipants(id);
            break;

        case 5:
            printf("Enter Event ID to Cancel: ");
            scanf("%d", &id);
            cancelEvent(id);
            break;

        case 6:
            printf("Enter Resource ID: ");
            scanf("%d", &id);
            printf("Enter Resource Name: ");
            scanf(" %[^\n]", name);
            if(searchResource(root,id)!=NULL){
            printf("Resource ID already exists!\n");
            }
            else{
            root = insertResource(root, id, name);
            printf("Resource Added!\n");
            }
            break;

        case 7:
            inorderResources(root);
            break;

        case 8:
            searchResourceMenu();
            break;
        case 9:
            {
                Event* e = removeFromPQ(&pq);
            
                while(e != NULL && e->Cancelled){
                    e = removeFromPQ(&pq);
                }
            
                if(e == NULL){
                    printf("No event to process!\n");
                 }
                   else{
                    printf("\nProcessing Event:\n");
                    printf("ID:%d | %s | Priority:%d\n", e->eventID, e->name, e->priority);
                }
            }
break;

         case 10:
            systemSummary();
            break;
        
        case 11:
            printf("Exiting.....\n");
            freeResource(root);
            exit(0);


        default:
            printf("Invalid Choice!\n");
        }
    }

    return 0;
}