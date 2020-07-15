#include <gtest/gtest.h>
#include <ros/ros.h>

#include <vtr/planning/state_machine.h>

#include <asrl/common/logging.hpp>
// ** FOLLOWING LINE SHOULD BE USED ONCE AND ONLY ONCE IN WHOLE APPLICATION **
// ** THE BEST PLACE TO PUT THIS LINE IS IN main.cpp RIGHT AFTER INCLUDING
// easylogging++.h **
INITIALIZE_EASYLOGGINGPP

#include <test_utils.h>

using namespace vtr::planning;
using namespace vtr::path_planning;
using state::Action;
using state::BaseState;
using state::Event;
using state::Signal;
using state::StateMachine;

/** Test callback to ensure that the state machine makes the correct callbacks
 * to the mission planning server.
 */
class TestCallbacks : public StateMachineCallbacks {
 public:
  PTR_TYPEDEFS(TestCallbacks)

  void stateChanged(const __shared_ptr<state::BaseState>&) {}
  void stateSuccess() {}
  void stateAbort(const std::string&) {}
  void stateUpdate(double) {}
};

/** Convenience class to create one of every State. */
struct StateContainer {
  StateContainer()
      : idle(new state::Idle()),
        repeat_topo_loc(new state::repeat::TopologicalLocalize()),
        plan(new state::repeat::Plan()),
        metric_loc(new state::repeat::MetricLocalize()),
        follow(new state::repeat::Follow()),
        teach_topo_loc(new state::teach::TopologicalLocalize()),
        branch(new state::teach::Branch()),
        merge(new state::teach::Merge()) {}

  StateContainer(const state::BaseState& sm)
      : idle(new state::Idle(sm)),
        repeat_topo_loc(new state::repeat::TopologicalLocalize(sm)),
        plan(new state::repeat::Plan(sm)),
        metric_loc(new state::repeat::MetricLocalize(sm)),
        follow(new state::repeat::Follow(sm)),
        teach_topo_loc(new state::teach::TopologicalLocalize(sm)),
        branch(new state::teach::Branch(sm)),
        merge(new state::teach::Merge(sm)) {}

  BaseState::Ptr idle;

  BaseState::Ptr repeat_topo_loc;
  BaseState::Ptr plan;
  BaseState::Ptr metric_loc;
  BaseState::Ptr follow;

  BaseState::Ptr teach_topo_loc;
  BaseState::Ptr branch;
  BaseState::Ptr merge;
};

/** Test transition from A state to B state. */
TEST(StateTransition, idle) {
  StateContainer p;
  EXPECT_EQ(p.idle.get()->nextStep(p.idle.get()), nullptr);
}

/** Ensure the state machine can handle all events properly. */
TEST(EventHandling, eventHandling) {
  StateMachine::Ptr state_machine = StateMachine::InitialState();

  TestCallbacks::Ptr callbacks(new TestCallbacks());
  state_machine->setCallbacks(callbacks.get());
  TestTactic::Ptr tactic(new TestTactic());
  state_machine->setTactic(tactic.get());
  state_machine->setPlanner(TestPathPlanner::Ptr(new TestPathPlanner()));

  // Start in idle
  EXPECT_EQ(state_machine->name(), "::Idle");
  EXPECT_EQ(state_machine->goals().size(), 1);

  // Handle idle -> idle: nothing should have changed
  state_machine->handleEvents(Event::StartIdle());
  EXPECT_EQ(state_machine->name(), "::Idle");
  EXPECT_EQ(state_machine->goals().size(), 1);

  // Handle pause from idle:
  //   Goal size is increased with another idle in goal stack.
  //     \todo Confirm that this is the intended result.
  state_machine->handleEvents(Event::Pause());
  EXPECT_EQ(state_machine->name(), "::Idle");
  EXPECT_EQ(state_machine->goals().size(), 2);

  // Handle idle -> teach::branch:
  //   Goes into topological localization state first (entry state of teach)
  //   Trigger stateChanged callback saying it's in topological localization
  //   Call tactic to LockPipeline
  //   Perform idle onExit, topological localization setPipeline and onEntry
  //     Call tactic to addRun \todo there is a ephermeral flag seems never used
  //   Trigger stateChanged callback saying it's in branch
  //   Perform topological localization onExit, teach setPipeline and onEntry
  //   Pipeline unlocked (out scope)
  state_machine->handleEvents(Event::StartTeach());
  EXPECT_EQ(state_machine->name(), "::Teach::Branch");
  EXPECT_EQ(state_machine->goals().size(), 1);

  // Handle teach::branch -> teach::merge:
  //   Trigger stateChanged callback saying it's in merge (change directly)
  //   Call tactic to LockPipeline
  //   Perform branch onExit, merge setPipeline and onEntry
  //      Call tactic to setPath, setting merge target
  //      Reset cancelled_ to false (cancelled_ says merge is cancelled/failed)
  //   Pipeline unlocked (out scope)
  // \todo the second argument is necessary?
  state_machine->handleEvents(
      Event::StartMerge(std::vector<VertexId>{{1, 50}, {1, 300}}, {1, 50}));
  EXPECT_EQ(state_machine->name(), "::Teach::Merge");
  EXPECT_EQ(state_machine->goals().size(), 1);

  // Handle signal AttemptClosure in merge without successful localization:
  //   AttemptClosure failed so fall back to ContinueTeach via swap goal
  //   Trigger stateChanged callback saying it's in branch (change directly)
  //   Perform merge onExit, branch setPipeline and onEntry
  //   Pipeline unlocked (out scope)
  state_machine->handleEvents(Event(Signal::AttemptClosure));
  EXPECT_EQ(state_machine->name(), "::Teach::Branch");
  EXPECT_EQ(state_machine->goals().size(), 1);

  // \todo Need tests for AttemptClusure in merge with successful localization

  // Handle end goal event in teach:
  //   triggerSuccess
  //   Trigger stateChanged callback saying it's in idle
  //   Call tactic to LockPipeline
  //   Perform branch onExit, idle setPipeline and onEntry
  //     call tactic to lockPipeline, relaxGraph and saveGraph
  //     call path planner to updatePrivileged
  //     call tactic setPath to clear the path when entering Idle
  state_machine->handleEvents(Event(Action::EndGoal));
  EXPECT_EQ(state_machine->name(), "::Idle");
  EXPECT_EQ(state_machine->goals().size(), 1);

  // Handle idle -> repeat (without persistent_loc):
  //   Goes into topological localization state first (entry state of repeat)
  //   Trigger stateChanged callback saying it's in topological localization
  //   Call tactic to LockPipeline
  //   Perform idle onExit, topological localization setPipeline and onEntry
  //     Call tactic to addRun \todo there is a ephermeral flag seems never used
  //   Check tactic->persistentLoc, found vertex not set and call Action::Abort
  //   Trigger stateChanged callback saying it's in Idle
  //   Pipeline unlocked (out scope)
  state_machine->handleEvents(Event::StartRepeat({{1, 50}, {1, 300}}));
  EXPECT_EQ(state_machine->name(), "::Idle");
  EXPECT_EQ(state_machine->goals().size(), 1);

  // \todo Need tests to handle idle -> repeat with persistent_loc
}

// Run all the tests that were declared with TEST()
int main(int argc, char** argv) {
  testing::InitGoogleTest(&argc, argv);
#if 0
  ros::init(argc, argv, "state_machine_tests");
  ros::NodeHandle nh;
#endif
  return RUN_ALL_TESTS();
}
