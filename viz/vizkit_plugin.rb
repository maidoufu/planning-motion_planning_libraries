Vizkit::UiLoader.register_3d_plugin('MotionPlanningLibrariesStateVisualization', 'motion_planning_libraries', 'MotionPlanningLibrariesStateVisualization')
Vizkit::UiLoader.register_3d_plugin_for('MotionPlanningLibrariesStateVisualization', "/motion_planning_libraries/State", :updateData )
Vizkit::UiLoader.register_3d_plugin_for('MotionPlanningLibrariesStateVisualization', "/std/vector</motion_planning_libraries/State>", :updateData )
