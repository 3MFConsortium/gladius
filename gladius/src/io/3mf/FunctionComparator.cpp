#include "FunctionComparator.h"
#include <algorithm>
#include <map>

namespace gladius::io
{
    bool arePortsEqual(Lib3MF::PImplicitPortIterator const& portIterator1,
                      Lib3MF::PImplicitPortIterator const& portIterator2,
                      bool ignoreReference)
    {
        if (portIterator1->Count() != portIterator2->Count()) {
            return false;
        }

        // Create copies of the iterators because MoveNext() modifies them
        auto iter1 = portIterator1;
        auto iter2 = portIterator2;

        // Reset iterators to the beginning
        while (iter1->MoveNext()) {}
        while (iter2->MoveNext()) {}
        
        // Store ports from first iterator in a map for easy lookup
        std::map<std::string, Lib3MF::PImplicitPort> portsMap;
        while (iter1->MoveNext()) {
            auto port = iter1->GetCurrent();
            portsMap[port->GetIdentifier()] = port;
        }
        
        // Check each port from second iterator against the map
        while (iter2->MoveNext()) {
            auto port2 = iter2->GetCurrent();
            std::string identifier = port2->GetIdentifier();
            
            if (portsMap.find(identifier) == portsMap.end()) {
                return false;  // Port identifier not found
            }
            
            auto port1 = portsMap[identifier];
            
            // Check properties that must match
            if (port1->GetType() != port2->GetType() ||
                port1->GetDisplayName() != port2->GetDisplayName()) {
                return false;
            }
            
            // Check reference if required
            if (!ignoreReference && port1->GetReference() != port2->GetReference()) {
                return false;
            }
        }
        
        return true;
    }

    bool areImplicitFunctionsEqual(Lib3MF::CImplicitFunction const* function1,
                                  Lib3MF::CImplicitFunction const* function2)
    {
        if (!function1 || !function2) {
            // Consider nullptr functions equal to themselves only
            return function1 == function2;
        }

        // Check basic properties
        if (const_cast<Lib3MF::CImplicitFunction*>(function1)->GetDisplayName() != 
            const_cast<Lib3MF::CImplicitFunction*>(function2)->GetDisplayName()) {
            return false;
        }

        // Compare nodes
        auto nodeIterator1 = const_cast<Lib3MF::CImplicitFunction*>(function1)->GetNodes();
        auto nodeIterator2 = const_cast<Lib3MF::CImplicitFunction*>(function2)->GetNodes();
        
        if (nodeIterator1->Count() != nodeIterator2->Count()) {
            return false;
        }
        
        // Build a map of nodes from function1 for quick lookup by identifier
        std::map<std::string, Lib3MF::PImplicitNode> nodesMap;
        while (nodeIterator1->MoveNext()) {
            auto node = nodeIterator1->GetCurrent();
            nodesMap[node->GetIdentifier()] = node;
        }
        
        // Compare each node from function2 with the corresponding node in function1
        while (nodeIterator2->MoveNext()) {
            auto node2 = nodeIterator2->GetCurrent();
            std::string identifier = node2->GetIdentifier();
            
            if (nodesMap.find(identifier) == nodesMap.end()) {
                return false;  // Node with this identifier not found
            }
            
            auto node1 = nodesMap[identifier];
            
            // Check basic node properties
            if (node1->GetNodeType() != node2->GetNodeType() ||
                node1->GetDisplayName() != node2->GetDisplayName()) {
                return false;
            }
            
            // Check node types validity
            if (node1->AreTypesValid() != node2->AreTypesValid()) {
                return false;
            }
            
            // For constant nodes, check the constant value
            if (node1->GetNodeType() == Lib3MF::eImplicitNodeType::Constant) {
                auto constantNode1 = dynamic_cast<Lib3MF::CConstantNode*>(node1.get());
                auto constantNode2 = dynamic_cast<Lib3MF::CConstantNode*>(node2.get());
                
                if (!constantNode1 || !constantNode2) {
                    return false;
                }
                
                if (constantNode1->GetConstant() != constantNode2->GetConstant()) {
                    return false;
                }
            }
            
            // For constant vector nodes, check vector values
            else if (node1->GetNodeType() == Lib3MF::eImplicitNodeType::ConstVec) {
                auto vecNode1 = dynamic_cast<Lib3MF::CConstVecNode*>(node1.get());
                auto vecNode2 = dynamic_cast<Lib3MF::CConstVecNode*>(node2.get());
                
                if (!vecNode1 || !vecNode2) {
                    return false;
                }
                
                auto vec1 = vecNode1->GetVector();
                auto vec2 = vecNode2->GetVector();
                
                for (int i = 0; i < 3; i++) {
                    if (vec1.m_Coordinates[i] != vec2.m_Coordinates[i]) {
                        return false;
                    }
                }
            }
            
            // For constant matrix nodes, check matrix values
            else if (node1->GetNodeType() == Lib3MF::eImplicitNodeType::ConstMat) {
                auto matNode1 = dynamic_cast<Lib3MF::CConstMatNode*>(node1.get());
                auto matNode2 = dynamic_cast<Lib3MF::CConstMatNode*>(node2.get());
                
                if (!matNode1 || !matNode2) {
                    return false;
                }
                
                auto mat1 = matNode1->GetMatrix();
                auto mat2 = matNode2->GetMatrix();
                
                for (int i = 0; i < 4; i++) {
                    for (int j = 0; j < 3; j++) {
                        if (mat1.m_Field[i][j] != mat2.m_Field[i][j]) {
                            return false;
                        }
                    }
                }
            }
            
            // For resource ID nodes, check resource IDs
            else if (node1->GetNodeType() == Lib3MF::eImplicitNodeType::ConstResourceID) {
                auto resNode1 = dynamic_cast<Lib3MF::CResourceIdNode*>(node1.get());
                auto resNode2 = dynamic_cast<Lib3MF::CResourceIdNode*>(node2.get());
                
                if (!resNode1 || !resNode2) {
                    return false;
                }
                
                auto res1 = resNode1->GetResource();
                auto res2 = resNode2->GetResource();
                
                if ((res1 == nullptr) != (res2 == nullptr)) {
                    return false;
                }
                
                if (res1 && res2 && res1->GetModelResourceID() != res2->GetModelResourceID()) {
                    return false;
                }
            }
            
            // Compare input and output ports
            if (!arePortsEqual(node1->GetInputs(), node2->GetInputs(), false)) {
                return false;
            }
            
            if (!arePortsEqual(node1->GetOutputs(), node2->GetOutputs(), true)) {
                return false;
            }
        }
        
        // Compare function inputs
        auto inputs1 = const_cast<Lib3MF::CImplicitFunction*>(function1)->GetInputs();
        auto inputs2 = const_cast<Lib3MF::CImplicitFunction*>(function2)->GetInputs();
        
        if (inputs1->Count() != inputs2->Count()) {
            return false;
        }
        
        std::map<std::string, Lib3MF::PImplicitPort> inputsMap;
        while (inputs1->MoveNext()) {
            auto input = inputs1->GetCurrent();
            inputsMap[input->GetIdentifier()] = input;
        }
        
        while (inputs2->MoveNext()) {
            auto input2 = inputs2->GetCurrent();
            std::string identifier = input2->GetIdentifier();
            
            if (inputsMap.find(identifier) == inputsMap.end()) {
                return false;
            }
            
            auto input1 = inputsMap[identifier];
            
            if (input1->GetDisplayName() != input2->GetDisplayName() ||
                input1->GetType() != input2->GetType()) {
                return false;
            }
        }
        
        // Compare function outputs
        auto outputs1 = const_cast<Lib3MF::CImplicitFunction*>(function1)->GetOutputs();
        auto outputs2 = const_cast<Lib3MF::CImplicitFunction*>(function2)->GetOutputs();
        
        if (outputs1->Count() != outputs2->Count()) {
            return false;
        }
        
        std::map<std::string, Lib3MF::PImplicitPort> outputsMap;
        while (outputs1->MoveNext()) {
            auto output = outputs1->GetCurrent();
            outputsMap[output->GetIdentifier()] = output;
        }
        
        while (outputs2->MoveNext()) {
            auto output2 = outputs2->GetCurrent();
            std::string identifier = output2->GetIdentifier();
            
            if (outputsMap.find(identifier) == outputsMap.end()) {
                return false;
            }
            
            auto output1 = outputsMap[identifier];
            
            if (output1->GetDisplayName() != output2->GetDisplayName() ||
                output1->GetType() != output2->GetType()) {
                return false;
            }
        }
        
        return true;
    }

    Lib3MF::PImplicitFunction findEquivalentFunction(Lib3MF::CModel & model,
                                                     Lib3MF::CImplicitFunction & function)
    {
        // Get all resources in the model
        auto resourceIterator = const_cast<Lib3MF::CModel&>(model).GetResources();
        
        // Iterate through resources
        while (resourceIterator->MoveNext()) {
            auto resource = resourceIterator->GetCurrent();
            
            // Try to cast resource to ImplicitFunction
            auto existingFunction = std::dynamic_pointer_cast<Lib3MF::CImplicitFunction>(resource);
            
            // Skip if not an implicit function
            if (!existingFunction) {
                continue;
            }
            
            // Skip comparing function with itself (if it already exists in the model)
            if (function.GetResourceID() == existingFunction->GetResourceID()) {
                continue;
            }
            
            // Check if the functions are equivalent
            if (areImplicitFunctionsEqual(&function, existingFunction.get())) {
                return existingFunction;
            }
        }
        
        // No equivalent function found
        return nullptr;
    }

} // namespace gladius::io
