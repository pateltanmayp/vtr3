// Copyright 2021, Autonomous Space Robotics Lab (ASRL)
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

/**
 * \file keyframe_optimization_module.hpp
 * \brief KeyframeOptimizationModule class definition
 *
 * \author Autonomous Space Robotics Lab (ASRL)
 */
#pragma once

#include <lgmath.hpp>
#include <steam.hpp>

//#include <vtr_steam_extensions/evaluator/range_conditioning_eval.hpp>
#include <vtr_tactic/modules/base_module.hpp>
#include <vtr_vision/cache.hpp>
#include <vtr_vision/modules/optimization/steam_module.hpp>

namespace vtr {
namespace vision {

/**
 * \brief Reject outliers and estimate a preliminary transform
 * \details
 * requires:
 *   qdata.[rig_calibrations, rig_features, T_sensor_vehicle, steam_mutex,
 *          live_id, map_landmarks, T_sensor_vehicle_map, ransac_matches,
 *          *T_r_m_prior]
 * outputs:
 *   qdata.[trajectory, success, T_r_m]
 *
 * qdata.stamp used to compute trajectory
 * produces a trajectory estimate
 */
class KeyframeOptimizationModule : public SteamModule {
 public:
 PTR_TYPEDEFS(KeyframeOptimizationModule);
  /** \brief Static module identifier. */
  static constexpr auto static_name = "keyframe_optimization";

  /** \brief Collection of config parameters */
  struct Config : SteamModule::Config {
    PTR_TYPEDEFS(Config);

    bool depth_prior_enable = true;
    double depth_prior_weight = 100000000.0;
    bool pose_prior_enable = false;
    bool use_migrated_points = false;
    int min_inliers = 6;

    static ConstPtr fromROS(const rclcpp::Node::SharedPtr &node,
                            const std::string &param_prefix);
  };

  KeyframeOptimizationModule( const Config::ConstPtr &config,
      const std::shared_ptr<tactic::ModuleFactory> &module_factory = nullptr,
      const std::string &name = static_name) : SteamModule(config, module_factory, name), keyframe_config_(config) {}


 protected:

  /** \brief Given two frames, builds a sensor specific optimization problem. */
  steam::OptimizationProblem generateOptimizationProblem(
      CameraQueryCache &qdata, const tactic::Graph::ConstPtr &graph) override;

  void updateCaches(CameraQueryCache &qdata) override;

 private:
  /**
   * \brief Verifies the input data being used in the optimization problem,
   * namely, the inlier matches and initial estimate.
   * \param qdata The query data.
   */
  bool verifyInputData(CameraQueryCache &qdata) override;

  /**
   * \brief Verifies the output data generated by the optimization problem
   * \param qdata The query data.
   */
  bool verifyOutputData(CameraQueryCache &qdata) override;
#if false
  /** \brief samples and saves the trajectory results to disk. */
  void saveTrajectory(CameraQueryCache &qdata,
                      const std::shared_ptr<Graph> &graph, VertexId id);
#endif
  /**
   * \brief performs sanity checks on the landmark
   * \param point The landmark.
   * \return true if the landmark meets all checks, false otherwise.
   */
  bool isLandmarkValid(const Eigen::Vector3d &point);

  /**
   * \brief Initializes the problem based on an initial condition.
   * \param T_q_m The initial guess at the transformation between the query
   * frame and the map frame.
   */
  void resetProblem(tactic::EdgeTransform &T_q_m);

  /**
   * \brief Adds a depth cost associated with this landmark to the depth cost
   * terms.
   * \param landmark The landmark in question.
   */
  void addDepthCost(steam::stereo::HomoPointStateVar::Ptr landmark);

  /**
   * \brief Adds a steam trajectory for the state variables in the problem.
   * \param qdata The query data
   * \param graph The pose graph.
   */
  void computeTrajectory(CameraQueryCache &qdata,
                         const tactic::Graph::ConstPtr &graph,
                         steam::OptimizationProblem &problem);

  void addPosePrior(CameraQueryCache &qdata, steam::OptimizationProblem &problem);

  // /** \brief the cost terms associated with landmark observations. */
  // std::vector<steam::BaseCostTerm> cost_terms_;

  // /** \brief The cost terms associated with landmark depth. */
  // std::vector<steam::BaseCostTerm> depth_cost_terms_;

  /** \brief The loss function used for the depth cost. */
  steam::BaseLossFunc::Ptr sharedDepthLossFunc_;

  /** \brief the loss function assicated with observation cost. */
  steam::BaseLossFunc::Ptr sharedLossFunc_;

  /** \brief the locked map pose. */
  steam::se3::SE3StateVar::Ptr map_pose_;

  // /** \brief the unlocked query pose. */
  steam::se3::SE3StateVar::Ptr query_pose_;

  /** \brief Algorithm Configuration */
  Config::ConstPtr keyframe_config_;

  /**
   * \brief Maps velocity variable pointers to their respective vertices
   * \note a value of -1 is used for the live frame.
   */
  std::map<tactic::VertexId, steam::vspace::VSpaceStateVar<6>::Ptr> velocity_map_;

  VTR_REGISTER_MODULE_DEC_TYPE(KeyframeOptimizationModule);
};

}  // namespace vision
}  // namespace vtr
