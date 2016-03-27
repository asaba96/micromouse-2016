#include "Arduino.h"
#include "stdio.h"

#include "conf.h"
#include "motors.h"
#include "sensors_encoders.h"
#include "sensors_orientation.h"
#include "Menu.h"
#include "MotionCalc.h"
#include "PIDController.h"
#include "RangeSensorContainer.h"
#include "RangeSensor.h"
#include "motion.h"
#include "IdealSweptTurns.h"
#include "Menu.h"

static float max_accel_straight = MAX_ACCEL_STRAIGHT;
static float max_decel_straight = MAX_DECEL_STRAIGHT;

// This can probably match the straight accels until they become very high.  testing required
static float max_accel_rotate = MAX_ACCEL_ROTATE;
static float max_decel_rotate = MAX_DECEL_ROTATE;

// should be calculated based on turn radius and max velocity so that the friction force required is not more than the force available
static float max_accel_corner = MAX_ACCEL_CORNER;
static float max_decel_corner = MAX_DECEL_CORNER;

static float max_vel_straight = MAX_VEL_STRAIGHT;
static float max_vel_rotate = MAX_VEL_ROTATE;

static Orientation* orientation = NULL;

// instantiate the turn lookup tables
//IdealSweptTurns turn_45_table(SWEPT_TURN_45_FORWARD_SPEED,
//                              SWEPT_TURN_45_ANGLE, 0.001);
IdealSweptTurns turn_90_table(SWEPT_TURN_90_FORWARD_SPEED,
                              SWEPT_TURN_90_ANGLE, 0.001);
IdealSweptTurns turn_135_table(SWEPT_TURN_135_FORWARD_SPEED,
                              SWEPT_TURN_135_ANGLE, 0.001);
IdealSweptTurns turn_180_table(SWEPT_TURN_180_FORWARD_SPEED,
                              SWEPT_TURN_180_ANGLE, 0.001);

void motion_forward(float distance, float exit_speed) {
  float errorFrontRight, errorBackRight, errorFrontLeft, errorBackLeft, rotationOffset;
  float distancePerDegree = 3.14159265359 * MM_BETWEEN_WHEELS / 360;
  float idealDistance, idealVelocity;
  elapsedMicros moveTime;

  // float current_speed = (enc_left_velocity() + enc_right_velocity()) / 2;
  float current_speed = (enc_left_front_velocity() + enc_left_back_velocity() + enc_right_front_velocity() + enc_right_back_velocity())/4;
  MotionCalc motionCalc (distance, max_vel_straight, current_speed, exit_speed, max_accel_straight,
                         max_decel_straight);

  if (orientation == NULL) {
    orientation = Orientation::getInstance();
  }

  PIDController left_back_PID (KP_POSITION, KI_POSITION, KD_POSITION);
  PIDController left_front_PID (KP_POSITION, KI_POSITION, KD_POSITION);
  PIDController right_back_PID (KP_POSITION, KI_POSITION, KD_POSITION);
  PIDController right_front_PID (KP_POSITION, KI_POSITION, KD_POSITION);

  PIDController rotation_PID (KP_ROTATION, KI_ROTATION, KD_ROTATION);

  // zero clock before move
  moveTime = 0;

  RangeSensors.updateReadings();
  float savedError = 0;
  bool passedMiddle = false;

  // execute motion
  while (idealDistance != distance) {
    orientation->update();
    //Run sensor protocol here.  Sensor protocol should use encoder_left/right_write() to adjust for encoder error
    idealDistance = motionCalc.idealDistance(moveTime);
    idealVelocity = motionCalc.idealVelocity(moveTime);

    float leftReading, rightReading, position, nearestWhole, difference;

    leftReading = enc_left_front_extrapolate();
    rightReading = enc_right_front_extrapolate();
    position = (leftReading + rightReading) / 2 / MM_PER_BLOCK;
    nearestWhole = (int) (position + 0.5);
    difference = position - nearestWhole;

    // Add error from rangefinder data. Positive error is when it is too close
    // to the left wall, requiring a positive angle to fix it.
    RangeSensors.updateReadings();
    rotationOffset = rotation_PID.Calculate(RangeSensors.errorFromCenter());

    errorFrontLeft = enc_left_front_extrapolate() - idealDistance - rotationOffset;
    errorBackLeft = enc_left_back_extrapolate() - idealDistance - rotationOffset;
    errorFrontRight = enc_right_front_extrapolate() - idealDistance + rotationOffset;
    errorBackRight = enc_right_back_extrapolate() - idealDistance + rotationOffset;

    // Save isWall state for use by high-level code.
    if (!passedMiddle && position > distance / MM_PER_BLOCK - 0.5) {
	    RangeSensors.saveIsWall();
	    passedMiddle = true;
    }

    if (-0.25 < difference && difference < 0.25)
      rotationOffset = savedError;
    else
      savedError = rotationOffset;

    // Run PID to determine the offset that should be added/subtracted to the left/right wheels to fix the error.  Remember to remove or at the very least increase constraints on the I term
    // the offsets that are less than an encoder tick need to be added/subtracted from errorFrontLeft and errorFrontRight instead of encoderWrite being used.  Maybe add a third variable to the error calculation for these and other offsets

    motor_lf.Set(motionCalc.idealAccel(moveTime) + left_front_PID.Calculate(errorFrontLeft), idealVelocity);
    motor_rf.Set(motionCalc.idealAccel(moveTime) + right_front_PID.Calculate(errorFrontRight), idealVelocity);
    motor_lb.Set(motionCalc.idealAccel(moveTime) + left_back_PID.Calculate(errorBackLeft), idealVelocity);
    motor_rb.Set(motionCalc.idealAccel(moveTime) + right_back_PID.Calculate(errorBackRight), idealVelocity);
  }
  orientation->update();
  enc_left_front_write(0);
  enc_right_front_write(0);
  enc_left_back_write(0);
  enc_right_back_write(0);
  orientation->resetHeading();
}

void motion_collect(float distance, float exit_speed){
  float errorFrontRight, errorFrontLeft, errorBackRight, errorBackLeft;
  float rotationOffset;
  float idealDistance, idealVelocity;
  float distancePerDegree = 3.14159265359 * MM_BETWEEN_WHEELS / 360;
  elapsedMicros moveTime;

  float current_speed = (enc_left_front_velocity() + enc_left_back_velocity() + enc_right_front_velocity() + enc_right_back_velocity())/4;
  MotionCalc motionCalc (distance, max_vel_straight, current_speed, exit_speed, max_accel_straight,
                         max_decel_straight);

  PIDController left_back_PID (KP_POSITION, KI_POSITION, KD_POSITION);
  PIDController left_front_PID (KP_POSITION, KI_POSITION, KD_POSITION);
  PIDController right_back_PID (KP_POSITION, KI_POSITION, KD_POSITION);
  PIDController right_front_PID (KP_POSITION, KI_POSITION, KD_POSITION);

  PIDController rotation_PID (KP_ROTATION, KI_ROTATION, KD_ROTATION);

  if (orientation == NULL) {
    orientation = Orientation::getInstance();
  }
  rotationOffset = 0;

  float value_L, value_R, value_DL, value_DR;
  int num_range_value = distance/MOTION_COLLECT_MM_PER_READING;
  int num_readings = 1;
  int i;
  int last_reading_distance = -1;
  int int_distance;

  int reading_output_L[num_range_value];
  int reading_output_R[num_range_value];
  int reading_output_DL[num_range_value];
  int reading_output_DR[num_range_value];
  memset(reading_output_L, 0, num_range_value * sizeof(int));
  memset(reading_output_R, 0, num_range_value * sizeof(int));
  memset(reading_output_DL, 0, num_range_value * sizeof(int));
  memset(reading_output_DR, 0, num_range_value * sizeof(int));

  int reading_counter = 0;

  // zero clock before move
  moveTime = 0;

  // execute motion
  while (idealDistance != distance) {
    //Run sensor protocol here.  Sensor protocol should use encoder_left/right_write() to adjust for encoder error
    orientation->update();
    idealDistance = motionCalc.idealDistance(moveTime);
    idealVelocity = motionCalc.idealVelocity(moveTime);

    int_distance = (int) ((enc_left_front_extrapolate() + enc_right_front_extrapolate() + enc_left_back_extrapolate() + enc_right_back_extrapolate())/4);

    // takes a reading every 5mm
    if (int_distance % MOTION_COLLECT_MM_PER_READING == 0 && int_distance > last_reading_distance) {
      last_reading_distance = int_distance;

      for (i = 0; i < num_readings; i++)
      {
        // Front left
        RangeSensors.frontLeftSensor.updateRange();
        reading_output_L[reading_counter] += RangeSensors.frontLeftSensor.getRawReading();

        // Front right
        RangeSensors.frontRightSensor.updateRange();
        reading_output_R[reading_counter] += RangeSensors.frontRightSensor.getRawReading();

        // Diag left
        RangeSensors.diagLeftSensor.updateRange();
        reading_output_DL[reading_counter] += RangeSensors.diagLeftSensor.getRawReading();

        // Diag right
        RangeSensors.diagRightSensor.updateRange();
        reading_output_DR[reading_counter] += RangeSensors.diagRightSensor.getRawReading();
      }

      reading_output_L[reading_counter] /= num_readings;
      reading_output_R[reading_counter] /= num_readings;
      reading_output_DL[reading_counter] /= num_readings;
      reading_output_DR[reading_counter] /= num_readings;

      reading_counter++;
    }

    rotationOffset += rotation_PID.Calculate(orientation->getHeading() * distancePerDegree);

    errorFrontLeft = enc_left_front_extrapolate() - idealDistance - rotationOffset;
    errorFrontRight = enc_right_front_extrapolate() - idealDistance - rotationOffset;
    errorBackLeft = enc_left_back_extrapolate() - idealDistance + rotationOffset;
    errorBackRight = enc_right_back_extrapolate() - idealDistance + rotationOffset;

    // Run PID to determine the offset that should be added/subtracted to the left/right wheels to fix the error.  Remember to remove or at the very least increase constraints on the I term
    // the offsets that are less than an encoder tick need to be added/subtracted from errorFrontLeft and errorFrontRight instead of encoderWrite being used.  Maybe add a third variable to the error calculation for these and other offsets

    motor_lf.Set(motionCalc.idealAccel(moveTime) + left_front_PID.Calculate(errorFrontLeft), idealVelocity);
    motor_rf.Set(motionCalc.idealAccel(moveTime) + right_front_PID.Calculate(errorFrontRight), idealVelocity);
    motor_lb.Set(motionCalc.idealAccel(moveTime) + left_back_PID.Calculate(errorBackLeft), idealVelocity);
    motor_rb.Set(motionCalc.idealAccel(moveTime) + right_back_PID.Calculate(errorBackRight), idealVelocity);
  }

  enc_left_front_write(0);
  enc_right_front_write(0);
  enc_left_back_write(0);
  enc_right_back_write(0);

  motion_hold(100);
  while (!menu.buttonOkPressed()) {}

  Serial.println("=== Front Left ===");
  for(i = 0; i<num_range_value; i++){
    Serial.print(reading_output_L[i]);
    Serial.print(" ");
    Serial.print((int) distance - MOTION_COLLECT_MM_PER_READING * i);
    Serial.println();
  }
  Serial.println("=== Front Right ===");
  for(i = 0; i<num_range_value; i++){
    Serial.print(reading_output_R[i]);
    Serial.print(" ");
    Serial.print((int) distance - MOTION_COLLECT_MM_PER_READING * i);
    Serial.println();
  }
  Serial.println("=== Diag Left ===");
  for(i = 0; i<num_range_value; i++){
    Serial.print(reading_output_DL[i]);
    Serial.print(" ");
    Serial.print((int) distance - MOTION_COLLECT_MM_PER_READING * i);
    Serial.println();
  }
  Serial.println("=== Diag Right ===");
  for(i = 0; i<num_range_value; i++){
    Serial.print(reading_output_DR[i]);
    Serial.print(" ");
    Serial.print((int) distance - MOTION_COLLECT_MM_PER_READING * i);
    Serial.println();
  }
}

// clockwise angle is positive, angle is in degrees
void motion_rotate(float angle) {
  float distancePerDegree = 3.14159265359 * MM_BETWEEN_WHEELS / 360;
  float idealLinearDistance, idealLinearVelocity;
  float errorFrontRight, errorBackRight, errorFrontLeft, errorBackLeft, errorRotation;
  float rotation_correction;
  float linearDistance = distancePerDegree * angle;
  elapsedMicros moveTime;

  float current_speed = ((enc_left_front_velocity() + enc_left_back_velocity()) - (enc_right_front_velocity() + enc_right_back_velocity()))/4;
  MotionCalc motionCalc (linearDistance, max_vel_rotate, current_speed, 0, max_accel_rotate,
                         max_decel_rotate);

  if (orientation == NULL) {
    orientation = Orientation::getInstance();
  }

  PIDController left_front_PID (KP_POSITION, KI_POSITION, KD_POSITION);
  PIDController right_front_PID (KP_POSITION, KI_POSITION, KD_POSITION);
  PIDController left_back_PID (KP_POSITION, KI_POSITION, KD_POSITION);
  PIDController right_back_PID (KP_POSITION, KI_POSITION, KD_POSITION);
  PIDController rotation_PID (KP_ROTATION, KI_ROTATION, KD_ROTATION);

  // zero encoders and clock before move
  moveTime = 0;

  // the right will always be the negative of the left in order to rotate on a point.
  while (idealLinearDistance != linearDistance) {
    orientation->update();

    //Run sensor protocol here.  Sensor protocol should use encoder_left/right_write() to adjust for encoder error
    idealLinearDistance = motionCalc.idealDistance(moveTime);
    idealLinearVelocity = motionCalc.idealVelocity(moveTime);

    errorRotation = orientation->getHeading() * distancePerDegree - idealLinearDistance;
    rotation_correction += rotation_PID.Calculate(errorRotation);

    errorFrontLeft = enc_left_front_extrapolate() - idealLinearDistance - rotation_correction;
    errorBackLeft = enc_left_back_extrapolate() - idealLinearDistance - rotation_correction;
    errorFrontRight = enc_right_front_extrapolate() + idealLinearDistance + rotation_correction;
    errorBackRight = enc_right_back_extrapolate() + idealLinearDistance + rotation_correction;

    motor_lf.Set(motionCalc.idealAccel(moveTime) + left_front_PID.Calculate(errorFrontLeft),
                idealLinearVelocity);
    motor_rf.Set(-motionCalc.idealAccel(moveTime) + right_front_PID.Calculate(errorFrontRight),
                idealLinearVelocity);
    motor_lb.Set(motionCalc.idealAccel(moveTime) + left_back_PID.Calculate(errorBackLeft),
                idealLinearVelocity);
    motor_rb.Set(-motionCalc.idealAccel(moveTime) + right_back_PID.Calculate(errorBackRight),
                idealLinearVelocity);

    //run PID loop here.  new PID loop will add or subtract from a predetermined PWM value that was calculated with the motor curve and current ideal speed

  }
  orientation->update();
  enc_left_front_write(0);
  enc_right_front_write(0);
  enc_left_back_write(0);
  enc_right_back_write(0);
  orientation->resetHeading();
}

void motion_gyro_rotate(float angle) {
  float distancePerDegree = 3.14159265359 * MM_BETWEEN_WHEELS / 360;
  float idealLinearDistance, idealLinearVelocity;
  float errorLeft, errorRight, errorRotation;
  float rotation_correction;
  float linearDistance = distancePerDegree * angle;
  elapsedMicros moveTime;

  float current_speed = (enc_left_front_velocity() + enc_left_back_velocity()
                         - enc_right_front_velocity() - enc_right_back_velocity()) / 4;
  MotionCalc motionCalc (linearDistance, max_vel_rotate, current_speed, 0, max_accel_rotate,
                         max_decel_rotate);

  if (orientation == NULL) {
    orientation = Orientation::getInstance();
  }

  PIDController rotation_PID (KP_ROTATION, KI_ROTATION, KD_ROTATION);

  // zero encoders and clock before move
  moveTime = 0;

  // the right will always be the negative of the left in order to rotate on a point.
  while (idealLinearDistance != linearDistance) {
    orientation->update();

    //Run sensor protocol here.  Sensor protocol should use encoder_left/right_write() to adjust for encoder error
    idealLinearDistance = motionCalc.idealDistance(moveTime);
    idealLinearVelocity = motionCalc.idealVelocity(moveTime);

    errorRotation = orientation->getHeading() * distancePerDegree - idealLinearDistance;
    rotation_correction = rotation_PID.Calculate(errorRotation);

    // run PID loop here.  new PID loop will add or subtract from a predetermined
    //   PWM value that was calculated with the motor curve and current ideal speed
    motor_lf.Set(motionCalc.idealAccel(moveTime) + rotation_correction,
                idealLinearVelocity);
    motor_rf.Set(-motionCalc.idealAccel(moveTime) - rotation_correction,
                idealLinearVelocity);
    motor_lb.Set(motionCalc.idealAccel(moveTime) + rotation_correction,
                idealLinearVelocity);
    motor_rb.Set(-motionCalc.idealAccel(moveTime) - rotation_correction,
                idealLinearVelocity);
  }

  enc_left_front_write(0);
  enc_right_front_write(0);
  enc_left_back_write(0);
  enc_right_back_write(0);
  orientation->resetHeading();
}

void motion_corner(SweptTurnType turn_type, float speed) {
  float sign;
  float errorFrontRight, errorFrontLeft, errorBackRight, errorBackLeft;
  float errorRotation;
  float idealDistance;
  float rotation_offset;
  float rotation_correction;
  float time_scaling;
  float gyro_offset;
  int move_time_scaled = 0;
  float distancePerDegree = 3.14159265359 * MM_BETWEEN_WHEELS / 360;
  float total_time;
  IdealSweptTurns* turn_table;

  if (orientation == NULL) {
    orientation = Orientation::getInstance();
  }

  switch (turn_type) {
    //case kLeftTurn45:
    //  turn_table = &turn_45_table;
    //  time_scaling = speed / SWEPT_TURN_45_FORWARD_SPEED;
    //  sign = -1;
    //  break;
    case kLeftTurn90:
      turn_table = &turn_90_table;
      time_scaling = speed / SWEPT_TURN_90_FORWARD_SPEED;
      sign = -1;
      break;
    case kLeftTurn135:
      turn_table = &turn_135_table;
      time_scaling = speed / SWEPT_TURN_135_FORWARD_SPEED;
      sign = -1;
      break;
    case kLeftTurn180:
      turn_table = &turn_180_table;
      time_scaling = speed / SWEPT_TURN_180_FORWARD_SPEED;
      sign = -1;
      break;
    //case kRightTurn45:
    //  turn_table = &turn_45_table;
    //  time_scaling = speed / SWEPT_TURN_45_FORWARD_SPEED;
    //  sign = 1;
    //  break;
    case kRightTurn90:
      turn_table = &turn_90_table;
      time_scaling = speed / SWEPT_TURN_90_FORWARD_SPEED;
      sign = 1;
      break;
    case kRightTurn135:
      turn_table = &turn_135_table;
      time_scaling = speed / SWEPT_TURN_135_FORWARD_SPEED;
      sign = 1;
      break;
    case kRightTurn180:
      turn_table = &turn_180_table;
      time_scaling = speed / SWEPT_TURN_180_FORWARD_SPEED;
      sign = 1;
      break;
  }

  total_time = turn_table->getTotalTime();

  elapsedMicros move_time;

  PIDController left_front_PID (KP_POSITION, KI_POSITION, KD_POSITION);
  PIDController right_front_PID (KP_POSITION, KI_POSITION, KD_POSITION);
  PIDController left_back_PID (KP_POSITION, KI_POSITION, KD_POSITION);
  PIDController right_back_PID (KP_POSITION, KI_POSITION, KD_POSITION);
  PIDController rotation_PID (KP_ROTATION, KI_ROTATION, KD_ROTATION);

  // zero clock before move
  move_time = 0;

  // execute motion
  while (move_time_scaled < total_time) {
    //Run sensor protocol here.  Sensor protocol should use encoder_left/right_write() to adjust for encoder error
    orientation->update();

    move_time_scaled = move_time * time_scaling;
    
    idealDistance = move_time * speed / 1000;

    rotation_offset = sign * turn_table->getOffsetAtMicros(move_time_scaled);

    errorRotation = orientation->getHeading() * distancePerDegree - rotation_offset;
    rotation_correction += rotation_PID.Calculate(errorRotation);

    errorFrontLeft = enc_left_front_extrapolate() - idealDistance - rotation_offset - rotation_correction;
    errorBackLeft = enc_left_back_extrapolate() - idealDistance - rotation_offset - rotation_correction;
    errorFrontRight = enc_right_front_extrapolate() - idealDistance + rotation_offset + rotation_correction;
    errorBackRight = enc_right_back_extrapolate() - idealDistance + rotation_offset + rotation_correction;

//    motor_l.Set(distancePerDegree
//        * time_scaling
//        * turn_90_table.getAngularAcceleration(move_time_scaled / 1000) / 1000
//        + left_PID.Calculate(errorFrontLeft),
//        enc_left_velocity());
//    motor_r.Set(- distancePerDegree
//        * time_scaling
//        * turn_90_table.getAngularAcceleration(move_time_scaled / 1000) / 1000
//        + right_PID.Calculate(errorFrontRight),
//        enc_right_velocity());
   
       motor_lf.Set(left_front_PID.Calculate(errorFrontLeft),
        enc_left_front_velocity());
       motor_rf.Set(right_front_PID.Calculate(errorFrontRight),
        enc_right_front_velocity());
       motor_lb.Set(left_back_PID.Calculate(errorBackLeft),
        enc_left_back_velocity());
       motor_rb.Set(right_back_PID.Calculate(errorBackRight),
        enc_right_back_velocity());

   }

  enc_left_front_write(0);
  enc_right_front_write(0);
  enc_left_back_write(0);
  enc_right_back_write(0);
  orientation->incrementHeading(-sign * turn_table->getTotalAngle());
}

void motion_hold(unsigned int time) {
  float errorFrontRight, errorFrontLeft, errorBackRight, errorBackLeft;
  float rightFrontOutput, leftFrontOutput, rightBackOutput, leftBackOutput;
  elapsedMicros currentTime;

  PIDController left_front_PID (KP_POSITION, KI_POSITION, KD_POSITION);
  PIDController right_front_PID (KP_POSITION, KI_POSITION, KD_POSITION);
  PIDController left_back_PID (KP_POSITION, KI_POSITION, KD_POSITION);
  PIDController right_back_PID (KP_POSITION, KI_POSITION, KD_POSITION);

  currentTime = 0;

  while (currentTime / 1000 < time) {
    errorFrontLeft = enc_left_front_extrapolate();
    errorFrontRight = enc_right_front_extrapolate();
    errorBackLeft = enc_left_back_extrapolate();
    errorBackRight = enc_right_back_extrapolate();

    leftFrontOutput = left_front_PID.Calculate(errorFrontLeft);
    rightFrontOutput = right_front_PID.Calculate(errorFrontRight);
    leftBackOutput = left_back_PID.Calculate(errorBackLeft);
    rightBackOutput = right_back_PID.Calculate(errorBackRight);

    motor_lf.Set(leftFrontOutput, 0);
    motor_rf.Set(rightFrontOutput, 0);
    motor_lb.Set(leftBackOutput,0);
    motor_rb.Set(rightBackOutput,0);
  }

  motor_lf.Set(0, 0);
  motor_rf.Set(0, 0);
  motor_lb.Set(0, 0);
  motor_rb.Set(0, 0);
}

void motion_hold_range(int setpoint, unsigned int time) {
  float errorFrontRight, errorFrontLeft, errorBackRight, errorBackLeft;
  float rightFrontOutput, leftFrontOutput, rightBackOutput, leftBackOutput;
  elapsedMicros currentTime;

  PIDController left_front_PID (KP_HOLD_RANGE, KI_HOLD_RANGE, KD_HOLD_RANGE);
  PIDController right_front_PID (KP_HOLD_RANGE, KI_HOLD_RANGE, KD_HOLD_RANGE);
  PIDController left_back_PID (KP_HOLD_RANGE, KI_HOLD_RANGE, KD_HOLD_RANGE);
  PIDController right_back_PID (KP_HOLD_RANGE, KI_HOLD_RANGE, KD_HOLD_RANGE);

  currentTime = 0;

  while (currentTime / 1000 < time) {

    RangeSensors.frontLeftSensor.updateRange();
    RangeSensors.frontRightSensor.updateRange();
    
    errorFrontLeft = setpoint - RangeSensors.frontLeftSensor.getRange();
    errorFrontRight = setpoint - RangeSensors.frontRightSensor.getRange();
    errorBackLeft = errorFrontLeft;
    errorBackRight = errorFrontRight;

    leftFrontOutput = left_front_PID.Calculate(errorFrontLeft);
    rightFrontOutput = right_front_PID.Calculate(errorFrontRight);
    leftBackOutput = left_back_PID.Calculate(errorBackLeft);
    rightBackOutput = right_back_PID.Calculate(errorBackRight);

    motor_lf.Set(leftFrontOutput, 0);
    motor_rf.Set(rightFrontOutput, 0);
    motor_lb.Set(leftBackOutput, 0);
    motor_rb.Set(rightBackOutput, 0);
  }

  motor_lf.Set(0, 0);
  motor_rf.Set(0, 0);
  motor_lb.Set(0, 0);
  motor_rb.Set(0, 0);
}

// functions to set max velocity variables
void motion_set_maxAccel_straight(float temp_max_accel_straight) {
  max_accel_straight = temp_max_accel_straight;
}
void motion_set_maxDecel_straight(float temp_max_decel_straight) {
  max_decel_straight = temp_max_decel_straight;
}

void motion_set_maxAccel_rotate(float temp_max_accel_rotate) {
  max_accel_rotate = temp_max_accel_rotate;
}
void motion_set_maxDecel_rotate(float temp_max_decel_rotate) {
  max_decel_rotate = temp_max_decel_rotate;
}

void motion_set_maxAccel_corner(float temp_max_accel_corner) {
  max_accel_corner = temp_max_accel_corner;
}
void motion_set_maxDecel_corner(float temp_max_decel_corner) {
  max_decel_corner = temp_max_decel_corner;
}

void motion_set_maxVel_straight(float temp_max_vel_straight) {
  max_vel_straight = temp_max_vel_straight;
}
void motion_set_maxVel_rotate(float temp_max_vel_rotate) {
  max_vel_rotate = temp_max_vel_rotate;
}
