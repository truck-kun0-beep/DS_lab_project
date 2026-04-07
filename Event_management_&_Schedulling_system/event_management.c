#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <ctype.h>

#define MAX_EVENTS   10000
#define HASH_SIZE    16381   /* prime near 10000*1.6 for good load factor */

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

/* ======================================== EVENT STRUCTURE ================================== */
typedef struct Event {
    int eventID;
    char name[150];
    int priority;      /* 1=High, 2=Medium, 3=Low */
    int date;          /* YYYYMMDD */
    int startTime;     /* HHMM, e.g. 1300 = 1:00 PM */
    int endTime;
    int resourceID;
    int isCancelled;
    int isDone;

    Participant* participantHead;
    int participantCount;
    int maxParticipants;
    int participantCounter;
} Event;

/* ================================== RESOURCE BST ================================== */
typedef struct Resource {
    int resourceID;
    char name[80];
    struct Resource* left;
    struct Resource* right;
} Resource;

/* ================================== PRIORITY QUEUE (MIN-HEAP) ================================== */
typedef struct {
    int indices[MAX_EVENTS];
    int size;
} PriorityQueue;

/* ================================== HASH MAP: eventID -> eventList index ================================== */
/* Deletions mark slots as Deleted (tombstone) so probing chains stay intact. */
#define HM_EMPTY   -1   // slot has never been used
#define HM_DELETED -2   // slot held a key that was removed

typedef struct {
    int key;    // eventID
    int value;  // index in eventList[]
} HashSlot;

typedef struct {
    HashSlot slots[HASH_SIZE];
} HashMap;

/* ================================== GLOBAL VARIABLES ================================== */
Event eventList[MAX_EVENTS];
int totalEvents = 0;
PriorityQueue pq;
Resource* root = NULL;
HashMap eventMap;   //eventID lookup

/* ================================== FUNCTION PROTOTYPES ================================== */
/* Hash Map */
void   hmInit(HashMap* hm);
void   hmInsert(HashMap* hm, int key, int value);
int    hmGet(HashMap* hm, int key);          // returns index or -1
void   hmDelete(HashMap* hm, int key);
void   hmUpdateValue(HashMap* hm, int key, int newValue);

/* Heap */
int  priorityOrder(PriorityQueue* pq);
int  compareEvents(int a, int b);
void rebuildHeap(PriorityQueue* pq);
void insertIntoPQ(PriorityQueue* pq, int index);

/* Resources */
Resource* searchResource(Resource* root, int id);
void showAvailableResourcesRecursive(Resource* root, int start, int end, int date);
void freeResource(Resource* root);
void inorderResources(Resource* root);
int  countResources(Resource* root);

/* Events */
Event* searchEventByID(int id);
void   searchEventByName(char* keyword);
void   printEventRow(Event* e);
int    checkTimeConflict(int start, int end, int resourceID, int ignoreEventID, int date);
void   displayDoneEvents();
void   displayCancelledEvents();
void   displayAllEvents();
void   displayEventsByDate();

/* Participants */
void freeParticipants(Event* e);

/* Utilities */
int partialMatch(char haystack[], char needle[]);
const char* priorityColor(int p);
const char* priorityText(int p);
void pause();
int validTime(int t);
int validDate(int date);
void printTime(int t);
void printDate(int date);
void printEventTableHeader();
void printEventTableFooter();

/* Real-time */
void getCurrentDateTime(int* outDate, int* outTime);
void checkAndAutoComplete();
void printNextEventBanner();
void purgeHistory();

/* ID generators */
int eventIDExists(int id);
int generateEventID();
int generateResourceID();

/* File I/O */
void saveResourcesToFile();
void loadResourcesFromFile();
void saveEventsToFile();
void loadEventsFromFile();
void saveParticipantsToFile();
void loadParticipantsFromFile();

/* ================================================ HASH MAP IMPLEMENTATION ================================================ */

void hmInit(HashMap* hm) {
    for (int i = 0; i < HASH_SIZE; i++)
        hm->slots[i].key = HM_EMPTY;
}

static int hmHash(int key) {
    /* Knuth multiplicative hash, stays positive */
    unsigned int k = (unsigned int)key;
    return (int)((k * 2654435761u) % (unsigned int)HASH_SIZE);
}

void hmInsert(HashMap* hm, int key, int value) {
    int h = hmHash(key);
    int firstDeleted = -1;
    for (int i = 0; i < HASH_SIZE; i++) {
        int idx = (h + i) % HASH_SIZE;
        if (hm->slots[idx].key == HM_EMPTY) {
            int dest = (firstDeleted != -1) ? firstDeleted : idx;
            hm->slots[dest].key = key;
            hm->slots[dest].value = value;
            return;
        }
        if (hm->slots[idx].key == HM_DELETED) {
            if (firstDeleted == -1) firstDeleted = idx;
        } else if (hm->slots[idx].key == key) {
            hm->slots[idx].value = value;  /* update existing */
            return;
        }
    }
    // Fallback: use first deleted slot (table should never truly fill)
    if (firstDeleted != -1) {
        hm->slots[firstDeleted].key = key;
        hm->slots[firstDeleted].value = value;
    }
}

int hmGet(HashMap* hm, int key) {
    int h = hmHash(key);
    for (int i = 0; i < HASH_SIZE; i++) {
        int idx = (h + i) % HASH_SIZE;
        if (hm->slots[idx].key == HM_EMPTY)  return -1;
        if (hm->slots[idx].key == HM_DELETED) continue;
        if (hm->slots[idx].key == key) return hm->slots[idx].value;
    }
    return -1;
}

void hmDelete(HashMap* hm, int key) {
    int h = hmHash(key);
    for (int i = 0; i < HASH_SIZE; i++) {
        int idx = (h + i) % HASH_SIZE;
        if (hm->slots[idx].key == HM_EMPTY)  return;
        if (hm->slots[idx].key == HM_DELETED) continue;
        if (hm->slots[idx].key == key) {
            hm->slots[idx].key = HM_DELETED;
            return;
        }
    }
}

// After purge/compaction, indices in eventList[] shift — update map values
void hmUpdateValue(HashMap* hm, int key, int newValue) {
    int h = hmHash(key);
    for (int i = 0; i < HASH_SIZE; i++) {
        int idx = (h + i) % HASH_SIZE;
        if (hm->slots[idx].key == HM_EMPTY)  return;
        if (hm->slots[idx].key == HM_DELETED) continue;
        if (hm->slots[idx].key == key) {
            hm->slots[idx].value = newValue;
            return;
        }
    }
}

/* ================================================ UTILITIES ================================================ */

int partialMatch(char haystack[], char needle[]) {
    if (needle == NULL || strlen(needle) == 0) return 0;
    int hLen = strlen(haystack);
    int nLen = strlen(needle);
    if (nLen > hLen) return 0;
    for (int i = 0; i <= hLen - nLen; i++) {
        int match = 1;
        for (int j = 0; j < nLen; j++) {
            if (tolower(haystack[i+j]) != tolower(needle[j])) { match = 0; break; }
        }
        if (match) return 1;
    }
    return 0;
}

const char* priorityColor(int p) {
    if (p == 1) return RED;
    if (p == 2) return YELLOW;
    return GREEN;
}

const char* priorityText(int p) {
    if (p == 1) return "HIGH";
    if (p == 2) return "MED ";
    return "LOW ";
}

void pause() {
    printf("\n" CYAN "+-------------------------------------+" RESET "\n");
    printf(CYAN "|" RESET "      Press Enter to continue...     " CYAN "|" RESET "\n");
    printf(CYAN "+-------------------------------------+" RESET "\n");
    while (getchar() != '\n');
    getchar();
}

int validDate(int date) {
    int year = date / 10000;
    int month = (date / 100) % 100;
    int day = date % 100;
    if (year < 2024 || year > 2050) return 0;
    if (month < 1 || month > 12)  return 0;
    if (day < 1 || day > 31)    return 0;
    if ((month==4||month==6||month==9||month==11) && day > 30) return 0;
    if (month == 2 && day > 29) return 0;
    return 1;
}

void printDate(int date) {
    printf("%04d-%02d-%02d", date/10000, (date/100)%100, date%100);
}

int validTime(int t) {
    int hour = t / 100, min = t % 100;
    return (hour >= 0 && hour <= 23 && min >= 0 && min <= 59);
}

void printTime(int t) {
    printf("%02d:%02d", t/100, t%100);
}

/* ================================================ REAL-TIME CLOCK ================================================ */

void getCurrentDateTime(int* outDate, int* outTime) {
    time_t now = time(NULL);
    struct tm* t = localtime(&now);
    *outDate = (t->tm_year+1900)*10000 + (t->tm_mon+1)*100 + t->tm_mday;
    *outTime = t->tm_hour*100 + t->tm_min;
}

/* If the nearest event's end time has passed → pop, mark done, repeat. */
void checkAndAutoComplete() {
    int nowDate, nowTime;
    getCurrentDateTime(&nowDate, &nowTime);
    int autoCompleted = 0;

    while (pq.size > 0) {
        int idx = pq.indices[0];
        Event* e = &eventList[idx];

        if (e->isCancelled || e->isDone) { priorityOrder(&pq); continue; }

        int expired = (e->date < nowDate) || (e->date == nowDate && e->endTime <= nowTime);
        if (expired) {
            priorityOrder(&pq);
            e->isDone = 1;
            autoCompleted++;
            printf(GREEN "  [AUTO] Event \"%s\" marked as completed.\n" RESET, e->name);
        } else break;
    }
    if (autoCompleted > 0)
        printf(CYAN "  %d event(s) auto-completed based on current time.\n" RESET, autoCompleted);
}

void printNextEventBanner() {
    int bannerIdx = -1;
    for (int i = 0; i < pq.size; i++) {
        int idx = pq.indices[i];
        if (!eventList[idx].isCancelled && !eventList[idx].isDone) { bannerIdx = idx; break; }
    }

    int nowDate, nowTime;
    getCurrentDateTime(&nowDate, &nowTime);

    printf(CYAN "+==========================================+" RESET "\n");
    if (bannerIdx == -1) {
        printf(CYAN "|" YELLOW "  No upcoming events scheduled.           " CYAN "|" RESET "\n");
    } else {
        Event* e = &eventList[bannerIdx];
        Resource* r = searchResource(root, e->resourceID);
        int isNow = (e->date == nowDate && e->startTime <= nowTime && nowTime < e->endTime);

        printf(CYAN "|" RESET " Current time : " YELLOW "%04d-%02d-%02d  %02d:%02d" RESET
               "         " CYAN "|" RESET "\n",
               nowDate/10000, (nowDate/100)%100, nowDate%100, nowTime/100, nowTime%100);

        if (isNow)
            printf(CYAN "|" GREEN "  NOW LIVE" RESET ": %-30.30s " CYAN "|" RESET "\n", e->name);
        else
            printf(CYAN "|" RESET "  Next event  : %s%-26.26s" RESET  CYAN "|" RESET "\n", priorityColor(e->priority), e->name);

        printf(CYAN "|" RESET "  Date/Time   : ");
        printDate(e->date); printf("  "); printTime(e->startTime);
        printf("-"); printTime(e->endTime);
        printf("   " CYAN "|" RESET "\n");
        printf(CYAN "|" RESET "  Room        : %-24.24s " CYAN " |" RESET "\n", r ? r->name : "Unknown");
    }
}

/* ================================================ RESOURCE BST ================================================ */

Resource* createResource(int id, char name[]) {
    Resource* r = (Resource*)malloc(sizeof(Resource));
    if (r==NULL) {
        printf("Memory allocation failed\n");
        return NULL;
    }
    r->resourceID = id;
    strcpy(r->name, name);
    r->left = r->right = NULL;
    return r;
}

Resource* insertResource(Resource* r, int id, char name[]) {
    if (r==NULL) return createResource(id, name);
    if (id < r->resourceID) {
        r->left  = insertResource(r->left,  id, name);
    } else if (id > r->resourceID) {
        r->right = insertResource(r->right, id, name);
    }
    return r;
}

Resource* searchResource(Resource* r, int id) {
    if (r == NULL || r->resourceID == id) return r;
    return (id < r->resourceID) ? searchResource(r->left, id) : searchResource(r->right, id);
}

int searchResourceByName(Resource* r, char name[]) {
    if (r == NULL || name == NULL || strlen(name) == 0) return 0;
    int found = searchResourceByName(r->left, name);
    if (partialMatch(r->name, name)) {
        printf(CYAN "  Found:" RESET " %-6d | %s\n", r->resourceID, r->name);
        found = 1;
    }
    return found | searchResourceByName(r->right, name);
}

void inorderResources(Resource* r) {
    if (!r) return;
    inorderResources(r->left);
    printf("| " YELLOW "%-6d" RESET " | %-34.34s |\n", r->resourceID, r->name);
    inorderResources(r->right);
}

void displayResources() {
    if (!root) {
        printf(RED "  No resources available.\n" RESET);
        return;
    }
    printf("\n" CYAN "+--------+------------------------------------+" RESET "\n");
    printf(CYAN "| RoomID | %-34s |" RESET "\n", "Name");
    printf(CYAN "+--------+------------------------------------+" RESET "\n");
    inorderResources(root);
    printf(CYAN "+--------+------------------------------------+" RESET "\n");
}

void freeResource(Resource* r) {
    if (!r) return;
    freeResource(r->left); freeResource(r->right); free(r);
}

int countResources(Resource* r) {
    if (!r) return 0;
    return 1 + countResources(r->left) + countResources(r->right);
}

int isResourceAvailable(int resourceID, int start, int end, int date) {
    for (int i = 0; i < totalEvents; i++) {
        if (eventList[i].isCancelled || eventList[i].isDone) continue;
        if (eventList[i].resourceID == resourceID && eventList[i].date == date)
            if (eventList[i].startTime < end && start < eventList[i].endTime)
                return 0;
    }
    return 1;
}

void showAvailableResourcesRecursive(Resource* r, int start, int end, int date) {
    if (!r) return;
    showAvailableResourcesRecursive(r->left, start, end, date);
    if (isResourceAvailable(r->resourceID, start, end, date))
        printf("| " YELLOW "%-6d" RESET " | %-34.34s |\n", r->resourceID, r->name);
    showAvailableResourcesRecursive(r->right, start, end, date);
}

void showAvailableResources() {
    int start, end, date;
    printf("Enter Date (YYYYMMDD): ");
    scanf("%d", &date);
    if (!validDate(date)) { printf(RED "  Invalid date format!\n" RESET); return; }
    printf("Enter Start Time: ");
    scanf("%d", &start);
    printf("Enter End Time: ");
    scanf("%d", &end);
    if (!validTime(start) || !validTime(end) || start >= end) {
        printf(RED "  Invalid time range.\n" RESET); return;
    }
    printf("\n" CYAN "+--------+------------------------------------+" RESET "\n");
    printf(CYAN "|      AVAILABLE RESOURCES                   |" RESET "\n");
    printf(CYAN "+--------+------------------------------------+" RESET "\n");
    showAvailableResourcesRecursive(root, start, end, date);
    printf(CYAN "+--------+------------------------------------+" RESET "\n");
}

/* ================================================ PRIORITY QUEUE ================================================ */

void swap(int* a, int* b) {
    int temp = *a;
    *a = *b;
    *b = temp;
}

int compareEvents(int a, int b) {
    if (eventList[a].priority != eventList[b].priority)
        return eventList[a].priority < eventList[b].priority;
    if (eventList[a].date != eventList[b].date)
        return eventList[a].date < eventList[b].date;
    return eventList[a].startTime < eventList[b].startTime;
}

void heapifyUp(PriorityQueue* pq, int i) {
    while (i > 0) {
        int parent = (i-1)/2;
        if (compareEvents(pq->indices[i], pq->indices[parent])) {
            swap(&pq->indices[parent], &pq->indices[i]);
            i = parent;
        } else break;
    }
}

void heapifyDown(PriorityQueue* pq, int i) {
    int best = i, left = 2*i+1, right = 2*i+2;
    if (left  < pq->size && compareEvents(pq->indices[left],  pq->indices[best])) best = left;
    if (right < pq->size && compareEvents(pq->indices[right], pq->indices[best])) best = right;
    if (best != i) {
        swap(&pq->indices[best], &pq->indices[i]);
        heapifyDown(pq, best);
    }
}

void insertIntoPQ(PriorityQueue* pq, int index) {
    if (pq->size >= MAX_EVENTS) { printf(RED "  Priority Queue Full!\n" RESET); return; }
    pq->indices[pq->size++] = index;
    heapifyUp(pq, pq->size - 1);
}

int priorityOrder(PriorityQueue* pq) {
    if (pq->size == 0) return -1;
    int top = pq->indices[0];
    pq->indices[0] = pq->indices[--pq->size];
    heapifyDown(pq, 0);
    return top;
}

void rebuildHeap(PriorityQueue* pq) {
    for (int i = pq->size/2 - 1; i >= 0; i--)
        heapifyDown(pq, i);
}

/* ================================================ EVENT FUNCTIONS ================================================ */
/*
 * O(1) lookup via hash map — replaces the old O(n) linear scan.
 * Only returns the event if it is active (not cancelled, not done).
 */
Event* searchEventByID(int id) {
    int idx = hmGet(&eventMap, id);
    if (idx == -1) return NULL;
    Event* e = &eventList[idx];
    if (e->isCancelled || e->isDone) return NULL;
    return e;
}

void searchEventByName(char* keyword) {
    int found = 0;
    printf("\n" CYAN "+--------+------------------------------------------+------+------------+-------------+---------------------------+----------+" RESET "\n");
    printf(CYAN "| %-6s | %-40s | %-3s | %-10s | %-11s | %-25s | %-8s |" RESET "\n","ID","Event Name","Pri","Date","Time","Resource","Slots");
    printf(CYAN "+--------+------------------------------------------+------+------------+-------------+---------------------------+----------+" RESET "\n");
    for (int i = 0; i < totalEvents; i++) {
        if (partialMatch(eventList[i].name, keyword)) {
            printEventRow(&eventList[i]);
            found = 1;
        }
    }
    printf(CYAN "+--------+------------------------------------------+------+------------+-------------+---------------------------+----------+" RESET "\n");
    if (!found) printf(RED "  No events found matching \"%s\".\n" RESET, keyword);
}

int checkTimeConflict(int start, int end, int resourceID, int ignoreEventID, int date) {
    for (int i = 0; i < totalEvents; i++) {
        if (eventList[i].isCancelled || eventList[i].isDone) continue;
        if (eventList[i].resourceID == resourceID &&
            eventList[i].eventID != ignoreEventID &&
            eventList[i].date == date)
            if (eventList[i].startTime < end && start < eventList[i].endTime)
                return 1;
    }
    return 0;
}

int eventIDExists(int id) {
    return (hmGet(&eventMap, id) != -1);
}

int generateEventID() {
    int id;
    do { id = rand() % 900000 + 100000; } while (eventIDExists(id));
    return id;
}

int generateResourceID() {
    int id;
    do { id = rand() % 9000 + 1000; } while (searchResource(root, id) != NULL);
    return id;
}

void printEventTableHeader() {
    printf("\n" CYAN "+--------+------------------------------------------+------+------------+-------------+---------------------------+----------+" RESET "\n");
    printf(CYAN "| %-6s | %-40s | %-3s | %-10s | %-11s | %-25s | %-8s |" RESET "\n",
           "ID","Event Name","Pri ","Date","Time","Resource","Slots");
    printf(CYAN "+--------+------------------------------------------+------+------------+-------------+---------------------------+----------+" RESET "\n");
}

void printEventTableFooter() {
    printf(CYAN "+--------+------------------------------------------+-----+------------+-------------+---------------------------+----------+" RESET "\n");
}

void printEventRow(Event* e) {
    Resource* r = searchResource(root, e->resourceID);
    char timeStr[12], slots[10];
    snprintf(timeStr, sizeof(timeStr), "%02d:%02d-%02d:%02d",
             e->startTime/100, e->startTime%100, e->endTime/100, e->endTime%100);
    snprintf(slots, sizeof(slots), "%d/%d", e->participantCount, e->maxParticipants);
    printf("| " YELLOW "%-6d" RESET " | %-40.40s | %s%-3s" RESET " | ",
           e->eventID, e->name, priorityColor(e->priority), priorityText(e->priority));
    printDate(e->date);
    printf(" | %-11s | %-25.25s | %-8s |\n", timeStr, r ? r->name : "Unknown", slots);
}

void addEvent() {
    if (!root) {
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
    if (e->priority < 1 || e->priority > 3) {
        printf(RED "  Invalid priority!\n" RESET);
        return;
    }

    printf("Enter Event Date (YYYYMMDD): ");
    scanf("%d", &e->date);
    if (!validDate(e->date)) {
        printf(RED "  Invalid date!\n" RESET);
        return;
    }

    printf("Enter Start Time (e.g., 1300): ");
    scanf("%d", &e->startTime);
    printf("Enter End Time: ");
    scanf("%d", &e->endTime);
    if (!validTime(e->startTime)) {
        printf(RED "  Invalid Start Time!\n" RESET);
        return;
    }
    if (!validTime(e->endTime)) {
        printf(RED "  Invalid End Time!\n" RESET);
        return;
    }
    if (e->startTime >= e->endTime) {
        printf(RED "  Invalid time range!\n" RESET);
        return;
    }

    printf("\n" CYAN "+--------+------------------------------------+" RESET "\n");
    printf(CYAN "|   AVAILABLE RESOURCES FOR SELECTED TIME    |" RESET "\n");
    printf(CYAN "+--------+------------------------------------+" RESET "\n");
    showAvailableResourcesRecursive(root, e->startTime, e->endTime, e->date);
    printf(CYAN "+--------+------------------------------------+" RESET "\n");

    printf("Enter Resource ID: "); scanf("%d", &e->resourceID);
    if (!searchResource(root, e->resourceID)) {
        printf(RED "  Resource does not exist!\n" RESET);
        return;
    }
    if (!isResourceAvailable(e->resourceID, e->startTime, e->endTime, e->date)) {
        printf(RED "  Resource not available in that time range!\n" RESET);
        return;
    }
    if (checkTimeConflict(e->startTime, e->endTime, e->resourceID, -1, e->date)) {
        printf(RED "  Time conflict detected!\n" RESET);
        return;
    }

    e->eventID = generateEventID();
    printf(GREEN "  Generated Event ID: %d\n" RESET, e->eventID);

    printf("Enter Maximum Participants: ");
    scanf("%d", &e->maxParticipants);
    if (e->maxParticipants <= 0) {
        printf(RED "  Invalid participant limit!\n" RESET);
        return;
    }

    e->participantHead=NULL;
    e->participantCount=0;
    e->isCancelled=0;
    e->isDone=0;
    e->participantCounter=1;

    /* Register in hash map BEFORE incrementing totalEvents */
    hmInsert(&eventMap, e->eventID, totalEvents);
    insertIntoPQ(&pq, totalEvents);
    totalEvents++;

    printf(GREEN "  Event Added & Scheduled Successfully!\n" RESET);
}

void searchEventMenu() {
    int searchType;
    printf("Search by:\n  1. Event ID\n  2. Event Name\nEnter choice: ");
    scanf("%d", &searchType);

    if (searchType == 1) {
        int id; printf("Enter Event ID: "); scanf("%d", &id);

        /* Use hash map for O(1) lookup — includes cancelled/done events */
        int idx = hmGet(&eventMap, id);
        if (idx == -1) { printf(RED "  Event not found!\n" RESET); return; }

        Event* e = &eventList[idx];
        Resource* r = searchResource(root, e->resourceID);
        const char* statusColor = e->isDone ? GREEN : (e->isCancelled ? RED : YELLOW);
        const char* statusText  = e->isDone ? "DONE" : (e->isCancelled ? "CANCELLED" : "ACTIVE");

        printf("\n" CYAN "+----------------------------------------------+" RESET "\n");
        printf(CYAN "|              EVENT DETAILS                   |" RESET "\n");
        printf(CYAN "+----------------------------------------------+" RESET "\n");
        printf(CYAN "|" RESET " Name     : %-34.34s " CYAN "|" RESET "\n", e->name);
        printf(CYAN "|" RESET " Priority : %s%-34s" RESET CYAN "|" RESET "\n",
               priorityColor(e->priority), priorityText(e->priority));
        printf(CYAN "|" RESET " Date     : "); printDate(e->date);
        printf("                           " CYAN "|" RESET "\n");
        printf(CYAN "|" RESET " Time     : "); printTime(e->startTime);
        printf(" - "); printTime(e->endTime);
        printf("                        " CYAN "|" RESET "\n");
        printf(CYAN "|" RESET " Resource : %-34.34s " CYAN "|" RESET "\n", r ? r->name : "Unknown");
        printf(CYAN "|" RESET " Status   : %s%-34s" RESET CYAN "|" RESET "\n", statusColor, statusText);
        printf(CYAN "+----------------------------------------------+" RESET "\n");
    } else if (searchType == 2) {
        char keyword[150];
        printf("Enter Event Name: "); scanf(" %149[^\n]", keyword);
        searchEventByName(keyword);
    } else {
        printf(RED "  Invalid choice!\n" RESET);
    }
}

void updateEvent(int id) {
    /* O(1) via hash map */
    Event* e = searchEventByID(id);
    if (!e) { printf(RED "  Event not found or not active!\n" RESET); return; }

    char newName[150];
    int newPriority, newStart, newEnd, newDate;

    printf("Enter New Event Name: ");
    scanf(" %[^\n]", newName);
    printf("Enter New Priority (1=High, 2=Medium, 3=Low): ");
    scanf("%d", &newPriority);
    printf("Enter New Date (YYYYMMDD): ");
    scanf("%d", &newDate);
    printf("Enter New Start Time: ");
    scanf("%d", &newStart);
    printf("Enter New End Time: ");
    scanf("%d", &newEnd);

    if (newPriority < 1 || newPriority > 3) {
        printf(RED "  Invalid priority!\n" RESET);
        return;
    }
    if (!validDate(newDate))  {
        printf(RED "  Invalid date!\n" RESET);
        return;
    }
    if (!validTime(newStart) || !validTime(newEnd)) {
        printf(RED "  Invalid time!\n" RESET);
        return;
    }
    if (newStart >= newEnd)   {
        printf(RED "  Invalid time range!\n" RESET);
        return;
    }
    if (checkTimeConflict(newStart, newEnd, e->resourceID, e->eventID, newDate)) {
        printf(RED "  Time conflict detected!\n" RESET);
        return;
    }

    strcpy(e->name, newName);
    e->priority = newPriority;
    e->date = newDate;
    e->startTime = newStart;
    e->endTime = newEnd;

    rebuildHeap(&pq);
    printf(GREEN "  Event Updated Successfully!\n" RESET);
}

void cancelEvent(int id) {
    int idx = hmGet(&eventMap, id);
    if (idx == -1) { printf(RED "  Event not found!\n" RESET); return; }

    Event* e = &eventList[idx];
    if (e->isDone) {
        printf(YELLOW "  Event already completed!\n"  RESET);
        return;
    }
    if (e->isCancelled) {
        printf(YELLOW "  Event already cancelled!\n"  RESET);
        return;
    }

    e->isCancelled = 1;
    rebuildHeap(&pq);
    printf(GREEN "  Event Cancelled Successfully! Participants preserved.\n" RESET);
}

void displayAllEvents() {
    int count = 0;
    for (int i = 0; i < totalEvents; i++)
        if (!eventList[i].isCancelled && !eventList[i].isDone) count++;
    if (!count) {
        printf(YELLOW "  No active events available.\n" RESET);
        return;
    }

    int sortChoice;
    printf("Sort by:\n  1. Priority\n  2. Date & Time\nEnter choice: ");
    scanf("%d", &sortChoice);

    if (sortChoice == 1) {
        PriorityQueue temp = pq;
        printf("\n" CYAN BOLD "  EVENTS — PRIORITY ORDER" RESET "\n");
        printEventTableHeader();
        while (temp.size > 0) {
            int idx = priorityOrder(&temp);
            if (idx == -1) break;
            if (!eventList[idx].isCancelled && !eventList[idx].isDone)
                printEventRow(&eventList[idx]);
        }
        printEventTableFooter();

    } else if (sortChoice == 2) {
        int active[MAX_EVENTS], k = 0;
        for (int i = 0; i < totalEvents; i++)
            if (!eventList[i].isCancelled && !eventList[i].isDone) active[k++] = i;
        for (int i = 0; i < k-1; i++)
            for (int j = i+1; j < k; j++)
                if (eventList[active[i]].date > eventList[active[j]].date ||
                   (eventList[active[i]].date == eventList[active[j]].date &&
                    eventList[active[i]].startTime > eventList[active[j]].startTime)) {
                    int t = active[i]; active[i] = active[j]; active[j] = t;
                }
        printf("\n" CYAN BOLD "  EVENTS — DATE/TIME ORDER" RESET "\n");
        printEventTableHeader();
        for (int i = 0; i < k; i++) printEventRow(&eventList[active[i]]);
        printEventTableFooter();
    } else {
        printf(RED "  Invalid choice!\n" RESET);
    }
}

void displayEventsByDate() {
    int date;
    printf("Enter Date (YYYYMMDD): ");
    scanf("%d", &date);
    if (!validDate(date)) {
        printf(RED "  Invalid date!\n" RESET);
        return;
    }

    int matches[MAX_EVENTS], count = 0;
    for (int i = 0; i < totalEvents; i++)
        if (!eventList[i].isCancelled && !eventList[i].isDone && eventList[i].date == date)
            matches[count++] = i;
    if (!count) { printf(YELLOW "  No active events found for this date.\n" RESET); return; }

    int sortChoice;
    printf("Sort by:\n  1. Priority\n  2. Time\nEnter choice: ");
    scanf("%d", &sortChoice);

    if (sortChoice == 1) {
        for (int i = 0; i < count-1; i++)
            for (int j = i+1; j < count; j++)
                if (compareEvents(matches[j], matches[i]))
                    { int t = matches[i]; matches[i] = matches[j]; matches[j] = t; }
        printf("\n" CYAN BOLD "  EVENTS ON ");
        printDate(date);
        printf(" — PRIORITY ORDER" RESET "\n");
    } else if (sortChoice == 2) {
        for (int i = 0; i < count-1; i++)
            for (int j = i+1; j < count; j++)
                if (eventList[matches[i]].startTime > eventList[matches[j]].startTime)
                    { int t = matches[i]; matches[i] = matches[j]; matches[j] = t; }
        printf("\n" CYAN BOLD "  EVENTS ON ");
        printDate(date);
        printf(" — TIME ORDER" RESET "\n");
    } else {
        printf(RED "  Invalid choice!\n" RESET);
        return;
    }
    printEventTableHeader();
    for (int i = 0; i < count; i++) printEventRow(&eventList[matches[i]]);
    printEventTableFooter();
}

void displayDoneEvents() {
    int found = 0;
    printf("\n" CYAN "+--------+------------------------------------------+------------+--------+" RESET "\n");
    printf(CYAN "| %-6s | %-40s | %-10s | %-6s |" RESET "\n","ID","Event Name","Date","People");
    printf(CYAN "+--------+------------------------------------------+------------+--------+" RESET "\n");
    for (int i = 0; i < totalEvents; i++) {
        if (eventList[i].isDone) {
            printf("| " GREEN "%-6d" RESET " | %-40.40s | ", eventList[i].eventID, eventList[i].name);
            printDate(eventList[i].date);
            printf(" | %-6d |\n", eventList[i].participantCount);
            found = 1;
        }
    }
    if (!found) printf(CYAN "|" YELLOW "  No completed events.                                                    " CYAN "|" RESET "\n");
    printf(CYAN "+--------+------------------------------------------+------------+--------+" RESET "\n");
}

void displayCancelledEvents() {
    int found = 0;
    printf("\n" CYAN "+--------+------------------------------------------+------------+--------+" RESET "\n");
    printf(CYAN "| %-6s | %-40s | %-10s | %-6s |" RESET "\n","ID","Event Name","Date","People");
    printf(CYAN "+--------+------------------------------------------+------------+--------+" RESET "\n");
    for (int i = 0; i < totalEvents; i++) {
        if (eventList[i].isCancelled) {
            printf("| " RED "%-6d" RESET " | %-40.40s | ", eventList[i].eventID, eventList[i].name);
            printDate(eventList[i].date);
            printf(" | %-6d |\n", eventList[i].participantCount);
            found = 1;
        }
    }
    if (!found) printf(CYAN "|" YELLOW "  No cancelled events.                                                    " CYAN "|" RESET "\n");
    printf(CYAN "+--------+------------------------------------------+------------+--------+" RESET "\n");
}

/* ================================================ PARTICIPANT FUNCTIONS ================================================ */

void registerParticipant(int eventID) {
    Event* e = searchEventByID(eventID);   /* O(1) */
    if (!e) { printf(RED "  Active event not found!\n" RESET); return; }
    if (e->participantCount >= e->maxParticipants)
        { printf(RED "  Participant limit reached!\n" RESET); return; }

    int newID = e->participantCounter++;
    printf(GREEN "  Generated Participant ID: %d\n" RESET, newID);

    Participant* newP = (Participant*)malloc(sizeof(Participant));
    if (!newP) { printf(RED "  Memory allocation failed!\n" RESET); return; }
    newP->participantID = newID;
    printf("Enter Participant Name: "); scanf(" %99[^\n]", newP->name);
    newP->next = e->participantHead;
    e->participantHead = newP;
    e->participantCount++;
    printf(GREEN "  Participant Registered Successfully!\n" RESET);
}

void removeParticipant(int eventID, int participantID) {
    Event* e = searchEventByID(eventID);   /* O(1) */
    if (!e) { printf(RED "  Active event not found!\n" RESET); return; }

    Participant *temp = e->participantHead, *prev = NULL;
    while (temp) {
        if (temp->participantID == participantID) {
            if (!prev) e->participantHead = temp->next;
            else prev->next = temp->next;
            free(temp);
            e->participantCount--;
            printf(GREEN "  Participant removed successfully!\n" RESET);
            return;
        }
        prev = temp; temp = temp->next;
    }
    printf(RED "  Participant not found!\n" RESET);
}

void displayParticipants(int eventID) {
    int idx = hmGet(&eventMap, eventID);
    if (idx == -1) { printf(RED "  Event not found!\n" RESET); return; }

    Event* e = &eventList[idx];
    if (!e->participantHead) { printf(YELLOW "  No participants registered.\n" RESET); return; }

    const char* statusColor = e->isDone ? GREEN : (e->isCancelled ? RED : YELLOW);
    const char* statusText  = e->isDone ? "DONE" : (e->isCancelled ? "CANCELLED" : "ACTIVE");

    printf("\n" CYAN "+-------+----------------------------------------+" RESET "\n");
    printf(CYAN "|  PARTICIPANTS : %-30.30s   |" RESET "\n", e->name);
    printf(CYAN "|  Status       : %s%-10s" RESET CYAN "                            |" RESET "\n",
           statusColor, statusText);
    printf(CYAN "+-------+----------------------------------------+" RESET "\n");
    printf(CYAN "| %-5s | %-38s   |" RESET "\n","ID","Name");
    printf(CYAN "+-------+----------------------------------------+" RESET "\n");
    for (Participant* p = e->participantHead; p; p = p->next)
        printf("| " YELLOW "%03d" RESET "   | %-40.40s |\n", p->participantID, p->name);
    printf(CYAN "+-------+----------------------------------------+" RESET "\n");
}

void freeParticipants(Event* e) {
    Participant* temp = e->participantHead;
    while (temp) { Participant* nx = temp->next; free(temp); temp = nx; }
    e->participantHead  = NULL;
    e->participantCount = 0;
}

/* ================================================ PURGE ================================================ */

/*
 * After compaction, eventList[] indices shift.
 * We rebuild the hash map from scratch — simpler and guaranteed correct.
 */
void purgeHistory() {
    int doneCount = 0, cancelledCount = 0;
    for (int i = 0; i < totalEvents; i++) {
        if (eventList[i].isDone)      doneCount++;
        if (eventList[i].isCancelled) cancelledCount++;
    }
    if (!doneCount && !cancelledCount) {
        printf(YELLOW "  No completed or cancelled events to purge.\n" RESET); return;
    }
    printf(YELLOW "  This will permanently delete %d completed and %d cancelled event(s).\n" RESET,
           doneCount, cancelledCount);
    printf("  Are you sure? (y/n): ");
    char confirm; scanf(" %c", &confirm);
    if (confirm != 'y' && confirm != 'Y') { printf(YELLOW "  Purge cancelled.\n" RESET); return; }

    for (int i = 0; i < totalEvents; i++)
        if (eventList[i].isDone || eventList[i].isCancelled)
            freeParticipants(&eventList[i]);

    /* Compact array */
    int newTotal = 0;
    for (int i = 0; i < totalEvents; i++) {
        if (!eventList[i].isDone && !eventList[i].isCancelled) {
            if (i != newTotal) eventList[newTotal] = eventList[i];
            newTotal++;
        }
    }
    totalEvents = newTotal;

    /* Rebuild heap */
    pq.size = 0;
    for (int i = 0; i < totalEvents; i++) insertIntoPQ(&pq, i);

    /* Rebuild hash map — indices have shifted after compaction */
    hmInit(&eventMap);
    for (int i = 0; i < totalEvents; i++)
        hmInsert(&eventMap, eventList[i].eventID, i);

    printf(GREEN "  Purge complete. %d completed and %d cancelled event(s) removed.\n" RESET,
           doneCount, cancelledCount);
    printf(GREEN "  Heap and hash map rebuilt with %d active event(s).\n" RESET, totalEvents);
}

/* ================================================ SYSTEM SUMMARY ================================================ */

void systemSummary() {
    int active = 0, cancelled = 0, done = 0, totalParticipants = 0, eventsInQueue = 0;
    for (int i = 0; i < totalEvents; i++) {
        if (eventList[i].isCancelled) cancelled++;
        else if (eventList[i].isDone) done++;
        else active++;
        totalParticipants += eventList[i].participantCount;
    }
    for (int i = 0; i < pq.size; i++) {
        int idx = pq.indices[i];
        if (!eventList[idx].isCancelled && !eventList[idx].isDone) eventsInQueue++;
    }
    int nowDate, nowTime;
    getCurrentDateTime(&nowDate, &nowTime);

    printf("\n" CYAN "+----------------------------------+" RESET "\n");
    printf(CYAN "|         SYSTEM SUMMARY           |" RESET "\n");
    printf(CYAN "+----------------------------------+" RESET "\n");
    printf(CYAN "|" RESET " Current Date/Time      : " YELLOW "%04d-%02d-%02d %02d:%02d" RESET CYAN " |" RESET "\n",
           nowDate/10000,(nowDate/100)%100,nowDate%100,nowTime/100,nowTime%100);
    printf(CYAN "|" RESET " Total Events Created   : " YELLOW "%-6d" RESET CYAN " |" RESET "\n", totalEvents);
    printf(CYAN "|" RESET " Active Events          : " GREEN  "%-6d" RESET CYAN " |" RESET "\n", active);
    printf(CYAN "|" RESET " Completed Events       : " GREEN  "%-6d" RESET CYAN " |" RESET "\n", done);
    printf(CYAN "|" RESET " Cancelled Events       : " RED    "%-6d" RESET CYAN " |" RESET "\n", cancelled);
    printf(CYAN "|" RESET " Events in Queue        : " YELLOW "%-6d" RESET CYAN " |" RESET "\n", eventsInQueue);
    printf(CYAN "|" RESET " Total Participants     : " YELLOW "%-6d" RESET CYAN " |" RESET "\n", totalParticipants);
    printf(CYAN "|" RESET " Total Resources        : " YELLOW "%-6d" RESET CYAN " |" RESET "\n", countResources(root));
    printf(CYAN "+----------------------------------+" RESET "\n");
}

/* ================================================ FILE I/O ================================================ */

void saveResourcesRecursive(Resource* node, FILE* fp) {
    if (!node) return;
    saveResourcesRecursive(node->left, fp);
    fprintf(fp, "%d|%s\n", node->resourceID, node->name);
    saveResourcesRecursive(node->right, fp);
}

void saveResourcesToFile() {
    FILE* fp = fopen("resources.txt","w");
    if (!fp) { printf(RED "  Error saving resources!\n" RESET); return; }
    saveResourcesRecursive(root, fp);
    fclose(fp);
}

void loadResourcesFromFile() {
    FILE* fp = fopen("resources.txt","r");
    if (!fp) return;
    int id; char name[80];
    while (fscanf(fp, "%d|%79[^\n]\n", &id, name) != EOF)
        root = insertResource(root, id, name);
    fclose(fp);
}

void saveEventsToFile() {
    FILE* fp = fopen("events.txt","w");
    if (!fp) { printf(RED "  Error saving events!\n" RESET); return; }
    fprintf(fp, "%d\n", totalEvents);
    for (int i = 0; i < totalEvents; i++) {
        Event* e = &eventList[i];
        fprintf(fp, "%d|%s|%d|%d|%d|%d|%d|%d|%d|%d|%d\n",e->eventID, e->name, e->priority,
                e->date, e->startTime, e->endTime, e->resourceID,
                e->isCancelled, e->isDone, e->maxParticipants, e->participantCounter);
    }
    fclose(fp);
}

void loadEventsFromFile() {
    FILE* fp = fopen("events.txt","r");
    if (!fp) return;
    fscanf(fp, "%d\n", &totalEvents);
    if (totalEvents > MAX_EVENTS) totalEvents = MAX_EVENTS;
    for (int i = 0; i < totalEvents; i++) {
        Event* e = &eventList[i];
        int counter = 1;
        fscanf(fp, "%d|%149[^|]|%d|%d|%d|%d|%d|%d|%d|%d|%d\n",
               &e->eventID, e->name, &e->priority,
               &e->date, &e->startTime, &e->endTime, &e->resourceID,
               &e->isCancelled, &e->isDone, &e->maxParticipants, &counter);
        e->participantHead    = NULL;
        e->participantCount   = 0;
        e->participantCounter = counter;

        /* Register every event (active or not) in the hash map */
        hmInsert(&eventMap, e->eventID, i);

        if (!e->isCancelled && !e->isDone)
            insertIntoPQ(&pq, i);
    }
    fclose(fp);
}

void saveParticipantsToFile() {
    FILE* fp = fopen("participants.txt","w");
    if (!fp) { printf(RED "  Error saving participants!\n" RESET); return; }
    for (int i = 0; i < totalEvents; i++) {
        for (Participant* p = eventList[i].participantHead; p; p = p->next)
            fprintf(fp, "%d | %d | %s\n", eventList[i].eventID, p->participantID, p->name);
    }
    fclose(fp);
}

void loadParticipantsFromFile() {
    FILE* fp = fopen("participants.txt","r");
    if (!fp) return;
    int eventID, pid; char name[100];
    while (fscanf(fp, "%d | %d | %99[^\n]\n", &eventID, &pid, name) != EOF) {
        int idx = hmGet(&eventMap, eventID);   /* O(1) lookup */
        if (idx == -1) continue;
        Event* e = &eventList[idx];
        Participant* newP = malloc(sizeof(Participant));
        if (!newP) return;
        newP->participantID = pid;
        strcpy(newP->name, name);
        newP->next = e->participantHead;
        e->participantHead = newP;
        e->participantCount++;
        if (pid >= e->participantCounter) e->participantCounter = pid + 1;
    }
    fclose(fp);
}

/* ================================================ MAIN MENU ================================================ */

void menu() {
    printNextEventBanner();
    printf(CYAN "|==========================================|" RESET "\n");
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
    printf(CYAN "|" RESET "  14. Show Completed Events               " CYAN "|" RESET "\n");
    printf(CYAN "|" RESET "  15. Show Cancelled Events               " CYAN "|" RESET "\n");
    printf(CYAN "|" RESET "  16. System Summary                      " CYAN "|" RESET "\n");
    printf(CYAN "|" YELLOW "  17. Purge Completed & Cancelled Events  " CYAN "|" RESET "\n");
    printf(CYAN "|" RED   "  18. Exit                                " CYAN "|" RESET "\n");
    printf(CYAN "+==========================================+" RESET "\n");
    printf("  Enter choice: ");
}

int main() {
    pq.size = 0;
    root    = NULL;
    hmInit(&eventMap);

    loadResourcesFromFile();
    loadEventsFromFile();
    loadParticipantsFromFile();

    srand(time(NULL));

    int choice, id;
    char name[80];

    while (1) {
        checkAndAutoComplete();
        menu();
        scanf("%d", &choice);

        switch (choice) {
        case 1:  addEvent();
                pause();
                break;

        case 2:
            printf("Enter Event ID: ");
            scanf("%d", &id);
            registerParticipant(id);
            pause();
            break;

        case 3:  displayAllEvents();
        pause(); break;
        case 4:  displayEventsByDate();
        pause(); break;

        case 5:
            printf("Enter Event ID to Display Participants: ");
            scanf("%d", &id);
            displayParticipants(id);
            pause(); break;

        case 6: {
            int eventID, participantID;
            printf("Enter Event ID: ");
            scanf("%d", &eventID);
            printf("Enter Participant ID to remove: ");
            scanf("%d", &participantID);
            removeParticipant(eventID, participantID);
            pause();
            break;
        }

        case 7:
            printf("Enter Event ID to Update: ");
            scanf("%d", &id);
            updateEvent(id);
            pause();
            break;

        case 8:  searchEventMenu();
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
            printf("Enter Resource Name: "); scanf(" %[^\n]", name);
            root = insertResource(root, id, name);
            printf(GREEN "  Resource Added!\n" RESET);
            pause(); break;

        case 11: displayResources(); pause(); break;

        case 12:
            printf("Enter Resource Name: ");
            scanf(" %[^\n]", name);
            if (!searchResourceByName(root, name))
                printf(RED "  Resource not found!\n" RESET);
            pause(); break;

        case 13: showAvailableResources();
            pause(); break;
        case 14: displayDoneEvents();
            pause(); break;
        case 15: displayCancelledEvents();
            pause(); break;
        case 16: systemSummary();
            pause(); break;
        case 17: purgeHistory();
            pause(); break;

        case 18:
            printf(YELLOW "  Saving data...\n" RESET);
            saveResourcesToFile();
            saveEventsToFile();
            saveParticipantsToFile();
            for (int i = 0; i < totalEvents; i++) freeParticipants(&eventList[i]);
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