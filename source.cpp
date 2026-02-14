#include <iostream>
#include <fstream>
#include <string>
#include <thread>
#include <mutex>
#include <chrono>
#include <ctime>
#include <climits>
#include <cmath>
#include <iomanip>

using namespace std;

// =========================================================
// UI HELPERS (COLORS)
// =========================================================
namespace Color {
    const string RESET  = "\033[0m";
    const string RED    = "\033[31m";
    const string GREEN  = "\033[32m";
    const string YELLOW = "\033[33m";
    const string BLUE   = "\033[34m";
    const string MAGENTA= "\033[35m";
    const string CYAN   = "\033[36m";
    const string BOLD   = "\033[1m";
}

// =========================================================
// 1. CONFIGURATION & CONSTANTS
// =========================================================
const int MAX_CITIES = 8;
const int SECONDS_PER_DAY = 180; 
const int SECONDS_PER_NODE = 2;

const string CITIES[MAX_CITIES] = {
    "Lahore", "Karachi", "Islamabad", "Multan",
    "Faisalabad", "Peshawar", "Quetta", "Sialkot"
};

const string OFFICES[6] = {"Hub", "Office-1", "Office-2", "Office-3", "Office-4", "Office-5"};

const int DIST_MATRIX[8][8] = {
    {0, 15, 8, 6, 4, 10, 14, 3},
    {15, 0, 12, 10, 13, 14, 6, 14},
    {8, 12, 0, 6, 7, 4, 13, 10},
    {6, 10, 6, 0, 5, 9, 12, 7},
    {4, 13, 7, 5, 0, 8, 14, 5},
    {10, 14, 4, 9, 8, 0, 13, 11},
    {14, 6, 13, 12, 14, 13, 0, 15},
    {3, 14, 10, 7, 5, 11, 15, 0}
};

mutex dataMutex;
mutex fileMutex;

// =========================================================
// 2. CUSTOM DATA STRUCTURES (NO STL)
// =========================================================

template <typename T>
class ListNode {
public:
    T data;
    ListNode* next;
    ListNode(T val) : data(val), next(nullptr) {}
};

template <typename T>
class LinkedList {
public:
    ListNode<T>* head;
    ListNode<T>* tail;
    int size;

    LinkedList() { head = tail = nullptr; size = 0; }
    
    void append(T val) {
        ListNode<T>* newNode = new ListNode<T>(val);
        if (!head) {
            head = tail = newNode;
        } else {
            tail->next = newNode;
            tail = newNode;
        }
        size++;
    }

    void clear() {
        while(head) {
            ListNode<T>* temp = head;
            head = head->next;
            delete temp;
        }
        tail = nullptr;
        size = 0;
    }
    
    bool isEmpty() { return size == 0; }
};

struct HeapNode {
    int v;
    int dist;
};

class MinHeap {
    HeapNode* array;
    int capacity;
    int size;
    int* pos; 

public:
    MinHeap(int cap) {
        size = 0;
        capacity = cap;
        array = new HeapNode[cap];
        pos = new int[cap];
    }

    void swapNodes(int a, int b) {
        HeapNode t = array[a];
        array[a] = array[b];
        array[b] = t;
        pos[array[a].v] = a;
        pos[array[b].v] = b;
    }

    void minHeapify(int idx) {
        int smallest = idx;
        int left = 2 * idx + 1;
        int right = 2 * idx + 2;

        if (left < size && array[left].dist < array[smallest].dist)
            smallest = left;
        if (right < size && array[right].dist < array[smallest].dist)
            smallest = right;

        if (smallest != idx) {
            swapNodes(idx, smallest);
            minHeapify(smallest);
        }
    }

    bool isEmpty() { return size == 0; }

    HeapNode extractMin() {
        if (isEmpty()) return {-1, -1};
        HeapNode root = array[0];
        HeapNode last = array[size - 1];
        array[0] = last;
        pos[root.v] = size - 1;
        pos[last.v] = 0;
        size--;
        minHeapify(0);
        return root;
    }

    void decreaseKey(int v, int dist) {
        int i = pos[v];
        array[i].dist = dist;
        while (i && array[(i - 1) / 2].dist > array[i].dist) {
            swapNodes(i, (i - 1) / 2);
            i = (i - 1) / 2;
        }
    }

    bool isInMinHeap(int v) {
        return pos[v] < size;
    }

    void insert(int v, int dist) {
        array[size] = {v, dist};
        pos[v] = size;
        size++;
    }
};

// =========================================================
// 3. CORE CLASSES
// =========================================================

class Parcel {
public:
    string id;
    int srcCity, srcOffice;
    int destCity, destOffice;
    int weight;
    int priority; // 1=Overnight, 2=2Day, 3=Normal
    string status;
    int bookingDay;
    long long dispatchTime; 
    int totalRouteDistance; 
    
    Parcel(string i, int sC, int sO, int dC, int dO, int w, int p, int d) 
        : id(i), srcCity(sC), srcOffice(sO), destCity(dC), destOffice(dO), 
          weight(w), priority(p), bookingDay(d) {
        status = "Booked";
        dispatchTime = 0;
        totalRouteDistance = 0;
    }
};

// --- DATA STRUCTURE: HASH TABLE FOR O(1) LOOKUP ---
class ParcelHashTable {
    static const int TABLE_SIZE = 1009; // Prime number for better distribution
    LinkedList<Parcel*>* table[TABLE_SIZE];

   unsigned long hash(string key) {
        unsigned long hash = 5381;
        for (char c : key) {
            hash = ((hash << 5) + hash) + c; /* hash * 33 + c */
        }
        return hash % TABLE_SIZE;
    }

public:
    ParcelHashTable() {
        for(int i=0; i<TABLE_SIZE; i++) table[i] = nullptr;
    }

    void insert(Parcel* p) {
        unsigned long idx = hash(p->id);
        if(table[idx] == nullptr) {
            table[idx] = new LinkedList<Parcel*>();
        }
        // Collision Handling: Chaining via LinkedList
        table[idx]->append(p);
    }

    Parcel* search(string id) {
        unsigned long idx = hash(id);
        if(table[idx] == nullptr) return nullptr;

        ListNode<Parcel*>* curr = table[idx]->head;
        while(curr) {
            if(curr->data->id == id) return curr->data;
            curr = curr->next;
        }
        return nullptr;
    }
};

class Graph {
    struct AdjListNode {
        int dest;
        int weight;
        AdjListNode* next;
        AdjListNode(int d, int w) : dest(d), weight(w), next(nullptr) {}
    };
    AdjListNode* adj[MAX_CITIES];

public:
    Graph() {
        for(int i=0; i<MAX_CITIES; i++) adj[i] = nullptr;
        for(int i=0; i<MAX_CITIES; i++) {
            for(int j=0; j<MAX_CITIES; j++) {
                if(i!=j && DIST_MATRIX[i][j] > 0) {
                    AdjListNode* node = new AdjListNode(j, DIST_MATRIX[i][j]);
                    node->next = adj[i];
                    adj[i] = node;
                }
            }
        }
    }

    int getShortestPath(int src, int dest) {
        int dist[MAX_CITIES];
        MinHeap minHeap(MAX_CITIES);

        for (int v = 0; v < MAX_CITIES; ++v) {
            dist[v] = INT_MAX;
            minHeap.insert(v, dist[v]);
        }

        minHeap.decreaseKey(src, 0);
        dist[src] = 0;

        while (!minHeap.isEmpty()) {
            HeapNode minNode = minHeap.extractMin();
            int u = minNode.v;

            AdjListNode* crawl = adj[u];
            while (crawl) {
                int v = crawl->dest;
                if (minHeap.isInMinHeap(v) && dist[u] != INT_MAX && 
                    crawl->weight + dist[u] < dist[v]) {
                    dist[v] = dist[u] + crawl->weight;
                    minHeap.decreaseKey(v, dist[v]);
                }
                crawl = crawl->next;
            }
        }
        return (dist[dest] == INT_MAX) ? -1 : dist[dest];
    }
};

class Trip {
public:
    int src, dest;
    string vehicleType;
    int distance; 
    long long startTime; 
    bool isFinished;
    LinkedList<Parcel*> parcels;

    Trip(int s, int d, string v, int dist, long long time)
        : src(s), dest(d), vehicleType(v), distance(dist), startTime(time) {
        isFinished = false;
    }
};

// =========================================================
// 4. ENGINE CLASS (The Brain)
// =========================================================
class Engine {
    Graph graph;
    LinkedList<Parcel*> allParcels; // Main list for iteration
    ParcelHashTable parcelMap;      // O(1) Lookup for Tracking/Undo
    LinkedList<Trip*> activeTrips;  
    
    int bus300[MAX_CITIES];
    int bus600[MAX_CITIES];
    int truck2000[MAX_CITIES];

    int day;
    int second; 
    long long totalSeconds;
    int totalLost;
    bool running;
    
public:
    Engine() {
        day = 1;
        second = 0;
        totalSeconds = 0;
        totalLost = 0;
        running = true;
        resetVehicles();
        ofstream f("notifications.txt", ios::trunc);
        f.close();
    }

    void resetVehicles() {
        for(int i=0; i<MAX_CITIES; i++) {
            bus300[i] = 14;
            bus600[i] = 14;
            truck2000[i] = 7;
        }
    }

    void logSystemEvent(int d, int t, string city, string type, string msg) {
        lock_guard<mutex> lock(fileMutex);
        ofstream f("notifications.txt", ios::app);
        f << "[" << d << "][" << t << "] [" << city << "] " << type << ": " << msg << endl;
        f.close();
    }

    // --- Customer Functions (Styled) ---
    void bookParcel(int sC, int sO, int dC, int dO, int w, int p) {
        lock_guard<mutex> lock(dataMutex);
        
        if(sC == dC && sO == dO) {
            cout << Color::RED << "\n[!] ERROR: Source and Destination cannot be the same office.\n" << Color::RESET;
            return;
        }

        string id = "P-" + to_string(rand()%100000);
        Parcel* newP = new Parcel(id, sC, sO, dC, dO, w, p, day);
        
        // Add to both Master List and Hash Table
        allParcels.append(newP);
        parcelMap.insert(newP);

        cout << Color::GREEN << "\n[SUCCESS] Parcel Booked Successfully! Tracking ID: " << Color::BOLD << id << Color::RESET << endl;
        logSystemEvent(day, second, CITIES[sC], "BOOKING", "Customer booked parcel " + id + " to " + CITIES[dC]);
    }

    void undoParcel(string id) {
        lock_guard<mutex> lock(dataMutex);
        
        // O(1) Search via Hash Table
        Parcel* p = parcelMap.search(id);

        if(p) {
            if(p->status == "Booked") {
                p->status = "Cancelled";
                cout << Color::GREEN << "[SUCCESS] Parcel " << id << " has been cancelled.\n" << Color::RESET;
                logSystemEvent(day, second, CITIES[p->srcCity], "UNDO", "Parcel " + id + " cancelled by user.");
            } else {
                cout << Color::RED << "[ERROR] Cannot Undo. Parcel is already " << p->status << ".\n" << Color::RESET;
            }
            return;
        }
        cout << Color::RED << "[ERROR] Parcel ID not found in system.\n" << Color::RESET;
    }

    void trackParcel(string id) {
        lock_guard<mutex> lock(dataMutex);
        
        // O(1) Search via Hash Table
        Parcel* p = parcelMap.search(id);

        if(p) {
            cout << Color::CYAN << "\n+------------------------------------------------+\n";
            cout << "|               TRACKING DETAILS                 |\n";
            cout << "+------------------------------------------------+\n" << Color::RESET;
            cout << " ID:       " << Color::BOLD << p->id << Color::RESET << "\n";
            cout << " From:     " << CITIES[p->srcCity] << " (" << OFFICES[p->srcOffice] << ")\n";
            cout << " To:       " << CITIES[p->destCity] << " (" << OFFICES[p->destOffice] << ")\n";
            
            string sColor = Color::YELLOW;
            if(p->status == "Delivered") sColor = Color::GREEN;
            if(p->status == "LOST") sColor = Color::RED;
            if(p->status == "Cancelled") sColor = Color::RED;

            cout << " Status:   " << sColor << Color::BOLD << p->status << Color::RESET << "\n";
            
            if(p->status == "In Transit") {
                long long elapsed = totalSeconds - p->dispatchTime;
                double traveledKm = elapsed / (double)SECONDS_PER_NODE;
                if (traveledKm > p->totalRouteDistance) traveledKm = p->totalRouteDistance;
                
                cout << " Progress: " << Color::CYAN;
                int barWidth = 20;
                float progress = (float)traveledKm / (float)p->totalRouteDistance;
                int pos = barWidth * progress;
                cout << "[";
                for (int i = 0; i < barWidth; ++i) {
                    if (i < pos) cout << "=";
                    else if (i == pos) cout << ">";
                    else cout << " ";
                }
                cout << "] " << int(progress * 100.0) << "%\n" << Color::RESET;
                
                cout << fixed << setprecision(1);
                cout << " Traveled: " << traveledKm << " km\n";
                cout << " Remaining:" << (p->totalRouteDistance - traveledKm) << " km\n";
            }
            cout << Color::CYAN << "+------------------------------------------------+\n" << Color::RESET;
            return;
        }
        cout << Color::RED << "[!] ID Not Found.\n" << Color::RESET;
    }

    // --- Background (Silent) ---
    void runLoop() {
        while(running) {
            this_thread::sleep_for(chrono::seconds(1));
            {
                lock_guard<mutex> lock(dataMutex);
                second++;
                totalSeconds++;

                if(second >= SECONDS_PER_DAY) {
                    second = 0;
                    day++;
                    if(day > 5) day = 1; 
                    cleanFinishedTrips();
                    resetVehicles();
                    logSystemEvent(day, 0, "SYSTEM", "NEW DAY", "Day " + to_string(day) + " Started.");
                }

                updateTrips();
                if(second == 150) dispatchLogic();
                writeAdminState();
            }
        }
    }

    void cleanFinishedTrips() {
        ListNode<Trip*>* curr = activeTrips.head;
        LinkedList<Trip*> nextDayList;
        while(curr) {
            Trip* t = curr->data;
            if(t->isFinished) delete t; 
            else nextDayList.append(t);
            curr = curr->next;
        }
        activeTrips.clear();
        activeTrips = nextDayList;
    }

    void updateTrips() {
        ListNode<Trip*>* curr = activeTrips.head;
        LinkedList<Trip*> nextList; 
        while(curr) {
            Trip* t = curr->data;
            if(t->isFinished) {
                nextList.append(t);
                curr = curr->next;
                continue;
            }
            long long elapsed = totalSeconds - t->startTime;
            int reqTime = t->distance * SECONDS_PER_NODE;

            if(elapsed >= reqTime) {
                t->isFinished = true; 
                ListNode<Parcel*>* pNode = t->parcels.head;
                while(pNode) {
                    int r = rand() % 1000;
                    if(r < 5) { 
                        pNode->data->status = "LOST";
                        totalLost++;
                        logSystemEvent(day, second, CITIES[t->dest], "CRITICAL", "Parcel " + pNode->data->id + " lost in transit.");
                    } else {
                        pNode->data->status = "Delivered";
                    }
                    pNode = pNode->next;
                }
                logSystemEvent(day, second, CITIES[t->dest], "ARRIVAL", "Trip from " + CITIES[t->src] + " Arrived (" + t->vehicleType + ")");
                nextList.append(t); 
            } else {
                nextList.append(t);
            }
            curr = curr->next;
        }
        activeTrips.clear(); 
        activeTrips = nextList; 
    }

    void writeAdminState() {
        lock_guard<mutex> lock(fileMutex);
        ofstream f("system_state.txt");
        int bookedCount = 0;
        int transitCount = 0;
        ListNode<Parcel*>* p = allParcels.head;
        while(p) {
            if(p->data->status == "Booked") bookedCount++;
            if(p->data->status == "In Transit") transitCount++;
            p = p->next;
        }
        f << "DAY: " << day << endl;
        f << "TIME: " << second << endl;
        f << "PARCELS_BOOKED: " << bookedCount << endl;
        f << "PARCELS_TRANSIT: " << transitCount << endl;
        f << "PARCELS_LOST: " << totalLost << endl;
        f << "--- TRIPS ---" << endl;
        ListNode<Trip*>* t = activeTrips.head;
        while(t) {
            long long elapsed = totalSeconds - t->data->startTime;
            double traveled = elapsed / (double)SECONDS_PER_NODE;
            if(traveled > t->data->distance) traveled = t->data->distance;
            f << t->data->src << " " << t->data->dest << " " << t->data->vehicleType << " " << (int)traveled << "km " << t->data->distance << "km" << endl;
            t = t->next;
        }
        f.close();
    }

    void dispatchLogic() {
        for(int s = 0; s < MAX_CITIES; s++) {
            for(int d = 0; d < MAX_CITIES; d++) {
                int count = 0;
                ListNode<Parcel*>* curr = allParcels.head;
                while(curr) {
                    if(curr->data->srcCity == s && curr->data->destCity == d && curr->data->status == "Booked") count++;
                    curr = curr->next;
                }
                if(count == 0) continue;

                Parcel** routeParcels = new Parcel*[count];
                int idx = 0;
                curr = allParcels.head;
                while(curr) {
                    if(curr->data->srcCity == s && curr->data->destCity == d && curr->data->status == "Booked") routeParcels[idx++] = curr->data;
                    curr = curr->next;
                }

                for(int i=0; i<count-1; i++) {
                    for(int j=0; j<count-i-1; j++) {
                        if(routeParcels[j]->priority > routeParcels[j+1]->priority) {
                            Parcel* temp = routeParcels[j];
                            routeParcels[j] = routeParcels[j+1];
                            routeParcels[j+1] = temp;
                        }
                    }
                }

                LinkedList<Parcel*> batch;
                int currentBatchWeight = 0;
                for(int i=0; i<count; i++) {
                    Parcel* p = routeParcels[i];
                    bool select = false;
                    if (day == 5) select = true;
                    else if (day == 1 || day == 3) {
                        if (p->priority == 1) select = true;
                        else if (currentBatchWeight + p->weight <= 300) select = true;
                    }
                    else if (day == 2 || day == 4) {
                        if (p->priority <= 2) select = true;
                        else if (currentBatchWeight + p->weight <= 600) select = true;
                    }
                    if(select) {
                        batch.append(p);
                        currentBatchWeight += p->weight;
                    }
                }
                delete[] routeParcels;

                if(batch.isEmpty()) continue;

                int routeDist = -1;
                bool isReroute = false;

                if (s == d) routeDist = 5; 
                else {
                    routeDist = graph.getShortestPath(s, d);
                    int directDist = DIST_MATRIX[s][d];
                    if(routeDist != -1 && directDist > 0 && routeDist > directDist) isReroute = true;
                }

                if(routeDist == -1) {
                    logSystemEvent(day, second, CITIES[s], "FAILURE", "Route blocked/unreachable to " + CITIES[d]);
                    continue;
                }

                string vType = "None";
                bool allocated = false;
                string reason = "Standard";
                
                if(currentBatchWeight <= 300 && bus300[s] > 0) { bus300[s]--; vType="Bus-300"; allocated=true; reason="Standard Overnight"; }
                else if(currentBatchWeight <= 600 && bus600[s] > 0) { bus600[s]--; vType="Bus-600"; allocated=true; if(currentBatchWeight>300) reason="Capacity Upgrade"; }
                else if(currentBatchWeight <= 900 && bus600[s]>0 && bus300[s]>0) { bus600[s]--; bus300[s]--; vType="Bus-600+300"; allocated=true; }
                else if(currentBatchWeight <= 1200 && bus600[s]>=2) { bus600[s]-=2; vType="2xBus-600"; allocated=true; }
                else if(currentBatchWeight <= 2000 && truck2000[s]>0) { truck2000[s]--; vType="Truck"; allocated=true; reason="Heavy Load Upgrade"; }
                else if(truck2000[s]>0) { truck2000[s]--; vType="Truck+Convoy"; allocated=true; reason="Heavy Load Upgrade"; }

                if(allocated) {
                    Trip* newTrip = new Trip(s, d, vType, routeDist, totalSeconds);
                    ListNode<Parcel*>* b = batch.head;
                    while(b) {
                        b->data->status = "In Transit";
                        b->data->dispatchTime = totalSeconds;
                        b->data->totalRouteDistance = routeDist;
                        newTrip->parcels.append(b->data);
                        b = b->next;
                    }
                    activeTrips.append(newTrip);
                    logSystemEvent(day, second, CITIES[s], "DISPATCH", "Sent " + vType + " to " + CITIES[d] + " (Load: " + to_string(currentBatchWeight) + "kg). " + reason + (isReroute?" [REROUTE]":""));
                } else {
                    logSystemEvent(day, second, CITIES[s], "DEFER", "Resource Shortage for " + CITIES[d] + " (Req: " + to_string(currentBatchWeight) + "kg). Deferred.");
                }
            }
        }
    }

    void stop() { running = false; }
};

// =========================================================
// 5. MAIN UI
// =========================================================
void clearScreen() {
    #ifdef _WIN32
        system("cls");
    #else
        system("clear");
    #endif
}

int main() {
    srand(time(0));
    Engine engine;
    thread simThread(&Engine::runLoop, &engine);
    
    int choice;
    while(true) {
        clearScreen();
        cout << Color::BLUE << "===========================================\n";
        cout << "      SWIFTEX LOGISTICS CUSTOMER PANEL     \n";
        cout << "===========================================\n" << Color::RESET;
        cout << Color::CYAN << " 1." << Color::RESET << " Book a New Parcel\n";
        cout << Color::CYAN << " 2." << Color::RESET << " Track Your Parcel\n";
        cout << Color::CYAN << " 3." << Color::RESET << " Cancel (Undo) Booking\n";
        cout << Color::RED << " 0." << Color::RESET << " Exit Application\n";
        cout << Color::BLUE << "===========================================\n" << Color::RESET;
        cout << " Select Option: ";
        
        if(!(cin >> choice)) {
            cin.clear(); cin.ignore(100, '\n');
            continue;
        }

        if(choice == 0) break;

        if(choice == 1) {
            int sc, so, dc, doo, w, p;
            cout << "\n" << Color::YELLOW << "--- NEW BOOKING WIZARD ---" << Color::RESET << "\n";
            cout << "Available Cities:\n";
            for(int i=0; i<MAX_CITIES; i++) {
                cout << " " << Color::CYAN << i << "." << Color::RESET << " " << left << setw(12) << CITIES[i];
                if((i+1)%2==0) cout << endl;
            }
            cout << "--------------------------\n";

            cout << " Source City ID: "; cin >> sc;
            cout << " Source Office (0=Hub, 1-5=Off): "; cin >> so;
            cout << " Dest City ID:   "; cin >> dc;
            cout << " Dest Office (0=Hub, 1-5=Off):   "; cin >> doo;
            cout << " Weight (kg):    "; cin >> w;
            cout << " Priority (1=Overnight, 2=2Day, 3=Normal): "; cin >> p;
            
            if(sc>=0 && sc<8 && dc>=0 && dc<8) engine.bookParcel(sc, so, dc, doo, w, p);
            else cout << Color::RED << "Invalid City ID Selected.\n" << Color::RESET;
            
            cout << "\nPress Enter to return..."; cin.ignore(); cin.get();
        }
        else if(choice == 2) {
            string id;
            char sub;
            do {
                cout << "\n" << Color::YELLOW << "--- PARCEL TRACKING ---" << Color::RESET << "\n";
                cout << " Enter Tracking ID: "; cin >> id;
                engine.trackParcel(id);
                cout << "\n [R] Refresh | [0] Back to Menu: ";
                cin >> sub;
            } while(sub == 'r' || sub == 'R');
        }
        else if(choice == 3) {
            string id;
            cout << "\n" << Color::YELLOW << "--- CANCEL BOOKING ---" << Color::RESET << "\n";
            cout << " Enter Tracking ID: "; cin >> id;
            engine.undoParcel(id);
            cout << "\nPress Enter to return..."; cin.ignore(); cin.get();
        }
    }

    engine.stop();
    simThread.join();
    return 0;
}