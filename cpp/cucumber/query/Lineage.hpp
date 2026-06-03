#ifndef CUCUMBER_QUERY_LINEAGE_HPP
#define CUCUMBER_QUERY_LINEAGE_HPP

#include "cucumber/messages/all.hpp"
#include "cucumber/messages/background.hpp"
#include "cucumber/messages/examples.hpp"
#include "cucumber/messages/feature.hpp"
#include "cucumber/messages/gherkin_document.hpp"
#include "cucumber/messages/rule.hpp"
#include "cucumber/messages/scenario.hpp"
#include "cucumber/messages/table_row.hpp"
#include <cstddef>
#include <optional>

namespace cucumber::query
{

    struct Lineage
    {
        std::optional<cucumber::messages::gherkin_document> gherkinDocument;
        std::optional<cucumber::messages::feature> feature;
        std::optional<cucumber::messages::background> background;
        std::optional<cucumber::messages::rule> rule;
        std::optional<cucumber::messages::background> ruleBackground;
        std::optional<cucumber::messages::scenario> scenario;
        std::optional<cucumber::messages::examples> examples;
        std::optional<std::size_t> examplesIndex;
        std::optional<cucumber::messages::table_row> example;
        std::optional<std::size_t> exampleIndex;
    };

} // namespace cucumber::query

#endif
