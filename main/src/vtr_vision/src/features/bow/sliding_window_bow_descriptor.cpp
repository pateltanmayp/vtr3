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
 * \file incremental_bow_trainer.cpp
 * \brief Source file for the ASRL vision package
 * \details
 *
 * \author Autonomous Space Robotics Lab (ASRL)
 */
#include <vtr_vision/features/bow/sliding_window_bow_descriptor.hpp>

namespace vtr {
namespace vision {

template class SlidingWindowBOWDescriptor<unsigned>;

}
}  // namespace vtr