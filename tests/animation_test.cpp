#include "animation/action_library.hpp"

#include <gtest/gtest.h>

namespace {

bool armDiffers(const gen::Pose& left, const gen::Pose& right) {
  return left.shoulderRight != right.shoulderRight || left.elbowRight != right.elbowRight;
}

}  // namespace

TEST(ActionLibraryTest, WalkKneesChangeAcrossTheStepAndRepeatAfterOneSecond) {
  const gen::ActionLibrary actions;
  const auto start = actions.poseAt(gen::ActionId::Walk, 0.0f);
  const auto mid = actions.poseAt(gen::ActionId::Walk, 0.5f);
  const auto next = actions.poseAt(gen::ActionId::Walk, 1.0f);

  EXPECT_NE(start.kneeLeft, mid.kneeLeft);
  EXPECT_NE(start.kneeRight, mid.kneeRight);
  EXPECT_FLOAT_EQ(start.kneeLeft, next.kneeLeft);
  EXPECT_FLOAT_EQ(start.kneeRight, next.kneeRight);
  EXPECT_FLOAT_EQ(start.hipLeft, next.hipLeft);
  EXPECT_FLOAT_EQ(start.hipRight, next.hipRight);
  EXPECT_FLOAT_EQ(start.shoulderLeft, next.shoulderLeft);
  EXPECT_FLOAT_EQ(start.elbowRight, next.elbowRight);
  EXPECT_LT(start.root.z, next.root.z);
}

TEST(ActionLibraryTest, WaveMovesTheRightArmAndLeavesTheRoot) {
  const gen::ActionLibrary actions;
  const auto start = actions.poseAt(gen::ActionId::Wave, 0.0f);
  const auto mid = actions.poseAt(gen::ActionId::Wave, 0.5f);
  const auto later = actions.poseAt(gen::ActionId::Wave, 2.5f);

  EXPECT_FLOAT_EQ(start.root.x, 0.0f);
  EXPECT_FLOAT_EQ(start.root.z, 0.0f);
  EXPECT_FLOAT_EQ(mid.root.z, 0.0f);
  EXPECT_FLOAT_EQ(later.root.z, 0.0f);
  EXPECT_TRUE(armDiffers(start, mid));
  EXPECT_FLOAT_EQ(start.hipLeft, 0.0f);
  EXPECT_FLOAT_EQ(mid.kneeLeft, 0.0f);
  EXPECT_FLOAT_EQ(mid.kneeRight, 0.0f);
}

TEST(ActionLibraryTest, WalkAndWaveCombinesBothChanges) {
  const gen::ActionLibrary actions;
  const auto walkStart = actions.poseAt(gen::ActionId::Walk, 0.0f);
  const auto walkMid = actions.poseAt(gen::ActionId::Walk, 0.5f);
  const auto waveStart = actions.poseAt(gen::ActionId::Wave, 0.0f);
  const auto waveMid = actions.poseAt(gen::ActionId::Wave, 0.5f);
  const auto bothStart = actions.poseAt(gen::ActionId::WalkAndWave, 0.0f);
  const auto bothMid = actions.poseAt(gen::ActionId::WalkAndWave, 0.5f);
  const auto bothNext = actions.poseAt(gen::ActionId::WalkAndWave, 1.0f);

  EXPECT_FLOAT_EQ(bothStart.kneeLeft, walkStart.kneeLeft);
  EXPECT_FLOAT_EQ(bothMid.kneeRight, walkMid.kneeRight);
  EXPECT_FLOAT_EQ(bothStart.root.z, walkStart.root.z);
  EXPECT_LT(bothStart.root.z, bothNext.root.z);
  EXPECT_FLOAT_EQ(bothStart.shoulderRight, waveStart.shoulderRight);
  EXPECT_FLOAT_EQ(bothMid.elbowRight, waveMid.elbowRight);
  EXPECT_TRUE(armDiffers(bothStart, bothMid));
}

TEST(ActionLibraryTest, TheSameTimeProducesTheSamePose) {
  const gen::ActionLibrary actions;
  EXPECT_EQ(actions.poseAt(gen::ActionId::WalkAndWave, 1.25f), actions.poseAt(gen::ActionId::WalkAndWave, 1.25f));
}

TEST(AnimationControllerTest, FrameTimeIsMonotonic) {
  const gen::AnimationController controller;
  EXPECT_FLOAT_EQ(controller.timeAt(0, 24), 0.0f);
  EXPECT_FLOAT_EQ(controller.timeAt(1, 24), 1.0f / 24.0f);
  EXPECT_FLOAT_EQ(controller.timeAt(24, 24), 1.0f);
  EXPECT_EQ(controller.poseAt(gen::ActionId::Walk, 12, 24), gen::ActionLibrary{}.poseAt(gen::ActionId::Walk, 0.5f));
}
