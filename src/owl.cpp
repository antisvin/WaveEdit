#include <regex>
#include "WaveEdit.hpp"
#include "RtMidi.h"

void OwlController::scan(bool set_active){
    printf("Scanning for OWL devices\n");
    RtMidiIn *midiin = 0;
    RtMidiOut *midiout = 0;    

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
    for ( unsigned int i=0; i<nPorts; i++ ) {
        try {
            portName = midiin->getPortName(i);
        }
        catch ( RtMidiError &error ) {
            error.printMessage();
            break;
        }
        if (std::regex_match (portName, std::regex(".*OWL\\-.*") )){
            printf("Found %s\n", portName.c_str());
            devices.push_back(OwlDevice(i));
            if (set_active){
                setActiveDevice(devices[0]);
                set_active = false;
            }
        }
    }
}


OwlController owl;
