#include "cucumber/query/Query.hpp"
#include "cucumber/messages/all.hpp"
#include "cucumber/messages/envelope.hpp"
#include "cucumber/messages/location.hpp"
#include "cucumber/messages/pickle.hpp"
#include "cucumber/messages/pickle_step.hpp"
#include "cucumber/messages/test_case.hpp"
#include "cucumber/messages/test_case_finished.hpp"
#include "cucumber/messages/test_case_started.hpp"
#include "cucumber/messages/test_run_finished.hpp"
#include "cucumber/messages/test_run_hook_finished.hpp"
#include "cucumber/messages/test_run_hook_started.hpp"
#include "cucumber/messages/test_run_started.hpp"
#include "cucumber/messages/test_step.hpp"
#include "cucumber/messages/test_step_finished.hpp"
#include "cucumber/messages/test_step_result_status.hpp"
#include "cucumber/messages/test_step_started.hpp"
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

    std::pair<std::vector<cucumber::messages::envelope>, cucumber::query::Query> LoadNdjsonWithQuery(const std::string& path)
    {
        auto envelopes = LoadNdjson(path);
        cucumber::query::Query query;

        for (const auto& env : envelopes)
            query.Update(env);

        return { envelopes, std::move(query) };
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

    const cucumber::messages::pickle& FindFirstPickle(const std::vector<cucumber::messages::envelope>& envelopes)
    {
        const auto pickleIt = std::ranges::find_if(envelopes,
            [](const auto& e)
            {
                return e.pickle.has_value();
            });
        EXPECT_THAT(pickleIt, testing::Ne(envelopes.end()));
        return *pickleIt->pickle;
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

TEST(TestQuery, CountMostSevereTestStepResultStatus_MinimalHasOnePassed)
{
    using namespace cucumber::messages;

    // Given
    const auto query = QueryFromNdjson(TestdataFile("minimal.ndjson"));

    // When
    const auto counts = query.CountMostSevereTestStepResultStatus();

    // Then
    EXPECT_THAT(counts.at(test_step_result_status::PASSED), testing::Eq(1));
    EXPECT_THAT(counts.at(test_step_result_status::FAILED), testing::Eq(0));
    EXPECT_THAT(counts.at(test_step_result_status::SKIPPED), testing::Eq(0));
    EXPECT_THAT(counts.at(test_step_result_status::PENDING), testing::Eq(0));
    EXPECT_THAT(counts.at(test_step_result_status::UNDEFINED), testing::Eq(0));
    EXPECT_THAT(counts.at(test_step_result_status::AMBIGUOUS), testing::Eq(0));
    EXPECT_THAT(counts.at(test_step_result_status::UNKNOWN), testing::Eq(0));
}

TEST(TestQuery, CountTestCasesStarted_Minimal)
{
    // Given
    const auto query = QueryFromNdjson(TestdataFile("minimal.ndjson"));

    // Then
    EXPECT_THAT(query.CountTestCasesStarted(), testing::Eq(1u));
}

TEST(TestQuery, CountTestCasesStarted_EmptySteps)
{
    // Given
    const auto query = QueryFromNdjson(TestdataFile("empty.ndjson"));

    // Then
    EXPECT_THAT(query.CountTestCasesStarted(), testing::Eq(1u));
}

TEST(TestQuery, FindAllPickles_MinimalHasOnePickle)
{
    // Given
    const auto query = QueryFromNdjson(TestdataFile("minimal.ndjson"));

    // Then
    EXPECT_THAT(query.FindAllPickles(), testing::SizeIs(1u));
}

TEST(TestQuery, FindAllPickleSteps_MinimalHasOnePickleStep)
{
    // Given
    const auto query = QueryFromNdjson(TestdataFile("minimal.ndjson"));

    // Then
    EXPECT_THAT(query.FindAllPickleSteps(), testing::SizeIs(1u));
}

TEST(TestQuery, FindAllStepDefinitions_MinimalHasOneStepDefinition)
{
    // Given
    const auto query = QueryFromNdjson(TestdataFile("minimal.ndjson"));

    // Then
    EXPECT_THAT(query.FindAllStepDefinitions(), testing::SizeIs(1u));
}

TEST(TestQuery, FindAllTestCaseFinished_MinimalHasOneFinished)
{
    // Given
    const auto query = QueryFromNdjson(TestdataFile("minimal.ndjson"));

    // Then
    EXPECT_THAT(query.FindAllTestCaseFinished(), testing::SizeIs(1u));
}

TEST(TestQuery, FindAllTestCaseFinished_TimestampSortDominatesIdOrder)
{
    using namespace cucumber::messages;

    // Given: "zzz_tcs" finishes earlier than "aaa_tcs", so it must sort first
    const test_case_started startedA{ .attempt = 0, .id = "aaa_tcs", .test_case_id = "tc1", .timestamp = { .seconds = 0, .nanos = 0 } };
    const test_case_started startedZ{ .attempt = 0, .id = "zzz_tcs", .test_case_id = "tc2", .timestamp = { .seconds = 0, .nanos = 0 } };
    const test_case_finished finishedA{ .test_case_started_id = "aaa_tcs", .timestamp = { .seconds = 2, .nanos = 0 }, .will_be_retried = false };
    const test_case_finished finishedZ{ .test_case_started_id = "zzz_tcs", .timestamp = { .seconds = 1, .nanos = 0 }, .will_be_retried = false };

    cucumber::query::Query query;
    query.Update(envelope{ .test_case_started = startedA });
    query.Update(envelope{ .test_case_started = startedZ });
    query.Update(envelope{ .test_case_finished = finishedA });
    query.Update(envelope{ .test_case_finished = finishedZ });

    // When
    const auto result = query.FindAllTestCaseFinished();

    // Then: earlier finish timestamp wins over lexicographic id order
    ASSERT_THAT(result, testing::SizeIs(2u));
    EXPECT_THAT(result[0].test_case_started_id, testing::Eq("zzz_tcs"));
    EXPECT_THAT(result[1].test_case_started_id, testing::Eq("aaa_tcs"));
}

TEST(TestQuery, FindAllTestCaseFinishedOrderBy_EmptyQuery)
{
    using namespace cucumber::messages;

    // Given
    cucumber::query::Query query;

    // When
    const auto result = query.FindAllTestCaseFinishedOrderBy<pickle>(
        [](const cucumber::query::Query& query, const test_case_finished& testCaseFinished)
        {
            return query.FindPickleBy(testCaseFinished);
        },
        ReversePickleComparator);

    // Then
    EXPECT_THAT(result, testing::IsEmpty());
}

TEST(TestQuery, FindAllTestCaseFinishedOrderBy_ItemsWithoutPickleAreSortedToEnd)
{
    using namespace cucumber::messages;

    // Given
    const test_case_started testCaseStartedWithPickle{ .attempt = 0, .id = "linked", .test_case_id = "tc1", .timestamp = { .seconds = 1, .nanos = 0 } };
    const test_case_finished testCaseFinishedWithPickle{ .test_case_started_id = "linked", .timestamp = { .seconds = 2, .nanos = 0 }, .will_be_retried = false };
    const test_case_started testCaseStartedWithoutPickle{ .attempt = 0, .id = "unlinked", .test_case_id = "tc_unknown", .timestamp = { .seconds = 3, .nanos = 0 } };
    const test_case_finished testCaseFinishedWithoutPickle{ .test_case_started_id = "unlinked", .timestamp = { .seconds = 4, .nanos = 0 }, .will_be_retried = false };
    const pickle aPickle{ .id = "pickle1", .uri = "features/a.feature", .location = location{ .line = 3, .column = 1 } };
    const test_case testCase{ .id = "tc1", .pickle_id = "pickle1" };

    cucumber::query::Query query;
    query.Update(envelope{ .pickle = aPickle });
    query.Update(envelope{ .test_case = testCase });
    query.Update(envelope{ .test_case_started = testCaseStartedWithPickle });
    query.Update(envelope{ .test_case_finished = testCaseFinishedWithPickle });
    query.Update(envelope{ .test_case_started = testCaseStartedWithoutPickle });
    query.Update(envelope{ .test_case_finished = testCaseFinishedWithoutPickle });

    // When
    const auto result = query.FindAllTestCaseFinishedOrderBy<pickle>(
        [](const cucumber::query::Query& query, const test_case_finished& testCaseFinished)
        {
            return query.FindPickleBy(testCaseFinished);
        },
        ReversePickleComparator);

    // Then
    ASSERT_THAT(result, testing::SizeIs(2u));
    EXPECT_THAT(result[0].test_case_started_id, testing::Eq("linked"));
    EXPECT_THAT(result[1].test_case_started_id, testing::Eq("unlinked"));
}

TEST(TestQuery, FindAllTestCaseFinishedOrderBy_OrderIsRespected)
{
    using namespace cucumber::messages;

    // Given
    const pickle pickleA{ .id = "p1", .uri = "features/a.feature", .location = location{ .line = 1, .column = 1 } };
    const pickle pickleB{ .id = "p2", .uri = "features/b.feature", .location = location{ .line = 1, .column = 1 } };
    const test_case testCaseA{ .id = "tc1", .pickle_id = "p1" };
    const test_case testCaseB{ .id = "tc2", .pickle_id = "p2" };
    const test_case_started startedA{ .attempt = 0, .id = "tcs1", .test_case_id = "tc1", .timestamp = { .seconds = 1, .nanos = 0 } };
    const test_case_started startedB{ .attempt = 0, .id = "tcs2", .test_case_id = "tc2", .timestamp = { .seconds = 2, .nanos = 0 } };
    const test_case_finished finishedA{ .test_case_started_id = "tcs1", .timestamp = { .seconds = 3, .nanos = 0 }, .will_be_retried = false };
    const test_case_finished finishedB{ .test_case_started_id = "tcs2", .timestamp = { .seconds = 4, .nanos = 0 }, .will_be_retried = false };

    cucumber::query::Query query;
    query.Update(envelope{ .pickle = pickleA });
    query.Update(envelope{ .pickle = pickleB });
    query.Update(envelope{ .test_case = testCaseA });
    query.Update(envelope{ .test_case = testCaseB });
    query.Update(envelope{ .test_case_started = startedA });
    query.Update(envelope{ .test_case_started = startedB });
    query.Update(envelope{ .test_case_finished = finishedA });
    query.Update(envelope{ .test_case_finished = finishedB });

    // When: sort by URI descending (b before a)
    const auto result = query.FindAllTestCaseFinishedOrderBy<pickle>(
        [](const cucumber::query::Query& q, const test_case_finished& tcf)
        {
            return q.FindPickleBy(tcf);
        },
        [](const pickle& lhs, const pickle& rhs)
        {
            return static_cast<int>(rhs.uri.compare(lhs.uri));
        });

    // Then: b.feature before a.feature
    ASSERT_THAT(result, testing::SizeIs(2u));
    EXPECT_THAT(result[0].test_case_started_id, testing::Eq("tcs2"));
    EXPECT_THAT(result[1].test_case_started_id, testing::Eq("tcs1"));
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
    using namespace cucumber::messages;

    // Given
    const auto& fixture = GetParam();
    const auto query = QueryFromNdjson(TestdataFile(fixture.ndjsonName));
    const auto expectedIds = LoadExpectedIds(TestdataFile(fixture.resultsName));

    // When
    const auto result = query.FindAllTestCaseFinishedOrderBy<pickle>(
        [](const cucumber::query::Query& query, const test_case_finished& testCaseFinished)
        {
            return query.FindPickleBy(testCaseFinished);
        },
        ReversePickleComparator);

    // Then
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

TEST(TestQuery, FindAllTestCases_MinimalHasOneTestCase)
{
    // Given
    const auto query = QueryFromNdjson(TestdataFile("minimal.ndjson"));
    const auto all = query.FindAllTestCaseStarted();
    ASSERT_THAT(all, testing::SizeIs(1u));

    // When
    const auto testCase = query.FindTestCaseBy(all[0]);

    // Then
    EXPECT_THAT(testCase, testing::Optional(testing::_));
}

TEST(TestQuery, FindAllTestCaseStarted_RetainsTimestampOrder)
{
    // Given
    using namespace cucumber::messages;

    const test_case_started a{ .attempt = 0, .id = "1", .test_case_id = "1", .timestamp = { .seconds = 1, .nanos = 1 } };
    const test_case_started b{ .attempt = 0, .id = "2", .test_case_id = "2", .timestamp = { .seconds = 2, .nanos = 1 } };
    const test_case_started c{ .attempt = 0, .id = "3", .test_case_id = "3", .timestamp = { .seconds = 2, .nanos = 3 } };

    cucumber::query::Query query;
    query.Update(envelope{ .test_case_started = c });
    query.Update(envelope{ .test_case_started = b });
    query.Update(envelope{ .test_case_started = a });

    // When
    const auto result = query.FindAllTestCaseStarted();

    // Then
    ASSERT_THAT(result, testing::SizeIs(3u));
    EXPECT_THAT(result[0].id, testing::Eq("1"));
    EXPECT_THAT(result[1].id, testing::Eq("2"));
    EXPECT_THAT(result[2].id, testing::Eq("3"));
}

TEST(TestQuery, FindAllTestCaseStarted_UsesIdAsTieBreaker)
{
    // Given
    using namespace cucumber::messages;

    const test_case_started a{ .attempt = 0, .id = "1", .test_case_id = "1", .timestamp = { .seconds = 1, .nanos = 1 } };
    const test_case_started b{ .attempt = 0, .id = "2", .test_case_id = "2", .timestamp = { .seconds = 1, .nanos = 1 } };

    cucumber::query::Query query;
    query.Update(envelope{ .test_case_started = b });
    query.Update(envelope{ .test_case_started = a });

    // When
    const auto result = query.FindAllTestCaseStarted();

    // Then
    ASSERT_THAT(result, testing::SizeIs(2u));
    EXPECT_THAT(result[0].id, testing::Eq("1"));
    EXPECT_THAT(result[1].id, testing::Eq("2"));
}

TEST(TestQuery, FindAllTestCaseStarted_ExcludesRetriedTestCases)
{
    // Given
    using namespace cucumber::messages;

    const test_case_started tcs{ .attempt = 0, .id = "1", .test_case_id = "1", .timestamp = { .seconds = 0, .nanos = 0 } };
    const test_case_finished tcf{ .test_case_started_id = "1", .timestamp = { .seconds = 1, .nanos = 0 }, .will_be_retried = true };

    cucumber::query::Query query;
    query.Update(envelope{ .test_case_started = tcs });
    query.Update(envelope{ .test_case_finished = tcf });

    // Then
    EXPECT_THAT(query.FindAllTestCaseStarted(), testing::IsEmpty());
}

TEST(TestQuery, FindAllTestCaseStarted_TimestampSortDominatesIdOrder)
{
    using namespace cucumber::messages;

    // Given: "aaa" has a later timestamp than "zzz", so "zzz" must sort first
    const test_case_started early{ .attempt = 0, .id = "aaa", .test_case_id = "1", .timestamp = { .seconds = 2, .nanos = 0 } };
    const test_case_started late{ .attempt = 0, .id = "zzz", .test_case_id = "2", .timestamp = { .seconds = 1, .nanos = 0 } };

    cucumber::query::Query query;
    query.Update(envelope{ .test_case_started = early });
    query.Update(envelope{ .test_case_started = late });

    // When
    const auto result = query.FindAllTestCaseStarted();

    // Then: earlier timestamp wins over lexicographic id order
    ASSERT_THAT(result, testing::SizeIs(2u));
    EXPECT_THAT(result[0].id, testing::Eq("zzz"));
    EXPECT_THAT(result[1].id, testing::Eq("aaa"));
}

TEST(TestQuery, FindAllTestCaseStarted_NanosResolutionAffectsSortOrder)
{
    using namespace cucumber::messages;

    // Given: same seconds, "zzz" has nanos=0 and "aaa" has nanos=2'000'000 (2ms later)
    const test_case_started first{ .attempt = 0, .id = "zzz", .test_case_id = "1", .timestamp = { .seconds = 1, .nanos = 0 } };
    const test_case_started second{ .attempt = 0, .id = "aaa", .test_case_id = "2", .timestamp = { .seconds = 1, .nanos = 2000000 } };

    cucumber::query::Query query;
    query.Update(envelope{ .test_case_started = second });
    query.Update(envelope{ .test_case_started = first });

    // When
    const auto result = query.FindAllTestCaseStarted();

    // Then: nanos difference is reflected in sort order
    ASSERT_THAT(result, testing::SizeIs(2u));
    EXPECT_THAT(result[0].id, testing::Eq("zzz"));
    EXPECT_THAT(result[1].id, testing::Eq("aaa"));
}

TEST(TestQuery, FindAllTestCaseStartedOrderBy_EmptyQuery)
{
    using namespace cucumber::messages;

    // Given
    cucumber::query::Query query;

    // When
    const auto result = query.FindAllTestCaseStartedOrderBy<pickle>(
        [](const cucumber::query::Query& query, const test_case_started& testCaseStarted)
        {
            return query.FindPickleBy(testCaseStarted);
        },
        ReversePickleComparator);

    // Then
    EXPECT_THAT(result, testing::IsEmpty());
}

TEST(TestQuery, FindAllTestCaseStartedOrderBy_ItemsWithoutPickleAreSortedToEnd)
{
    using namespace cucumber::messages;

    // Given
    const test_case_started testCaseStartedWithPickle{ .attempt = 0, .id = "linked", .test_case_id = "tc1", .timestamp = { .seconds = 1, .nanos = 0 } };
    const test_case_started testCaseStartedWithoutPickle{ .attempt = 0, .id = "unlinked", .test_case_id = "tc_unknown", .timestamp = { .seconds = 2, .nanos = 0 } };
    const pickle aPickle{ .id = "pickle1", .uri = "features/a.feature", .location = location{ .line = 3, .column = 1 } };
    const test_case testCase{ .id = "tc1", .pickle_id = "pickle1" };

    cucumber::query::Query query;
    query.Update(envelope{ .pickle = aPickle });
    query.Update(envelope{ .test_case = testCase });
    query.Update(envelope{ .test_case_started = testCaseStartedWithPickle });
    query.Update(envelope{ .test_case_started = testCaseStartedWithoutPickle });

    // When
    const auto result = query.FindAllTestCaseStartedOrderBy<pickle>(
        [](const cucumber::query::Query& query, const test_case_started& testCaseStarted)
        {
            return query.FindPickleBy(testCaseStarted);
        },
        ReversePickleComparator);

    // Then
    ASSERT_THAT(result, testing::SizeIs(2u));
    EXPECT_THAT(result[0].id, testing::Eq("linked"));
    EXPECT_THAT(result[1].id, testing::Eq("unlinked"));
}

TEST(TestQuery, FindAllTestCaseStartedOrderBy_OrderIsRespected)
{
    using namespace cucumber::messages;

    // Given
    const pickle pickleA{ .id = "p1", .uri = "features/a.feature", .location = location{ .line = 1, .column = 1 } };
    const pickle pickleB{ .id = "p2", .uri = "features/b.feature", .location = location{ .line = 1, .column = 1 } };
    const test_case testCaseA{ .id = "tc1", .pickle_id = "p1" };
    const test_case testCaseB{ .id = "tc2", .pickle_id = "p2" };
    const test_case_started startedA{ .attempt = 0, .id = "tcs1", .test_case_id = "tc1", .timestamp = { .seconds = 1, .nanos = 0 } };
    const test_case_started startedB{ .attempt = 0, .id = "tcs2", .test_case_id = "tc2", .timestamp = { .seconds = 2, .nanos = 0 } };
    const test_case_finished finishedA{ .test_case_started_id = "tcs1", .timestamp = { .seconds = 3, .nanos = 0 }, .will_be_retried = false };
    const test_case_finished finishedB{ .test_case_started_id = "tcs2", .timestamp = { .seconds = 4, .nanos = 0 }, .will_be_retried = false };

    cucumber::query::Query query;
    query.Update(envelope{ .pickle = pickleA });
    query.Update(envelope{ .pickle = pickleB });
    query.Update(envelope{ .test_case = testCaseA });
    query.Update(envelope{ .test_case = testCaseB });
    query.Update(envelope{ .test_case_started = startedA });
    query.Update(envelope{ .test_case_started = startedB });
    query.Update(envelope{ .test_case_finished = finishedA });
    query.Update(envelope{ .test_case_finished = finishedB });

    // When: sort by URI descending (b before a)
    const auto result = query.FindAllTestCaseStartedOrderBy<pickle>(
        [](const cucumber::query::Query& q, const test_case_started& tcs)
        {
            return q.FindPickleBy(tcs);
        },
        [](const pickle& lhs, const pickle& rhs)
        {
            return static_cast<int>(rhs.uri.compare(lhs.uri));
        });

    // Then: b.feature before a.feature
    ASSERT_THAT(result, testing::SizeIs(2u));
    EXPECT_THAT(result[0].id, testing::Eq("tcs2"));
    EXPECT_THAT(result[1].id, testing::Eq("tcs1"));
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
    using namespace cucumber::messages;

    // Given
    const auto& fixture = GetParam();
    const auto query = QueryFromNdjson(TestdataFile(fixture.ndjsonName));
    const auto expectedIds = LoadExpectedIds(TestdataFile(fixture.resultsName));

    // When
    const auto result = query.FindAllTestCaseStartedOrderBy<pickle>(
        [](const cucumber::query::Query& query, const test_case_started& testCaseStarted)
        {
            return query.FindPickleBy(testCaseStarted);
        },
        ReversePickleComparator);

    // Then
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

TEST(TestQuery, FindAllTestRunHookFinished_GlobalHooksHasHooks)
{
    // Given
    const auto query = QueryFromNdjson(TestdataFile("global-hooks.ndjson"));

    // Then
    EXPECT_THAT(query.FindAllTestRunHookFinished(), testing::Not(testing::IsEmpty()));
}

TEST(TestQuery, FindAllTestRunHookStarted_GlobalHooksHasHooks)
{
    // Given
    const auto query = QueryFromNdjson(TestdataFile("global-hooks.ndjson"));

    // Then
    EXPECT_THAT(query.FindAllTestRunHookStarted(), testing::Not(testing::IsEmpty()));
}

TEST(TestQuery, FindAllTestSteps_MinimalHasOneTestStep)
{
    // Given
    const auto query = QueryFromNdjson(TestdataFile("minimal.ndjson"));

    // Then
    EXPECT_THAT(query.FindAllTestSteps(), testing::SizeIs(1u));
}

TEST(TestQuery, FindAllTestStepFinished_MinimalHasOneTestStepFinished)
{
    // Given
    const auto query = QueryFromNdjson(TestdataFile("minimal.ndjson"));

    // Then
    EXPECT_THAT(query.FindAllTestStepFinished(), testing::SizeIs(1u));
}

TEST(TestQuery, FindAllTestStepStarted_MinimalHasOneTestStepStarted)
{
    // Given
    const auto query = QueryFromNdjson(TestdataFile("minimal.ndjson"));

    // Then
    EXPECT_THAT(query.FindAllTestStepStarted(), testing::SizeIs(1u));
}

TEST(TestQuery, FindAllUndefinedParameterTypes_UnknownParameterTypeHasAtLeastOne)
{
    // Given
    const auto query = QueryFromNdjson(TestdataFile("unknown-parameter-type.ndjson"));

    // Then
    EXPECT_THAT(query.FindAllUndefinedParameterTypes(), testing::Not(testing::IsEmpty()));
}

TEST(TestQuery, FindAttachmentsBy_MinimalHasNone)
{
    // Given
    const auto query = QueryFromNdjson(TestdataFile("minimal.ndjson"));
    const auto all = query.FindAllTestCaseStarted();
    ASSERT_THAT(all, testing::SizeIs(1u));
    const auto finished = query.FindTestStepsFinishedBy(all[0]);
    ASSERT_THAT(finished, testing::SizeIs(1u));

    // When
    const auto attachments = query.FindAttachmentsBy(finished[0]);

    // Then
    EXPECT_THAT(attachments, testing::IsEmpty());
}

TEST(TestQuery, FindAttachmentsBy_AttachmentsNdjsonHasAttachments)
{
    // Given
    const auto query = QueryFromNdjson(TestdataFile("attachments.ndjson"));
    bool foundAttachment = false;

    // When
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

    // Then
    EXPECT_THAT(foundAttachment, testing::IsTrue());
}

TEST(TestQuery, FindAttachmentsBy_TestRunHookFinishedHasAttachments)
{
    // Given
    const auto query = QueryFromNdjson(TestdataFile("global-hooks-attachments.ndjson"));
    const auto finished = query.FindAllTestRunHookFinished();
    ASSERT_THAT(finished, testing::Not(testing::IsEmpty()));
    bool foundAttachment = false;

    // When
    for (const auto& hookFinished : finished)
        if (!query.FindAttachmentsBy(hookFinished).empty())
        {
            foundAttachment = true;
            break;
        }

    // Then
    EXPECT_THAT(foundAttachment, testing::IsTrue());
}

TEST(TestQuery, FindAttachmentsBy_UnknownTestStepFinishedReturnsEmpty)
{
    using namespace cucumber::messages;

    // Given
    cucumber::query::Query query;
    const test_step_finished stepFinished{ .test_case_started_id = "nonexistent", .test_step_id = "s1" };

    // When / Then
    EXPECT_THAT(query.FindAttachmentsBy(stepFinished), testing::IsEmpty());
}

TEST(TestQuery, FindAttachmentsBy_UnknownTestRunHookFinishedReturnsEmpty)
{
    using namespace cucumber::messages;

    // Given
    cucumber::query::Query query;
    const test_run_hook_finished hookFinished{ .test_run_hook_started_id = "nonexistent" };

    // When / Then
    EXPECT_THAT(query.FindAttachmentsBy(hookFinished), testing::IsEmpty());
}

TEST(TestQuery, FindHookBy_ViaTestStepInHooksFixture)
{
    // Given
    const auto query = QueryFromNdjson(TestdataFile("hooks.ndjson"));
    const auto all = query.FindAllTestCaseStarted();
    ASSERT_THAT(all, testing::Not(testing::IsEmpty()));
    bool foundHook = false;

    // When
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

    // Then
    EXPECT_THAT(foundHook, testing::IsTrue());
}

TEST(TestQuery, FindHookBy_ViaTestRunHookStarted)
{
    // Given
    const auto query = QueryFromNdjson(TestdataFile("global-hooks.ndjson"));
    const auto hooks = query.FindAllTestRunHookStarted();
    ASSERT_THAT(hooks, testing::Not(testing::IsEmpty()));

    // When
    const auto hook = query.FindHookBy(hooks[0]);

    // Then
    EXPECT_THAT(hook, testing::Optional(testing::_));
}

TEST(TestQuery, FindHookBy_ViaTestRunHookFinished)
{
    // Given
    const auto query = QueryFromNdjson(TestdataFile("global-hooks.ndjson"));
    const auto hooks = query.FindAllTestRunHookFinished();
    ASSERT_THAT(hooks, testing::Not(testing::IsEmpty()));

    // When
    const auto hook = query.FindHookBy(hooks[0]);

    // Then
    EXPECT_THAT(hook, testing::Optional(testing::_));
}

TEST(TestQuery, FindHookBy_UnknownTestStepHookIdReturnsNullopt)
{
    using namespace cucumber::messages;

    // Given
    cucumber::query::Query query;
    const test_step testStep{ .hook_id = "nonexistent-hook", .id = "ts1" };

    // When / Then
    EXPECT_THAT(query.FindHookBy(testStep), testing::Eq(std::nullopt));
}

TEST(TestQuery, FindHookBy_UnknownTestRunHookStartedReturnsNullopt)
{
    using namespace cucumber::messages;

    // Given
    cucumber::query::Query query;
    const test_run_hook_started hookStarted{ .id = "h1", .hook_id = "nonexistent" };

    // When / Then
    EXPECT_THAT(query.FindHookBy(hookStarted), testing::Eq(std::nullopt));
}

TEST(TestQuery, FindHookBy_UnknownTestRunHookFinishedReturnsNullopt)
{
    using namespace cucumber::messages;

    // Given
    cucumber::query::Query query;
    const test_run_hook_finished hookFinished{ .test_run_hook_started_id = "nonexistent" };

    // When / Then
    EXPECT_THAT(query.FindHookBy(hookFinished), testing::Eq(std::nullopt));
}

TEST(TestQuery, FindLineageBy_MinimalScenario)
{
    // Given
    const auto [envelopes, query] = LoadNdjsonWithQuery(TestdataFile("minimal.ndjson"));

    const auto docIt = std::ranges::find_if(envelopes,
        [](const auto& e)
        {
            return e.gherkin_document.has_value();
        });
    ASSERT_THAT(docIt, testing::Ne(envelopes.end()));
    const auto& gherkinDoc = *docIt->gherkin_document;

    const auto& pickle = FindFirstPickle(envelopes);

    // When
    const auto lineage = query.FindLineageBy(pickle);

    // Then
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
    // Given
    const auto [envelopes, query] = LoadNdjsonWithQuery(TestdataFile("examples-tables.ndjson"));

    const auto& pickle = FindFirstPickle(envelopes);

    // When
    const auto lineage = query.FindLineageBy(pickle);

    // Then
    ASSERT_THAT(lineage, testing::Optional(testing::_));
    EXPECT_THAT(lineage->scenario, testing::Optional(testing::_));
    EXPECT_THAT(lineage->examples, testing::Optional(testing::_));
    EXPECT_THAT(lineage->example, testing::Optional(testing::_));
    EXPECT_THAT(lineage->examplesIndex, testing::Eq(0u));
    EXPECT_THAT(lineage->exampleIndex, testing::Eq(0u));
}

TEST(TestQuery, FindLineageBy_RulesBackgroundsPickle)
{
    // Given
    const auto [envelopes, query] = LoadNdjsonWithQuery(TestdataFile("rules-backgrounds.ndjson"));

    const auto& pickle = FindFirstPickle(envelopes);

    // When
    const auto lineage = query.FindLineageBy(pickle);

    // Then
    ASSERT_THAT(lineage, testing::Optional(testing::_));
    EXPECT_THAT(lineage->feature, testing::Optional(testing::_));
    EXPECT_THAT(lineage->rule, testing::Optional(testing::_));
    EXPECT_THAT(lineage->ruleBackground, testing::Optional(testing::_));
    EXPECT_THAT(lineage->scenario, testing::Optional(testing::_));
}

TEST(TestQuery, FindLineageBy_ViaTestCaseStarted)
{
    // Given
    const auto query = QueryFromNdjson(TestdataFile("minimal.ndjson"));
    const auto all = query.FindAllTestCaseStarted();
    ASSERT_THAT(all, testing::SizeIs(1u));

    // When
    const auto lineage = query.FindLineageBy(all[0]);

    // Then
    ASSERT_THAT(lineage, testing::Optional(testing::_));
    EXPECT_THAT(lineage->feature, testing::Optional(testing::_));
    EXPECT_THAT(lineage->scenario, testing::Optional(testing::_));
}

TEST(TestQuery, FindLineageBy_ViaTestCaseFinished)
{
    // Given
    const auto query = QueryFromNdjson(TestdataFile("minimal.ndjson"));
    const auto all = query.FindAllTestCaseStarted();
    ASSERT_THAT(all, testing::SizeIs(1u));
    const auto finished = query.FindTestCaseFinishedBy(all[0]);
    ASSERT_THAT(finished, testing::Optional(testing::_));

    // When
    const auto lineage = query.FindLineageBy(*finished);

    // Then
    ASSERT_THAT(lineage, testing::Optional(testing::_));
    EXPECT_THAT(lineage->feature, testing::Optional(testing::_));
    EXPECT_THAT(lineage->scenario, testing::Optional(testing::_));
}

TEST(TestQuery, FindLineageBy_EmptyAstNodeIdsReturnsNullopt)
{
    using namespace cucumber::messages;

    // Given: pickle with no ast_node_ids
    cucumber::query::Query query;
    const pickle p{ .id = "p1" };

    // When / Then
    EXPECT_THAT(query.FindLineageBy(p), testing::Eq(std::nullopt));
}

TEST(TestQuery, FindLineageBy_UnknownAstNodeIdReturnsNullopt)
{
    using namespace cucumber::messages;

    // Given: pickle referencing a node that was never indexed
    cucumber::query::Query query;
    const pickle p{ .id = "p1", .ast_node_ids = { "no-such-node" } };

    // When / Then
    EXPECT_THAT(query.FindLineageBy(p), testing::Eq(std::nullopt));
}

TEST(TestQuery, FindLineageBy_SecondExamplesHasCorrectIndex)
{
    using namespace cucumber::messages;

    // Given: examples-tables.ndjson has a scenario with 2 examples blocks and multiple rows
    const auto [envelopes, query] = LoadNdjsonWithQuery(TestdataFile("examples-tables.ndjson"));

    bool foundSecondExamples = false;
    bool foundSecondRow = false;

    // When: inspect lineage of every pickle
    for (const auto& env : envelopes)
    {
        if (!env.pickle)
            continue;
        const auto lineage = query.FindLineageBy(*env.pickle);
        if (!lineage || !lineage->examples)
            continue;
        if (lineage->examplesIndex == 1u)
            foundSecondExamples = true;
        if (lineage->exampleIndex == 1u)
            foundSecondRow = true;
    }

    // Then: at least one pickle has examplesIndex==1 and one has exampleIndex==1
    EXPECT_THAT(foundSecondExamples, testing::IsTrue()) << "No pickle with examplesIndex==1 found";
    EXPECT_THAT(foundSecondRow, testing::IsTrue()) << "No pickle with exampleIndex==1 found";
}

TEST(TestQuery, FindLineageBy_RuleScenarioIsIndexed)
{
    // Given: rules.ndjson has scenarios inside rules
    const auto [envelopes, query] = LoadNdjsonWithQuery(TestdataFile("rules.ndjson"));

    bool foundRuleScenario = false;

    // When: inspect lineage of every pickle
    for (const auto& env : envelopes)
    {
        if (!env.pickle)
            continue;
        const auto lineage = query.FindLineageBy(*env.pickle);
        if (lineage && lineage->rule)
        {
            foundRuleScenario = true;
            break;
        }
    }

    // Then: at least one pickle has rule lineage
    EXPECT_THAT(foundRuleScenario, testing::IsTrue()) << "No pickle with rule lineage found";
}

TEST(TestQuery, FindLineageBy_RuleBackgroundStepIsIndexed)
{
    // Given: rules-backgrounds.ndjson has scenarios in rules with backgrounds
    const auto query = QueryFromNdjson(TestdataFile("rules-backgrounds.ndjson"));
    const auto all = query.FindAllTestCaseStarted();
    ASSERT_THAT(all, testing::Not(testing::IsEmpty()));

    bool foundStep = false;

    // When: walk all steps looking for one that resolves through FindStepBy
    for (const auto& tcs : all)
    {
        const auto stepsStarted = query.FindTestStepsStartedBy(tcs);
        for (const auto& stepStarted : stepsStarted)
        {
            const auto testStep = query.FindTestStepBy(stepStarted);
            if (!testStep)
                continue;
            const auto pickleStep = query.FindPickleStepBy(*testStep);
            if (!pickleStep)
                continue;
            if (query.FindStepBy(*pickleStep))
            {
                foundStep = true;
                break;
            }
        }
        if (foundStep)
            break;
    }

    // Then: at least one step from the rule background is indexed
    EXPECT_THAT(foundStep, testing::IsTrue()) << "No step found via rule-background UpdateSteps";
}

TEST(TestQuery, FindLocationOf_MinimalPickleHasLocation)
{
    // Given
    const auto [envelopes, query] = LoadNdjsonWithQuery(TestdataFile("minimal.ndjson"));

    // When
    const auto location = query.FindLocationOf(FindFirstPickle(envelopes));

    // Then
    EXPECT_THAT(location, testing::Optional(testing::_));
}

TEST(TestQuery, FindLocationOf_ExamplesTablePickleUsesExampleLocation)
{
    // Given: examples-table pickle whose lineage has a row-level example
    const auto [envelopes, query] = LoadNdjsonWithQuery(TestdataFile("examples-tables.ndjson"));
    const auto& pickle = FindFirstPickle(envelopes);

    // When
    const auto location = query.FindLocationOf(pickle);

    // Then: location comes from the example row
    ASSERT_THAT(location, testing::Optional(testing::_));
    EXPECT_THAT(location->line, testing::Gt(0u));
}

TEST(TestQuery, FindMeta_MinimalHasMeta)
{
    // Given
    const auto query = QueryFromNdjson(TestdataFile("minimal.ndjson"));

    // Then
    EXPECT_THAT(query.FindMeta(), testing::Optional(testing::_));
}

TEST(TestQuery, FindMostSevereTestStepResultBy_MinimalPassed)
{
    using namespace cucumber::messages;

    // Given
    const auto query = QueryFromNdjson(TestdataFile("minimal.ndjson"));
    const auto all = query.FindAllTestCaseStarted();
    ASSERT_THAT(all, testing::SizeIs(1u));

    // When
    const auto result = query.FindMostSevereTestStepResultBy(all[0]);

    // Then
    ASSERT_THAT(result, testing::Optional(testing::_));
    EXPECT_THAT(result->status, testing::Eq(test_step_result_status::PASSED));
}

TEST(TestQuery, FindMostSevereTestStepResultBy_AllStatusesMostSevereIsFailed)
{
    using namespace cucumber::messages;

    // Given
    const auto query = QueryFromNdjson(TestdataFile("all-statuses.ndjson"));
    bool foundFailed = false;

    // When
    for (const auto& testCaseStarted : query.FindAllTestCaseStarted())
    {
        const auto result = query.FindMostSevereTestStepResultBy(testCaseStarted);

        if (result && result->status == test_step_result_status::FAILED)
        {
            foundFailed = true;
            break;
        }
    }

    // Then
    EXPECT_THAT(foundFailed, testing::IsTrue());
}

TEST(TestQuery, FindMostSevereTestStepResultBy_MinimalPassedViaTestCaseFinished)
{
    using namespace cucumber::messages;

    // Given
    const auto query = QueryFromNdjson(TestdataFile("minimal.ndjson"));
    const auto all = query.FindAllTestCaseStarted();
    ASSERT_THAT(all, testing::SizeIs(1u));
    const auto finished = query.FindTestCaseFinishedBy(all[0]);
    ASSERT_THAT(finished, testing::Optional(testing::_));

    // When
    const auto result = query.FindMostSevereTestStepResultBy(*finished);

    // Then
    ASSERT_THAT(result, testing::Optional(testing::_));
    EXPECT_THAT(result->status, testing::Eq(test_step_result_status::PASSED));
}

TEST(TestQuery, FindMostSevereTestStepResultBy_EmptyTestCaseStartedReturnsNullopt)
{
    using namespace cucumber::messages;

    // Given
    cucumber::query::Query query;
    const test_case_started started{ .attempt = 0, .id = "nonexistent", .test_case_id = "tc1" };

    // When / Then
    EXPECT_THAT(query.FindMostSevereTestStepResultBy(started), testing::Eq(std::nullopt));
}

TEST(TestQuery, FindMostSevereTestStepResultBy_SelectsMostSevereWhenMultipleSteps)
{
    using namespace cucumber::messages;

    // Given
    const test_case_started started{ .attempt = 0, .id = "tcs1", .test_case_id = "tc1", .timestamp = { .seconds = 0, .nanos = 0 } };
    const test_step_finished passedStep{
        .test_case_started_id = "tcs1",
        .test_step_id = "s1",
        .test_step_result = { .duration = {}, .message = {}, .status = test_step_result_status::PASSED }
    };
    const test_step_finished failedStep{
        .test_case_started_id = "tcs1",
        .test_step_id = "s2",
        .test_step_result = { .duration = {}, .message = {}, .status = test_step_result_status::FAILED }
    };

    cucumber::query::Query query;
    query.Update(envelope{ .test_case_started = started });
    query.Update(envelope{ .test_step_finished = passedStep });
    query.Update(envelope{ .test_step_finished = failedStep });

    // When
    const auto result = query.FindMostSevereTestStepResultBy(started);

    // Then
    ASSERT_THAT(result, testing::Optional(testing::_));
    EXPECT_THAT(result->status, testing::Eq(test_step_result_status::FAILED));
}

TEST(TestQuery, FindMostSevereTestStepResultBy_TestCaseStartedWithNoStepsReturnsNullopt)
{
    using namespace cucumber::messages;

    // Given: test case is registered but has no test step finished entries
    const test_case_started started{ .attempt = 0, .id = "tcs1", .test_case_id = "tc1", .timestamp = { .seconds = 0, .nanos = 0 } };
    const test_step_started stepStarted{ .test_case_started_id = "tcs1", .test_step_id = "s1" };

    cucumber::query::Query query;
    query.Update(envelope{ .test_case_started = started });
    query.Update(envelope{ .test_step_started = stepStarted });

    // When / Then: no finished steps, so no result
    EXPECT_THAT(query.FindMostSevereTestStepResultBy(started), testing::Eq(std::nullopt));
}

TEST(TestQuery, FindMostSevereTestStepResultBy_PassedBeforeFailedPicksFailed)
{
    using namespace cucumber::messages;

    // Given: three steps — passed, skipped, failed — FAILED must win
    const test_case_started started{ .attempt = 0, .id = "tcs1", .test_case_id = "tc1", .timestamp = { .seconds = 0, .nanos = 0 } };

    auto makeFinished = [](const std::string& stepId, test_step_result_status status) -> test_step_finished
    {
        return test_step_finished{
            .test_case_started_id = "tcs1",
            .test_step_id = stepId,
            .test_step_result = { .duration = {}, .message = {}, .status = status }
        };
    };

    cucumber::query::Query query;
    query.Update(envelope{ .test_case_started = started });
    query.Update(envelope{ .test_step_finished = makeFinished("s1", test_step_result_status::PASSED) });
    query.Update(envelope{ .test_step_finished = makeFinished("s2", test_step_result_status::SKIPPED) });
    query.Update(envelope{ .test_step_finished = makeFinished("s3", test_step_result_status::FAILED) });

    // When
    const auto result = query.FindMostSevereTestStepResultBy(started);

    // Then
    ASSERT_THAT(result, testing::Optional(testing::_));
    EXPECT_THAT(result->status, testing::Eq(test_step_result_status::FAILED));
}

TEST(TestQuery, FindPickleBy_ViaTestCaseStarted)
{
    // Given
    const auto query = QueryFromNdjson(TestdataFile("minimal.ndjson"));
    const auto all = query.FindAllTestCaseStarted();
    ASSERT_THAT(all, testing::SizeIs(1u));

    // When
    const auto pickle = query.FindPickleBy(all[0]);

    // Then
    ASSERT_THAT(pickle, testing::Optional(testing::_));
    EXPECT_THAT(pickle->name, testing::Eq("cukes"));
}

TEST(TestQuery, FindPickleBy_ViaTestCaseFinished)
{
    // Given
    const auto query = QueryFromNdjson(TestdataFile("minimal.ndjson"));
    const auto all = query.FindAllTestCaseStarted();
    ASSERT_THAT(all, testing::SizeIs(1u));
    const auto finished = query.FindTestCaseFinishedBy(all[0]);
    ASSERT_THAT(finished, testing::Optional(testing::_));

    // When
    const auto pickle = query.FindPickleBy(*finished);

    // Then
    ASSERT_THAT(pickle, testing::Optional(testing::_));
    EXPECT_THAT(pickle->name, testing::Eq("cukes"));
}

TEST(TestQuery, FindPickleBy_ViaTestStepStarted)
{
    // Given
    const auto query = QueryFromNdjson(TestdataFile("minimal.ndjson"));
    const auto all = query.FindAllTestCaseStarted();
    ASSERT_THAT(all, testing::SizeIs(1u));
    const auto stepsStarted = query.FindTestStepsStartedBy(all[0]);
    ASSERT_THAT(stepsStarted, testing::SizeIs(1u));

    // When
    const auto pickle = query.FindPickleBy(stepsStarted[0]);

    // Then
    ASSERT_THAT(pickle, testing::Optional(testing::_));
    EXPECT_THAT(pickle->name, testing::Eq("cukes"));
}

TEST(TestQuery, FindPickleBy_ViaTestStepFinished)
{
    // Given
    const auto query = QueryFromNdjson(TestdataFile("minimal.ndjson"));
    const auto all = query.FindAllTestCaseStarted();
    ASSERT_THAT(all, testing::SizeIs(1u));
    const auto stepsFinished = query.FindTestStepsFinishedBy(all[0]);
    ASSERT_THAT(stepsFinished, testing::SizeIs(1u));

    // When
    const auto pickle = query.FindPickleBy(stepsFinished[0]);

    // Then
    ASSERT_THAT(pickle, testing::Optional(testing::_));
    EXPECT_THAT(pickle->name, testing::Eq("cukes"));
}

TEST(TestQuery, FindPickleBy_ViaTestCaseStarted_UnknownPickleIdReturnsNullopt)
{
    using namespace cucumber::messages;

    // Given: test_case registered with a pickle_id that is not indexed
    const test_case tc{ .id = "tc1", .pickle_id = "no-such-pickle" };
    const test_case_started tcs{ .attempt = 0, .id = "tcs1", .test_case_id = "tc1", .timestamp = { .seconds = 0, .nanos = 0 } };

    cucumber::query::Query query;
    query.Update(envelope{ .test_case = tc });
    query.Update(envelope{ .test_case_started = tcs });

    // When / Then
    EXPECT_THAT(query.FindPickleBy(tcs), testing::Eq(std::nullopt));
}

TEST(TestQuery, FindPickleBy_ViaTestStepStarted_UnknownTestCaseStartedIdReturnsNullopt)
{
    using namespace cucumber::messages;

    // Given: step started referencing an unknown test case started id
    cucumber::query::Query query;
    const test_step_started tss{ .test_case_started_id = "nonexistent", .test_step_id = "s1" };

    // When / Then
    EXPECT_THAT(query.FindPickleBy(tss), testing::Eq(std::nullopt));
}

TEST(TestQuery, FindPickleBy_ViaTestStepFinished_UnknownTestCaseStartedIdReturnsNullopt)
{
    using namespace cucumber::messages;

    // Given: step finished referencing an unknown test case started id
    cucumber::query::Query query;
    const test_step_finished tsf{
        .test_case_started_id = "nonexistent",
        .test_step_id = "s1",
        .test_step_result = { .duration = {}, .message = {}, .status = test_step_result_status::PASSED }
    };

    // When / Then
    EXPECT_THAT(query.FindPickleBy(tsf), testing::Eq(std::nullopt));
}

TEST(TestQuery, FindPickleStepBy_ViaTestStep)
{
    // Given
    const auto query = QueryFromNdjson(TestdataFile("minimal.ndjson"));
    const auto all = query.FindAllTestCaseStarted();
    ASSERT_THAT(all, testing::SizeIs(1u));
    const auto stepsStarted = query.FindTestStepsStartedBy(all[0]);
    ASSERT_THAT(stepsStarted, testing::SizeIs(1u));
    const auto testStep = query.FindTestStepBy(stepsStarted[0]);
    ASSERT_THAT(testStep, testing::Optional(testing::_));

    // When
    const auto pickleStep = query.FindPickleStepBy(*testStep);

    // Then
    EXPECT_THAT(pickleStep, testing::Optional(testing::_));
}

TEST(TestQuery, FindPickleStepBy_UnknownPickleStepIdReturnsNullopt)
{
    using namespace cucumber::messages;

    // Given: test step referencing a pickle step id that was never indexed
    cucumber::query::Query query;
    const test_step ts{ .id = "ts1", .pickle_step_id = std::string("no-such-pickle-step") };

    // When / Then
    EXPECT_THAT(query.FindPickleStepBy(ts), testing::Eq(std::nullopt));
}

TEST(TestQuery, FindStepBy_ViaPickleStep)
{
    // Given
    const auto query = QueryFromNdjson(TestdataFile("minimal.ndjson"));
    const auto all = query.FindAllTestCaseStarted();
    ASSERT_THAT(all, testing::SizeIs(1u));
    const auto stepsStarted = query.FindTestStepsStartedBy(all[0]);
    ASSERT_THAT(stepsStarted, testing::SizeIs(1u));
    const auto testStep = query.FindTestStepBy(stepsStarted[0]);
    ASSERT_THAT(testStep, testing::Optional(testing::_));
    const auto pickleStep = query.FindPickleStepBy(*testStep);
    ASSERT_THAT(pickleStep, testing::Optional(testing::_));

    // When
    const auto step = query.FindStepBy(*pickleStep);

    // Then
    EXPECT_THAT(step, testing::Optional(testing::_));
}

TEST(TestQuery, FindStepBy_UnknownAstNodeIdReturnsNullopt)
{
    using namespace cucumber::messages;

    // Given
    cucumber::query::Query query;
    const pickle_step pickleStep{ .ast_node_ids = { "nonexistent" }, .id = "ps1" };

    // When / Then
    EXPECT_THAT(query.FindStepBy(pickleStep), testing::Eq(std::nullopt));
}

TEST(TestQuery, FindStepBy_EmptyAstNodeIdsReturnsNullopt)
{
    using namespace cucumber::messages;

    // Given
    cucumber::query::Query query;
    const pickle_step pickleStep{ .ast_node_ids = {}, .id = "ps1" };

    // When / Then
    EXPECT_THAT(query.FindStepBy(pickleStep), testing::Eq(std::nullopt));
}

TEST(TestQuery, FindStepBy_FeatureBackgroundStepIsIndexed)
{
    // Given: backgrounds.ndjson has a feature-level background whose steps must be indexed
    const auto query = QueryFromNdjson(TestdataFile("backgrounds.ndjson"));
    const auto all = query.FindAllTestCaseStarted();
    ASSERT_THAT(all, testing::Not(testing::IsEmpty()));

    const auto stepsStarted = query.FindTestStepsStartedBy(all[0]);
    ASSERT_THAT(stepsStarted, testing::Not(testing::IsEmpty()));

    const auto testStep = query.FindTestStepBy(stepsStarted[0]);
    ASSERT_THAT(testStep, testing::Optional(testing::_));

    const auto pickleStep = query.FindPickleStepBy(*testStep);
    ASSERT_THAT(pickleStep, testing::Optional(testing::_));

    // When / Then: step from background is findable
    EXPECT_THAT(query.FindStepBy(*pickleStep), testing::Optional(testing::_));
}

TEST(TestQuery, FindStepDefinitionsBy_MinimalTestStepHasOneDefinition)
{
    // Given
    const auto query = QueryFromNdjson(TestdataFile("minimal.ndjson"));
    const auto all = query.FindAllTestCaseStarted();
    ASSERT_THAT(all, testing::SizeIs(1u));
    const auto stepsStarted = query.FindTestStepsStartedBy(all[0]);
    ASSERT_THAT(stepsStarted, testing::SizeIs(1u));
    const auto testStep = query.FindTestStepBy(stepsStarted[0]);
    ASSERT_THAT(testStep, testing::Optional(testing::_));

    // When
    const auto stepDefinitions = query.FindStepDefinitionsBy(*testStep);

    // Then
    EXPECT_THAT(stepDefinitions, testing::SizeIs(1u));
}

TEST(TestQuery, FindStepDefinitionsBy_UnknownIdReturnsEmpty)
{
    using namespace cucumber::messages;

    // Given
    cucumber::query::Query query;
    const std::vector<std::string> ids{ "nonexistent" };
    const test_step testStep{ .id = "ts1", .step_definition_ids = ids };

    // When / Then
    EXPECT_THAT(query.FindStepDefinitionsBy(testStep), testing::IsEmpty());
}

TEST(TestQuery, FindStepDefinitionsBy_KnownIdIsIncluded)
{
    // Given: minimal fixture with one step definition
    const auto query = QueryFromNdjson(TestdataFile("minimal.ndjson"));
    const auto all = query.FindAllTestCaseStarted();
    ASSERT_THAT(all, testing::SizeIs(1u));
    const auto stepsStarted = query.FindTestStepsStartedBy(all[0]);
    ASSERT_THAT(stepsStarted, testing::SizeIs(1u));
    const auto testStep = query.FindTestStepBy(stepsStarted[0]);
    ASSERT_THAT(testStep, testing::Optional(testing::_));

    // When
    const auto defs = query.FindStepDefinitionsBy(*testStep);

    // Then
    EXPECT_THAT(defs, testing::SizeIs(1u));
}

TEST(TestQuery, FindSuggestionsBy_UnknownParameterTypePickleStepHasSuggestions)
{
    // Given
    const auto query = QueryFromNdjson(TestdataFile("unknown-parameter-type.ndjson"));
    const auto pickleSteps = query.FindAllPickleSteps();
    ASSERT_THAT(pickleSteps, testing::Not(testing::IsEmpty()));
    bool foundSuggestion = false;

    // When
    for (const auto& pickleStep : pickleSteps)
        if (!query.FindSuggestionsBy(pickleStep).empty())
        {
            foundSuggestion = true;
            break;
        }

    // Then
    EXPECT_THAT(foundSuggestion, testing::IsTrue());
}

TEST(TestQuery, FindSuggestionsBy_UnknownParameterTypePickleHasSuggestions)
{
    // Given
    const auto query = QueryFromNdjson(TestdataFile("unknown-parameter-type.ndjson"));
    const auto pickles = query.FindAllPickles();
    ASSERT_THAT(pickles, testing::Not(testing::IsEmpty()));
    bool foundSuggestion = false;

    // When
    for (const auto& pickle : pickles)
        if (!query.FindSuggestionsBy(pickle).empty())
        {
            foundSuggestion = true;
            break;
        }

    // Then
    EXPECT_THAT(foundSuggestion, testing::IsTrue());
}

TEST(TestQuery, FindSuggestionsBy_UnknownPickleStepReturnsEmpty)
{
    using namespace cucumber::messages;

    // Given
    cucumber::query::Query query;
    const pickle_step pickleStep{ .id = "nonexistent" };

    // When / Then
    EXPECT_THAT(query.FindSuggestionsBy(pickleStep), testing::IsEmpty());
}

TEST(TestQuery, FindSuggestionsBy_KnownPickleStepWithSuggestionIsFound)
{
    // Given: unknown-parameter-type fixture has at least one pickle step with a suggestion
    const auto query = QueryFromNdjson(TestdataFile("unknown-parameter-type.ndjson"));
    const auto pickleSteps = query.FindAllPickleSteps();
    ASSERT_THAT(pickleSteps, testing::Not(testing::IsEmpty()));

    bool found = false;

    // When: search for a pickle step that returns suggestions
    for (const auto& ps : pickleSteps)
    {
        if (!query.FindSuggestionsBy(ps).empty())
        {
            found = true;
            break;
        }
    }

    // Then
    EXPECT_THAT(found, testing::IsTrue());
}

TEST(TestQuery, FindTestCaseBy_ViaTestCaseFinished)
{
    // Given
    const auto query = QueryFromNdjson(TestdataFile("minimal.ndjson"));
    const auto all = query.FindAllTestCaseStarted();
    ASSERT_THAT(all, testing::SizeIs(1u));
    const auto finished = query.FindTestCaseFinishedBy(all[0]);
    ASSERT_THAT(finished, testing::Optional(testing::_));

    // When
    const auto testCase = query.FindTestCaseBy(*finished);

    // Then
    EXPECT_THAT(testCase, testing::Optional(testing::_));
}

TEST(TestQuery, FindTestCaseBy_ViaTestStepStarted)
{
    // Given
    const auto query = QueryFromNdjson(TestdataFile("minimal.ndjson"));
    const auto all = query.FindAllTestCaseStarted();
    ASSERT_THAT(all, testing::SizeIs(1u));
    const auto stepsStarted = query.FindTestStepsStartedBy(all[0]);
    ASSERT_THAT(stepsStarted, testing::SizeIs(1u));

    // When
    const auto testCase = query.FindTestCaseBy(stepsStarted[0]);

    // Then
    EXPECT_THAT(testCase, testing::Optional(testing::_));
}

TEST(TestQuery, FindTestCaseBy_ViaTestStepFinished)
{
    // Given
    const auto query = QueryFromNdjson(TestdataFile("minimal.ndjson"));
    const auto all = query.FindAllTestCaseStarted();
    ASSERT_THAT(all, testing::SizeIs(1u));
    const auto stepsFinished = query.FindTestStepsFinishedBy(all[0]);
    ASSERT_THAT(stepsFinished, testing::SizeIs(1u));

    // When
    const auto testCase = query.FindTestCaseBy(stepsFinished[0]);

    // Then
    EXPECT_THAT(testCase, testing::Optional(testing::_));
}

TEST(TestQuery, FindTestCaseDurationBy_Minimal)
{
    // Given
    const auto query = QueryFromNdjson(TestdataFile("minimal.ndjson"));
    const auto all = query.FindAllTestCaseStarted();
    ASSERT_THAT(all, testing::SizeIs(1u));

    // When
    const auto duration = query.FindTestCaseDurationBy(all[0]);

    // Then
    ASSERT_THAT(duration, testing::Optional(testing::_));
    EXPECT_THAT(duration->seconds, testing::Eq(0u));
    EXPECT_THAT(duration->nanos, testing::Eq(3'000'000u));
}

TEST(TestQuery, FindTestCaseDurationBy_MinimalViaTestCaseFinished)
{
    // Given
    const auto query = QueryFromNdjson(TestdataFile("minimal.ndjson"));
    const auto all = query.FindAllTestCaseStarted();
    ASSERT_THAT(all, testing::SizeIs(1u));
    const auto finished = query.FindTestCaseFinishedBy(all[0]);
    ASSERT_THAT(finished, testing::Optional(testing::_));

    // When
    const auto duration = query.FindTestCaseDurationBy(*finished);

    // Then
    ASSERT_THAT(duration, testing::Optional(testing::_));
    EXPECT_THAT(duration->seconds, testing::Eq(0u));
    EXPECT_THAT(duration->nanos, testing::Eq(3'000'000u));
}

TEST(TestQuery, FindTestCaseFinishedBy_UnknownTestCaseStartedReturnsNullopt)
{
    using namespace cucumber::messages;

    // Given
    cucumber::query::Query query;
    const test_case_started started{ .attempt = 0, .id = "nonexistent", .test_case_id = "tc1" };

    // When / Then
    EXPECT_THAT(query.FindTestCaseFinishedBy(started), testing::Eq(std::nullopt));
}

TEST(TestQuery, FindTestCaseStartedBy_ViaTestCaseFinished)
{
    // Given
    const auto query = QueryFromNdjson(TestdataFile("minimal.ndjson"));
    const auto all = query.FindAllTestCaseStarted();
    ASSERT_THAT(all, testing::SizeIs(1u));
    const auto finished = query.FindTestCaseFinishedBy(all[0]);
    ASSERT_THAT(finished, testing::Optional(testing::_));

    // When
    const auto started = query.FindTestCaseStartedBy(*finished);

    // Then
    ASSERT_THAT(started, testing::Optional(testing::_));
    EXPECT_THAT(started->id, testing::Eq(all[0].id));
}

TEST(TestQuery, FindTestCaseStartedBy_ViaTestStepStarted)
{
    // Given
    const auto query = QueryFromNdjson(TestdataFile("minimal.ndjson"));
    const auto all = query.FindAllTestCaseStarted();
    ASSERT_THAT(all, testing::SizeIs(1u));
    const auto stepsStarted = query.FindTestStepsStartedBy(all[0]);
    ASSERT_THAT(stepsStarted, testing::SizeIs(1u));

    // When
    const auto started = query.FindTestCaseStartedBy(stepsStarted[0]);

    // Then
    ASSERT_THAT(started, testing::Optional(testing::_));
    EXPECT_THAT(started->id, testing::Eq(all[0].id));
}

TEST(TestQuery, FindTestCaseStartedBy_ViaTestStepFinished)
{
    // Given
    const auto query = QueryFromNdjson(TestdataFile("minimal.ndjson"));
    const auto all = query.FindAllTestCaseStarted();
    ASSERT_THAT(all, testing::SizeIs(1u));
    const auto stepsFinished = query.FindTestStepsFinishedBy(all[0]);
    ASSERT_THAT(stepsFinished, testing::SizeIs(1u));

    // When
    const auto started = query.FindTestCaseStartedBy(stepsFinished[0]);

    // Then
    ASSERT_THAT(started, testing::Optional(testing::_));
    EXPECT_THAT(started->id, testing::Eq(all[0].id));
}

TEST(TestQuery, FindTestCaseStartedBy_UnknownTestCaseFinishedReturnsNullopt)
{
    using namespace cucumber::messages;

    // Given
    cucumber::query::Query query;
    const test_case_finished finished{ .test_case_started_id = "nonexistent" };

    // When / Then
    EXPECT_THAT(query.FindTestCaseStartedBy(finished), testing::Eq(std::nullopt));
}

TEST(TestQuery, FindTestCaseStartedBy_UnknownTestStepStartedReturnsNullopt)
{
    using namespace cucumber::messages;

    // Given
    cucumber::query::Query query;
    const test_step_started stepStarted{ .test_case_started_id = "nonexistent", .test_step_id = "s1" };

    // When / Then
    EXPECT_THAT(query.FindTestCaseStartedBy(stepStarted), testing::Eq(std::nullopt));
}

TEST(TestQuery, FindTestCaseStartedBy_UnknownTestStepFinishedReturnsNullopt)
{
    using namespace cucumber::messages;

    // Given
    cucumber::query::Query query;
    const test_step_finished stepFinished{ .test_case_started_id = "nonexistent", .test_step_id = "s1" };

    // When / Then
    EXPECT_THAT(query.FindTestCaseStartedBy(stepFinished), testing::Eq(std::nullopt));
}

TEST(TestQuery, FindTestRun_MinimalStartedAndFinished)
{
    // Given
    const auto query = QueryFromNdjson(TestdataFile("minimal.ndjson"));

    // Then
    ASSERT_THAT(query.FindTestRunStarted(), testing::Optional(testing::_));
    ASSERT_THAT(query.FindTestRunFinished(), testing::Optional(testing::_));
    const auto duration = query.FindTestRunDuration();
    ASSERT_THAT(duration, testing::Optional(testing::_));
}

TEST(TestQuery, FindTestRunDuration_EmptyQueryReturnsNullopt)
{
    // Given
    cucumber::query::Query query;

    // Then
    EXPECT_THAT(query.FindTestRunDuration(), testing::Eq(std::nullopt));
}

TEST(TestQuery, FindTestRunDuration_ValueMatchesTimestampDifference)
{
    using namespace cucumber::messages;

    // Given: run from t=1.000s to t=3.500s
    const test_run_started started{ .timestamp = { .seconds = 1, .nanos = 0 } };
    const test_run_finished finished{ .timestamp = { .seconds = 3, .nanos = 500000000 } };

    cucumber::query::Query query;
    query.Update(envelope{ .test_run_started = started });
    query.Update(envelope{ .test_run_finished = finished });

    // When
    const auto duration = query.FindTestRunDuration();

    // Then: duration is 2s 500'000'000ns
    ASSERT_THAT(duration, testing::Optional(testing::_));
    EXPECT_THAT(duration->seconds, testing::Eq(2u));
    EXPECT_THAT(duration->nanos, testing::Eq(500000000u));
}

TEST(TestQuery, FindTestRunHookFinishedBy_ViaTestRunHookStarted)
{
    // Given
    const auto query = QueryFromNdjson(TestdataFile("global-hooks.ndjson"));
    const auto started = query.FindAllTestRunHookStarted();
    ASSERT_THAT(started, testing::Not(testing::IsEmpty()));

    // When
    const auto finished = query.FindTestRunHookFinishedBy(started[0]);

    // Then
    EXPECT_THAT(finished, testing::Optional(testing::_));
}

TEST(TestQuery, FindTestRunHookFinishedBy_UnknownTestRunHookStartedReturnsNullopt)
{
    using namespace cucumber::messages;

    // Given
    cucumber::query::Query query;
    const test_run_hook_started hookStarted{ .id = "nonexistent", .hook_id = "h1" };

    // When / Then
    EXPECT_THAT(query.FindTestRunHookFinishedBy(hookStarted), testing::Eq(std::nullopt));
}

TEST(TestQuery, FindTestRunHookStartedBy_ViaTestRunHookFinished)
{
    // Given
    const auto query = QueryFromNdjson(TestdataFile("global-hooks.ndjson"));
    const auto finished = query.FindAllTestRunHookFinished();
    ASSERT_THAT(finished, testing::Not(testing::IsEmpty()));

    // When
    const auto started = query.FindTestRunHookStartedBy(finished[0]);

    // Then
    EXPECT_THAT(started, testing::Optional(testing::_));
}

TEST(TestQuery, FindTestRunHookStartedBy_UnknownTestRunHookFinishedReturnsNullopt)
{
    using namespace cucumber::messages;

    // Given
    cucumber::query::Query query;
    const test_run_hook_finished hookFinished{ .test_run_hook_started_id = "nonexistent" };

    // When / Then
    EXPECT_THAT(query.FindTestRunHookStartedBy(hookFinished), testing::Eq(std::nullopt));
}

TEST(TestQuery, FindTestStepFinishedAndTestStepBy_MinimalHasOnePair)
{
    // Given
    const auto query = QueryFromNdjson(TestdataFile("minimal.ndjson"));
    const auto all = query.FindAllTestCaseStarted();
    ASSERT_THAT(all, testing::SizeIs(1u));

    // When
    const auto pairs = query.FindTestStepFinishedAndTestStepBy(all[0]);

    // Then
    ASSERT_THAT(pairs, testing::SizeIs(1u));
    EXPECT_THAT(pairs[0].first.test_case_started_id, testing::Eq(all[0].id));
}

TEST(TestQuery, FindTestStepBy_UnknownTestStepStartedReturnsNullopt)
{
    using namespace cucumber::messages;

    // Given
    cucumber::query::Query query;
    const test_step_started stepStarted{ .test_case_started_id = "tcs1", .test_step_id = "nonexistent" };

    // When / Then
    EXPECT_THAT(query.FindTestStepBy(stepStarted), testing::Eq(std::nullopt));
}

TEST(TestQuery, FindTestStepBy_UnknownTestStepFinishedReturnsNullopt)
{
    using namespace cucumber::messages;

    // Given
    cucumber::query::Query query;
    const test_step_finished stepFinished{ .test_case_started_id = "tcs1", .test_step_id = "nonexistent" };

    // When / Then
    EXPECT_THAT(query.FindTestStepBy(stepFinished), testing::Eq(std::nullopt));
}

TEST(TestQuery, FindTestStepsFinishedBy_MinimalViaTestCaseFinished)
{
    // Given
    const auto query = QueryFromNdjson(TestdataFile("minimal.ndjson"));
    const auto all = query.FindAllTestCaseStarted();
    ASSERT_THAT(all, testing::SizeIs(1u));
    const auto finished = query.FindTestCaseFinishedBy(all[0]);
    ASSERT_THAT(finished, testing::Optional(testing::_));

    // Then
    EXPECT_THAT(query.FindTestStepsFinishedBy(*finished), testing::SizeIs(1u));
}

TEST(TestQuery, FindTestStepsFinishedBy_UnknownTestCaseStartedReturnsEmpty)
{
    using namespace cucumber::messages;

    // Given
    cucumber::query::Query query;
    const test_case_started started{ .attempt = 0, .id = "nonexistent", .test_case_id = "tc1" };

    // When / Then
    EXPECT_THAT(query.FindTestStepsFinishedBy(started), testing::IsEmpty());
}

TEST(TestQuery, FindTestStepsStartedBy_MinimalViaTestCaseFinished)
{
    // Given
    const auto query = QueryFromNdjson(TestdataFile("minimal.ndjson"));
    const auto all = query.FindAllTestCaseStarted();
    ASSERT_THAT(all, testing::SizeIs(1u));
    const auto finished = query.FindTestCaseFinishedBy(all[0]);
    ASSERT_THAT(finished, testing::Optional(testing::_));

    // Then
    EXPECT_THAT(query.FindTestStepsStartedBy(*finished), testing::SizeIs(1u));
}

TEST(TestQuery, FindTestStepsStartedBy_UnknownTestCaseStartedReturnsEmpty)
{
    using namespace cucumber::messages;

    // Given
    cucumber::query::Query query;
    const test_case_started started{ .attempt = 0, .id = "nonexistent", .test_case_id = "tc1" };

    // When / Then
    EXPECT_THAT(query.FindTestStepsStartedBy(started), testing::IsEmpty());
}

TEST(TestQuery, FindUnambiguousStepDefinitionBy_MinimalReturnsDefinition)
{
    // Given
    const auto query = QueryFromNdjson(TestdataFile("minimal.ndjson"));
    const auto all = query.FindAllTestCaseStarted();
    ASSERT_THAT(all, testing::SizeIs(1u));
    const auto stepsStarted = query.FindTestStepsStartedBy(all[0]);
    ASSERT_THAT(stepsStarted, testing::SizeIs(1u));
    const auto testStep = query.FindTestStepBy(stepsStarted[0]);
    ASSERT_THAT(testStep, testing::Optional(testing::_));

    // When
    const auto stepDefinition = query.FindUnambiguousStepDefinitionBy(*testStep);

    // Then
    EXPECT_THAT(stepDefinition, testing::Optional(testing::_));
}

TEST(TestQuery, Retry_RetriedCasesExcludedFinalIncluded)
{
    // Given
    const auto query = QueryFromNdjson(TestdataFile("retry.ndjson"));

    // When
    const auto all = query.FindAllTestCaseStarted();

    // Then
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

TEST(TestQuery, CountMostSevereTestStepResultStatus_AllStatusesHasExpectedCounts)
{
    // Given
    const auto query = QueryFromNdjson(TestdataFile("all-statuses.ndjson"));

    // When
    const auto counts = query.CountMostSevereTestStepResultStatus();

    // Then
    using namespace cucumber::messages;
    using Status = test_step_result_status;
    EXPECT_THAT(counts.at(Status::PASSED), testing::Gt(0));
    EXPECT_THAT(counts.at(Status::FAILED), testing::Gt(0));
    EXPECT_THAT(counts.at(Status::SKIPPED), testing::Gt(0));
    EXPECT_THAT(counts.at(Status::PENDING), testing::Gt(0));
    EXPECT_THAT(counts.at(Status::UNDEFINED), testing::Gt(0));
}
