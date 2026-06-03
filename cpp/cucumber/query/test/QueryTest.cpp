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
#include <gtest/gtest.h>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace
{

    std::vector<cucumber::messages::envelope>
    loadNdjson(const std::string& path)
    {
        std::ifstream file(path);
        EXPECT_TRUE(file.is_open()) << "Cannot open: " << path;
        std::vector<cucumber::messages::envelope> envelopes;
        std::string line;

        while (std::getline(file, line))
        {
            if (line.empty())
                continue;

            auto j = nlohmann::json::parse(line);
            cucumber::messages::envelope env;
            cucumber::messages::from_json(j, env);
            envelopes.push_back(std::move(env));
        }
        return envelopes;
    }

    cucumber::query::Query queryFromNdjson(const std::string& path)
    {
        cucumber::query::Query q;

        for (const auto& env : loadNdjson(path))
        {
            q.Update(env);
        }

        return q;
    }

    std::string testdataFile(const std::string& name)
    {
        return std::string(TESTDATA_DIR) + "/" + name;
    }

    std::vector<std::string> LoadExpectedIds(const std::string& path)
    {
        std::ifstream file(path);
        EXPECT_TRUE(file.is_open()) << "Cannot open: " << path;
        auto parsedJson = nlohmann::json::parse(file);
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

        const auto lhsColumn = (lhs.location && lhs.location->column) ? *lhs.location->column : 0u;
        const auto rhsColumn = (rhs.location && rhs.location->column) ? *rhs.location->column : 0u;
        return static_cast<int32_t>(rhsColumn) - static_cast<int32_t>(lhsColumn);
    }

} // namespace

TEST(FindAllTestCaseStarted, RetainsTimestampOrder)
{
    using namespace cucumber::messages;

    test_case_started a;
    a.id = "1";
    a.test_case_id = "1";
    a.attempt = 0;
    a.timestamp.seconds = 1;
    a.timestamp.nanos = 1;

    test_case_started b;
    b.id = "2";
    b.test_case_id = "2";
    b.attempt = 0;
    b.timestamp.seconds = 2;
    b.timestamp.nanos = 1;

    test_case_started c;
    c.id = "3";
    c.test_case_id = "3";
    c.attempt = 0;
    c.timestamp.seconds = 2;
    c.timestamp.nanos = 3;

    cucumber::query::Query q;
    envelope ec;
    ec.test_case_started = c;
    q.Update(ec);
    envelope eb;
    eb.test_case_started = b;
    q.Update(eb);
    envelope ea;
    ea.test_case_started = a;
    q.Update(ea);

    auto result = q.FindAllTestCaseStarted();
    ASSERT_EQ(result.size(), 3u);
    EXPECT_EQ(result[0].id, "1");
    EXPECT_EQ(result[1].id, "2");
    EXPECT_EQ(result[2].id, "3");
}

TEST(FindAllTestCaseStarted, UsesIdAsTieBreaker)
{
    using namespace cucumber::messages;

    test_case_started a;
    a.id = "1";
    a.test_case_id = "1";
    a.attempt = 0;
    a.timestamp.seconds = 1;
    a.timestamp.nanos = 1;

    test_case_started b;
    b.id = "2";
    b.test_case_id = "2";
    b.attempt = 0;
    b.timestamp.seconds = 1;
    b.timestamp.nanos = 1;

    cucumber::query::Query q;
    envelope eb;
    eb.test_case_started = b;
    q.Update(eb);
    envelope ea;
    ea.test_case_started = a;
    q.Update(ea);

    auto result = q.FindAllTestCaseStarted();
    ASSERT_EQ(result.size(), 2u);
    EXPECT_EQ(result[0].id, "1");
    EXPECT_EQ(result[1].id, "2");
}

TEST(FindAllTestCaseStarted, ExcludesRetriedTestCases)
{
    using namespace cucumber::messages;

    test_case_started tcs;
    tcs.id = "1";
    tcs.test_case_id = "1";
    tcs.attempt = 0;
    tcs.timestamp.seconds = 0;
    tcs.timestamp.nanos = 0;

    test_case_finished tcf;
    tcf.test_case_started_id = "1";
    tcf.timestamp.seconds = 1;
    tcf.timestamp.nanos = 0;
    tcf.will_be_retried = true;

    cucumber::query::Query q;
    envelope e1;
    e1.test_case_started = tcs;
    q.Update(e1);
    envelope e2;
    e2.test_case_finished = tcf;
    q.Update(e2);

    EXPECT_TRUE(q.FindAllTestCaseStarted().empty());
}

TEST(FindLineageBy, MinimalScenario)
{
    auto envelopes = loadNdjson(testdataFile("minimal.ndjson"));

    cucumber::query::Query q;

    for (const auto& env : envelopes)
        q.Update(env);

    auto docIt = std::ranges::find_if(envelopes,
        [](const auto& e)
        {
            return e.gherkin_document.has_value();
        });
    ASSERT_NE(docIt, envelopes.end());
    const auto& gherkinDoc = *docIt->gherkin_document;

    auto pickleIt = std::ranges::find_if(envelopes,
        [](const auto& e)
        {
            return e.pickle.has_value();
        });
    ASSERT_NE(pickleIt, envelopes.end());
    const auto& pickle = *pickleIt->pickle;

    auto lineage = q.FindLineageBy(pickle);
    ASSERT_TRUE(lineage.has_value());
    ASSERT_TRUE(lineage->gherkinDocument.has_value());
    ASSERT_TRUE(lineage->feature.has_value());
    ASSERT_TRUE(lineage->scenario.has_value());
    EXPECT_FALSE(lineage->background.has_value());
    EXPECT_FALSE(lineage->rule.has_value());
    EXPECT_FALSE(lineage->examples.has_value());

    EXPECT_EQ(lineage->feature->name, gherkinDoc.feature->name);
    EXPECT_EQ(lineage->scenario->name, "cukes");
}

TEST(FindLineageBy, ExamplesTablePickle)
{
    auto envelopes = loadNdjson(testdataFile("examples-tables.ndjson"));

    cucumber::query::Query q;

    for (const auto& env : envelopes)
        q.Update(env);

    auto pickleIt = std::ranges::find_if(envelopes,
        [](const auto& e)
        {
            return e.pickle.has_value();
        });
    ASSERT_NE(pickleIt, envelopes.end());
    const auto& pickle = *pickleIt->pickle;

    auto lineage = q.FindLineageBy(pickle);
    ASSERT_TRUE(lineage.has_value());
    EXPECT_TRUE(lineage->scenario.has_value());
    EXPECT_TRUE(lineage->examples.has_value());
    EXPECT_TRUE(lineage->example.has_value());
    EXPECT_EQ(lineage->examplesIndex, 0u);
    EXPECT_EQ(lineage->exampleIndex, 0u);
}

TEST(FindLineageBy, RulesBackgroundsPickle)
{
    auto envelopes = loadNdjson(testdataFile("rules-backgrounds.ndjson"));

    cucumber::query::Query q;

    for (const auto& env : envelopes)
        q.Update(env);

    auto pickleIt = std::ranges::find_if(envelopes,
        [](const auto& e)
        {
            return e.pickle.has_value();
        });
    ASSERT_NE(pickleIt, envelopes.end());
    const auto& pickle = *pickleIt->pickle;

    auto lineage = q.FindLineageBy(pickle);
    ASSERT_TRUE(lineage.has_value());
    EXPECT_TRUE(lineage->feature.has_value());
    EXPECT_TRUE(lineage->rule.has_value());
    EXPECT_TRUE(lineage->ruleBackground.has_value());
    EXPECT_TRUE(lineage->scenario.has_value());
}

TEST(CountTestCasesStarted, Minimal)
{
    auto q = queryFromNdjson(testdataFile("minimal.ndjson"));
    EXPECT_EQ(q.CountTestCasesStarted(), 1u);
}

TEST(CountTestCasesStarted, EmptySteps)
{
    auto q = queryFromNdjson(testdataFile("empty.ndjson"));
    EXPECT_EQ(q.CountTestCasesStarted(), 1u);
}

TEST(FindMostSevereTestStepResultBy, MinimalPassed)
{
    auto envelopes = loadNdjson(testdataFile("minimal.ndjson"));
    cucumber::query::Query q;

    for (const auto& env : envelopes)
        q.Update(env);

    auto all = q.FindAllTestCaseStarted();
    ASSERT_EQ(all.size(), 1u);

    auto result = q.FindMostSevereTestStepResultBy(all[0]);
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->status, cucumber::messages::test_step_result_status::PASSED);
}

TEST(FindMostSevereTestStepResultBy, AllStatusesMostSevereIsFailed)
{
    auto envelopes = loadNdjson(testdataFile("all-statuses.ndjson"));
    cucumber::query::Query q;

    for (const auto& env : envelopes)
        q.Update(env);

    bool foundFailed = false;

    for (const auto& testCaseStarted : q.FindAllTestCaseStarted())
    {
        auto result = q.FindMostSevereTestStepResultBy(testCaseStarted);

        if (result && result->status == cucumber::messages::test_step_result_status::FAILED)
        {
            foundFailed = true;
            break;
        }
    }

    EXPECT_TRUE(foundFailed);
}

TEST(FindTestCaseDurationBy, Minimal)
{
    auto envelopes = loadNdjson(testdataFile("minimal.ndjson"));
    cucumber::query::Query q;

    for (const auto& env : envelopes)
        q.Update(env);

    auto all = q.FindAllTestCaseStarted();
    ASSERT_EQ(all.size(), 1u);

    auto duration = q.FindTestCaseDurationBy(all[0]);
    ASSERT_TRUE(duration.has_value());
    EXPECT_EQ(duration->seconds, 0u);
    EXPECT_EQ(duration->nanos, 3'000'000u);
}

TEST(FindAttachmentsBy, MinimalHasNone)
{
    auto envelopes = loadNdjson(testdataFile("minimal.ndjson"));
    cucumber::query::Query q;

    for (const auto& env : envelopes)
        q.Update(env);

    auto all = q.FindAllTestCaseStarted();
    ASSERT_EQ(all.size(), 1u);
    auto finished = q.FindTestStepsFinishedBy(all[0]);
    ASSERT_EQ(finished.size(), 1u);

    auto attachments = q.FindAttachmentsBy(finished[0]);
    EXPECT_TRUE(attachments.empty());
}

TEST(FindAttachmentsBy, AttachmentsNdjsonHasAttachments)
{
    auto envelopes = loadNdjson(testdataFile("attachments.ndjson"));
    cucumber::query::Query q;

    for (const auto& env : envelopes)
        q.Update(env);

    bool foundAttachment = false;

    for (const auto& testCaseStarted : q.FindAllTestCaseStarted())
    {
        for (const auto& testStepFinished : q.FindTestStepsFinishedBy(testCaseStarted))
        {
            if (!q.FindAttachmentsBy(testStepFinished).empty())
            {
                foundAttachment = true;
                break;
            }
        }

        if (foundAttachment)
            break;
    }

    EXPECT_TRUE(foundAttachment);
}

TEST(CountMostSevereTestStepResultStatus, MinimalHasOnePassed)
{
    auto q = queryFromNdjson(testdataFile("minimal.ndjson"));
    auto counts = q.CountMostSevereTestStepResultStatus();

    EXPECT_EQ(counts[cucumber::messages::test_step_result_status::PASSED], 1);
    EXPECT_EQ(counts[cucumber::messages::test_step_result_status::FAILED], 0);
    EXPECT_EQ(counts[cucumber::messages::test_step_result_status::SKIPPED], 0);
    EXPECT_EQ(counts[cucumber::messages::test_step_result_status::PENDING], 0);
    EXPECT_EQ(counts[cucumber::messages::test_step_result_status::UNDEFINED], 0);
    EXPECT_EQ(counts[cucumber::messages::test_step_result_status::AMBIGUOUS], 0);
    EXPECT_EQ(counts[cucumber::messages::test_step_result_status::UNKNOWN], 0);
}

TEST(FindAllPickles, MinimalHasOnePickle)
{
    auto q = queryFromNdjson(testdataFile("minimal.ndjson"));
    EXPECT_EQ(q.FindAllPickles().size(), 1u);
}

TEST(FindAllStepDefinitions, MinimalHasOneStepDefinition)
{
    auto q = queryFromNdjson(testdataFile("minimal.ndjson"));
    EXPECT_EQ(q.FindAllStepDefinitions().size(), 1u);
}

TEST(FindTestRun, MinimalStartedAndFinished)
{
    auto q = queryFromNdjson(testdataFile("minimal.ndjson"));

    ASSERT_TRUE(q.FindTestRunStarted().has_value());
    ASSERT_TRUE(q.FindTestRunFinished().has_value());

    auto duration = q.FindTestRunDuration();
    ASSERT_TRUE(duration.has_value());
}

TEST(FindPickleBy, ViaTestCaseStarted)
{
    auto envelopes = loadNdjson(testdataFile("minimal.ndjson"));
    cucumber::query::Query q;

    for (const auto& env : envelopes)
        q.Update(env);

    auto all = q.FindAllTestCaseStarted();
    ASSERT_EQ(all.size(), 1u);

    auto pickle = q.FindPickleBy(all[0]);
    ASSERT_TRUE(pickle.has_value());
    EXPECT_EQ(pickle->name, "cukes");
}

TEST(FindAllTestCaseFinished, MinimalHasOneFinished)
{
    auto q = queryFromNdjson(testdataFile("minimal.ndjson"));
    EXPECT_EQ(q.FindAllTestCaseFinished().size(), 1u);
}

TEST(Retry, RetriedCasesExcludedFinalIncluded)
{
    auto q = queryFromNdjson(testdataFile("retry.ndjson"));
    auto all = q.FindAllTestCaseStarted();
    EXPECT_FALSE(all.empty());

    for (const auto& testCaseStarted : all)
    {
        auto testCaseFinished = q.FindTestCaseFinishedBy(testCaseStarted);

        if (testCaseFinished)
        {
            EXPECT_FALSE(testCaseFinished->will_be_retried)
                << "Retried test case should not appear in FindAllTestCaseStarted()";
        }
    }
}

TEST(FindAllTestCases, MinimalHasOneTestCase)
{
    auto envelopes = loadNdjson(testdataFile("minimal.ndjson"));
    cucumber::query::Query q;

    for (const auto& env : envelopes)
        q.Update(env);

    auto all = q.FindAllTestCaseStarted();
    ASSERT_EQ(all.size(), 1u);

    auto testCase = q.FindTestCaseBy(all[0]);
    EXPECT_TRUE(testCase.has_value());
}

TEST(FindAllTestCaseStartedOrderBy, EmptyQuery)
{
    cucumber::query::Query q;

    auto result = q.FindAllTestCaseStartedOrderBy<cucumber::messages::pickle>(
        [](const cucumber::query::Query& query, const cucumber::messages::test_case_started& testCaseStarted)
        {
            return query.FindPickleBy(testCaseStarted);
        },
        ReversePickleComparator);

    EXPECT_TRUE(result.empty());
}

TEST(FindAllTestCaseStartedOrderBy, ItemsWithoutPickleAreSortedToEnd)
{
    cucumber::messages::test_case_started testCaseStartedWithPickle;
    testCaseStartedWithPickle.id = "linked";
    testCaseStartedWithPickle.test_case_id = "tc1";
    testCaseStartedWithPickle.attempt = 0;
    testCaseStartedWithPickle.timestamp.seconds = 1;
    testCaseStartedWithPickle.timestamp.nanos = 0;

    cucumber::messages::test_case_started testCaseStartedWithoutPickle;
    testCaseStartedWithoutPickle.id = "unlinked";
    testCaseStartedWithoutPickle.test_case_id = "tc_unknown";
    testCaseStartedWithoutPickle.attempt = 0;
    testCaseStartedWithoutPickle.timestamp.seconds = 2;
    testCaseStartedWithoutPickle.timestamp.nanos = 0;

    cucumber::messages::pickle pickle;
    pickle.id = "pickle1";
    pickle.uri = "features/a.feature";
    pickle.location = cucumber::messages::location{};
    pickle.location->line = 3;
    pickle.location->column = 1;

    cucumber::messages::test_case testCase;
    testCase.id = "tc1";
    testCase.pickle_id = "pickle1";

    cucumber::query::Query q;
    cucumber::messages::envelope e1;
    e1.pickle = pickle;
    q.Update(e1);
    cucumber::messages::envelope e2;
    e2.test_case = testCase;
    q.Update(e2);
    cucumber::messages::envelope e3;
    e3.test_case_started = testCaseStartedWithPickle;
    q.Update(e3);
    cucumber::messages::envelope e4;
    e4.test_case_started = testCaseStartedWithoutPickle;
    q.Update(e4);

    auto result = q.FindAllTestCaseStartedOrderBy<cucumber::messages::pickle>(
        [](const cucumber::query::Query& query, const cucumber::messages::test_case_started& testCaseStarted)
        {
            return query.FindPickleBy(testCaseStarted);
        },
        ReversePickleComparator);

    ASSERT_EQ(result.size(), 2u);
    EXPECT_EQ(result[0].id, "linked");
    EXPECT_EQ(result[1].id, "unlinked");
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
    auto query = queryFromNdjson(testdataFile(fixture.ndjsonName));
    auto expectedIds = LoadExpectedIds(testdataFile(fixture.resultsName));

    auto result = query.FindAllTestCaseStartedOrderBy<Pickle>(
        [](const cucumber::query::Query& q, const TestCaseStarted& testCaseStarted)
        {
            return q.FindPickleBy(testCaseStarted);
        },
        ReversePickleComparator);

    std::vector<std::string> actualIds;
    actualIds.reserve(result.size());

    for (const auto& testCaseStarted : result)
        actualIds.push_back(testCaseStarted.id);

    EXPECT_EQ(actualIds, expectedIds);
}

INSTANTIATE_TEST_SUITE_P(
    Fixtures,
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
        FindAllTestCaseStartedOrderByFixture{ "unknown-parameter-type.ndjson", "unknown-parameter-type.findAllTestCaseStartedOrderBy.results.json" }
    )
);

TEST(FindAllTestCaseFinishedOrderBy, EmptyQuery)
{
    cucumber::query::Query q;

    auto result = q.FindAllTestCaseFinishedOrderBy<cucumber::messages::pickle>(
        [](const cucumber::query::Query& query, const cucumber::messages::test_case_finished& testCaseFinished)
        {
            return query.FindPickleBy(testCaseFinished);
        },
        ReversePickleComparator);

    EXPECT_TRUE(result.empty());
}

TEST(FindAllTestCaseFinishedOrderBy, ItemsWithoutPickleAreSortedToEnd)
{
    cucumber::messages::test_case_started testCaseStartedWithPickle;
    testCaseStartedWithPickle.id = "linked";
    testCaseStartedWithPickle.test_case_id = "tc1";
    testCaseStartedWithPickle.attempt = 0;
    testCaseStartedWithPickle.timestamp.seconds = 1;
    testCaseStartedWithPickle.timestamp.nanos = 0;

    cucumber::messages::test_case_finished testCaseFinishedWithPickle;
    testCaseFinishedWithPickle.test_case_started_id = "linked";
    testCaseFinishedWithPickle.timestamp.seconds = 2;
    testCaseFinishedWithPickle.timestamp.nanos = 0;
    testCaseFinishedWithPickle.will_be_retried = false;

    cucumber::messages::test_case_started testCaseStartedWithoutPickle;
    testCaseStartedWithoutPickle.id = "unlinked";
    testCaseStartedWithoutPickle.test_case_id = "tc_unknown";
    testCaseStartedWithoutPickle.attempt = 0;
    testCaseStartedWithoutPickle.timestamp.seconds = 3;
    testCaseStartedWithoutPickle.timestamp.nanos = 0;

    cucumber::messages::test_case_finished testCaseFinishedWithoutPickle;
    testCaseFinishedWithoutPickle.test_case_started_id = "unlinked";
    testCaseFinishedWithoutPickle.timestamp.seconds = 4;
    testCaseFinishedWithoutPickle.timestamp.nanos = 0;
    testCaseFinishedWithoutPickle.will_be_retried = false;

    cucumber::messages::pickle pickle;
    pickle.id = "pickle1";
    pickle.uri = "features/a.feature";
    pickle.location = cucumber::messages::location{};
    pickle.location->line = 3;
    pickle.location->column = 1;

    cucumber::messages::test_case testCase;
    testCase.id = "tc1";
    testCase.pickle_id = "pickle1";

    cucumber::query::Query q;
    cucumber::messages::envelope e1;
    e1.pickle = pickle;
    q.Update(e1);
    cucumber::messages::envelope e2;
    e2.test_case = testCase;
    q.Update(e2);
    cucumber::messages::envelope e3;
    e3.test_case_started = testCaseStartedWithPickle;
    q.Update(e3);
    cucumber::messages::envelope e4;
    e4.test_case_finished = testCaseFinishedWithPickle;
    q.Update(e4);
    cucumber::messages::envelope e5;
    e5.test_case_started = testCaseStartedWithoutPickle;
    q.Update(e5);
    cucumber::messages::envelope e6;
    e6.test_case_finished = testCaseFinishedWithoutPickle;
    q.Update(e6);

    auto result = q.FindAllTestCaseFinishedOrderBy<cucumber::messages::pickle>(
        [](const cucumber::query::Query& query, const cucumber::messages::test_case_finished& testCaseFinished)
        {
            return query.FindPickleBy(testCaseFinished);
        },
        ReversePickleComparator);

    ASSERT_EQ(result.size(), 2u);
    EXPECT_EQ(result[0].test_case_started_id, "linked");
    EXPECT_EQ(result[1].test_case_started_id, "unlinked");
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
    auto query = queryFromNdjson(testdataFile(fixture.ndjsonName));
    auto expectedIds = LoadExpectedIds(testdataFile(fixture.resultsName));

    auto result = query.FindAllTestCaseFinishedOrderBy<Pickle>(
        [](const cucumber::query::Query& q, const TestCaseFinished& testCaseFinished)
        {
            return q.FindPickleBy(testCaseFinished);
        },
        ReversePickleComparator);

    std::vector<std::string> actualIds;
    actualIds.reserve(result.size());

    for (const auto& testCaseFinished : result)
        actualIds.push_back(testCaseFinished.test_case_started_id);

    EXPECT_EQ(actualIds, expectedIds);
}

INSTANTIATE_TEST_SUITE_P(
    Fixtures,
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
        FindAllTestCaseFinishedOrderByFixture{ "unknown-parameter-type.ndjson", "unknown-parameter-type.findAllTestCaseFinishedOrderBy.results.json" }
    )
);
