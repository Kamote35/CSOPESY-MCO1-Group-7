#include <iostream>
#include <string>
#include <thread>
#include <vector>
#include <map>
#include <memory>
#include <mutex>
#include <chrono>
#include <random>
#include <fstream>
#include <sstream>
#include <iomanip>
#include <ctime>
#include <atomic>

using namespace std;

// forward declaration (class style)
class Process;

// Globals
map<string, unique_ptr<Process>> procList;
mutex processLock;
int next_pid = 1;

// trim helper
static inline void trim(string &s) {
    auto l = s.find_first_not_of(" \t\r\n");
    if (l == string::npos) { s.clear(); return; }
    auto r = s.find_last_not_of(" \t\r\n");
    s = s.substr(l, r - l + 1);
}

pair<int,int> readConfigRange() {
    ifstream f("config.txt");
    int a = 50, b = 200;
    if (f >> a >> b) return make_pair(a,b);
    return make_pair(50,200);
}

void clearScreen() {
#ifdef _WIN32
    system("cls");
#else
    system("clear");
#endif
}

void printHeader() {
    cout << "------------------------------------" << endl;
    cout << "Welcome to CSOPESY Emulator!" << endl << endl;
    cout << "Developers:" << endl;
    cout << "Corpuz, Gerald Justine\n";
	cout << "De Jesus, Andrei Zarmin\n";
	cout << "Manaois, Chriscel John\n";
	cout << "Sayat, John Christian\n";
    cout << "Last updated: " << __DATE__ << endl;
    cout << "-------------------------------------" << endl << endl;
}

void handleSchedulerStart() {
    cout << "[scheduler-start] Placeholder" << endl;
}
void handleSchedulerStop() {
    cout << "[scheduler-stop] Placeholder" << endl;
}
void handleReportUtil() {
    cout << "[report-util] Placeholder" << endl;
}

class Process {
    string name;
    int id;
    int total_instructions;
    int current_instruction;
    bool finished;
    vector<string> logs;
    mutex logLock;
    thread worker;

public:
    Process(const string &n, int pid, int lines)
    : name(n), id(pid), total_instructions(lines), current_instruction(0), finished(false) {}

    // non-copyable
    Process(const Process&) = delete;
    Process& operator=(const Process&) = delete;

    // destructor ensures worker is joined (RAII)
    ~Process() { join(); }

    void start() {
        worker = thread([this]() {
            random_device rd;
            mt19937 gen(rd());
            uniform_int_distribution<int> delayMs(40, 120);
            uniform_int_distribution<int> core(0,1);

            while (current_instruction < total_instructions) {
                this_thread::sleep_for(chrono::milliseconds(delayMs(gen)));
                ++current_instruction;

                if ((current_instruction % 25) == 0) {
                    time_t now = time(nullptr);
                    tm local_tm = *localtime(&now);
                    char buf[64];
                    strftime(buf, sizeof(buf), "(%m/%d/%Y %I:%M:%S %p)", &local_tm);
                    ostringstream oss;
                    oss << buf << " Core:" << core(gen) << " \"Hello world from " << name << "!\"";
                    lock_guard<mutex> lg(logLock);
                    logs.push_back(oss.str());
                }
            }

            lock_guard<mutex> lg(logLock);
            finished = true;
            logs.push_back("Process finished!");
        });
    }

    void join() {
        if (worker.joinable()) worker.join();
    }

    // simple accessors for external use
    const string& getName() const { return name; }
    int getId() const { return id; }
    int getCurrent() const { return current_instruction; }
    int getTotal() const { return total_instructions; }
    bool isFinished() const { return finished; }

    vector<string> snapshotLogs() {
        lock_guard<mutex> lg(logLock);
        return logs; // copy for simplicity
    }
};

void cmdScreenCreate(const string &rawName) {
    string name = rawName;
    trim(name);

    {
        lock_guard<mutex> lock(processLock);
        if (procList.count(name)) {
            cout << "Process with name '" << name << "' already exists." << endl;
            return;
        }
    }

    pair<int,int> range = readConfigRange();
    int minv = range.first; int maxv = range.second;
    random_device rd; 
    mt19937 gen(rd()); 
    uniform_int_distribution<int> dist(minv, maxv);
    int lines = dist(gen);

    {
        lock_guard<mutex> lock(processLock);
        procList[name] = make_unique<Process>(name, next_pid++, lines);
    }
    Process *p = procList[name].get();
    p->start();

    cout << "Created process '" << name << "' with ID " << p->getId() << "." << endl;
    this_thread::sleep_for(chrono::milliseconds(200));
    clearScreen();
    cout << "Process name: " << p->getName() << endl;
    cout << "ID: " << p->getId() << endl;
    cout << "Logs:" << endl << endl;
}

void cmdScreenList() {
    lock_guard<mutex> lock(processLock);
    bool any = false;
    for (auto &kv : procList) {
        Process *p = kv.second.get();
        if (p && !p->isFinished()) {
            cout << "- " << kv.first << " (ID: " << p->getId() << ")" << endl;
            any = true;
        }
    }
    if (!any) cout << "(no running processes)" << endl;
}

void attachToProcess(const string &rawName) {
    string name = rawName;
    trim(name);
    Process *p = nullptr;
    {
        lock_guard<mutex> lock(processLock);
        auto it = procList.find(name);
        if (it == procList.end()) {
            cout << "Process '" << name << "' not found." << endl;
            return;
        }
        p = it->second.get();
    }

    clearScreen();
    cout << "Process name: " << p->getName() << endl;
    cout << "ID: " << p->getId() << endl;
    cout << "Logs:" << endl;

    string cmd;
    while (true) {
        cout << "root:\\> ";
        if (!getline(cin, cmd)) break;
        trim(cmd);
        if (cmd.empty()) continue;

        if (cmd == "exit") {
            cout << "Returning to main menu..." << endl;
            if (p->isFinished()) {
                p->join();
                lock_guard<mutex> lock(processLock);
                auto it = procList.find(name);
                if (it != procList.end() && it->second.get() == p) {
                    procList.erase(it); // unique_ptr destructor will delete and join
                }
            }
            break;
        }

        if (cmd == "process-smi") {
            auto logs = p->snapshotLogs();
            cout << "\nProcess name: " << p->getName() << endl;
            cout << "ID: " << p->getId() << endl;
            cout << "Logs:" << endl;
            for (size_t i = 0; i < logs.size(); ++i) cout << logs[i] << endl;
            cout << "\nCurrent instruction line: " << p->getCurrent() << endl;
            cout << "Lines of code: " << p->getTotal() << endl;
            if (p->isFinished()) cout << "Finished!" << endl;
        } else {
            cout << "Unknown command inside process screen. Available: process-smi, exit" << endl;
        }
    }
}

int main() {
    bool initialized = false;
    
    string line;
    while (true) {
        printHeader();
        cout << "root:\\> ";
        if (!getline(cin, line)) break;
        trim(line);
        if (line.empty()) continue;

        if (line == "exit") {
            cout << "Exiting console." << endl;
            // clean up
            {
                lock_guard<mutex> lock(processLock);
                for (auto &kv : procList) {
                    Process *p = kv.second.get();
                    if (p) p->join();
                }
                procList.clear();
            }
            break;
        }

        if (!initialized) {
            if (line == "initialize") {
                initialized = true;
                cout << "Processor configuration initialized." << endl;
            } else {
                cout << "Error: 'initialize' must be called before other commands. Type 'exit' to quit." << endl;
            }
            continue;
        }

        if (line == "initialize") {
            cout << "Already initialized." << endl;
            continue;
        }

        // parse commands
        if (line.rfind("screen", 0) == 0) {
            istringstream iss(line);
            string cmd, arg, name;
            iss >> cmd >> arg;
            getline(iss, name);
            trim(name);

            if (arg == "-ls") {
                cmdScreenList();
            } else if (arg == "-s") {
                if (name.empty()) {
                    cout << "Usage: screen -s <name>" << endl;
                } else {
                    cmdScreenCreate(name);
                    attachToProcess(name);
                }
            } else if (arg == "-r") {
                if (name.empty()) {
                    cout << "Usage: screen -r <name>" << endl;
                } else {
                    attachToProcess(name);
                }
            } else {
                cout << "Invalid 'screen' command. Usage:\n  screen -s <name> | screen -ls | screen -r <name>" << endl;
            }
            continue;
        }

        if (line == "scheduler-start") { handleSchedulerStart(); continue; }
        if (line == "scheduler-stop")  { handleSchedulerStop(); continue; }
        if (line == "report-util")     { handleReportUtil(); continue; }

        cout << "Unknown command: '" << line << "'" << endl;
    }

    return 0;
}