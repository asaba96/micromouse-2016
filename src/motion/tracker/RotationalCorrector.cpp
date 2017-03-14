#include "../../device/Orientation.h"
#include "../../device/RangeSensorContainer.h"
#include "RotationalCorrector.h"

namespace Motion {

const LengthUnit GyroResetter::kTotalDistance = LengthUnit::fromCells(0.0);

const LengthUnit GyroResetter::kDistancePerPoint = LengthUnit::fromAbstract(
                                          kTotalDistance.abstract() / kPoints);

const AngleUnit GyroResetter::kGyroLimit = AngleUnit::fromDegrees(0.0);
const AngleUnit GyroResetter::kRangeLimit = AngleUnit::fromDegrees(0.0);

GyroResetter::GyroResetter() :
  gyro_(kGyroLimit.degrees()), range_(kRangeLimit.degrees())
{}

void GyroResetter::point(LinearRotationalPoint point)
{
  LengthUnit displacement = point.linear_point.displacement;

  if (displacement.abstract() > kDistancePerPoint.abstract() * counter_++)
    iteration(point);
}

void GyroResetter::iteration(LinearRotationalPoint point)
{
  AngleUnit gyro = AngleUnit::fromDegrees(-gOrientation->getHeading());
  AngleUnit range = AngleUnit::fromDegrees(-RangeSensors.errorFromCenter());

  gyro_.push(gyro.degrees());
  range_.push(range.degrees());

  if (gyro_.isStable() && range_.isStable()) {
    gOrientation->incrementHeading(-gOrientation->getHeading());
    gyro_.reset();
    range_.reset();
  }
}

RotationalCorrector::RotationalCorrector(PIDParameters gyro_pid_parameters,
                                          PIDParameters range_pid_parameters) :
  gyro_pid_(gyro_pid_parameters), range_pid_(range_pid_parameters)
{}

Matrix<double> RotationalCorrector::output(LinearRotationalPoint target)
{
  gOrientation->update();
  RangeSensors.updateReadings();

  updateResetter(target);

  double response = gyroResponse(target) + rangeResponse(target);

  return Matrix<double>::splat(response).oppose();
}

void RotationalCorrector::reset()
{
  gyro_pid_.reset();
  range_pid_.reset();
}

void RotationalCorrector::updateResetter(LinearRotationalPoint point)
{
  gyro_resetter_.point(point);
}

double RotationalCorrector::gyroResponse(LinearRotationalPoint target)
{
  AngleUnit current = AngleUnit::fromDegrees(-gOrientation->getHeading());
  AngleUnit setpoint = target.rotational_point.displacement;

  return gyro_pid_.response(current.degrees(), setpoint.degrees());
}

double RotationalCorrector::rangeResponse(LinearRotationalPoint target)
{
  AngleUnit current = AngleUnit::fromDegrees(-RangeSensors.errorFromCenter());
  AngleUnit setpoint = target.rotational_point.displacement;

  return range_pid_.response(current.degrees(), setpoint.degrees());
}

}
