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
#include <algorithm>

using namespace std;

// forward declaration
class Process;

// Globals
map<string, unique_ptr<Process>> procList;
mutex processLock;
int next_pid = 1;

int g_minIns = 50;
int g_maxIns = 200;
bool g_configLoaded = false;
bool g_schedulerRunning = false;

static inline void trim(string &s) {
    auto l = s.find_first_not_of(" \t\r\n");
    if (l == string::npos) { s.clear(); return; }
    auto r = s.find_last_not_of(" \t\r\n");
    s = s.substr(l, r - l + 1);
}

// Read config.txt (space-separated key value per line).
pair<int,int> readConfigRange() {
    if (g_configLoaded) return {g_minIns, g_maxIns};

    ifstream file("config.txt");
    int minv = 50, maxv = 200;
    if (!file) {
        g_minIns = minv; g_maxIns = maxv; g_configLoaded = false;
        return {minv, maxv};
    }

    string line;
    while (getline(file, line)) {
        trim(line);
        if (line.empty() || line[0] == '#') continue;

        istringstream iss(line);
        string key, value;
        iss >> key >> value;
        if (!key.empty() && key.back() == ':') key.pop_back();
        transform(key.begin(), key.end(), key.begin(), ::tolower);
        try {
            if (key == "min-ins" || key == "minins") minv = stoi(value);
            else if (key == "max-ins" || key == "maxins") maxv = stoi(value);
        } catch (...) {}
    }
    if (maxv < minv) swap(maxv, minv);
    g_minIns = minv; g_maxIns = maxv; g_configLoaded = true;
    return {minv, maxv};
}

void clearScreen() {
#ifdef _WIN32
    system("cls");
#else
    system("clear");
#endif
}

void printHeader() {
    cout << "------------------------------------\n";
    cout << "Welcome to CSOPESY Emulator!\n\n";
    cout << "Last updated: " << __DATE__ << "\n";
    cout << "------------------------------------\n\n";
}

// ---------------------------------------------------------------------
// INSTRUCTION SYSTEM
// ---------------------------------------------------------------------
enum class InstrType { PRINT, DECLARE, ADD, SUBTRACT, SLEEP, FOR };

struct Instruction {
    InstrType type;
    vector<string> args;
    vector<Instruction> nested; // used if type == FOR
    int repeatCount = 0;
};

// ---------------------------------------------------------------------
// Process Class
// ---------------------------------------------------------------------
class Process {
    string name;
    int id;
    bool finished;
    thread worker;
    vector<string> logs;
    map<string, uint16_t> vars;
    mutex logLock;
    vector<Instruction> instructions;

    random_device rd;
    mt19937 gen;

    void log(const string &msg) {
        lock_guard<mutex> lg(logLock);
        time_t now = time(nullptr);
        tm local_tm = *localtime(&now);
        char buf[64];
        strftime(buf, sizeof(buf), "(%m/%d/%Y %I:%M:%S %p)", &local_tm);
        logs.push_back(string(buf) + " " + msg);
    }

    void execInstruction(const Instruction &ins) {
        switch (ins.type) {
        case InstrType::PRINT:
            if (!ins.args.empty()) {
                string msg = ins.args[0];
                if (msg.size() > 1 && msg[0] == '+') {
                    string var = msg.substr(1);
                    if (vars.count(var))
                        msg = var + " = " + to_string(vars[var]);
                }
                log("PRINT: " + msg);
            }
            break;
        case InstrType::DECLARE:
            if (ins.args.size() >= 2) {
                string var = ins.args[0];
                uint16_t val = stoi(ins.args[1]);
                vars[var] = val;
                log("DECLARE: " + var + " = " + to_string(val));
            }
            break;
        case InstrType::ADD:
            if (ins.args.size() >= 3) {
                string v1 = ins.args[0], v2 = ins.args[1], v3 = ins.args[2];
                uint16_t a = vars.count(v2) ? vars[v2] : (uint16_t)stoi(v2);
                uint16_t b = vars.count(v3) ? vars[v3] : (uint16_t)stoi(v3);
                vars[v1] = a + b;
                log("ADD: " + v1 + " = " + to_string(a) + " + " + to_string(b));
            }
            break;
        case InstrType::SUBTRACT:
            if (ins.args.size() >= 3) {
                string v1 = ins.args[0], v2 = ins.args[1], v3 = ins.args[2];
                uint16_t a = vars.count(v2) ? vars[v2] : (uint16_t)stoi(v2);
                uint16_t b = vars.count(v3) ? vars[v3] : (uint16_t)stoi(v3);
                vars[v1] = a - b;
                log("SUBTRACT: " + v1 + " = " + to_string(a) + " - " + to_string(b));
            }
            break;
        case InstrType::SLEEP:
            if (!ins.args.empty()) {
                int ticks = stoi(ins.args[0]);
                log("SLEEP for " + to_string(ticks) + " ticks");
                this_thread::sleep_for(chrono::milliseconds(ticks * 10));
            }
            break;
        case InstrType::FOR:
            for (int r = 0; r < ins.repeatCount; ++r)
                for (auto &nestedIns : ins.nested)
                    execInstruction(nestedIns);
            break;
        }
    }

    vector<Instruction> generateRandomInstructions(int count, int depth = 0) {
        vector<Instruction> list;
        uniform_int_distribution<int> typeDist(0, 4);
        uniform_int_distribution<int> sleepDist(1, 5);
        uniform_int_distribution<int> valueDist(0, 50);
        uniform_int_distribution<int> repeatDist(1, 3);

        for (int i = 0; i < count; ++i) {
            int type = typeDist(gen);
            Instruction ins;
            switch (type) {
            case 0: ins.type = InstrType::PRINT; ins.args = {"Hello world from " + name + "!"}; break;
            case 1: ins.type = InstrType::DECLARE; ins.args = {"x" + to_string(i), to_string(valueDist(gen))}; break;
            case 2: ins.type = InstrType::ADD; ins.args = {"x" + to_string(i), to_string(valueDist(gen)), to_string(valueDist(gen))}; break;
            case 3: ins.type = InstrType::SUBTRACT; ins.args = {"x" + to_string(i), to_string(valueDist(gen)), to_string(valueDist(gen))}; break;
            case 4: ins.type = InstrType::SLEEP; ins.args = {to_string(sleepDist(gen))}; break;
            }

            if (depth < 3 && uniform_int_distribution<int>(0, 9)(gen) == 0) {
                Instruction loop;
                loop.type = InstrType::FOR;
                loop.repeatCount = repeatDist(gen);
                loop.nested = generateRandomInstructions(3, depth + 1);
                list.push_back(loop);
            } else list.push_back(ins);
        }
        return list;
    }

public:
    Process(const string &n, int pid, int lines)
        : name(n), id(pid), finished(false), gen(rd()) {
        instructions = generateRandomInstructions(lines);
    }

    ~Process() { join(); }

    void start() {
        worker = thread([this]() {
            for (auto &ins : instructions)
                execInstruction(ins);
            lock_guard<mutex> lg(logLock);
            finished = true;
            logs.push_back("Process finished!");
        });
    }

    void join() {
        if (worker.joinable()) worker.join();
    }

    const string &getName() const { return name; }
    int getId() const { return id; }
    bool isFinished() const { return finished; }

    vector<string> snapshotLogs() {
        lock_guard<mutex> lg(logLock);
        return logs;
    }
};

// ---------------------------------------------------------------------
// Command Handlers
// ---------------------------------------------------------------------

void handleSchedulerStart() {
    if (g_schedulerRunning) {
        cout << "Scheduler already running.\n";
        return;
    }
    g_schedulerRunning = true;
    cout << "Starting dummy scheduler... generating processes...\n";

    thread([]() {
        random_device rd; mt19937 gen(rd());
        uniform_int_distribution<int> sleepDist(1000, 2000);
        int counter = 1;

        while (g_schedulerRunning) {
            string name = "proc_" + to_string(counter++);
            auto range = readConfigRange();
            uniform_int_distribution<int> dist(range.first, range.second);
            int lines = dist(gen);

            {
                lock_guard<mutex> lock(processLock);
                procList[name] = make_unique<Process>(name, next_pid++, lines);
                procList[name]->start();
            }
            this_thread::sleep_for(chrono::milliseconds(sleepDist(gen)));
        }
    }).detach();
}

void handleSchedulerStop() {
    if (!g_schedulerRunning) {
        cout << "Scheduler not running.\n";
        return;
    }
    g_schedulerRunning = false;
    cout << "Scheduler stopped.\n";
}

void handleReportUtil() {
    lock_guard<mutex> lock(processLock);
    cout << "=== CPU UTILIZATION REPORT ===\n";
    int total = 0, finished = 0;
    for (auto &kv : procList) {
        total++;
        if (kv.second->isFinished()) finished++;
    }
    cout << "Total processes: " << total << "\n";
    cout << "Finished: " << finished << "\n";
    cout << "Running: " << (total - finished) << "\n";
    cout << "================================\n";
}

/* v3
void cmdScreenList() {
    lock_guard<mutex> lock(processLock);
    if (procList.empty()) {
        cout << "(no processes)\n";
        return;
    }
    for (auto &kv : procList)
        cout << "- " << kv.first << (kv.second->isFinished() ? " [FINISHED]" : " [RUNNING]") << endl;
}
*/
void cmdScreenList() {
    lock_guard<mutex> lock(processLock);
    if (procList.empty()) {
        cout << "(no processes)\n";
        return;
    }

    int total = (int)procList.size();
    int finished = 0;
    for (auto &kv : procList)
        if (kv.second->isFinished())
            finished++;

    int usedCores = min(total, 4); // assuming 4 CPUs (from config)
    int available = max(0, 4 - usedCores);

    cout << "CPU utilization: " << (usedCores * 25) << "%\n";
    cout << "Cores used: " << usedCores << "\n";
    cout << "Cores available: " << available << "\n";
    cout << "------------------------------------\n";

    // === Running processes ===
    cout << "Running processes:\n";
    int core = 0;
    for (auto &kv : procList) {
        auto &p = kv.second;
        if (!p->isFinished()) {
            time_t now = time(nullptr);
            tm local_tm = *localtime(&now);
            char buf[64];
            strftime(buf, sizeof(buf), "(%m/%d/%Y %I:%M:%S%p)", &local_tm);

            // Simulate progress out of total (since we don’t have real counters)
            int totalIns = 1000 + (p->getId() * 10);
            int done = rand() % totalIns;
            cout << left << setw(10) << p->getName()
                 << " " << buf
                 << "   Core: " << core++ % 4
                 << "   " << done << " / " << totalIns << "\n";
        }
    }

    // === Finished processes ===
    cout << "\nFinished processes:\n";
    for (auto &kv : procList) {
        auto &p = kv.second;
        if (p->isFinished()) {
            time_t now = time(nullptr);
            tm local_tm = *localtime(&now);
            char buf[64];
            strftime(buf, sizeof(buf), "(%m/%d/%Y %I:%M:%S%p)", &local_tm);

            int totalIns = 1000 + (p->getId() * 10);
            cout << left << setw(10) << p->getName()
                 << " " << buf
                 << "   Finished   " << totalIns << " / " << totalIns << "\n";
        }
    }
}


void attachToProcess(const string &rawName) {
    string name = rawName;
    trim(name);
    Process *p = nullptr;
    {
        lock_guard<mutex> lock(processLock);
        auto it = procList.find(name);
        if (it == procList.end()) {
            cout << "Process '" << name << "' not found.\n";
            return;
        }
        p = it->second.get();
    }

    clearScreen();
    cout << "Attached to process: " << name << "\nType 'process-smi' to view logs, 'exit' to leave.\n";

    string cmd;
    while (true) {
        cout << "proc> ";
        if (!getline(cin, cmd)) break;
        trim(cmd);
        if (cmd == "exit") break;
        if (cmd == "process-smi") {
            auto logs = p->snapshotLogs();
            for (auto &line : logs) cout << line << endl;
            if (p->isFinished()) cout << "(process finished)\n";
        } else cout << "Unknown command.\n";
    }
}

int main() {
    bool initialized = false;
    printHeader();
    string line;

    while (true) {
        cout << "root:\\> ";
        if (!getline(cin, line)) break;
        trim(line);
        if (line.empty()) continue;

        if (line == "exit") {
            cout << "Exiting console.\n";
            {
                lock_guard<mutex> lock(processLock);
                for (auto &kv : procList) kv.second->join();
                procList.clear();
            }
            break;
        }

        if (!initialized) {
            if (line == "initialize") {
                readConfigRange();
                initialized = true;
                cout << "Processor initialized.\n";
            } else {
                cout << "Error: call 'initialize' first.\n";
            }
            continue;
        }

        if (line == "scheduler-start") { handleSchedulerStart(); continue; }
        if (line == "scheduler-stop")  { handleSchedulerStop(); continue; }
        if (line == "report-util")     { handleReportUtil(); continue; }
        if (line == "screen -ls")      { cmdScreenList(); continue; }

        if (line.rfind("screen -r", 0) == 0) {
            string name = line.substr(10);
            trim(name);
            attachToProcess(name);
            continue;
        }

        cout << "Unknown command.\n";
    }

    return 0;
}
