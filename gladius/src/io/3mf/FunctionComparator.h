#pragma once

#include <lib3mf_implicit.hpp>

namespace gladius::io
{
    /**
     * @brief Compares two implicit functions for equality.
     * 
     * This function compares two Lib3MF::CImplicitFunction instances to determine if they represent
     * the same function. The comparison checks:
     * - Display names
     * - Nodes and their properties
     * - Input ports
     * - Output ports
     * - References and links between nodes
     * 
     * This is useful when merging models to avoid duplicate functions.
     * 
     * @param function1 The first function to compare
     * @param function2 The second function to compare
     * @return true if the functions are equivalent, false otherwise
     */
    bool areImplicitFunctionsEqual(Lib3MF::CImplicitFunction const* function1, 
                                  Lib3MF::CImplicitFunction const* function2);
                             
    /**
     * @brief Compares ports from two functions for equality.
     * 
     * @param portIterator1 Iterator for ports from the first function
     * @param portIterator2 Iterator for ports from the second function
     * @param ignoreReference Whether to ignore references when comparing ports
     * @return true if the ports are equal, false otherwise
     */
    bool arePortsEqual(Lib3MF::PImplicitPortIterator const& portIterator1,
                      Lib3MF::PImplicitPortIterator const& portIterator2,
                      bool ignoreReference = false);

} // namespace gladius::io
