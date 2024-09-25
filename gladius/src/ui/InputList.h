#pragma once

#include <Model.h>

namespace gladius::ui
{
    using OptionalPortId = std::optional<nodes::PortId>;
    auto inputMenu(nodes::Model & nodes, nodes::ParameterId targetId) -> OptionalPortId;
} // namespace gladius::ui
