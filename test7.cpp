/*
	To do:
		1. fix process-smi core
		2. add report-util
		3. do some minor fixes
		4. fix cpu utilization (100% when fully used, 0% if not used, etc..)
		6. not sure if auto quit screen if process is already finished
		7. when everything else is okay, try to make classes in the different files for better readability
*/

// main.cpp
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

// globals
map<string, unique_ptr<Process>> procList;
mutex processLock;
int next_pid = 1;

// default values if config.txt does not exist
int g_numCPU = 4;
string g_schedulerType = "rr"; // forced to "rr"
int g_quantumCycles = 5;
uint64_t g_batchFreq = 1; // in CPU cycles; stored as uint64_t to match large range
int g_minIns = 50;
int g_maxIns = 200;
uint64_t g_delayPerExec = 0; // in CPU cycles (we will convert to ms internally)
bool g_configLoaded = false;
bool g_schedulerRunning = false;

static inline void trim(string &s) {
    auto l = s.find_first_not_of(" \t\r\n");
    if (l == string::npos) { s.clear(); return; }
    auto r = s.find_last_not_of(" \t\r\n");
    s = s.substr(l, r - l + 1);
}

// config.txt reader
void readConfig() {
    if (g_configLoaded) return;

    ifstream file("config.txt");
    if (!file) {
        cout << "Warning: config.txt not found. Using defaults.\n";
        // enforce scheduler to rr
        g_schedulerType = "rr";
        g_configLoaded = true;
        cout << "Configuration (defaults):\n";
        cout << "  num-cpu = " << g_numCPU << "\n";
        cout << "  scheduler = " << g_schedulerType << " \n";
        cout << "  quantum-cycles = " << g_quantumCycles << "\n";
        cout << "  batch-process-freq = " << g_batchFreq << "\n";
        cout << "  min-ins = " << g_minIns << "\n";
        cout << "  max-ins = " << g_maxIns << "\n";
        cout << "  delay-per-exec = " << g_delayPerExec << "\n";
        cout << "----------------------------------------\n";
        return;
    }

    string line;
    while (getline(file, line)) {
        trim(line);
        if (line.empty() || line[0] == '#') continue;

        istringstream iss(line);
        string key, value;
        iss >> key;
        // read rest of line as value (in case of quoted strings)
        string rest;
        getline(iss, rest);
        trim(rest);
        if (!rest.empty()) value = rest;
        // strip surrounding quotes if present
        if (!value.empty() && value.front() == '"' && value.back() == '"') {
            value = value.substr(1, value.size() - 2);
        }

        if (key.empty()) continue;
        transform(key.begin(), key.end(), key.begin(), ::tolower);
        try {
            if (key == "num-cpu" || key == "numcpu") {
                int v = stoi(value);
                if (v < 1) v = 1;
                if (v > 128) v = 128;
                g_numCPU = v;
            } else if (key == "scheduler") {
                // ignore what's written: always force rr
                (void)value;
                g_schedulerType = "rr";
            } else if (key == "quantum-cycles" || key == "quantumcycles") {
                int v = stoi(value);
                if (v < 1) v = 1;
                g_quantumCycles = v;
            } else if (key == "batch-process-freq" || key == "batchprocessfreq") {
                // large range supported; treat as unsigned 64-bit cycles
                uint64_t v = stoull(value);
                if (v < 1) v = 1;
                g_batchFreq = v;
            } else if (key == "min-ins" || key == "minins") {
                int v = stoi(value);
                if (v < 1) v = 1;
                g_minIns = v;
            } else if (key == "max-ins" || key == "maxins") {
                int v = stoi(value);
                if (v < 1) v = 1;
                g_maxIns = v;
            } else if (key == "delay-per-exec" || key == "delayperexec") {
                uint64_t v = stoull(value);
                g_delayPerExec = v;
            }
        } catch (...) {
            cout << "Invalid config entry ignored: " << line << "\n";
        }
    }

    if (g_maxIns < g_minIns) swap(g_maxIns, g_minIns);
    if (g_numCPU < 1) g_numCPU = 1;
    if (g_numCPU > 128) g_numCPU = 128;

    // enforce scheduler to rr (test case)
    g_schedulerType = "rr";

    // print loaded configuration from config.txt
    cout << "Configuration loaded:\n";
    cout << "  num-cpu = " << g_numCPU << "\n";
    cout << "  scheduler = " << g_schedulerType << " (forced)\n";
    cout << "  quantum-cycles = " << g_quantumCycles << "\n";
    cout << "  batch-process-freq = " << g_batchFreq << "\n";
    cout << "  min-ins = " << g_minIns << "\n";
    cout << "  max-ins = " << g_maxIns << "\n";
    cout << "  delay-per-exec = " << g_delayPerExec << "\n";
    cout << "----------------------------------------\n";

    g_configLoaded = true;
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
    // insert our names here after everything is working
    cout << "Last updated: " << __DATE__ << "\n";
    cout << "------------------------------------\n";
}

// INSTRUCTION SYSTEM
enum class InstrType { PRINT, DECLARE, ADD, SUBTRACT, SLEEP, FOR };

struct Instruction {
    InstrType type;
    vector<string> args;
    vector<Instruction> nested; // used if type == FOR
    int repeatCount = 0;
};

// Process Class
class Process {
    string name;
    int id;
    bool finished;
    thread worker;
    vector<string> logs;
    map<string, uint16_t> vars;
    mutex logLock;
    vector<Instruction> instructions;

    // variables for tracking
    int coreAssigned;               // simulated core id (for display)
    atomic<int> currentInstrIndex;  // 1-based current instruction index
    int totalLines;

    random_device rd;
    mt19937 gen;

    void log(const string &msg) {
        lock_guard<mutex> lg(logLock);
        time_t now = time(nullptr);
        tm local_tm = *localtime(&now);
        char buf[64];
        strftime(buf, sizeof(buf), "(%m/%d/%Y %I:%M:%S%p)", &local_tm);
        // sample format: (08/06/2024 09:15:22AM) Core:0 "Hello world..."
        string entry = string(buf) + " Core:" + to_string(coreAssigned) + " \"" + msg + "\"";
        logs.push_back(entry);
    }

    void execInstruction(const Instruction &ins) {
        // If delay-per-exec > 0, convert CPU-cycle delay to milliseconds.
        // We interpret 1 CPU cycle -> 1 millisecond for simulation purposes.
        if (g_delayPerExec > 0) {
            this_thread::sleep_for(chrono::milliseconds(static_cast<int>(g_delayPerExec)));
        }

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
                uint16_t val = static_cast<uint16_t>(stoi(ins.args[1]));
                vars[var] = val;
                log("DECLARE: " + var + " = " + to_string(val));
            }
            break;
        case InstrType::ADD:
            if (ins.args.size() >= 3) {
                string v1 = ins.args[0], v2 = ins.args[1], v3 = ins.args[2];
                uint16_t a = vars.count(v2) ? vars[v2] : static_cast<uint16_t>(stoi(v2));
                uint16_t b = vars.count(v3) ? vars[v3] : static_cast<uint16_t>(stoi(v3));
                vars[v1] = a + b;
                log("ADD: " + v1 + " = " + to_string(a) + " + " + to_string(b));
            }
            break;
        case InstrType::SUBTRACT:
            if (ins.args.size() >= 3) {
                string v1 = ins.args[0], v2 = ins.args[1], v3 = ins.args[2];
                uint16_t a = vars.count(v2) ? vars[v2] : static_cast<uint16_t>(stoi(v2));
                uint16_t b = vars.count(v3) ? vars[v3] : static_cast<uint16_t>(stoi(v3));
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
        : name(n), id(pid), finished(false), coreAssigned(0), currentInstrIndex(0), gen(rd()) {
        // assign core in a simple, deterministic way:
        // ensure g_numCPU is at least 1 (should be after config load)
        int numcpu = max(1, g_numCPU);
        coreAssigned = ( (pid - 1) % numcpu );
        instructions = generateRandomInstructions(lines);
        totalLines = static_cast<int>(instructions.size());
    }

    ~Process() { join(); }

    void start() {
        worker = thread([this]() {
            for (size_t i = 0; i < instructions.size(); ++i) {
                currentInstrIndex.store(static_cast<int>(i) + 1);
                execInstruction(instructions[i]);
            }
            currentInstrIndex.store(static_cast<int>(instructions.size()));
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

    // snapshot logs
    vector<string> snapshotLogs() {
        lock_guard<mutex> lg(logLock);
        return logs;
    }

    // exposed getters for process-smi
    int getCoreAssigned() const { return coreAssigned; }
    int getCurrentInstructionLine() const { return currentInstrIndex.load(); } // 1-based
    int getTotalLines() const { return totalLines; }
};

// Command Handlers
void handleSchedulerStart() {
    if (g_schedulerRunning) {
        cout << "Scheduler already running.\n";
        return;
    }
    g_schedulerRunning = true;
    cout << "Starting scheduler (" << g_schedulerType << ")...\n";
    cout << "------------------------------------\n";

    thread([]() {
        random_device rd; mt19937 gen(rd());
        int counter = 1;

        const uint64_t cycle_ms = 100;

        while (g_schedulerRunning) {
            string name = "proc-" + to_string(counter++);
            uniform_int_distribution<int> dist(g_minIns, g_maxIns);
            int lines = dist(gen);

            {
                lock_guard<mutex> lock(processLock);
                procList[name] = make_unique<Process>(name, next_pid++, lines);
                procList[name]->start();
            }

            // sleep according to batch-process-freq
            uint64_t sleep_ms = g_batchFreq * cycle_ms;
            if (sleep_ms == 0) sleep_ms = cycle_ms;
            this_thread::sleep_for(chrono::milliseconds(static_cast<int>(sleep_ms)));
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
    cout << "------------------------------------\n";
}

// like screen -ls, but report-util saves this into a text file – “csopesylog.txt.” 
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

void cmdScreenList() {
    lock_guard<mutex> lock(processLock);
    
    // temporarily hardcoded
    if (procList.empty()) {
    	cout << "CPU utilization: 0%";
	    cout << "Cores used: 0\n";
	    cout << "Cores available: " << g_numCPU << "\n";
	    cout << "------------------------------------\n";
        cout << "(no processes)\n";
        return;
    }

    int total = (int)procList.size();
    int finished = 0;
    for (auto &kv : procList)
        if (kv.second->isFinished())
            finished++;

    int usedCores = min(total, g_numCPU);
    int available = max(0, g_numCPU - usedCores);
    
    // fix cpu utilization

    cout << "CPU utilization: " << fixed << setprecision(1)
         << (100.0 * usedCores / g_numCPU) << "%\n";
    cout << "Cores used: " << usedCores << "\n";
    cout << "Cores available: " << available << "\n";
    cout << "------------------------------------\n";

    cout << "Running processes:\n";
    int core = 0;
    for (auto &kv : procList) {
        auto &p = kv.second;
        if (!p->isFinished()) {
            time_t now = time(nullptr);
            tm local_tm = *localtime(&now);
            char buf[64];
            strftime(buf, sizeof(buf), "(%m/%d/%Y %I:%M:%S%p)", &local_tm);
            int totalIns = 1000 + (p->getId() * 10);
            int done = rand() % totalIns;
            cout << left << setw(10) << p->getName()
                 << " " << buf
                 << "   Core: " << core++ % g_numCPU
                 << "   " << done << " / " << totalIns << "\n";
        }
    }

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
    cout << "------------------------------------\n";
}

// attach + process-smi
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
        
        // if the process is already finished, this is base on the mc01 specs
        if (p->isFinished()) {
        	cout << "Process "<< p->getName() << " not found.\n";
        	return;
		}
    }

    clearScreen();
    cout << "Attached to process: " << name << "\nType 'process-smi' to view logs, 'exit' to leave.\n"; // remove this after everything is working fine

    string cmd;
    while (true) {
        cout << "proc> ";
        if (!getline(cin, cmd)) break;
        trim(cmd);
        
        if (cmd == "exit") break;
        
        if (cmd == "process-smi") {
            cout << "\nProcess name: " << p->getName() << "\n";
            cout << "ID: " << p->getId() << "\n";
            cout << "Logs:\n";
            auto logs = p->snapshotLogs();
            for (auto &line : logs) cout << line << "\n";
            cout << "\nCurrent instruction line: " << p->getCurrentInstructionLine() << "\n";
            cout << "Lines of code: " << p->getTotalLines() << "\n\n";
            if (p->isFinished()) cout << "(process finished)\n";
        } else cout << "Unknown command.\n";
    }
}

void handleScreenSpawn(const string &rawName) {
    string name = rawName;
    trim(name);

    if (name.empty()) {
        cout << "Usage: screen -s <process-name>\n";
        return;
    }

    {
        lock_guard<mutex> lock(processLock);
        if (procList.find(name) != procList.end()) {
            cout << "Process with name '" << name << "' already exists.\n";
            return;
        }

        random_device rd; mt19937 gen(rd());
        uniform_int_distribution<int> dist(g_minIns, g_maxIns);
        int lines = dist(gen);

        procList[name] = make_unique<Process>(name, next_pid++, lines);
        procList[name]->start();
    }

    attachToProcess(name);
}

// main loop
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
                readConfig();
                initialized = true;
                cout << "Processor initialized.\n";
            } else {
                cout << "Unknown command: call 'initialize' first.\n";
            }
            continue;
        }

        if (line == "scheduler-start") { handleSchedulerStart(); continue; }
        if (line == "scheduler-stop")  { handleSchedulerStop(); continue; }
        if (line == "report-util")     { handleReportUtil(); continue; } // this needs fixing, same as screen -ls but this logs into a text file
        if (line == "screen -ls")      { cmdScreenList(); continue; }

        if (line.rfind("screen -r", 0) == 0) {
            string name = line.substr(10);
            trim(name);
            attachToProcess(name);
            continue;
        }

        if (line.rfind("screen -s", 0) == 0) {
            string name = line.substr(10);
            trim(name);
            handleScreenSpawn(name);
            continue;
        }

        cout << "Unknown command.\n";
    }

    return 0;
}