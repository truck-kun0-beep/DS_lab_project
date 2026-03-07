#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_EVENTS 100

/* ================================== PARTICIPANT LINKED LIST ================================== */

typedef struct Participant {        //typedef is basically using to not write "struct" all the time
    int participantID;
    char name[100];
    struct Participant* next;
} Participant;                      //Here Participant becomes alias, so we can use it without "struct"

/* ======================================== EVENT STRUCTURE =================================================== */

typedef struct Event {
    int eventID;
    char name[150];
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
    char name[80];
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
void inorderResources(Resource* root);

/* ============================================================================================================================= */
/* =================================================== RESOURCE BST FUNCTIONS =================================================== */

Resource* createResource(int id, char name[]) {                     //Why "Resource*" instead of void. Because we will use this function later. It must give return a value. "void" doesn't return anything.
    Resource* newNode = (Resource*)malloc(sizeof(Resource));
    if (newNode == NULL) {
        printf("Memory allocation failed\n");
        return NULL;
    }
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

void displayResources(){

    if(root == NULL){
    printf("No resources available\n");
    return;
    }

    printf("\n=============================================\n");
    printf("Resource ID | Resource Name\n");
    printf("=============================================\n");
    inorderResources(root);
}


void inorderResources(Resource* root) {

    if(root == NULL)
        return;                 //stops the recursion when done

    inorderResources(root->left);

    printf("%-11d | %-30.30s\n",
           root->resourceID,
           root->name);

    inorderResources(root->right);
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
    pq->size++;                     //Increasing size first beaccuase it is safer
    heapifyUp(pq, pq->size - 1);
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

//Necessary when updating event
void rebuildHeap(PriorityQueue* pq){
    for(int i = pq->size/2 - 1; i >= 0; i--){
        heapifyDown(pq, i);
    }
}

/* ================================================================================================================================================================ */
/* =================================================== EVENT FUNCTIONS =========================================================================================== */

Event* searchEventByID(int id) {
    for (int i = 0; i < totalEvents; i++) {
        if (eventList[i].eventID == id)
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

int checkTimeConflict(int start, int end, int resourceID, int ignoreEventID) {
    for (int i = 0; i < totalEvents; i++) {
        if (eventList[i].Cancelled == 0 && eventList[i].resourceID == resourceID && eventList[i].eventID != ignoreEventID) {
            if (eventList[i].startTime < end && start < eventList[i].endTime)
                return 1;       // conflict
        }
    }
    return 0;
}

char* priorityText(int p){
    if(p==1) return "HIGH";
    if(p==2) return "MEDIUM";
    return "LOW";
}

void addEvent() {
    if (totalEvents >= MAX_EVENTS) {
        printf("Event limit reached!\n");
        return;
    }
    
    Event temp;
    Event* e = &temp;

    printf("Enter Event ID: ");
    scanf("%d", &e->eventID);

    if (e->eventID<=0) {
    printf("Invalid Event ID!\n");
    return;
    }

    /* Check duplicate ID immediately */
    if(searchEventByID(e->eventID)!=NULL) {
    printf("Event ID already exists!\n");
    return;
    }

    printf("Enter Event Name: ");
    scanf(" %[^\n]", e->name);                      //" %[^\n]" is used to read strings with spaces

    printf("Enter Priority (1=High,2=Medium,3=Low): ");
    scanf("%d", &e->priority);

    if(e->priority <1 || e->priority >3){
        printf("Invalid priority!\n");
        return;
    }

    printf("Enter Start Time (e.g., 1300): ");
    scanf("%d", &e->startTime);

    printf("Enter End Time: ");
    scanf("%d", &e->endTime);

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

    printf("Enter Resource ID: ");
    scanf("%d", &e->resourceID);

    if (searchResource(root, e->resourceID) == NULL) {
        printf("Resource does not exist!\n");
        return;
    }


    if (checkTimeConflict(e->startTime, e->endTime, e->resourceID, -1)) {
        printf("Time conflict detected! Event not added.\n");
        return;
    }

    printf("Enter Maximum Participants: ");
    scanf("%d", &e->maxParticipants);

    if(e->maxParticipants <= 0){
    printf("Invalid participant limit!\n");
    return;
    }

    e->participantHead = NULL;
    e->participantCount = 0;
    e->Cancelled = 0;

    eventList[totalEvents] = temp;
    insertIntoPQ(&pq, &eventList[totalEvents]);

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

    printf("Event Found: %s | Priority:%s | ",
            e->name, priorityText(e->priority));
    printTime(e->startTime);
    printf(" - ");
    printTime(e->endTime);
}

void updateEvent(int id){

    Event* e = searchEventByID(id);

    if(e == NULL){
        printf("Event not found!\n");
        return;
    }

    char newName[150];
    int newPriority;
    int newStart;
    int newEnd;

    printf("Enter New Event Name: ");
    scanf(" %[^\n]", newName);

    printf("Enter New Priority (1=High,2=Medium,3=Low): ");
    scanf("%d",&newPriority);

    printf("Enter New Start Time: ");
    scanf("%d",&newStart);

    printf("Enter New End Time: ");
    scanf("%d",&newEnd);

    if(newPriority <1 || newPriority >3){
        printf("Invalid priority!\n");
        return;
    }

    if(!validTime(newStart) || !validTime(newEnd)){
        printf("Invalid time!\n");
        return;
    }

    if(newStart >= newEnd){
        printf("Invalid time range!\n");
        return;
    }

    if(checkTimeConflict(newStart, newEnd, e->resourceID, e->eventID)){
        printf("Time conflict detected!\n");
        return;
    }

    strcpy(e->name,newName);
    e->priority = newPriority;
    e->startTime = newStart;
    e->endTime = newEnd;

    rebuildHeap(&pq);

    printf("Event Updated Successfully!\n");
}

void displayAllEvents() {

    printf("\n========================================================================================================================\n");
    printf("ID   | Event Name                                         | Pri | Time        | Resource Name             | Participants\n");
    printf("========================================================================================================================\n");

    Event* events[MAX_EVENTS];
    int count = 0;

    for(int i = 0; i < totalEvents; i++){
        if(!eventList[i].Cancelled){
            events[count++] = &eventList[i];
        }
    }

    if(count == 0){
        printf("No active events available.\n");
        return;
    }

    for(int i = 0; i < count - 1; i++){
        for(int j = i + 1; j < count; j++){
            if(events[i]->priority > events[j]->priority){
                Event* temp = events[i];
                events[i] = events[j];
                events[j] = temp;
            }
        }
    }

    for(int i = 0; i < count; i++){
        Event* e = events[i];

        Resource* r = searchResource(root, e->resourceID);

        printf("%-4d | %-50.50s | %-3s | ",
               e->eventID, e->name, priorityText(e->priority));

        printTime(e->startTime);
        printf("-");
        printTime(e->endTime);

        printf(" | %-25.25s | %d/%d\n",
               r ? r->name : "Unknown",
               e->participantCount,
               e->maxParticipants);
    }

    printf("========================================================================================================================\n");
}

void displayEventsByTime() {
    // Count active events
    int active = 0;
    for(int i=0; i<totalEvents; i++) {
        if(!eventList[i].Cancelled) active++;
    }

    if(active == 0) {
        printf("No active events to display.\n");
        return;
    }

    // Create array of pointers to active events. Storing active events to a temporary pointer
    Event* activeEvents[MAX_EVENTS];
    int i = 0;
    for(int j=0; j<totalEvents; j++) {
        if(!eventList[j].Cancelled) {
            activeEvents[i++] = &eventList[j];
        }
    }

    // Sort by startTime using simple bubble sort (good enough for small n)
    for(int i=0; i<active-1; i++){
        for(int j=i+1; j<active; j++){
            if(activeEvents[i]->startTime > activeEvents[j]->startTime){
                Event* temp = activeEvents[i];
                activeEvents[i] = activeEvents[j];
                activeEvents[j] = temp;
            }
        }
    }

    // Print table header
    printf("\n========================================================================================================================\n");
    printf("ID   | Event Name                                         | Pri | Time        | Resource Name             | Participants\n");
    printf("========================================================================================================================\n");

    // Print events
    for(int i=0; i<active; i++){
        Event* e = activeEvents[i];
        Resource* r = searchResource(root, e->resourceID);

        printf("%-4d | %-50.50s | %-3s | ", e->eventID, e->name, priorityText(e->priority));
        printTime(e->startTime);
        printf("-"); 
        printTime(e->endTime);
        printf(" | %-25.25s | %d/%d\n", r ? r->name : "Unknown", e->participantCount, e->maxParticipants);
    }

    printf("========================================================================================================================\n");
}

void cancelEvent(int id) {
    Event* e = searchEventByID(id);
    if (e == NULL) {
        printf("Event not found!\n");
        return;
    }

    if(e->Cancelled){
    printf("Event already cancelled!\n");
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

    if(e->Cancelled){
        printf("Cannot register. Event is cancelled!\n");
        return;
    }

    if (e->participantCount >= e->maxParticipants) {
        printf("Participant limit reached!\n");
        return;
    }

    int newID;

    printf("Enter Participant ID: ");
    scanf("%d", &newID);

    if(newID <= 0){
        printf("Invalid Participant ID!\n");
        return;
    }

    Participant* temp = e->participantHead;
    while(temp != NULL){
        if(temp->participantID == newID){
            printf("Participant ID already exists!\n");
            return;
        }
        temp = temp->next;
    }

    Participant* newP = (Participant*)malloc(sizeof(Participant));
    if(newP == NULL){
        printf("Memory allocation failed!\n");
        return;
    }

    newP->participantID = newID;

    printf("Enter Participant Name: ");
    scanf(" %[^\n]", newP->name);

    newP->next = e->participantHead;
    e->participantHead = newP;
    e->participantCount++;

    printf("Participant Registered Successfully!\n");
}

void removeParticipant(int eventID, int participantID) {

    Event* e = searchEventByID(eventID);

    if(e == NULL){
        printf("Event not found!\n");
        return;
    }

    if(e->Cancelled){
    printf("Event is cancelled!\n");
    return;
    }

    Participant* temp = e->participantHead;
    Participant* prev = NULL;

    while(temp != NULL){

        if(temp->participantID == participantID){

            if(prev == NULL)
                e->participantHead = temp->next;
            else
                prev->next = temp->next;

            free(temp);
            e->participantCount--;

            printf("Participant removed successfully!\n");
            return;
        }

        prev = temp;
        temp = temp->next;
    }

    printf("Participant not found!\n");
}

void displayParticipants(int eventID) {
    Event* e = searchEventByID(eventID);

    if (e == NULL) {
        printf("Event not found!\n");
        return;
    }

    if(e->participantHead == NULL){
        printf("No participants registered for this event.\n");
        return;
    }

    Participant* temp = e->participantHead;

    printf("\nParticipants for Event: %s\n", e->name);
    printf("=============================================\n");
    printf("Participant ID | Name\n");
    printf("=============================================\n");

    while (temp != NULL) {
        printf("%-15d | %-30.30s\n", temp->participantID, temp->name);
        temp = temp->next;
    }
    printf("=============================================\n");
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
    int eventsInQueue = 0;

    for(int i=0;i<totalEvents;i++){
        if(eventList[i].Cancelled)
            cancelled++;
        else
            active++;
        totalParticipants += eventList[i].participantCount;
    }

    for(int i = 0; i < pq.size; i++){
        if(!pq.events[i]->Cancelled)
            eventsInQueue++;
    }

    printf("\n=====================================\n");
    printf("            SYSTEM SUMMARY\n");
    printf("=====================================\n");
    printf("Total Events Created      : %d\n", totalEvents);
    printf("Active Events             : %d\n", active);
    printf("Cancelled Events          : %d\n", cancelled);
    printf("Events in Priority Queue  : %d\n", eventsInQueue);
    printf("Total Participants        : %d\n", totalParticipants);
    printf("=====================================\n");
}

/* =============================================== MAIN MENU ================================================================================================= */

void menu() {
     printf("=====================================\n");
    printf("\n===== Event Management System =====\n");
    printf("=====================================\n");

    printf("1. Add Event\n");
    printf("2. Register Participant\n");
    printf("3. Display Events (Priority Order)\n");
    printf("4. Display Events (Time Order)\n");
    printf("5. Display Participants\n");
    printf("6. Update Event\n");
    printf("7. Cancel Event\n");
    printf("-------------------------------------\n");
    printf("8. Add Resource\n");
    printf("9. Display Resources\n");
    printf("10. Search Resource\n");
    printf("-------------------------------------\n");
    printf("11. Process Event\n");
    printf("12. System Summary\n");
    printf("13. Remove Participant\n");
    printf("14. Exit\n");

    printf("=====================================\n");
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
            displayEventsByTime();
            break;
        case 5:
            printf("Enter Event ID to Display Participants: ");
            scanf("%d", &id);
            displayParticipants(id);
            break;

        case 6:
            printf("Enter Event ID to Update: ");
            scanf("%d", &id);
            updateEvent(id);
            break;

        case 7:
            printf("Enter Event ID to Cancel: ");
            scanf("%d", &id);
            cancelEvent(id);
            break;

        case 8:
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

        case 9:
            displayResources();
            break;

        case 10:
            searchResourceMenu();
            break;
        case 11:
            {
                Event* e = removeFromPQ(&pq);
            
                while(e != NULL && e->Cancelled){
                    e = removeFromPQ(&pq);
                }
            
                if(e == NULL){
                    printf("No event to process!\n");
                 }
                   else{
                    printf("\n=====================================\n");
                    printf("          PROCESSING EVENT\n");
                    printf("=====================================\n");
                    printf("Event ID   : %d\n", e->eventID);
                    printf("Event Name : %s\n", e->name);
                    printf("Priority   : %s\n", priorityText(e->priority));
                    printf("=====================================\n");
                }
            }
            break;
         case 12:
            systemSummary();
            break;
        case 13:
            {
                int eventID;
                int participantID;

                printf("Enter Event ID: ");
                scanf("%d", &eventID);

                printf("Enter Participant ID to remove: ");
                scanf("%d", &participantID);

                removeParticipant(eventID, participantID);
            }
            break;
        case 14:
            printf("Exiting.....\n");
            for(int i=0;i<totalEvents;i++) {
                freeParticipants(&eventList[i]);
            }
            freeResource(root);
            exit(0);


        default:
            printf("Invalid Choice!\n");
        }
    }

    return 0;
}