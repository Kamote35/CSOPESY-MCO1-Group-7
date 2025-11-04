#ifndef MC01_FUNCTIONS_H
#define MC01_FUNCTIONS_H

// Start message to be displayed in the beginning of the Emulator program
void startMessage () {
	using namespace std;
	cout << "-----------------------------------------------\n";
	cout << "WELCOME to CSOPESY Emulator!\n";
	
	cout << "Group 7 developers:\n";
	cout << "Corpuz, Gerald Justine\n";
	cout << "De Jesus, Andrei Zarmin\n";
	cout << "Manaois, Chriscel John\n";
	cout << "Sayat, John Christian\n\n";
	
	cout << "Last updated: 11-04-2025\n";
	cout << "Version date: 4.0\n";
	cout << "-----------------------------------------------\n";
}

// The step before actually initializing the processor configuration
// Modifies the original object properties through pointers 
void startUp (Emulator* emulator) {
	// string pointers for command input
	std::string command;

	// Prompt user for command input
		std::cout << "\nroot:\\> ";
		std::cin >> command;
		emulator->setCommand(command);
		
		if (emulator->getCommand() == "initialize") {
			std::cout << "Initializing processor configuration...";
			//*initializeFlag = true;
			emulator->setFlag_initializeFlag(true);
		}
		else if (emulator->getCommand() == "exit") {
			// *runningOS = false;
			// *exitOS = true;

			emulator->setFlag_runningOS(false);
			emulator->setFlag_exitOS(true);
		}
		else {
			std::cout << "Must \"initialize\" the processor configuration first!\n";
			std::cout << "Enter \"exit\" to exit application.\n";
		}
} 

// reads the config.txt to initialize the Process Multiplexer
// Modifies the original object properties through pointers
void readConfig (Emulator* emulator) {
	std::ifstream configFile;
	
	configFile.open("config.txt");
	
	if (!configFile.is_open()) {
		std::cerr << "Error: Could not open config.txt!" << std::endl;
		std::cout << "Using default parameters...\n";

		return;
	}
	
	std::string line;
	while (std::getline(configFile, line)) {
		std::stringstream ss(line);
		std::string key;
		std::string value;
		
		if (ss >> key >> value) // check the token and the value in the space separated config .txt file
			if (key == "num-cpu") 
				emulator->set_nCPU(std::stoi(value));
				
			else if (key == "scheduler" ) {
				value.erase(std::remove(value.begin(), value.end(), '\"'), value.end());
				emulator->set_scheduler(value);
			}
			else if (key == "quantum-cycles")
				emulator->set_nQuantCycle(std::stoul(value));

			else if (key == "batch-process-freq") 
				emulator->set_nBatchProcessFreq(std::stoul(value));

			else if (key == "min-ins") 
				emulator->set_min_Ins(std::stoul(value));

			else if (key == "max-ins") 
				emulator->set_max_Ins(std::stoul(value));

			else if (key == "delay-per-exec" )
				emulator->set_delayPerExec(std::stoul(value));
	}
	configFile.close();
}

#endif // MC01_FUNCTIONS_H