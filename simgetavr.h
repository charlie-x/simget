#ifndef AVRSIMULATOR_H
#define AVRSIMULATOR_H

#include <string>
#include <GLFW/glfw3.h>

#include <iostream>
#include <chrono>
#include <thread>


#include "sim_avr.h"
#include "sim_elf.h"
#include "sim_gdb.h"

class AvrSimulator {
public:
    AvrSimulator();
    ~AvrSimulator();

    bool Initialize(const std::string& mcu_type, const std::string& firmware_file, uint32_t frequency, int gdb_port = 0);
    int Run();
    void Cleanup();
    int RunAnimate();

    void Reset(){
        avr_reset(avr);
    }
    int state;                  // state of avr

    avr_t* avr;                 // pointer to the AVR simulator instance
    
    bool animate = false;
    bool run = false;
    bool step = false;
    
    uint32_t animateDelay = 100;
private:
    std::string mcu_type;       // type of AVR microcontroller to simulate
    std::string firmware_file;  // path to the firmware file
    uint32_t frequency;         // frequency at which the AVR runs
    int gdb_port;               // port for GDB server (0 if not used)


    uint32_t loadBase = AVR_SEGMENT_OFFSET_FLASH;
    elf_firmware_t f = {{0}};

    static void sig_int(int sign); // signal handler for SIGINT/SIGTERM
};

#endif // AVRSIMULATOR_H