#include <Arduino.h>

#include "MotionCalc.h"

MotionCalc::MotionCalc (float temp_dTot, float temp_vMax, float temp_vStart, float temp_vEnd,
                        float temp_max_accel, float temp_max_decel) {
  dTot = temp_dTot;
  vMax = temp_vMax;
  vStart = temp_vStart;
  vEnd = temp_vEnd;
  max_accel = temp_max_accel;
  max_decel = temp_max_decel;

  // make sure that accel and decel have the correct sign
  if (max_accel < 0) {
    max_accel *= -1;
  }
  if (max_decel > 0) {
    max_decel *= -1;
  }

  // turn dTot into meters
  dTot /= 1000;

  // set constants from global.  Do this a different way later to reference conf.h
  if (((dTot < 0) && (vMax > 0)) || ((dTot > 0) && (vMax < 0))) {
    vMax = -vMax;
  }

  if (vStart <= vMax) {
    aStart = max_accel;
  }
  else {
    aStart = max_decel;
  }

  if (vEnd <= vMax) {
    aEnd = max_decel;
  }
  else {
    aEnd = max_accel;
  }

  // do initial calculations
  // set distances assuming there is room to reach max speed
  dStart = (vMax * vMax - vStart * vStart) / (2 * aStart);
  dEnd = (vEnd * vEnd - vMax * vMax) / (2 * aEnd);
  tConst = 1000000 * (dTot - dStart - dEnd) / vMax;

  // set distances if there is not space to reach max speed
  if (tConst < 0) {
    dStart = (vStart * vStart - vEnd * vEnd + 2 * aEnd * dTot) / (2 * aEnd - 2 * aStart);
    dEnd = dTot - dStart;
    vMax = sqrt(vStart * vStart + 2 * aStart * dStart);
    tConst = 0;
  }

  // calculate tStart and tEnd based on dStart and dEnd
  tStart = (1000000 * (-vStart + sqrt(vStart * vStart + 2 * aStart * dStart)) / aStart);
  tEnd =  (1000000 * (-vEnd + sqrt(vEnd * vEnd + 2 * -aEnd * dEnd)) / -aEnd);
}

float MotionCalc::idealDistance (int elapsedTime) {
  if (elapsedTime < tStart) {
    return (elapsedTime / 1000 * (vStart + .5 * aStart * elapsedTime / 1000000));
  }
  else if (elapsedTime < (tStart + tConst)) {
    return (dStart * 1000 + vMax * (elapsedTime - tStart) / 1000);
  }
  else if (elapsedTime < (tStart + tConst + tEnd)) {
    float t = ((float)elapsedTime - tStart - tConst) / 1000000;
    return (((dTot - dEnd) + (vMax * t + .5 * aEnd * t * t)) * 1000);
  }
  else {
    return (dTot * 1000);
  }
}

float MotionCalc::idealVelocity (int elapsedTime) {
  if (elapsedTime <= tStart) {
    return (vStart + aStart * elapsedTime / 1000000);
  }
  else if (elapsedTime < (tStart + tConst)) {
    return (vMax);
  }
  else if (elapsedTime < (tStart + tConst + tEnd)) {
    return (vMax + aEnd * (elapsedTime - tStart - tConst) / 1000000);
  }
  else {
    return (vEnd);
  }
}

float MotionCalc::idealAccel (int elapsedTime) {
  if (elapsedTime <= tStart) {
    return (aStart);
  }
  else if (elapsedTime < (tStart + tConst)) {
    return (0);
  }
  else if (elapsedTime < (tStart + tConst + tEnd)) {
    return (aEnd);
  }
  else {
    return (0);
  }
}