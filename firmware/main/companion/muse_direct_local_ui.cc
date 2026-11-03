#include "muse_direct_credential_policy.hpp"
// SPDX-License-Identifier: Apache-2.0
#include "muse_direct_local_ui.hpp"
#include "muse_direct_local_model.hpp"
#include <apps/common/tool_visual.hpp>
#include <array>
#include <cstdio>
#include <string>

namespace companion::muse {
struct LocalSetupUi::Impl {
 enum class Page { Mode, Ids, Review, Scope, Keys, Prepared, MessageTarget, MessageReview, MessageResult };
 struct Binding { Impl* self=nullptr; unsigned action=0; uint64_t revision=0,input=0; bool armed=false; };
 LocalSetupModel model; lv_obj_t* root=nullptr; lv_obj_t* title=nullptr; lv_obj_t* detail=nullptr;
 std::array<lv_obj_t*,20> buttons{},labels{}; std::array<Binding,20> bindings{};
 Page page=Page::Mode; LocalField editing=LocalField::None; unsigned key_page=0,scope=0,review_page=0; uint64_t revision=0;bool queued=false;const char* notice=nullptr;
 explicit Impl(LocalPort& p):model(p){}
 static void Event(lv_event_t* e) {
  auto& b=*static_cast<Binding*>(lv_event_get_user_data(e)); auto& u=*b.self;
  if(lv_event_get_code(e)==LV_EVENT_PRESSED){b.revision=u.revision;b.input=u.model.InputEpoch();b.armed=true;return;}
  if(!b.armed||b.revision!=u.revision||b.input!=u.model.InputEpoch()){b.armed=false;return;}
  b.armed=false;u.Act(b.action);
 }
 void Button(unsigned i,const char* text,int x,int y,int w,int h){
  lv_obj_remove_flag(buttons[i],LV_OBJ_FLAG_HIDDEN);lv_obj_set_pos(buttons[i],x,y);lv_obj_set_size(buttons[i],w,h);lv_label_set_text(labels[i],text);
 }
 void Build(lv_obj_t* parent){
  page=Page::Mode;editing=LocalField::None;key_page=scope=0;notice=nullptr;
  root=lv_obj_create(parent);stock_ui::Page(root);title=stock_ui::Text(root,"",8,5,304);detail=stock_ui::Text(root,"",8,28,304);
  lv_label_set_long_mode(detail,LV_LABEL_LONG_SCROLL_CIRCULAR);
  for(unsigned i=0;i<buttons.size();++i){buttons[i]=stock_ui::Button(root,"",0,0,1,1);labels[i]=lv_obj_get_child(buttons[i],0);bindings[i]={this,i};
   lv_obj_add_event_cb(buttons[i],Event,LV_EVENT_PRESSED,&bindings[i]);lv_obj_add_event_cb(buttons[i],Event,LV_EVENT_CLICKED,&bindings[i]);}
  model.Begin();Render();
 }
 std::string NetworkLabel()const{
  std::string result;for(auto c:model.Network().ssid){if(!c)break;result.push_back(c>=32&&c<=126?char(c):'?');}
  char bssid[24]{};const auto& b=model.Network().bssid;std::snprintf(bssid,sizeof(bssid)," (%02x:%02x:%02x:%02x:%02x:%02x)",b[0],b[1],b[2],b[3],b[4],b[5]);return result+bssid;
 }
 void Render(){
  ++revision; for(auto* button:buttons)lv_obj_add_flag(button,LV_OBJ_FLAG_HIDDEN);
  auto menu=[this](unsigned i,const char* name){Button(i,name,8,52+int(i)*35,304,32);};
  if(page==Page::Mode){lv_label_set_text(title,"Muse private setup");lv_label_set_text(detail,notice?notice:"Choose an action");menu(0,"Pair device");menu(1,"Prepare text chat");menu(2,"Close");return;}
  if(page==Page::Ids){lv_label_set_text(title,"Choose exact chat");std::string s="VM: "+std::string(model.Value(LocalField::Vm))+"   Chat: "+std::string(model.Value(LocalField::Chat));lv_label_set_text(detail,s.c_str());menu(0,"Enter VM ID");menu(1,"Enter chat ID");menu(2,"Review device and Wi-Fi");menu(3,"Back");return;}
  if(page==Page::Review){lv_label_set_text(title,"Review before approval");std::string s="Device: "+model.Identity().device+"   Node: "+model.Identity().node+"   Wi-Fi: "+NetworkLabel();if(model.Mode()==LocalMode::Chat)s+="   VM: "+std::string(model.Value(LocalField::Vm))+"   Chat: "+std::string(model.Value(LocalField::Chat));lv_label_set_text(detail,s.c_str());menu(0,"Review permissions");menu(1,"Back");return;}
  if(page==Page::Scope){
   static const char* pair[]={"Allow device enrollment?","Allow current Wi-Fi?",StorageConsent};
   static const char* chat[]={CredentialReadConsent,"Allow current Wi-Fi?","Allow this exact VM and chat?"};
   lv_label_set_text(title,"Separate permissions");lv_label_set_text(detail,notice?notice:(model.Mode()==LocalMode::Pair?pair[scope]:chat[scope]));
   menu(0,model.Allowed(scope)?"Allowed - tap to revoke":"Allow this permission");menu(1,scope==2?"Continue":"Next permission");menu(2,"Cancel");return;
  }
  if(page==Page::Prepared){lv_label_set_text(title,"Settings prepared");lv_label_set_text(detail,PreparedHelp);if(model.OwnsChat()){menu(0,"Enter message");menu(1,"Return to Settings");}else menu(0,"Return to Settings");return;}
  if(page==Page::MessageTarget){lv_label_set_text(title,"Review message destination");std::string s="VM: "+std::string(model.Value(LocalField::Vm))+"   Chat: "+std::string(model.Value(LocalField::Chat));lv_label_set_text(detail,s.c_str());menu(0,"Review message text");menu(1,"Cancel message");return;}
  if(page==Page::MessageReview){const auto value=model.Value(LocalField::Message);const unsigned count=unsigned((value.size()+47)/48);char heading[48]{};std::snprintf(heading,sizeof(heading),"Message page %u of %u",review_page+1,count);lv_label_set_text(title,heading);std::string slice(value.substr(review_page*48,48));lv_label_set_text(detail,slice.c_str());if(review_page+1<count)menu(0,"Next message page");else menu(0,"Queue this message");menu(1,"Cancel message");return;}
  if(page==Page::MessageResult){lv_label_set_text(title,queued?"Message queued":"Message not queued");lv_label_set_text(detail,queued?"Wait for a reply; do not resend.":"Check the connection and try again.");menu(0,"Return to Settings");return;}
  const char* alphabet=editing==LocalField::Token?"ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-_":"ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-_.:";
  const unsigned len=editing==LocalField::Message?95:unsigned(std::char_traits<char>::length(alphabet)), start=key_page*16;
  lv_label_set_text(title,editing==LocalField::Token?"SDK token (no echo)":editing==LocalField::Vm?"Enter exact VM ID":editing==LocalField::Chat?"Enter exact chat ID":"Enter message");
  char counter[48]{};std::string visible;
  if(editing==LocalField::Token){std::snprintf(counter,sizeof(counter),"%u / 43 characters",unsigned(model.TokenCount()));lv_label_set_text(detail,counter);}
  else {visible=std::string(model.Value(editing));lv_label_set_text(detail,visible.c_str());}
  for(unsigned i=0;i<16&&start+i<len;++i){char value[2]={editing==LocalField::Message?char(32+start+i):alphabet[start+i],0};Button(i,value[0]==' '?"SP":value,8+int(i%4)*76,53+int(i/4)*31,70,28);}
  Button(16,"Prev",8,181,70,28);Button(17,"Next",84,181,70,28);Button(18,"Delete",160,181,70,28);Button(19,"Done",236,181,70,28);
 }
 void Review(){if(model.ReviewIdentity(model.Epoch())&&model.ReviewNetwork(model.Epoch()))page=Page::Review;else notice="Device or saved Wi-Fi unavailable";}
 void Edit(LocalField field){if(model.Edit(model.Epoch(),field)){editing=field;key_page=0;page=Page::Keys;}}
 void Act(unsigned a){
  notice=nullptr;
  if(page==Page::Mode){if(a==2){Close();return;}if(a<2&&model.Select(model.Epoch(),a==0?LocalMode::Pair:LocalMode::Chat)){if(a==0)Review();else page=Page::Ids;}}
  else if(page==Page::Ids){if(a==0)Edit(LocalField::Vm);else if(a==1)Edit(LocalField::Chat);else if(a==2)Review();else if(a==3)page=Page::Mode;}
  else if(page==Page::Review){if(a==0){scope=0;page=Page::Scope;}else if(a==1)page=model.Mode()==LocalMode::Pair?Page::Mode:Page::Ids;}
  else if(page==Page::Scope){if(a==0){if(!model.Allow(model.Epoch(),scope))notice="Permission or network expired";}else if(a==1){if(scope<2)++scope;else if(model.Allowed(0)&&model.Allowed(1)&&model.Allowed(2)){if(model.Mode()==LocalMode::Pair){Edit(LocalField::Token);if(page==Page::Scope)notice="Permissions or network expired";}else if(model.Stage(model.Epoch()))page=Page::Prepared;else notice="Check IDs, permissions and network";}else notice="Allow all three permissions";}else if(a==2){model.Close();model.Begin();page=Page::Mode;}}
  else if(page==Page::Prepared){if(a==0&&model.OwnsChat())Edit(LocalField::Message);else if((a==0&&!model.OwnsChat())||(a==1&&model.OwnsChat())){lv_obj_add_flag(root,LV_OBJ_FLAG_HIDDEN);return;}}
  else if(page==Page::MessageTarget){if(a==0&&!model.Value(LocalField::Message).empty()){review_page=0;page=Page::MessageReview;}else if(a==1){model.EndEdit(true);page=Page::Prepared;}}
  else if(page==Page::MessageReview){if(a==0){const auto count=(model.Value(LocalField::Message).size()+47)/48;if(review_page+1<count)++review_page;else{queued=model.Send(model.Epoch());model.EndEdit(true);page=Page::MessageResult;}}else if(a==1){model.EndEdit(true);page=Page::Prepared;}}
  else if(page==Page::MessageResult){if(a==0){lv_obj_add_flag(root,LV_OBJ_FLAG_HIDDEN);return;}}
  else if(page==Page::Keys){
   const char* alphabet=editing==LocalField::Token?"ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-_":"ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-_.:";
   const unsigned len=editing==LocalField::Message?95:unsigned(std::char_traits<char>::length(alphabet));
   if(a<16&&key_page*16+a<len)model.Append(model.InputEpoch(),editing==LocalField::Message?char(32+key_page*16+a):alphabet[key_page*16+a]);
   else if(a==16&&key_page) --key_page;else if(a==17&&(key_page+1)*16<len)++key_page;else if(a==18)model.Pop(model.InputEpoch());
   else if(a==19){if(editing==LocalField::Token){if(model.Stage(model.Epoch()))page=Page::Prepared;else{model.EndEdit(true);page=Page::Mode;notice="Token or permissions invalid or expired";}}else if(editing==LocalField::Message){page=Page::MessageTarget;}else{model.EndEdit(false);page=Page::Ids;}}
  }
  Render();
 }
 void Close(){model.Close();if(root){lv_obj_delete(root);root=nullptr;}}
};
LocalSetupUi::LocalSetupUi(LocalPort& p):impl_(new Impl(p)){}
LocalSetupUi::~LocalSetupUi(){Close();}
void LocalSetupUi::Open(lv_obj_t* parent){if(!impl_->root)impl_->Build(parent);else lv_obj_remove_flag(impl_->root,LV_OBJ_FLAG_HIDDEN);}
void LocalSetupUi::Poll(){if(impl_->root){const auto count=impl_->model.TokenCount();const bool scoped=impl_->model.HasScope();impl_->model.Poll();if(scoped&&!impl_->model.HasScope()){impl_->page=Impl::Page::Mode;impl_->Render();}else if(count!=impl_->model.TokenCount())impl_->Render();}}
void LocalSetupUi::Close(){impl_->Close();}
bool LocalSetupUi::Visible()const{return impl_->root&&!lv_obj_has_flag(impl_->root,LV_OBJ_FLAG_HIDDEN);}
}
