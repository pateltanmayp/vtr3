#include <filesystem>

#include "rclcpp/rclcpp.hpp"

#include <vtr_common/utils/filesystem.hpp>
#include <vtr_logging/logging_init.hpp>
#include <vtr_tactic/types.hpp>  // TemporalEvaluator

namespace fs = std::filesystem;

using namespace vtr;

int main(int argc, char** argv) {
  logging::configureLogging();

  fs::path data_dir{fs::current_path()};
  if (argc > 1)
    common::utils::expand_user(common::utils::expand_env(data_dir = argv[1]));

  auto graph = pose_graph::RCGraph::MakeShared(data_dir / "graph");

  LOG(INFO) << "Loaded pose graph has " << graph->numberOfRuns() << " runs and "
            << graph->numberOfVertices() << " vertices in total.";
  if (!graph->numberOfVertices()) return 0;

  /// Create a temporal evaluator
  tactic::TemporalEvaluator<tactic::Graph>::Ptr evaluator(
      new tactic::TemporalEvaluator<tactic::Graph>());
  evaluator->setGraph(graph.get());

  /// Iterate over all runs
  double total_length = 0;
  double teach_length = 0;
  double repeat_length = 0;
  const auto& runs = graph->runs()->locked().get();
  for (auto iter = runs.begin(); iter != runs.end(); iter++) {
    if (iter->second->numberOfVertices() == 0) continue;
    auto graph_run =
        graph->getSubgraph(tactic::VertexId(iter->first, 0), evaluator);
    tactic::VertexId::Vector sequence;
    for (auto it = graph_run->begin(tactic::VertexId(iter->first, 0));
         it != graph_run->end(); ++it) {
      // LOG(INFO) << it->v()->id();
      sequence.push_back(it->v()->id());
    }

    tactic::LocalizationChain chain(graph);
    chain.setSequence(sequence);
    chain.expand();
    LOG(INFO) << "Length of the run " << iter->first
              << " is: " << chain.length() << " m.";
    total_length += chain.length();
    if (iter->second->isManual())
      teach_length += chain.length();
    else
      repeat_length += chain.length();
  }
  LOG(INFO) << "Teach length of this graph is: " << teach_length << " m.";
  LOG(INFO) << "Repeat length of this graph is: " << repeat_length << " m.";
  LOG(INFO) << "Total length of this graph is: " << total_length << " m.";
}