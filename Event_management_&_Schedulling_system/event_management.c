#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h> //for random number seeding
#include <ctype.h>

#define MAX_EVENTS 10000

/* ================================== ANSI COLOR CODES ================================== */
#define RESET   "\033[0m"
#define BOLD    "\033[1m"
#define RED     "\033[1;31m"
#define GREEN   "\033[1;32m"
#define YELLOW  "\033[1;33m"
#define CYAN    "\033[1;36m"
#define WHITE   "\033[1;37m"

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
    int date;          // format: YYYYMMDD
    int startTime;     // Example: 1300 = 1:00 PM
    int endTime;
    int resourceID;
    int isCancelled;
    int isDone;

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
    int indices[MAX_EVENTS];
    int size;
} PriorityQueue;

/* ========================================= GLOBAL VARIABLES =================================================== */

Event eventList[MAX_EVENTS];
int totalEvents = 0;
PriorityQueue pq;
Resource* root = NULL;

/* ============================== Function Prototypes ================================== */

int priorityOrder(PriorityQueue* pq);
// Event
int eventIDExists(int id);
int compareEvents(int a, int b);
Event* searchEventByID(int id);
void searchEventByName(char* keyword);
void printEventRow(Event* e);
int validTime(int t);
int validDate(int date);
void printTime(int t);
void printDate(int date);
int checkTimeConflict(int start, int end, int resourceID, int ignoreEventID, int date);
void completeEvent(int id);
void displayDoneEvents();
void displayCancelledEvents();
void displayAllEvents();
void displayEventsByDate();

// Resource
Resource* searchResource(Resource* root, int id);
void showAvailableResourcesRecursive(Resource* root, int start, int end, int date);
void freeResource(Resource* root);

// Participant
void freeParticipants(Event* e);
void inorderResources(Resource* root);

/* ================================================ FEATURES ============================================================================= */

int partialMatch(char haystack[], char needle[]) {
    if (needle == NULL || strlen(needle) == 0) return 0;

    int hLen = strlen(haystack);
    int nLen = strlen(needle);

    if (nLen > hLen) return 0;
    for (int i = 0; i <= hLen - nLen; i++) {

        int match = 1;
        for (int j = 0; j < nLen; j++) {
            if (tolower(haystack[i + j]) != tolower(needle[j])) {
                match = 0;
                break;
            }
        }
        // If all characters matched, the needle exists in the haystack
        if (match) return 1;
    }

    return 0;
}

// Returns color string based on priority — used across event displays
const char* priorityColor(int p){
    if(p == 1) return RED;
    if(p == 2) return YELLOW;
    return GREEN;
}

// Pause function to make output more readable
void pause() {
    printf("\n" CYAN "+-------------------------------------+" RESET "\n");
    printf(CYAN "|" RESET "      Press Enter to continue...     " CYAN "|" RESET "\n");
    printf(CYAN "+-------------------------------------+" RESET "\n");
    while (getchar() != '\n');
    getchar();
}

/*  ID GENERATORS */

int eventIDExists(int id) {
    for (int i = 0; i < totalEvents; i++) {
        if (eventList[i].eventID == id) {
            return 1;  // Event ID exists
        }
    }
    return 0;  // Event ID does not exist
}

int generateEventID(){

    int id;

    do{
        id = rand() % 900000 + 100000;
    }
    while(eventIDExists(id));
    return id;
}

int generateResourceID(){

    int id;

    do{
        id = rand() % 9000 + 1000;   // 100-999
    }
    while(searchResource(root,id) != NULL);
    return id;
}

/* =================================================== RESOURCE BST FUNCTIONS =================================================== */

Resource* createResource(int id, char name[]) {
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

//BST Search
Resource* searchResource(Resource* root, int id) {
    if (root == NULL || root->resourceID == id)
        return root;
    if (id < root->resourceID) {
        return searchResource(root->left, id);
    }
    else {
        return searchResource(root->right, id);
    }
}

//Inorder Traversal to search by name
int searchResourceByName(Resource* root, char name[]) {

    if (name == NULL || strlen(name) == 0) return 0;
    
    if(root == NULL)
        return 0;

    int found = 0;

    found |= searchResourceByName(root->left, name);  // "|= means bitwise OR assignment

    if(partialMatch(root->name, name)) {
        printf(CYAN "  Found:" RESET " %-6d | %s\n", root->resourceID, root->name);
        found = 1;
    }

    found |= searchResourceByName(root->right, name);

    return found;
}

void displayResources() {
    if(root == NULL) {
        printf(RED "  No resources available.\n" RESET);
        return;
    }
    printf("\n" CYAN "+--------+------------------------------------+" RESET "\n");
    printf(CYAN "| RoomID | %-34s |" RESET "\n", "Name");
    printf(CYAN "+--------+------------------------------------+" RESET "\n");
    inorderResources(root);
    printf(CYAN "+--------+------------------------------------+" RESET "\n");
}

void inorderResources(Resource* root) {
    if(root == NULL) return;
    inorderResources(root->left);
    printf("| " YELLOW "%-6d" RESET " | %-34.34s |\n", root->resourceID, root->name);
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
        if(eventList[i].isCancelled || eventList[i].isDone)
            continue;       //Cancelled/done events don't occupy the room so we ignore them

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
        printf(RED "  Invalid date format!\n" RESET);
        return;
    }

    printf("Enter Start Time: ");
    scanf("%d",&start);

    printf("Enter End Time: ");
    scanf("%d",&end);

    if(!validTime(start) || !validTime(end) || start >= end){
        printf(RED "  Invalid time range.\n" RESET);
        return;
    }

    printf("\n" CYAN "+--------+------------------------------------+" RESET "\n");
    printf(CYAN "|      AVAILABLE RESOURCES                   |" RESET "\n");
    printf(CYAN "+--------+------------------------------------+" RESET "\n");
    showAvailableResourcesRecursive(root, start, end, date);
    printf(CYAN "+--------+------------------------------------+" RESET "\n");
}

void showAvailableResourcesRecursive(Resource* root, int start, int end, int date) {
    if(root == NULL)
        return;

    showAvailableResourcesRecursive(root->left, start, end, date);

    if(isResourceAvailable(root->resourceID, start, end, date)){
        printf("| " YELLOW "%-6d" RESET " | %-34.34s |\n", root->resourceID, root->name);
    }

    showAvailableResourcesRecursive(root->right, start, end, date);
}

int countResources(Resource* root) {
    if(root==NULL) return 0;
    return 1 + countResources(root->left) + countResources(root->right);        //Recursively count resources in BST
}

/* ============================================================================================================================= */
/* ========================================================= PRIORITY QUEUE FUNCTIONS ================================================== */

void swap(int* a, int* b){
    int temp = *a;
    *a = *b;
    *b = temp;
}

// Returns 1 if event at index A should come BEFORE event at index B
int compareEvents(int a, int b){
    // first sort by priority
    if(eventList[a].priority != eventList[b].priority)
        return eventList[a].priority < eventList[b].priority;

    // same priority then sort by date
    if(eventList[a].date != eventList[b].date)
        return eventList[a].date < eventList[b].date;

    // same date then sort by start time
    return eventList[a].startTime < eventList[b].startTime;
}

void heapifyUp(PriorityQueue* pq, int i) {
    while(i > 0) {
        int parent = (i-1)/2;

        if(compareEvents(pq->indices[i], pq->indices[parent])){
            swap(&pq->indices[parent], &pq->indices[i]);
            i = parent;
        }
        else break;
    }
}

void heapifyDown(PriorityQueue* pq, int i) {
    int best = i;
    int left = 2*i+1;
    int right = 2*i+2;

    if(left < pq->size && compareEvents(pq->indices[left], pq->indices[best]))
        best = left;

    if(right < pq->size && compareEvents(pq->indices[right], pq->indices[best]))
        best = right;

    if (best != i) {
        swap(&pq->indices[best], &pq->indices[i]);
        heapifyDown(pq, best);
    }
}

void insertIntoPQ(PriorityQueue* pq, int index) {
    if (pq->size >= MAX_EVENTS) {
        printf(RED "  Priority Queue Full!\n" RESET);
        return;
    }
    pq->indices[pq->size] = index;
    pq->size++;
    heapifyUp(pq, pq->size - 1);
}

int priorityOrder(PriorityQueue* pq) {
    if (pq->size == 0)
        return -1;
    int top = pq->indices[0];
    pq->indices[0] = pq->indices[pq->size - 1];
    pq->size--;
    heapifyDown(pq, 0);
    return top;
}

void rebuildHeap(PriorityQueue* pq){
    for(int i = pq->size/2 - 1; i >= 0; i--){
        heapifyDown(pq, i);
    }
}

/* =================================================== EVENT FUNCTIONS =========================================================================================== */

Event* searchEventByID(int id) {
    for (int i = 0; i < totalEvents; i++) {
        if (eventList[i].eventID == id && !eventList[i].isCancelled && !eventList[i].isDone)
            return &eventList[i];
    }
    return NULL;
}

void searchEventByName(char* keyword) {
    int found = 0;

    printf("\n" CYAN "+--------+------------------------------------------+------+------------+-------------+---------------------------+----------+" RESET "\n");
    printf(CYAN "| %-6s | %-40s | %-3s | %-10s | %-11s | %-25s | %-8s |" RESET "\n","ID", "Event Name", "Pri", "Date", "Time", "Resource", "Slots");
    printf(CYAN "+--------+------------------------------------------+------+------------+-------------+---------------------------+----------+" RESET "\n");

    for (int i = 0; i < totalEvents; i++) {
        if (partialMatch(eventList[i].name, keyword)) {
            printEventRow(&eventList[i]);
            found = 1;
        }
    }

    printf(CYAN "+--------+------------------------------------------+------+------------+-------------+---------------------------+----------+" RESET "\n");

    if (found==0)
        printf(RED "  No events found matching \"%s\".\n" RESET, keyword);
}

int validDate(int date){
    int year = date / 10000;
    int month = (date / 100) % 100;
    int day = date % 100;
    
    if(year < 2024 || year > 2050) return 0;
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

int validTime(int t) {
    int hour=t/100;
    int min=t%100;

    if(hour < 0 || hour > 23)
        return 0;

    if(min < 0 || min > 59)
        return 0;
    else
    return 1;
}

void printTime(int t){
    int hour = t / 100;
    int min = t % 100;

    printf("%02d:%02d", hour, min);
}

int checkTimeConflict(int start, int end, int resourceID, int ignoreEventID, int date) {
    for (int i = 0; i < totalEvents; i++) {
        if(eventList[i].isCancelled || eventList[i].isDone)
            continue;

        if(eventList[i].resourceID == resourceID && eventList[i].eventID != ignoreEventID &&
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
    if(p==2) return "MED ";
    return "LOW ";
}

void addEvent() {
    
    // Check if resources exist first
    if(root == NULL){
        printf(RED "  No resources available. Add resources first.\n" RESET);
        return;
    }

    if (totalEvents >= MAX_EVENTS) {
        printf(RED "  Event limit reached!\n" RESET);
        return;
    }
  
    Event* e = &eventList[totalEvents];

    printf("Enter Event Name: ");
    scanf(" %149[^\n]", e->name);

    printf("Enter Priority (1=High, 2=Medium, 3=Low): ");
    scanf("%d", &e->priority);

    if(e->priority <1 || e->priority >3){
        printf(RED "  Invalid priority!\n" RESET);
        return;
    }

    printf("Enter Event Date (YYYYMMDD): ");
    scanf("%d", &e->date);

    if(!validDate(e->date)){
        printf(RED "  Invalid date!\n" RESET);
        return;
    }

    printf("Enter Start Time (e.g., 1300): ");
    scanf("%d", &e->startTime);

    printf("Enter End Time: ");
    scanf("%d", &e->endTime);

    if(!validTime(e->startTime)){
        printf(RED "  Invalid Start Time!\n" RESET);
        return;
    }

    if(!validTime(e->endTime)){
        printf(RED "  Invalid End Time!\n" RESET);
        return;
    }

    if(e->startTime >= e->endTime){
        printf(RED "  Invalid time range!\n" RESET);
        return;
    }

    printf("\n" CYAN "+--------+------------------------------------+" RESET "\n");
    printf(CYAN "|   AVAILABLE RESOURCES FOR SELECTED TIME    |" RESET "\n");
    printf(CYAN "+--------+------------------------------------+" RESET "\n");
    showAvailableResourcesRecursive(root, e->startTime, e->endTime, e->date);
    printf(CYAN "+--------+------------------------------------+" RESET "\n");

    // Created a resource name search to find the resource id
    printf("Enter Resource ID: ");
    scanf("%d", &e->resourceID);

    // checking for resource existence
    if(searchResource(root, e->resourceID) == NULL) {
        printf(RED "  Resource does not exist!\n" RESET);
        return;
    }

    if(!isResourceAvailable(e->resourceID, e->startTime, e->endTime, e->date)){
        printf(RED "  Selected resource is not available in that time range!\n" RESET);
        return;
    }

    if (checkTimeConflict(e->startTime, e->endTime, e->resourceID, -1, e->date)) {
        printf(RED "  Time conflict detected! Event not added.\n" RESET);
        return;
    }

    //If everything is valid, then generate ID
    e->eventID = generateEventID();
    printf(GREEN "  Generated Event ID: %d\n" RESET, e->eventID);

    printf("Enter Maximum Participants: ");
    scanf("%d", &e->maxParticipants);

    if(e->maxParticipants <= 0) {
        printf(RED "  Invalid participant limit!\n" RESET);
        return;
    }

    e->participantHead = NULL;
    e->participantCount = 0;
    e->isCancelled = 0;
    e->isDone = 0;
    e->participantCounter = 1;

    insertIntoPQ(&pq, totalEvents);
    totalEvents++;

    printf(GREEN "  Event Added & Scheduled Successfully!\n" RESET);
}

void searchEventMenu() {
    int searchType;
    printf("Search by:\n");
    printf("  1. Event ID\n");
    printf("  2. Event Name \n");
    printf("Enter choice: ");
    scanf("%d", &searchType);

    if (searchType == 1) {
        int id;
        printf("Enter Event ID: ");
        scanf("%d", &id);

        Event* e = NULL;
        for (int i = 0; i < totalEvents; i++) {
            if (eventList[i].eventID == id) {
                e = &eventList[i];
                break;
            }
        }

        if (e == NULL) {
            printf(RED "  Event not found!\n" RESET);
            return;
        }

        Resource* r = searchResource(root, e->resourceID);
        const char* statusColor = e->isDone ? GREEN : (e->isCancelled ? RED : YELLOW);
        const char* statusText  = e->isDone ? "DONE" : (e->isCancelled ? "CANCELLED" : "ACTIVE");

        printf("\n" CYAN "+----------------------------------------------+" RESET "\n");
        printf(CYAN "|              EVENT DETAILS                   |" RESET "\n");
        printf(CYAN "+----------------------------------------------+" RESET "\n");
        printf(CYAN "|" RESET " Name     : %-34.34s " CYAN "|" RESET "\n", e->name);
        printf(CYAN "|" RESET " Priority : %s%-34s" RESET CYAN "|" RESET "\n", priorityColor(e->priority), priorityText(e->priority));
        printf(CYAN "|" RESET " Date     : "); printDate(e->date);
        printf("                           " CYAN "|" RESET "\n");
        printf(CYAN "|" RESET " Time     : "); printTime(e->startTime);
        printf(" - "); printTime(e->endTime);
        printf("                        " CYAN "|" RESET "\n");
        printf(CYAN "|" RESET " Resource : %-34.34s " CYAN "|" RESET "\n", r ? r->name : "Unknown");
        printf(CYAN "|" RESET " Status   : %s%-34s" RESET CYAN "|" RESET "\n", statusColor, statusText);
        printf(CYAN "+----------------------------------------------+" RESET "\n");
    } 
    else if (searchType == 2) {
        char keyword[150];
        printf("Enter Event Name : ");
        scanf(" %149[^\n]", keyword);
        searchEventByName(keyword);
    }
    else {
        printf(RED "  Invalid choice!\n" RESET);
    }
}

void updateEvent(int id){

    Event* e = searchEventByID(id);

    if(e == NULL){
        printf(RED "  Event not found or not active!\n" RESET);
        return;
    }

    char newName[150];
    int newPriority;
    int newStart;
    int newEnd;
    int newDate;

    printf("Enter New Event Name: ");
    scanf(" %[^\n]", newName);

    printf("Enter New Priority (1=High, 2=Medium, 3=Low): ");
    scanf("%d",&newPriority);

    printf("Enter New Date (YYYYMMDD): ");
    scanf("%d",&newDate);

    printf("Enter New Start Time: ");
    scanf("%d",&newStart);

    printf("Enter New End Time: ");
    scanf("%d",&newEnd);

    if(newPriority <1 || newPriority >3){
        printf(RED "  Invalid priority!\n" RESET);
        return;
    }

    if(!validDate(newDate)){
        printf(RED "  Invalid date!\n" RESET);
        return;
    }

    if(!validTime(newStart) || !validTime(newEnd)){
        printf(RED "  Invalid time!\n" RESET);
        return;
    }

    if(newStart >= newEnd){
        printf(RED "  Invalid time range!\n" RESET);
        return;
    }

    if(checkTimeConflict(newStart, newEnd, e->resourceID, e->eventID, newDate)){
        printf(RED "  Time conflict detected!\n" RESET);
        return;
    }

    strcpy(e->name,newName);
    e->priority = newPriority;
    e->date = newDate;
    e->startTime = newStart;
    e->endTime = newEnd;

    rebuildHeap(&pq);

    printf(GREEN "  Event Updated Successfully!\n" RESET);
}

// Shared header for both event table displays
void printEventTableHeader(){
    printf("\n" CYAN "+--------+------------------------------------------+------+------------+-------------+---------------------------+----------+" RESET "\n");
    printf(       CYAN "| %-6s | %-40s | %-3s | %-10s | %-11s | %-25s | %-8s |" RESET "\n","ID", "Event Name", "Pri ", "Date", "Time", "Resource", "Slots");
    printf(       CYAN "+--------+------------------------------------------+------+------------+-------------+---------------------------+----------+" RESET "\n");
}

void printEventTableFooter(){
    printf(CYAN "+--------+------------------------------------------+-----+------------+-------------+---------------------------+----------+" RESET "\n");
}

void printEventRow(Event* e){
    Resource* r = searchResource(root, e->resourceID);
    char timeStr[12];
    char slots[10];

    // Build time string into a buffer so we can print it in one shot
    snprintf(timeStr, sizeof(timeStr), "%02d:%02d-%02d:%02d",  //timeStr is where to write and sizeof is how much to write
             e->startTime/100, e->startTime%100,
             e->endTime/100, e->endTime%100);

    snprintf(slots, sizeof(slots), "%d/%d", e->participantCount, e->maxParticipants);

    printf("| " YELLOW "%-6d" RESET " | %-40.40s | %s%-3s" RESET " | ",
           e->eventID, e->name, priorityColor(e->priority), priorityText(e->priority));
    printDate(e->date);
    printf(" | %-11s | %-25.25s | %-8s |\n",timeStr,r ? r->name : "Unknown",slots);
}

void displayAllEvents() {
    // count active events first
    int count = 0;
    for(int i = 0; i < totalEvents; i++){
        if(eventList[i].isCancelled==0 && eventList[i].isDone==0)
            count++;
    }

    if(count==0) {
        printf(YELLOW "  No active events available.\n" RESET);
        return;
    }

    int sortChoice;
    printf("Sort by:\n");
    printf("  1. Priority\n");
    printf("  2. Date & Time\n");
    printf("Enter choice: ");
    scanf("%d", &sortChoice);

    if(sortChoice == 1){

        // use heap copy to extract in priority order
        PriorityQueue temp = pq;

        printf("\n" CYAN BOLD "  EVENTS — PRIORITY ORDER" RESET "\n");
        printEventTableHeader();

        while(temp.size > 0){
            int idx = priorityOrder(&temp);
            if(idx == -1) break;
            Event* e = &eventList[idx];
            if(e->isCancelled || e->isDone) continue;
            printEventRow(e);
        }

        printEventTableFooter();

    } else if(sortChoice == 2){

        // collect active indices
        int active[MAX_EVENTS];
        int k = 0;
        for(int i = 0; i < totalEvents; i++){
            if(!eventList[i].isCancelled && !eventList[i].isDone)
                active[k++] = i;
        }

        // sort by date then startTime
        for(int i = 0; i < k-1; i++){
            for(int j = i+1; j < k; j++){
                if(eventList[active[i]].date > eventList[active[j]].date ||
                  (eventList[active[i]].date == eventList[active[j]].date &&
                   eventList[active[i]].startTime > eventList[active[j]].startTime)){
                    int temp = active[i];
                    active[i] = active[j];
                    active[j] = temp;
                }
            }
        }

        printf("\n" CYAN BOLD "  EVENTS — DATE/TIME ORDER" RESET "\n");
        printEventTableHeader();

        for(int i = 0; i < k; i++)
            printEventRow(&eventList[active[i]]);

        printEventTableFooter();

    } else {
        printf(RED "  Invalid choice!\n" RESET);
    }
}

void displayEventsByDate() {
    int date;
    printf("Enter Date (YYYYMMDD): ");
    scanf("%d", &date);

    if(!validDate(date)){
        printf(RED "  Invalid date!\n" RESET);
        return;
    }

    // collect active events matching the date
    int matches[MAX_EVENTS];
    int count = 0;

    for(int i = 0; i < totalEvents; i++){
        if(!eventList[i].isCancelled && !eventList[i].isDone
           && eventList[i].date == date){
            matches[count++] = i;
        }
    }

    if(count == 0){
        printf(YELLOW "  No active events found for this date.\n" RESET);
        return;
    }

    int sortChoice;
    printf("Sort by:\n");
    printf("  1. Priority\n");
    printf("  2. Time\n");
    printf("Enter choice: ");
    scanf("%d", &sortChoice);

    if(sortChoice == 1){
        // sort by priority then time — reuse compareEvents
        for(int i = 0; i < count-1; i++){
            for(int j = i+1; j < count; j++){
                if(compareEvents(matches[j], matches[i])){
                    int temp = matches[i];
                    matches[i] = matches[j];
                    matches[j] = temp;
                }
            }
        }
        printf("\n" CYAN BOLD "  EVENTS ON ");
        printDate(date);
        printf(" — PRIORITY ORDER" RESET "\n");

    } else if(sortChoice == 2){
        // sort by time only
        for(int i = 0; i < count-1; i++){
            for(int j = i+1; j < count; j++){
                if(eventList[matches[i]].startTime > eventList[matches[j]].startTime){
                    int temp = matches[i];
                    matches[i] = matches[j];
                    matches[j] = temp;
                }
            }
        }
        printf("\n" CYAN BOLD "  EVENTS ON ");
        printDate(date);
        printf(" — TIME ORDER" RESET "\n");

    } else {
        printf(RED "  Invalid choice!\n" RESET);
        return;
    }

    printEventTableHeader();
    for(int i = 0; i < count; i++)
        printEventRow(&eventList[matches[i]]);
    printEventTableFooter();
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
        printf(RED "  Event not found!\n" RESET);
        return;
    }

    if(e->isDone){
        printf(YELLOW "  Event already completed!\n" RESET);
        return;
    }

    if(e->isCancelled){
        printf(YELLOW "  Event already cancelled!\n" RESET);
        return;
    }

    e->isCancelled = 1;

    printf(GREEN "  Event Cancelled Successfully! Participants preserved.\n" RESET);
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
        printf(RED "  Event not found!\n" RESET);
        return;
    }

    if(e->isCancelled){
        printf(YELLOW "  Event is cancelled!\n" RESET);
        return;
    }

    if(e->isDone){
        printf(YELLOW "  Event already marked as done!\n" RESET);
        return;
    }

    e->isDone = 1;

    printf(GREEN "  Event marked as completed! Participants preserved.\n" RESET);
}

void displayDoneEvents(){
    int found = 0;

    printf("\n" CYAN "+--------+------------------------------------------+------------+--------+" RESET "\n");
    printf(     CYAN "| %-6s | %-40s | %-10s | %-6s |" RESET "\n", "ID", "Event Name", "Date", "People");
    printf(     CYAN "+--------+------------------------------------------+------------+--------+" RESET "\n");

    for(int i=0;i<totalEvents;i++){
        if(eventList[i].isDone){
            printf("| " GREEN "%-6d" RESET " | %-40.40s | ", eventList[i].eventID, eventList[i].name);
            printDate(eventList[i].date);
            printf(" | %-6d |\n", eventList[i].participantCount);
            found = 1;
        }
    }

    if(!found)
        printf(CYAN "|" RESET YELLOW "  No completed events.                                                    " CYAN "|" RESET "\n");

    printf(CYAN "+--------+------------------------------------------+------------+--------+" RESET "\n");
}

void displayCancelledEvents(){
    int found = 0;

    printf("\n" CYAN "+--------+------------------------------------------+------------+--------+" RESET "\n");
    printf(     CYAN "| %-6s | %-40s | %-10s | %-6s |" RESET "\n", "ID", "Event Name", "Date", "People");
    printf(     CYAN "+--------+------------------------------------------+------------+--------+" RESET "\n");

    for(int i=0;i<totalEvents;i++){
        if(eventList[i].isCancelled){
            printf("| " RED "%-6d" RESET " | %-40.40s | ", eventList[i].eventID, eventList[i].name);
            printDate(eventList[i].date);
            printf(" | %-6d |\n", eventList[i].participantCount);
            found = 1;
        }
    }

    if(!found)
        printf(CYAN "|" RESET YELLOW "  No cancelled events.                                                    " CYAN "|" RESET "\n");

    printf(CYAN "+--------+------------------------------------------+------------+--------+" RESET "\n");
}

/* ========================================================================================================================================================= */
/* ============================================ PARTICIPANT FUNCTIONS ===================================================================================== */

void registerParticipant(int eventID) {

    Event* e = searchEventByID(eventID);

    if (e == NULL) {
        printf(RED "  Active event not found!\n" RESET);
        return;
    }

    if(e->isCancelled){
        printf(RED "  Cannot register. Event is cancelled!\n" RESET);
        return;
    }

    if(e->isDone){
        printf(RED "  Cannot register. Event is already completed!\n" RESET);
        return;
    }

    if (e->participantCount >= e->maxParticipants) {
        printf(RED "  Participant limit reached!\n" RESET);
        return;
    }
    int newID = e->participantCounter++;
    printf(GREEN "  Generated Participant ID: %d\n" RESET, newID);

    Participant* newP = (Participant*)malloc(sizeof(Participant));
    if(newP == NULL){
        printf(RED "  Memory allocation failed!\n" RESET);
        return;
    }

    newP->participantID = newID;

    printf("Enter Participant Name: ");
    scanf(" %99[^\n]", newP->name);

    newP->next = e->participantHead;
    e->participantHead = newP;
    e->participantCount++;

    printf(GREEN "  Participant Registered Successfully!\n" RESET);
}

void removeParticipant(int eventID, int participantID) {

    Event* e = searchEventByID(eventID);

    if(e == NULL){
        printf(RED "  Active event not found!\n" RESET);
        return;
    }

    if(e->isCancelled){
        printf(YELLOW "  Event is cancelled!\n" RESET);
        return;
    }

    if(e->isDone){
        printf(YELLOW "  Event is already completed!\n" RESET);
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

            printf(GREEN "  Participant removed successfully!\n" RESET);
            return;
        }

        prev = temp;
        temp = temp->next;
    }

    printf(RED "  Participant not found!\n" RESET);
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
        printf(RED "  Event not found!\n" RESET);
        return;
    }

    if(e->participantHead == NULL){
        printf(YELLOW "  No participants registered for this event.\n" RESET);
        return;
    }

    const char* statusColor = e->isDone ? GREEN : (e->isCancelled ? RED : YELLOW);
    const char* statusText  = e->isDone ? "DONE" : (e->isCancelled ? "CANCELLED" : "ACTIVE");

    Participant* temp = e->participantHead;

    printf("\n" CYAN "+-------+----------------------------------------+" RESET "\n");
    printf(     CYAN "|  PARTICIPANTS : %-30.30s   |" RESET "\n", e->name);
    printf(     CYAN "|  Status       : %s%-10s" RESET CYAN "                            |" RESET "\n", statusColor, statusText);
    printf(     CYAN "+-------+----------------------------------------+" RESET "\n");
    printf(     CYAN "| %-5s | %-38s   |" RESET "\n", "ID", "Name");
    printf(     CYAN "+-------+----------------------------------------+" RESET "\n");

    while (temp != NULL) {
        printf("| " YELLOW "%03d" RESET "   | %-40.40s |\n", temp->participantID, temp->name);
        temp = temp->next;
    }
    printf(CYAN "+-------+----------------------------------------+" RESET "\n");
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
/* ====================================== SYSTEM SUMMARY ========================================================================== */

void systemSummary(){

    int active=0;
    int cancelled=0;
    int done=0;
    int totalParticipants = 0;
    int eventsInQueue = 0;

    for(int i=0;i<totalEvents;i++){
        if(eventList[i].isCancelled)
            cancelled++;
        else if(eventList[i].isDone)
            done++;
        else
            active++;
        totalParticipants += eventList[i].participantCount;
    }

    for(int i = 0; i < pq.size; i++) {
        int idx = pq.indices[i];
        if(!eventList[idx].isCancelled && !eventList[idx].isDone)
            eventsInQueue++;
    }

    printf("\n" CYAN "+----------------------------------+" RESET "\n");
    printf(     CYAN "|         SYSTEM SUMMARY           |" RESET "\n");
    printf(     CYAN "+----------------------------------+" RESET "\n");
    printf(     CYAN "|" RESET " Total Events Created   : " YELLOW "%-6d" RESET CYAN " |" RESET "\n", totalEvents);
    printf(     CYAN "|" RESET " Active Events          : " GREEN  "%-6d" RESET CYAN " |" RESET "\n", active);
    printf(     CYAN "|" RESET " Completed Events       : " GREEN  "%-6d" RESET CYAN " |" RESET "\n", done);
    printf(     CYAN "|" RESET " Cancelled Events       : " RED    "%-6d" RESET CYAN " |" RESET "\n", cancelled);
    printf(     CYAN "|" RESET " Events in Queue        : " YELLOW "%-6d" RESET CYAN " |" RESET "\n", eventsInQueue);
    printf(     CYAN "|" RESET " Total Participants     : " YELLOW "%-6d" RESET CYAN " |" RESET "\n", totalParticipants);
    printf(     CYAN "|" RESET " Total Resources        : " YELLOW "%-6d" RESET CYAN " |" RESET "\n", countResources(root));
    printf(     CYAN "+----------------------------------+" RESET "\n");
}

/* ========================================= FILE SAVE FUNCTION ================================================= */

void saveResourcesRecursive(Resource* node, FILE *fp){

    if(node == NULL)
        return;

    saveResourcesRecursive(node->left, fp);

    fprintf(fp,"%d|%s\n", node->resourceID, node->name);

    saveResourcesRecursive(node->right, fp);
}

void saveResourcesToFile(){

    FILE *fp = fopen("resources.txt","w");

    if(fp == NULL){
        printf(RED "  Error saving resources!\n" RESET);
        return;
    }

    saveResourcesRecursive(root, fp);

    fclose(fp);
}
void loadResourcesFromFile(){

    FILE *fp = fopen("resources.txt","r");

    if(fp == NULL)
        return;

    int id;
    char name[80];

    while(fscanf(fp,"%d|%79[^\n]\n",&id,name) != EOF){
        root = insertResource(root,id,name);
    }

    fclose(fp);
}
void saveEventsToFile(){

    FILE *fp = fopen("events.txt","w");

    if(fp == NULL){
        printf(RED "  Error saving events!\n" RESET);
        return;
    }

    fprintf(fp,"%d\n", totalEvents);

    for(int i=0;i<totalEvents;i++){

        Event *e = &eventList[i];

        fprintf(fp,"%d|%s|%d|%d|%d|%d|%d|%d|%d|%d\n",e->eventID,e->name,e->priority,
            e->date,e->startTime,e->endTime,e->resourceID,e->isCancelled,e->isDone,e->maxParticipants);
    }
    fclose(fp);
}
void loadEventsFromFile(){

    FILE *fp = fopen("events.txt","r");

    if(fp == NULL)
        return;

    fscanf(fp,"%d\n",&totalEvents);

    if(totalEvents > MAX_EVENTS)
        totalEvents = MAX_EVENTS;

    for(int i=0;i<totalEvents;i++){

        Event *e = &eventList[i];

        fscanf(fp,"%d|%149[^|]|%d|%d|%d|%d|%d|%d|%d|%d\n",&e->eventID,e->name,&e->priority,&e->date,&e->startTime,&e->endTime,
            &e->resourceID,&e->isCancelled,&e->isDone,&e->maxParticipants);
        e->participantHead = NULL;
        e->participantCount = 0;
        e->participantCounter = 1;

        if(!e->isCancelled && !e->isDone)
            insertIntoPQ(&pq, i);
    }

    fclose(fp);
}

void saveParticipantsToFile(){

    FILE *fp = fopen("participants.txt","w");

    if(fp == NULL){
        printf(RED "  Error saving participants!\n" RESET);
        return;
    }

    for(int i=0;i<totalEvents;i++){

        Participant *p = eventList[i].participantHead;

        while(p != NULL){

            fprintf(fp,"%d | %d | %s\n",
                eventList[i].eventID,
                p->participantID,
                p->name);

            p = p->next;
        }
    }
    fclose(fp);
}
void loadParticipantsFromFile(){

    FILE *fp = fopen("participants.txt","r");

    if(fp == NULL)
        return;

    int eventID, pid;
    char name[100];

    while(fscanf(fp,"%d | %d | %99[^\n]\n",&eventID,&pid,name) != EOF){

        Event *e = NULL;

        for(int i=0;i<totalEvents;i++){
            if(eventList[i].eventID == eventID){
                e = &eventList[i];
                break;
            }
        }

        if(e != NULL){

            Participant *newP = malloc(sizeof(Participant));

            if(newP == NULL)
                return;

            newP->participantID = pid;
            strcpy(newP->name,name);

            newP->next = e->participantHead;
            e->participantHead = newP;

            e->participantCount++;

            if(pid >= e->participantCounter)
                e->participantCounter = pid + 1;
        }
    }

    fclose(fp);
}

/* =============================================== MAIN MENU ================================================================================================= */

void menu() {
    printf(CYAN "+==========================================+" RESET "\n");
    printf(CYAN "|" WHITE "   EVENT MANAGEMENT & SCHEDULING SYSTEM   " CYAN "|" RESET "\n");
    printf(CYAN "+==========================================+" RESET "\n");
    printf(CYAN "|" RESET "  1.  Add Event                           " CYAN "|" RESET "\n");
    printf(CYAN "|" RESET "  2.  Register Participant                " CYAN "|" RESET "\n");
    printf(CYAN "|" RESET "  3.  Display All Events                  " CYAN "|" RESET "\n");
    printf(CYAN "|" RESET "  4.  Display Events by Date/Time         " CYAN "|" RESET "\n");
    printf(CYAN "|" RESET "  5.  Display Participants                " CYAN "|" RESET "\n");
    printf(CYAN "|" RESET "  6.  Remove Participant                  " CYAN "|" RESET "\n");
    printf(CYAN "|" RESET "  7.  Update Event                        " CYAN "|" RESET "\n");
    printf(CYAN "|" RESET "  8.  Search Event                        " CYAN "|" RESET "\n");
    printf(CYAN "|" RESET "  9.  Cancel Event                        " CYAN "|" RESET "\n");
    printf(CYAN "+------------------------------------------+" RESET "\n");
    printf(CYAN "|" RESET "  10. Add Resource                        " CYAN "|" RESET "\n");
    printf(CYAN "|" RESET "  11. Display Resources                   " CYAN "|" RESET "\n");
    printf(CYAN "|" RESET "  12. Search Resource                     " CYAN "|" RESET "\n");
    printf(CYAN "|" RESET "  13. Show Available Resources            " CYAN "|" RESET "\n");
    printf(CYAN "+------------------------------------------+" RESET "\n");
    printf(CYAN "|" RESET "  14. Mark Event as Completed             " CYAN "|" RESET "\n");
    printf(CYAN "|" RESET "  15. Show Completed Events               " CYAN "|" RESET "\n");
    printf(CYAN "|" RESET "  16. Show Cancelled Events               " CYAN "|" RESET "\n");
    printf(CYAN "|" RESET "  17. System Summary                      " CYAN "|" RESET "\n");
    printf(CYAN "|" RED   "  18. Exit                                " CYAN "|" RESET "\n");
    printf(CYAN "+==========================================+" RESET "\n");
    printf("  Enter choice: ");
}

int main() {
    pq.size = 0;
    root = NULL;
    loadResourcesFromFile();
    loadEventsFromFile();
    loadParticipantsFromFile();

    srand(time(NULL));  //For random number generation
    int choice, id;
    char name[80];

    while (1) {
        menu();
        scanf("%d", &choice);

        switch(choice) {

        case 1:
            addEvent();
            pause();
            break;

        case 2:
            printf("Enter Event ID: ");
            scanf("%d", &id);
            registerParticipant(id);
            pause();
            break;

        case 3:
            displayAllEvents();
            pause();
            break;
            
        case 4:
            displayEventsByDate();
            pause();
            break;
            
        case 5:
            printf("Enter Event ID to Display Participants: ");
            scanf("%d", &id);
            displayParticipants(id);
            pause();
            break;

        case 6:
            {
            int eventID, participantID;

            printf("Enter Event ID: ");
            scanf("%d",&eventID);

            printf("Enter Participant ID to remove: ");
            scanf("%d",&participantID);

            removeParticipant(eventID,participantID);
            pause();
            }
            break;

        case 7:
            printf("Enter Event ID to Update: ");
            scanf("%d", &id);
            updateEvent(id);
            pause();
            break;

        case 8:
            searchEventMenu();
            pause();
            break;

        case 9:
            printf("Enter Event ID to Cancel: ");
            scanf("%d", &id);
            cancelEvent(id);
            pause();
            break;

        case 10:
            id = generateResourceID();
            printf(GREEN "  Generated Resource ID: %d\n" RESET, id);
            printf("Enter Resource Name: ");
            scanf(" %[^\n]", name);
            
            root = insertResource(root, id, name);
            printf(GREEN "  Resource Added!\n" RESET);
            pause();
            break;

        case 11:
            displayResources();
            pause();
            break;

        case 12:
            {
                printf("Enter Resource Name: ");
                scanf(" %[^\n]", name);

                if(!searchResourceByName(root, name))
                    printf(RED "  Resource not found!\n" RESET);
                pause();
            }
            break;
            
        case 13:
            showAvailableResources();
            pause();
            break;
            
        case 14:
            printf("Enter Event ID to mark as Completed: ");
            scanf("%d", &id);
            completeEvent(id);
            pause();
            break;
            
        case 15:
            displayDoneEvents();
            pause();
            break;
            
        case 16:
            displayCancelledEvents();
            pause();
            break;
            
        case 17:
            systemSummary();
            pause();
            break;
            
        case 18:
            printf(YELLOW "  Saving data...\n" RESET);
            saveResourcesToFile();
            saveEventsToFile();
            saveParticipantsToFile();

            for(int i=0;i<totalEvents;i++) {
                freeParticipants(&eventList[i]);
            }
            freeResource(root);
            printf(GREEN "  Goodbye!\n" RESET);
            exit(0);

        default:
            printf(RED "  Invalid Choice!\n" RESET);
            pause();
        }
    }
    return 0;
}