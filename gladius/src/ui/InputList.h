#pragma once

#include <Model.h>

namespace gladius::ui
{
    using OptionalPortId = std::optional<nodes::PortId>;
    auto inputMenu(nodes::Model & nodes, gladius::nodes::VariantParameter const & targetParameter, std::string const & targetName) -> OptionalPortId;
} // namespace gladius::ui
