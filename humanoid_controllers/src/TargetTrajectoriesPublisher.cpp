//
// Created by qiayuan on 2022/7/24.
//

#include "humanoid_controllers/TargetTrajectoriesPublisher.h"

#include <ocs2_core/Types.h>
#include <ocs2_core/misc/LoadData.h>
#include <ocs2_robotic_tools/common/RotationTransforms.h>

using namespace ocs2;
using namespace humanoid;

namespace {
scalar_t TARGET_DISPLACEMENT_VELOCITY;
scalar_t TARGET_ROTATION_VELOCITY;
scalar_t COM_HEIGHT;
vector_t DEFAULT_JOINT_STATE(12);
scalar_t TIME_TO_TARGET;
}  // namespace

scalar_t estimateTimeToTarget(const vector_t& desiredBaseDisplacement) {
  const scalar_t& dx = desiredBaseDisplacement(0);
  const scalar_t& dy = desiredBaseDisplacement(1);
  const scalar_t& dyaw = desiredBaseDisplacement(3);
  const scalar_t rotationTime = std::abs(dyaw) / TARGET_ROTATION_VELOCITY;
  const scalar_t displacement = std::sqrt(dx * dx + dy * dy);
  const scalar_t displacementTime = displacement / TARGET_DISPLACEMENT_VELOCITY;
  return std::max(rotationTime, displacementTime);
}

TargetTrajectories targetPoseToTargetTrajectories(const vector_t& targetPose, const SystemObservation& observation,
                                                  const scalar_t& targetReachingTime) {
  // desired time trajectory
  const scalar_array_t timeTrajectory{observation.time, targetReachingTime};

  // desired state trajectory
  vector_t currentPose = observation.state.segment<6>(6);
  currentPose(2) = COM_HEIGHT;
  currentPose(4) = 0;
  currentPose(5) = 0;
  vector_array_t stateTrajectory(2, vector_t::Zero(observation.state.size()));
  stateTrajectory[0] << vector_t::Zero(6), currentPose, DEFAULT_JOINT_STATE;
  stateTrajectory[1] << vector_t::Zero(6), targetPose, DEFAULT_JOINT_STATE;

  // desired input trajectory (just right dimensions, they are not used)
  const vector_array_t inputTrajectory(2, vector_t::Zero(observation.input.size()));

  return {timeTrajectory, stateTrajectory, inputTrajectory};
}

TargetTrajectories goalToTargetTrajectories(const vector_t& goal, const SystemObservation& observation) {
  const vector_t currentPose = observation.state.segment<6>(6);
  const vector_t targetPose = [&]() {
    vector_t target(6);
    target(0) = goal(0);
    target(1) = goal(1);
    target(2) = COM_HEIGHT;
    target(3) = goal(3);
    target(4) = 0;
    target(5) = 0;
    return target;
  }();
  const scalar_t targetReachingTime = observation.time + estimateTimeToTarget(targetPose - currentPose);
  return targetPoseToTargetTrajectories(targetPose, observation, targetReachingTime);
}

TargetTrajectories cmdVelToTargetTrajectories(const vector_t& cmdVel, const SystemObservation& observation) {
  static float targetX = 0.0;
  static float targetY = 0.0;
  static float targetYaw = 0.0;

  const vector_t currentPose = observation.state.segment<6>(6);
  const Eigen::Matrix<scalar_t, 3, 1> zyx = currentPose.tail(3);
  vector_t cmdVelRot = getRotationMatrixFromZyxEulerAngles(zyx) * cmdVel.head(3);

  const scalar_t timeToTarget = TIME_TO_TARGET;
  targetX += cmdVelRot(0) * 0.01;
  targetY += cmdVelRot(1) * 0.01;
  targetYaw += cmdVel(3) * 0.01;
  const vector_t targetPose = [&]() {
    vector_t target(6);
    target(0) = targetX;
    target(1) = targetY;
    target(2) = COM_HEIGHT;
    target(3) = targetYaw;
    target(4) = 0;
    target(5) = 0;
    return target;
  }();

  // target reaching duration
  const scalar_t targetReachingTime = observation.time + timeToTarget;
  auto trajectories = targetPoseToTargetTrajectories(targetPose, observation, targetReachingTime);
  trajectories.stateTrajectory[0].head(3) = cmdVelRot;
  trajectories.stateTrajectory[1].head(3) = cmdVelRot;
  return trajectories;
}

int main(int argc, char** argv) {
  const std::string robotName = "humanoid";

  // Initialize ros node
  ::ros::init(argc, argv, robotName + "_target");
  ::ros::NodeHandle nodeHandle;
  // Get node parameters
  std::string referenceFile;
  std::string taskFile;
  nodeHandle.getParam("/referenceFile", referenceFile);
  nodeHandle.getParam("/taskFile", taskFile);

  loadData::loadCppDataType(referenceFile, "comHeight", COM_HEIGHT);
  loadData::loadEigenMatrix(referenceFile, "defaultJointState", DEFAULT_JOINT_STATE);
  loadData::loadCppDataType(referenceFile, "targetRotationVelocity", TARGET_ROTATION_VELOCITY);
  loadData::loadCppDataType(referenceFile, "targetDisplacementVelocity", TARGET_DISPLACEMENT_VELOCITY);
  loadData::loadCppDataType(taskFile, "mpc.timeHorizon", TIME_TO_TARGET);

  TargetTrajectoriesPublisher target_pose_command(nodeHandle, robotName, &goalToTargetTrajectories, &cmdVelToTargetTrajectories);

  ros::spin();
  // Successful exit
  return 0;
}
