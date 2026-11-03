/* SPDX-License-Identifier: MIT */
#include <stackchan/motion/motion.h>
#include <cstdlib>
#include <iostream>
#include <limits>
using namespace stackchan::motion;

void check(bool condition) {
    if (!condition) { std::cerr << "motion input reached an invalid target\n"; std::exit(1); }
}

// Replace only servo/animation IO. The actual Motion controller and gaze math
// compile unchanged into this executable; rejected inputs must reach neither
// servo, even after a valid target has been issued.
void Servo::init() {}
void Servo::update() {}
void Servo::move(int angle) { set_angle_impl(angle); }
void Servo::moveWithSpeed(int angle, int) { set_angle_impl(angle); }
int Servo::getCurrentAngle() { return 0; }
bool Servo::isMoving() { return false; }
struct RecordingServo : Servo {
    int calls=0, target=0;
    RecordingServo(int low,int high) { set_angle_limit({low,high}); }
    void set_angle_impl(int angle) override { ++calls; target=angle; }
    void limits(int low,int high) { set_angle_limit({low,high}); }
};
int main() {
    auto yaw=std::make_unique<RecordingServo>(-1280,1280);
    auto pitch=std::make_unique<RecordingServo>(30,870);
    auto* y=yaw.get(); auto* p=pitch.get(); Motion m(std::move(yaw),std::move(pitch));
    for(float bad : {std::numeric_limits<float>::quiet_NaN(),
                     std::numeric_limits<float>::infinity(),
                     -std::numeric_limits<float>::infinity()}) {
        m.lookAtNormalized(bad,0); m.lookAtNormalized(0,bad);
        m.lookAtPoint(bad,1,1); m.lookAtPoint(1,bad,1); m.lookAtPoint(1,1,bad);
    }
    m.lookAtPoint(0,0,0);
    y->limits(5,-5); m.lookAtNormalized(0,0); y->limits(-1280,1280);
    p->limits(870,30); m.lookAtNormalized(0,0); p->limits(30,870);
    check(y->calls==0 && p->calls==0);
    m.lookAtNormalized(2,-2); check(y->calls==1 && p->calls==1);
    check(y->target==1280 && p->target==30);
    const float big=std::numeric_limits<float>::max();
    m.lookAtPoint(big,big,big); check(y->target==450 && p->target==352);
    m.lookAtNormalized(std::numeric_limits<float>::quiet_NaN(),0);
    check(y->calls==2 && p->calls==2 && y->target==450 && p->target==352);
}
