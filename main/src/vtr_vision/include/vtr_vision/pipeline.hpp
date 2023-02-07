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
 * \file template_pipeline.hpp
 * \author Yuchen Wu, Autonomous Space Robotics Lab (ASRL)
 */
#pragma once

#include "vtr_tactic/pipelines/base_pipeline.hpp"

namespace vtr {
namespace tactic {

class TemplatePipeline : public BasePipeline {
 public:
  using Ptr = std::shared_ptr<TemplatePipeline>;

  /** \brief Static pipeline identifier. */
  static constexpr auto static_name = "template";

  /** \brief Collection of config parameters */
  struct Config : public BasePipeline::Config {
    using Ptr = std::shared_ptr<Config>;
    using ConstPtr = std::shared_ptr<const Config>;

    std::string parameter = "default value";

    static ConstPtr fromROS(const rclcpp::Node::SharedPtr &node,
                            const std::string &param_prefix) {
      /// Get ROS parameters here
      auto config = std::make_shared<Config>();
      // clang-format off
      config->parameter = node->declare_parameter<std::string>(param_prefix + ".parameter", config->parameter);
      // clang-format on
      CLOG(INFO, "tactic.pipeline")
          << "Template pipeline parameter set to: " << config->parameter;
      return config;
    }
  };

  TemplatePipeline(
      const Config::ConstPtr &config,
      const std::shared_ptr<ModuleFactory> &module_factory = nullptr,
      const std::string &name = static_name)
      : BasePipeline{module_factory, name}, config_(config) {}

  virtual ~TemplatePipeline() = default;

  void initialize_(const OutputCache::Ptr &, const Graph::Ptr &) override {
    /// Perform necessary initialization of the pipeline, e.g., create and
    /// initialize modules.
    /// Pose-graph is given but may be an empty graph.
  }

  void preprocess_(const QueryCache::Ptr &, const OutputCache::Ptr &,
                   const Graph::Ptr &,
                   const std::shared_ptr<TaskExecutor> &) override {
    /// This method is called on every input data.
    /// The following will be in qdata:
    ///   - input data (raw)
    ///   - stamp: time stamp of this data.
    ///   - node: a shared pointer to ROS node that can be used to create
    ///   publishers for visualization
    /// Any processed data (e.g. features) should be put in qdata.
    /// This method should not touch the pose graph.
    /// Any data preprocessing module should not touch the pose graph.
  }

  void runOdometry_(const QueryCache::Ptr &, const OutputCache::Ptr &,
                    const Graph::Ptr &,
                    const std::shared_ptr<TaskExecutor> &) override {
    /// This method is called on every preprocessed input data.
    /// The following will be in qdata:
    ///   - everything from preprocessing.
    ///   - first_frame: the first data received for the current run (teach or
    ///   repeat).
    ///   - live_id: the current vertex being localized against for odometry, or
    ///   invalid if first frame.
    ///   - T_r_v_odo: odometry estimation from last input, or identity if this
    ///   is the first frame or a vertex has just been created.
    ///   - odo_success: whether or not odometry estimation is successful
    ///   - vertex_test_result: whether or not to create a new vertex,
    ///   always default to NO.
    /// This method should update the following:
    ///   - T_r_v_odo, odo_success, vertex_test_result
    /// This method should only read from the graph.
    /// Any debug info, extra stuff can be put in qdata.
  }

  void runLocalization_(const QueryCache::Ptr &, const OutputCache::Ptr &,
                        const Graph::Ptr &,
                        const std::shared_ptr<TaskExecutor> &) override {
    /// This method is called in the following cases:
    ///   - first vertex of a teach that branches from existing path to
    ///   localize against the existing path (i.e., trunk)
    ///   - every frame when merging into existing graph to create a loop
    ///   - every frame when doing metric localization (e.g. before path
    ///   following)
    ///   - every vertex when repeating a path
    /// The following will be in qdata:
    ///   - everything from odometry and onVertexCreation.
    ///   - map_id: the vertex to be localized against by this method.
    ///   - T_r_v_loc: prior estimate from localization chain based on odometry.
    ///   - loc_success: whether or not localization is successful.
    /// This method should update the following:
    ///   - T_r_v_loc, loc_success
    /// This method may read from or write to the graph.
  }

  void onVertexCreation_(const QueryCache::Ptr &, const OutputCache::Ptr &,
                         const Graph::Ptr &,
                         const std::shared_ptr<TaskExecutor> &) override {
    /// This method is called whenever a vertex is created.
    /// The following will be in qdata:
    ///   - everything from odometry
    ///   - vid_odo: always the vertex corresponding to the just-created vertex.
    /// This method may read from or write to the graph.
  }

 private:
  /** \brief Pipeline configuration */
  Config::ConstPtr config_;

  VTR_REGISTER_PIPELINE_DEC_TYPE(TemplatePipeline);
};

}  // namespace tactic
}  // namespace vtr
