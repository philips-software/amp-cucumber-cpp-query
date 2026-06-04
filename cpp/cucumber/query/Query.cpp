#include "cucumber/query/Query.hpp"
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
#include <map>
#include <optional>
#include <ranges>
#include <stdexcept>
#include <utility>
#include <vector>

namespace cucumber::query
{

    std::int64_t Query::TimestampToMilliseconds(const cucumber::messages::timestamp& timestamp) noexcept
    {
        return static_cast<std::int64_t>(timestamp.seconds) * 1'000 + static_cast<std::int64_t>(timestamp.nanos) / 1'000'000;
    }

    int Query::StatusOrdinal(cucumber::messages::test_step_result_status status) noexcept
    {
        using Status = cucumber::messages::test_step_result_status;
        switch (status)
        {
            case Status::UNKNOWN:
                return 0;
            case Status::PASSED:
                return 1;
            case Status::SKIPPED:
                return 2;
            case Status::PENDING:
                return 3;
            case Status::UNDEFINED:
                return 4;
            case Status::AMBIGUOUS:
                return 5;
            case Status::FAILED:
                return 6;
            default:
                return 0;
        }
    }

    // ---------------------------------------------------------------------------
    // Update (ingest)
    // ---------------------------------------------------------------------------

    void Query::Update(const cucumber::messages::envelope& envelope)
    {
        if (envelope.meta)
            meta = envelope.meta;

        if (envelope.gherkin_document)
            UpdateGherkinDocument(*envelope.gherkin_document);

        if (envelope.pickle)
            UpdatePickle(*envelope.pickle);

        if (envelope.hook)
            hookById[envelope.hook->id] = *envelope.hook;

        if (envelope.step_definition)
            stepDefinitionById[envelope.step_definition->id] = *envelope.step_definition;

        if (envelope.test_run_started)
            testRunStarted = envelope.test_run_started;

        if (envelope.test_run_hook_started)
            testRunHookStartedById[envelope.test_run_hook_started->id] =
                *envelope.test_run_hook_started;

        if (envelope.test_run_hook_finished)
            testRunHookFinishedByTestRunHookStartedId
                [envelope.test_run_hook_finished->test_run_hook_started_id] =
                    *envelope.test_run_hook_finished;

        if (envelope.test_case)
            UpdateTestCase(*envelope.test_case);

        if (envelope.test_case_started)
            testCaseStartedById[envelope.test_case_started->id] = *envelope.test_case_started;

        if (envelope.test_step_started)
            testStepStartedByTestCaseStartedId
                [envelope.test_step_started->test_case_started_id]
                    .push_back(*envelope.test_step_started);

        if (envelope.attachment)
        {
            if (envelope.attachment->test_case_started_id)
                attachmentsByTestCaseStartedId
                    [*envelope.attachment->test_case_started_id]
                        .push_back(*envelope.attachment);

            if (envelope.attachment->test_run_hook_started_id)
                attachmentsByTestRunHookStartedId
                    [*envelope.attachment->test_run_hook_started_id]
                        .push_back(*envelope.attachment);
        }

        if (envelope.test_step_finished)
            testStepFinishedByTestCaseStartedId
                [envelope.test_step_finished->test_case_started_id]
                    .push_back(*envelope.test_step_finished);

        if (envelope.test_case_finished)
            testCaseFinishedByTestCaseStartedId
                [envelope.test_case_finished->test_case_started_id] = *envelope.test_case_finished;

        if (envelope.test_run_finished)
            testRunFinished = envelope.test_run_finished;

        if (envelope.suggestion)
            suggestionsByPickleStepId[envelope.suggestion->pickle_step_id]
                .push_back(*envelope.suggestion);

        if (envelope.undefined_parameter_type)
            undefinedParameterTypes.push_back(*envelope.undefined_parameter_type);
    }

    void Query::UpdateGherkinDocument(const cucumber::messages::gherkin_document& document)
    {
        if (document.feature)
        {
            Lineage lineage;
            lineage.gherkinDocument = document;
            UpdateFeature(*document.feature, lineage);
        }
    }

    void Query::UpdateFeature(const cucumber::messages::feature& feature, Lineage lineage)
    {
        for (const auto& child : feature.children)
        {
            if (child.background)
            {
                lineage.background = child.background;
                UpdateSteps(child.background->steps);
            }

            if (child.scenario)
            {
                Lineage scenarioLineage = lineage;
                scenarioLineage.feature = feature;
                UpdateScenario(*child.scenario, scenarioLineage);
            }

            if (child.rule)
            {
                Lineage ruleLineage = lineage;
                ruleLineage.feature = feature;
                UpdateRule(*child.rule, ruleLineage);
            }
        }
    }

    void Query::UpdateRule(const cucumber::messages::rule& rule, Lineage lineage)
    {
        for (const auto& child : rule.children)
        {
            if (child.background)
            {
                lineage.ruleBackground = child.background;
                UpdateSteps(child.background->steps);
            }

            if (child.scenario)
            {
                Lineage scenarioLineage = lineage;
                scenarioLineage.rule = rule;
                UpdateScenario(*child.scenario, scenarioLineage);
            }
        }
    }

    void Query::UpdateScenario(const cucumber::messages::scenario& scenario, Lineage lineage)
    {
        {
            Lineage scenarioLineage = lineage;
            scenarioLineage.scenario = scenario;
            lineageById[scenario.id] = scenarioLineage;
        }

        for (std::size_t i = 0; i < scenario.examples.size(); ++i)
        {
            const auto& examples = scenario.examples[i];

            Lineage examplesLineage = lineage;
            examplesLineage.scenario = scenario;
            examplesLineage.examples = examples;
            examplesLineage.examplesIndex = i;
            lineageById[examples.id] = examplesLineage;

            for (std::size_t j = 0; j < examples.table_body.size(); ++j)
            {
                const auto& row = examples.table_body[j];

                Lineage rowLineage = examplesLineage;
                rowLineage.example = row;
                rowLineage.exampleIndex = j;
                lineageById[row.id] = rowLineage;
            }
        }

        UpdateSteps(scenario.steps);
    }

    void Query::UpdateSteps(const std::vector<cucumber::messages::step>& steps)
    {
        for (const auto& step : steps)
            stepById[step.id] = step;
    }

    void Query::UpdatePickle(const cucumber::messages::pickle& pickle)
    {
        pickleById[pickle.id] = pickle;

        for (const auto& pickleStep : pickle.steps)
            pickleStepById[pickleStep.id] = pickleStep;
    }

    void Query::UpdateTestCase(const cucumber::messages::test_case& testCase)
    {
        testCaseById[testCase.id] = testCase;

        for (const auto& testStep : testCase.test_steps)
            testStepById[testStep.id] = testStep;
    }

    std::size_t Query::CountTestCasesStarted() const
    {
        return FindAllTestCaseStarted().size();
    }

    std::map<cucumber::messages::test_step_result_status, int> Query::CountMostSevereTestStepResultStatus() const
    {
        using Status = cucumber::messages::test_step_result_status;
        std::map<Status, int> result = {
            { Status::AMBIGUOUS, 0 },
            { Status::FAILED, 0 },
            { Status::PASSED, 0 },
            { Status::PENDING, 0 },
            { Status::SKIPPED, 0 },
            { Status::UNDEFINED, 0 },
            { Status::UNKNOWN, 0 },
        };

        for (const auto& testCaseStarted : FindAllTestCaseStarted())
        {
            const auto mostSevere = FindMostSevereTestStepResultBy(testCaseStarted);

            if (mostSevere)
                ++result[mostSevere->status];
        }

        return result;
    }

    std::vector<cucumber::messages::pickle> Query::FindAllPickles() const
    {
        const auto values = pickleById | std::views::values;
        return { values.begin(), values.end() };
    }

    std::vector<cucumber::messages::pickle_step> Query::FindAllPickleSteps() const
    {
        const auto values = pickleStepById | std::views::values;
        return { values.begin(), values.end() };
    }

    std::vector<cucumber::messages::step_definition> Query::FindAllStepDefinitions() const
    {
        const auto values = stepDefinitionById | std::views::values;
        return { values.begin(), values.end() };
    }

    std::vector<cucumber::messages::test_case_started> Query::FindAllTestCaseStarted() const
    {
        auto filtered = testCaseStartedById | std::views::values | std::views::filter([&](const cucumber::messages::test_case_started& testCaseStarted)
                                                                       {
                                                                           const auto it = testCaseFinishedByTestCaseStartedId.find(testCaseStarted.id);

                                                                           if (it == testCaseFinishedByTestCaseStartedId.end())
                                                                               return true;

                                                                           return !it->second.will_be_retried;
                                                                       });

        std::vector<cucumber::messages::test_case_started> result(filtered.begin(), filtered.end());

        std::ranges::sort(result, [](const auto& a, const auto& b)
            {
                const auto millisecondsA = TimestampToMilliseconds(a.timestamp);
                const auto millisecondsB = TimestampToMilliseconds(b.timestamp);

                if (millisecondsA != millisecondsB)
                    return millisecondsA < millisecondsB;

                return a.id < b.id;
            });

        return result;
    }

    std::vector<cucumber::messages::test_case_finished> Query::FindAllTestCaseFinished() const
    {
        auto filtered = testCaseFinishedByTestCaseStartedId | std::views::values | std::views::filter([](const cucumber::messages::test_case_finished& testCaseFinished)
                                                                                       {
                                                                                           return !testCaseFinished.will_be_retried;
                                                                                       });

        std::vector<cucumber::messages::test_case_finished> result(filtered.begin(), filtered.end());

        std::ranges::sort(result, [](const auto& a, const auto& b)
            {
                const auto millisecondsA = TimestampToMilliseconds(a.timestamp);
                const auto millisecondsB = TimestampToMilliseconds(b.timestamp);

                if (millisecondsA != millisecondsB)
                    return millisecondsA < millisecondsB;

                return a.test_case_started_id < b.test_case_started_id;
            });

        return result;
    }

    std::vector<cucumber::messages::test_step> Query::FindAllTestSteps() const
    {
        const auto values = testStepById | std::views::values;
        return { values.begin(), values.end() };
    }

    std::vector<cucumber::messages::test_step_started> Query::FindAllTestStepStarted() const
    {
        std::vector<cucumber::messages::test_step_started> result;

        for (const auto& [_, vec] : testStepStartedByTestCaseStartedId)
            result.insert(result.end(), vec.begin(), vec.end());

        return result;
    }

    std::vector<cucumber::messages::test_step_finished> Query::FindAllTestStepFinished() const
    {
        std::vector<cucumber::messages::test_step_finished> result;

        for (const auto& [_, vec] : testStepFinishedByTestCaseStartedId)
            result.insert(result.end(), vec.begin(), vec.end());

        return result;
    }

    std::vector<cucumber::messages::test_run_hook_started> Query::FindAllTestRunHookStarted() const
    {
        const auto values = testRunHookStartedById | std::views::values;
        return { values.begin(), values.end() };
    }

    std::vector<cucumber::messages::test_run_hook_finished> Query::FindAllTestRunHookFinished() const
    {
        const auto values = testRunHookFinishedByTestRunHookStartedId | std::views::values;
        return { values.begin(), values.end() };
    }

    std::vector<cucumber::messages::undefined_parameter_type> Query::FindAllUndefinedParameterTypes() const
    {
        return undefinedParameterTypes;
    }

    std::optional<Lineage> Query::FindLineageBy(const cucumber::messages::pickle& pickle) const
    {
        if (pickle.ast_node_ids.empty())
            return std::nullopt;

        const auto& deepest = pickle.ast_node_ids.back();
        const auto it = lineageById.find(deepest);

        if (it == lineageById.end())
            return std::nullopt;

        return it->second;
    }

    std::optional<Lineage> Query::FindLineageBy(const cucumber::messages::test_case_started& testCaseStarted) const
    {
        const auto pickle = FindPickleBy(testCaseStarted);

        if (!pickle)
            return std::nullopt;

        return FindLineageBy(*pickle);
    }

    std::optional<Lineage> Query::FindLineageBy(const cucumber::messages::test_case_finished& testCaseFinished) const
    {
        const auto pickle = FindPickleBy(testCaseFinished);

        if (!pickle)
            return std::nullopt;

        return FindLineageBy(*pickle);
    }

    std::optional<cucumber::messages::meta> Query::FindMeta() const
    {
        return meta;
    }

    std::optional<cucumber::messages::test_run_started> Query::FindTestRunStarted() const
    {
        return testRunStarted;
    }

    std::optional<cucumber::messages::test_run_finished> Query::FindTestRunFinished() const
    {
        return testRunFinished;
    }

    std::optional<cucumber::messages::duration> Query::FindTestRunDuration() const
    {
        if (!testRunStarted || !testRunFinished)
            return std::nullopt;

        const auto milliseconds = TimestampToMilliseconds(testRunFinished->timestamp) - TimestampToMilliseconds(testRunStarted->timestamp);
        cucumber::messages::duration duration{};
        duration.seconds = static_cast<std::size_t>(milliseconds / 1'000);
        duration.nanos = static_cast<std::size_t>((milliseconds % 1'000) * 1'000'000);
        return duration;
    }

    std::optional<cucumber::messages::location> Query::FindLocationOf(const cucumber::messages::pickle& pickle) const
    {
        const auto lineage = FindLineageBy(pickle);

        if (!lineage)
            return std::nullopt;

        if (lineage->example)
            return lineage->example->location;

        if (lineage->scenario)
            return lineage->scenario->location;

        return std::nullopt;
    }

    std::optional<cucumber::messages::pickle> Query::FindPickleBy(const cucumber::messages::test_case_started& testCaseStarted) const
    {
        const auto testCase = FindTestCaseBy(testCaseStarted);

        if (!testCase)
            return std::nullopt;

        const auto it = pickleById.find(testCase->pickle_id);

        if (it == pickleById.end())
            return std::nullopt;

        return it->second;
    }

    std::optional<cucumber::messages::pickle> Query::FindPickleBy(const cucumber::messages::test_case_finished& testCaseFinished) const
    {
        const auto testCaseStarted = FindTestCaseStartedBy(testCaseFinished);

        if (!testCaseStarted)
            return std::nullopt;

        return FindPickleBy(*testCaseStarted);
    }

    std::optional<cucumber::messages::pickle> Query::FindPickleBy(const cucumber::messages::test_step_started& testStepStarted) const
    {
        const auto it = testCaseStartedById.find(testStepStarted.test_case_started_id);

        if (it == testCaseStartedById.end())
            return std::nullopt;

        return FindPickleBy(it->second);
    }

    std::optional<cucumber::messages::pickle> Query::FindPickleBy(const cucumber::messages::test_step_finished& testStepFinished) const
    {
        const auto it = testCaseStartedById.find(testStepFinished.test_case_started_id);

        if (it == testCaseStartedById.end())
            return std::nullopt;

        return FindPickleBy(it->second);
    }

    std::optional<cucumber::messages::pickle_step> Query::FindPickleStepBy(const cucumber::messages::test_step& testStep) const
    {
        if (!testStep.pickle_step_id)
            return std::nullopt;

        const auto it = pickleStepById.find(*testStep.pickle_step_id);

        if (it == pickleStepById.end())
            return std::nullopt;

        return it->second;
    }

    std::optional<cucumber::messages::step> Query::FindStepBy(const cucumber::messages::pickle_step& pickleStep) const
    {
        if (pickleStep.ast_node_ids.empty())
            return std::nullopt;

        const auto it = stepById.find(pickleStep.ast_node_ids.front());

        if (it == stepById.end())
            return std::nullopt;

        return it->second;
    }

    std::vector<cucumber::messages::step_definition> Query::FindStepDefinitionsBy(const cucumber::messages::test_step& testStep) const
    {
        if (!testStep.step_definition_ids)
            return {};

        std::vector<cucumber::messages::step_definition> result;

        for (const auto& id : *testStep.step_definition_ids)
        {
            const auto it = stepDefinitionById.find(id);

            if (it != stepDefinitionById.end())
                result.push_back(it->second);
        }

        return result;
    }

    std::optional<cucumber::messages::step_definition> Query::FindUnambiguousStepDefinitionBy(const cucumber::messages::test_step& testStep) const
    {
        if (!testStep.step_definition_ids || testStep.step_definition_ids->size() != 1)
            return std::nullopt;

        const auto it = stepDefinitionById.find(testStep.step_definition_ids->front());

        if (it == stepDefinitionById.end())
            return std::nullopt;

        return it->second;
    }

    std::vector<cucumber::messages::suggestion> Query::FindSuggestionsBy(const cucumber::messages::pickle_step& pickleStep) const
    {
        const auto it = suggestionsByPickleStepId.find(pickleStep.id);

        if (it == suggestionsByPickleStepId.end())
            return {};

        return it->second;
    }

    std::vector<cucumber::messages::suggestion> Query::FindSuggestionsBy(const cucumber::messages::pickle& pickle) const
    {
        std::vector<cucumber::messages::suggestion> result;

        for (const auto& step : pickle.steps)
        {
            const auto suggestions = FindSuggestionsBy(step);
            result.insert(result.end(), suggestions.begin(), suggestions.end());
        }

        return result;
    }

    std::optional<cucumber::messages::hook> Query::FindHookBy(const cucumber::messages::test_step& testStep) const
    {
        if (!testStep.hook_id)
            return std::nullopt;

        const auto it = hookById.find(*testStep.hook_id);

        if (it == hookById.end())
            return std::nullopt;

        return it->second;
    }

    std::optional<cucumber::messages::hook> Query::FindHookBy(const cucumber::messages::test_run_hook_started& testRunHookStarted) const
    {
        const auto it = hookById.find(testRunHookStarted.hook_id);

        if (it == hookById.end())
            return std::nullopt;

        return it->second;
    }

    std::optional<cucumber::messages::hook> Query::FindHookBy(const cucumber::messages::test_run_hook_finished& testRunHookFinished) const
    {
        const auto testRunHookStarted = FindTestRunHookStartedBy(testRunHookFinished);

        if (!testRunHookStarted)
            return std::nullopt;

        return FindHookBy(*testRunHookStarted);
    }

    std::optional<cucumber::messages::test_case> Query::FindTestCaseBy(const cucumber::messages::test_case_started& testCaseStarted) const
    {
        const auto it = testCaseById.find(testCaseStarted.test_case_id);

        if (it == testCaseById.end())
            return std::nullopt;

        return it->second;
    }

    std::optional<cucumber::messages::test_case> Query::FindTestCaseBy(const cucumber::messages::test_case_finished& testCaseFinished) const
    {
        const auto testCaseStarted = FindTestCaseStartedBy(testCaseFinished);

        if (!testCaseStarted)
            return std::nullopt;

        return FindTestCaseBy(*testCaseStarted);
    }

    std::optional<cucumber::messages::test_case> Query::FindTestCaseBy(const cucumber::messages::test_step_started& testStepStarted) const
    {
        const auto testCaseStarted = FindTestCaseStartedBy(testStepStarted);

        if (!testCaseStarted)
            return std::nullopt;

        return FindTestCaseBy(*testCaseStarted);
    }

    std::optional<cucumber::messages::test_case> Query::FindTestCaseBy(const cucumber::messages::test_step_finished& testStepFinished) const
    {
        const auto testCaseStarted = FindTestCaseStartedBy(testStepFinished);

        if (!testCaseStarted)
            return std::nullopt;

        return FindTestCaseBy(*testCaseStarted);
    }

    std::optional<cucumber::messages::test_case_started> Query::FindTestCaseStartedBy(const cucumber::messages::test_case_finished& testCaseFinished) const
    {
        const auto it = testCaseStartedById.find(testCaseFinished.test_case_started_id);

        if (it == testCaseStartedById.end())
            return std::nullopt;

        return it->second;
    }

    std::optional<cucumber::messages::test_case_started> Query::FindTestCaseStartedBy(const cucumber::messages::test_step_started& testStepStarted) const
    {
        const auto it = testCaseStartedById.find(testStepStarted.test_case_started_id);

        if (it == testCaseStartedById.end())
            return std::nullopt;

        return it->second;
    }

    std::optional<cucumber::messages::test_case_started> Query::FindTestCaseStartedBy(const cucumber::messages::test_step_finished& testStepFinished) const
    {
        const auto it = testCaseStartedById.find(testStepFinished.test_case_started_id);

        if (it == testCaseStartedById.end())
            return std::nullopt;

        return it->second;
    }

    std::optional<cucumber::messages::test_case_finished> Query::FindTestCaseFinishedBy(const cucumber::messages::test_case_started& testCaseStarted) const
    {
        const auto it = testCaseFinishedByTestCaseStartedId.find(testCaseStarted.id);

        if (it == testCaseFinishedByTestCaseStartedId.end())
            return std::nullopt;

        return it->second;
    }

    std::optional<cucumber::messages::duration> Query::FindTestCaseDurationBy(const cucumber::messages::test_case_started& testCaseStarted) const
    {
        const auto testCaseFinished = FindTestCaseFinishedBy(testCaseStarted);

        if (!testCaseFinished)
            return std::nullopt;

        const auto milliseconds = TimestampToMilliseconds(testCaseFinished->timestamp) - TimestampToMilliseconds(testCaseStarted.timestamp);
        cucumber::messages::duration duration{};
        duration.seconds = static_cast<std::size_t>(milliseconds / 1'000);
        duration.nanos = static_cast<std::size_t>((milliseconds % 1'000) * 1'000'000);
        return duration;
    }

    std::optional<cucumber::messages::duration> Query::FindTestCaseDurationBy(const cucumber::messages::test_case_finished& testCaseFinished) const
    {
        const auto testCaseStarted = FindTestCaseStartedBy(testCaseFinished);

        if (!testCaseStarted)
            return std::nullopt;

        return FindTestCaseDurationBy(*testCaseStarted);
    }

    std::optional<cucumber::messages::test_step_result> Query::FindMostSevereTestStepResultBy(const cucumber::messages::test_case_started& testCaseStarted) const
    {
        const auto it = testStepFinishedByTestCaseStartedId.find(testCaseStarted.id);

        if (it == testStepFinishedByTestCaseStartedId.end() || it->second.empty())
            return std::nullopt;

        const auto& steps = it->second;
        const auto maxIt = std::ranges::max_element(steps, [](const auto& a, const auto& b)
            {
                return StatusOrdinal(a.test_step_result.status) < StatusOrdinal(b.test_step_result.status);
            });

        if (maxIt == steps.end())
            return std::nullopt;

        return maxIt->test_step_result;
    }

    std::optional<cucumber::messages::test_step_result> Query::FindMostSevereTestStepResultBy(const cucumber::messages::test_case_finished& testCaseFinished) const
    {
        const auto testCaseStarted = FindTestCaseStartedBy(testCaseFinished);

        if (!testCaseStarted)
            return std::nullopt;

        return FindMostSevereTestStepResultBy(*testCaseStarted);
    }

    std::optional<cucumber::messages::test_run_hook_started> Query::FindTestRunHookStartedBy(const cucumber::messages::test_run_hook_finished& testRunHookFinished) const
    {
        const auto it = testRunHookStartedById.find(testRunHookFinished.test_run_hook_started_id);

        if (it == testRunHookStartedById.end())
            return std::nullopt;

        return it->second;
    }

    std::optional<cucumber::messages::test_run_hook_finished> Query::FindTestRunHookFinishedBy(const cucumber::messages::test_run_hook_started& testRunHookStarted) const
    {
        const auto it = testRunHookFinishedByTestRunHookStartedId.find(testRunHookStarted.id);

        if (it == testRunHookFinishedByTestRunHookStartedId.end())
            return std::nullopt;

        return it->second;
    }

    std::optional<cucumber::messages::test_step> Query::FindTestStepBy(const cucumber::messages::test_step_started& testStepStarted) const
    {
        const auto it = testStepById.find(testStepStarted.test_step_id);

        if (it == testStepById.end())
            return std::nullopt;

        return it->second;
    }

    std::optional<cucumber::messages::test_step> Query::FindTestStepBy(const cucumber::messages::test_step_finished& testStepFinished) const
    {
        const auto it = testStepById.find(testStepFinished.test_step_id);

        if (it == testStepById.end())
            return std::nullopt;

        return it->second;
    }

    std::vector<cucumber::messages::test_step_started> Query::FindTestStepsStartedBy(const cucumber::messages::test_case_started& testCaseStarted) const
    {
        const auto it = testStepStartedByTestCaseStartedId.find(testCaseStarted.id);

        if (it == testStepStartedByTestCaseStartedId.end())
            return {};

        return it->second;
    }

    std::vector<cucumber::messages::test_step_started> Query::FindTestStepsStartedBy(const cucumber::messages::test_case_finished& testCaseFinished) const
    {
        const auto testCaseStarted = FindTestCaseStartedBy(testCaseFinished);

        if (!testCaseStarted)
            return {};

        return FindTestStepsStartedBy(*testCaseStarted);
    }

    std::vector<cucumber::messages::test_step_finished> Query::FindTestStepsFinishedBy(const cucumber::messages::test_case_started& testCaseStarted) const
    {
        const auto it = testStepFinishedByTestCaseStartedId.find(testCaseStarted.id);

        if (it == testStepFinishedByTestCaseStartedId.end())
            return {};

        return it->second;
    }

    std::vector<cucumber::messages::test_step_finished> Query::FindTestStepsFinishedBy(const cucumber::messages::test_case_finished& testCaseFinished) const
    {
        const auto testCaseStarted = FindTestCaseStartedBy(testCaseFinished);

        if (!testCaseStarted)
            return {};

        return FindTestStepsFinishedBy(*testCaseStarted);
    }

    std::vector<std::pair<cucumber::messages::test_step_finished, cucumber::messages::test_step>> Query::FindTestStepFinishedAndTestStepBy(const cucumber::messages::test_case_started& testCaseStarted) const
    {
        std::vector<std::pair<cucumber::messages::test_step_finished, cucumber::messages::test_step>>
            result;

        for (const auto& testStepFinished : FindTestStepsFinishedBy(testCaseStarted))
        {
            const auto testStep = FindTestStepBy(testStepFinished);

            if (testStep)
                result.emplace_back(testStepFinished, *testStep);
        }

        return result;
    }

    std::vector<cucumber::messages::attachment> Query::FindAttachmentsBy(const cucumber::messages::test_step_finished& testStepFinished) const
    {
        const auto it = attachmentsByTestCaseStartedId.find(testStepFinished.test_case_started_id);

        if (it == attachmentsByTestCaseStartedId.end())
            return {};

        auto filtered = it->second | std::views::filter([&testStepFinished](const cucumber::messages::attachment& attachment)
                                         {
                                             return attachment.test_step_id && *attachment.test_step_id == testStepFinished.test_step_id;
                                         });
        return { filtered.begin(), filtered.end() };
    }

    std::vector<cucumber::messages::attachment> Query::FindAttachmentsBy(const cucumber::messages::test_run_hook_finished& testRunHookFinished) const
    {
        const auto it = attachmentsByTestRunHookStartedId.find(testRunHookFinished.test_run_hook_started_id);

        if (it == attachmentsByTestRunHookStartedId.end())
            return {};

        return it->second;
    }

} // namespace cucumber::query
