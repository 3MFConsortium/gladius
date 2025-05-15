#include "Document.h"

namespace gladius
{
    AssemblyToken Document::waitForAssemblyToken() const
    {
        return AssemblyToken(m_assemblyMutex);
    }

    OptionalAssemblyToken Document::requestAssemblyToken() const
    {
        if (!m_assemblyMutex.try_lock())
        {
            return {};
        }
        std::lock_guard<std::mutex> lock(m_assemblyMutex, std::adopt_lock);
        return OptionalAssemblyToken(m_assemblyMutex);
    }
}
