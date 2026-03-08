#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h> //for random number seeding
#include <ctype.h>

#define MAX_EVENTS 100

/* ================================== PARTICIPANT LINKED LIST ================================== */

typedef struct Participant {
    int participantID;
    char name[100];
    struct Participant* next;
} Participant;

/* ======================================== EVENT STRUCTURE =================================================== */

typedef struct Event {
    int eventID;
    char name[150];
    int priority;      // 1 = High, 2 = Medium, 3 = Low
    int date;          // NEW (format: YYYYMMDD)
    int startTime;     // Example: 1300 = 1:00 PM
    int endTime;
    int resourceID;
    int Cancelled;
    int Done;          // NEW

    //Used linked list to manage participants
    Participant* participantHead;
    int participantCount;
    int maxParticipants;
    int participantCounter;
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

PriorityQueue pq;
Resource* root = NULL;

int compareIgnoreCase(char a[], char b[]) {

    int i = 0;

    while(a[i] && b[i]) {

        if(tolower(a[i]) != tolower(b[i]))
            return 0;

        i++;
    }

    return a[i] == b[i];
}


/* Function Prototypes */

// Event
Event* searchEventByID(int id);
int validTime(int t);
int validDate(int date);
void printTime(int t);
void printDate(int date);
int checkTimeConflict(int start, int end, int resourceID, int ignoreEventID, int date);
void completeEvent(int id);
void displayDoneEvents();
void displayCancelledEvents();

// Resource
Resource* searchResource(Resource* root, int id);
void showAvailableResourcesRecursive(Resource* root, int start, int end, int date);
void freeResource(Resource* root);

// Participant
void freeParticipants(Event* e);
void inorderResources(Resource* root);

/* ============================================================================================================================= */

/* ================================= ID GENERATORS ================================= */

int generateEventID(){

    int id;

    do{
        id = rand() % 900000 + 100000;   //149  prevents buffer overflow space before % skips leftover newline
    }
    while(searchEventByID(id) != NULL);

    return id;
}

int generateResourceID(){

    int id;

    do{
        id = rand() % 900 + 100;   // 100–999
    }
    while(searchResource(root,id) != NULL);

    return id;
}

/* =================================================== RESOURCE BST FUNCTIONS =================================================== */

Resource* createResource(int id, char name[]) {                     //Why "Resource*" instead of void. Because we will use this function later. It must give return a value. "void" doesn't return anything.
    Resource* newNode = (Resource*)malloc(sizeof(Resource));
    if (newNode == NULL) {
        printf("Memory allocation failed\n");
        return NULL;
    }
    newNode->resourceID = id;
    strcpy(newNode->name, name);
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

int searchResourceByName(Resource* root, char name[]) {

    if (name == NULL || strlen(name) == 0) return 0;
    
    if(root == NULL)
        return 0;

    int found = 0;

    found |= searchResourceByName(root->left, name);

    if(compareIgnoreCase(root->name, name)) {
        printf("Found: %d | %s\n", root->resourceID, root->name);
        found = 1;
    }

    found |= searchResourceByName(root->right, name);

    return found;
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
    printf("=============================================\n");
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

int isResourceAvailable(int resourceID, int start, int end, int date){

    for(int i = 0; i < totalEvents; i++){
        if(eventList[i].Cancelled || eventList[i].Done)
            continue;

        if(eventList[i].resourceID == resourceID && eventList[i].date == date){

            if(eventList[i].startTime < end && start < eventList[i].endTime){
                return 0;   // not available
            }
        }
    }

    return 1;   // available
}

void showAvailableResources(){
    int start, end, date;

    printf("Enter Date (YYYYMMDD): ");
    scanf("%d", &date);

    if(!validDate(date)){
        printf("Invalid date format!\n");
        return;
    }

    printf("Enter Start Time: ");
    scanf("%d",&start);

    printf("Enter End Time: ");
    scanf("%d",&end);

    if(!validTime(start) || !validTime(end) || start >= end){
        printf("Invalid time range\n");
        return;
    }

    printf("\nAvailable Resources:\n");
    printf("=====================================\n");

    showAvailableResourcesRecursive(root, start, end, date);

    printf("=====================================\n");
}

void showAvailableResourcesRecursive(Resource* root, int start, int end, int date){
    if(root == NULL)
        return;

    showAvailableResourcesRecursive(root->left, start, end, date);

    if(isResourceAvailable(root->resourceID, start, end, date)){
        printf("%-10d | %-30.30s\n", root->resourceID, root->name);  // Improved formatting
    }

    showAvailableResourcesRecursive(root->right, start, end, date);
}

    int countResources(Resource* root){
    if(root==NULL) return 0;
    return 1 + countResources(root->left) + countResources(root->right);        //Recursively count resources in BST
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

void cleanPriorityQueue(PriorityQueue* pq){

    int newSize = 0;

    for(int i = 0; i < pq->size; i++){
        if(!pq->events[i]->Cancelled && !pq->events[i]->Done){
            pq->events[newSize++] = pq->events[i];
        }
    }

    pq->size = newSize;
    rebuildHeap(pq);
}

/* ================================================================================================================================================================ */
/* =================================================== EVENT FUNCTIONS =========================================================================================== */

Event* searchEventByID(int id) {
    for (int i = 0; i < totalEvents; i++) {
        if (eventList[i].eventID == id && !eventList[i].Cancelled && !eventList[i].Done)
            return &eventList[i];
    }
    return NULL;
}

int validDate(int date){
    int year = date / 10000;
    int month = (date / 100) % 100;
    int day = date % 100;
    
    if(year < 2024 || year > 2030) return 0;
    if(month < 1 || month > 12) return 0;
    if(day < 1 || day > 31) return 0;
    
    // Improved day validation based on month
    if((month == 4 || month == 6 || month == 9 || month == 11) && day > 30)
        return 0;
    
    if(month == 2 && day > 29)
        return 0;
    
    return 1;
}

void printDate(int date){
    int year = date / 10000;
    int month = (date / 100) % 100;
    int day = date % 100;
    
    printf("%04d-%02d-%02d", year, month, day);
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

int checkTimeConflict(int start, int end, int resourceID, int ignoreEventID, int date) {
    for (int i = 0; i < totalEvents; i++) {
        if(eventList[i].Cancelled || eventList[i].Done)
            continue;

        if(eventList[i].resourceID == resourceID &&
           eventList[i].eventID != ignoreEventID &&
           eventList[i].date == date)
        {
            if(eventList[i].startTime < end && start < eventList[i].endTime)
                return 1;       // conflict
        }
    }
    return 0;
}

const char* priorityText(int p){
    if(p==1) return "HIGH";
    if(p==2) return "MEDIUM";
    return "LOW";
}

void addEvent() {
    
    // Check if resources exist first
    if(root == NULL){
        printf("No resources available. Add resources first.\n");
        return;
    }

    if (totalEvents >= MAX_EVENTS) {
        printf("Event limit reached!\n");
        return;
    }
  
    Event* e = &eventList[totalEvents];


    printf("Enter Event Name: ");
    scanf(" %149[^\n]", e->name);

    printf("Enter Priority (1=High,2=Medium,3=Low): ");
    scanf("%d", &e->priority);

    if(e->priority <1 || e->priority >3){
        printf("Invalid priority!\n");
        return;
    }

    printf("Enter Event Date (YYYYMMDD): ");
    scanf("%d", &e->date);

    if(!validDate(e->date)){
        printf("Invalid date!\n");
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

    printf("\nAvailable Resources for selected time:\n");
    printf("=====================================\n");

    showAvailableResourcesRecursive(root, e->startTime, e->endTime, e->date);

    printf("=====================================\n");

    // Created a resource name search to find the resource id
    printf("Enter Resource ID: ");
    scanf("%d", &e->resourceID);

    // SINGLE CHECK for resource existence
    if(searchResource(root, e->resourceID) == NULL){
        printf("Resource does not exist!\n");
        return;
    }

    if(!isResourceAvailable(e->resourceID, e->startTime, e->endTime, e->date)){
        printf("Selected resource is not available in that time range!\n");
        return;
    }

    if (checkTimeConflict(e->startTime, e->endTime, e->resourceID, -1, e->date)) {
        printf("Time conflict detected! Event not added.\n");
        return;
    }

    //If everything is valid, then generate ID
    e->eventID = generateEventID();
    printf("Generated Event ID: %d\n", e->eventID);

    printf("Enter Maximum Participants: ");
    scanf("%d", &e->maxParticipants);

    if(e->maxParticipants <= 0){
    printf("Invalid participant limit!\n");
    return;
    }

    e->participantHead = NULL;
    e->participantCount = 0;
    e->Cancelled = 0;
    e->Done = 0;
    e->participantCounter = 1;

    insertIntoPQ(&pq, &eventList[totalEvents]);

    totalEvents++;

    printf("Event Added & Scheduled Successfully!\n");
}

void searchEventMenu(){
    int id;
    printf("Enter Event ID: ");
    scanf("%d",&id);

    Event* e = searchEventByID(id);

    if(e==NULL) {
        for(int i=0; i<totalEvents; i++){
            if(eventList[i].eventID == id){
                e = &eventList[i];
                break;
            }
        }
        if(e==NULL){
            printf("Event not found!\n");
            return;
        }
    }

    Resource* r = searchResource(root, e->resourceID);

    printf("\nEvent Found\n");
    printf("=====================================\n");
    printf("Event Name : %s\n", e->name);
    printf("Priority   : %s\n", priorityText(e->priority));
    printf("Date       : ");
    printDate(e->date);
    printf("\n");
    printf("Time       : ");
    printTime(e->startTime);
    printf(" - ");
    printTime(e->endTime);
    printf("\n");
    printf("Resource   : %s\n", r ? r->name : "Unknown");
    printf("Status     : %s\n", e->Done ? "DONE" : (e->Cancelled ? "CANCELLED" : "ACTIVE"));
    printf("=====================================\n");
}

void updateEvent(int id){

    Event* e = searchEventByID(id);

    if(e == NULL){
        printf("Event not found or not active!\n");
        return;
    }

    char newName[150];
    int newPriority;
    int newStart;
    int newEnd;
    int newDate;

    printf("Enter New Event Name: ");
    scanf(" %[^\n]", newName);

    printf("Enter New Priority (1=High,2=Medium,3=Low): ");
    scanf("%d",&newPriority);

    printf("Enter New Date (YYYYMMDD): ");
    scanf("%d",&newDate);

    printf("Enter New Start Time: ");
    scanf("%d",&newStart);

    printf("Enter New End Time: ");
    scanf("%d",&newEnd);

    if(newPriority <1 || newPriority >3){
        printf("Invalid priority!\n");
        return;
    }

    if(!validDate(newDate)){
        printf("Invalid date!\n");
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

    if(checkTimeConflict(newStart, newEnd, e->resourceID, e->eventID, newDate)){
        printf("Time conflict detected!\n");
        return;
    }

    strcpy(e->name,newName);
    e->priority = newPriority;
    e->date = newDate;
    e->startTime = newStart;
    e->endTime = newEnd;

    rebuildHeap(&pq);

    printf("Event Updated Successfully!\n");
}

void displayAllEvents() {

    // Clean the priority queue first to remove any Done/Cancelled events
    cleanPriorityQueue(&pq);

    if(pq.size == 0){
        printf("No active events available.\n");
        return;
    }

    PriorityQueue temp = pq;   // copy PQ so original is not destroyed

    printf("\n========================================================================================================================================\n");
    printf("ID   | Event Name                                         | Pri | Date       | Time        | Resource Name             | Participants\n");
    printf("========================================================================================================================================\n");

    while(temp.size > 0){

        Event* e = removeFromPQ(&temp);

        if(e->Cancelled || e->Done)
            continue;

        Resource* r = searchResource(root, e->resourceID);

        printf("%-4d | %-50.50s | %-3s | ", e->eventID, e->name, priorityText(e->priority));
        printDate(e->date);
        printf(" | ");
        printTime(e->startTime);
        printf("-");
        printTime(e->endTime);

        printf(" | %-25.25s | %d/%d\n",
               r ? r->name : "Unknown",
               e->participantCount,
               e->maxParticipants);
    }

    printf("========================================================================================================================================\n");
}

void displayEventsByTime() {
    // Count active events
    int active = 0;
    for(int i=0; i<totalEvents; i++) {
        if(!eventList[i].Cancelled && !eventList[i].Done) active++;
    }

    if(active == 0) {
        printf("No active events to display.\n");
        return;
    }

    // Create array of pointers to active events. Storing active events to a temporary pointer
    Event* activeEvents[MAX_EVENTS];
    int i = 0;
    for(int j=0; j<totalEvents; j++) {
        if(!eventList[j].Cancelled && !eventList[j].Done) {
            activeEvents[i++] = &eventList[j];
        }
    }

    // Sort by date then startTime
    for(int i=0; i<active-1; i++){
        for(int j=i+1; j<active; j++){
            if(activeEvents[i]->date > activeEvents[j]->date ||
               (activeEvents[i]->date == activeEvents[j]->date && 
                activeEvents[i]->startTime > activeEvents[j]->startTime)){
                Event* temp = activeEvents[i];
                activeEvents[i] = activeEvents[j];
                activeEvents[j] = temp;
            }
        }
    }

    // Print table header
    printf("\n========================================================================================================================================\n");
    printf("ID   | Event Name                                         | Pri | Date       | Time        | Resource Name             | Participants\n");
    printf("========================================================================================================================================\n");

    // Print events
    for(int i=0; i<active; i++){
        Event* e = activeEvents[i];
        Resource* r = searchResource(root, e->resourceID);

        printf("%-4d | %-50.50s | %-3s | ", e->eventID, e->name, priorityText(e->priority));
        printDate(e->date);
        printf(" | ");
        printTime(e->startTime);
        printf("-"); 
        printTime(e->endTime);
        printf(" | %-25.25s | %d/%d\n", r ? r->name : "Unknown", e->participantCount, e->maxParticipants);
    }

    printf("========================================================================================================================================\n");
}

void cancelEvent(int id) {
    Event* e = NULL;
    
    // Find event even if not active
    for(int i=0; i<totalEvents; i++){
        if(eventList[i].eventID == id){
            e = &eventList[i];
            break;
        }
    }

    if(e == NULL){
        printf("Event not found!\n");
        return;
    }

    if(e->Done){
        printf("Event already completed!\n");
        return;
    }

    if(e->Cancelled){
        printf("Event already cancelled!\n");
        return;
    }

    e->Cancelled = 1;
    cleanPriorityQueue(&pq);

    printf("Event Cancelled Successfully! Participants preserved.\n");
}

void completeEvent(int id){
    Event* e = NULL;
    
    // Find event even if not active
    for(int i=0; i<totalEvents; i++){
        if(eventList[i].eventID == id){
            e = &eventList[i];
            break;
        }
    }

    if(e == NULL){
        printf("Event not found!\n");
        return;
    }

    if(e->Cancelled){
        printf("Event is cancelled!\n");
        return;
    }

    if(e->Done){
        printf("Event already marked as done!\n");
        return;
    }

    e->Done = 1;
    cleanPriorityQueue(&pq);

    printf("Event marked as completed! Participants preserved.\n");
}

void displayDoneEvents(){
    int found = 0;

    printf("\n=====================================\n");
    printf("        COMPLETED EVENTS\n");
    printf("=====================================\n");

    for(int i=0;i<totalEvents;i++){
        if(eventList[i].Done){
            printf("%d | %s | ", eventList[i].eventID, eventList[i].name);
            printDate(eventList[i].date);
            printf(" | %d participants\n", eventList[i].participantCount);
            found = 1;
        }
    }

    if(!found)
        printf("No completed events.\n");

    printf("=====================================\n");
}

void displayCancelledEvents(){
    int found = 0;

    printf("\n=====================================\n");
    printf("        CANCELLED EVENTS\n");
    printf("=====================================\n");

    for(int i=0;i<totalEvents;i++){
        if(eventList[i].Cancelled){
            printf("%d | %s | ", eventList[i].eventID, eventList[i].name);
            printDate(eventList[i].date);
            printf(" | %d participants\n", eventList[i].participantCount);
            found = 1;
        }
    }

    if(!found)
        printf("No cancelled events.\n");

    printf("=====================================\n");
}

/* ========================================================================================================================================================= */
/* ============================================ PARTICIPANT FUNCTIONS ===================================================================================== */

void registerParticipant(int eventID) {

    Event* e = searchEventByID(eventID);

    if (e == NULL) {
        printf("Active event not found!\n");
        return;
    }

    if(e->Cancelled){
        printf("Cannot register. Event is cancelled!\n");
        return;
    }

    if(e->Done){
        printf("Cannot register. Event is already completed!\n");
        return;
    }

    if (e->participantCount >= e->maxParticipants) {
        printf("Participant limit reached!\n");
        return;
    }
    int newID = e->participantCounter++;
    printf("Generated Participant ID: %d\n", newID);

    Participant* newP = (Participant*)malloc(sizeof(Participant));
    if(newP == NULL){
        printf("Memory allocation failed!\n");
        return;
    }

    newP->participantID = newID;

    printf("Enter Participant Name: ");
    scanf(" %99[^\n]", newP->name);

    newP->next = e->participantHead;
    e->participantHead = newP;
    e->participantCount++;

    printf("Participant Registered Successfully!\n");
}

void removeParticipant(int eventID, int participantID) {

    Event* e = searchEventByID(eventID);

    if(e == NULL){
        printf("Active event not found!\n");
        return;
    }

    if(e->Cancelled){
    printf("Event is cancelled!\n");
    return;
    }

    if(e->Done){
        printf("Event is already completed!\n");
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
    Event* e = NULL;
    
    // Find event even if not active
    for(int i=0; i<totalEvents; i++){
        if(eventList[i].eventID == eventID){
            e = &eventList[i];
            break;
        }
    }

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
    printf("Status: %s\n", e->Done ? "DONE" : (e->Cancelled ? "CANCELLED" : "ACTIVE"));
    printf("=============================================\n");
    printf("Participant ID | Name\n");
    printf("=============================================\n");

    while (temp != NULL) {
        printf("%03d | %-30.30s\n", temp->participantID, temp->name);
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
/* ====================================== SYSTEM SUMMARY ====================================== */

void systemSummary(){

    int active=0;
    int cancelled=0;
    int done=0;
    int totalParticipants = 0;
    int eventsInQueue = 0;

    for(int i=0;i<totalEvents;i++){
        if(eventList[i].Cancelled)
            cancelled++;
        else if(eventList[i].Done)
            done++;
        else
            active++;
        totalParticipants += eventList[i].participantCount;
    }

    for(int i = 0; i < pq.size; i++){
        if(!pq.events[i]->Cancelled && !pq.events[i]->Done)
            eventsInQueue++;
    }

    printf("\n=====================================\n");
    printf("            SYSTEM SUMMARY\n");
    printf("=====================================\n");
    printf("Total Events Created      : %d\n", totalEvents);

    printf("Active Events             : %d\n", active);
    printf("Completed Events          : %d\n", done);
    printf("Cancelled Events          : %d\n", cancelled);
    printf("Events in Priority Queue  : %d\n", eventsInQueue);
    printf("Total Participants        : %d\n", totalParticipants);
    printf("Total Resources           : %d\n", countResources(root));
    printf("=====================================\n");
}

/* =============================================== MAIN MENU ================================================================================================= */

void menu() {
    printf("========================================\n");
    printf("  EVENT MANAGEMENT & SCHEDULING SYSTEM\n");
    printf("========================================\n");

    printf("1. Add Event\n");
    printf("2. Register Participant\n");
    printf("3. Display Events (Priority Order)\n");
    printf("4. Display Events (Time/Date Order)\n");
    printf("5. Display Participants\n");
    printf("6. Remove Participant\n");
    printf("7. Update Event\n");
    printf("8. Search Event\n");
    printf("9. Cancel Event\n");
    printf("-------------------------------------\n");
    printf("10. Add Resource\n");
    printf("11. Display Resources\n");
    printf("12. Search Resource\n");
    printf("13. Show Available Resources\n");
    printf("-------------------------------------\n");
    printf("14. Mark Event as Completed\n");
    printf("15. Show Completed Events\n");
    printf("16. Show Cancelled Events\n");
    printf("17. System Summary\n");
    printf("18. Exit\n");

    printf("=====================================\n");
    printf("Enter choice: ");
}

int main() {
    pq.size = 0;
    srand(time(NULL));  // Seed for random number generation
    root = NULL;
    int choice, id;
    char name[80];

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
            {
            int eventID, participantID;

            printf("Enter Event ID: ");
            scanf("%d",&eventID);

            printf("Enter Participant ID to remove: ");
            scanf("%d",&participantID);

            removeParticipant(eventID,participantID);
            }
            break;

        case 7:
            printf("Enter Event ID to Update: ");
            scanf("%d", &id);
            updateEvent(id);
            break;

        case 8:
            searchEventMenu();
            break;

        case 9:
            printf("Enter Event ID to Cancel: ");
            scanf("%d", &id);
            cancelEvent(id);
            break;

        case 10:
            id = generateResourceID();
            printf("Generated Resource ID: %d\n", id);
            printf("Enter Resource Name: ");
            scanf(" %[^\n]", name);
            
            root = insertResource(root, id, name);
            printf("Resource Added!\n");
            break;

        case 11:
            displayResources();
            break;

        case 12:
            {
                printf("Enter Resource Name: ");
                scanf(" %[^\n]", name);

                if(!searchResourceByName(root, name))
                printf("Resource not found!\n");
            }
            break;
            
        case 13:
            showAvailableResources();
            break;
            
        case 14:
            printf("Enter Event ID to mark as Completed: ");
            scanf("%d", &id);
            completeEvent(id);
            break;
            
        case 15:
            displayDoneEvents();
            break;
            
        case 16:
            displayCancelledEvents();
            break;
            
        case 17:
            systemSummary();
            break;
            
        case 18:
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