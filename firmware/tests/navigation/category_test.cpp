#include "apps/app_launcher/category_routes.hpp"
#include <cassert>
#include <string_view>
int main(){
 using namespace launcher_categories;
 assert(Titles.front()==std::string_view("Companions"));
 assert(Assign("Mira")==Group(0));
 assert(Assign("Stackchan")==Group(0));
 for(auto name:{"StackChan","Mira","Hermes","Muse","Claude","Codex"})assert(Assign(name)==Group::Companions);
 constexpr std::string_view installed[]={"Mira","Stackchan","Hermes","Muse","Claude","Codex","USB CONTROLS","Settings","AVATAR","DANCE","GAMES","SYNTH","NETWORK TOOLS","TOOLS"};
 unsigned companions=0;for(auto name:installed)companions+=Assign(name)==Group(0);
 assert(companions==6);
 assert(Assign("USB MIRA")==Group::Settings);
 assert(Assign("USB CONTROLS")==Group::Settings);
}
