#pragma once

#include <vtr_tactic/modules/base_module.hpp>

namespace vtr {
namespace tactic {

/**
 * \brief Migrate all landmarks found in the localization_map into a single
 * frame.
 * \details
 * requires:
 *   qdata.[rig_names, rig_features, rig_calibrations, T_sensor_vehicle]
 *   mdata.[localization_map, T_sensor_vehicle_map, map_id, localization_status
 *          T_r_m_prior]
 * outputs:
 *   mdata.[migrated_points, migrated_covariance, landmark_offset_map,
 *          migrated_landmark_ids, migrated_validity, migrated_points_3d,
 *          projected_map_points]
 */
class LandmarkMigrationModule : public BaseModule {
 public:
  /** \brief Static module identifier. */
  static constexpr auto static_name = "landmark_migration";

  /** \brief Module Configuration. */
  struct Config {};

  LandmarkMigrationModule(std::string name = static_name)
      : BaseModule{name}, config_(std::make_shared<Config>()) {}

  ~LandmarkMigrationModule() = default;

  /**
   * \brief Given a submap and target vertex located in this submap, this
   * module will transform all points into the coordinate frame of the target
   * vertex.
   * \param qdata The query data.
   * \param mdata The map data.
   * \param graph The STPG.
   */
  void runImpl(QueryCache &qdata, MapCache &mdata,
               const Graph::ConstPtr &graph) override;

  /** \brief Update the graph with the frame data for the live vertex */
  void updateGraphImpl(QueryCache &, MapCache &, const Graph::Ptr &,
                       VertexId) override {}

  /**
   * \brief Sets the module's configuration.
   * \param config The input configuration.
   */
  void setConfig(std::shared_ptr<Config> &config) { config_ = config; }

 private:
  /**
   * \brief Computes the transform that takes points from the current vertex to
   * the root vertex.
   * \param qdata The query data
   * \param mdata The map data
   * \param curr ID of the current vertex.
   * \return T_curr_root The transform that takes points from the current vertex
   * to the root
   */
  EdgeTransform getTRootCurr(QueryCache &qdata, MapCache &mdata,
                             VertexId &curr);

  /**
   * \brief Initializes the map data used in this module.
   * \param mdata The map data
   */
  void initializeMapData(QueryCache &qdata);

  /**
   * \brief migrates landmarks from the current vertex to the root vertex.
   * \param rig_idx the index into the current rig.
   * \param persist_id ID of the current vertex.
   * \param T_root_curr Transformation
   * \param mdata the Map data.
   * \param landmarks pointer to the landmarks.
   */
  void migrate(const int &rig_idx,
               const vtr_messages::msg::GraphPersistentId &persist_id,
               const EdgeTransform &T_root_curr, QueryCache &qdata,
               std::shared_ptr<vtr_messages::msg::RigLandmarks> &landmarks);

  /**
   * \brief Loads the sensor transform from robochunk via a vertex ID
   * \param vid The Vertex ID of the vertex we need to load the transform from.
   * \param transforms The map of vertex ID to T_s_v's
   * \param rig_name the name of the current rig
   * \param graph A pointer to the pose graph.
   */
  void loadSensorTransform(const VertexId &vid,
                           SensorVehicleTransformMap &transforms,
                           const std::string &rig_name,
                           const Graph::ConstPtr &graph);

  /** \brief Algorithm Configuration */
  std::shared_ptr<Config> config_;
};

}  // namespace tactic
}  // namespace vtr
