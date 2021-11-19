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
 * \file tactic.hpp
 * \brief Tactic class definition
 *
 * \author Yuchen Wu, Autonomous Space Robotics Lab (ASRL)
 */
#pragma once

#include "rclcpp/rclcpp.hpp"

/// for visualization in ROS
#include <tf2/convert.h>
#include <tf2_eigen/tf2_eigen.h>
#include <tf2_ros/static_transform_broadcaster.h>
#include <tf2_ros/transform_broadcaster.h>
#include <nav_msgs/msg/odometry.hpp>
#include <nav_msgs/msg/path.hpp>

#include <vtr_path_tracker/base.hpp>
#include <vtr_tactic/cache.hpp>
#include <vtr_tactic/pipelines/base_pipeline.hpp>
#include <vtr_tactic/publisher_interface.hpp>
#include <vtr_tactic/task_queues/async_task_queue.hpp>
#include <vtr_tactic/types.hpp>

using OdometryMsg = nav_msgs::msg::Odometry;
using ROSPathMsg = nav_msgs::msg::Path;
using PoseStampedMsg = geometry_msgs::msg::PoseStamped;

/// \todo define PathTracker::Ptr in Base
using PathTrackerPtr = std::shared_ptr<vtr::path_tracker::Base>;

namespace vtr {
namespace tactic {

/**
 * \brief Bounded buffer for the producer and consumer problem across
 * preprocessing, odometry&mapping and localization threads.
 * \details Data can be added as discardable and non-discardable. When the size
 * of the buffer is full, the oldest discardable data is removed when more data
 * are added. When no discardable data presents, push is blocked.
 */
template <typename T>
class QueryBuffer {
 public:
  using LockGuard = std::lock_guard<std::mutex>;
  using UniqueLock = std::unique_lock<std::mutex>;

  QueryBuffer(const size_t size = 1) : size_{size} {}

  bool push(const T& qdata, const bool discardable = true) {
    UniqueLock lock(mutex_);

    // a consistency check
    if (curr_size_ != (queries_.size() + nondiscardable_queries_.size()))
      throw std::runtime_error("QueryBuffer: inconsistent size");
    if (curr_size_ > size_)
      throw std::runtime_error("QueryBuffer: size exceeded");

    bool discarded = false;

    // move all non-discardable data to the nondiscardable queue
    while ((!queries_.empty()) && queries_.front().second == false) {
      nondiscardable_queries_.push(queries_.front().first);
      queries_.pop();
    }

    // see if we can add this in by discarding the oldest discardable data
    if (curr_size_ == size_) {
      // we have no space left for discardable data
      if (nondiscardable_queries_.size() == size_) {
        if (discardable) {
          discarded = true;
        } else {
          while (curr_size_ == size_) cv_not_full_.wait(lock);
          queries_.push(std::make_pair(qdata, discardable));
          curr_size_++;
        }
      }
      // we can discard the oldest discardable data
      else {
        queries_.push(std::make_pair(qdata, discardable));
        queries_.pop();
        discarded = true;
      }
    }
    // add directly since the buffer is not full
    else {
      queries_.push(std::make_pair(qdata, discardable));
      curr_size_++;
    }
    cv_not_empty_.notify_one();
    cv_size_changed_.notify_all();
    return discarded;
  }

  T pop() {
    UniqueLock lock(mutex_);
    while (curr_size_ == 0) cv_not_empty_.wait(lock);
    // if there are nondiscardable queries, pop from nondiscardable queries
    auto query = [&]() {
      if (!nondiscardable_queries_.empty()) {
        auto query = nondiscardable_queries_.front();
        nondiscardable_queries_.pop();
        return query;
      } else {
        auto query = queries_.front().first;
        queries_.pop();
        return query;
      }
    }();
    --curr_size_;
    cv_not_full_.notify_one();
    cv_size_changed_.notify_all();
    return query;
  }

  void wait(const size_t size = 0) {
    UniqueLock lock(mutex_);
    while (curr_size_ != size) cv_size_changed_.wait(lock);
  }

 private:
  /** \brief Buffer maximum size */
  const size_t size_;

  /** \brief Protects the queries_ queue */
  std::mutex mutex_;
  /** \brief Condition to wait when queue is full */
  std::condition_variable cv_not_full_;
  /** \brief Condition to wait when queue is empty */
  std::condition_variable cv_not_empty_;
  /** \brief Condition to wait until the size of buffer changes */
  std::condition_variable cv_size_changed_;

  /** \brief Current queue size */
  size_t curr_size_ = 0;
  /** \brief Queue of discardable + nondiscardable queries */
  std::queue<std::pair<T, bool>> queries_;
  /** \brief Queue of nondiscardable queries */
  std::queue<T> nondiscardable_queries_;
};

/**
 * \brief State estimation pipeline execution model. Can inherit from this class
 * for testing and debugging concurrency problems.
 */
class PipelineInterface {
 public:
  using PipelineMutex = std::recursive_timed_mutex;
  using PipelineLock = std::unique_lock<PipelineMutex>;

  PipelineInterface(const Graph::Ptr& graph, const size_t& num_async_threads,
                    const size_t& async_queue_size);

  /** \brief Subclass must call join due to inheritance. */
  virtual ~PipelineInterface() { join(); }

  void join();

  /**
   * \brief Stops passing query data into the pipeline immediately and then
   * waits until all pipeline threads finishes.
   */
  PipelineLock lockPipeline();

  /** \brief Changes the pipeine behavior based on current operation mode */
  void setPipeline(const PipelineMode& pipeline_mode);

  /** \brief Pipline entrypoint, gets query input from navigator */
  void input(const QueryCache::Ptr& qdata);

 private:
  /** \brief Data preprocessing thread, input->preprocess->odo&mapping */
  void preprocess();
  /** \brief Odometry & mapping thread, preprocess->odo&mapping->localization */
  void runOdometryMapping();
  /** \brief Localization thread, odomtry&mapping->localization */
  void runLocalization();

  /** \brief Performs the actual preprocessing task */
  virtual bool preprocess_(const QueryCache::Ptr& qdata) = 0;
  /** \brief Performs the actual odometry mapping task */
  virtual bool runOdometryMapping_(const QueryCache::Ptr& qdata) = 0;
  /** \brief Performs the actual localization task */
  virtual bool runLocalization_(const QueryCache::Ptr& qdata) = 0;

 private:
  PipelineMutex pipeline_mutex_;
  common::joinable_semaphore pipeline_semaphore_{0};
  PipelineMode pipeline_mode_;

  QueryBuffer<QueryCache::Ptr> preprocessing_buffer_{1};
  QueryBuffer<QueryCache::Ptr> odometry_mapping_buffer_{1};
  QueryBuffer<QueryCache::Ptr> localization_buffer_{1};

  std::thread preprocessing_thread_;
  std::thread odometry_mapping_thread_;
  std::thread localization_thread_;

  AsyncTaskExecutor::Ptr task_queue_;
};

}  // namespace tactic
}  // namespace vtr