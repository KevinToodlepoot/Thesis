/*
  =============================================================================

    Modulator.cp
    Created: 9 Feb 2022 4:29:54m
    Author:  Kevin Kopczynsi

  =============================================================================
*/

#include "Modulator.h"

Modulator::Modulator() {}
Modulator::~Modulator() {}

void Modulator::initMod(int fs) {
    this->samp_rate = fs;
}

void Modulator::setMod(float in_freq) {
    float fs, f0;
    
    mod.freq = in_freq;
    mod.phase = 0;
    
    if (in_freq == 0) {
        mod.phase_inc = -1;
    } else {
        fs = this->samp_rate;
        f0 = in_freq;
        mod.phase_inc = juce::MathConstants<float>::twoPi * f0 / fs;
    }
}

void Modulator::updateMod(float new_freq) {
    float fs, f0;
    
    mod.freq = new_freq;
    
    if( new_freq == 0) {
        mod.phase_inc = -1;
    } else {
        fs = this->samp_rate;
        f0  =new_freq;
        mod.phase_inc  =juce::MathConstants<float>::twoPi * f0 / fs;
    }
}

float *Modulator::modBlock(int len) {
    int k = 0;
    
    for (int i = 0; i < len; i++) {
        float v = 0;
        if (this->mod.phase_inc > 0) {
            v += sin(this->mod.phase);
            this->mod.phase += this->mod.phase_inc;
        }
        
        this->output[k++] = v;
    }
    
    return (&this->output[0]);
}
