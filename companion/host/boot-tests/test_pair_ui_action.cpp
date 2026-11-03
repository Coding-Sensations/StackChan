#include "companion/pair_ui_action.hpp"
#include <cassert>
int main(){using P=companion::PairUiAction;P p;
p.Click();assert(p.Take("","",false)==P::Action::Pair);assert(p.Take("","",false)==P::Action::None);
p.Click();assert(p.Take("123456","requestA",true)==P::Action::Stale);
p.Render("123456","requestA",false);p.Click();assert(p.Take("123456","requestA",true)==P::Action::Stale);
p.Click();assert(p.Take("123456","requestB",false)==P::Action::Stale);
p.Click();p.Render("654321","requestB",true);assert(p.Take("654321","requestB",true)==P::Action::Stale);
p.Click();assert(p.Take("654321","requestB",true)==P::Action::Confirm);assert(p.Take("654321","requestB",true)==P::Action::None);
p.Click();p.Reset();assert(p.Take("","",false)==P::Action::None);
p.Click();assert(p.Take("","",false)==P::Action::Pair);
}
