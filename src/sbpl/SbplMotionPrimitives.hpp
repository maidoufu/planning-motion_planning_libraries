#ifndef _MOTION_PLANNING_LIBRARIES_SBPL_MOTION_PRIMITIVES_HPP_
#define _MOTION_PLANNING_LIBRARIES_SBPL_MOTION_PRIMITIVES_HPP_

#include <fstream>
#include <iomanip> // std::setprecision
#include <iostream>
#include <vector>

#include <base/Eigen.hpp>

#include <motion_planning_libraries/Config.hpp>

namespace motion_planning_libraries {

/**
 * Speed in m/sec or rad/sec.
 */
struct MotionPrimitivesConfig {
    MotionPrimitivesConfig() :
            mSpeedForward(0.0),
            mSpeedBackward(0.0),
            mSpeedLateral(0.0),
            mSpeedTurn(0.0),
            mSpeedPointTurn(0.0),
            mMultiplierForward(1),
            mMultiplierBackward(1),
            mMultiplierLateral(1),
            mMultiplierTurn(1),
            mMultiplierPointTurn(1),
            mNumTurnPrimitives(0),
            mNumIntermediatePoses(0),
            mNumAngles(0),
            mMapWidth(0),
            mMapHeight(0),
            mGridSize(0.0) {   
    }
    
    MotionPrimitivesConfig(Config config, int trav_map_width, int trav_map_height, double grid_size) :
        mSpeedForward(config.mRobotForwardVelocity),
        mSpeedBackward(config.mRobotBackwardVelocity),
        mSpeedLateral(0.0),
        mSpeedTurn(config.mRobotRotationalVelocity),
        mSpeedPointTurn(0.0),
        mMultiplierForward(1),
        mMultiplierBackward(1),
        mMultiplierLateral(1),
        mMultiplierTurn(1),
        mMultiplierPointTurn(1),
        mNumTurnPrimitives(2),
        mNumIntermediatePoses(10),
        mNumAngles(16),
        mMapWidth(trav_map_width),
        mMapHeight(trav_map_height),
        mGridSize(grid_size) {   
    }   
    
  public:
    double mSpeedForward;
    double mSpeedBackward;
    double mSpeedLateral;
    double mSpeedTurn;
    double mSpeedPointTurn;
    unsigned int mMultiplierForward;
    unsigned int mMultiplierBackward;
    unsigned int mMultiplierLateral;
    unsigned int mMultiplierTurn;
    unsigned int mMultiplierPointTurn;
    unsigned int mNumTurnPrimitives;
    
    unsigned int mNumIntermediatePoses; // Number of points a primitive consists of.
    unsigned int mNumAngles; // Number of discrete angles (in general 2*M_PI / 16)
    
    unsigned int mMapWidth;
    unsigned int mMapHeight;
    double mGridSize; // Width/length of a grid cell in meter.
};

/**
 * Describes a single motion primitive.
 */
struct Primitive {
 public:
    Primitive() : mId(0), mStartAngle(0), mEndPose(), 
            mCostMultiplier(0), mIntermediatePoses() 
    {
    }
    
    Primitive(int id, int start_angle, base::Vector3d end_pose, unsigned int cost_multiplier) : 
            mId(id), mStartAngle(start_angle), mEndPose(end_pose), 
            mCostMultiplier(cost_multiplier), mIntermediatePoses() 
    {
    }
     
    int mId;
    int mStartAngle;
    /// Discrete end pose in grid coordinates
    base::Vector3d mEndPose;
    unsigned int mCostMultiplier;
    std::vector<base::Vector3d> mIntermediatePoses;
};

/**
 * Allows to create and store the motion primitives file which is required
 * by SBPL for x,y,theta planning. x = forward, y = left, z = up
 * Each motion primitive consists of the following components:
 *  - ID of the motion primitive from 0 to mNumTurnPrimitives - 1
 *  - Discrete start angle
 *  - Discrete end pose (x_grid, y_grid, theta) theta has to be +- 0 to mNumAngles
 *  - Cost multiplier
 *  - mNumIntermediatePoses intermediate poses including start and end with (x_m, y_m, theta_rad)
 */
struct SbplMotionPrimitives {
 public:
    struct MotionPrimitivesConfig mConfig;
    std::vector<struct Primitive> mListPrimitives;
    // Scale factor used to scale all primitives to take sure that they reach a new state.
    double mScaleFactor; 
     
    SbplMotionPrimitives() {
    }
     
    SbplMotionPrimitives(struct MotionPrimitivesConfig config) : mConfig(config) {
    }
    
    ~SbplMotionPrimitives() {
    }
    
    /**
     * First uses fillListEndposes() to calculate the end poses in the world for 
     * all angles. These poses are discretized by rounding down (this should 
     * take care that we do not exceed the max turning radius.
     */
    void createPrimitives() {
        fillListEndposes();
        createIntermediatePoses();
    }
    
    void storeToFile(std::string path) {
        std::ofstream mprim_file;
        mprim_file.open(path.c_str());
        
        mprim_file << "resolution_m: " << mConfig.mGridSize << std::endl;
        mprim_file << "numberofangles: " << mConfig.mNumAngles << std::endl;
        mprim_file << "totalnumberofprimitives: " << mListPrimitives.size() << std::endl;
        
        
        struct Primitive mprim;
        for(unsigned int i=0; i < mListPrimitives.size(); ++i) {
            mprim =  mListPrimitives[i];
            mprim_file << "primID: " << mprim.mId << std::endl;
            mprim_file << "startangle_c: " << mprim.mStartAngle << std::endl;
            mprim_file << "endpose_c: " << mprim.mEndPose[0] << " " << 
                    mprim.mEndPose[1] << " " << mprim.mEndPose[2] << std::endl;
            mprim_file << "additionalactioncostmult: " << mprim.mCostMultiplier << std::endl;
            mprim_file << "intermediateposes: " << mprim.mIntermediatePoses.size() << std::endl;
            base::Vector3d v3d;
            for(unsigned int i=0; i < mprim.mIntermediatePoses.size(); ++i) {
                v3d = mprim.mIntermediatePoses[i];
                mprim_file << std::fixed << std::setprecision(4) << 
                        v3d[0] << " " << v3d[1] << " " << v3d[2] << std::endl;
            }
        }
        
        mprim_file.close();
    }

    /**
     * Calculates the end poses for 0 radians in x_m, y_m, theta_rad which can be 
     * reached within 'mScaleFactor' seconds. After that these poses are rotated
     * mNumAngles-1 times to cover the complete 2*M_PI.
     */
    void fillListEndposes() {
        // end pose and multiplier
        typedef std::pair<base::Vector3d, int> endpose;
        std::vector< endpose > poses_zero_rad; // x_m, y_m, theta_rad
        
        // Scaling: Each movement has to reach a new discrete state, so we have
        // to find the min scale factor. This represents the required movement time
        // to create these primitives.
        mScaleFactor = 1.0;
        if(mConfig.mSpeedForward > 0)
        mScaleFactor = std::max(mScaleFactor, mConfig.mGridSize / mConfig.mSpeedForward);
        if(mConfig.mSpeedBackward > 0)
        mScaleFactor = std::max(mScaleFactor, mConfig.mGridSize / mConfig.mSpeedBackward);
        if(mConfig.mSpeedLateral > 0)
        mScaleFactor = std::max(mScaleFactor, mConfig.mGridSize / mConfig.mSpeedLateral);
        if(mConfig.mSpeedPointTurn > 0)
        mScaleFactor = std::max(mScaleFactor, (2*M_PI/mConfig.mNumAngles) / mConfig.mSpeedPointTurn);
        if(mConfig.mSpeedTurn > 0)
        mScaleFactor = std::max(mScaleFactor, (2*M_PI/mConfig.mNumAngles) / mConfig.mSpeedTurn);
        
        // Forward
        if(mConfig.mSpeedForward > 0) {
            poses_zero_rad.push_back(endpose(base::Vector3d(mConfig.mGridSize, 0, 0), mConfig.mMultiplierForward));
            poses_zero_rad.push_back(endpose(base::Vector3d(mConfig.mSpeedForward * mScaleFactor, 0, 0), mConfig.mMultiplierForward));
        }
        // Backward
        if(mConfig.mSpeedBackward > 0) {
            poses_zero_rad.push_back(endpose(base::Vector3d(mConfig.mSpeedBackward * mScaleFactor, 0, 0), mConfig.mMultiplierBackward));
        }
        // Lateral
        if(mConfig.mSpeedLateral > 0) {
            poses_zero_rad.push_back(endpose(base::Vector3d(0.0, mConfig.mSpeedLateral * mScaleFactor, 0.0), mConfig.mMultiplierLateral));
            poses_zero_rad.push_back(endpose(base::Vector3d(0.0, -mConfig.mSpeedLateral * mScaleFactor, 0.0), mConfig.mMultiplierLateral));
        }
        // Point turn
        if(mConfig.mSpeedPointTurn > 0) {
            poses_zero_rad.push_back(endpose(base::Vector3d(0.0, 0.0, mConfig.mSpeedPointTurn * mScaleFactor), mConfig.mMultiplierPointTurn));
            poses_zero_rad.push_back(endpose(base::Vector3d(0.0, 0.0, -mConfig.mSpeedPointTurn * mScaleFactor), mConfig.mMultiplierPointTurn));
        }
        // Forward + negative turn
        // Calculates end pose driving with full forward and turn speeds.
        if(mConfig.mSpeedForward > 0 && mConfig.mSpeedTurn > 0) {
            double turning_radius = mConfig.mSpeedForward / mConfig.mSpeedTurn;
            base::Vector3d turned_start = Eigen::AngleAxis<double>(-mConfig.mSpeedTurn * mScaleFactor, Eigen::Vector3d::UnitZ()) * 
                    base::Vector3d(0.0, turning_radius, 0.0);
            turned_start[1] += turning_radius;
            turned_start[2] = -mConfig.mSpeedTurn * mScaleFactor;
            poses_zero_rad.push_back(endpose(turned_start, mConfig.mMultiplierTurn));
            // Forward + positive turn, just mirror negative turn
            turned_start[1] *= -1;
            turned_start[2] *= -1;
            poses_zero_rad.push_back(endpose(turned_start, mConfig.mMultiplierTurn));
        }
        
        // Creates discrete end poses for all angles.
        mListPrimitives.clear();
        base::Vector3d vec_tmp;
        base::Vector3d discrete_pose_tmp;
        double turn_step_rad = (M_PI*2.0) / (double)mConfig.mNumAngles; 
        assert(mConfig.mNumAngles != 0);
        for(unsigned int angle=0; angle<mConfig.mNumAngles; ++angle) {
            std::vector< std::pair<base::Vector3d, int> >::iterator it = poses_zero_rad.begin();
            for(int id=0; it != poses_zero_rad.end(); ++it, ++id) {
            vec_tmp = Eigen::AngleAxis<double>(angle * turn_step_rad, Eigen::Vector3d::UnitZ()) * it->first;
            // Adapt z component. SBPL orientation uses the range [0, 2*M_PI).
            // TODO correct?
            vec_tmp[2] = vec_tmp[2] + angle * turn_step_rad;
            while(vec_tmp[2] >= 2*M_PI) {
                vec_tmp[2] -= 2*M_PI;
            }
            // TODO How to round correctly?
            discrete_pose_tmp[0] = (int)(vec_tmp[0] / mConfig.mGridSize);
            discrete_pose_tmp[1] = (int)(vec_tmp[1] / mConfig.mGridSize);
            discrete_pose_tmp[2] = (int)(vec_tmp[2] / turn_step_rad);
            // Scale factor should prevent unwanted (same state) values?
            if(discrete_pose_tmp[0] == 0 && 
                    discrete_pose_tmp[1] == 0 && 
                    (unsigned int)discrete_pose_tmp[2] == mConfig.mNumAngles) {
                std::cout << "Vector " << vec_tmp.transpose() << " does not lead to another end state for angle " << angle << std::endl;
            }
            mListPrimitives.push_back(Primitive(id, angle, discrete_pose_tmp, it->second));
            }
        }
    }
    
    void createIntermediatePoses() {
        // Just for testing: just interpolation.
        std::vector<struct Primitive>::iterator it = mListPrimitives.begin();
        base::Vector3d end_pose_world;
        base::Vector3d intermediate_pose;
        for(;it != mListPrimitives.end(); it++) {
            end_pose_world[0] = it->mEndPose[0] * mConfig.mGridSize;
            end_pose_world[1] = it->mEndPose[1] * mConfig.mGridSize;
            end_pose_world[2] = 0; // it->mEndPose[2] contains theta
            double x_step = end_pose_world[0] / ((double)mConfig.mNumIntermediatePoses-1);
            double y_step = end_pose_world[1] / ((double)mConfig.mNumIntermediatePoses-1);
            double theta_step = it->mEndPose[2] / ((double)mConfig.mNumIntermediatePoses-1);
            for(int i=0; i<mConfig.mNumIntermediatePoses; i++) {
                intermediate_pose[0] = i * x_step;
                intermediate_pose[1] = i * y_step;
                intermediate_pose[2] = i * theta_step;
                it->mIntermediatePoses.push_back(intermediate_pose);
            }
            //it->mIntermediatePoses.push_back(end_pose_world);
        }
    }
};

} // end namespace motion_planning_libraries

#endif
