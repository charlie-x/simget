#include <argparse/argparse.hpp>
#include "imgui.h"
#include "imgui_stdlib.h"
#include "imgui_internal.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"
#include <glad/glad.h>
#include <GLFW/glfw3.h>

#include "framebuffer.h"

// comes from the imgui_club repo
#include "imgui_memory_editor.h"

#include <iostream>
#include <string>
#include <vector>
#include <signal.h>

#include "simgetavr.h"

extern "C"
{
#include "simavr/sim/avr_ioport.h"
#include "simavr/sim/sim_avr.h"
#include "simavr/sim/avr_uart.h"
#include "simavr/sim/sim_gdb.h"
#include "simavr/sim/sim_core.h"
#include "simavr/sim/sim_vcd_file.h"
#include "simavr/sim/sim_hex.h"
#include "simavr/sim/sim_elf.h"
#include "simavr/sim/sim_mcu_structs.h"
}

// handy macros
#define BIT_SET(port, bit) ((port) |= (1 << (bit)))
#define BIT_CLEAR(port, bit) ((port) &= ~(1 << (bit)))
#define BIT_TEST(port, bit) ((port) & (1 << (bit)))

int setupAVRDisasm();
void Disasm(char *Bitstream, int Pos, int Read);

void sig_int(int sign)
{

    std::cout << "Signal caught, simavr terminating" << std::endl;

    exit(0);
}

int m_width, m_height;

FrameBuffer *sceneBuffer;
//

// Function to list supported AVR cores
void list_cores()
{
}

void ShowAvrDetailsFull(AvrSimulator &avrSim)
{
    avr_t *avr = avrSim.avr;

    if (!avr)
        return;

    // Start a new ImGui window
    if (ImGui::Begin("AVR Details Full"))
    { // Only proceed if the window is open
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
        ImGui::Text("\t0x%X%X%X", avr->signature[0], avr->signature[1], avr->signature[2]);

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

void HexEditor(AvrSimulator &avrSim, bool run)
{
    static MemoryEditor mem_edit_1;

    avr_t *avr = avrSim.avr;

    if (!avr)
        return;

    static size_t start;

    // only chase if not pc changed
    {

        if (start != avr->pc)
        {

            size_t end = avr->pc + 1;
            start = avr->pc;

            mem_edit_1.HighlightPC(start, end);
        }
    }

    mem_edit_1.DrawWindow("Memory Editor FLASH", avr->flash, avr->flashend);
}

void HexEditorRAM(AvrSimulator &avrSim)
{

    avr_t *avr = avrSim.avr;

    static MemoryEditor mem_edit_1;
    mem_edit_1.DrawWindow("Memory Editor RAM", avr->data, avr->ramend);
}

void displayIO(AvrSimulator &avrSim, const std::string &io_type, uint8_t addr, char *cname)
{
    avr_t *avr = avrSim.avr;

    // display register name, address, and current value
    ImGui::Text("%s%s (0x%02X) 0x%02X", io_type.c_str(), cname, addr, avrSim.avr->data[addr]);
    // ImGui::SameLine();

    // calculate spacing based on window width
    float windowWidth = ImGui::GetWindowWidth();
    float spacing = windowWidth / 9; // 8 bits + label

    // display the bit numbers
    ImGui::SetCursorPosX(spacing); // Initial spacing for alignment
    for (int i = 7; i >= 0; --i)
    {
        ImGui::Text("%d", i);
        if (i > 0)
        {
            ImGui::SameLine();
            ImGui::SetCursorPosX((spacing * (8 - i + 1)));
        }
    }

    // put text in box
    ImGui::SameLine();

    std::string name;

    // display the checkboxes for each bit
    for (int i = 7; i >= 0; --i)
    {
        ImGui::SetCursorPosX((spacing * (8 - i))); // adjust for checkbox size
        bool bitSet = avrSim.avr->data[addr] & (1 << i);
        name = std::string("##") + io_type + cname + std::to_string(i);

        if (ImGui::Checkbox(name.c_str(), &bitSet))
        {
            if (io_type == "PIN")
            {

                avr_irq_t *irq = avr_io_getirq(avr, AVR_IOCTL_IOPORT_GETIRQ(cname[0]), i);

                if (irq)
                {
                    if (bitSet)
                    {

                        avr_raise_irq(irq, 1); // Simulate a high state on the pin
                    }
                    else
                    {

                        avr_raise_irq(irq, 0); // Simulate a low state on the pin
                    }
                }
            }
            else
            {
                // toggle the bit if the checkbox is clicked
                if (bitSet)
                {
                    avrSim.avr->data[addr] |= (1 << i);
                }
                else
                {
                    avrSim.avr->data[addr] &= ~(1 << i);
                }
            }
        }
        if (i > 0)
        {
            ImGui::SameLine();
        }
    }
}

void ModifyAvrIoRegister(AvrSimulator &avrSim, avr_ioport_t *port)
{
    if (!port)
        return;

    char cname[2] = {
        0,
        0,
    };
    cname[0] = port->name;

    displayIO(avrSim, "PORT", port->r_port, cname);
    displayIO(avrSim, "DDR", port->r_ddr, cname);
    displayIO(avrSim, "PIN", port->r_pin, cname);
}

void ModifyAvrIoRegisters(AvrSimulator &avrSim)
{
    avr_t *avr = avrSim.avr;

    if (!avr)
        return;

    // recast to mcu type
    mcu_attiny4313_t *mcu = (mcu_attiny4313_t *)avr;

    ImGui::Begin("AVR IO Register Control");

    ModifyAvrIoRegister(avrSim, &mcu->porta);
    ModifyAvrIoRegister(avrSim, &mcu->portb);
    ModifyAvrIoRegister(avrSim, &mcu->portd);

    ImGui::End();
}

const char *GetAvrStateName(int state)
{
    switch (state)
    {
    case cpu_Limbo:
        return "Limbo";
    case cpu_Stopped:
        return "Stopped";
    case cpu_Running:
        return "Running";
    case cpu_Sleeping:
        return "Sleeping";
    case cpu_Step:
        return "Step";
    case cpu_StepDone:
        return "Step Done";
    case cpu_Done:
        return "Done";
    case cpu_Crashed:
        return "Crashed";
    default:
        return "Unknown";
    }
}

void ShowAvrState(const int state)
{

    ImGui::Text("State: %s", GetAvrStateName(state));
}

void DumpAvrRegisters(AvrSimulator &avrSim)
{

    avr_t *avr = avrSim.avr;

    if (!avr)
        return;

    ImGui::Begin("AVR Register Dump");

    for (int i = 0; i < 32; i++)
    {
        ImGui::Text("%s=%02x", avr_regname(avr, i), avr->data[i]);
        if ((i % 8) != 7)
            ImGui::SameLine();
    }

    uint16_t y = avr->data[R_YL] | (avr->data[R_YH] << 8);
    for (int i = 0; i < 20; i++)
    {
        if( y + i < 352 )   {
            ImGui::Text("Y+%02d=%02x", i, avr->data[y + i]);
        }
        if (i % 10 != 9)
            ImGui::SameLine();
    }

    ImGui::End();
}

bool ShowAvrDisasm(AvrSimulator &avr)
{
    if (ImGui::Begin("AVR Disasm Window"))
    {
        Disasm((char *)avr.avr->flash, avr.avr->pc, 4096);
        ImGui::End();
    }
    return true;
}

bool ShowAvrDetails(AvrSimulator &avr)
{
    // Start a new ImGui window
    if (ImGui::Begin("AVR Details Window"))
    { // Only proceed if the window is open

        ImGui::Text("MCU: %s", avr.avr->mmcu);
        ImGui::Text("Frequency: %d Hz", avr.avr->frequency);
        ImGui::Text("Cycle Counter: %lu", avr.avr->cycle);

        ImGui::Text("VCC: %d V", avr.avr->vcc);
        ImGui::Text("AVCC: %d V", avr.avr->avcc);
        ImGui::Text("AREF: %d V", avr.avr->aref);

        ImGui::Text("pc:       0x%x", avr.avr->pc);
        ImGui::Text("reset pc: 0x%x", avr.avr->reset_pc);

        {
            const char *_sreg_bit_name = "cznvshti";
            char buffer[32];
            for (int _sbi = 0; _sbi < 8; _sbi++)
            {
                sprintf(buffer, "%c", avr.avr->sreg[_sbi] ? toupper(_sreg_bit_name[_sbi]) : '.');
            }
            ImGui::Text("sreg: %s", buffer);
        }

        ImGui::Checkbox("run", &avr.run);
        ImGui::Checkbox("animate", &avr.animate);

        if (ImGui::Button("step"))
        {
            avr.RunAnimate();
        }

        if (ImGui::Button("reset"))
        {
            avr.Reset();
        }

        ShowAvrState(avr.state);

        if (avr.state == cpu_Done || avr.state == cpu_Crashed)
        {
            ImGui::Text("DONE/CRASHED");
        };

        ImGui::End(); // End of AVR Details Window
    }

    return avr.run;
}

/**
 * @brief Draw a line between two points with a given color.
 *
 * @param x1 The x-coordinate of the start point.
 * @param y1 The y-coordinate of the start point.
 * @param x2 The x-coordinate of the end point.
 * @param y2 The y-coordinate of the end point.
 * @param color The color of the line (ImVec4: red, green, blue, alpha).
 */
void drawLine(float x1, float y1, float x2, float y2, ImVec4 color)
{
    ImVec2 p1 = ImGui::GetCursorScreenPos();

    ImGui::GetWindowDrawList()->AddLine(
        ImVec2(p1.x+x1,p1.y+y1), 
        ImVec2(p1.x+x2,p1.y+y2), 
        IM_COL32(255, 0, 0, 255), 1.0f
    );

    glColor4f(color.x, color.y, color.z, color.w); // Set the color using the ImVec4 values
    glBegin(GL_LINES);                             // Begin drawing lines
        glVertex2f(x1, y1);                        // Define the start point
        glVertex2f(x2, y2);                        // Define the end point
    glEnd();                                       // End drawing lines
}

void drawCircle(GLfloat x, GLfloat y, GLfloat radius, bool on, int numSegments = 32)
{
    if(on)
        ImGui::GetWindowDrawList()->AddCircleFilled(ImVec2(x,y), radius, IM_COL32(255, 0, 0, 255), 20);
    else
        ImGui::GetWindowDrawList()->AddCircle(ImVec2(x,y), radius, IM_COL32(128, 0, 0, 255), 20, 1);    
}

const int LED_COUNT = 12;
const int OFFSET = 3; // Offset for the first LED from the center
const double PI = 3.14159265358979323846;

class FidgetSpinner
{
public:
    FidgetSpinner();
    void update();
    void draw(ImVec2 center, ImVec2 size);
    void calculateRPM();
    void sineWaveEffect();

    void setAvr(avr_t *_avr)
    {
        avr = _avr;
    }

    avr_t *avr;

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

FidgetSpinner::FidgetSpinner() : angle(0), rpm(0), waveIndex(0)
{
    ledStates.resize(LED_COUNT, false);
    lastTDC = std::chrono::steady_clock::now();
    calculateLedPositions();
    updateLedStates();
}

void FidgetSpinner::update()
{
    angle += PI / 100;
    if (angle >= 2 * PI)
    {
        angle -= 2 * PI;
        calculateRPM();

        {
            //std::cerr << "trigger\n";

            avr_irq_t *irq = avr_io_getirq(avr, AVR_IOCTL_IOPORT_GETIRQ('D'), 3);

            if (irq)
            {
                static bool state = 0;
                state = 1 - state;

                if (state)
                    avr_raise_irq(irq, 1); // Simulate a low state on the pin
                else
                    avr_raise_irq(irq, 0); // Simulate a high state on the pin
            }
        }
    }

    // std::cout << angle <<std::endl;

    calculateLedPositions();
    updateLedStates();
}

void FidgetSpinner::calculateLedPositions()
{
    ledPositions.clear();
    for (int i = 0; i < LED_COUNT; ++i)
    {
        float x = (i + OFFSET) * cos(angle);
        float y = (i + OFFSET) * sin(angle);
        ledPositions.emplace_back(x, y);
    }
}

void FidgetSpinner::updateLedStates()
{

    if (!avr)
    {
        std::cerr << "avr is null\n";
        return;
    }

    mcu_attiny4313_t *mcu = (mcu_attiny4313_t *)avr;

#define PORTA avr->data[mcu->porta.r_port]
#define PORTB avr->data[mcu->portb.r_port]
#define PORTD avr->data[mcu->portd.r_port]

    if (BIT_TEST(PORTD, 0))
        ledStates[0] = true;
    else
        ledStates[0] = false;
    if (BIT_TEST(PORTD, 1))
        ledStates[1] = true;
    else
        ledStates[1] = false;
    if (BIT_TEST(PORTD, 4))
        ledStates[2] = true;
    else
        ledStates[2] = false;
    if (BIT_TEST(PORTD, 5))
        ledStates[3] = true;
    else
        ledStates[3] = false;
    if (BIT_TEST(PORTD, 6))
        ledStates[4] = true;
    else
        ledStates[4] = false;
    if (BIT_TEST(PORTB, 0))
        ledStates[5] = true;
    else
        ledStates[5] = false;
    if (BIT_TEST(PORTB, 1))
        ledStates[6] = true;
    else
        ledStates[6] = false;
    if (BIT_TEST(PORTB, 2))
        ledStates[7] = true;
    else
        ledStates[7] = false;
    if (BIT_TEST(PORTB, 3))
        ledStates[8] = true;
    else
        ledStates[8] = false;
    if (BIT_TEST(PORTB, 4))
        ledStates[9] = true;
    else
        ledStates[9] = false;
    if (BIT_TEST(PORTA, 0))
        ledStates[10] = true;
    else
        ledStates[10] = false;
    if (BIT_TEST(PORTA, 1))
        ledStates[11] = true;
    else
        ledStates[11] = false;
    ;
}

void FidgetSpinner::calculateRPM()
{
    auto now = std::chrono::steady_clock::now();
    std::chrono::duration<double> timePassed = now - lastTDC;
    if (timePassed.count() > 0)
    {
        rpm = 60 / timePassed.count();
        lastTDC = now;
    }
}

void FidgetSpinner::sineWaveEffect()
{
    const int sineWaveSize = LED_COUNT * 2;
    for (int i = 0; i < LED_COUNT; ++i)
    {
        int index = (waveIndex + i) % sineWaveSize;
        ledStates[i] = index < LED_COUNT;
    }
    waveIndex = (waveIndex + 1) % sineWaveSize;
}
void FidgetSpinner::draw(ImVec2 center, ImVec2 size)
{
    ImVec4  db = ImVec4(0.0f, 0.0f, 1.0f, 1.0f) ;

    // Debug: Draw a cross at the center of the viewport
    drawLine(center.x - 10, center.y, center.x + 10, center.y, ImVec4(1.0f, 1.0f, 1.0f, 1.0f));
    drawLine(center.x, center.y - 10, center.x, center.y + 10, ImVec4(1.0f, 1.0f, 1.0f, 1.0f));

    drawCircle( center.x,center.y,3.0f,1 );

    for (size_t i = 0; i < ledPositions.size(); ++i)
    {

        float x = (ledPositions[i].first  * 6) + center.x;
        float y = (ledPositions[i].second * 6) + center.y;

        drawCircle(x, y, 4.0f, ledStates[i]);
    }

    ImGui::Text("RPM: %d", static_cast<int>(rpm));
}

FidgetSpinner spinner;

void renderLEDsInImGuiWindow()
{
    ImGui::SetNextWindowContentSize(ImVec2(200,200));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0,0));

    ImGui::Begin("LED Control", NULL, ImGuiWindowFlags_AlwaysAutoResize);

    // Get the size of the ImGui child window
    ImVec2 size = ImGui::GetContentRegionAvail();
    ImVec2 p1 = ImGui::GetCursorScreenPos();
    // Calculate the center position for the LEDs
    ImVec2 center = ImVec2(size.x / 2, size.y / 2);
    center.x += p1.x;
    center.y += p1.y;

    spinner.update();
    spinner.draw(center, size); // Pass the center and size to the draw method

    ImGui::End();
    ImGui::PopStyleVar();


}

void window_size_callback_static(GLFWwindow *window, int width, int height)
{
    glViewport(0, 0, width, height);
    if (sceneBuffer)
        sceneBuffer->RescaleFrameBuffer(width, height);
}

int main(int argc, char **argv)
{

    try
    {
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

        try
        {
            program.parse_args(argc, argv);
        }
        catch (const std::runtime_error &err)
        {
            std::cerr << err.what() << std::endl;
            std::cerr << program;
            return 1;
        }

        if (program["--list-cores"] == true)
        {
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

        std::cout << "init glfw\n";

        // apple stuck at 2.1
        glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
        glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
        glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE); // For Mac
        glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
        // Initialize GLFW for ImGui
        if (!glfwInit())
        {
            std::cerr << "Failed to initialize GLFW" << std::endl;
            return 1;
        }

        GLFWwindow *window = glfwCreateWindow(900, 840, "SimAVR", NULL, NULL);
        if (!window)
        {
            std::cerr << "Failed to create GLFW window" << std::endl;
            glfwTerminate();
            return 1;
        }

        glfwMakeContextCurrent(window);

        // glad: load all OpenGL function pointers
        // ---------------------------------------
        if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress))
        {
            std::cerr << "Failed to initialize GLAD" << std::endl;
            return -1;
        }
        if (!gladLoadGL())
        {
            std::cout << "Failed to initialize OpenGL context" << std::endl;
            return -1;
        }

        std::cout << "OpenGL " << GLVersion.major << " " << GLVersion.minor << std::endl;

        if (GLAD_GL_VERSION_3_0)
        {
            std::cout << "gl version 3.0\n";
        }
        std::cout << "init imgui\n";

        // setup ImGui context
        IMGUI_CHECKVERSION();
        ImGui::CreateContext();
        ImGuiIO &io = ImGui::GetIO();
        (void)io;

        // setup ImGui + GLFW binding
        if (ImGui_ImplGlfw_InitForOpenGL(window, true) == false)
        {
            std::cerr << "ImGui_ImplGlfw_InitForOpenGL failed\n";
        }

        if (ImGui_ImplOpenGL3_Init("#version 120") == false)
        {
            std::cerr << "ImGui_ImplOpenGL3_Init failed\n";
        }

        printf("OpenGL version supported by this platform (%s): \n", glGetString(GL_VERSION));

        std::cout << "setup avr dis\n";

        // setup the disasm
        setupAVRDisasm();

        std::cout << "avr sim init\n";

        avrSim.Initialize(mcu, firmware_file, frequency, gdb_port);

        // Setup signal handlers
        signal(SIGINT, sig_int);
        signal(SIGTERM, sig_int);

        std::cout << "starting thread\n";

        // Start a new thread and use a lambda to call the run() method on the Avr instance
        std::thread avrThread([&avrSim]()
                              {
        while(1) {
            if (avrSim.run || avrSim.animate) {
                avrSim.Run();
            }

            std::this_thread::yield();
        } });

#ifndef __APPLE__
        std::cout << "setup scenebuffer\n";
        glfwGetFramebufferSize(window, &m_width, &m_height);
        std::cout << " w h " << m_width << " " << m_height << "\n";
        sceneBuffer = new FrameBuffer(m_width, m_height);
        if (sceneBuffer == nullptr)
        {
            std::cerr << "failed to create scene buffer\n";
        }

        glfwSetWindowSizeCallback(window, window_size_callback_static);

        spinner.setAvr(avrSim.avr);
#endif

        std::cout << "main loop\n";
        // Main loop
        while (!glfwWindowShouldClose(window))
        {

            glfwPollEvents();

            // Start ImGui frame
            ImGui_ImplOpenGL3_NewFrame();
            ImGui_ImplGlfw_NewFrame();
            ImGui::NewFrame();

            bool run = ShowAvrDetails(avrSim);

            ShowAvrDisasm(avrSim);

            ShowAvrDetailsFull(avrSim);
            DumpAvrRegisters(avrSim);

            HexEditor(avrSim, run);
            HexEditorRAM(avrSim);

            ModifyAvrIoRegisters(avrSim);

#ifndef __APPLE__
            renderLEDsInImGuiWindow();
#endif

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
    }
    catch (...)
    {
    }
    return 0;
}
