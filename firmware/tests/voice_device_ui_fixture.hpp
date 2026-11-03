// SPDX-License-Identifier: Apache-2.0
#pragma once
#include "voice_admission_fixture.hpp"
#include <companion/muse_voice_session_owner.hpp>
#include <companion/muse_voice_core_codec.hpp>
#include "voice_pcm_fixture.hpp"
#include <companion/muse_voice_device_ui.hpp>
#include <companion/muse_direct_credential_custody.hpp>
#include <type_traits>
static_assert(!std::is_copy_constructible_v<companion::muse::CredentialLease> && !std::is_copy_assignable_v<companion::muse::CredentialLease> && !std::is_move_constructible_v<companion::muse::CredentialLease>);
struct Device {lv_indev_t* input=nullptr;bool foreground=true;std::string node="synthetic-device";bool Foreground()const{return foreground;}bool Node(std::string& out)const{out=node;return true;}lv_indev_t* Input()const{return input;}};
using Consent=DeviceVoiceConsent<Admission,Reader,Device>;
using Session=VoiceSessionOwner<Admission>;
using Ui=DeviceVoiceReviewUi<Session,Consent>;
static void Down(lv_obj_t* button,lv_indev_t& input){active_input=event_input=&input;input.active=button;input.state=LV_INDEV_STATE_PRESSED;lv_area_t area{};lv_obj_get_coords(button,&area);input.point={(area.x1+area.x2)/2,(area.y1+area.y2)/2};Press(button);active_input=event_input=nullptr;}
static void Up(lv_obj_t* button,lv_indev_t& input){active_input=event_input=&input;input.active=button;input.state=LV_INDEV_STATE_RELEASED;lv_event_t released{button,button->callback_user,LV_EVENT_RELEASED};button->callback(&released);ReleaseClick(button);active_input=event_input=nullptr;}
static void Tap(lv_obj_t*button,lv_indev_t&input){assert(button&&Visible(button));Down(button,input);Up(button,input);}
struct Rig {
 AdmissionOwner original;CoreS3AudioCodec core=Make();CoreVoiceCodec<CoreS3AudioCodec> codec{core};Device device;lv_indev_t pointer;Admission* admitted=original.admission.get();
 Consent consent{*admitted,original.reader,original.clock,device};std::unique_ptr<Session> session;std::unique_ptr<Ui> ui;
 Rig(){Reset();screen.w=320;screen.h=240;device.input=&pointer;session=Session::Create(original.protocol,original.admission,codec,consent,original.clock);assert(session);ui=std::make_unique<Ui>(*session,consent);}
 ~Rig(){assert(ui->Close());assert(session->Close());}
 void Live(){assert(session->Open(original.Scope()));original.port->incoming.push_back(original.server.Response(7,200));assert(session->Tick());original.port->incoming.push_back(original.server.Body(7,session_event));assert(session->Tick());}
 void Review(unsigned mask){assert(ui->Open(&screen));while(auto*next=Button("Next"))Tap(next,pointer);if(mask&1)Tap(Button("Mic: no"),pointer);if(mask&2)Tap(Button("Out: no"),pointer);Tap(Button("Review"),pointer);}
};
