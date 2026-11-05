#include "common/logger.hpp"
#include "common/token_verifier.hpp"
#include "jobs/ijob_executor.hpp"
#include "jobs/job_manager.hpp"
#include "net/protocol.hpp"
#include "net/request_dispatcher.hpp"
#include "test_services.hpp"

#include <gtest/gtest.h>
#include <nlohmann/json.hpp>

#include <sstream>
#include <string>

namespace {

class NoopExecutor : public gen::IJobExecutor {
 public:
  gen::JobResult execute(const gen::Job& job) override {
    return {job.type == gen::JobType::Image ? "image" : "video", "outputs/" + job.id + "/stub.txt"};
  }
};

class DispatcherFixture : public ::testing::Test {
 protected:
  DispatcherFixture()
      : services_(logger_),
        jobs_(8, std::make_unique<NoopExecutor>(), logger_, services_.store, services_.quota, services_.metrics),
        dispatcher_(jobs_, logger_, 64 * 1024, services_.metrics, verifier_) {}

  nlohmann::json line(std::string_view request) {
    return nlohmann::json::parse(dispatcher_.handleLine(request));
  }

  std::ostringstream log_;
  gen::Logger logger_{log_};
  TestServices services_;
  gen::TokenVerifier verifier_;
  gen::JobManager jobs_;
  gen::RequestDispatcher dispatcher_;
};

const char* kSection4Image =
    R"({"protocol_version":1,"request_id":"client-request-001","type":"image","prompt":"A tall woman with curly red hair wearing a blue coat, standing on a city street at sunset","options":{"width":1024,"height":1024,"seed":12345,"style":"stylized_3d"}})";

const char* kSection15Image =
    R"({"protocol_version":1,"request_id":"r-1","type":"image","prompt":"A short man with black hair wearing a yellow shirt, standing","options":{"width":512,"height":512,"seed":42,"style":"stylized_3d"}})";

const char* kVideoRequest =
    R"({"protocol_version":1,"request_id":"client-request-002","type":"video","prompt":"A young man in a green jacket walks through a park and waves at the camera","options":{"width":1024,"height":720,"fps":24,"duration_seconds":4,"seed":12345,"style":"stylized_3d","action":"walk_and_wave"}})";

void expectCode(std::string_view request, std::string_view code, std::string_view requestId = "") {
  gen::Protocol protocol;
  const auto parsed = protocol.parse(request);
  const auto* failure = std::get_if<gen::ProtocolFailure>(&parsed);
  ASSERT_NE(failure, nullptr) << request;
  const auto body = nlohmann::json::parse(protocol.errorResponse(failure->requestId, failure->error));
  EXPECT_EQ(body["protocol_version"], 1);
  EXPECT_EQ(body["ok"], false);
  EXPECT_EQ(body["request_id"], requestId);
  EXPECT_EQ(body["error"]["code"], code);
  EXPECT_FALSE(body.contains("job_id"));
  EXPECT_FALSE(body["error"]["message"].get<std::string>().empty());
}

}  // namespace

TEST_F(DispatcherFixture, AcceptsSection4ImageRequest) {
  const auto body = line(kSection4Image);
  EXPECT_EQ(body["ok"], true);
  EXPECT_EQ(body["status"], "queued");
  EXPECT_EQ(body["request_id"], "client-request-001");
  EXPECT_FALSE(body.contains("seed"));
  ASSERT_TRUE(body["job_id"].is_string());
  EXPECT_FALSE(body["job_id"].get<std::string>().empty());
}

TEST_F(DispatcherFixture, AcceptsSection15ImageRequest) {
  const auto body = line(kSection15Image);
  EXPECT_EQ(body["ok"], true);
  EXPECT_EQ(body["request_id"], "r-1");
  EXPECT_FALSE(body["job_id"].get<std::string>().empty());
}

TEST_F(DispatcherFixture, AcceptsVideoRequestInsideTheSizeLimit) {
  const auto body = line(kVideoRequest);
  EXPECT_EQ(body["ok"], true);
  EXPECT_EQ(body["status"], "queued");
  const auto status = line(
      R"({"protocol_version":1,"request_id":"status-1","type":"status","job_id":")" +
      body["job_id"].get<std::string>() + "\"}");
  EXPECT_EQ(status["ok"], true);
  EXPECT_EQ(status["request_id"], "status-1");
  EXPECT_EQ(status["status"], "queued");
  EXPECT_EQ(status["seed"], 12345);
  EXPECT_FALSE(status.contains("result"));
}

TEST_F(DispatcherFixture, RepeatedMessageCreatesDistinctJobs) {
  const auto first = line(kSection15Image);
  const auto second = line(kSection15Image);
  EXPECT_NE(first["job_id"], second["job_id"]);
}

TEST_F(DispatcherFixture, ReturnsGeneratedSeedOnStatus) {
  const auto accepted = line(
      R"({"protocol_version":1,"request_id":"seed-1","type":"image","prompt":"person","options":{"width":16,"height":16}})");
  const auto status = line(R"({"protocol_version":1,"request_id":"seed-status","type":"status","job_id":")" +
                           accepted["job_id"].get<std::string>() + "\"}");
  EXPECT_TRUE(status["seed"].is_number_integer());
  EXPECT_LE(status["seed"].get<std::uint64_t>(), 4294967295ULL);
}

TEST_F(DispatcherFixture, AcceptsMaximumFrameCount) {
  const auto body = line(
      R"({"protocol_version":1,"request_id":"frames","type":"video","prompt":"person","options":{"width":16,"height":16,"fps":30,"duration_seconds":8}})");
  EXPECT_EQ(body["ok"], true);
  EXPECT_EQ(body["status"], "queued");
}

TEST_F(DispatcherFixture, RejectsUnknownJob) {
  const auto body = line(R"({"protocol_version":1,"request_id":"missing","type":"status","job_id":"job-missing"})");
  EXPECT_EQ(body["ok"], false);
  EXPECT_EQ(body["error"]["code"], "JOB_NOT_FOUND");
  EXPECT_FALSE(body.contains("job_id"));
}

TEST_F(DispatcherFixture, NinthQueuedJobExceedsTheQueue) {
  for (int index = 0; index < 8; ++index) {
    const auto body = line(
        R"({"protocol_version":1,"request_id":"queue-)" + std::to_string(index) +
        R"(","type":"image","prompt":"person","options":{"width":16,"height":16}})");
    EXPECT_EQ(body["ok"], true) << index;
  }
  const auto rejected = line(
      R"({"protocol_version":1,"request_id":"queue-9","type":"image","prompt":"person","options":{"width":16,"height":16}})");
  EXPECT_EQ(rejected["ok"], false);
  EXPECT_EQ(rejected["error"]["code"], "RESOURCE_LIMIT_EXCEEDED");
  EXPECT_FALSE(rejected.contains("job_id"));
  EXPECT_EQ(jobs_.queuedCount(), 8U);
}

TEST(ProtocolTest, DefaultsStyleAndLeavesSeedEmpty) {
  gen::Protocol protocol;
  const auto parsed = protocol.parse(
      R"({"protocol_version":1,"request_id":"style","type":"image","prompt":"person","options":{"width":16,"height":16}})");
  const auto* image = std::get_if<gen::ImageRequest>(&parsed);
  ASSERT_NE(image, nullptr);
  EXPECT_EQ(image->style, "stylized_3d");
  EXPECT_FALSE(image->seed.has_value());
}

TEST(ProtocolTest, RejectsMalformedJsonWithoutRecoveringRequestId) {
  expectCode("{\"request_id\":", "MALFORMED_JSON");
  expectCode("[1,2,3]", "MALFORMED_JSON");
}

TEST(ProtocolTest, RejectsTheDocumentedFieldErrors) {
  expectCode(R"({"request_id":"r","type":"image"})", "MISSING_FIELD", "r");
  expectCode(R"({"protocol_version":"1","request_id":"r","type":"image"})", "INVALID_FIELD_VALUE", "r");
  expectCode(R"({"protocol_version":2,"request_id":"client-request-001","type":"image"})",
             "UNSUPPORTED_PROTOCOL_VERSION", "client-request-001");
  expectCode(R"({"protocol_version":1,"type":"image"})", "MISSING_FIELD");
  expectCode(R"({"protocol_version":1,"request_id":"","type":"image"})", "INVALID_FIELD_VALUE");
  expectCode(R"({"protocol_version":1,"request_id":")" + std::string(129, 'a') + R"(","type":"image"})",
             "INVALID_FIELD_VALUE", std::string(129, 'a'));
  expectCode(R"({"protocol_version":1,"request_id":"r"})", "MISSING_FIELD", "r");
  expectCode(R"({"protocol_version":1,"request_id":"r","type":1})", "INVALID_FIELD_VALUE", "r");
  expectCode(R"({"protocol_version":1,"request_id":"r","type":"audio"})", "INVALID_REQUEST_TYPE", "r");
  expectCode(
      R"({"protocol_version":1,"request_id":"r","type":"image","options":{"width":16,"height":16}})",
      "MISSING_FIELD", "r");
  expectCode(
      R"({"protocol_version":1,"request_id":"r","type":"image","prompt":"","options":{"width":16,"height":16}})",
      "INVALID_FIELD_VALUE", "r");
  expectCode(R"({"protocol_version":1,"request_id":"r","type":"image","prompt":")" + std::string(1025, 'b') +
                 R"(","options":{"width":16,"height":16}})",
             "INVALID_FIELD_VALUE", "r");
  expectCode(
      R"({"protocol_version":1,"request_id":"r","type":"image","prompt":"person","options":{"width":0,"height":16}})",
      "INVALID_FIELD_VALUE", "r");
  expectCode(
      R"({"protocol_version":1,"request_id":"r","type":"image","prompt":"person","options":{"width":"16","height":16}})",
      "INVALID_FIELD_VALUE", "r");
  expectCode(
      R"({"protocol_version":1,"request_id":"r","type":"image","prompt":"person","options":{"width":1280,"height":720}})",
      "INVALID_FIELD_VALUE", "r");
  expectCode(
      R"({"protocol_version":1,"request_id":"r","type":"image","prompt":"person","options":{"width":16,"height":16,"fps":24}})",
      "INVALID_FIELD_VALUE", "r");
  expectCode(
      R"({"protocol_version":1,"request_id":"r","type":"status","job_id":"job-1","prompt":"person"})",
      "INVALID_FIELD_VALUE", "r");
  expectCode(
      R"({"protocol_version":1,"request_id":"r","type":"image","prompt":"person","options":{"width":16,"height":16},"extra":true})",
      "INVALID_FIELD_VALUE", "r");
  expectCode(
      R"({"protocol_version":1,"request_id":"r","type":"image","prompt":"person","options":{"width":16,"height":16,"seed":-1}})",
      "INVALID_FIELD_VALUE", "r");
  expectCode(
      R"({"protocol_version":1,"request_id":"r","type":"image","prompt":"person","options":{"width":16,"height":16,"seed":4294967296}})",
      "INVALID_FIELD_VALUE", "r");
}

TEST(ProtocolTest, CountsPromptLengthInCodePoints) {
  std::string accents;
  accents.reserve(1024 * 2);
  for (int index = 0; index < 1024; ++index) {
    accents += "\xC3\xA9";
  }
  gen::Protocol protocol;
  const auto accepted = protocol.parse(
      R"({"protocol_version":1,"request_id":"utf8","type":"image","prompt":")" + accents +
      R"(","options":{"width":16,"height":16}})");
  EXPECT_TRUE(std::holds_alternative<gen::ImageRequest>(accepted));

  std::string tooLong = accents + "\xC3\xA9";
  const auto rejected = protocol.parse(
      R"({"protocol_version":1,"request_id":"utf8","type":"image","prompt":")" + tooLong +
      R"(","options":{"width":16,"height":16}})");
  const auto* failure = std::get_if<gen::ProtocolFailure>(&rejected);
  ASSERT_NE(failure, nullptr);
  EXPECT_EQ(failure->error.code, gen::ErrorCode::InvalidFieldValue);
}

TEST(ProtocolTest, AcceptsAuthTokenOnEveryRequestType) {
  gen::Protocol protocol;
  const auto image = protocol.parse(
      R"({"protocol_version":1,"request_id":"r","type":"image","auth_token":"secret","prompt":"person","options":{"width":16,"height":16}})");
  const auto* imageRequest = std::get_if<gen::ImageRequest>(&image);
  ASSERT_NE(imageRequest, nullptr);
  EXPECT_EQ(imageRequest->authToken, "secret");

  const auto video = protocol.parse(
      R"({"protocol_version":1,"request_id":"r","type":"video","auth_token":"secret","prompt":"person","options":{"width":16,"height":16,"fps":1,"duration_seconds":1}})");
  const auto* videoRequest = std::get_if<gen::VideoRequest>(&video);
  ASSERT_NE(videoRequest, nullptr);
  EXPECT_EQ(videoRequest->authToken, "secret");

  const auto status = protocol.parse(
      R"({"protocol_version":1,"request_id":"r","type":"status","auth_token":"secret","job_id":"job-1"})");
  const auto* statusRequest = std::get_if<gen::StatusRequest>(&status);
  ASSERT_NE(statusRequest, nullptr);
  EXPECT_EQ(statusRequest->authToken, "secret");

  expectCode(
      R"({"protocol_version":1,"request_id":"r","type":"image","auth_token":1,"prompt":"person","options":{"width":16,"height":16}})",
      "INVALID_FIELD_VALUE", "r");
}

TEST(ProtocolTest, MissingPromptMessageNamesTheField) {
  gen::Protocol protocol;
  const auto parsed = protocol.parse(
      R"({"protocol_version":1,"request_id":"client-request-001","type":"image","options":{"width":16,"height":16}})");
  const auto* failure = std::get_if<gen::ProtocolFailure>(&parsed);
  ASSERT_NE(failure, nullptr);
  EXPECT_EQ(failure->error.code, gen::ErrorCode::MissingField);
  EXPECT_EQ(failure->error.message, "Field 'prompt' is required.");
  const auto body = nlohmann::json::parse(protocol.errorResponse(failure->requestId, failure->error));
  EXPECT_EQ(body["request_id"], "client-request-001");
  EXPECT_EQ(body["error"]["message"], "Field 'prompt' is required.");
}
