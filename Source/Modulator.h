/*
  ==============================================================================

    Modulator.h
    Created: 9 Feb 2022 4:29:54pm
    Author:  Kevin Kopczynski

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>

#define MAX_BUFFER_SIZE     2048

typedef struct {
  double freq;
  double phase_inc;
  double phase;
} Mod;

class Modulator
{
public:
    Modulator();
    ~Modulator();
    void initMod(int fs);
    void setMod(float in_freq);
    void updateMod(float new_freq);
    float *modBlock(int len);
    
    Mod mod;
    int samp_rate;
    float output[MAX_BUFFER_SIZE];
};