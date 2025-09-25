#pragma once

namespace gladius::nodes
{
    class Assembly;
    class Model;
}

namespace gladius::nodes
{
    class OptimizeOutputs
    {
      public:
        OptimizeOutputs(Assembly * assembly);
        void optimize();

      private:
        Assembly * m_assembly{nullptr};

        void markFunctionOutputsAsUnused(Model & model);
        void markUsedOutputs(Model & model);
        void propagateUsedFunctionOutputs(Model & model);
    };
} // namespace gladius::nodes
