#pragma once
#include <mooncake.h>
#include <memory>
#include <functional>
namespace direct_usb_pcm {
class Page:public mooncake::AppAbility {
public:
 // Request stop without discarding the page or its checked-close supervision.
 virtual bool RequestClose()=0;
};
std::unique_ptr<Page> MakeApp(std::function<void()> returned = {});
}
