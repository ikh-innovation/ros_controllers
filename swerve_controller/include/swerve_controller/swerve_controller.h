/*********************************************************************
 * Software License Agreement (BSD License)
 *
 *  Copyright (c) 2020, Exobotic
 *  Copyright (c) 2017, Irstea
 *  Copyright (c) 2013, PAL Robotics, S.L.
 *  All rights reserved.
 *
 *  Redistribution and use in source and binary forms, with or without
 *  modification, are permitted provided that the following conditions
 *  are met:
 *
 *   * Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 *   * Redistributions in binary form must reproduce the above
 *     copyright notice, this list of conditions and the following
 *     disclaimer in the documentation and/or other materials provided
 *     with the distribution.
 *   * Neither the name of Exobotic nor the names of its
 *     contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 *  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 *  "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 *  LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 *  FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
 *  COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 *  INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 *  BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 *  LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 *  CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 *  LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
 *  ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 *  POSSIBILITY OF SUCH DAMAGE.
 *********************************************************************/

#ifndef SWERVE_CONTROLLER_SWERVE_CONTROLLER_H
#define SWERVE_CONTROLLER_SWERVE_CONTROLLER_H

#include <cmath>
#include <string>
#include <boost/optional.hpp>

#include <controller_interface/multi_interface_controller.h>
#include <hardware_interface/joint_command_interface.h>
#include <urdf_geometry_parser/urdf_geometry_parser.h>
#include <pluginlib/class_list_macros.hpp>
#include <dynamic_reconfigure/server.h>

#include <nav_msgs/Odometry.h>
#include <tf/tfMessage.h>
#include <tf/transform_datatypes.h>

#include <realtime_tools/realtime_buffer.h>
#include <realtime_tools/realtime_publisher.h>

#include <swerve_controller/odometry.h>
#include <swerve_controller/speed_limiter.h>
#include <swerve_controller/SwerveControllerConfig.h>


namespace swerve_controller
{

    /**
 * This class makes some assumptions on the model of the robot:
 *  - the rotation axes of wheels are collinear
 *  - the wheels are identical in radius
 * Additional assumptions to not duplicate information readily available in the URDF:
 *  - the wheels have the same parent frame
 *  - a wheel collision geometry is a cylinder in the urdf
 *  - a wheel joint frame center's vertical projection on the floor must lie within the contact patch
 */
    class SwerveController
        : public controller_interface::MultiInterfaceController<hardware_interface::VelocityJointInterface,
                                                                hardware_interface::PositionJointInterface>
    {
    public:
        SwerveController();

        bool init(hardware_interface::RobotHW *robot_hw,
                  ros::NodeHandle &root_nh,
                  ros::NodeHandle &controller_nh);

        void update(const ros::Time &time, const ros::Duration &period);

        void starting(const ros::Time &time);

        void stopping(const ros::Time &time);

    private:
        std::string name_;

        // Node handle and timer to check for changes in wheel radius
        // ros::NodeHandle param_nh{};
        // ros::Timer param_timer_ ;

        /// Odometry related:
        ros::Duration publish_period_;
        ros::Time last_state_publish_time_;
        bool open_loop_;

        /// Hardware handles:
        boost::optional<hardware_interface::JointHandle> lf_wheel_joint_;
        boost::optional<hardware_interface::JointHandle> rf_wheel_joint_;
        boost::optional<hardware_interface::JointHandle> lh_wheel_joint_;
        boost::optional<hardware_interface::JointHandle> rh_wheel_joint_;
        boost::optional<hardware_interface::JointHandle> lf_steering_joint_;
        boost::optional<hardware_interface::JointHandle> rf_steering_joint_;
        boost::optional<hardware_interface::JointHandle> lh_steering_joint_;
        boost::optional<hardware_interface::JointHandle> rh_steering_joint_;

        /// Velocity command related:
        struct CommandTwist
        {
            ros::Time stamp;
            double lin_x;
            double lin_y;
            double ang;

            CommandTwist() : lin_x(0.0), lin_y(0.0), ang(0.0), stamp(0.0) {}
        };
        realtime_tools::RealtimeBuffer<CommandTwist> command_twist_;
        CommandTwist command_struct_twist_;
        ros::Subscriber sub_command_;

        /// Odometry related:
        std::shared_ptr<realtime_tools::RealtimePublisher<nav_msgs::Odometry>> odom_pub_;
        std::shared_ptr<realtime_tools::RealtimePublisher<tf::tfMessage>> tf_odom_pub_;
        Odometry odometry_;

        /// Wheel separation (or track), distance between left and right wheels
        /// (from the midpoint of the wheel width)
        double track_;

        /// Distance between a wheel joint (from the midpoint of the wheel width) and the
        /// associated steering joint: We consider that the distance is the same for every wheel
        double wheel_steering_y_offset_;

        /// Wheel radius (assuming it's the same for the left and right wheels):
        double wheel_radius_;

        /// Wheel base (distance between front and rear wheel):
        double wheel_base_;

        /// Range of steering angle:
        double min_steering_angle_, max_steering_angle_;

        /// Timeout to consider cmd_vel commands old:
        double cmd_vel_timeout_;

        /// Frame to use for the robot base:
        std::string base_frame_id_;

        /// Whether to publish odometry to tf or not:
        bool enable_odom_tf_;
        
        /// Whether to publish commands to a single wheel for debug purposes:
        bool debug_single_wheel_;

        /// Frame to use for odometry tf publishing:
        std::string odom_frame_;

        /// Odometry topic name
        std::string odom_topic_name_;

        /// Command topic name
        std::string command_topic_name_;

        /// Speed limiters:
        CommandTwist last1_cmd_;
        CommandTwist last0_cmd_;
        SpeedLimiter limiter_lin_;
        SpeedLimiter limiter_ang_;

        /// Node handle to use when storing odometry values while switching controllers
        ros::NodeHandle switching_nh;

        /// Clipping indication for steering joints
        int lf_clipped_ {0};
        int rf_clipped_ {0};
        int lh_clipped_ {0};
        int rh_clipped_ {0};
        
        /// Angle threshold to apply wheel rotation before translational velocity
        double angle_threshold_ {0.5}; 

        // A struct to hold dynamic parameters
        // set from dynamic_reconfigure server
        struct DynamicParams
        {
            bool update;
            bool enable_odom_tf;
            double angle_threshold;
            double wheel_radius;

            DynamicParams()
                : angle_threshold(0.5)
                , enable_odom_tf(true)
                , wheel_radius(0.135)
            {}

            friend std::ostream& operator<<(std::ostream& os, const DynamicParams& params)
            {
                os << "DynamicParams:\n"
                //
                << "\t\tAngle threshold: "   << params.angle_threshold  << "\n"
                //
                << "\t\tPublish frame odom on tf: " << (params.enable_odom_tf?"enabled":"disabled") << "\n" 
                //
                << "\t\tWheel radius: "   << params.wheel_radius  << "\n";
                return os;
            }
        };

        realtime_tools::RealtimeBuffer<DynamicParams> dynamic_params_;

        /// Dynamic Reconfigure server
        typedef dynamic_reconfigure::Server<SwerveControllerConfig> ReconfigureServer;
        
        std::shared_ptr<ReconfigureServer> dyn_reconf_server_;

    private:
        // void timerCallback(const ros::TimerEvent &event);

        void updateOdometry(const ros::Time &time);

        void updateCommand(const ros::Time &time, const ros::Duration &period);

        void brake();

        bool clipSteeringAngle(double &steering, double &speed, int &is_clipped);

        void minimizeTurn(double &new_angle, double &current_angle, double &speed);

        bool checkError(double &target_angle, double &current_angle);

        void cmdVelCallback(const geometry_msgs::Twist &command);

        bool getPhysicalParams(ros::NodeHandle &controller_nh);

        void setOdomPubFields(ros::NodeHandle &root_nh, ros::NodeHandle &controller_nh);
        
        /**
         * \brief Callback for dynamic_reconfigure server
         * \param config The config set from dynamic_reconfigure server
         * \param level not used at this time.
         * \see dyn_reconf_server_
         */
        void reconfCallback(SwerveControllerConfig& config, uint32_t /*level*/);

        /**
         * \brief Update the dynamic parameters in the RT loop
         */
        void updateDynamicParams();
    
    };

    PLUGINLIB_EXPORT_CLASS(swerve_controller::SwerveController,
                           controller_interface::ControllerBase);

} // namespace swerve_controller

#endif // SWERVE_CONTROLLER_SWERVE_CONTROLLER_H
