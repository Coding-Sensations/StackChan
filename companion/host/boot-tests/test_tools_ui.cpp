#include "tools_ui.hpp"
#include "audio_owner.hpp"
#include <cassert>
#include <set>
using namespace stock_tools::ui;
bool inside(Rect r){return r.x>=0&&r.y>=0&&r.x+r.w<=320&&r.y+r.h<=240;}
bool overlap(Rect a,Rect b){return a.x<b.x+b.w&&b.x<a.x+a.w&&a.y<b.y+b.h&&b.y<a.y+a.h;}
int main(){
 for(auto r:{BackButton,RowOne,RowTwo,Primary,EditButton,EmulateButton,UrlInput})assert(inside(r)&&r.w>=44&&r.h>=44);
 assert(inside(EmulateDetail)&&!overlap(EmulateDetail,Primary));
 assert(!overlap(Rect{8,108,304,38},Rect{8,148,304,24}));
 assert(!overlap(Rect{8,120,304,24},Rect{8,148,304,24}));
 assert(!overlap(RowOne,RowTwo));assert(!overlap(EditButton,EmulateButton));
 for(auto r:ListRows)assert(inside(r)&&r.w>=44&&r.h>=44);
 for(size_t i=0;i<ListRows.size();++i)for(size_t j=i+1;j<ListRows.size();++j)assert(!overlap(ListRows[i],ListRows[j]));
 for(auto r:SplitPrimary)assert(inside(r)&&r.w>=44&&r.h>=44);
 assert(!overlap(SplitPrimary[0],SplitPrimary[1]));
 for(auto r:RemoteRows)assert(inside(r)&&r.w>=44&&r.h>=44&&!overlap(r,Primary));
 assert(!overlap(RemoteRows[0],RemoteRows[1]));
 for(int i=0;i<10;++i){auto a=KeyRect(i);assert(inside(a)&&a.w>=44&&a.h>=44);assert(!overlap(a,UrlInput));for(auto f:EditorFooter)assert(!overlap(a,f));for(int j=i+1;j<10;++j)assert(!overlap(a,KeyRect(j)));}
 for(auto f:EditorFooter)assert(inside(f)&&f.w>=44&&f.h>=44);
 std::set<char> keys;for(auto page:KeyPages){assert(page.size()<=10);for(char c:page){assert(keys.insert(c).second);assert(Allowed(Screen::Edit,Character+c));}}
 for(char c:std::string_view("https://example.com/A?x=1&y=2#tag"))assert(keys.count(c));
 std::set<char> uart_keys;for(auto page:UartKeyPages){assert(page.size()<=10);for(char c:page){assert(uart_keys.insert(c).second);assert(Allowed(Screen::UartEdit,Character+c));}}
 for(int c=32;c<=126;++c)assert(uart_keys.count(static_cast<char>(c)));
 assert(!Allowed(Screen::Edit,Character+32));
 assert(Parent(Screen::Learn)==Screen::Infrared&&Parent(Screen::Send)==Screen::Infrared);
 assert(Parent(Screen::Edit)==Screen::Nfc&&Parent(Screen::Emulate)==Screen::Nfc);
 for(auto s:{Screen::Home,Screen::Infrared,Screen::Learn,Screen::Send,Screen::Library,Screen::Nfc,Screen::Edit,Screen::Emulate,Screen::Uart,Screen::UartEdit})assert(Allowed(s,Back));
 assert(Parent(Screen::Library)==Screen::Infrared&&Parent(Screen::UartEdit)==Screen::Uart);
 assert(Allowed(Screen::Home,OpenUart)&&Allowed(Screen::Infrared,OpenLibrary)&&Allowed(Screen::Send,SaveRemote));
 // The current named IR archive and its consumer both bound the library to64.
 assert(Allowed(Screen::Library,ChooseRemote+63)&&!Allowed(Screen::Library,ChooseRemote+64));
 assert(Allowed(Screen::Uart,StartDemo)&&Allowed(Screen::Uart,SendDemo)&&!Allowed(Screen::Uart,SendOnce));
 assert(!Allowed(Screen::Library,StartEmulate)&&!Allowed(Screen::UartEdit,SendDemo));
 assert(!Allowed(Screen::Home,StartLearn)&&!Allowed(Screen::Edit,StartEmulate)&&!Allowed(Screen::Send,StartLearn));
 using namespace companion;
 // In-memory authority only: this fixture never opens hardware or workers.
 AudioLease mira_lease,stock_lease;
 assert(CurrentAudioOwner()==AudioOwner::None);
 assert(AcquireAudio(AudioOwner::Mira,AudioPurpose::Session,1,mira_lease));
 assert(!AcquireAudio(AudioOwner::Stock,AudioPurpose::StockRuntime,1,stock_lease));
 assert(!ReleaseAudio(stock_lease,true));assert(CurrentAudioOwner()==AudioOwner::Mira);
 assert(ReleaseAudio(mira_lease,true));
 assert(AcquireAudio(AudioOwner::Stock,AudioPurpose::StockRuntime,2,stock_lease));
 assert(!AcquireAudio(AudioOwner::Mira,AudioPurpose::Session,2,mira_lease));
 assert(ReleaseAudio(stock_lease,true));assert(CurrentAudioOwner()==AudioOwner::None);
}
