#include <argparse/argparse.hpp>
#include "imgui.h"
#include "imgui_stdlib.h"
#include "imgui_internal.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"
#include <GLFW/glfw3.h>

#include <iostream>
#include <string>
#include <vector>
#include <signal.h>

extern "C" {
    #include "simavr/sim/avr_ioport.h"
    #include "simavr/sim/sim_avr.h"
    #include "simavr/sim/avr_uart.h"
    #include "simavr/sim/sim_gdb.h"
    #include "simavr/sim/sim_core.h"
    #include "simavr/sim/sim_vcd_file.h"
    #include "simavr/sim/sim_hex.h"
    #include "simavr/sim/sim_elf.h"
    }


static avr_t * avr = NULL;
uint32_t loadBase = AVR_SEGMENT_OFFSET_FLASH;

void sig_int(int sign) {
    std::cout << "Signal caught, simavr terminating" << std::endl;
    if (avr)
        avr_terminate(avr);
    exit(0);
}



// Function to list supported AVR cores
void list_cores() {

}

void ShowAvrDetailsFull(const avr_t* avr) 
{
    if (!avr) return;

    // Start a new ImGui window
    if (ImGui::Begin("AVR Details Full")) { // Only proceed if the window is open
    ImGui::Text("IO End: %u", avr->ioend);
    ImGui::Text("RAM End: %u", avr->ramend);
    ImGui::Text("Flash End: %u", avr->flashend);
    ImGui::Text("E2 End: %u", avr->e2end);
    ImGui::Text("Vector Size: %u", avr->vector_size);

    ImGui::Text("Fuse:");
    for (int i = 0; i < 6; ++i)
        ImGui::Text("\t[%d]: 0x%X", i, avr->fuse[i]);

    ImGui::Text("Lockbits: 0x%X", avr->lockbits);

    ImGui::Text("Signature:");
    for (int i = 0; i < 3; ++i)
        ImGui::Text("\t[%d]: 0x%X", i, avr->signature[i]);

    ImGui::Text("Serial:");
    for (int i = 0; i < 9; ++i)
        ImGui::Text("\t[%d]: 0x%X", i, avr->serial[i]);

    ImGui::Text("RAMPZ: %u", avr->rampz);
    ImGui::Text("EIND: %u", avr->eind);
    ImGui::Text("Address Size: %u", avr->address_size);

    ImGui::Text("Reset Flags:");
    ImGui::Text("\tPORF: %u", avr->reset_flags.porf.bit);
    ImGui::Text("\tEXTRF: %u", avr->reset_flags.extrf.bit);
    ImGui::Text("\tBORF: %u", avr->reset_flags.borf.bit);
    ImGui::Text("\tWDRF: %u", avr->reset_flags.wdrf.bit);

    ImGui::Text("Code End: %u", avr->codeend);
    ImGui::Text("Run Cycle Count: %llu", avr->run_cycle_count);
    ImGui::Text("Run Cycle Limit: %llu", avr->run_cycle_limit);
    ImGui::Text("Sleep Usec: %u", avr->sleep_usec);
    ImGui::Text("Time Base: %llu", avr->time_base);

    // Custom init/deinit functions and data are not displayed as they are function pointers and context data

    // Skipping run and sleep function pointers for the same reason

    // Skipping irq_pool, sreg, interrupt_state, pc, reset_pc, io, io_shared_io, flash, data, io_port, commands, cycle_timers, interrupts, trace, log, trace_data, vcd, gdb, gdb_port, io_console_buffer, data_names as these are complex types or pointers

    ImGui::End();

    }
}
void DumpAvrRegisters(avr_t* avr) {
    if (!avr) return;

    ImGui::Begin("AVR Register Dump");

    for (int i = 0; i < 32; i++) {
        ImGui::Text("%s=%02x", avr_regname(avr, i), avr->data[i]);
        if ((i % 8) != 7) ImGui::SameLine();
    }

    uint16_t y = avr->data[R_YL] | (avr->data[R_YH] << 8);
    for (int i = 0; i < 20; i++) {
        ImGui::Text("Y+%02d=%02x", i, avr->data[y + i]);
        if (i % 10 != 9) ImGui::SameLine();
    }

    ImGui::End();
}

void ShowAvrDetails(avr_t* avr) 
{
    static bool run = false;

    if (!avr) return;

    // Start a new ImGui window
    if (ImGui::Begin("AVR Details Window")) { // Only proceed if the window is open

        ImGui::Text("MCU: %s", avr->mmcu);
        ImGui::Text("Frequency: %lu Hz", avr->frequency);
        ImGui::Text("Cycle Counter: %lu", avr->cycle);

        ImGui::Text("VCC: %f V", avr->vcc);
        ImGui::Text("AVCC: %f V", avr->avcc);
        ImGui::Text("AREF: %f V", avr->aref);

        ImGui::Text("pc      : %d", avr->pc);
        ImGui::Text("reset pc: %d", avr->reset_pc);
        
        {
            const char * _sreg_bit_name = "cznvshti";
            char buffer[32];
           	for (int _sbi = 0; _sbi < 8; _sbi++) {
                sprintf(buffer,"%c", avr->sreg[_sbi] ? toupper(_sreg_bit_name[_sbi]) : '.');
            }
            ImGui::Text("sreg: %s",buffer);
        }
        static int state ;

        // Simavr run cycle
        if( run ) {
            state = avr_run(avr);
        }

        ImGui::Checkbox("run",&run);

        if(ImGui::Button("step")){
            state = avr_run(avr);
        }

        if(ImGui::Button("reset")){
            avr_reset(avr);
        }

        ImGui::Text("STATE: %d ", state);
        if (state == cpu_Done || state == cpu_Crashed){
                ImGui::Text("DONE/CRASHED");
        };        ImGui::End(); // End of AVR Details Window
    }
}


	elf_firmware_t f = {{0}};

int main(int argc, char** argv) {
    argparse::ArgumentParser program("SimAVR with ImGui");

    program.add_argument("--list-cores")
           .default_value(false)
           .implicit_value(true)
           .help("List all supported AVR cores and exit");

    program.add_argument("--mcu", "-m")
           .default_value(std::string("attiny4313"))
           .help("Sets the MCU type for an .hex firmware");

    program.add_argument("--freq", "-f")
           .scan<'i', int>()
           .default_value(1000000)
           .help("Sets the frequency for an .hex firmware");

    program.add_argument("--gdb", "-g")
           .default_value(0)
           .implicit_value(1234)
           .help("Listen for gdb connection on <port> (default 1234)");

    program.add_argument("--firmware")
           .required()
           .help("Path to the firmware file for simulation (ELF or hex)");

    program.add_argument("--input", "-i")
           .default_value("")
           .help("A VCD file to use as input signals");

    program.add_argument("--output", "-o")
           .default_value("")
           .help("A VCD file to save the traced signals");

    program.add_argument("--trace", "-t")
           .default_value(false)
           .implicit_value(true)
           .help("Run full scale decoder trace");

    program.add_argument("--add-trace", "-at")
           .default_value("")
           .help("Add signal to be included in VCD output (format: name=kind@addr/mask)");

    try {
        program.parse_args(argc, argv);
    }
    catch (const std::runtime_error& err) {
        std::cerr << err.what() << std::endl;
        std::cerr << program;
        return 1;
    }

    if (program["--list-cores"] == true) {
        list_cores();
        return 0;
    }

    std::string mcu = program.get<std::string>("--mcu");
    int frequency = program.get<int>("--freq");
    int gdb_port = program.get<int>("--gdb");
    std::string firmware_file = program.get<std::string>("--firmware");
    std::string vcd_input = program.get<std::string>("--input");
    std::string vcd_output = program.get<std::string>("--output");
    bool trace = program.get<bool>("--trace");
    std::string add_trace = program.get<std::string>("--add-trace");

    // Initialize GLFW for ImGui
    if (!glfwInit()) {
        std::cerr << "Failed to initialize GLFW" << std::endl;
        return 1;
    }

    GLFWwindow* window = glfwCreateWindow(640, 480, "SimAVR with ImGui", NULL, NULL);
    if (!window) {
        std::cerr << "Failed to create GLFW window" << std::endl;
        glfwTerminate();
        return 1;
    }

    glfwMakeContextCurrent(window);

    // Setup ImGui context
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO(); (void)io;

    // Setup ImGui + GLFW binding
    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init("#version 120");

    printf("OpenGL version supported by this platform (%s): \n", glGetString(GL_VERSION));

	sim_setup_firmware(firmware_file.c_str(), loadBase, &f, argv[0]);


    // Initialize simavr
    avr = avr_make_mcu_by_name(mcu.c_str());
    if (!avr) {
        std::cerr << "AVR '" << mcu << "' not known" << std::endl;
        return 1;
    }

    avr_init(avr);
    avr->frequency = frequency;

	if (mcu.length() )
		strcpy(f.mmcu, mcu.c_str());
	if (frequency)
		f.frequency = frequency;

    // Load firmware (ELF or hex)
    avr_load_firmware(avr, &f);
    
    // Setup GDB if specified
    if (gdb_port) {
        avr->gdb_port = gdb_port;
        avr_gdb_init(avr);
    }

    // Setup signal handlers
    signal(SIGINT, sig_int);
    signal(SIGTERM, sig_int);

    // Handle additional arguments like VCD input/output, trace, add-trace
    // Implement the logic to handle these arguments as per your application's requirements

    // Main loop
    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();

        // Start ImGui frame
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        ShowAvrDetails(avr);
        ShowAvrDetailsFull(avr);
        DumpAvrRegisters(avr);

       // Rendering
        ImGui::Render();
        int display_w, display_h;
        glfwGetFramebufferSize(window, &display_w, &display_h);
        glViewport(0, 0, display_w, display_h);
        glClearColor(0.45f, 0.55f, 0.60f, 1.00f);
        glClear(GL_COLOR_BUFFER_BIT);
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());


        glfwSwapBuffers(window);
    }

    // Cleanup
    avr_terminate(avr);
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();

    glfwDestroyWindow(window);
    glfwTerminate();

    return 0;
}
