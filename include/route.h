#pragma once

#include "parameter.h"
#include "modulator.h"
#include <map>
#include <vector>
#include <algorithm>

//Routes let you send a Modulator into a Parameter
//The Depth Param adjusts the amount, and can be targeted like any other Param
//A second Modulator can be added as source2 and Operations can be performed on the pair
//Routes are themselves modulators, and can be used/targeted by other routes
struct Route : public Modulator {
    enum Operation { MULT, ADD, SUB, MIN, MAX, XOR, CLAMP, FMOD, FOLD, OP_COUNT };
    Operation operation;
    Modulator* source1 = nullptr;
    Modulator* source2 = nullptr;
    Param* destination = nullptr;
    const char* prefix = nullptr;
    Param depth;

    Route(Modulator* src1 = nullptr, Param* dest = nullptr, Modulator* src2 = nullptr, const char* pre = nullptr,
        Operation op = MULT);

    void step();
};

namespace RouteGraph {
    constexpr uint8_t MAX_NODES = 20 * 4; //max 4 nodes per route * 20 routes
    constexpr uint8_t UNDISCOVERED = 0xFE, BLANK = 0xFF; //unused Node entries are flagged "BLANK"
    struct Node {
        uint8_t index = BLANK;
        uint8_t lowLink = BLANK;
        std::vector<uint8_t> children;
    };

    void generateModOrder(Route* routes, const uint8_t numRoutes, std::map<Param*, Modulator*> paramToOwner, std::vector<Modulator *> & modSort);
}