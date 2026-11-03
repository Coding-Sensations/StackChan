#pragma once
#include <cstdint>
#include <limits>
#include <mutex>
#include <utility>
namespace companion::network {
enum class Owner { None, Saved, Vendor };
class StationAuthority;
class StationBorrow {
    friend class StationAuthority;
    StationAuthority* authority_=nullptr;
    uint64_t generation_=0;
    StationBorrow(StationAuthority* a,uint64_t g):authority_(a),generation_(g){}
public:
    StationBorrow()=default;
    StationBorrow(const StationBorrow&)=delete;
    StationBorrow& operator=(const StationBorrow&)=delete;
    StationBorrow(StationBorrow&& other)noexcept:authority_(std::exchange(other.authority_,nullptr)),generation_(other.generation_){}
    StationBorrow& operator=(StationBorrow&& other)noexcept {if(this!=&other){Reset();authority_=std::exchange(other.authority_,nullptr);generation_=other.generation_;}return *this;}
    ~StationBorrow(){Reset();}
    explicit operator bool()const{return authority_!=nullptr;}
    void Reset();
};
// Current custody, independent of SDK initialization/history. A failed stop or
// a live borrowed transport never authorizes another owner to touch the driver.
class StationAuthority {
    friend class StationBorrow;
    mutable std::mutex mutex_;
    Owner owner_=Owner::None;
    uint64_t generation_=0;
    unsigned borrowers_=0;
    bool stopping_=false;
    void ReleaseBorrow(uint64_t generation){std::lock_guard<std::mutex> lock(mutex_);if(owner_==Owner::Saved&&generation==generation_&&borrowers_)--borrowers_;}
public:
    bool Acquire(Owner owner){
        std::lock_guard<std::mutex> lock(mutex_);
        if(owner==Owner::None||stopping_)return false;
        if(owner_==owner)return true;
        if(owner_!=Owner::None||generation_==std::numeric_limits<uint64_t>::max())return false;
        owner_=owner;++generation_;return true;
    }
    Owner Current()const{std::lock_guard<std::mutex> lock(mutex_);return owner_;}
    uint64_t Generation()const{std::lock_guard<std::mutex> lock(mutex_);return generation_;}
    bool CanAcquire(Owner owner)const{std::lock_guard<std::mutex> lock(mutex_);return !stopping_&&(owner_==Owner::None||owner_==owner);}
    bool Accepts(Owner owner,uint64_t epoch)const{std::lock_guard<std::mutex> lock(mutex_);return !stopping_&&owner_==owner&&epoch&&epoch==generation_;}
    unsigned Borrowers()const{std::lock_guard<std::mutex> lock(mutex_);return borrowers_;}
    void RequestStop(Owner owner){std::lock_guard<std::mutex> lock(mutex_);if(owner_==owner)stopping_=true;}
    bool FinishStop(Owner owner,bool idle){std::lock_guard<std::mutex> lock(mutex_);if(!idle||owner_!=owner||!stopping_||borrowers_)return false;owner_=Owner::None;stopping_=false;return true;}
    StationBorrow BorrowSaved(){std::lock_guard<std::mutex> lock(mutex_);if(owner_!=Owner::Saved||stopping_||borrowers_==std::numeric_limits<unsigned>::max())return {}; ++borrowers_;return StationBorrow(this,generation_);}
};
inline void StationBorrow::Reset(){if(authority_){authority_->ReleaseBorrow(generation_);authority_=nullptr;}}
inline StationAuthority& Authority(){static auto* value=new StationAuthority;return *value;}
}
