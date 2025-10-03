/*
  ==============================================================================

    MidiTypes.cpp
    Created: 25 Sep 2025 10:29:11pm
    Author:  ikamo

  ==============================================================================
*/

#include "Registry.h"

void WaviateFlow2025AudioProcessor::initializeRegistryMidi() {
    NodeType::registryCreatePrefix = 400000;


    {
        NodeType t(129);
        t.name = "midi hz";
        t.address = "audio/midi/";
        t.tooltip = "Converts MIDI note numbers to frequencies in Hz.";
        t.inputs = {
            InputFeatures("midi", InputType::integer, 0, false)
        };
        t.getOutputSize = outputSizeByInputPin(0); // same length as input
        t.buildUI = [](NodeComponent&, NodeData&) {};
        t.onResized = [](NodeComponent&) {};
        t.execute = [](const NodeData&, UserInput&, const std::vector<std::span<ddtype>>& in,
            std::span<ddtype> out, const RunnerInput&)
            {
                const auto& midi = in[0];
                int n = static_cast<int>(midi.size());

                for (int i = 0; i < n; i++) {
                    int m = midi[i].i;
                    double hz = 440.0 * std::pow(2.0, (m - 69) / 12.0);
                    out[i].d = hz;
                }
            };
        t.whichInputToFollowWildcard = 0;
        t.outputType = InputType::decimal;
        t.alwaysOutputsRuntimeData = false;
        t.fromScene = nullptr;
        registry.push_back(t);
    }

    // MIDI Controller value node
    {
        NodeType t(130);
        t.name = "midi cc value";
        t.address = "audio/midi/";
        t.tooltip = "Reads MIDI controller (CC) values from UserInput, normalized 0..1.";
        t.inputs = {
            InputFeatures("ccIndex", InputType::integer, 0, false)
        };
        t.getOutputSize = outputSizeByInputPin(0); // same length as indices
        t.buildUI = [](NodeComponent&, NodeData&) {};
        t.onResized = [](NodeComponent&) {};
        t.execute = [](const NodeData&, UserInput& userInput,
            const std::vector<std::span<ddtype>>& in,
            std::span<ddtype> out, const RunnerInput&)
            {
                const auto& ccIdx = in[0];
                int n = static_cast<int>(ccIdx.size());

                for (int i = 0; i < n; ++i)
                {
                    int cc = ccIdx[i].i;
                    float val = 0.0f;
                    if (cc >= 0 && cc < 128)
                        val = userInput.midiCCValues[cc];
                    out[i].d = val; // already normalized 0..1
                }
            };
        t.whichInputToFollowWildcard = 0;
        t.outputType = InputType::decimal;
        t.alwaysOutputsRuntimeData = true;
        t.fromScene = nullptr;
        registry.push_back(t);
    }


    NodeType velocity(32);
    velocity.name = "velocity";
    velocity.address = "audio/midi/";
    velocity.tooltip = "Vector of 128 values for all MIDI note velocities";
    velocity.inputs = {};
    velocity.getOutputSize = outputSizeAllMidi;
    velocity.buildUI = [](NodeComponent&, NodeData&) {};
    velocity.onResized = [](NodeComponent&) {};
    velocity.execute = [](const NodeData&, UserInput& userInput, const std::vector<std::span<ddtype>>&, std::span<ddtype> output, const RunnerInput& inlineInstance)
        {
            for (int i = 0; i < 128; i += 1) {
                output[i] = userInput.noteVelocity[i];
            }

        };
    velocity.outputType = InputType::decimal;
    velocity.alwaysOutputsRuntimeData = true;
    velocity.fromScene = nullptr;
    registry.push_back(velocity);
    keyCodeTypeMapping.insert({ juce::KeyPress::createFromDescription("shift v").getTextDescription(), registry.size() - 1 });


    NodeType pitchWheelType(33);
    pitchWheelType.name = "pitch wheel";
    pitchWheelType.address = "audio/midi/";
    pitchWheelType.tooltip = "Current MIDI pitch wheel value (runtime).";
    pitchWheelType.inputs = {};
    pitchWheelType.getOutputSize = outputSize1Known;
    pitchWheelType.buildUI = [](NodeComponent&, NodeData&) {};
    pitchWheelType.onResized = [](NodeComponent&) {};
    pitchWheelType.execute = [](const NodeData&, UserInput& userInput, const std::vector<std::span<ddtype>>&, std::span<ddtype> output, const RunnerInput& inlineInstance)
        {
            output[0] = userInput.pitchWheelValue;
        };
    pitchWheelType.outputType = InputType::decimal;
    pitchWheelType.alwaysOutputsRuntimeData = true;
    pitchWheelType.fromScene = nullptr;
    registry.push_back(pitchWheelType);
    keyCodeTypeMapping.insert({ juce::KeyPress::createFromDescription("p").getTextDescription(), registry.size() - 1 });

    NodeType allNotesType(34);
    allNotesType.name = "note on";
    allNotesType.address = "audio/midi/";
    allNotesType.tooltip = "Vector of 128 booleans for all MIDI notes.";
    allNotesType.inputs = {};
    allNotesType.getOutputSize = outputSizeAllMidi;
    allNotesType.buildUI = [](NodeComponent&, NodeData&) {};
    allNotesType.onResized = [](NodeComponent&) {};
    allNotesType.execute = [](const NodeData&, UserInput& userInput, const std::vector<std::span<ddtype>>&, std::span<ddtype> output, const RunnerInput& inlineInstance)
        {
            for (int i = 0; i < 128; ++i) output[i] = userInput.notesOn[i];
        };
    allNotesType.outputType = InputType::boolean;
    allNotesType.alwaysOutputsRuntimeData = true;
    allNotesType.fromScene = nullptr;
    registry.push_back(allNotesType);
    keyCodeTypeMapping.insert({ juce::KeyPress::createFromDescription("shift n").getTextDescription(), registry.size() - 1 });

    NodeType waveCycleType(35);
    waveCycleType.name = "wave cycle";
    waveCycleType.address = "audio/midi/";
    waveCycleType.tooltip = "Per-note cycle phase in [0,1) for 128 MIDI notes.";
    waveCycleType.inputs = {};
    waveCycleType.getOutputSize = outputSizeAllMidi;
    waveCycleType.buildUI = [](NodeComponent&, NodeData&) {};
    waveCycleType.onResized = [](NodeComponent&) {};
    waveCycleType.execute = [](const NodeData&, UserInput& userInput, const std::vector<std::span<ddtype>>&, std::span<ddtype> output, const RunnerInput& inlineInstance)
        {
            for (int i = 0; i < 128; ++i) output[i] = userInput.noteCycle[i];
        };
    waveCycleType.outputType = InputType::decimal;
    waveCycleType.alwaysOutputsRuntimeData = true;
    waveCycleType.fromScene = nullptr;
    registry.push_back(waveCycleType);
    keyCodeTypeMapping.insert({ juce::KeyPress::createFromDescription("w").getTextDescription(), registry.size() - 1 });


    // ======== Envelope (ADSR, per MIDI note) ========
    NodeType envelopeType(49);
    envelopeType.name = "envelope (ADSR)";
    envelopeType.address = "audio/midi/envelopes/";
    envelopeType.tooltip = "Per-note ADSR: outputs 128 values in [0,1] from note start/end and ADSR times.";
    envelopeType.inputs = {
        // scalar controls (seconds for A/D/R; [0,1] for sustain)
        InputFeatures("attack (s)",  InputType::decimal, 1, false),
        InputFeatures("decay (s)",   InputType::decimal, 1, false),
        InputFeatures("sustain",     InputType::decimal, 1, false),
        InputFeatures("release (s)", InputType::decimal, 1, false)
    };
    envelopeType.inputs[0].defaultValue = 0.01;
    envelopeType.inputs[0].defaultValue = 0.2;
    envelopeType.inputs[0].defaultValue = 0.8;
    envelopeType.inputs[0].defaultValue = 0.3;
    envelopeType.getOutputSize = outputSizeAllMidi;
    envelopeType.buildUI = [](NodeComponent&, NodeData&) {};
    envelopeType.onResized = [](NodeComponent&) {};
    envelopeType.outputType = InputType::decimal;
    envelopeType.alwaysOutputsRuntimeData = true;
    envelopeType.fromScene = nullptr;

    envelopeType.execute = [](
        const NodeData&,
        UserInput& userInput,
        const std::vector<std::span<ddtype>>& in,
        std::span<ddtype> out, const RunnerInput& inlineInstance)
        {
            // Read scalar parameters (with sane clamps)
            const double A = (in.size() > 0 && !in[0].empty()) ? std::max(0.0, in[0][0].d) : 0.0;
            const double D = (in.size() > 1 && !in[1].empty()) ? std::max(0.0, in[1][0].d) : 0.0;
            const double S = (in.size() > 2 && !in[2].empty()) ? std::clamp(in[2][0].d, 0.0, 1.0) : 0.0;
            const double R = (in.size() > 3 && !in[3].empty()) ? std::max(0.0, in[3][0].d) : 0.0;

            // Current time in frames and seconds
            const double curFrame = userInput.numFramesStartOfBlock + userInput.sampleInBlock;
            const double sr = (userInput.sampleRate > 0.0 ? userInput.sampleRate : 44100.0);

            // Helpers to avoid division by zero in linear ramps
            auto linUp = [](double t, double T) -> double {
                if (T <= 0.0) return 1.0;           // instantaneous to 1
                return std::clamp(t / T, 0.0, 1.0); // 0 -> 1
                };
            auto linDownTo = [](double t, double T, double target) -> double {
                // starts at 1.0 at t=0, linearly reaches target at t=T
                if (T <= 0.0) return target;
                double v = 1.0 - (t / T) * (1.0 - target);
                return std::clamp(v, target, 1.0);
                };
            auto releaseFrom = [](double t, double T, double startVal) -> double {
                if (T <= 0.0) return 0.0;
                double v = startVal * (1.0 - t / T);
                return std::clamp(v, 0.0, startVal);
                };

            // Compute per-note envelope
            for (int n = 0; n < 128; ++n) {
                const bool  on = (userInput.notesOn[n] != 0.0);
                const double tOnF = userInput.noteStartFrame[n];
                const double tOffF = userInput.noteEndFrame[n];

                // Default
                double env = 0.0;

                if (on) {
                    // NOTE IS HELD: time since key-down
                    const double tHeldSec = (curFrame - tOnF) / sr;
                    if (tHeldSec <= 0.0) {
                        env = 0.0;
                    }
                    else if (tHeldSec < A) {
                        // Attack: 0 -> 1
                        env = linUp(tHeldSec, A);
                    }
                    else if (tHeldSec < A + D) {
                        // Decay: 1 -> S
                        env = linDownTo(tHeldSec - A, D, S);
                    }
                    else {
                        // Sustain
                        env = S;
                    }
                }
                else {
                    // NOTE IS RELEASED: compute level at release moment, then decay
                    // If tOffF <= tOnF, or not meaningful, env=0 (no valid note)
                    const double holdDurSec = (tOffF - tOnF) / sr;
                    if (holdDurSec <= 0.0 || curFrame < tOffF) {
                        // No valid release, or we're before the off time
                        env = 0.0;
                    }
                    else {
                        // Level at the exact release moment:
                        double levelAtRelease = 0.0;
                        if (holdDurSec < A) {
                            levelAtRelease = linUp(holdDurSec, A); // still in attack when released
                        }
                        else if (holdDurSec < A + D) {
                            levelAtRelease = linDownTo(holdDurSec - A, D, S); // in decay when released
                        }
                        else {
                            levelAtRelease = S; // at sustain when released
                        }

                        // Time since release:
                        const double tRelSec = (curFrame - tOffF) / sr;
                        if (tRelSec <= 0.0) {
                            env = levelAtRelease;
                        }
                        else {
                            env = releaseFrom(tRelSec, R, levelAtRelease);
                        }
                    }
                }

                out[n] = std::clamp(env, 0.0, 1.0);
            }
        };

    registry.push_back(envelopeType);
    keyCodeTypeMapping.insert({ juce::KeyPress::createFromDescription("e").getTextDescription(), registry.size() - 1 });

}