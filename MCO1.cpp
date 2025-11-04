#include <iostream> // C++
#include <fstream> // reading .txt files
#include <string> // String libraries
#include <sstream> // String stream
#include <thread> // multi threading
#include <chrono> // time
#include <atomic> // atomic operations (thread-safety)
#include <mutex> // mutual exclusion
#include <windows.h> // Windows API call
#include <cstdlib> // C++ version of the stdlib.h
#include <algorithm> //
#include <stdio.h> // C standard input and output library

// header files
#include "MCO1_classes.h" // contains the classes for the OOP parts
#include "MC01_functions.h" // contains the function declarations

int main () {
	
	Emulator CSOPESYemulator;

	startMessage();

	while(CSOPESYemulator.getFlag_runningOS()) {
		while (!CSOPESYemulator.getFlag_initializeFlag() && !CSOPESYemulator.getFlag_exitOS())
			startUp (&CSOPESYemulator);
		if (CSOPESYemulator.getFlag_runningOS() && !CSOPESYemulator.getFlag_exitOS() && !CSOPESYemulator.getFlag_readConfigFlag()) {
			readConfig (&CSOPESYemulator);
			CSOPESYemulator.setFlag_readConfigFlag(true);
		}
		std::this_thread::sleep_for(std::chrono::seconds(1));
	}
	
	std::cout << "Exiting application...";
	return 0;
}