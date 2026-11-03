// SPDX-License-Identifier: Apache-2.0
#pragma once
#include <cstdint>
#include <limits>
#include <mutex>
namespace companion {
enum class BleOwner { None, Stock, Muse };
class BleLease {
    const void* authority_=nullptr;
    BleOwner owner_=BleOwner::None;
    uint64_t generation_=0;
    friend class BleAuthority;
public:
    explicit operator bool()const{return generation_!=0;}
    BleOwner owner()const{return owner_;}
};
// Exclusive controller custody, not pairing/credential consent. Revocation
// keeps custody until callbacks and all hardware/worker cleanup are joined.
class BleAuthority {
    mutable std::mutex mutex_;
    BleLease held_{};
    uint64_t next_=0;
    bool revoked_=false;
    bool Match(const BleLease& l)const{return l.authority_==this&&l.generation_&&l.generation_==held_.generation_&&l.owner_==held_.owner_;}
public:
    bool Acquire(BleOwner owner,BleLease& out){
        std::lock_guard<std::mutex> lock(mutex_);
        if(out||held_||owner==BleOwner::None||next_==std::numeric_limits<uint64_t>::max())return false;
        held_.authority_=this;held_.owner_=owner;held_.generation_=++next_;revoked_=false;out=held_;return true;
    }
    bool Current(const BleLease& l)const{std::lock_guard<std::mutex> lock(mutex_);return Match(l)&&!revoked_;}
    bool Revoke(const BleLease& l){std::lock_guard<std::mutex> lock(mutex_);if(!Match(l))return false;revoked_=true;return true;}
    bool Release(BleLease& l,bool joined){std::lock_guard<std::mutex> lock(mutex_);if(!Match(l))return false;revoked_=true;if(!joined)return false;held_={};l={};return true;}
    BleOwner Owner()const{std::lock_guard<std::mutex> lock(mutex_);return held_.owner_;}
};
inline BleAuthority& CommonBleAuthority(){static BleAuthority authority;return authority;}
}
