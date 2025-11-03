#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
using namespace std;

// Struct to store the configuration
// Can make this one a class
struct ProcessorConfig {
    int numCpu;
    string scheduler;
    int quantumCycles;
    int batchProcessFreq;
    int minIns;
    int maxIns;
    int delayPerExec;
};

void mainMenu() {
    cout << "\n=== MAIN MENU ===\n";
    cout << "initialize\n";
    cout << "exit\n";
    cout << "screen\n";
    cout << "scheduler-start\n";
    cout << "scheduler-stop\n";
    cout << "report-util\n";
    cout << "=================\n";
}

bool readConfig(ProcessorConfig &config) {
    ifstream file("config.txt");
    if (!file.is_open()) {
        cerr << "Error: Could not open config.txt\n";
        return false;
    }

    string key, value;
    while (file >> key >> value) {
        if (key == "num-cpu") config.numCpu = stoi(value);
        else if (key == "scheduler") {
            // remove quotes if present
            if (value.front() == '"' && value.back() == '"')
                value = value.substr(1, value.size() - 2);
            config.scheduler = value;
        }
        else if (key == "quantum-cycles") config.quantumCycles = stoi(value);
        else if (key == "batch-process-freq") config.batchProcessFreq = stoi(value);
        else if (key == "min-ins") config.minIns = stoi(value);
        else if (key == "max-ins") config.maxIns = stoi(value);
        else if (key == "delay-per-exec") config.delayPerExec = stoi(value);
    }

    file.close();
    return true;
}

int main() {
    bool isInitialized = false;
    ProcessorConfig config;
    string command;

    while (true) {
        mainMenu();
        cout << "\nEnter command: ";
        cin >> command;

        if (command == "exit") {
            cout << "Exiting program...\n";
            break;
        }

        if (command == "initialize") {
            if (readConfig(config)) {
                isInitialized = true;
                cout << "Processor configuration initialized successfully!\n";
            } else {
                cout << "Failed to initialize configuration.\n";
            }
        } 
        else if (!isInitialized) {
            cout << "Processor configuration is not yet initialized.\n";
        } 
        else {
            if (command == "screen") {
                cout << "\n--- Current Configuration ---\n";
                cout << "num-cpu: " << config.numCpu << endl;
                cout << "scheduler: " << config.scheduler << endl;
                cout << "quantum-cycles: " << config.quantumCycles << endl;
                cout << "batch-process-freq: " << config.batchProcessFreq << endl;
                cout << "min-ins: " << config.minIns << endl;
                cout << "max-ins: " << config.maxIns << endl;
                cout << "delay-per-exec: " << config.delayPerExec << endl;
                cout << "------------------------------\n";
            } 
            else if (command == "scheduler-start") {
                cout << "Starting scheduler (" << config.scheduler << ")...\n";
            } 
            else if (command == "scheduler-stop") {
                cout << "Stopping scheduler...\n";
            } 
            else if (command == "report-util") {
                cout << "Generating utilization report...\n";
            } 
            else {
                cout << "Unknown command.\n";
            }
        }
    }

    return 0;
}

