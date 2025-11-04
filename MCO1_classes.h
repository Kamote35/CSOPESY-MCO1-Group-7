#ifndef MCO1_CLASSES_H
#define MCO1_CLASSES_H

class Emulator {
	
	// properties
	private:
		// flags
		bool runningOS;
		bool initializeFlag;
		bool exitOS;
		bool readConfigFlag;
		
		// strings
		std::string command;
		
		// processor parameters 
		int nCPU;
		std::string scheduler;
		unsigned int nQuantCycle;
		unsigned int nBatchProcessFreq;
		unsigned int min_Ins;
		unsigned int max_Ins;
		unsigned int delayPerExec;
		
	// methods
	public:
		// constructor
		Emulator () {
			
			// flags initial values
			runningOS = true;
			initializeFlag = false;
			exitOS = false;
			readConfigFlag = false;
			
			// default processor parameters
			nCPU = 4;
			scheduler = "rr";
			nQuantCycle = 5;
			nBatchProcessFreq = 1;
			min_Ins = 1000;
			max_Ins = 2000;
			delayPerExec = 0;
		}
		
		// getters for flags
		bool getFlag_runningOS () {
			return runningOS;
		}
		
		bool getFlag_initializeFlag () {
			return initializeFlag;
		}

		bool getFlag_exitOS () {
			return exitOS;
		}
		
		bool getFlag_readConfigFlag () {
			return readConfigFlag;
		}

		// setters for flags
		void setFlag_runningOS (bool value) {
			runningOS = value;
		}

		void setFlag_initializeFlag (bool value) {
			initializeFlag = value;
		}

		void setFlag_exitOS (bool value) {
			exitOS = value;
		}
		
		void setFlag_readConfigFlag (bool value) {
			readConfigFlag = value;
		}

		// getter for command
		std::string getCommand () {
			return command;
		}

		// setter for command
		void setCommand (std::string value) {
			command = value;
		}

		// getters for processor parameters 
		int get_nCPU () {
			return nCPU;
		}

		std::string get_scheduler () {
			return scheduler;
		}

		unsigned int get_nQuantCycle () {
			return nQuantCycle;
		}

		unsigned int get_nBatchProcessFreq () {
			return nBatchProcessFreq;
		}

		unsigned int get_min_Ins () {
			return min_Ins;
		}

		unsigned int get_max_Ins () {
			return max_Ins;
		}

		unsigned int get_delayPerExec () {
			return delayPerExec;
		}

		// setters for processor parameters

		void set_nCPU (int value) {
			nCPU = value;
		}

		void set_scheduler (std::string value) {
			scheduler = value;
		}

		void set_nQuantCycle (unsigned int value) {
			nQuantCycle = value;
		}

		void set_nBatchProcessFreq (unsigned int value) {
			nBatchProcessFreq = value;
		}

		void set_min_Ins (unsigned int value) {
			min_Ins = value;
		}

		void set_max_Ins (unsigned int value) {
			max_Ins = value;
		}

		void set_delayPerExec (unsigned int value) {
			delayPerExec = value;
		}
};

enum class ProcessState {

	NEW, // Process being created
	READY, // Process is created and waiting to be dispatched
	RUNNING, // Process is currently executing
	WAITING, // Process is waiting for event or I/O
	TERMINATED // Process has finished execution

}

class Process {

	// properties
	private:
		std::string processName;
		unsigned int processID;
		
};

#endif // MCO1_CLASSES_H