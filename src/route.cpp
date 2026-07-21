#include "route.h"

Route::Route(Modulator* src1, Param* dest, Modulator* src2, const char* pre, Operation op)
    : source1(src1), source2(src2), destination(dest), prefix(pre),
        depth { "Depth", Param::toPercentStr, &prefix },
        operation { "Operation", _operation.getStr, &prefix } {
    _operation.value = op;
}

void Route::step() {
    depth.commit();
    _operation.update(operation);

    if (!source1) {
        output = 0;
        return;
    }

    float combined = source1->output; //default
    if (source2) {
        float& input1 = source1->output; //alias
        float& input2 = source2->output; //alias

        switch (_operation.value) {
            case MULT:  combined = input1 * input2; break;
            case ADD:   combined = input1 + input2; break;
            case SUB:   combined = input1 - input2; break;
            case MIN:   combined = min(input1, input2); break;
            case MAX:   combined = max(input1, input2); break;
            case XOR:   combined = abs(input1 - input2) - 1; break;
            case CLAMP: {
                float limit = abs(input2);
                combined = max(-limit, min(input1, limit));
                break;
            }
            case FMOD: { //guard against undefined fmod of 0
                float sign = (input2 >= 0) ? 1 : -1;
                combined = ((input2 != 0) ? fmod(input1, input2) : input1) * sign;
                break;
            }
            case FOLD: {
                if (input2 != 0) {
                    const float FOLD_LIM = .05;
                    float in2 = 1 - abs(input2);
                    in2 = in2 * in2 + FOLD_LIM; //expo response
                    float x = (input1 / in2); //scale
                    float wrap = x - 4.0 * floor((x + 1) / 4.0);
                    combined = (wrap <= 1) ? wrap : (2 - wrap);
                } else {
                    combined = input1;
                }
                break;
            }
        }
    }

    output = combined * depth.get();
    if (destination) destination->summingNode += output;
}

namespace RouteGraph {
    //turn the routes into an ordered list of Modulators
    void generateModOrder(Route* routes, const uint8_t numRoutes, std::map<Param*, Modulator*> paramToOwner, std::vector<Modulator *> & modSort) {
        std::map<Modulator *, uint8_t> modToNodeMap;
        uint8_t nodeCount = 0; //running count use to assign node numbers
        Node nodes[MAX_NODES];

        modSort.clear();
        if (!numRoutes) return; //nothing to process

        //Helper functions-------------------------------------------------------------------------
        //adds children to Nodes and initializes indexes
        auto addChild = [&](uint8_t parent, uint8_t child) {
            auto& children = nodes[parent].children; //avoid duplicate children
            if (std::find(children.begin(), children.end(), child) == children.end()) children.push_back(child);

            nodes[child].index = nodes[parent].index = UNDISCOVERED; //flag nodes to say they exist
        };

        //assign/get node # for a modulator pointer
        auto modToNode = [&](Modulator * mod) {
            if (auto it = modToNodeMap.find(mod); it != modToNodeMap.end()) return it->second; //found, return node #

            modToNodeMap[mod] = nodeCount;
            return nodeCount++;
        };

        auto nodeToMod = [&](uint8_t nodeNum)-> Modulator * {
            for (auto& [mod, node] : modToNodeMap)
                if (node == nodeNum) return mod;

            return nullptr;
        };

        //Init Nodes from routes-------------------------------------------------------------------
        for (uint8_t i = 0; i < numRoutes; i++) { //init nodes with their children
            Route& route = routes[i]; //alias
            uint8_t routeNode = modToNode(&route);

            //the two inputs are nodes, and their edges point into the route
            if (route.source1) addChild(modToNode(route.source1), routeNode);
            if (route.source2) addChild(modToNode(route.source2), routeNode);
            if (route.destination) //the route is a node, and its edge points to the dest owner
                if (auto it = paramToOwner.find(route.destination); it != paramToOwner.end() && it->second)
                    addChild(routeNode, modToNode(it->second));  //add paramOwner
        }

        //Tarjan's SCC Algo------------------------------------------------------------------------
        //Finds the earliest node in walk that can be reached by moving forward (lowLink)
        //When we find the node with an index matching this lowLink (root), we emit it and the walk that reached it (SCC)
        //This could be a cycle, or a singleton. The emission order is the reverse topological order
        std::vector<uint8_t> sccStack, dfsStack, output;
        uint8_t nextNode = 0, indexCount = 0;

        auto tarjanStep = [&]() {
            const uint8_t node = nextNode;
            auto& [index, lowLink, children] = nodes[node];

            if (index == UNDISCOVERED) { //found new node
                lowLink = index = indexCount++; //give it a new index
                dfsStack.push_back(node);
                sccStack.push_back(node);
            }

            if (!children.empty()) {
                const uint8_t child = children.back(), childIndex = nodes[child].index;
                children.pop_back();

                if (childIndex == UNDISCOVERED) nextNode = child; //dive into child if undiscovered
                else if (childIndex < lowLink) //if next node is a back-edge, inherit lowLink
                    if (auto it = std::find(sccStack.begin(), sccStack.end(), child); it != sccStack.end())
                        lowLink = childIndex;
            } else { //ran out of undiscovered children
                if (lowLink == index) { //found a root, splice out members to emit in *reverse*
                    auto rit = std::find(sccStack.rbegin(), sccStack.rend(), node); //find start
                    output.insert(output.end(), sccStack.rbegin(), std::next(rit)); //graft onto output
                    sccStack.erase(std::next(rit).base(), sccStack.end()); //remove from stack
                }

                dfsStack.pop_back();
                if (!dfsStack.empty()) {
                    nextNode = dfsStack.back(); //ascend back up the stack

                    //make parent inherit lowLink from us
                    if (lowLink < nodes[nextNode].lowLink) nodes[nextNode].lowLink = lowLink;
                } else { //ran out of parents
                    nextNode = BLANK; //find a new node to start on that hasn't been discovered yet
                    for (uint8_t i = 0; i < nodeCount && nextNode == BLANK; i++)
                        if (nodes[i].index == UNDISCOVERED) nextNode = i;

                    if (nextNode == BLANK) return false; //ran out of new nodes, done
                }
            }

            return true; //need another pass
        };

        while (tarjanStep()) { } //loop until we run out of nodes

        //convert the output back into modulators, and put them in the external modSort vector reversed
        std::transform(output.rbegin(), output.rend(), std::back_inserter(modSort),
            [&](uint8_t node) { return nodeToMod(node); });
    }
}