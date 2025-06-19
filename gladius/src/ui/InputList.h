#pragma once

#include <Model.h>

namespace gladius::ui
{
    using OptionalPortId = std::optional<nodes::PortId>;
    auto inputMenu(nodes::Model & nodes, gladius::nodes::VariantParameter targetParameter, std::string targetName) -> OptionalPortId;
} // namespace gladius::ui
