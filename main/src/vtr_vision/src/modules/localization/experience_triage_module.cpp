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
 * \file experience_triage_module.cpp
 * \brief ExperienceTriageModule class method definitions
 *
 * \author Autonomous Space Robotics Lab (ASRL)
 */
#include <algorithm>

#include <vtr_messages/msg/localization_status.hpp>
#include <vtr_vision/modules/localization/experience_triage_module.hpp>

namespace std {
std::ostream &operator<<(std::ostream &os,
                         const vtr_messages::msg::ExpRecogStatus &msg) {
  std::ios fmt(nullptr);
  fmt.copyfmt(os);
  os << std::fixed;
  os << (msg.in_the_loop ? "[itl] " : "[off] ");
  os << "run: " << std::setw(5) << std::setprecision(1)
     << msg.computation_time_ms << " ms ";
  os << "load: " << std::setw(4) << std::setprecision(1) << msg.load_time_ms
     << " ms ";
  // If it has distances, this is preferred over straight up recommendations
  if (!msg.cosine_distances.empty()) {
    os << "nrec: " << msg.recommended_ids.size() << " ";
    std::ostringstream oss;
    oss << std::setprecision(3) << std::fixed;
    for (const auto &run_dist : msg.cosine_distances) {
      if (oss.str().size() >= 150) {
        oss << "...";
        break;
      }
      oss << std::setw(3) << run_dist.run_id << ": " << run_dist.cosine_distance
          << " ";
    }
    os << "dist: " << oss.str();
    // Some only have recommendations, not cosine distances
  } else if (!msg.recommended_ids.empty()) {
    os << "rec: ";
    for (const auto &rec : msg.recommended_ids)
      os << std::setw(3) << rec << " ";
  }

  os.copyfmt(fmt);
  return os;
}
}  // namespace std

namespace vtr {
namespace vision {

using namespace tactic;

RunIdSet getRunIds(const pose_graph::RCGraphBase &graph) {
  RunIdSet rids;
  // iterate through each vertex in the graph
  for (const auto &itr : graph)
    // insert the run id for this vertex in the set
    rids.insert(itr.v()->id().majorId());
  return rids;
}

RunIdSet privilegedRuns(const pose_graph::RCGraphBase &graph, RunIdSet rids) {
  RunIdSet prids;
  // go through each run in the run id set
  for (RunId rid : rids)
    // check to see if it was a manual (privileged) run
    if (graph.run(rid)->isManual())
      // if it was, insert it in the new privileged set
      prids.insert(rid);
  return prids;
}

pose_graph::RCGraphBase::Ptr maskSubgraph(
    const pose_graph::RCGraphBase::Ptr &graph, const RunIdSet &mask) {
  VertexId::Vector kept_vertex_ids;
  kept_vertex_ids.reserve(graph->numberOfVertices());
  // Check all the runs for inclusion in the new masked subgraph
  for (VertexId vid : graph->subgraph().getNodeIds())
    if (mask.count(vid.majorId())) kept_vertex_ids.push_back(vid);
  return graph->getSubgraph(kept_vertex_ids);
}

RunIdSet fillRecommends(RunIdSet *recommends, const ScoredRids &distance_rids,
                        unsigned n) {
  RunIdSet new_recs;
  // go through the scored runs, from highest to lowest
  for (const auto &scored_rid : distance_rids) {
    // if we are supplementing existing recommendations
    if (recommends) {
      // insert the new run until the recommendation is large enough
      if (recommends->size() >= n) break;
      recommends->insert(scored_rid.second);
    }
    // recored the newly recommended runs until ther are enough
    if (new_recs.size() >= n) break;
    new_recs.insert(scored_rid.second);
  }
  // return the newly recommended runs
  return new_recs;
}

void ExperienceTriageModule::configFromROS(const rclcpp::Node::SharedPtr &node,
                                           const std::string param_prefix) {
  config_ = std::make_shared<Config>();
  // clang-format off
  config_->verbose = node->declare_parameter<bool>(param_prefix + ".verbose", config_->verbose);
  config_->always_privileged = node->declare_parameter<bool>(param_prefix + ".always_privileged", config_->always_privileged);
  config_->only_privileged = node->declare_parameter<bool>(param_prefix + ".only_privileged", config_->only_privileged);
  config_->in_the_loop = node->declare_parameter<bool>(param_prefix + ".in_the_loop", config_->in_the_loop);
  // clang-format on
}

void ExperienceTriageModule::runImpl(QueryCache &qdata0,
                                     const Graph::ConstPtr &graph) {
  auto &qdata = dynamic_cast<CameraQueryCache &>(qdata0);

  // Grab what has been recommended so far by upstream recommenders
  if (!qdata.recommended_experiences) qdata.recommended_experiences.fallback();
  RunIdSet &recommended = *qdata.recommended_experiences;

  // Check if we need to do things...
  // --------------------------------
  
  pose_graph::RCGraphBase::Ptr &submap_ptr = *qdata.localization_map;
  if (config_->in_the_loop) {
    // If the mask is empty, we default to using all runs
    if (recommended.empty()) {
      if (config_->only_privileged) {
        recommended = privilegedRuns(*graph, getRunIds(*submap_ptr));

        // Apply the mask to the localization subgraph
        submap_ptr = maskSubgraph(submap_ptr, recommended);
        if (qdata.localization_status)
          qdata.localization_status->set__window_num_vertices(
              submap_ptr->numberOfVertices());
      
      } else {
        recommended = getRunIds(*submap_ptr);
      }
    } else {
      // If we always want to include the priveleged, make sure they're included
      if (config_->only_privileged) {
        recommended = privilegedRuns(*graph, getRunIds(*submap_ptr));
      } else if (config_->always_privileged) {
        RunIdSet priv_runs = privilegedRuns(*graph, getRunIds(*submap_ptr));
        recommended.insert(priv_runs.begin(), priv_runs.end());
      } 

      // Apply the mask to the localization subgraph
      submap_ptr = maskSubgraph(submap_ptr, recommended);
      if (qdata.localization_status)
        qdata.localization_status->set__window_num_vertices(
            submap_ptr->numberOfVertices());
    }
  }

  if ((recommended.size() > 1) || (*recommended.begin() != 0 )) { 
    LOG(ERROR) << "We are getting the wrong or more than one experience recommended!!";
    LOG(ERROR) << "Recommended experience: " << recommended;
  }

  // Build the status message we'll save out to the graph
  // ----------------------------------------------------

  // Basic status info
  Vertex::Ptr query_vertex = graph->at(*qdata.live_id);
  status_msg_ = vtr_messages::msg::ExpRecogStatus();
  status_msg_.set__in_the_loop(config_->in_the_loop);
  status_msg_.keyframe_time = query_vertex->keyFrameTime();
  status_msg_.set__query_id(query_vertex->id());

  // The recommended runs for localization
  for (const RunId &rid : recommended)
    status_msg_.recommended_ids.push_back(rid);

  LOG_IF(config_->verbose, INFO) << "ET: " << status_msg_;
}

void ExperienceTriageModule::updateGraphImpl(QueryCache &,
                                             const Graph::Ptr &graph,
                                             VertexId live_id) {
  // Save the status/results message
  // -------------------------------

  // Make sure the saved status message matches the live id
  if (status_msg_.query_id == live_id) {
    Vertex::Ptr vertex = graph->at(live_id);
    RunId rid = vertex->id().majorId();
    std::string results_stream = "experience_triage";
    graph->registerVertexStream<vtr_messages::msg::ExpRecogStatus>(
        rid, results_stream);
    vertex->insert(results_stream, status_msg_, vertex->keyFrameTime());
  }
}

}  // namespace vision
}  // namespace vtr