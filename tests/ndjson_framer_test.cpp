#include "net/ndjson_framer.hpp"

#include <gtest/gtest.h>

#include <string>

namespace {

std::string bytes(std::size_t count, char character = 'a') { return std::string(count, character); }

}  // namespace

TEST(NdjsonFramerTest, SplitsALineAcrossAppends) {
  gen::NdjsonFramer framer(64 * 1024);
  EXPECT_TRUE(framer.append("{\"a\":").empty());
  const auto frames = framer.append("1}\n");
  ASSERT_EQ(frames.size(), 1U);
  EXPECT_EQ(frames[0].kind, gen::FrameKind::Message);
  EXPECT_EQ(frames[0].payload, "{\"a\":1}");
}

TEST(NdjsonFramerTest, ReadsTwoObjectsFromOneBuffer) {
  gen::NdjsonFramer framer(64 * 1024);
  const auto frames = framer.append("{\"a\":1}\n{\"b\":2}\n");
  ASSERT_EQ(frames.size(), 2U);
  EXPECT_EQ(frames[0].payload, "{\"a\":1}");
  EXPECT_EQ(frames[1].payload, "{\"b\":2}");
}

TEST(NdjsonFramerTest, KeepsAFullMessageUntilNewline) {
  gen::NdjsonFramer framer(64 * 1024);
  EXPECT_TRUE(framer.append(bytes(64 * 1024)).empty());
}

TEST(NdjsonFramerTest, AcceptsExactlySixtyFourKibibytes) {
  gen::NdjsonFramer framer(64 * 1024);
  const auto payload = bytes(64 * 1024);
  const auto frames = framer.append(payload + "\n");
  ASSERT_EQ(frames.size(), 1U);
  EXPECT_EQ(frames[0].kind, gen::FrameKind::Message);
  EXPECT_EQ(frames[0].payload, payload);
}

TEST(NdjsonFramerTest, RejectsOneByteOverTheLimit) {
  gen::NdjsonFramer framer(64 * 1024);
  const auto frames = framer.append(bytes(64 * 1024 + 1));
  ASSERT_EQ(frames.size(), 1U);
  EXPECT_EQ(frames[0].kind, gen::FrameKind::TooLarge);
  EXPECT_TRUE(frames[0].payload.empty());
}

TEST(NdjsonFramerTest, DiscardsTheOversizedTailAndReadsTheNextLine) {
  gen::NdjsonFramer framer(64 * 1024);
  const auto frames = framer.append(bytes(64 * 1024 + 1) + "\n{\"ok\":1}\n");
  ASSERT_EQ(frames.size(), 2U);
  EXPECT_EQ(frames[0].kind, gen::FrameKind::TooLarge);
  EXPECT_EQ(frames[1].kind, gen::FrameKind::Message);
  EXPECT_EQ(frames[1].payload, "{\"ok\":1}");
}
