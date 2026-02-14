#include <iostream>
#include <fstream>
#include <string>
#include <thread>
#include <chrono>
#include <iomanip>
#include <conio.h> 

// REMOVED: #include <vector> 

using namespace std;

// =========================================================
// CUSTOM VECTOR IMPLEMENTATION 
// =========================================================
template <typename T>
class Vector {
    T* arr;
    int capacity;
    int currentSize;

public:
    // Constructor
    Vector() {
        capacity = 10;
        currentSize = 0;
        arr = new T[capacity];
    }

    // Destructor
    ~Vector() {
        if (arr) delete[] arr;
    }

    // Copy Constructor (Deep Copy) - Critical for passing structs by value
    Vector(const Vector& other) {
        capacity = other.capacity;
        currentSize = other.currentSize;
        arr = new T[capacity];
        for (int i = 0; i < currentSize; i++) {
            arr[i] = other.arr[i];
        }
    }

    // Assignment Operator (Deep Copy)
    Vector& operator=(const Vector& other) {
        if (this != &other) {
            if (arr) delete[] arr;
            capacity = other.capacity;
            currentSize = other.currentSize;
            arr = new T[capacity];
            for (int i = 0; i < currentSize; i++) {
                arr[i] = other.arr[i];
            }
        }
        return *this;
    }

    void push_back(T val) {
        if (currentSize == capacity) {
            resize();
        }
        arr[currentSize++] = val;
    }

    void resize() {
        capacity *= 2;
        T* newArr = new T[capacity];
        for (int i = 0; i < currentSize; i++) {
            newArr[i] = arr[i];
        }
        delete[] arr;
        arr = newArr;
    }

    T& operator[](int index) {
        return arr[index];
    }

    const T& operator[](int index) const {
        return arr[index];
    }

    int size() const {
        return currentSize;
    }

    bool empty() const {
        return currentSize == 0;
    }

    // Iterator support for range-based for loops
    T* begin() { return arr; }
    T* end() { return arr + currentSize; }
    const T* begin() const { return arr; }
    const T* end() const { return arr + currentSize; }
};

// =========================================================
// UI HELPERS
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
    const string WHITE  = "\033[37m";
}

const int MAX_CITIES = 8;
const string CITIES[MAX_CITIES] = {
    "Lahore", "Karachi", "Islamabad", "Multan",
    "Faisalabad", "Peshawar", "Quetta", "Sialkot"
};

struct TripInfo {
    int src, dest;
    string vehicle;
    int traveled;
    int total;
};

struct SystemState {
    int day;
    int time;
    int booked;
    int transit;
    int lost;
    Vector<TripInfo> trips; // Using Custom Vector
};

int parseKm(string s) {
    size_t pos = s.find("km");
    if (pos != string::npos) return stoi(s.substr(0, pos));
    try { return stoi(s); } catch(...) { return 0; }
}

void clearScreen() {
    #ifdef _WIN32
        system("cls");
    #else
        system("clear");
    #endif
}

void drawProgressBar(int current, int total) {
    if (total == 0) total = 1;
    if (current >= total) {
        cout << Color::GREEN << Color::BOLD << "[   ARRIVED  ]" << Color::RESET; 
        return;
    }

    int percent = (current * 100) / total;
    int bars = percent / 10; 
    
    cout << Color::YELLOW << "[";
    for(int i=0; i<10; i++) {
        if(i < bars) cout << "=";
        else if (i == bars) cout << ">";
        else cout << " ";
    }
    cout << "] " << setw(3) << percent << "%" << Color::RESET;
}

// =========================================================
// ADMIN PANEL CLASS
// =========================================================
class AdminPanel {
    int monitoredCity; 
    bool running;

public:
    AdminPanel() {
        monitoredCity = -1;
        running = true;
    }

    void selectCity() {
        while(true) {
            clearScreen();
            cout << Color::BLUE << "=== SWIFTEX ADMIN LOGIN ===" << Color::RESET << endl;
            cout << "Select Hub to Monitor:\n";
            for(int i=0; i<MAX_CITIES; i++) {
                cout << Color::CYAN << i << "." << Color::RESET << " " << CITIES[i] << endl;
            }
            cout << "Enter City ID: ";
            if(cin >> monitoredCity && monitoredCity >= 0 && monitoredCity < MAX_CITIES) {
                break;
            }
            cin.clear(); cin.ignore(100, '\n');
        }
    }

    SystemState readState() {
        SystemState state = {0, 0, 0, 0, 0};
        ifstream f("system_state.txt");
        if(!f.is_open()) return state;

        string label;
        f >> label >> state.day;
        f >> label >> state.time;
        f >> label >> state.booked;
        f >> label >> state.transit;
        
        string tempLine; 
        f >> label; 
        if(label == "PARCELS_LOST:") f >> state.lost;
        else getline(f, tempLine); 

        string dummy;
        f >> dummy >> dummy >> dummy; 

        int s, d;
        string v, t_str, tot_str;
        while(f >> s >> d >> v >> t_str >> tot_str) {
            TripInfo t;
            t.src = s; t.dest = d; t.vehicle = v;
            t.traveled = parseKm(t_str); t.total = parseKm(tot_str);
            state.trips.push_back(t); // Custom Vector push_back
        }
        f.close();
        return state;
    }

    // Changed return type to Custom Vector
    Vector<string> getLogs(string city) {
        Vector<string> lines;
        ifstream f("notifications.txt");
        if(!f.is_open()) return lines;
        
        string line;
        string tag = "[" + city + "]"; 
        while(getline(f, line)) {
            if(line.find(tag) != string::npos || line.find("[SYSTEM]") != string::npos) {
                lines.push_back(line);
            }
        }
        f.close();
        
        // Manual slicing logic since we don't have STL iterators
        if(lines.size() <= 5) return lines;

        Vector<string> lastFive;
        int start = lines.size() - 5;
        for(int i = start; i < lines.size(); i++) {
            lastFive.push_back(lines[i]);
        }
        return lastFive;
    }

    void showMenu() {
        clearScreen();
        cout << Color::BLUE << "=== ADMIN MENU (" << CITIES[monitoredCity] << ") ===" << Color::RESET << endl;
        cout << " 1. Block a Route\n";
        cout << " 2. Clear All Blocks\n";
        cout << " 3. Resume Dashboard\n";
        cout << " 0. Exit\n";
        cout << "Select: ";
        
        char choice = _getch();
        if(choice == '1') {
            int dest, dur;
            cout << "\nDest City ID: "; cin >> dest;
            cout << "Duration (Days): "; cin >> dur;
            ofstream f("blocks.txt", ios::app);
            f << monitoredCity << " " << dest << " " << dur << endl;
            f.close();
            cout << Color::RED << ">> Route Blocked.\n" << Color::RESET;
            _getch();
        } 
        else if(choice == '2') {
            ofstream f("blocks.txt", ios::trunc);
            f.close();
            cout << Color::GREEN << ">> All Blocks Cleared.\n" << Color::RESET;
            _getch();
        }
        else if(choice == '0') {
            exit(0);
        }
    }

    void dashboardLoop() {
        cout << "Starting Dashboard...\n";
        this_thread::sleep_for(chrono::seconds(1));

        while(running) {
            if(_kbhit()) {
                char ch = _getch();
                if(ch == 'm' || ch == 'M') {
                    showMenu();
                    continue; 
                }
            }

            SystemState state = readState();
            // Vector<string> allows range-based loops because we implemented begin() and end()
            Vector<string> logs = getLogs(CITIES[monitoredCity]); 

            clearScreen();
            cout << Color::BLUE << "========================================================\n";
            cout << "   SWIFTEX LIVE MONITOR: " << Color::BOLD << Color::WHITE << CITIES[monitoredCity] << Color::RESET << Color::BLUE << "\n";
            cout << "========================================================\n" << Color::RESET;
            
            cout << " Day: " << Color::BOLD << state.day << Color::RESET;
            cout << "  |  Time: " << Color::BOLD << state.time << "/180s" << Color::RESET << "\n";
            
            cout << " Stats: " 
                 << Color::CYAN << "Booked: " << state.booked << Color::RESET << " | "
                 << Color::YELLOW << "Transit: " << state.transit << Color::RESET << " | "
                 << Color::RED << "Lost: " << state.lost << Color::RESET << "\n";
            
            cout << Color::BLUE << "--------------------------------------------------------\n" << Color::RESET;
            cout << Color::WHITE << " OUTGOING TRAFFIC (" << CITIES[monitoredCity] << ")\n" << Color::RESET;
            cout << Color::BLUE << "--------------------------------------------------------\n" << Color::RESET;
            cout << left << setw(12) << "Destination" << setw(15) << "Vehicle" << "Status\n";

            // Local Traffic Check
             bool localActive = false;
             TripInfo localTrip;
             for(const auto& t : state.trips) {
                if(t.src == monitoredCity && t.dest == monitoredCity) {
                    localTrip = t; localActive = true; break;
                }
             }
             if(localActive) {
                cout << left << setw(12) << "LOCAL" << setw(15) << localTrip.vehicle;
                drawProgressBar(localTrip.traveled, localTrip.total);
                cout << "\n";
             }

            // Outgoing Traffic
            for(int i=0; i<MAX_CITIES; i++) {
                if(i == monitoredCity) continue;
                bool active = false;
                TripInfo currentTrip;
                for(const auto& t : state.trips) {
                    if(t.src == monitoredCity && t.dest == i) {
                        currentTrip = t; active = true; break;
                    }
                }
                if(active) {
                    cout << left << setw(12) << CITIES[i] << setw(15) << currentTrip.vehicle;
                    drawProgressBar(currentTrip.traveled, currentTrip.total);
                    cout << "\n";
                } else {
                    cout << Color::WHITE << left << setw(12) << CITIES[i] << setw(15) << "-" << "Idle\n" << Color::RESET;
                }
            }

            cout << Color::BLUE << "--------------------------------------------------------\n" << Color::RESET;
            cout << Color::WHITE << " LIVE NOTIFICATIONS\n" << Color::RESET;
            cout << Color::BLUE << "--------------------------------------------------------\n" << Color::RESET;
            
            if(logs.empty()) cout << " No events yet.\n";
            else {
                for(const string& l : logs) {
                    // Context-Aware Coloring
                    if(l.find("CRITICAL") != string::npos || l.find("LOST") != string::npos || l.find("FAILURE") != string::npos)
                        cout << Color::RED << l << Color::RESET << endl;
                    else if(l.find("DISPATCH") != string::npos || l.find("ARRIVAL") != string::npos)
                        cout << Color::GREEN << l << Color::RESET << endl;
                    else if(l.find("REROUTE") != string::npos)
                        cout << Color::MAGENTA << l << Color::RESET << endl;
                    else if(l.find("DEFER") != string::npos)
                        cout << Color::YELLOW << l << Color::RESET << endl;
                    else
                        cout << l << endl;
                }
            }
            
            cout << Color::BLUE << "========================================================\n" << Color::RESET;
            cout << " [M] Menu/Block Route  |  [Ctrl+C] Exit\n";

            this_thread::sleep_for(chrono::seconds(1));
        }
    }
};

int main() {
    AdminPanel admin;
    admin.selectCity();
    admin.dashboardLoop();
    return 0;
}