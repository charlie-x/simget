#include <signal.h>
#include <iostream>
#include <string.h>

#include "simgetavr.h"
#include "sim_hex.h"

AvrSimulator::AvrSimulator()
: mcu_type(""),
  firmware_file(""),
  frequency(0),
  gdb_port(0),
  animate(false),
  run(false),
  step(false),
  avr(nullptr)
{
}

void AvrSimulator::Cleanup()
{
    if(avr) {
        avr_terminate(avr);
    }
}

bool AvrSimulator::Initialize(const std::string& mcu_type, const std::string& firmware_file, uint32_t frequency, int gdb_port)
{

    sim_setup_firmware(firmware_file.c_str(), loadBase, &f, "");

    // initialize simavr
    avr = avr_make_mcu_by_name(mcu_type.c_str());
    if (!avr) {
        std::cerr << "AVR '" << mcu_type << "' not known" << std::endl;
        return false;
    }

    avr_init(avr);
    avr->frequency = frequency;

	if (mcu_type.length() ){
		strcpy(f.mmcu, mcu_type.c_str());
    }

	if (frequency){
		f.frequency = frequency;
    }

    // load firmware (ELF or hex)
    avr_load_firmware(avr, &f);
    
    // setup GDB if specified
    if (gdb_port) {
        avr->gdb_port = gdb_port;
        avr_gdb_init(avr);
    }

    return true;
}

AvrSimulator::~AvrSimulator()
{
    Cleanup(); // Ensure all resources are released
}

void AvrSimulator::sig_int(int sign)
{
    // Signal handler code to gracefully exit the simulation
    // This function can set a flag checked in the main loop to exit
    if (sign == SIGINT || sign == SIGTERM) {
        // Handle interrupt signal (e.g., Ctrl+C)
        std::cout << "Interrupt signal (" << sign << ") received. Exiting..." << std::endl;
    }
}

int AvrSimulator::Run()
{
    if (!avr) {
        std::cerr << "AVR Simulator not initialized." << std::endl;
        return -1;
    }

    if( animate ) {
        
        RunAnimate();

    } else if( run ) {

        state = avr_run(avr);
    }

    return state;
}

int AvrSimulator::RunAnimate() 
{
    static std::chrono::steady_clock::time_point lastCall = std::chrono::steady_clock::now();
    auto now = std::chrono::steady_clock::now();
    
    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - lastCall);
    
    if ( elapsed.count() > animateDelay ) {
        
        state = avr_run(avr);

        // update the time of the last call to the current time
        lastCall = std::chrono::steady_clock::now();

    } else {
        return 0;
    }

    return state;
}
