#pragma once

#include "Model.h"
#include "nodesfwd.h"
#include <filesystem>
#include <optional>
#include <stdexcept>
#include <unordered_map>

namespace gladius::nodes
{
    using Models = std::map<ResourceId, SharedModel>; // The models should be sorted for convience
    using ModelNames = std::vector<std::string>;

    using AssemblyException = std::runtime_error;

    using OptionalFAllbackvalue = std::optional<double>;

    class ModelDoesNotExist : public AssemblyException
    {
      public:
        explicit ModelDoesNotExist(NodeName const & missingModelName)
            : AssemblyException("The Assembly does not contain a model with the name " +
                                missingModelName)
        {
        }
    };
    class ModelDoesAlreadyExist : public AssemblyException
    {
      public:
        explicit ModelDoesAlreadyExist(NodeName const & nameOfTheModelToAdd)
            : AssemblyException("The Assembly does already contain a model with the name " +
                                nameOfTheModelToAdd)
        {
        }
    };

    class Assembly
    {
      public:
        Assembly();

        // copy ctor
        Assembly(Assembly const & other);

        void visitNodes(Visitor & visitor);

        void visitAssemblyNodes(Visitor & visitor);

        [[nodiscard]] auto getFunctions() -> Models &;

        auto assemblyModel() -> SharedModel &;

        auto addModelIfNotExisting(ResourceId id) -> bool;

        auto getModelId(std::string const & name) -> int;

        void deleteModel(ResourceId id);

        bool equals(Assembly const & other);

        void setFilename(std::filesystem::path fileName)
        {
            m_fileName = fileName;
        }

        [[nodiscard]] std::filesystem::path getFilename() const
        {
            return m_fileName;
        }

        [[nodiscard]] bool isValid();

        void setAssemblyModelId(ResourceId id)
        {
            m_assemblyModelId = id;
        }

        [[nodiscard]] ResourceId getAssemblyModelId() const
        {
            return m_assemblyModelId;
        }

        SharedModel findModel(ResourceId id);

        void updateInputsAndOutputs();

        void setFallbackValueLevelSet(OptionalFAllbackvalue value)
        {
            m_FallbackValueLevelSet = value;
        }

        [[nodiscard]] OptionalFAllbackvalue getFallbackValueLevelSet() const
        {
            return m_FallbackValueLevelSet;
        }

      private:
        Models m_subModels;
        ResourceId m_assemblyModelId{};
        std::filesystem::path m_fileName{};

        OptionalFAllbackvalue m_FallbackValueLevelSet{};
    };
} // namespace gladius::nodes
