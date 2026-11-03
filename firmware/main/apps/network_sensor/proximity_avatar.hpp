#pragma once
#include "proximity_gestures.hpp"
#include <stackchan/avatar/avatar/elements/emotion.h>
namespace proximity_expression {
template<class Avatar> void ApplyFace(Avatar& avatar,Face face){
 using stackchan::avatar::Emotion;
 avatar.setEmotion(face==Face::Curious?Emotion::Doubt:face==Face::Happy?Emotion::Happy:face==Face::Sad?Emotion::Sad:Emotion::Neutral);
}
}
