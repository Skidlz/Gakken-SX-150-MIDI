#pragma once

#include "parameter.h"
#include "modulator.h"

//idea, add operations to routes other than the implicit multiply of "depth"
//  add, subtract, min/max, clamp, modulo (?), xor abs(A - B)

//Voice owns a finite list of routes
//Each route is ticked, and the output is summed onto each Param before being "finalized"

//Routes let you send a Modulator into a Parameter
//The Depth param adjusts the amount, and works like any other Param
//A second Modulator can be added as a depthSource, that will multiply the first
//Routes are themselves modulators, and can be used/targeted by other routes
struct Route : public Modulator {
    Modulator* source = nullptr;
    Param* destination = nullptr;
    Modulator* depthSource = nullptr;
    const char* prefix = nullptr;
    Param depth;

    Route(Modulator* src = nullptr, Param* dest = nullptr, Modulator* depthSrc = nullptr, const char* pre = nullptr)
        : source(src), depthSource(depthSrc), destination(dest), prefix(pre),
        depth { "Depth", Param::toPercentStr, &prefix } { }

    void step() {
        if (!source) {
            output = 0;
            return;
        }

        float d = depth.get() + depth.modulation;
        if (depthSource) d *= depthSource->output;

        output = source->output * d; //test .5 depth
        if (destination) destination->summingNode += output;
    }
};