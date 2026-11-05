#pragma once

// Easing curves: take t in [0, 1] and return the eased progress (0 at t = 0, 1 at t = 1).
// Back and Elastic go a little outside [0, 1] on purpose. Out of range t is clamped.
//     float y = Lerp(startY, endY, Ease::OutBack(t));
//     Tween<float> pop{0, 1, 0.3f, Ease::OutElastic};
namespace BitMotor::Ease {

using Function = float (*)(float t);

float Linear(float t);

float InSine(float t);
float OutSine(float t);
float InOutSine(float t);

float InQuad(float t);
float OutQuad(float t);
float InOutQuad(float t);

float InCubic(float t);
float OutCubic(float t);
float InOutCubic(float t);

float InExpo(float t);
float OutExpo(float t);
float InOutExpo(float t);

float InBack(float t);
float OutBack(float t);
float InOutBack(float t);

float InElastic(float t);
float OutElastic(float t);
float InOutElastic(float t);

float InBounce(float t);
float OutBounce(float t);
float InOutBounce(float t);

} // namespace BitMotor::Ease
