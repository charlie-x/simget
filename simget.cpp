#include <argparse/argparse.hpp>
#include "imgui.h"
#include "imgui_stdlib.h"
#include "imgui_internal.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"
#include <GLFW/glfw3.h>

// comes from the imgui_club repo
#include "imgui_memory_editor.h"

#include <iostream>
#include <string>
#include <vector>
#include <signal.h>

#include "simgetavr.h"

extern "C" {
    #include "simavr/sim/avr_ioport.h"
    #include "simavr/sim/sim_avr.h"
    #include "simavr/sim/avr_uart.h"
    #include "simavr/sim/sim_gdb.h"
    #include "simavr/sim/sim_core.h"
    #include "simavr/sim/sim_vcd_file.h"
    #include "simavr/sim/sim_hex.h"
    #include "simavr/sim/sim_elf.h"
    #include "simavr/sim/sim_mcu_structs.h"


    #include "Globals.h"
    #include "Callbacks_Assembly.h"
    #include "Callbacks_PseudoCode.h"
    #include "Options.h"
    #include "JumpCall.h"
    #include "IORegisters.h"
    #include "MNemonics.h"
    #include "Tagfile.h"

}

int setupAVRDisasm();
void Disasm(char *Bitstream, int Pos, int Read);

void sig_int(int sign) {

    std::cout << "Signal caught, simavr terminating" << std::endl;

    exit(0);
}


//avrdisam

static int Number_Opcodes = 0;
static struct Opcode Opcodes[256];
static int Registers[256];

struct Options Options;
static char Code_Line[256];
static char Comment_Line[256];
static char After_Code_Line[256];

//

// Function to list supported AVR cores
void list_cores() {

}

void ShowAvrDetailsFull(AvrSimulator& avrSim) 
{
    avr_t * avr = avrSim.avr;

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
    ImGui::Text("\t0x%X%X%X", avr->signature[0], avr->signature[1],avr->signature[2]);

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
    ImGui::Text("Run Cycle Count: %lu", avr->run_cycle_count);
    ImGui::Text("Run Cycle Limit: %lu", avr->run_cycle_limit);
    ImGui::Text("Sleep Usec: %u", avr->sleep_usec);
    ImGui::Text("Time Base: %lu", avr->time_base);

    // Custom init/deinit functions and data are not displayed as they are function pointers and context data

    // Skipping run and sleep function pointers for the same reason

    // Skipping irq_pool, sreg, interrupt_state, pc, reset_pc, io, io_shared_io, flash, data, io_port, commands, cycle_timers, interrupts, trace, log, trace_data, vcd, gdb, gdb_port, io_console_buffer, data_names as these are complex types or pointers

    ImGui::End();

    }
}

void HexEditor( AvrSimulator& avrSim, bool run){

   static MemoryEditor mem_edit_1;

   avr_t * avr = avrSim.avr;
    
   if(!avr) return;

   static size_t  start ;

    // only chase if not pc changed
    {

        if ( start!= avr->pc ) {

            size_t end  = avr->pc+1;
            start = avr->pc;

            mem_edit_1.HighlightPC( start, end );
        }
    }

   mem_edit_1.DrawWindow("Memory Editor FLASH", avr->flash, avr->flashend);
}

void HexEditorRAM( AvrSimulator& avrSim ) {

    avr_t * avr = avrSim.avr;
    
   static MemoryEditor mem_edit_1;
   mem_edit_1.DrawWindow("Memory Editor RAM", avr->data , avr->ramend);
}

void displayIO(AvrSimulator& avrSim, const std::string &io_type, uint8_t addr , char *cname)
{
	avr_t *avr  = avrSim.avr;

   // display register name, address, and current value
    ImGui::Text("%s%s (0x%02X) 0x%02X",io_type.c_str(), cname , addr, avrSim.avr->data[addr]);
    //ImGui::SameLine();

    // calculate spacing based on window width
    float windowWidth = ImGui::GetWindowWidth();
    float spacing = windowWidth / 9; // 8 bits + label

    // display the bit numbers
    ImGui::SetCursorPosX( spacing); // Initial spacing for alignment
    for (int i = 7; i >= 0; --i) {
        ImGui::Text("%d", i);
        if (i > 0) {
            ImGui::SameLine();
            ImGui::SetCursorPosX( ( spacing * (8 - i + 1 ))); 
        }
    }
            
    // put text in box        
    ImGui::SameLine();

    std::string name ;

    // display the checkboxes for each bit
    for (int i = 7; i >= 0; --i) {
        ImGui::SetCursorPosX( (spacing * (8 - i))); // adjust for checkbox size
        bool bitSet = avrSim.avr->data[addr] & (1 << i);
        name = std::string("##") + io_type + cname+ std::to_string(i) ;

        if (ImGui::Checkbox(name.c_str(), &bitSet)) {

			if( io_type == "PIN" ) {

				avr_irq_t* irq = avr_io_getirq(avr, AVR_IOCTL_IOPORT_GETIRQ(cname[0]), i);

				if(irq) {
					if (bitSet) {

						avr_raise_irq(irq, 1); // Simulate a high state on the pin

					}else {

						avr_raise_irq(irq, 0); // Simulate a low state on the pin
					}
				}	
			} else{
				// toggle the bit if the checkbox is clicked
				if (bitSet) {
					avrSim.avr->data[addr] |= (1 << i);
				} else {
					avrSim.avr->data[addr] &= ~(1 << i);
				}
				}
        }
        if (i > 0) {
            ImGui::SameLine();
        }
    }
}

void ModifyAvrIoRegister(AvrSimulator& avrSim, avr_ioport_t*port) 
{
    if (!port) return;
    
    char cname[2] = {0,0,};
    cname[0]=port->name;

    displayIO(avrSim,"PORT", port->r_port, cname);
    displayIO(avrSim,"DDR", port->r_ddr, cname);
    displayIO(avrSim,"PIN", port->r_pin, cname);
}


void ModifyAvrIoRegisters(AvrSimulator& avrSim) 
{
    avr_t * avr = avrSim.avr;
    
    if(!avr) return;
    
    // recast to mcu type
    mcu_attiny4313_t *mcu = (mcu_attiny4313_t*)avr;

    ImGui::Begin("AVR IO Register Control");

    ModifyAvrIoRegister(avrSim, &mcu->porta);
    ModifyAvrIoRegister(avrSim, &mcu->portb);
    ModifyAvrIoRegister(avrSim, &mcu->portd);

    ImGui::End();

}

const char* GetAvrStateName(int state) {
    switch (state) {
        case cpu_Limbo: return "Limbo";
        case cpu_Stopped: return "Stopped";
        case cpu_Running: return "Running";
        case cpu_Sleeping: return "Sleeping";
        case cpu_Step: return "Step";
        case cpu_StepDone: return "Step Done";
        case cpu_Done: return "Done";
        case cpu_Crashed: return "Crashed";
        default: return "Unknown";
    }
}

void ShowAvrState(const int state) 
{
    
    ImGui::Text("State: %s", GetAvrStateName( state ));
}

void DumpAvrRegisters(AvrSimulator &avrSim) {
    
    avr_t * avr = avrSim.avr;
    
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

bool  ShowAvrDisasm(AvrSimulator &avr) 
{    
    if (ImGui::Begin("AVR Disasm Window")) { 
        Disasm((char*)avr.avr->flash,avr.avr->pc,4096);
        ImGui::End();
    }
    return true;
}

bool  ShowAvrDetails(AvrSimulator &avr) 
{
    // Start a new ImGui window
    if (ImGui::Begin("AVR Details Window")) { // Only proceed if the window is open

        ImGui::Text("MCU: %s", avr.avr->mmcu);
        ImGui::Text("Frequency: %d Hz", avr.avr->frequency);
        ImGui::Text("Cycle Counter: %lu", avr.avr->cycle);

        ImGui::Text("VCC: %d V", avr.avr->vcc);
        ImGui::Text("AVCC: %d V", avr.avr->avcc);
        ImGui::Text("AREF: %d V", avr.avr->aref);

        ImGui::Text("pc:       0x%x", avr.avr->pc);
        ImGui::Text("reset pc: 0x%x", avr.avr->reset_pc);
        
        {
            const char * _sreg_bit_name = "cznvshti";
            char buffer[32];
           	for (int _sbi = 0; _sbi < 8; _sbi++) {
                sprintf(buffer,"%c", avr.avr->sreg[_sbi] ? toupper(_sreg_bit_name[_sbi]) : '.');
            }
            ImGui::Text("sreg: %s",buffer);
        }

        ImGui::Checkbox("run",&avr.run);
        ImGui::Checkbox("animate",&avr.animate);

        if(ImGui::Button("step")){
            avr.RunAnimate();
        } 

        if(ImGui::Button("reset")){
            avr.Reset();
        }


        ShowAvrState(avr.state);

        if (avr.state == cpu_Done || avr.state == cpu_Crashed){
                ImGui::Text("DONE/CRASHED");
        };
        
        ImGui::End(); // End of AVR Details Window
    }

    return avr.run;
}

void drawCircle(GLfloat x, GLfloat y, GLfloat radius, const ImVec4 &color, int numSegments = 32) {
    GLfloat twicePi = 2.0f * M_PI;

    // Activate the OpenGL context
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    // Start drawing a triangle fan
    glBegin(GL_TRIANGLE_FAN);

    // Set the color for the circle
    glColor4f(color.x, color.y, color.z, color.w);

    // Center of the circle
    glVertex2f(x, y);

    for (int i = 0; i <= numSegments; i++) {
        glVertex2f(
            x + (radius * cos(i *  twicePi / numSegments)),
            y + (radius * sin(i * twicePi / numSegments))
        );
    }

    glEnd();
}


const int LED_COUNT = 12;
const int OFFSET = 10; // Offset for the first LED from the center
const double PI = 3.14159265358979323846;

class FidgetSpinner {
public:
    FidgetSpinner();
    void update();
    void draw();
    void calculateRPM();
    void sineWaveEffect();

private:
    double angle; // Current angle of rotation in radians
    std::vector<std::pair<float, float>> ledPositions;
    std::vector<bool> ledStates; // LED states: true for on, false for off
    std::chrono::steady_clock::time_point lastTDC;
    double rpm;
    int waveIndex;
    void calculateLedPositions();
    void updateLedStates();
};

FidgetSpinner::FidgetSpinner() : angle(0), rpm(0), waveIndex(0) {
    ledStates.resize(LED_COUNT, false);
    lastTDC = std::chrono::steady_clock::now();
    calculateLedPositions();
    updateLedStates();
}

void FidgetSpinner::update() {
    angle += PI / 120;
    if (angle >= 2 * PI) {
        angle -= 2 * PI;
        calculateRPM();
    }
    calculateLedPositions();
    updateLedStates();
    sineWaveEffect();
}

void FidgetSpinner::calculateLedPositions() {
    ledPositions.clear();
    for (int i = 0; i < LED_COUNT; ++i) {
        float x = (i + OFFSET) * cos(angle);
        float y = (i + OFFSET) * sin(angle);
        ledPositions.emplace_back(x, y);
    }
}

void FidgetSpinner::updateLedStates() {
    static int currentLed = 0;
    ledStates[currentLed] = true;
    currentLed = (currentLed + 1) % LED_COUNT;
    ledStates[currentLed] = false;
}

void FidgetSpinner::calculateRPM() {
    auto now = std::chrono::steady_clock::now();
    std::chrono::duration<double> timePassed = now - lastTDC;
    if (timePassed.count() > 0) {
        rpm = 60 / timePassed.count();
        lastTDC = now;
    }
}

void FidgetSpinner::sineWaveEffect() {
    const int sineWaveSize = LED_COUNT * 2;
    for (int i = 0; i < LED_COUNT; ++i) {
        int index = (waveIndex + i) % sineWaveSize;
        ledStates[i] = index < LED_COUNT;
    }
    waveIndex = (waveIndex + 1) % sineWaveSize;
}

void FidgetSpinner::draw() {
    for (size_t i = 0; i < ledPositions.size(); ++i) {
        float x = ledPositions[i].first * 20 + 400;
        float y = ledPositions[i].second * 20 + 300;
        bool ledOn = ledStates[i];
        ImVec4 color = ledOn ? ImVec4(1.0f, 0.0f, 0.0f, 1.0f) : ImVec4(0.0f, 0.0f, 0.0f, 0.0f);
        drawCircle(x, y, 5.0f, color);
    }

    ImGui::Text("RPM: %d", static_cast<int>(rpm));
}

FidgetSpinner spinner;

void renderLEDsInImGuiWindow() {
    // Start an ImGui window
    ImGui::Begin("LED Control");
	
    // Get the window position and size for LED rendering
    ImVec2 winPos = ImGui::GetWindowPos();
    ImVec2 winSize = ImGui::GetWindowSize();

    // Calculate the center position for the LEDs
    ImVec2 center = ImVec2(winPos.x + winSize.x / 2, winPos.y + winSize.y / 2);

    spinner.update();
	spinner.draw();
    
	// End the ImGui window
    ImGui::End();
}


int main(int argc, char** argv) {

    AvrSimulator avrSim;

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

    GLFWwindow* window = glfwCreateWindow(900, 840, "SimAVR", NULL, NULL);
    if (!window) {
        std::cerr << "Failed to create GLFW window" << std::endl;
        glfwTerminate();
        return 1;
    }

    glfwMakeContextCurrent(window);

    // setup ImGui context
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO(); (void)io;

    // setup ImGui + GLFW binding
    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init("#version 120");

    printf("OpenGL version supported by this platform (%s): \n", glGetString(GL_VERSION));

    // setup the disasm
    setupAVRDisasm();

    avrSim.Initialize( mcu, firmware_file, frequency,gdb_port);

    // Setup signal handlers
    signal(SIGINT, sig_int);
    signal(SIGTERM, sig_int);


    // Start a new thread and use a lambda to call the run() method on the Avr instance
    std::thread avrThread([&avrSim]() {
        while(1) {
            if (avrSim.run || avrSim.animate) {
                avrSim.Run();
            }

            std::this_thread::yield();
        }
    });

    // Main loop
    while (!glfwWindowShouldClose(window)) {

        glfwPollEvents();

        // Start ImGui frame
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();



        bool run =  ShowAvrDetails(avrSim);

        ShowAvrDisasm(avrSim);

        ShowAvrDetailsFull(avrSim);
        DumpAvrRegisters(avrSim);

        HexEditor(avrSim,run);
        HexEditorRAM(avrSim);

        ModifyAvrIoRegisters(avrSim);

		renderLEDsInImGuiWindow();

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
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();

    glfwDestroyWindow(window);
    glfwTerminate();

    return 0;
}


///////avrdisasm


void Display_Registers() {
	int i;
	printf("Register dump:\n");
	for (i = 0; i < 256; i++) {
		if (Registers[i] != 0) {
			printf("Registers[%3d] '%c': %d = 0x%x\n", i, i, Registers[i], Registers[i]);
		}
	}
	printf("End of register dump.\n");
}

int Compare_Opcode(char *Bitstream, char *Bitmask) {
	size_t i;
	char Bit;

	for (i = 0; i < strlen(Bitmask); i++) {
		if ((Bitmask[i] != 'x') && (Bitmask[i] != '1') && (Bitmask[i] != '0')) {
			fprintf(stderr, "Invalid Bitmask!\n");
			return 0;
		}
		
		if (Bitmask[i] == 'x') continue;	/* Ignore character */
		/* Retrieve the i-th Bit of Bitstream */
		Bit = (Bitstream[i / 8] >> (7 - (i % 8))) & 1;
/*		printf("Bit %d is %d [should be %c]\n",i,Bit,Bitmask[i]); */
		if ((Bitmask[i] == '1') && (Bit == 1)) continue;
		if ((Bitmask[i] == '0') && (Bit == 0)) continue;
		return 0;	/* No match */
	}
	return 1;		/* Match */
}

void Register_Opcode(void (*Callback)(char*, int, int), const char *New_Opcode_String, int New_MNemonic) {
	Number_Opcodes++;
	Opcodes[Number_Opcodes-1].Opcode_String = (char*)malloc(strlen(New_Opcode_String) + 1);
	strcpy(Opcodes[Number_Opcodes-1].Opcode_String, New_Opcode_String);
	Opcodes[Number_Opcodes-1].MNemonic = New_MNemonic;
	Opcodes[Number_Opcodes-1].Callback = Callback;
}

void Supersede_Opcode(void (*Callback)(char*, int, int), int New_MNemonic) {
	int i;
	for (i = 0; i < Number_Opcodes; i++) {
		if (Opcodes[i].MNemonic == New_MNemonic) {
			/* Supersede callback */
			Opcodes[i].Callback = Callback;
			return;
		}
	}
	fprintf(stderr, "Error: No callback to supersede opcode %d found (%s).\n", New_MNemonic, MNemonic[New_MNemonic]);
}

int Get_Bitmask_Length(char *Bitmask) {
	int Length = 0;
	size_t i;
	for (i = 0; i < strlen(Bitmask); i++) {
		if (Bitmask[i] != ' ') Length++;
	}
	return Length;
}

void Clear_Registers() {
	int i;
	for (i = 0; i < 256; i++) Registers[i] = 0;
}

char Get_From_Bitmask(char *Bitmask, int Byte, int Bit) {
	size_t i;
	int Cnt = 0;
	int GetBit;
	GetBit = (Byte * 8) + Bit;
	for (i = 0; i < strlen(Bitmask); i++) {
		if (Bitmask[i] != ' ') {
			if (Cnt == GetBit) return Bitmask[i];
			Cnt++;
		}
	}
	return '?';
}

void Display_Binary(char *Bitstream, int Count) {
	int i, j;
	for (i = 0; i < Count; i++) {
		for (j = 7; j >= 0; j--) {
			if ((Bitstream[i] & (1 << j)) != 0) printf("1");
				else printf("0");
			if (j == 4) printf(" ");
		}
		printf("  ");
		if ((((i + 1) % 2) == 0) && (i != 0)) printf("  ");
	}
	printf("\n");
}


int Match_Opcode(char *Bitmask, char *Bitstream) {
	int i;
	int Length;
	int Byte_Mask, Bit_Mask;
	int Byte_Stream, Bit_Stream;
	char Mask_Val, Stream_Val;
	
	Clear_Registers();
	Length = Get_Bitmask_Length(Bitmask);
	
	for (i = 0; i < Length; i++) {
		Byte_Mask = i / 8;
		Bit_Mask = i % 8;
		
		Byte_Stream = i / 8;
		Byte_Stream ^= 1;	/* Invert last bit */
		Bit_Stream = 7 - (i % 8);
		
		Mask_Val = Get_From_Bitmask(Bitmask, Byte_Mask, Bit_Mask);
		Stream_Val = (Bitstream[Byte_Stream] >> Bit_Stream) & 0x01;
		
/*		printf("Extracting Bit %2d: Maske = (%d, %d) [%c], Stream = (%d, %d) [%d] ",i,Byte_Mask,Bit_Mask,Mask_Val,Byte_Stream,Bit_Stream,Stream_Val); */
		if ((Mask_Val == '0') || (Mask_Val == '1')) {
			/* This Bit is a identification Bit */
			if (Mask_Val == '0') {
				if (Stream_Val == 1) {
/*					printf("\nMatch failed.\n");*/
					return 0;
				}
			} else {
				if (Stream_Val == 0) {
/*					printf("\nMatch failed.\n");*/
					return 0;
				}
			}
		} else {
			/* This Bit is a register Bit, set in appropriate place */
			Registers[(int)Mask_Val] <<= 1;
			Registers[(int)Mask_Val] |= Stream_Val;
/*			printf("-> %d Stored [%x]",Stream_Val,Registers[(int)Mask_Val]); */
		}
/*		printf("\n"); */
	}
	return 1;
}

int Get_Next_Opcode(char *Bitstream) {
	int i;
	for (i = 0; i < Number_Opcodes; i++) {
		if (Match_Opcode(Opcodes[i].Opcode_String, Bitstream) == 1) {
			return i;
		}
	}
	return -1;
}

int Get_Specifity(char *Opcode) {
	size_t i;
	int Specifity = 0;
	for (i = 0; i < strlen(Opcode); i++) {
		if ((Opcode[i] == '0') || (Opcode[i] == '1')) Specifity++;
	}
	return Specifity;
}

int Comparison(const void *Element1, const void *Element2) {
	struct Opcode *OC1, *OC2;
	int SP1, SP2;
	OC1 = (struct Opcode*)Element1;
	OC2 = (struct Opcode*)Element2;
	SP1 = Get_Specifity(OC1->Opcode_String);
	SP2 = Get_Specifity(OC2->Opcode_String);
	if (SP1 < SP2) return 1;
		else if (SP2 == SP1) return 0;
	return -1;
}

int setupAVRDisasm()
{
	Options_Default(&Options);
//	if (!Options_ParseCmdLine(&Options, argc, argv)) return 1;

    Options.Show_Addresses = true;

	Activate_Callbacks(Code_Line, Comment_Line, After_Code_Line, Registers, &Options);
	Activate_PC_Callbacks(Code_Line, Comment_Line, After_Code_Line, Registers, &Options);
	Register_Opcode(adc_Callback,		"0001 11rd  dddd rrrr",								OPCODE_adc);
	Register_Opcode(add_Callback,		"0000 11rd  dddd rrrr",								OPCODE_add);
	Register_Opcode(adiw_Callback,		"1001 0110  KKdd KKKK",								OPCODE_adiw);
	Register_Opcode(and_Callback,		"0010 00rd  dddd rrrr",								OPCODE_and);
	Register_Opcode(andi_Callback,		"0111 KKKK  dddd KKKK",								OPCODE_andi);
	Register_Opcode(asr_Callback,		"1001 010d  dddd 0101",								OPCODE_asr);
	Register_Opcode(bclr_Callback,		"1001 0100  1sss 1000",								OPCODE_bclr);
	Register_Opcode(bld_Callback,		"1111 100d  dddd 0bbb",								OPCODE_bld);
	Register_Opcode(brbc_Callback,		"1111 01kk  kkkk ksss",								OPCODE_brbc);
	Register_Opcode(brbs_Callback,		"1111 00kk  kkkk ksss",								OPCODE_brbs);
	Register_Opcode(brcc_Callback,		"1111 01kk  kkkk k000",								OPCODE_brcc);
	Register_Opcode(brcs_Callback,		"1111 00kk  kkkk k000",								OPCODE_brcs);
	Register_Opcode(break_Callback,		"1001 0101  1001 1000",								OPCODE_break);
	Register_Opcode(breq_Callback,		"1111 00kk  kkkk k001",								OPCODE_breq);
	Register_Opcode(brge_Callback,		"1111 01kk  kkkk k100",								OPCODE_brge);
	Register_Opcode(brhc_Callback,		"1111 01kk  kkkk k101",								OPCODE_brhc);
	Register_Opcode(brhs_Callback,		"1111 00kk  kkkk k101",								OPCODE_brhs);
	Register_Opcode(brid_Callback,		"1111 01kk  kkkk k111",								OPCODE_brid);
	Register_Opcode(brie_Callback,		"1111 00kk  kkkk k111",								OPCODE_brie);
	Register_Opcode(brlo_Callback,		"1111 00kk  kkkk k000",								OPCODE_brlo);
	Register_Opcode(brlt_Callback,		"1111 00kk  kkkk k100",								OPCODE_brlt);
	Register_Opcode(brmi_Callback,		"1111 00kk  kkkk k010",								OPCODE_brmi);
	Register_Opcode(brne_Callback,		"1111 01kk  kkkk k001",								OPCODE_brne);
	Register_Opcode(brpl_Callback,		"1111 01kk  kkkk k010",								OPCODE_brpl);
	Register_Opcode(brsh_Callback,		"1111 01kk  kkkk k000",								OPCODE_brsh);
	Register_Opcode(brtc_Callback,		"1111 01kk  kkkk k110",								OPCODE_brtc);
	Register_Opcode(brts_Callback,		"1111 00kk  kkkk k110",								OPCODE_brts);
	Register_Opcode(brvc_Callback,		"1111 01kk  kkkk k011",								OPCODE_brvc);
	Register_Opcode(brvs_Callback,		"1111 00kk  kkkk k011",								OPCODE_brvs);
	Register_Opcode(bset_Callback,		"1001 0100  0sss 1000",								OPCODE_bset);
	Register_Opcode(bst_Callback,		"1111 101d  dddd 0bbb",								OPCODE_bst);
	Register_Opcode(call_Callback,		"1001 010k  kkkk 111k    kkkk kkkk  kkkk kkkk",		OPCODE_call);
	Register_Opcode(cbi_Callback,		"1001 1000  AAAA Abbb",								OPCODE_cbi);
	Register_Opcode(clc_Callback,		"1001 0100  1000 1000",								OPCODE_clc);
	Register_Opcode(clh_Callback,		"1001 0100  1101 1000",								OPCODE_clh);
	Register_Opcode(cli_Callback,		"1001 0100  1111 1000",								OPCODE_cli);
	Register_Opcode(cln_Callback,		"1001 0100  1010 1000",								OPCODE_cln);
/*	Register_Opcode(clr_Callback,		"0010 01dd  dddd dddd",								OPCODE_clr); */		/* Implied by eor */
	Register_Opcode(cls_Callback,		"1001 0100  1100 1000",								OPCODE_cls);
	Register_Opcode(clt_Callback,		"1001 0100  1110 1000",								OPCODE_clt);
	Register_Opcode(clv_Callback,		"1001 0100  1011 1000",								OPCODE_clv);
	Register_Opcode(clz_Callback,		"1001 0100  1001 1000",								OPCODE_clz);
	Register_Opcode(com_Callback,		"1001 010d  dddd 0000",								OPCODE_com);
	Register_Opcode(cp_Callback,		"0001 01rd  dddd rrrr",								OPCODE_cp);
	Register_Opcode(cpc_Callback,		"0000 01rd  dddd rrrr",								OPCODE_cpc);
	Register_Opcode(cpi_Callback,		"0011 KKKK  dddd KKKK",								OPCODE_cpi);
	Register_Opcode(cpse_Callback,		"0001 00rd  dddd rrrr",								OPCODE_cpse);
	Register_Opcode(dec_Callback,		"1001 010d  dddd 1010",								OPCODE_dec);
	Register_Opcode(eicall_Callback,	"1001 0101  0001 1001",								OPCODE_eicall);
	Register_Opcode(eijmp_Callback,		"1001 0100  0001 1001",								OPCODE_eijmp);
	Register_Opcode(elpm1_Callback,		"1001 0101  1101 1000",								OPCODE_elpm_1);
	Register_Opcode(elpm2_Callback,		"1001 000d  dddd 0110",								OPCODE_elpm_2);
	Register_Opcode(elpm3_Callback,		"1001 000d  dddd 0111",								OPCODE_elpm_3);
	Register_Opcode(eor_Callback,		"0010 01rd  dddd rrrr",								OPCODE_eor);
	Register_Opcode(fmul_Callback,		"0000 0011  0ddd 1rrr",								OPCODE_fmul);
	Register_Opcode(fmuls_Callback,		"0000 0011  1ddd 0rrr",								OPCODE_fmuls);
	Register_Opcode(fmulsu_Callback,	"0000 0011  1ddd 1rrr",								OPCODE_fmulsu);
	Register_Opcode(icall_Callback,		"1001 0101  0000 1001",								OPCODE_icall);
	Register_Opcode(ijmp_Callback,		"1001 0100  0000 1001",								OPCODE_ijmp);
	Register_Opcode(in_Callback,		"1011 0AAd  dddd AAAA",								OPCODE_in);
	Register_Opcode(inc_Callback,		"1001 010d  dddd 0011",								OPCODE_inc);
	Register_Opcode(jmp_Callback,		"1001 010k  kkkk 110k    kkkk kkkk  kkkk kkkk",		OPCODE_jmp);
	Register_Opcode(ld1_Callback,		"1001 000d  dddd 1100",								OPCODE_ld_1);
	Register_Opcode(ld2_Callback,		"1001 000d  dddd 1101",								OPCODE_ld_2);
	Register_Opcode(ld3_Callback,		"1001 000d  dddd 1110",								OPCODE_ld_3);
	Register_Opcode(ldy1_Callback,		"1000 000d  dddd 1000",								OPCODE_ld_4);
	Register_Opcode(ldy2_Callback,		"1001 000d  dddd 1001",								OPCODE_ld_5);
	Register_Opcode(ldy3_Callback,		"1001 000d  dddd 1010",								OPCODE_ld_6);
	Register_Opcode(ldy4_Callback,		"10q0 qq0d  dddd 1qqq",								OPCODE_ldd_1);
	Register_Opcode(ldz1_Callback,		"1000 000d  dddd 0000",								OPCODE_ld_7);
	Register_Opcode(ldz2_Callback,		"1001 000d  dddd 0001",								OPCODE_ld_8);
	Register_Opcode(ldz3_Callback,		"1001 000d  dddd 0010",								OPCODE_ld_9);
	Register_Opcode(ldz4_Callback,		"10q0 qq0d  dddd 0qqq",								OPCODE_ldd_2);
	Register_Opcode(ldi_Callback,		"1110 KKKK  dddd KKKK",								OPCODE_ldi);
	Register_Opcode(lds_Callback,		"1001 000d  dddd 0000    kkkk kkkk  kkkk kkkk",		OPCODE_lds);
	Register_Opcode(lpm1_Callback,		"1001 0101  1100 1000",								OPCODE_lpm_1);
	Register_Opcode(lpm2_Callback,		"1001 000d  dddd 0100",								OPCODE_lpm_2);
	Register_Opcode(lpm3_Callback,		"1001 000d  dddd 0101",								OPCODE_lpm_3);
/*	Register_Opcode(lsl_Callback,		"0000 11dd  dddd dddd",								OPCODE_lsl);	*/		/* Implied by add */
	Register_Opcode(lsr_Callback,		"1001 010d  dddd 0110",								OPCODE_lsr);
	Register_Opcode(mov_Callback,		"0010 11rd  dddd rrrr",								OPCODE_mov);
	Register_Opcode(movw_Callback,		"0000 0001  dddd rrrr",								OPCODE_movw);
	Register_Opcode(mul_Callback,		"1001 11rd  dddd rrrr",								OPCODE_mul);
	Register_Opcode(muls_Callback,		"0000 0010  dddd rrrr",								OPCODE_muls);
	Register_Opcode(mulsu_Callback,		"0000 0011  0ddd 0rrr",								OPCODE_mulsu);
	Register_Opcode(neg_Callback,		"1001 010d  dddd 0001",								OPCODE_neg);
	Register_Opcode(nop_Callback,		"0000 0000  0000 0000",								OPCODE_nop);
	Register_Opcode(or_Callback,		"0010 10rd  dddd rrrr",								OPCODE_or);
	Register_Opcode(ori_Callback,		"0110 KKKK  dddd KKKK",								OPCODE_ori);
	Register_Opcode(out_Callback,		"1011 1AAr  rrrr AAAA",								OPCODE_out);
	Register_Opcode(pop_Callback,		"1001 000d  dddd 1111",								OPCODE_pop);
	Register_Opcode(push_Callback,		"1001 001d  dddd 1111",								OPCODE_push);
	Register_Opcode(rcall_Callback,		"1101 kkkk  kkkk kkkk",								OPCODE_rcall);
	Register_Opcode(ret_Callback,		"1001 0101  0000 1000",								OPCODE_ret);
	Register_Opcode(reti_Callback,		"1001 0101  0001 1000",								OPCODE_reti);
	Register_Opcode(rjmp_Callback,		"1100 kkkk  kkkk kkkk",								OPCODE_rjmp);
/*	Register_Opcode(rol_Callback,		"0001 11dd  dddd dddd",								OPCODE_rol);	*/		/* Implied by adc */
	Register_Opcode(ror_Callback,		"1001 010d  dddd 0111",								OPCODE_ror);
	Register_Opcode(sbc_Callback,		"0000 10rd  dddd rrrr",								OPCODE_sbc);
	Register_Opcode(sbci_Callback,		"0100 KKKK  dddd KKKK",								OPCODE_sbci);
	Register_Opcode(sbi_Callback,		"1001 1010  AAAA Abbb",								OPCODE_sbi);
	Register_Opcode(sbic_Callback,		"1001 1001  AAAA Abbb",								OPCODE_sbic);
	Register_Opcode(sbis_Callback,		"1001 1011  AAAA Abbb",								OPCODE_sbis);
	Register_Opcode(sbiw_Callback,		"1001 0111  KKdd KKKK",								OPCODE_sbiw);
	Register_Opcode(sbr_Callback,		"0110 KKKK  dddd KKKK",								OPCODE_sbr);
	Register_Opcode(sbrc_Callback,		"1111 110r  rrrr 0bbb",								OPCODE_sbrc);
	Register_Opcode(sbrs_Callback,		"1111 111r  rrrr 0bbb",								OPCODE_sbrs);
	Register_Opcode(sec_Callback,		"1001 0100  0000 1000",								OPCODE_sec);
	Register_Opcode(seh_Callback,		"1001 0100  0101 1000",								OPCODE_seh);
	Register_Opcode(sei_Callback,		"1001 0100  0111 1000",								OPCODE_sei);
	Register_Opcode(sen_Callback,		"1001 0100  0010 1000",								OPCODE_sen);
	Register_Opcode(ser_Callback,		"1110 1111  dddd 1111",								OPCODE_ser);
	Register_Opcode(ses_Callback,		"1001 0100  0100 1000",								OPCODE_ses);
	Register_Opcode(set_Callback,		"1001 0100  0110 1000",								OPCODE_set);
	Register_Opcode(sev_Callback,		"1001 0100  0011 1000",								OPCODE_sev);
	Register_Opcode(sez_Callback,		"1001 0100  0001 1000",								OPCODE_sez);
	Register_Opcode(sleep_Callback,		"1001 0101  1000 1000",								OPCODE_sleep);
	Register_Opcode(spm_Callback,		"1001 0101  1110 1000",								OPCODE_spm);
	Register_Opcode(st1_Callback,		"1001 001r  rrrr 1100",								OPCODE_st_1);
	Register_Opcode(st2_Callback,		"1001 001r  rrrr 1101",								OPCODE_st_2);
	Register_Opcode(st3_Callback,		"1001 001r  rrrr 1110",								OPCODE_st_3);
	Register_Opcode(sty1_Callback,		"1000 001r  rrrr 1000",								OPCODE_st_4);
	Register_Opcode(sty2_Callback,		"1001 001r  rrrr 1001",								OPCODE_st_5);
	Register_Opcode(sty3_Callback,		"1001 001r  rrrr 1010",								OPCODE_st_6);
	Register_Opcode(sty4_Callback,		"10q0 qq1r  rrrr 1qqq",								OPCODE_std_1);
	Register_Opcode(stz1_Callback,		"1000 001r  rrrr 0000",								OPCODE_st_7);
	Register_Opcode(stz2_Callback,		"1001 001r  rrrr 0001",								OPCODE_st_8);
	Register_Opcode(stz3_Callback,		"1001 001r  rrrr 0010",								OPCODE_st_9);
	Register_Opcode(stz4_Callback,		"10q0 qq1r  rrrr 0qqq",								OPCODE_std_2);
	Register_Opcode(sts_Callback,		"1001 001d  dddd 0000    kkkk kkkk  kkkk kkkk",		OPCODE_sts);
	Register_Opcode(sub_Callback,		"0001 10rd  dddd rrrr",								OPCODE_sub);
	Register_Opcode(subi_Callback,		"0101 KKKK  dddd KKKK",								OPCODE_subi);
	Register_Opcode(swap_Callback,		"1001 010d  dddd 0010",								OPCODE_swap);
/*	Register_Opcode(tst_Callback,		"0010 00dd  dddd dddd",								OPCODE_tst);	*/		/* Implied by and */
	Register_Opcode(wdr_Callback,		"1001 0101  1010 1000",								OPCODE_wdr);

	if (Options.Show_PseudoCode) {
		Supersede_Opcode(adc_Callback_PC,	OPCODE_adc);
		Supersede_Opcode(add_Callback_PC,	OPCODE_add);
		Supersede_Opcode(sub_Callback_PC,	OPCODE_sub);
		Supersede_Opcode(sbc_Callback_PC,	OPCODE_sbc);
		Supersede_Opcode(mov_Callback_PC,	OPCODE_mov);
		Supersede_Opcode(brcc_Callback_PC,	OPCODE_brcc);
		Supersede_Opcode(brcs_Callback_PC,	OPCODE_brcs);
		Supersede_Opcode(breq_Callback_PC,	OPCODE_breq);
		Supersede_Opcode(brge_Callback_PC,	OPCODE_brge);
		Supersede_Opcode(brhc_Callback_PC,	OPCODE_brhc);
		Supersede_Opcode(brhs_Callback_PC,	OPCODE_brhs);
		Supersede_Opcode(brid_Callback_PC,	OPCODE_brid);
		Supersede_Opcode(brie_Callback_PC,	OPCODE_brie);
		Supersede_Opcode(brlo_Callback_PC,	OPCODE_brlo);
		Supersede_Opcode(brlt_Callback_PC,	OPCODE_brlt);
		Supersede_Opcode(brmi_Callback_PC,	OPCODE_brmi);
		Supersede_Opcode(brne_Callback_PC,	OPCODE_brne);
		Supersede_Opcode(brpl_Callback_PC,	OPCODE_brpl);
		Supersede_Opcode(brsh_Callback_PC,	OPCODE_brsh);
		Supersede_Opcode(brtc_Callback_PC,	OPCODE_brtc);
		Supersede_Opcode(brts_Callback_PC,	OPCODE_brts);
		Supersede_Opcode(brvc_Callback_PC,	OPCODE_brvc);
		Supersede_Opcode(brvs_Callback_PC,	OPCODE_brvs);
		Supersede_Opcode(out_Callback_PC,	OPCODE_out);
		Supersede_Opcode(in_Callback_PC,	OPCODE_in);
		Supersede_Opcode(cli_Callback_PC,	OPCODE_cli);
		Supersede_Opcode(sei_Callback_PC,	OPCODE_sei);
		Supersede_Opcode(ret_Callback_PC,	OPCODE_ret);
		Supersede_Opcode(reti_Callback_PC,	OPCODE_reti);
		Supersede_Opcode(andi_Callback_PC,	OPCODE_andi);
		Supersede_Opcode(subi_Callback_PC,	OPCODE_subi);
		Supersede_Opcode(sbci_Callback_PC,	OPCODE_sbci);
		Supersede_Opcode(sbr_Callback_PC,	OPCODE_sbr);
		Supersede_Opcode(ori_Callback_PC,	OPCODE_ori);
		Supersede_Opcode(ldi_Callback_PC,	OPCODE_ldi);
		Supersede_Opcode(lds_Callback_PC,	OPCODE_lds);
		Supersede_Opcode(sts_Callback_PC,	OPCODE_sts);
		Supersede_Opcode(call_Callback_PC,	OPCODE_call);
		Supersede_Opcode(rcall_Callback_PC,	OPCODE_rcall);
		Supersede_Opcode(ror_Callback_PC,	OPCODE_ror);
		Supersede_Opcode(lsr_Callback_PC,	OPCODE_lsr);
		Supersede_Opcode(eor_Callback_PC,	OPCODE_eor);
		Supersede_Opcode(swap_Callback_PC,	OPCODE_swap);
		Supersede_Opcode(jmp_Callback_PC,	OPCODE_jmp);
		Supersede_Opcode(rjmp_Callback_PC,	OPCODE_rjmp);
		Supersede_Opcode(cpi_Callback_PC,	OPCODE_cpi);
		Supersede_Opcode(asr_Callback_PC,	OPCODE_asr);
		Supersede_Opcode(inc_Callback_PC,	OPCODE_inc);
		Supersede_Opcode(dec_Callback_PC,	OPCODE_dec);
		Supersede_Opcode(cp_Callback_PC,	OPCODE_cp);
		Supersede_Opcode(cpc_Callback_PC,	OPCODE_cpc);
		Supersede_Opcode(cpse_Callback_PC,	OPCODE_cpse);
		Supersede_Opcode(and_Callback_PC,	OPCODE_and);
		Supersede_Opcode(or_Callback_PC,	OPCODE_or);
		Supersede_Opcode(mul_Callback_PC,	OPCODE_mul);
		Supersede_Opcode(sbi_Callback_PC,	OPCODE_sbi);
		Supersede_Opcode(sbis_Callback_PC,	OPCODE_sbis);
		Supersede_Opcode(sbic_Callback_PC,	OPCODE_sbic);
		Supersede_Opcode(cbi_Callback_PC,	OPCODE_cbi);
		Supersede_Opcode(ser_Callback_PC,	OPCODE_ser);
		Supersede_Opcode(movw_Callback_PC,	OPCODE_movw);
		Supersede_Opcode(adiw_Callback_PC,	OPCODE_adiw);
		Supersede_Opcode(lpm1_Callback_PC,	OPCODE_lpm_1);
		Supersede_Opcode(st2_Callback_PC,	OPCODE_st_2);
	}

	qsort(Opcodes, Number_Opcodes, sizeof(struct Opcode), Comparison);

    return 1;
}

extern "C" int Get_JumpCall_Count();
extern "C" struct JumpCall *Get_JumpCall();

/* Show all references which refer to "Position" as destination */
void Print_JumpCallsIM(int Position) {
	int i;
	int Match = 0;
	
    struct JumpCall *JumpCalls = Get_JumpCall();

	for (i = 0; i < Get_JumpCall_Count(); i++) {
		if ((JumpCalls[i].To) == Position) {
			if (Match == 0) {
				Match = 1;
			}
			ImGui::Text("; Referenced from offset 0x%02x by %s", JumpCalls[i].From, MNemonic[JumpCalls[i].Type]);
            break;
		}
	}
	if (Match == 1) {
		char *LabelName;
		char *LabelComment = NULL;
		LabelName = Get_Label_Name(Position, &LabelComment);
		if (LabelComment == NULL) {
			 ImGui::Text("%s:", LabelName);
		} else {
			 ImGui::Text("%s:     ; %s", LabelName, LabelComment);
		}
	}
}



void Disasm(char *Bitstream, int Pos, int Read)
{
   	int Opcode;

    for(int j=0; j < 5 ; j++)
    {
		int Added;



		/* Check if this is actually code or maybe only data from tagfile */
		Added = Tagfile_Process_Data(Bitstream, Pos);
		if (Added != 0) {
			/* Data was added */
			Pos += Added;
			return;
		}
		if( j == 0 ){
		    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.0f, 1.0f, 0.0f, 1.0f));
		}

		Opcode = Get_Next_Opcode(Bitstream + Pos);
		if (Opcode != -1) {
			Code_Line[0] = 0;
			Comment_Line[0] = 0;
			After_Code_Line[0] = 0;
			Opcodes[Opcode].Callback(Bitstream + Pos, Pos, Opcodes[Opcode].MNemonic);
			
			if (Options.Process_Labels) {
				Print_JumpCallsIM(Pos);
			}
				
			if (Options.Show_Addresses) {
                ImGui::Text("0x%04x:   ", Pos);
                ImGui::SameLine();
            }

			if (Options.Show_Cycles) {
				const char *Cycle = Cycles[Opcodes[Opcode].MNemonic];
                if (!Cycle) {
                    ImGui::Text("      ");
                } else {
                    ImGui::Text("[%-3s] ", Cycle);
                }
			}

	        if (Options.Show_Opcodes) {
                ImGui::Text(" "); // Start with a space
                for (int i = 0; i < (Get_Bitmask_Length(Opcodes[Opcode].Opcode_String)) / 8; i++) {
                    ImGui::SameLine();
                    ImGui::Text("%02x ", (unsigned char)(Bitstream[Pos+i]));
                }
                for (int i = 0; i < 5 - ((Get_Bitmask_Length(Opcodes[Opcode].Opcode_String)) / 8); i++) {
                    ImGui::SameLine();
                    ImGui::Text("   ");
                }
            }
				
			if (Code_Line[0] == 0) {
				/* No code was generated? */
                ImGui::Text("; - Not implemented opcode: %d -", Opcodes[Opcode].MNemonic);
			} else {
				if ((Comment_Line[0] == 0) || (!Options.Show_Comments)) {
					/* No comment */
                    ImGui::Text("%s", Code_Line);

				} else {
					/* Comment available */
					if (!Options.Show_PseudoCode) {
                        ImGui::SameLine();
                        ImGui::Text("%-35s ; %s", Code_Line, Comment_Line);
					} else {
                        ImGui::SameLine();
                        ImGui::Text("%-35s ; %s", Code_Line, Comment_Line);
					}
				}
			}
            ImGui::SameLine();
            ImGui::Text("%s", After_Code_Line);

			Pos += Get_Bitmask_Length(Opcodes[Opcode].Opcode_String) / 8;
		} else {
            ImGui::Text(".word 0x%02x%02x    ; Invalid opcode at 0x%04x (%d). Disassembler skipped two bytes.",
                        Bitstream[Pos + 1], Bitstream[Pos], Pos, Pos);
			Pos += 2;
		}
		if( j == 0 ){
    		ImGui::PopStyleColor();
		}
	}
}