#ifndef CUCUMBER_QUERY_QUERY_HPP
#define CUCUMBER_QUERY_QUERY_HPP

#include "cucumber/messages/all.hpp"
#include "cucumber/messages/attachment.hpp"
#include "cucumber/messages/duration.hpp"
#include "cucumber/messages/envelope.hpp"
#include "cucumber/messages/feature.hpp"
#include "cucumber/messages/gherkin_document.hpp"
#include "cucumber/messages/hook.hpp"
#include "cucumber/messages/location.hpp"
#include "cucumber/messages/meta.hpp"
#include "cucumber/messages/pickle.hpp"
#include "cucumber/messages/pickle_step.hpp"
#include "cucumber/messages/rule.hpp"
#include "cucumber/messages/scenario.hpp"
#include "cucumber/messages/step.hpp"
#include "cucumber/messages/step_definition.hpp"
#include "cucumber/messages/suggestion.hpp"
#include "cucumber/messages/test_case.hpp"
#include "cucumber/messages/test_case_finished.hpp"
#include "cucumber/messages/test_case_started.hpp"
#include "cucumber/messages/test_run_finished.hpp"
#include "cucumber/messages/test_run_hook_finished.hpp"
#include "cucumber/messages/test_run_hook_started.hpp"
#include "cucumber/messages/test_run_started.hpp"
#include "cucumber/messages/test_step.hpp"
#include "cucumber/messages/test_step_finished.hpp"
#include "cucumber/messages/test_step_result.hpp"
#include "cucumber/messages/test_step_result_status.hpp"
#include "cucumber/messages/test_step_started.hpp"
#include "cucumber/messages/timestamp.hpp"
#include "cucumber/messages/undefined_parameter_type.hpp"
#include "cucumber/query/Lineage.hpp"
#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <map>
#include <optional>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace cucumber::query
{

    class Query
    {
    public:
        void Update(const cucumber::messages::envelope& envelope);

        std::size_t CountTestCasesStarted() const;
        std::map<cucumber::messages::test_step_result_status, int> CountMostSevereTestStepResultStatus() const;

        std::vector<cucumber::messages::pickle> FindAllPickles() const;
        std::vector<cucumber::messages::pickle_step> FindAllPickleSteps() const;
        std::vector<cucumber::messages::step_definition> FindAllStepDefinitions() const;
        std::vector<cucumber::messages::test_case_started> FindAllTestCaseStarted() const;
        std::vector<cucumber::messages::test_case_finished> FindAllTestCaseFinished() const;
        std::vector<cucumber::messages::test_step> FindAllTestSteps() const;
        std::vector<cucumber::messages::test_step_started> FindAllTestStepStarted() const;
        std::vector<cucumber::messages::test_step_finished> FindAllTestStepFinished() const;
        std::vector<cucumber::messages::test_run_hook_started> FindAllTestRunHookStarted() const;
        std::vector<cucumber::messages::test_run_hook_finished> FindAllTestRunHookFinished() const;
        std::vector<cucumber::messages::undefined_parameter_type> FindAllUndefinedParameterTypes() const;

        template<class T>
        std::vector<cucumber::messages::test_case_started> FindAllTestCaseStartedOrderBy(std::function<std::optional<T>(const Query&, const cucumber::messages::test_case_started&)> findOrderBy, std::function<int(const T&, const T&)> order) const;

        template<class T>
        std::vector<cucumber::messages::test_case_finished> FindAllTestCaseFinishedOrderBy(std::function<std::optional<T>(const Query&, const cucumber::messages::test_case_finished&)> findOrderBy, std::function<int(const T&, const T&)> order) const;

        std::optional<Lineage> FindLineageBy(const cucumber::messages::pickle& pickle) const;
        std::optional<Lineage> FindLineageBy(const cucumber::messages::test_case_started& testCaseStarted) const;
        std::optional<Lineage> FindLineageBy(const cucumber::messages::test_case_finished& testCaseFinished) const;

        std::optional<cucumber::messages::meta> FindMeta() const;

        std::optional<cucumber::messages::location> FindLocationOf(const cucumber::messages::pickle& pickle) const;

        std::optional<cucumber::messages::pickle> FindPickleBy(const cucumber::messages::test_case_started& testCaseStarted) const;
        std::optional<cucumber::messages::pickle> FindPickleBy(const cucumber::messages::test_case_finished& testCaseFinished) const;
        std::optional<cucumber::messages::pickle> FindPickleBy(const cucumber::messages::test_step_started& testStepStarted) const;
        std::optional<cucumber::messages::pickle> FindPickleBy(const cucumber::messages::test_step_finished& testStepFinished) const;

        std::optional<cucumber::messages::pickle_step> FindPickleStepBy(const cucumber::messages::test_step& testStep) const;

        std::optional<cucumber::messages::step> FindStepBy(const cucumber::messages::pickle_step& pickleStep) const;

        std::vector<cucumber::messages::step_definition> FindStepDefinitionsBy(const cucumber::messages::test_step& testStep) const;

        std::optional<cucumber::messages::step_definition> FindUnambiguousStepDefinitionBy(const cucumber::messages::test_step& testStep) const;

        std::vector<cucumber::messages::suggestion> FindSuggestionsBy(const cucumber::messages::pickle_step& pickleStep) const;
        std::vector<cucumber::messages::suggestion> FindSuggestionsBy(const cucumber::messages::pickle& pickle) const;

        std::optional<cucumber::messages::hook> FindHookBy(const cucumber::messages::test_step& testStep) const;
        std::optional<cucumber::messages::hook> FindHookBy(const cucumber::messages::test_run_hook_started& testRunHookStarted) const;
        std::optional<cucumber::messages::hook> FindHookBy(const cucumber::messages::test_run_hook_finished& testRunHookFinished) const;

        std::optional<cucumber::messages::test_case> FindTestCaseBy(const cucumber::messages::test_case_started& testCaseStarted) const;
        std::optional<cucumber::messages::test_case> FindTestCaseBy(const cucumber::messages::test_case_finished& testCaseFinished) const;
        std::optional<cucumber::messages::test_case> FindTestCaseBy(const cucumber::messages::test_step_started& testStepStarted) const;
        std::optional<cucumber::messages::test_case> FindTestCaseBy(const cucumber::messages::test_step_finished& testStepFinished) const;

        std::optional<cucumber::messages::test_case_started> FindTestCaseStartedBy(const cucumber::messages::test_case_finished& testCaseFinished) const;
        std::optional<cucumber::messages::test_case_started> FindTestCaseStartedBy(const cucumber::messages::test_step_started& testStepStarted) const;
        std::optional<cucumber::messages::test_case_started> FindTestCaseStartedBy(const cucumber::messages::test_step_finished& testStepFinished) const;

        std::optional<cucumber::messages::test_case_finished> FindTestCaseFinishedBy(const cucumber::messages::test_case_started& testCaseStarted) const;

        std::optional<cucumber::messages::duration> FindTestCaseDurationBy(const cucumber::messages::test_case_started& testCaseStarted) const;
        std::optional<cucumber::messages::duration> FindTestCaseDurationBy(const cucumber::messages::test_case_finished& testCaseFinished) const;

        std::optional<cucumber::messages::test_step_result> FindMostSevereTestStepResultBy(const cucumber::messages::test_case_started& testCaseStarted) const;
        std::optional<cucumber::messages::test_step_result> FindMostSevereTestStepResultBy(const cucumber::messages::test_case_finished& testCaseFinished) const;

        std::optional<cucumber::messages::test_run_hook_started> FindTestRunHookStartedBy(const cucumber::messages::test_run_hook_finished& testRunHookFinished) const;
        std::optional<cucumber::messages::test_run_hook_finished> FindTestRunHookFinishedBy(const cucumber::messages::test_run_hook_started& testRunHookStarted) const;

        std::optional<cucumber::messages::test_run_started> FindTestRunStarted() const;
        std::optional<cucumber::messages::test_run_finished> FindTestRunFinished() const;
        std::optional<cucumber::messages::duration> FindTestRunDuration() const;

        std::optional<cucumber::messages::test_step> FindTestStepBy(const cucumber::messages::test_step_started& testStepStarted) const;
        std::optional<cucumber::messages::test_step> FindTestStepBy(const cucumber::messages::test_step_finished& testStepFinished) const;

        std::vector<cucumber::messages::test_step_started> FindTestStepsStartedBy(const cucumber::messages::test_case_started& testCaseStarted) const;
        std::vector<cucumber::messages::test_step_started> FindTestStepsStartedBy(const cucumber::messages::test_case_finished& testCaseFinished) const;

        std::vector<cucumber::messages::test_step_finished> FindTestStepsFinishedBy(const cucumber::messages::test_case_started& testCaseStarted) const;
        std::vector<cucumber::messages::test_step_finished> FindTestStepsFinishedBy(const cucumber::messages::test_case_finished& testCaseFinished) const;

        std::vector<std::pair<cucumber::messages::test_step_finished, cucumber::messages::test_step>> FindTestStepFinishedAndTestStepBy(const cucumber::messages::test_case_started& testCaseStarted) const;

        std::vector<cucumber::messages::attachment> FindAttachmentsBy(const cucumber::messages::test_step_finished& testStepFinished) const;
        std::vector<cucumber::messages::attachment> FindAttachmentsBy(const cucumber::messages::test_run_hook_finished& testRunHookFinished) const;

    private:
        void UpdateGherkinDocument(const cucumber::messages::gherkin_document& document);
        void UpdateFeature(const cucumber::messages::feature& feature, Lineage lineage);
        void UpdateRule(const cucumber::messages::rule& rule, Lineage lineage);
        void UpdateScenario(const cucumber::messages::scenario& scenario, Lineage lineage);
        void UpdateSteps(const std::vector<cucumber::messages::step>& steps);
        void UpdatePickle(const cucumber::messages::pickle& pickle);
        void UpdateTestCase(const cucumber::messages::test_case& testCase);

        static std::int64_t TimestampToMilliseconds(const cucumber::messages::timestamp& timestamp) noexcept;
        static int StatusOrdinal(cucumber::messages::test_step_result_status status) noexcept;

        std::optional<cucumber::messages::meta> meta;
        std::optional<cucumber::messages::test_run_started> testRunStarted;
        std::optional<cucumber::messages::test_run_finished> testRunFinished;

        std::unordered_map<std::string, cucumber::messages::test_case_started, std::hash<std::string>, std::equal_to<>> testCaseStartedById;
        std::unordered_map<std::string, Lineage, std::hash<std::string>, std::equal_to<>> lineageById;
        std::unordered_map<std::string, cucumber::messages::step, std::hash<std::string>, std::equal_to<>> stepById;
        std::unordered_map<std::string, cucumber::messages::pickle, std::hash<std::string>, std::equal_to<>> pickleById;
        std::unordered_map<std::string, cucumber::messages::pickle_step, std::hash<std::string>, std::equal_to<>> pickleStepById;
        std::unordered_map<std::string, cucumber::messages::hook, std::hash<std::string>, std::equal_to<>> hookById;
        std::unordered_map<std::string, cucumber::messages::step_definition, std::hash<std::string>, std::equal_to<>> stepDefinitionById;
        std::unordered_map<std::string, cucumber::messages::test_case, std::hash<std::string>, std::equal_to<>> testCaseById;
        std::unordered_map<std::string, cucumber::messages::test_step, std::hash<std::string>, std::equal_to<>> testStepById;
        std::unordered_map<std::string, cucumber::messages::test_case_finished, std::hash<std::string>, std::equal_to<>> testCaseFinishedByTestCaseStartedId;
        std::unordered_map<std::string, cucumber::messages::test_run_hook_started, std::hash<std::string>, std::equal_to<>> testRunHookStartedById;
        std::unordered_map<std::string, cucumber::messages::test_run_hook_finished, std::hash<std::string>, std::equal_to<>> testRunHookFinishedByTestRunHookStartedId;

        std::unordered_map<std::string, std::vector<cucumber::messages::test_step_started>, std::hash<std::string>, std::equal_to<>> testStepStartedByTestCaseStartedId;
        std::unordered_map<std::string, std::vector<cucumber::messages::test_step_finished>, std::hash<std::string>, std::equal_to<>> testStepFinishedByTestCaseStartedId;
        std::unordered_map<std::string, std::vector<cucumber::messages::attachment>, std::hash<std::string>, std::equal_to<>> attachmentsByTestCaseStartedId;
        std::unordered_map<std::string, std::vector<cucumber::messages::attachment>, std::hash<std::string>, std::equal_to<>> attachmentsByTestRunHookStartedId;
        std::unordered_map<std::string, std::vector<cucumber::messages::suggestion>, std::hash<std::string>, std::equal_to<>> suggestionsByPickleStepId;

        std::vector<cucumber::messages::undefined_parameter_type> undefinedParameterTypes;

        template<class U, class T>
        static std::vector<U> SortBy(std::vector<U> all, const Query& query, std::function<std::optional<T>(const Query&, const U&)> findOrderBy, std::function<int(const T&, const T&)> order);
    };

    template<class U, class T>
    std::vector<U> Query::SortBy(std::vector<U> all, const Query& query, std::function<std::optional<T>(const Query&, const U&)> findOrderBy, std::function<int(const T&, const T&)> order)
    {
        struct Item
        {
            U value;
            std::optional<T> orderBy;
        };

        std::vector<Item> items;
        items.reserve(all.size());

        for (const auto& element : all)
            items.push_back({ element, findOrderBy(query, element) });

        std::ranges::sort(items, [&](const Item& a, const Item& b)
            {
                if (!a.orderBy && !b.orderBy)
                    return false;

                if (!a.orderBy)
                    return false;

                if (!b.orderBy)
                    return true;

                return order(*a.orderBy, *b.orderBy) < 0;
            });

        std::vector<U> result;
        result.reserve(items.size());

        for (auto& item : items)
            result.push_back(std::move(item.value));

        return result;
    }

    template<class T>
    std::vector<cucumber::messages::test_case_started> Query::FindAllTestCaseStartedOrderBy(std::function<std::optional<T>(const Query&, const cucumber::messages::test_case_started&)> findOrderBy, std::function<int(const T&, const T&)> order) const
    {
        return SortBy<cucumber::messages::test_case_started, T>(FindAllTestCaseStarted(), *this, findOrderBy, order);
    }

    template<class T>
    std::vector<cucumber::messages::test_case_finished> Query::FindAllTestCaseFinishedOrderBy(std::function<std::optional<T>(const Query&, const cucumber::messages::test_case_finished&)> findOrderBy, std::function<int(const T&, const T&)> order) const
    {
        return SortBy<cucumber::messages::test_case_finished, T>(FindAllTestCaseFinished(), *this, findOrderBy, order);
    }

} // namespace cucumber::query

#endif
