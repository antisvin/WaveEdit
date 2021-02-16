#include <functional>
#include <regex>
#include "WaveEdit.hpp"
#include "RtMidi.h"
#include "imgui.h"
#include "MidiMessage.h"


static void midiCallback(double deltatime, std::vector< unsigned char > *message, void *userData){
    reinterpret_cast<OwlDevice*>(userData)->handleResponse(deltatime, message);
}

OwlDevice::OwlDevice(int port_number, const std::string& name, RtMidiIn* midiin, RtMidiOut* midiout)
	: port_number(port_number)
    , device_name(name)
    , midiin(midiin)
    , midiout(midiout)
    {
        midiin->setCallback(midiCallback, (void*)this);
        /*
        midiin->setCallback(std::bind(
            &OwlDevice::handleResponse, this,
            std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));
        */
}

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
            devices.push_back(OwlDevice(i, match.str(0).substr(4), midiin, midiout));
            if (set_active){
                printf("Active\n");
                setActiveDevice(0);
                set_active = false;
            }
        }
    }
}

void OwlDevice::fetchResources(){
  	midiout->openPort(port_number);
	sendRequest(SYSEX_FIRMWARE_VERSION);
}

void OwlDevice::sendRequest(uint8_t cmd){
    sendCc(REQUEST_SETTINGS, cmd);
}

void OwlDevice::sendCc(uint8_t cc, uint8_t value){
    auto message = MidiMessage::cc(0, cc, value);
    midiout->sendMessage((const unsigned char*)&message.data[1], 3);
}

void OwlDevice::handleResponse(double deltatime, std::vector< unsigned char > *message){
    printf("%X%X\n", *(uint32_t*)message, *(uint32_t*)(&message[2]));
}

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

    ImGui::BeginChild("Resources", ImVec2(200, 0), true);
	{
		float dummyZ = 0.0;
		ImGui::PushItemWidth(-1);
		//renderBankGrid("SidebarGrid", BANK_LEN * 35.0, 1, &dummyZ, &morphZ);
		//refreshMorphSnap();
	}
	ImGui::EndChild();
}

OwlController owl;
