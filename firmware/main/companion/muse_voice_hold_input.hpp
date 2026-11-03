// SPDX-License-Identifier: Apache-2.0
#pragma once
#include <build_profiles/features.hpp>
#if STACKCHAN_BUILD_MUSE && STACKCHAN_BUILD_MUSE_VOICE_PROTOCOL
#include <lvgl.h>
#include <atomic>
#include <functional>
#include <cassert>
namespace companion::muse {
class VoiceUiAccess {public:virtual ~VoiceUiAccess()=default;virtual bool Enter()=0;virtual void Leave()=0;};
template<class Session,class Consent>class DeviceVoiceReviewUi;
// Pointer fields are accessed only with the original LVGL lock; cancellation is
// atomic and may arrive during IO. No public data API may mint a held ticket.
class NativePointerHold final {
 std::function<lv_indev_t*()> input_;VoiceUiAccess*access_;
 lv_indev_t*original_=nullptr;lv_obj_t*target_=nullptr;uint64_t generation_=0,last_ticket_=0;
 mutable std::atomic<uint64_t> ticket_{0};std::atomic<bool> revoked_{false};
 bool Sample()const{
  if(!original_||!target_||input_()!=original_||!lv_obj_is_visible(target_)||lv_indev_get_type(original_)!=LV_INDEV_TYPE_POINTER||lv_indev_get_state(original_)!=LV_INDEV_STATE_PRESSED)return false;
  lv_point_t point{};lv_area_t area{};lv_indev_get_point(original_,&point);lv_obj_get_coords(target_,&area);
  return point.x>=area.x1&&point.x<=area.x2&&point.y>=area.y1&&point.y<=area.y2;
 }
 bool Begin(uint64_t generation,lv_obj_t*target){
  if(revoked_.load()||ticket_.load())return false;
  original_=input_();target_=target;generation_=generation;
  if(!generation||last_ticket_==UINT64_MAX||!Sample()||lv_indev_active()!=original_||lv_indev_get_active_obj()!=target_)return false;
  ticket_.store(++last_ticket_);if(revoked_.load()){SignalCancel();return false;}return true;
 }
 void Detach(){SignalCancel();target_=nullptr;original_=nullptr;generation_=0;}
 template<class Session,class Consent>friend class DeviceVoiceReviewUi;
public:
 NativePointerHold(std::function<lv_indev_t*()> input,VoiceUiAccess*access):input_(std::move(input)),access_(access){}
 NativePointerHold(const NativePointerHold&)=delete;NativePointerHold&operator=(const NativePointerHold&)=delete;
 ~NativePointerHold(){assert(!ticket_.load()&&!target_);}
 void SignalRevoke(){revoked_.store(true);SignalCancel();}
 void CancelTicket(uint64_t ticket)const{auto original=ticket;ticket_.compare_exchange_strong(original,0);}
 void SignalCancel()const{ticket_.store(0);} // never a stop/join receipt
 uint64_t Ticket()const{return ticket_.load();} // metadata, not a permit
 bool Current(uint64_t generation,uint64_t ticket)const{
  if(revoked_.load()||!ticket||ticket_.load()!=ticket)return false;
  if(access_&&!access_->Enter()){auto old=ticket;ticket_.compare_exchange_strong(old,0);return false;}
  const bool current=!revoked_.load()&&generation==generation_&&ticket_.load()==ticket&&Sample()&&ticket_.load()==ticket;
  if(access_)access_->Leave();
  const bool final=current&&!revoked_.load()&&ticket_.load()==ticket;
  if(!final)CancelTicket(ticket);
  return final;
 }
};
}
#endif
