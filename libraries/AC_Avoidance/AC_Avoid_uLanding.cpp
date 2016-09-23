#include "AC_Avoid_uLanding.h"

const AP_Param::GroupInfo AC_Avoid_uLanding::var_info[] = {

	// @Param: ENABLE
	// @DisplayName: uLanding Avoidance control enable/disable
    // @Description: Enabled/disable stopping based on uLanding feedback
    // @Values: 0:Disable,1:Enable
    // @User: Standard
	AP_GROUPINFO("ENABLE",1, AC_Avoid_uLanding, _uLanding_avoid_enable, ULANDING_ENABLE_DEFAULT),

	// @Param: DIST
	// @DisplayName: uLanding Avoidance standoff distance
    // @Description: uLanding distance to maintain from obstacle
	// @Units: cm
    // @Values: 0 1000.0
    // @User: Standard
	AP_GROUPINFO("DIST",2, AC_Avoid_uLanding, _uLanding_avoid_dist, ULANDING_AVOID_DIST_DEFAULT),

	// @Param: DIST_BUFF
	// @DisplayName: uLanding Avoidance buffer distance
    // @Description: uLanding distance required before exiting obstacle avoidance
	// @Units: cm
    // @Values: 0 1000.0
    // @User: Standard
	AP_GROUPINFO("DIST_BUFF",3, AC_Avoid_uLanding, _uLanding_avoid_dist_buffer, ULANDING_AVOID_DIST_BUFF_DEFAULT),

	// @Param: RNG_VALID
	// @DisplayName: uLanding Avoidance valid distance
    // @Description: minimum uLanding distance reading required before measurement is considered valid
	// @Units: cm
    // @Values: 31.0 100.0
    // @User: Standard
	AP_GROUPINFO("RNG_VALID",4, AC_Avoid_uLanding, _uLanding_avoid_dist_valid, ULANDING_AVOID_DIST_VALID_DEFAULT),

	// @Param: PIT_LIM
	// @DisplayName: uLanding Avoidance pitch limit
    // @Description: uLanding distance pitch limit
	// @Units: centi-degrees
    // @Values: 1000.0 4500.0 
    // @User: Standard
	AP_GROUPINFO("PIT_LIM",5, AC_Avoid_uLanding, _uLanding_avoid_pitch_lim, ULANDING_PITCH_LIMIT),

    // @Param: PIT_P
    // @DisplayName: Track Error controller P gain
    // @Description: Track Error controller P gain.  Converts track error into a velocity output (perpendicular to flight path)
    // @Range: 0.0 12.0
    // @Increment: 0.01
    // @User: Standard

    // @Param: PIT_I
    // @DisplayName: Track Error controller I gain
    // @Description: Track Error controller I gain.  Corrects long-term difference in track error
    // @Range: 0.0 5.0
    // @Increment: 0.01
    // @User: Standard

    // @Param: PIT_IMAX
    // @DisplayName: Track Error controller I gain maximum
    // @Description: Track Error controller I gain maximum.  Constrains the maximum rate output that the I gain will output
    // @Range: 0 100
    // @Increment: 0.01
    // @Units: cm/s
    // @User: Standard

    // @Param: PIT_D
    // @DisplayName: Track Error controller D gain
    // @Description: Track Error controller D gain.  Compensates for short-term change in track error
    // @Range: 0.0 5.0
    // @Increment: 0.001
    // @User: Standard

    // @Param: PIT_FILT
    // @DisplayName: Track Error conroller input frequency in Hz
    // @Description: Track Error conroller input frequency in Hz
    // @Range: 1 100
    // @Increment: 1
    // @Units: Hz
    AP_SUBGROUPINFO(_pid_stab_avoid, "PIT_", 6, AC_Avoid_uLanding, AC_PID),


	AP_GROUPEND
};

/// Constructor
AC_Avoid_uLanding::AC_Avoid_uLanding(const AP_Motors& motors, const RangeFinder& range, float dt)
	: _motors(motors),
	  _range(range),
	  _dt(dt),
	  _pid_stab_avoid(ULAND_STB_kP, ULAND_STB_kI, ULAND_STB_kD, ULAND_STB_IMAX, ULAND_STB_FILT_HZ, dt)
{
	AP_Param::setup_object_defaults(this, var_info);

	// initialize avoidance flags
	_avoid = false;
	_avoid_prev = false;
}

// monitor - monitor whether or not to avoid an obstacle
bool AC_Avoid_uLanding::monitor(void)
{
	
	// If motors are unarmed, do nothing
	if (!_motors.armed() || !_motors.get_interlock() || !_uLanding_avoid_enable) {
		return false;
	}else{

        return obstacle_detect(_range.distance_cm(0));
		//return obstacle_detect(100.0f);
	}
}

// stabilize_avoid - returns new pitch command in centi-degrees to avoid obstacle
void AC_Avoid_uLanding::stabilize_avoid(float &pitch_cmd)
{
	// Calculate distance buffer required to exit avoidance
	float buffer = _uLanding_avoid_dist + _uLanding_avoid_dist_buffer;

	// Reset integrator if the previous avoidance state was false
	if (!_avoid_prev) {
		_pid_stab_avoid.reset_I();
	}

	// calcualate distance error
	float err = _uLanding_avoid_dist - _range.distance_cm(0);
	err = -err;

	// pass error to the PID controller for avoidance distance
	_pid_stab_avoid.set_input_filter_d(err);

	// determine if pilot is commanding pitch to back away from obstacle
	bool pilot_cmd_avoidance = pitch_cmd < -500.0; // includes dead-zone of 5 degrees

	if (pilot_cmd_avoidance || (_range.distance_cm(0) > buffer)) {
		// allow pilot to maintain pitch command if actively avoiding obstacle
		pitch_cmd = pitch_cmd;
	}else{
		// compute pitch command from pid controller
		pitch_cmd = _pid_stab_avoid.get_pi();

		// limit pitch_cmd
		pitch_cmd = constrain_float(pitch_cmd, -_uLanding_avoid_pitch_lim, _uLanding_avoid_pitch_lim);
	}

	// set previous avoid state for next step through the monitor
	if (_range.distance_cm(0) > buffer) {
			_avoid = false;
	}

	_avoid_prev = _avoid;
}

// loiter_avoid - currently a place holder
/*
void AC_Avoid_uLanding::loiter_avoid(void)
{
	void
}
*/

// obstacle_detect - read uLanding and determine if obstacle is present and needs to be avoided
// 			dist is read in cm
bool AC_Avoid_uLanding::obstacle_detect(float dist)
{
	bool uLanding_valid = dist > _uLanding_avoid_dist_valid; // valid distance in cm

	if (uLanding_valid && (dist <= _uLanding_avoid_dist)) {
        _avoid = true;
    }else if (!uLanding_valid && _avoid_prev) {
		_avoid = false;
		_avoid_prev = false;
	}

	bool run_avoid = uLanding_valid && _avoid;

    return run_avoid;
}

