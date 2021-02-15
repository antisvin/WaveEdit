#include <regex>
#include "WaveEdit.hpp"
#include "RtMidi.h"
#include "imgui.h"


void OwlController::scan(bool set_active){
    printf("Scanning for OWL devices\n");
    if (midiin != nullptr)
        delete midiin;
    if (midiout != nullptr)
        delete midiout;

    try {
        midiin = new RtMidiIn();        
        midiout = new RtMidiOut();        
    } catch (RtMidiError &error) {
        printf("Error opening MIDI devices\n");
        error.printMessage();
        return;
    }

    devices.clear();

    // Check inputs.
    unsigned int nPorts = midiin->getPortCount();
    std::string portName;    
    std::regex regexp("OWL\\-\\w+");
    for ( unsigned int i=0; i<nPorts; i++ ) {
        try {
            portName = midiin->getPortName(i);
        }
        catch ( RtMidiError &error ) {
            error.printMessage();
            break;
        }
        std::smatch match;
        std::regex_search(portName, match, regexp);
        if (match.size()){
            printf("Found %s\n", portName.c_str());            
            devices.push_back(OwlDevice(i, match.str(0).substr(4)));
            if (set_active){
                printf("Active\n");
                setActiveDevice(0);
                set_active = false;
            }
        }
    }
}

OwlDevice::OwlDevice(int port_number, const std::string& name)
	: port_number(port_number)
    , device_name(name) {};


void owlPage() {
	ImGui::BeginChild("OWL Page", ImVec2(0, 0), true);
    {
		if (ImGui::Button("Scan")) {
			owl.scan(true);
		}

        if (owl.devices.size() == 0){
            ImGui::SameLine();
            ImGui::Text("No devices found");
        }
        else {
            for (auto device = owl.devices.begin(); device != owl.devices.end(); device++){
                ImGui::SameLine();
                if (ImGui::RadioButton(device->getName().c_str(), owl.isActive(device - owl.devices.begin()))){
                    // Device radio clicked
                    owl.setActiveDevice(device - owl.devices.begin());
                }
            }        
        }
    }
    ImGui::EndChild();
}

OwlController owl;
