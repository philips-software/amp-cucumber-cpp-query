#pragma once

// Provides from_json implementations for cucumber::messages types, enabling
// nlohmann::json deserialization of NDJSON envelopes produced by Cucumber.
// The JSON field names follow the camelCase convention used by the messages library.

#include "cucumber/messages/attachment.hpp"
#include "cucumber/messages/attachment_content_encoding.hpp"
#include "cucumber/messages/background.hpp"
#include "cucumber/messages/ci.hpp"
#include "cucumber/messages/comment.hpp"
#include "cucumber/messages/data_table.hpp"
#include "cucumber/messages/doc_string.hpp"
#include "cucumber/messages/duration.hpp"
#include "cucumber/messages/envelope.hpp"
#include "cucumber/messages/examples.hpp"
#include "cucumber/messages/exception.hpp"
#include "cucumber/messages/feature.hpp"
#include "cucumber/messages/feature_child.hpp"
#include "cucumber/messages/gherkin_document.hpp"
#include "cucumber/messages/git.hpp"
#include "cucumber/messages/group.hpp"
#include "cucumber/messages/hook.hpp"
#include "cucumber/messages/hook_type.hpp"
#include "cucumber/messages/java_method.hpp"
#include "cucumber/messages/java_stack_trace_element.hpp"
#include "cucumber/messages/location.hpp"
#include "cucumber/messages/meta.hpp"
#include "cucumber/messages/pickle.hpp"
#include "cucumber/messages/pickle_doc_string.hpp"
#include "cucumber/messages/pickle_step.hpp"
#include "cucumber/messages/pickle_step_argument.hpp"
#include "cucumber/messages/pickle_step_type.hpp"
#include "cucumber/messages/pickle_table.hpp"
#include "cucumber/messages/pickle_table_cell.hpp"
#include "cucumber/messages/pickle_table_row.hpp"
#include "cucumber/messages/pickle_tag.hpp"
#include "cucumber/messages/product.hpp"
#include "cucumber/messages/rule.hpp"
#include "cucumber/messages/rule_child.hpp"
#include "cucumber/messages/scenario.hpp"
#include "cucumber/messages/snippet.hpp"
#include "cucumber/messages/source_reference.hpp"
#include "cucumber/messages/step.hpp"
#include "cucumber/messages/step_definition.hpp"
#include "cucumber/messages/step_definition_pattern.hpp"
#include "cucumber/messages/step_definition_pattern_type.hpp"
#include "cucumber/messages/step_keyword_type.hpp"
#include "cucumber/messages/step_match_argument.hpp"
#include "cucumber/messages/step_match_arguments_list.hpp"
#include "cucumber/messages/suggestion.hpp"
#include "cucumber/messages/table_cell.hpp"
#include "cucumber/messages/table_row.hpp"
#include "cucumber/messages/tag.hpp"
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
#include "nlohmann/json_fwd.hpp"
#include <cstddef>
#include <cucumber/messages/all.hpp>
#include <nlohmann/json.hpp>
#include <string>
#include <vector>

namespace cucumber::messages
{

    // ---- Enums -----------------------------------------------------------------

    inline void from_json(const nlohmann::json& j, test_step_result_status& v)
    {
        const auto s = j.get<std::string>();
        if (s == "PASSED")
            v = test_step_result_status::PASSED;
        else if (s == "SKIPPED")
            v = test_step_result_status::SKIPPED;
        else if (s == "PENDING")
            v = test_step_result_status::PENDING;
        else if (s == "UNDEFINED")
            v = test_step_result_status::UNDEFINED;
        else if (s == "AMBIGUOUS")
            v = test_step_result_status::AMBIGUOUS;
        else if (s == "FAILED")
            v = test_step_result_status::FAILED;
        else
            v = test_step_result_status::UNKNOWN;
    }

    inline void from_json(const nlohmann::json& j, attachment_content_encoding& v)
    {
        v = (j.get<std::string>() == "BASE64") ? attachment_content_encoding::BASE64
                                               : attachment_content_encoding::IDENTITY;
    }

    inline void from_json(const nlohmann::json& j, step_keyword_type& v)
    {
        const auto s = j.get<std::string>();
        if (s == "Context")
            v = step_keyword_type::CONTEXT;
        else if (s == "Action")
            v = step_keyword_type::ACTION;
        else if (s == "Outcome")
            v = step_keyword_type::OUTCOME;
        else if (s == "Conjunction")
            v = step_keyword_type::CONJUNCTION;
        else
            v = step_keyword_type::UNKNOWN;
    }

    inline void from_json(const nlohmann::json& j, pickle_step_type& v)
    {
        const auto s = j.get<std::string>();
        if (s == "Context")
            v = pickle_step_type::CONTEXT;
        else if (s == "Action")
            v = pickle_step_type::ACTION;
        else if (s == "Outcome")
            v = pickle_step_type::OUTCOME;
        else
            v = pickle_step_type::UNKNOWN;
    }

    inline void from_json(const nlohmann::json& j, hook_type& v)
    {
        const auto s = j.get<std::string>();
        if (s == "BEFORE_TEST_RUN")
            v = hook_type::BEFORE_TEST_RUN;
        else if (s == "AFTER_TEST_RUN")
            v = hook_type::AFTER_TEST_RUN;
        else if (s == "BEFORE_TEST_CASE")
            v = hook_type::BEFORE_TEST_CASE;
        else if (s == "AFTER_TEST_CASE")
            v = hook_type::AFTER_TEST_CASE;
        else if (s == "BEFORE_TEST_STEP")
            v = hook_type::BEFORE_TEST_STEP;
        else if (s == "AFTER_TEST_STEP")
            v = hook_type::AFTER_TEST_STEP;
        else
            throw nlohmann::json::other_error::create(501, "unknown hook_type: " + s, &j);
    }

    inline void from_json(const nlohmann::json& j, step_definition_pattern_type& v)
    {
        v = (j.get<std::string>() == "REGULAR_EXPRESSION")
                ? step_definition_pattern_type::REGULAR_EXPRESSION
                : step_definition_pattern_type::CUCUMBER_EXPRESSION;
    }

    // ---- Primitive structs -----------------------------------------------------

    inline void from_json(const nlohmann::json& j, location& v)
    {
        v.line = j.value("line", std::size_t{ 0 });
        if (j.contains("column"))
            v.column = j.at("column").get<std::size_t>();
    }

    inline void from_json(const nlohmann::json& j, timestamp& v)
    {
        v.seconds = j.value("seconds", std::size_t{ 0 });
        v.nanos = j.value("nanos", std::size_t{ 0 });
    }

    inline void from_json(const nlohmann::json& j, duration& v)
    {
        v.seconds = j.value("seconds", std::size_t{ 0 });
        v.nanos = j.value("nanos", std::size_t{ 0 });
    }

    inline void from_json(const nlohmann::json& j, exception& v)
    {
        v.type = j.value("type", std::string{});
        if (j.contains("message"))
            v.message = j.at("message").get<std::string>();
        if (j.contains("stackTrace"))
            v.stack_trace = j.at("stackTrace").get<std::string>();
    }

    inline void from_json(const nlohmann::json& j, test_step_result& v)
    {
        if (j.contains("duration"))
            v.duration = j.at("duration").get<duration>();
        if (j.contains("message"))
            v.message = j.at("message").get<std::string>();
        if (j.contains("status"))
            v.status = j.at("status").get<test_step_result_status>();
        if (j.contains("exception"))
            v.exception = j.at("exception").get<exception>();
    }

    // ---- Gherkin AST types -----------------------------------------------------

    inline void from_json(const nlohmann::json& j, tag& v)
    {
        if (j.contains("location"))
            v.location = j.at("location").get<location>();
        v.name = j.value("name", std::string{});
        v.id = j.value("id", std::string{});
    }

    inline void from_json(const nlohmann::json& j, table_cell& v)
    {
        if (j.contains("location"))
            v.location = j.at("location").get<location>();
        v.value = j.value("value", std::string{});
    }

    inline void from_json(const nlohmann::json& j, table_row& v)
    {
        if (j.contains("location"))
            v.location = j.at("location").get<location>();
        if (j.contains("cells"))
            v.cells = j.at("cells").get<std::vector<table_cell>>();
        v.id = j.value("id", std::string{});
    }

    inline void from_json(const nlohmann::json& j, doc_string& v)
    {
        if (j.contains("location"))
            v.location = j.at("location").get<location>();
        if (j.contains("mediaType"))
            v.media_type = j.at("mediaType").get<std::string>();
        v.content = j.value("content", std::string{});
        v.delimiter = j.value("delimiter", std::string{});
    }

    inline void from_json(const nlohmann::json& j, data_table& v)
    {
        if (j.contains("location"))
            v.location = j.at("location").get<location>();
        if (j.contains("rows"))
            v.rows = j.at("rows").get<std::vector<table_row>>();
    }

    inline void from_json(const nlohmann::json& j, step& v)
    {
        if (j.contains("location"))
            v.location = j.at("location").get<location>();
        v.keyword = j.value("keyword", std::string{});
        if (j.contains("keywordType"))
            v.keyword_type = j.at("keywordType").get<step_keyword_type>();
        v.text = j.value("text", std::string{});
        if (j.contains("docString"))
            v.doc_string = j.at("docString").get<doc_string>();
        if (j.contains("dataTable"))
            v.data_table = j.at("dataTable").get<data_table>();
        v.id = j.value("id", std::string{});
    }

    inline void from_json(const nlohmann::json& j, background& v)
    {
        if (j.contains("location"))
            v.location = j.at("location").get<location>();
        v.keyword = j.value("keyword", std::string{});
        v.name = j.value("name", std::string{});
        v.description = j.value("description", std::string{});
        if (j.contains("steps"))
            v.steps = j.at("steps").get<std::vector<step>>();
        v.id = j.value("id", std::string{});
    }

    inline void from_json(const nlohmann::json& j, examples& v)
    {
        if (j.contains("location"))
            v.location = j.at("location").get<location>();
        if (j.contains("tags"))
            v.tags = j.at("tags").get<std::vector<tag>>();
        v.keyword = j.value("keyword", std::string{});
        v.name = j.value("name", std::string{});
        v.description = j.value("description", std::string{});
        if (j.contains("tableHeader"))
            v.table_header = j.at("tableHeader").get<table_row>();
        if (j.contains("tableBody"))
            v.table_body = j.at("tableBody").get<std::vector<table_row>>();
        v.id = j.value("id", std::string{});
    }

    inline void from_json(const nlohmann::json& j, scenario& v)
    {
        if (j.contains("location"))
            v.location = j.at("location").get<location>();
        if (j.contains("tags"))
            v.tags = j.at("tags").get<std::vector<tag>>();
        v.keyword = j.value("keyword", std::string{});
        v.name = j.value("name", std::string{});
        v.description = j.value("description", std::string{});
        if (j.contains("steps"))
            v.steps = j.at("steps").get<std::vector<step>>();
        if (j.contains("examples"))
            v.examples = j.at("examples").get<std::vector<examples>>();
        v.id = j.value("id", std::string{});
    }

    inline void from_json(const nlohmann::json& j, rule_child& v)
    {
        if (j.contains("background"))
            v.background = j.at("background").get<background>();
        if (j.contains("scenario"))
            v.scenario = j.at("scenario").get<scenario>();
    }

    inline void from_json(const nlohmann::json& j, rule& v)
    {
        if (j.contains("location"))
            v.location = j.at("location").get<location>();
        if (j.contains("tags"))
            v.tags = j.at("tags").get<std::vector<tag>>();
        v.keyword = j.value("keyword", std::string{});
        v.name = j.value("name", std::string{});
        v.description = j.value("description", std::string{});
        if (j.contains("children"))
            v.children = j.at("children").get<std::vector<rule_child>>();
        v.id = j.value("id", std::string{});
    }

    inline void from_json(const nlohmann::json& j, feature_child& v)
    {
        if (j.contains("rule"))
            v.rule = j.at("rule").get<rule>();
        if (j.contains("background"))
            v.background = j.at("background").get<background>();
        if (j.contains("scenario"))
            v.scenario = j.at("scenario").get<scenario>();
    }

    inline void from_json(const nlohmann::json& j, comment& v)
    {
        if (j.contains("location"))
            v.location = j.at("location").get<location>();
        v.text = j.value("text", std::string{});
    }

    inline void from_json(const nlohmann::json& j, feature& v)
    {
        if (j.contains("location"))
            v.location = j.at("location").get<location>();
        if (j.contains("tags"))
            v.tags = j.at("tags").get<std::vector<tag>>();
        v.language = j.value("language", std::string{});
        v.keyword = j.value("keyword", std::string{});
        v.name = j.value("name", std::string{});
        v.description = j.value("description", std::string{});
        if (j.contains("children"))
            v.children = j.at("children").get<std::vector<feature_child>>();
    }

    inline void from_json(const nlohmann::json& j, gherkin_document& v)
    {
        if (j.contains("uri"))
            v.uri = j.at("uri").get<std::string>();
        if (j.contains("feature"))
            v.feature = j.at("feature").get<feature>();
        if (j.contains("comments"))
            v.comments = j.at("comments").get<std::vector<comment>>();
    }

    // ---- Pickle types ----------------------------------------------------------

    inline void from_json(const nlohmann::json& j, pickle_tag& v)
    {
        v.name = j.value("name", std::string{});
        v.ast_node_id = j.value("astNodeId", std::string{});
    }

    inline void from_json(const nlohmann::json& j, pickle_doc_string& v)
    {
        if (j.contains("mediaType"))
            v.media_type = j.at("mediaType").get<std::string>();
        v.content = j.value("content", std::string{});
    }

    inline void from_json(const nlohmann::json& j, pickle_table_cell& v)
    {
        v.value = j.value("value", std::string{});
    }

    inline void from_json(const nlohmann::json& j, pickle_table_row& v)
    {
        if (j.contains("cells"))
            v.cells = j.at("cells").get<std::vector<pickle_table_cell>>();
    }

    inline void from_json(const nlohmann::json& j, pickle_table& v)
    {
        if (j.contains("rows"))
            v.rows = j.at("rows").get<std::vector<pickle_table_row>>();
    }

    inline void from_json(const nlohmann::json& j, pickle_step_argument& v)
    {
        if (j.contains("docString"))
            v.doc_string = j.at("docString").get<pickle_doc_string>();
        if (j.contains("dataTable"))
            v.data_table = j.at("dataTable").get<pickle_table>();
    }

    inline void from_json(const nlohmann::json& j, pickle_step& v)
    {
        if (j.contains("argument"))
            v.argument = j.at("argument").get<pickle_step_argument>();
        if (j.contains("astNodeIds"))
            v.ast_node_ids = j.at("astNodeIds").get<std::vector<std::string>>();
        v.id = j.value("id", std::string{});
        if (j.contains("type"))
            v.type = j.at("type").get<pickle_step_type>();
        v.text = j.value("text", std::string{});
    }

    inline void from_json(const nlohmann::json& j, pickle& v)
    {
        v.id = j.value("id", std::string{});
        v.uri = j.value("uri", std::string{});
        if (j.contains("location"))
            v.location = j.at("location").get<location>();
        v.name = j.value("name", std::string{});
        v.language = j.value("language", std::string{});
        if (j.contains("steps"))
            v.steps = j.at("steps").get<std::vector<pickle_step>>();
        if (j.contains("tags"))
            v.tags = j.at("tags").get<std::vector<pickle_tag>>();
        if (j.contains("astNodeIds"))
            v.ast_node_ids = j.at("astNodeIds").get<std::vector<std::string>>();
    }

    // ---- Hook / StepDefinition -------------------------------------------------

    inline void from_json(const nlohmann::json& j, java_method& v)
    {
        v.class_name = j.value("className", std::string{});
        v.method_name = j.value("methodName", std::string{});
        if (j.contains("methodParameterTypes"))
            v.method_parameter_types = j.at("methodParameterTypes").get<std::vector<std::string>>();
    }

    inline void from_json(const nlohmann::json& j, java_stack_trace_element& v)
    {
        v.class_name = j.value("className", std::string{});
        v.file_name = j.value("fileName", std::string{});
        v.method_name = j.value("methodName", std::string{});
    }

    inline void from_json(const nlohmann::json& j, source_reference& v)
    {
        if (j.contains("uri"))
            v.uri = j.at("uri").get<std::string>();
        if (j.contains("javaMethod"))
            v.java_method = j.at("javaMethod").get<java_method>();
        if (j.contains("javaStackTraceElement"))
            v.java_stack_trace_element = j.at("javaStackTraceElement").get<java_stack_trace_element>();
        if (j.contains("location"))
            v.location = j.at("location").get<location>();
    }

    inline void from_json(const nlohmann::json& j, hook& v)
    {
        v.id = j.value("id", std::string{});
        if (j.contains("name"))
            v.name = j.at("name").get<std::string>();
        if (j.contains("sourceReference"))
            v.source_reference = j.at("sourceReference").get<source_reference>();
        if (j.contains("tagExpression"))
            v.tag_expression = j.at("tagExpression").get<std::string>();
        if (j.contains("type"))
            v.type = j.at("type").get<hook_type>();
    }

    inline void from_json(const nlohmann::json& j, step_definition_pattern& v)
    {
        if (j.contains("type"))
            v.type = j.at("type").get<step_definition_pattern_type>();
        v.source = j.value("source", std::string{});
    }

    inline void from_json(const nlohmann::json& j, step_definition& v)
    {
        v.id = j.value("id", std::string{});
        if (j.contains("pattern"))
            v.pattern = j.at("pattern").get<step_definition_pattern>();
        if (j.contains("sourceReference"))
            v.source_reference = j.at("sourceReference").get<source_reference>();
    }

    // ---- Meta ------------------------------------------------------------------

    inline void from_json(const nlohmann::json& j, product& v)
    {
        v.name = j.value("name", std::string{});
        if (j.contains("version"))
            v.version = j.at("version").get<std::string>();
    }

    inline void from_json(const nlohmann::json& j, git& v)
    {
        v.remote = j.value("remote", std::string{});
        v.revision = j.value("revision", std::string{});
        if (j.contains("branch"))
            v.branch = j.at("branch").get<std::string>();
        if (j.contains("tag"))
            v.tag = j.at("tag").get<std::string>();
    }

    inline void from_json(const nlohmann::json& j, ci& v)
    {
        v.name = j.value("name", std::string{});
        if (j.contains("url"))
            v.url = j.at("url").get<std::string>();
        if (j.contains("buildNumber"))
            v.build_number = j.at("buildNumber").get<std::string>();
        if (j.contains("git"))
            v.git = j.at("git").get<cucumber::messages::git>();
    }

    inline void from_json(const nlohmann::json& j, meta& v)
    {
        v.protocol_version = j.value("protocolVersion", std::string{});
        if (j.contains("implementation"))
            v.implementation = j.at("implementation").get<product>();
        if (j.contains("runtime"))
            v.runtime = j.at("runtime").get<product>();
        if (j.contains("os"))
            v.os = j.at("os").get<product>();
        if (j.contains("cpu"))
            v.cpu = j.at("cpu").get<product>();
        if (j.contains("ci"))
            v.ci = j.at("ci").get<cucumber::messages::ci>();
    }

    // ---- Test infrastructure types ---------------------------------------------

    inline void from_json(const nlohmann::json& j, group& v)
    {
        if (j.contains("children"))
            v.children = j.at("children").get<std::vector<group>>();
        if (j.contains("start"))
            v.start = j.at("start").get<std::size_t>();
        if (j.contains("value"))
            v.value = j.at("value").get<std::string>();
    }

    inline void from_json(const nlohmann::json& j, step_match_argument& v)
    {
        if (j.contains("group"))
            v.group = j.at("group").get<group>();
        if (j.contains("parameterTypeName"))
            v.parameter_type_name = j.at("parameterTypeName").get<std::string>();
    }

    inline void from_json(const nlohmann::json& j, step_match_arguments_list& v)
    {
        if (j.contains("stepMatchArguments"))
            v.step_match_arguments = j.at("stepMatchArguments").get<std::vector<step_match_argument>>();
    }

    inline void from_json(const nlohmann::json& j, test_step& v)
    {
        if (j.contains("hookId"))
            v.hook_id = j.at("hookId").get<std::string>();
        v.id = j.value("id", std::string{});
        if (j.contains("pickleStepId"))
            v.pickle_step_id = j.at("pickleStepId").get<std::string>();
        if (j.contains("stepDefinitionIds"))
            v.step_definition_ids = j.at("stepDefinitionIds").get<std::vector<std::string>>();
        if (j.contains("stepMatchArgumentsLists"))
            v.step_match_arguments_lists =
                j.at("stepMatchArgumentsLists").get<std::vector<step_match_arguments_list>>();
    }

    inline void from_json(const nlohmann::json& j, test_case& v)
    {
        v.id = j.value("id", std::string{});
        v.pickle_id = j.value("pickleId", std::string{});
        if (j.contains("testSteps"))
            v.test_steps = j.at("testSteps").get<std::vector<test_step>>();
        if (j.contains("testRunStartedId"))
            v.test_run_started_id = j.at("testRunStartedId").get<std::string>();
    }

    inline void from_json(const nlohmann::json& j, test_case_started& v)
    {
        v.attempt = j.value("attempt", std::size_t{ 0 });
        v.id = j.value("id", std::string{});
        v.test_case_id = j.value("testCaseId", std::string{});
        if (j.contains("workerId"))
            v.worker_id = j.at("workerId").get<std::string>();
        if (j.contains("timestamp"))
            v.timestamp = j.at("timestamp").get<timestamp>();
    }

    inline void from_json(const nlohmann::json& j, test_case_finished& v)
    {
        v.test_case_started_id = j.value("testCaseStartedId", std::string{});
        if (j.contains("timestamp"))
            v.timestamp = j.at("timestamp").get<timestamp>();
        v.will_be_retried = j.value("willBeRetried", false);
    }

    inline void from_json(const nlohmann::json& j, test_step_started& v)
    {
        v.test_case_started_id = j.value("testCaseStartedId", std::string{});
        v.test_step_id = j.value("testStepId", std::string{});
        if (j.contains("timestamp"))
            v.timestamp = j.at("timestamp").get<timestamp>();
    }

    inline void from_json(const nlohmann::json& j, test_step_finished& v)
    {
        v.test_case_started_id = j.value("testCaseStartedId", std::string{});
        v.test_step_id = j.value("testStepId", std::string{});
        if (j.contains("testStepResult"))
            v.test_step_result = j.at("testStepResult").get<test_step_result>();
        if (j.contains("timestamp"))
            v.timestamp = j.at("timestamp").get<timestamp>();
    }

    inline void from_json(const nlohmann::json& j, test_run_started& v)
    {
        if (j.contains("timestamp"))
            v.timestamp = j.at("timestamp").get<timestamp>();
        if (j.contains("id"))
            v.id = j.at("id").get<std::string>();
    }

    inline void from_json(const nlohmann::json& j, test_run_finished& v)
    {
        if (j.contains("message"))
            v.message = j.at("message").get<std::string>();
        v.success = j.value("success", false);
        if (j.contains("timestamp"))
            v.timestamp = j.at("timestamp").get<timestamp>();
        if (j.contains("exception"))
            v.exception = j.at("exception").get<exception>();
        if (j.contains("testRunStartedId"))
            v.test_run_started_id = j.at("testRunStartedId").get<std::string>();
    }

    inline void from_json(const nlohmann::json& j, test_run_hook_started& v)
    {
        v.id = j.value("id", std::string{});
        v.test_run_started_id = j.value("testRunStartedId", std::string{});
        v.hook_id = j.value("hookId", std::string{});
        if (j.contains("workerId"))
            v.worker_id = j.at("workerId").get<std::string>();
        if (j.contains("timestamp"))
            v.timestamp = j.at("timestamp").get<timestamp>();
    }

    inline void from_json(const nlohmann::json& j, test_run_hook_finished& v)
    {
        v.test_run_hook_started_id = j.value("testRunHookStartedId", std::string{});
        if (j.contains("result"))
            v.result = j.at("result").get<test_step_result>();
        if (j.contains("timestamp"))
            v.timestamp = j.at("timestamp").get<timestamp>();
    }

    // ---- Attachment ------------------------------------------------------------

    inline void from_json(const nlohmann::json& j, attachment& v)
    {
        v.body = j.value("body", std::string{});
        if (j.contains("contentEncoding"))
            v.content_encoding = j.at("contentEncoding").get<attachment_content_encoding>();
        if (j.contains("fileName"))
            v.file_name = j.at("fileName").get<std::string>();
        v.media_type = j.value("mediaType", std::string{});
        if (j.contains("testCaseStartedId"))
            v.test_case_started_id = j.at("testCaseStartedId").get<std::string>();
        if (j.contains("testStepId"))
            v.test_step_id = j.at("testStepId").get<std::string>();
        if (j.contains("url"))
            v.url = j.at("url").get<std::string>();
        if (j.contains("testRunStartedId"))
            v.test_run_started_id = j.at("testRunStartedId").get<std::string>();
        if (j.contains("testRunHookStartedId"))
            v.test_run_hook_started_id = j.at("testRunHookStartedId").get<std::string>();
        if (j.contains("timestamp"))
            v.timestamp = j.at("timestamp").get<timestamp>();
    }

    // ---- Suggestion / UndefinedParameterType -----------------------------------

    inline void from_json(const nlohmann::json& j, snippet& v)
    {
        v.language = j.value("language", std::string{});
        v.code = j.value("code", std::string{});
    }

    inline void from_json(const nlohmann::json& j, suggestion& v)
    {
        v.id = j.value("id", std::string{});
        v.pickle_step_id = j.value("pickleStepId", std::string{});
        if (j.contains("snippets"))
            v.snippets = j.at("snippets").get<std::vector<snippet>>();
    }

    inline void from_json(const nlohmann::json& j, undefined_parameter_type& v)
    {
        v.expression = j.value("expression", std::string{});
        v.name = j.value("name", std::string{});
    }

    // ---- Top-level envelope ----------------------------------------------------

    inline void from_json(const nlohmann::json& j, envelope& v)
    {
        if (j.contains("attachment"))
            v.attachment = j.at("attachment").get<attachment>();
        if (j.contains("gherkinDocument"))
            v.gherkin_document = j.at("gherkinDocument").get<gherkin_document>();
        if (j.contains("hook"))
            v.hook = j.at("hook").get<hook>();
        if (j.contains("meta"))
            v.meta = j.at("meta").get<meta>();
        if (j.contains("pickle"))
            v.pickle = j.at("pickle").get<pickle>();
        if (j.contains("suggestion"))
            v.suggestion = j.at("suggestion").get<suggestion>();
        if (j.contains("stepDefinition"))
            v.step_definition = j.at("stepDefinition").get<step_definition>();
        if (j.contains("testCase"))
            v.test_case = j.at("testCase").get<test_case>();
        if (j.contains("testCaseFinished"))
            v.test_case_finished = j.at("testCaseFinished").get<test_case_finished>();
        if (j.contains("testCaseStarted"))
            v.test_case_started = j.at("testCaseStarted").get<test_case_started>();
        if (j.contains("testRunFinished"))
            v.test_run_finished = j.at("testRunFinished").get<test_run_finished>();
        if (j.contains("testRunStarted"))
            v.test_run_started = j.at("testRunStarted").get<test_run_started>();
        if (j.contains("testStepFinished"))
            v.test_step_finished = j.at("testStepFinished").get<test_step_finished>();
        if (j.contains("testStepStarted"))
            v.test_step_started = j.at("testStepStarted").get<test_step_started>();
        if (j.contains("testRunHookStarted"))
            v.test_run_hook_started = j.at("testRunHookStarted").get<test_run_hook_started>();
        if (j.contains("testRunHookFinished"))
            v.test_run_hook_finished = j.at("testRunHookFinished").get<test_run_hook_finished>();
        if (j.contains("undefinedParameterType"))
            v.undefined_parameter_type = j.at("undefinedParameterType").get<undefined_parameter_type>();
    }

} // namespace cucumber::messages
