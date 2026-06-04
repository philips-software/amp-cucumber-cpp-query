#include "cucumber/query/Query.hpp"
#include "cucumber/messages/all.hpp"
#include "cucumber/messages/envelope.hpp"
#include "cucumber/messages/location.hpp"
#include "cucumber/messages/pickle.hpp"
#include "cucumber/messages/test_case.hpp"
#include "cucumber/messages/test_case_finished.hpp"
#include "cucumber/messages/test_case_started.hpp"
#include "cucumber/messages/test_step_result_status.hpp"
#include "cucumber/query/Lineage.hpp"
#include "messages_from_json.hpp"
#include "nlohmann/json.hpp"
#include "nlohmann/json_fwd.hpp"
#include <algorithm>
#include <cstdint>
#include <fstream>
#include <functional>
#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace
{

    std::vector<cucumber::messages::envelope> LoadNdjson(const std::string& path)
    {
        std::ifstream file(path);
        EXPECT_THAT(file.is_open(), testing::IsTrue()) << "Cannot open: " << path;
        std::vector<cucumber::messages::envelope> envelopes;
        std::string line;

        while (std::getline(file, line))
        {
            if (line.empty())
                continue;

            const auto j = nlohmann::json::parse(line);
            cucumber::messages::envelope env;
            cucumber::messages::from_json(j, env);
            envelopes.push_back(std::move(env));
        }
        return envelopes;
    }

    cucumber::query::Query QueryFromNdjson(const std::string& path)
    {
        cucumber::query::Query query;

        for (const auto& env : LoadNdjson(path))
            query.Update(env);

        return query;
    }

    std::string TestdataFile(const std::string& name)
    {
        return std::string(TESTDATA_DIR) + "/" + name;
    }

    std::vector<std::string> LoadExpectedIds(const std::string& path)
    {
        std::ifstream file(path);
        EXPECT_THAT(file.is_open(), testing::IsTrue()) << "Cannot open: " << path;
        const auto parsedJson = nlohmann::json::parse(file);
        return parsedJson.get<std::vector<std::string>>();
    }

    int32_t ReversePickleComparator(const cucumber::messages::pickle& lhs, const cucumber::messages::pickle& rhs)
    {
        if (lhs.uri != rhs.uri)
            return static_cast<int32_t>(rhs.uri.compare(lhs.uri));

        const auto lhsLine = lhs.location ? lhs.location->line : 0u;
        const auto rhsLine = rhs.location ? rhs.location->line : 0u;

        if (lhsLine != rhsLine)
            return static_cast<int32_t>(rhsLine) - static_cast<int32_t>(lhsLine);

        const auto lhsColumn = lhs.location.value_or(cucumber::messages::location{}).column.value_or(0u);
        const auto rhsColumn = rhs.location.value_or(cucumber::messages::location{}).column.value_or(0u);
        return static_cast<int32_t>(rhsColumn) - static_cast<int32_t>(lhsColumn);
    }

} // namespace

TEST(TestQuery, FindAllTestCaseStarted_RetainsTimestampOrder)
{
    using namespace cucumber::messages;

    const test_case_started a{ .attempt = 0, .id = "1", .test_case_id = "1", .timestamp = { .seconds = 1, .nanos = 1 } };
    const test_case_started b{ .attempt = 0, .id = "2", .test_case_id = "2", .timestamp = { .seconds = 2, .nanos = 1 } };
    const test_case_started c{ .attempt = 0, .id = "3", .test_case_id = "3", .timestamp = { .seconds = 2, .nanos = 3 } };

    cucumber::query::Query query;
    query.Update(envelope{ .test_case_started = c });
    query.Update(envelope{ .test_case_started = b });
    query.Update(envelope{ .test_case_started = a });

    const auto result = query.FindAllTestCaseStarted();
    ASSERT_THAT(result, testing::SizeIs(3u));
    EXPECT_THAT(result[0].id, testing::Eq("1"));
    EXPECT_THAT(result[1].id, testing::Eq("2"));
    EXPECT_THAT(result[2].id, testing::Eq("3"));
}

TEST(TestQuery, FindAllTestCaseStarted_UsesIdAsTieBreaker)
{
    using namespace cucumber::messages;

    const test_case_started a{ .attempt = 0, .id = "1", .test_case_id = "1", .timestamp = { .seconds = 1, .nanos = 1 } };
    const test_case_started b{ .attempt = 0, .id = "2", .test_case_id = "2", .timestamp = { .seconds = 1, .nanos = 1 } };

    cucumber::query::Query query;
    query.Update(envelope{ .test_case_started = b });
    query.Update(envelope{ .test_case_started = a });

    const auto result = query.FindAllTestCaseStarted();
    ASSERT_THAT(result, testing::SizeIs(2u));
    EXPECT_THAT(result[0].id, testing::Eq("1"));
    EXPECT_THAT(result[1].id, testing::Eq("2"));
}

TEST(TestQuery, FindAllTestCaseStarted_ExcludesRetriedTestCases)
{
    using namespace cucumber::messages;

    const test_case_started tcs{ .attempt = 0, .id = "1", .test_case_id = "1", .timestamp = { .seconds = 0, .nanos = 0 } };
    const test_case_finished tcf{ .test_case_started_id = "1", .timestamp = { .seconds = 1, .nanos = 0 }, .will_be_retried = true };

    cucumber::query::Query query;
    query.Update(envelope{ .test_case_started = tcs });
    query.Update(envelope{ .test_case_finished = tcf });

    EXPECT_THAT(query.FindAllTestCaseStarted(), testing::IsEmpty());
}

TEST(TestQuery, FindLineageBy_MinimalScenario)
{
    const auto envelopes = LoadNdjson(TestdataFile("minimal.ndjson"));

    cucumber::query::Query query;

    for (const auto& env : envelopes)
        query.Update(env);

    const auto docIt = std::ranges::find_if(envelopes,
        [](const auto& e)
        {
            return e.gherkin_document.has_value();
        });
    ASSERT_THAT(docIt, testing::Ne(envelopes.end()));
    const auto& gherkinDoc = *docIt->gherkin_document;

    const auto pickleIt = std::ranges::find_if(envelopes,
        [](const auto& e)
        {
            return e.pickle.has_value();
        });
    ASSERT_THAT(pickleIt, testing::Ne(envelopes.end()));
    const auto& pickle = *pickleIt->pickle;

    const auto lineage = query.FindLineageBy(pickle);
    ASSERT_THAT(lineage, testing::Optional(testing::_));
    ASSERT_THAT(lineage->gherkinDocument, testing::Optional(testing::_));
    ASSERT_THAT(lineage->feature, testing::Optional(testing::_));
    ASSERT_THAT(lineage->scenario, testing::Optional(testing::_));
    EXPECT_THAT(lineage->background, testing::Eq(std::nullopt));
    EXPECT_THAT(lineage->rule, testing::Eq(std::nullopt));
    EXPECT_THAT(lineage->examples, testing::Eq(std::nullopt));

    EXPECT_THAT(lineage->feature->name, testing::Eq(gherkinDoc.feature->name));
    EXPECT_THAT(lineage->scenario->name, testing::Eq("cukes"));
}

TEST(TestQuery, FindLineageBy_ExamplesTablePickle)
{
    const auto envelopes = LoadNdjson(TestdataFile("examples-tables.ndjson"));

    cucumber::query::Query query;

    for (const auto& env : envelopes)
        query.Update(env);

    const auto pickleIt = std::ranges::find_if(envelopes,
        [](const auto& e)
        {
            return e.pickle.has_value();
        });
    ASSERT_THAT(pickleIt, testing::Ne(envelopes.end()));
    const auto& pickle = *pickleIt->pickle;

    const auto lineage = query.FindLineageBy(pickle);
    ASSERT_THAT(lineage, testing::Optional(testing::_));
    EXPECT_THAT(lineage->scenario, testing::Optional(testing::_));
    EXPECT_THAT(lineage->examples, testing::Optional(testing::_));
    EXPECT_THAT(lineage->example, testing::Optional(testing::_));
    EXPECT_THAT(lineage->examplesIndex, testing::Eq(0u));
    EXPECT_THAT(lineage->exampleIndex, testing::Eq(0u));
}

TEST(TestQuery, FindLineageBy_RulesBackgroundsPickle)
{
    const auto envelopes = LoadNdjson(TestdataFile("rules-backgrounds.ndjson"));

    cucumber::query::Query query;

    for (const auto& env : envelopes)
        query.Update(env);

    const auto pickleIt = std::ranges::find_if(envelopes,
        [](const auto& e)
        {
            return e.pickle.has_value();
        });
    ASSERT_THAT(pickleIt, testing::Ne(envelopes.end()));
    const auto& pickle = *pickleIt->pickle;

    const auto lineage = query.FindLineageBy(pickle);
    ASSERT_THAT(lineage, testing::Optional(testing::_));
    EXPECT_THAT(lineage->feature, testing::Optional(testing::_));
    EXPECT_THAT(lineage->rule, testing::Optional(testing::_));
    EXPECT_THAT(lineage->ruleBackground, testing::Optional(testing::_));
    EXPECT_THAT(lineage->scenario, testing::Optional(testing::_));
}

TEST(TestQuery, CountTestCasesStarted_Minimal)
{
    const auto query = QueryFromNdjson(TestdataFile("minimal.ndjson"));
    EXPECT_THAT(query.CountTestCasesStarted(), testing::Eq(1u));
}

TEST(TestQuery, CountTestCasesStarted_EmptySteps)
{
    const auto query = QueryFromNdjson(TestdataFile("empty.ndjson"));
    EXPECT_THAT(query.CountTestCasesStarted(), testing::Eq(1u));
}

TEST(TestQuery, FindMostSevereTestStepResultBy_MinimalPassed)
{
    const auto query = QueryFromNdjson(TestdataFile("minimal.ndjson"));

    const auto all = query.FindAllTestCaseStarted();
    ASSERT_THAT(all, testing::SizeIs(1u));

    const auto result = query.FindMostSevereTestStepResultBy(all[0]);
    ASSERT_THAT(result, testing::Optional(testing::_));
    EXPECT_THAT(result->status, testing::Eq(cucumber::messages::test_step_result_status::PASSED));
}

TEST(TestQuery, FindMostSevereTestStepResultBy_AllStatusesMostSevereIsFailed)
{
    const auto query = QueryFromNdjson(TestdataFile("all-statuses.ndjson"));

    bool foundFailed = false;

    for (const auto& testCaseStarted : query.FindAllTestCaseStarted())
    {
        const auto result = query.FindMostSevereTestStepResultBy(testCaseStarted);

        if (result && result->status == cucumber::messages::test_step_result_status::FAILED)
        {
            foundFailed = true;
            break;
        }
    }

    EXPECT_THAT(foundFailed, testing::IsTrue());
}

TEST(TestQuery, FindTestCaseDurationBy_Minimal)
{
    const auto query = QueryFromNdjson(TestdataFile("minimal.ndjson"));

    const auto all = query.FindAllTestCaseStarted();
    ASSERT_THAT(all, testing::SizeIs(1u));

    const auto duration = query.FindTestCaseDurationBy(all[0]);
    ASSERT_THAT(duration, testing::Optional(testing::_));
    EXPECT_THAT(duration->seconds, testing::Eq(0u));
    EXPECT_THAT(duration->nanos, testing::Eq(3'000'000u));
}

TEST(TestQuery, FindAttachmentsBy_MinimalHasNone)
{
    const auto query = QueryFromNdjson(TestdataFile("minimal.ndjson"));

    const auto all = query.FindAllTestCaseStarted();
    ASSERT_THAT(all, testing::SizeIs(1u));
    const auto finished = query.FindTestStepsFinishedBy(all[0]);
    ASSERT_THAT(finished, testing::SizeIs(1u));

    const auto attachments = query.FindAttachmentsBy(finished[0]);
    EXPECT_THAT(attachments, testing::IsEmpty());
}

TEST(TestQuery, FindAttachmentsBy_AttachmentsNdjsonHasAttachments)
{
    const auto query = QueryFromNdjson(TestdataFile("attachments.ndjson"));

    bool foundAttachment = false;

    for (const auto& testCaseStarted : query.FindAllTestCaseStarted())
    {
        for (const auto& testStepFinished : query.FindTestStepsFinishedBy(testCaseStarted))
            if (!query.FindAttachmentsBy(testStepFinished).empty())
            {
                foundAttachment = true;
                break;
            }

        if (foundAttachment)
            break;
    }

    EXPECT_THAT(foundAttachment, testing::IsTrue());
}

TEST(TestQuery, CountMostSevereTestStepResultStatus_MinimalHasOnePassed)
{
    const auto query = QueryFromNdjson(TestdataFile("minimal.ndjson"));
    const auto counts = query.CountMostSevereTestStepResultStatus();

    EXPECT_THAT(counts.at(cucumber::messages::test_step_result_status::PASSED), testing::Eq(1));
    EXPECT_THAT(counts.at(cucumber::messages::test_step_result_status::FAILED), testing::Eq(0));
    EXPECT_THAT(counts.at(cucumber::messages::test_step_result_status::SKIPPED), testing::Eq(0));
    EXPECT_THAT(counts.at(cucumber::messages::test_step_result_status::PENDING), testing::Eq(0));
    EXPECT_THAT(counts.at(cucumber::messages::test_step_result_status::UNDEFINED), testing::Eq(0));
    EXPECT_THAT(counts.at(cucumber::messages::test_step_result_status::AMBIGUOUS), testing::Eq(0));
    EXPECT_THAT(counts.at(cucumber::messages::test_step_result_status::UNKNOWN), testing::Eq(0));
}

TEST(TestQuery, FindAllPickles_MinimalHasOnePickle)
{
    const auto query = QueryFromNdjson(TestdataFile("minimal.ndjson"));
    EXPECT_THAT(query.FindAllPickles(), testing::SizeIs(1u));
}

TEST(TestQuery, FindAllStepDefinitions_MinimalHasOneStepDefinition)
{
    const auto query = QueryFromNdjson(TestdataFile("minimal.ndjson"));
    EXPECT_THAT(query.FindAllStepDefinitions(), testing::SizeIs(1u));
}

TEST(TestQuery, FindTestRun_MinimalStartedAndFinished)
{
    const auto query = QueryFromNdjson(TestdataFile("minimal.ndjson"));

    ASSERT_THAT(query.FindTestRunStarted(), testing::Optional(testing::_));
    ASSERT_THAT(query.FindTestRunFinished(), testing::Optional(testing::_));

    const auto duration = query.FindTestRunDuration();
    ASSERT_THAT(duration, testing::Optional(testing::_));
}

TEST(TestQuery, FindPickleBy_ViaTestCaseStarted)
{
    const auto query = QueryFromNdjson(TestdataFile("minimal.ndjson"));

    const auto all = query.FindAllTestCaseStarted();
    ASSERT_THAT(all, testing::SizeIs(1u));

    const auto pickle = query.FindPickleBy(all[0]);
    ASSERT_THAT(pickle, testing::Optional(testing::_));
    EXPECT_THAT(pickle->name, testing::Eq("cukes"));
}

TEST(TestQuery, FindAllTestCaseFinished_MinimalHasOneFinished)
{
    const auto query = QueryFromNdjson(TestdataFile("minimal.ndjson"));
    EXPECT_THAT(query.FindAllTestCaseFinished(), testing::SizeIs(1u));
}

TEST(TestQuery, Retry_RetriedCasesExcludedFinalIncluded)
{
    const auto query = QueryFromNdjson(TestdataFile("retry.ndjson"));
    const auto all = query.FindAllTestCaseStarted();
    EXPECT_THAT(all, testing::Not(testing::IsEmpty()));

    for (const auto& testCaseStarted : all)
    {
        const auto testCaseFinished = query.FindTestCaseFinishedBy(testCaseStarted);

        if (testCaseFinished)
        {
            EXPECT_THAT(testCaseFinished->will_be_retried, testing::IsFalse())
                << "Retried test case should not appear in FindAllTestCaseStarted()";
        }
    }
}

TEST(TestQuery, FindAllTestCases_MinimalHasOneTestCase)
{
    const auto query = QueryFromNdjson(TestdataFile("minimal.ndjson"));

    const auto all = query.FindAllTestCaseStarted();
    ASSERT_THAT(all, testing::SizeIs(1u));

    const auto testCase = query.FindTestCaseBy(all[0]);
    EXPECT_THAT(testCase, testing::Optional(testing::_));
}

TEST(TestQuery, FindAllTestCaseStartedOrderBy_EmptyQuery)
{
    cucumber::query::Query query;

    const auto result = query.FindAllTestCaseStartedOrderBy<cucumber::messages::pickle>(
        [](const cucumber::query::Query& query, const cucumber::messages::test_case_started& testCaseStarted)
        {
            return query.FindPickleBy(testCaseStarted);
        },
        ReversePickleComparator);

    EXPECT_THAT(result, testing::IsEmpty());
}

TEST(TestQuery, FindAllTestCaseStartedOrderBy_ItemsWithoutPickleAreSortedToEnd)
{
    const cucumber::messages::test_case_started testCaseStartedWithPickle{ .attempt = 0, .id = "linked", .test_case_id = "tc1", .timestamp = { .seconds = 1, .nanos = 0 } };
    const cucumber::messages::test_case_started testCaseStartedWithoutPickle{ .attempt = 0, .id = "unlinked", .test_case_id = "tc_unknown", .timestamp = { .seconds = 2, .nanos = 0 } };
    const cucumber::messages::pickle pickle{ .id = "pickle1", .uri = "features/a.feature", .location = cucumber::messages::location{ .line = 3, .column = 1 } };
    const cucumber::messages::test_case testCase{ .id = "tc1", .pickle_id = "pickle1" };

    cucumber::query::Query query;
    query.Update(cucumber::messages::envelope{ .pickle = pickle });
    query.Update(cucumber::messages::envelope{ .test_case = testCase });
    query.Update(cucumber::messages::envelope{ .test_case_started = testCaseStartedWithPickle });
    query.Update(cucumber::messages::envelope{ .test_case_started = testCaseStartedWithoutPickle });

    const auto result = query.FindAllTestCaseStartedOrderBy<cucumber::messages::pickle>(
        [](const cucumber::query::Query& query, const cucumber::messages::test_case_started& testCaseStarted)
        {
            return query.FindPickleBy(testCaseStarted);
        },
        ReversePickleComparator);

    ASSERT_THAT(result, testing::SizeIs(2u));
    EXPECT_THAT(result[0].id, testing::Eq("linked"));
    EXPECT_THAT(result[1].id, testing::Eq("unlinked"));
}

struct FindAllTestCaseStartedOrderByFixture
{
    std::string ndjsonName;
    std::string resultsName;
};

class FindAllTestCaseStartedOrderByAcceptance : public testing::TestWithParam<FindAllTestCaseStartedOrderByFixture>
{};

TEST_P(FindAllTestCaseStartedOrderByAcceptance, MatchesExpectedIds)
{
    using Pickle = cucumber::messages::pickle;
    using TestCaseStarted = cucumber::messages::test_case_started;

    const auto& fixture = GetParam();
    const auto query = QueryFromNdjson(TestdataFile(fixture.ndjsonName));
    const auto expectedIds = LoadExpectedIds(TestdataFile(fixture.resultsName));

    const auto result = query.FindAllTestCaseStartedOrderBy<Pickle>(
        [](const cucumber::query::Query& query, const TestCaseStarted& testCaseStarted)
        {
            return query.FindPickleBy(testCaseStarted);
        },
        ReversePickleComparator);

    std::vector<std::string> actualIds;
    actualIds.reserve(result.size());

    for (const auto& testCaseStarted : result)
        actualIds.push_back(testCaseStarted.id);

    EXPECT_THAT(actualIds, testing::Eq(expectedIds));
}

INSTANTIATE_TEST_SUITE_P(
    TestQuery,
    FindAllTestCaseStartedOrderByAcceptance,
    testing::Values(
        FindAllTestCaseStartedOrderByFixture{ "attachments.ndjson", "attachments.findAllTestCaseStartedOrderBy.results.json" },
        FindAllTestCaseStartedOrderByFixture{ "empty.ndjson", "empty.findAllTestCaseStartedOrderBy.results.json" },
        FindAllTestCaseStartedOrderByFixture{ "global-hooks.ndjson", "global-hooks.findAllTestCaseStartedOrderBy.results.json" },
        FindAllTestCaseStartedOrderByFixture{ "global-hooks-attachments.ndjson", "global-hooks-attachments.findAllTestCaseStartedOrderBy.results.json" },
        FindAllTestCaseStartedOrderByFixture{ "hooks.ndjson", "hooks.findAllTestCaseStartedOrderBy.results.json" },
        FindAllTestCaseStartedOrderByFixture{ "minimal.ndjson", "minimal.findAllTestCaseStartedOrderBy.results.json" },
        FindAllTestCaseStartedOrderByFixture{ "rules.ndjson", "rules.findAllTestCaseStartedOrderBy.results.json" },
        FindAllTestCaseStartedOrderByFixture{ "examples-tables.ndjson", "examples-tables.findAllTestCaseStartedOrderBy.results.json" },
        FindAllTestCaseStartedOrderByFixture{ "unknown-parameter-type.ndjson", "unknown-parameter-type.findAllTestCaseStartedOrderBy.results.json" }));

TEST(TestQuery, FindAllTestCaseFinishedOrderBy_EmptyQuery)
{
    cucumber::query::Query query;

    const auto result = query.FindAllTestCaseFinishedOrderBy<cucumber::messages::pickle>(
        [](const cucumber::query::Query& query, const cucumber::messages::test_case_finished& testCaseFinished)
        {
            return query.FindPickleBy(testCaseFinished);
        },
        ReversePickleComparator);

    EXPECT_THAT(result, testing::IsEmpty());
}

TEST(TestQuery, FindAllTestCaseFinishedOrderBy_ItemsWithoutPickleAreSortedToEnd)
{
    const cucumber::messages::test_case_started testCaseStartedWithPickle{ .attempt = 0, .id = "linked", .test_case_id = "tc1", .timestamp = { .seconds = 1, .nanos = 0 } };
    const cucumber::messages::test_case_finished testCaseFinishedWithPickle{ .test_case_started_id = "linked", .timestamp = { .seconds = 2, .nanos = 0 }, .will_be_retried = false };
    const cucumber::messages::test_case_started testCaseStartedWithoutPickle{ .attempt = 0, .id = "unlinked", .test_case_id = "tc_unknown", .timestamp = { .seconds = 3, .nanos = 0 } };
    const cucumber::messages::test_case_finished testCaseFinishedWithoutPickle{ .test_case_started_id = "unlinked", .timestamp = { .seconds = 4, .nanos = 0 }, .will_be_retried = false };
    const cucumber::messages::pickle pickle{ .id = "pickle1", .uri = "features/a.feature", .location = cucumber::messages::location{ .line = 3, .column = 1 } };
    const cucumber::messages::test_case testCase{ .id = "tc1", .pickle_id = "pickle1" };

    cucumber::query::Query query;
    query.Update(cucumber::messages::envelope{ .pickle = pickle });
    query.Update(cucumber::messages::envelope{ .test_case = testCase });
    query.Update(cucumber::messages::envelope{ .test_case_started = testCaseStartedWithPickle });
    query.Update(cucumber::messages::envelope{ .test_case_finished = testCaseFinishedWithPickle });
    query.Update(cucumber::messages::envelope{ .test_case_started = testCaseStartedWithoutPickle });
    query.Update(cucumber::messages::envelope{ .test_case_finished = testCaseFinishedWithoutPickle });

    const auto result = query.FindAllTestCaseFinishedOrderBy<cucumber::messages::pickle>(
        [](const cucumber::query::Query& query, const cucumber::messages::test_case_finished& testCaseFinished)
        {
            return query.FindPickleBy(testCaseFinished);
        },
        ReversePickleComparator);

    ASSERT_THAT(result, testing::SizeIs(2u));
    EXPECT_THAT(result[0].test_case_started_id, testing::Eq("linked"));
    EXPECT_THAT(result[1].test_case_started_id, testing::Eq("unlinked"));
}

struct FindAllTestCaseFinishedOrderByFixture
{
    std::string ndjsonName;
    std::string resultsName;
};

class FindAllTestCaseFinishedOrderByAcceptance : public testing::TestWithParam<FindAllTestCaseFinishedOrderByFixture>
{};

TEST_P(FindAllTestCaseFinishedOrderByAcceptance, MatchesExpectedIds)
{
    using Pickle = cucumber::messages::pickle;
    using TestCaseFinished = cucumber::messages::test_case_finished;

    const auto& fixture = GetParam();
    const auto query = QueryFromNdjson(TestdataFile(fixture.ndjsonName));
    const auto expectedIds = LoadExpectedIds(TestdataFile(fixture.resultsName));

    const auto result = query.FindAllTestCaseFinishedOrderBy<Pickle>(
        [](const cucumber::query::Query& query, const TestCaseFinished& testCaseFinished)
        {
            return query.FindPickleBy(testCaseFinished);
        },
        ReversePickleComparator);

    std::vector<std::string> actualIds;
    actualIds.reserve(result.size());

    for (const auto& testCaseFinished : result)
        actualIds.push_back(testCaseFinished.test_case_started_id);

    EXPECT_THAT(actualIds, testing::Eq(expectedIds));
}

INSTANTIATE_TEST_SUITE_P(
    TestQuery,
    FindAllTestCaseFinishedOrderByAcceptance,
    testing::Values(
        FindAllTestCaseFinishedOrderByFixture{ "attachments.ndjson", "attachments.findAllTestCaseFinishedOrderBy.results.json" },
        FindAllTestCaseFinishedOrderByFixture{ "empty.ndjson", "empty.findAllTestCaseFinishedOrderBy.results.json" },
        FindAllTestCaseFinishedOrderByFixture{ "global-hooks.ndjson", "global-hooks.findAllTestCaseFinishedOrderBy.results.json" },
        FindAllTestCaseFinishedOrderByFixture{ "global-hooks-attachments.ndjson", "global-hooks-attachments.findAllTestCaseFinishedOrderBy.results.json" },
        FindAllTestCaseFinishedOrderByFixture{ "hooks.ndjson", "hooks.findAllTestCaseFinishedOrderBy.results.json" },
        FindAllTestCaseFinishedOrderByFixture{ "minimal.ndjson", "minimal.findAllTestCaseFinishedOrderBy.results.json" },
        FindAllTestCaseFinishedOrderByFixture{ "rules.ndjson", "rules.findAllTestCaseFinishedOrderBy.results.json" },
        FindAllTestCaseFinishedOrderByFixture{ "examples-tables.ndjson", "examples-tables.findAllTestCaseFinishedOrderBy.results.json" },
        FindAllTestCaseFinishedOrderByFixture{ "unknown-parameter-type.ndjson", "unknown-parameter-type.findAllTestCaseFinishedOrderBy.results.json" }));

TEST(TestQuery, FindAllPickleSteps_MinimalHasOnePickleStep)
{
    const auto query = QueryFromNdjson(TestdataFile("minimal.ndjson"));
    EXPECT_THAT(query.FindAllPickleSteps(), testing::SizeIs(1u));
}

TEST(TestQuery, FindAllTestSteps_MinimalHasOneTestStep)
{
    const auto query = QueryFromNdjson(TestdataFile("minimal.ndjson"));
    EXPECT_THAT(query.FindAllTestSteps(), testing::SizeIs(1u));
}

TEST(TestQuery, FindAllTestStepStarted_MinimalHasOneTestStepStarted)
{
    const auto query = QueryFromNdjson(TestdataFile("minimal.ndjson"));
    EXPECT_THAT(query.FindAllTestStepStarted(), testing::SizeIs(1u));
}

TEST(TestQuery, FindAllTestStepFinished_MinimalHasOneTestStepFinished)
{
    const auto query = QueryFromNdjson(TestdataFile("minimal.ndjson"));
    EXPECT_THAT(query.FindAllTestStepFinished(), testing::SizeIs(1u));
}

TEST(TestQuery, FindAllTestRunHookStarted_GlobalHooksHasHooks)
{
    const auto query = QueryFromNdjson(TestdataFile("global-hooks.ndjson"));
    EXPECT_THAT(query.FindAllTestRunHookStarted(), testing::Not(testing::IsEmpty()));
}

TEST(TestQuery, FindAllTestRunHookFinished_GlobalHooksHasHooks)
{
    const auto query = QueryFromNdjson(TestdataFile("global-hooks.ndjson"));
    EXPECT_THAT(query.FindAllTestRunHookFinished(), testing::Not(testing::IsEmpty()));
}

TEST(TestQuery, FindAllUndefinedParameterTypes_UnknownParameterTypeHasAtLeastOne)
{
    const auto query = QueryFromNdjson(TestdataFile("unknown-parameter-type.ndjson"));
    EXPECT_THAT(query.FindAllUndefinedParameterTypes(), testing::Not(testing::IsEmpty()));
}

TEST(TestQuery, FindMeta_MinimalHasMeta)
{
    const auto query = QueryFromNdjson(TestdataFile("minimal.ndjson"));
    EXPECT_THAT(query.FindMeta(), testing::Optional(testing::_));
}

TEST(TestQuery, FindLocationOf_MinimalPickleHasLocation)
{
    const auto envelopes = LoadNdjson(TestdataFile("minimal.ndjson"));
    cucumber::query::Query query;

    for (const auto& env : envelopes)
        query.Update(env);

    const auto pickleIt = std::ranges::find_if(envelopes,
        [](const auto& e)
        {
            return e.pickle.has_value();
        });
    ASSERT_THAT(pickleIt, testing::Ne(envelopes.end()));

    const auto location = query.FindLocationOf(*pickleIt->pickle);
    EXPECT_THAT(location, testing::Optional(testing::_));
}

TEST(TestQuery, FindTestCaseDurationBy_MinimalViaTestCaseFinished)
{
    const auto query = QueryFromNdjson(TestdataFile("minimal.ndjson"));

    const auto all = query.FindAllTestCaseStarted();
    ASSERT_THAT(all, testing::SizeIs(1u));

    const auto finished = query.FindTestCaseFinishedBy(all[0]);
    ASSERT_THAT(finished, testing::Optional(testing::_));

    const auto duration = query.FindTestCaseDurationBy(*finished);
    ASSERT_THAT(duration, testing::Optional(testing::_));
    EXPECT_THAT(duration->seconds, testing::Eq(0u));
    EXPECT_THAT(duration->nanos, testing::Eq(3'000'000u));
}

TEST(TestQuery, FindMostSevereTestStepResultBy_MinimalPassedViaTestCaseFinished)
{
    const auto query = QueryFromNdjson(TestdataFile("minimal.ndjson"));

    const auto all = query.FindAllTestCaseStarted();
    ASSERT_THAT(all, testing::SizeIs(1u));

    const auto finished = query.FindTestCaseFinishedBy(all[0]);
    ASSERT_THAT(finished, testing::Optional(testing::_));

    const auto result = query.FindMostSevereTestStepResultBy(*finished);
    ASSERT_THAT(result, testing::Optional(testing::_));
    EXPECT_THAT(result->status, testing::Eq(cucumber::messages::test_step_result_status::PASSED));
}

TEST(TestQuery, FindTestStepsStartedBy_MinimalViaTestCaseFinished)
{
    const auto query = QueryFromNdjson(TestdataFile("minimal.ndjson"));

    const auto all = query.FindAllTestCaseStarted();
    ASSERT_THAT(all, testing::SizeIs(1u));

    const auto finished = query.FindTestCaseFinishedBy(all[0]);
    ASSERT_THAT(finished, testing::Optional(testing::_));

    EXPECT_THAT(query.FindTestStepsStartedBy(*finished), testing::SizeIs(1u));
}

TEST(TestQuery, FindTestStepsFinishedBy_MinimalViaTestCaseFinished)
{
    const auto query = QueryFromNdjson(TestdataFile("minimal.ndjson"));

    const auto all = query.FindAllTestCaseStarted();
    ASSERT_THAT(all, testing::SizeIs(1u));

    const auto finished = query.FindTestCaseFinishedBy(all[0]);
    ASSERT_THAT(finished, testing::Optional(testing::_));

    EXPECT_THAT(query.FindTestStepsFinishedBy(*finished), testing::SizeIs(1u));
}

TEST(TestQuery, FindTestRunDuration_EmptyQueryReturnsNullopt)
{
    cucumber::query::Query query;
    EXPECT_THAT(query.FindTestRunDuration(), testing::Eq(std::nullopt));
}

TEST(TestQuery, FindLineageBy_ViaTestCaseStarted)
{
    const auto query = QueryFromNdjson(TestdataFile("minimal.ndjson"));

    const auto all = query.FindAllTestCaseStarted();
    ASSERT_THAT(all, testing::SizeIs(1u));

    const auto lineage = query.FindLineageBy(all[0]);
    ASSERT_THAT(lineage, testing::Optional(testing::_));
    EXPECT_THAT(lineage->feature, testing::Optional(testing::_));
    EXPECT_THAT(lineage->scenario, testing::Optional(testing::_));
}

TEST(TestQuery, FindLineageBy_ViaTestCaseFinished)
{
    const auto query = QueryFromNdjson(TestdataFile("minimal.ndjson"));

    const auto all = query.FindAllTestCaseStarted();
    ASSERT_THAT(all, testing::SizeIs(1u));

    const auto finished = query.FindTestCaseFinishedBy(all[0]);
    ASSERT_THAT(finished, testing::Optional(testing::_));

    const auto lineage = query.FindLineageBy(*finished);
    ASSERT_THAT(lineage, testing::Optional(testing::_));
    EXPECT_THAT(lineage->feature, testing::Optional(testing::_));
    EXPECT_THAT(lineage->scenario, testing::Optional(testing::_));
}

TEST(TestQuery, FindPickleBy_ViaTestCaseFinished)
{
    const auto query = QueryFromNdjson(TestdataFile("minimal.ndjson"));

    const auto all = query.FindAllTestCaseStarted();
    ASSERT_THAT(all, testing::SizeIs(1u));

    const auto finished = query.FindTestCaseFinishedBy(all[0]);
    ASSERT_THAT(finished, testing::Optional(testing::_));

    const auto pickle = query.FindPickleBy(*finished);
    ASSERT_THAT(pickle, testing::Optional(testing::_));
    EXPECT_THAT(pickle->name, testing::Eq("cukes"));
}

TEST(TestQuery, FindPickleBy_ViaTestStepStarted)
{
    const auto query = QueryFromNdjson(TestdataFile("minimal.ndjson"));

    const auto all = query.FindAllTestCaseStarted();
    ASSERT_THAT(all, testing::SizeIs(1u));

    const auto stepsStarted = query.FindTestStepsStartedBy(all[0]);
    ASSERT_THAT(stepsStarted, testing::SizeIs(1u));

    const auto pickle = query.FindPickleBy(stepsStarted[0]);
    ASSERT_THAT(pickle, testing::Optional(testing::_));
    EXPECT_THAT(pickle->name, testing::Eq("cukes"));
}

TEST(TestQuery, FindPickleBy_ViaTestStepFinished)
{
    const auto query = QueryFromNdjson(TestdataFile("minimal.ndjson"));

    const auto all = query.FindAllTestCaseStarted();
    ASSERT_THAT(all, testing::SizeIs(1u));

    const auto stepsFinished = query.FindTestStepsFinishedBy(all[0]);
    ASSERT_THAT(stepsFinished, testing::SizeIs(1u));

    const auto pickle = query.FindPickleBy(stepsFinished[0]);
    ASSERT_THAT(pickle, testing::Optional(testing::_));
    EXPECT_THAT(pickle->name, testing::Eq("cukes"));
}

TEST(TestQuery, FindPickleStepBy_ViaTestStep)
{
    const auto query = QueryFromNdjson(TestdataFile("minimal.ndjson"));

    const auto all = query.FindAllTestCaseStarted();
    ASSERT_THAT(all, testing::SizeIs(1u));

    const auto stepsStarted = query.FindTestStepsStartedBy(all[0]);
    ASSERT_THAT(stepsStarted, testing::SizeIs(1u));

    const auto testStep = query.FindTestStepBy(stepsStarted[0]);
    ASSERT_THAT(testStep, testing::Optional(testing::_));

    const auto pickleStep = query.FindPickleStepBy(*testStep);
    EXPECT_THAT(pickleStep, testing::Optional(testing::_));
}

TEST(TestQuery, FindStepBy_ViaPickleStep)
{
    const auto query = QueryFromNdjson(TestdataFile("minimal.ndjson"));

    const auto all = query.FindAllTestCaseStarted();
    ASSERT_THAT(all, testing::SizeIs(1u));

    const auto stepsStarted = query.FindTestStepsStartedBy(all[0]);
    ASSERT_THAT(stepsStarted, testing::SizeIs(1u));

    const auto testStep = query.FindTestStepBy(stepsStarted[0]);
    ASSERT_THAT(testStep, testing::Optional(testing::_));

    const auto pickleStep = query.FindPickleStepBy(*testStep);
    ASSERT_THAT(pickleStep, testing::Optional(testing::_));

    const auto step = query.FindStepBy(*pickleStep);
    EXPECT_THAT(step, testing::Optional(testing::_));
}

TEST(TestQuery, FindStepDefinitionsBy_MinimalTestStepHasOneDefinition)
{
    const auto query = QueryFromNdjson(TestdataFile("minimal.ndjson"));

    const auto all = query.FindAllTestCaseStarted();
    ASSERT_THAT(all, testing::SizeIs(1u));

    const auto stepsStarted = query.FindTestStepsStartedBy(all[0]);
    ASSERT_THAT(stepsStarted, testing::SizeIs(1u));

    const auto testStep = query.FindTestStepBy(stepsStarted[0]);
    ASSERT_THAT(testStep, testing::Optional(testing::_));

    const auto stepDefinitions = query.FindStepDefinitionsBy(*testStep);
    EXPECT_THAT(stepDefinitions, testing::SizeIs(1u));
}

TEST(TestQuery, FindUnambiguousStepDefinitionBy_MinimalReturnsDefinition)
{
    const auto query = QueryFromNdjson(TestdataFile("minimal.ndjson"));

    const auto all = query.FindAllTestCaseStarted();
    ASSERT_THAT(all, testing::SizeIs(1u));

    const auto stepsStarted = query.FindTestStepsStartedBy(all[0]);
    ASSERT_THAT(stepsStarted, testing::SizeIs(1u));

    const auto testStep = query.FindTestStepBy(stepsStarted[0]);
    ASSERT_THAT(testStep, testing::Optional(testing::_));

    const auto stepDefinition = query.FindUnambiguousStepDefinitionBy(*testStep);
    EXPECT_THAT(stepDefinition, testing::Optional(testing::_));
}

TEST(TestQuery, FindHookBy_ViaTestStepInHooksFixture)
{
    const auto query = QueryFromNdjson(TestdataFile("hooks.ndjson"));

    const auto all = query.FindAllTestCaseStarted();
    ASSERT_THAT(all, testing::Not(testing::IsEmpty()));

    bool foundHook = false;

    for (const auto& testCaseStarted : all)
    {
        for (const auto& testStepStarted : query.FindTestStepsStartedBy(testCaseStarted))
        {
            const auto testStep = query.FindTestStepBy(testStepStarted);

            if (testStep && query.FindHookBy(*testStep).has_value())
            {
                foundHook = true;
                break;
            }
        }

        if (foundHook)
            break;
    }

    EXPECT_THAT(foundHook, testing::IsTrue());
}

TEST(TestQuery, FindHookBy_ViaTestRunHookStarted)
{
    const auto query = QueryFromNdjson(TestdataFile("global-hooks.ndjson"));
    const auto hooks = query.FindAllTestRunHookStarted();
    ASSERT_THAT(hooks, testing::Not(testing::IsEmpty()));

    const auto hook = query.FindHookBy(hooks[0]);
    EXPECT_THAT(hook, testing::Optional(testing::_));
}

TEST(TestQuery, FindHookBy_ViaTestRunHookFinished)
{
    const auto query = QueryFromNdjson(TestdataFile("global-hooks.ndjson"));
    const auto hooks = query.FindAllTestRunHookFinished();
    ASSERT_THAT(hooks, testing::Not(testing::IsEmpty()));

    const auto hook = query.FindHookBy(hooks[0]);
    EXPECT_THAT(hook, testing::Optional(testing::_));
}

TEST(TestQuery, FindTestRunHookStartedBy_ViaTestRunHookFinished)
{
    const auto query = QueryFromNdjson(TestdataFile("global-hooks.ndjson"));
    const auto finished = query.FindAllTestRunHookFinished();
    ASSERT_THAT(finished, testing::Not(testing::IsEmpty()));

    const auto started = query.FindTestRunHookStartedBy(finished[0]);
    EXPECT_THAT(started, testing::Optional(testing::_));
}

TEST(TestQuery, FindTestRunHookFinishedBy_ViaTestRunHookStarted)
{
    const auto query = QueryFromNdjson(TestdataFile("global-hooks.ndjson"));
    const auto started = query.FindAllTestRunHookStarted();
    ASSERT_THAT(started, testing::Not(testing::IsEmpty()));

    const auto finished = query.FindTestRunHookFinishedBy(started[0]);
    EXPECT_THAT(finished, testing::Optional(testing::_));
}

TEST(TestQuery, FindAttachmentsBy_TestRunHookFinishedHasAttachments)
{
    const auto query = QueryFromNdjson(TestdataFile("global-hooks-attachments.ndjson"));
    const auto finished = query.FindAllTestRunHookFinished();
    ASSERT_THAT(finished, testing::Not(testing::IsEmpty()));

    bool foundAttachment = false;

    for (const auto& hookFinished : finished)
        if (!query.FindAttachmentsBy(hookFinished).empty())
        {
            foundAttachment = true;
            break;
        }

    EXPECT_THAT(foundAttachment, testing::IsTrue());
}

TEST(TestQuery, FindTestCaseBy_ViaTestCaseFinished)
{
    const auto query = QueryFromNdjson(TestdataFile("minimal.ndjson"));

    const auto all = query.FindAllTestCaseStarted();
    ASSERT_THAT(all, testing::SizeIs(1u));

    const auto finished = query.FindTestCaseFinishedBy(all[0]);
    ASSERT_THAT(finished, testing::Optional(testing::_));

    const auto testCase = query.FindTestCaseBy(*finished);
    EXPECT_THAT(testCase, testing::Optional(testing::_));
}

TEST(TestQuery, FindTestCaseBy_ViaTestStepStarted)
{
    const auto query = QueryFromNdjson(TestdataFile("minimal.ndjson"));

    const auto all = query.FindAllTestCaseStarted();
    ASSERT_THAT(all, testing::SizeIs(1u));

    const auto stepsStarted = query.FindTestStepsStartedBy(all[0]);
    ASSERT_THAT(stepsStarted, testing::SizeIs(1u));

    const auto testCase = query.FindTestCaseBy(stepsStarted[0]);
    EXPECT_THAT(testCase, testing::Optional(testing::_));
}

TEST(TestQuery, FindTestCaseBy_ViaTestStepFinished)
{
    const auto query = QueryFromNdjson(TestdataFile("minimal.ndjson"));

    const auto all = query.FindAllTestCaseStarted();
    ASSERT_THAT(all, testing::SizeIs(1u));

    const auto stepsFinished = query.FindTestStepsFinishedBy(all[0]);
    ASSERT_THAT(stepsFinished, testing::SizeIs(1u));

    const auto testCase = query.FindTestCaseBy(stepsFinished[0]);
    EXPECT_THAT(testCase, testing::Optional(testing::_));
}

TEST(TestQuery, FindTestCaseStartedBy_ViaTestCaseFinished)
{
    const auto query = QueryFromNdjson(TestdataFile("minimal.ndjson"));

    const auto all = query.FindAllTestCaseStarted();
    ASSERT_THAT(all, testing::SizeIs(1u));

    const auto finished = query.FindTestCaseFinishedBy(all[0]);
    ASSERT_THAT(finished, testing::Optional(testing::_));

    const auto started = query.FindTestCaseStartedBy(*finished);
    ASSERT_THAT(started, testing::Optional(testing::_));
    EXPECT_THAT(started->id, testing::Eq(all[0].id));
}

TEST(TestQuery, FindTestCaseStartedBy_ViaTestStepStarted)
{
    const auto query = QueryFromNdjson(TestdataFile("minimal.ndjson"));

    const auto all = query.FindAllTestCaseStarted();
    ASSERT_THAT(all, testing::SizeIs(1u));

    const auto stepsStarted = query.FindTestStepsStartedBy(all[0]);
    ASSERT_THAT(stepsStarted, testing::SizeIs(1u));

    const auto started = query.FindTestCaseStartedBy(stepsStarted[0]);
    ASSERT_THAT(started, testing::Optional(testing::_));
    EXPECT_THAT(started->id, testing::Eq(all[0].id));
}

TEST(TestQuery, FindTestCaseStartedBy_ViaTestStepFinished)
{
    const auto query = QueryFromNdjson(TestdataFile("minimal.ndjson"));

    const auto all = query.FindAllTestCaseStarted();
    ASSERT_THAT(all, testing::SizeIs(1u));

    const auto stepsFinished = query.FindTestStepsFinishedBy(all[0]);
    ASSERT_THAT(stepsFinished, testing::SizeIs(1u));

    const auto started = query.FindTestCaseStartedBy(stepsFinished[0]);
    ASSERT_THAT(started, testing::Optional(testing::_));
    EXPECT_THAT(started->id, testing::Eq(all[0].id));
}

TEST(TestQuery, FindTestStepFinishedAndTestStepBy_MinimalHasOnePair)
{
    const auto query = QueryFromNdjson(TestdataFile("minimal.ndjson"));

    const auto all = query.FindAllTestCaseStarted();
    ASSERT_THAT(all, testing::SizeIs(1u));

    const auto pairs = query.FindTestStepFinishedAndTestStepBy(all[0]);
    ASSERT_THAT(pairs, testing::SizeIs(1u));
    EXPECT_THAT(pairs[0].first.test_case_started_id, testing::Eq(all[0].id));
}

TEST(TestQuery, FindSuggestionsBy_UnknownParameterTypePickleStepHasSuggestions)
{
    const auto query = QueryFromNdjson(TestdataFile("unknown-parameter-type.ndjson"));
    const auto pickleSteps = query.FindAllPickleSteps();
    ASSERT_THAT(pickleSteps, testing::Not(testing::IsEmpty()));

    bool foundSuggestion = false;

    for (const auto& pickleStep : pickleSteps)
        if (!query.FindSuggestionsBy(pickleStep).empty())
        {
            foundSuggestion = true;
            break;
        }

    EXPECT_THAT(foundSuggestion, testing::IsTrue());
}

TEST(TestQuery, FindSuggestionsBy_UnknownParameterTypePickleHasSuggestions)
{
    const auto query = QueryFromNdjson(TestdataFile("unknown-parameter-type.ndjson"));
    const auto pickles = query.FindAllPickles();
    ASSERT_THAT(pickles, testing::Not(testing::IsEmpty()));

    bool foundSuggestion = false;

    for (const auto& pickle : pickles)
        if (!query.FindSuggestionsBy(pickle).empty())
        {
            foundSuggestion = true;
            break;
        }

    EXPECT_THAT(foundSuggestion, testing::IsTrue());
}

TEST(TestQuery, CountMostSevereTestStepResultStatus_AllStatusesHasExpectedCounts)
{
    const auto query = QueryFromNdjson(TestdataFile("all-statuses.ndjson"));
    const auto counts = query.CountMostSevereTestStepResultStatus();

    using Status = cucumber::messages::test_step_result_status;
    EXPECT_THAT(counts.at(Status::PASSED), testing::Gt(0));
    EXPECT_THAT(counts.at(Status::FAILED), testing::Gt(0));
    EXPECT_THAT(counts.at(Status::SKIPPED), testing::Gt(0));
    EXPECT_THAT(counts.at(Status::PENDING), testing::Gt(0));
    EXPECT_THAT(counts.at(Status::UNDEFINED), testing::Gt(0));
}
