#include <build_profiles/features.hpp>
#include <usb_composite_v2/product_controls.h>
#include <apps/common/tool_visual.hpp>
// Stock Mooncake shell, original native Tools and game implementations.
#include "stock_apps.h"
#include <apps/runtime_lifecycle/session.h>
#include <apps/runtime_lifecycle/cleanup.h>
#include "sd_status_app.h"
#include "feature_apps.h"
#include "provider_companion_app.h"
#include <apps/local_tools/local_tools_app.h>
#include <apps/instruments/instrument_apps.h>
#include <companion/direct_usb_pcm/usb_pcm_app.h>
#include <apps/local_tools/local_tools_navigation.hpp>
#include <apps/network_sensor/network_sensor_apps.h>
#include <apps/network_sensor/proximity_expression_app.h>
#include "menu_policy.hpp"
#include "navigation.h"
#include "shell_teardown.hpp"
#include "stock_tools_hw.h"
#include "tools_ui.hpp"
#include "toolbox_extras.hpp"
#include "ir_storage_binding.hpp"
#include <apps/stock_agent/stock_agent.h>
#include <apps/network_setup/network_setup.h>
#include <apps/stock_services/stock_services.h>
#include <companion/settings_policy.hpp>
#include <map>
#include <optional>
#include <apps/apps.h>
#include <apps/common/common.h>
#include <assets/assets.h>
#include <companion/boot.h>
#include <companion/reactive_avatar_service.hpp>
#include <companion/closing_view_policy.hpp>
#include <apps/led_expression/led_service_api.h>
#include <apps/led_expression/led_apps.h>
#include <companion/diagnostics.h>
#include <hal/hal.h>
#include <stackchan/stackchan.h>
#include <stackchan/avatar/skins/default/default.h>
#include <esp_random.h>
#include <esp_system.h>
#include <esp_timer.h>
#include <lvgl.h>
#include <atomic>
#include <memory>
#include <cstdio>
namespace stock_shell {
class Page : public mooncake::AppAbility {
    struct Action {
        Page* owner;
        int id;
    };
    std::vector<std::unique_ptr<Action>> callbacks_;
    bool exit_pending_ = false;

protected:
    lv_obj_t* root_ = nullptr;
    std::atomic<int> action_{0};
    void Panel(const char* title)
    {
        exit_pending_ = false;
        action_       = 0;
        LvglLockGuard lock;
        root_ = lv_obj_create(lv_screen_active());
        lv_obj_set_size(root_, 320, 240);
        lv_obj_set_pos(root_, 0, 0);
        lv_obj_set_style_pad_all(root_, 0, 0);
        lv_obj_remove_flag(root_, LV_OBJ_FLAG_SCROLLABLE);
        stock_ui::Page(root_);
        auto text = lv_label_create(root_);
        lv_label_set_text(text, title);
        lv_obj_set_pos(text, 10, 6);
        Button("Back", 260, 2, 56, 40, 999);
    }
    lv_obj_t* Text(const char* text, int x, int y, int width)
    {
        auto label = lv_label_create(root_);
        lv_label_set_text(label, text);
        lv_obj_set_pos(label, x, y);
        lv_obj_set_width(label, width);
        return label;
    }
    lv_obj_t* Button(const char* name, int x, int y, int w, int h, int id)
    {
        auto button = stock_ui::Button(root_,name,x,y,w,h);
        auto ctx   = std::make_unique<Action>();
        ctx->owner = this;
        ctx->id    = id;
        lv_obj_add_event_cb(
            button,
            [](lv_event_t* event) {
                auto a            = static_cast<Action*>(lv_event_get_user_data(event));
                a->owner->action_ = a->id;
            },
            LV_EVENT_CLICKED, ctx.get());
        callbacks_.push_back(std::move(ctx));
        return button;
    }
    void ClearPanel()
    {
        LvglLockGuard lock;
        lv_obj_clean(root_);
        callbacks_.clear();
        action_ = 0;
    }
    virtual bool StopResources()
    {
        return true;
    }
    virtual void BeforePanelDelete(bool) {}
    bool Back(int action)
    {
        if (action == 999) exit_pending_ = true;
        if (!exit_pending_) return false;
        if (StopResources()) {
            exit_pending_ = false;
            close();
        }
        return true;
    }

public:
    void onClose() override
    {
        const bool stopped=StopResources();
        BeforePanelDelete(stopped);
        LvglLockGuard lock;
        if (root_) lv_obj_delete(root_);
        root_ = nullptr;
        callbacks_.clear();
        action_ = 0;
    }
};
class CompanionApp : public Page {
    bool StopResources() override
    {
        return companion::desk::StopAndRenderClosing([]{return companion::CloseView();},[]{companion::RenderClosingCompanionView();});
    }
    void BeforePanelDelete(bool stopped) override {companion::desk::BeforeClosingPanelDelete(stopped,[]{companion::DetachClosingCompanionView();});}

public:
    CompanionApp()
    {
        setAppInfo().name = "COMPANION";
        static auto icon  = assets::get_image("icon_ai_agent.bin");
        setAppInfo().icon = &icon;
    }
    void onOpen() override
    {
        Panel("");
        companion::OpenView(root_);
    }
    void onRunning() override
    {
        auto action = action_.exchange(0);
        if(action==999)companion::diagnostic::Stage(companion::diagnostic::Task::Main,companion::diagnostic::StageCode::Back);
        if (Back(action)) return;
        companion::TickView();
    }
};
#if STACKCHAN_BUILD_TOOLBOX
class ToolsApp : public Page {
    using Screen = stock_tools::ui::Screen;
    stock_tools::Infrared ir_;
    stock_tools::NfcUrl nfc_;
    stock_tools::ConfiguredIrBankFiles remote_files_;
    stock_tools::PersistentIrLibrary remotes_{remote_files_};
    std::string remote_device_;
    stock_tools::DemoUart demo_uart_;
    stock_tools::TerminalSession terminal_{demo_uart_};
    size_t remote_page_=0;
    std::string uart_line_="ping";
    Screen screen_ = Screen::Home;
    std::optional<Screen> pending_screen_;
    lv_obj_t *title_ = nullptr, *summary_ = nullptr, *detail_ = nullptr, *status_ = nullptr, *primary_ = nullptr,
             *url_input_ = nullptr;
    std::string url_, draft_, notice_;
    lv_obj_t* graphic_ = nullptr;
    int cursor_        = 0;
    size_t key_page_   = 0;
    uint64_t deadline_ = 0, next_stop_retry_ = 0;
    bool captured_ = false, stop_failed_ = false;
    bool StopResources() override
    {
        ir_.Stop();
        bool nfc_stopped=nfc_.Stop();
        bool uart_stopped=terminal_.Stop();
        stop_failed_ = !nfc_stopped || !uart_stopped;
        return !stop_failed_;
    }
    lv_obj_t* Label(const char* text, int y, int height, const lv_font_t* font = &lv_font_montserrat_20)
    {
        auto label = Text(text, 8, y, 304);
        lv_obj_set_height(label, height);
        lv_label_set_long_mode(label, LV_LABEL_LONG_DOT);
        lv_obj_set_style_text_font(label, font, 0);
        return label;
    }
    lv_obj_t* TouchButton(const char* name, stock_tools::ui::Rect rect, int action)
    {
        auto button = Button(name, rect.x, rect.y, rect.w, rect.h, action);
        lv_obj_set_style_radius(button, 10, 0);
        lv_obj_set_style_bg_color(button, lv_color_hex(stock_ui::ButtonFill), 0);
        lv_obj_set_style_border_width(button, 1, 0);
        lv_obj_set_style_border_color(button, lv_color_hex(stock_ui::Muted), 0);
        lv_obj_set_style_text_color(button, lv_color_hex(stock_ui::Ink), 0);
        lv_obj_set_style_text_font(button, &lv_font_montserrat_20, 0);
        lv_obj_set_style_opa(button, LV_OPA_50, LV_STATE_DISABLED);
        return button;
    }
    lv_obj_t* Graphic(lv_obj_t* parent, int x, int y, bool tag, bool small = false)
    {
        auto graphic = lv_obj_create(parent);
        lv_obj_set_pos(graphic, x, y);
        lv_obj_set_size(graphic, tag ? (small ? 32 : 52) : (small ? 28 : 36), small ? 40 : 56);
        lv_obj_set_style_pad_all(graphic, 0, 0);
        lv_obj_set_style_radius(graphic, tag ? 10 : 6, 0);
        lv_obj_set_style_bg_opa(graphic, LV_OPA_TRANSP, 0);
        lv_obj_set_style_border_width(graphic, 2, 0);
        lv_obj_set_style_border_color(graphic, lv_color_hex(stock_ui::Accent), 0);
        lv_obj_remove_flag(graphic, static_cast<lv_obj_flag_t>(LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE));
        auto piece = [&](int px, int py, int w, int h) {
            auto child = lv_obj_create(graphic);
            lv_obj_set_pos(child, px, py);
            lv_obj_set_size(child, w, h);
            lv_obj_set_style_pad_all(child, 0, 0);
            lv_obj_set_style_border_width(child, 0, 0);
            lv_obj_set_style_radius(child, 2, 0);
            lv_obj_set_style_bg_color(child, lv_color_hex(stock_ui::Accent), 0);
            lv_obj_remove_flag(child, static_cast<lv_obj_flag_t>(LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE));
        };
        if (tag) {
            piece(5, 5, 6, 6);
            piece(12, small ? 16 : 20, small ? 12 : 28, 3);
            piece(12, small ? 23 : 28, small ? 12 : 22, 3);
        } else {
            piece(5, 5, small ? 14 : 22, 8);
            for (int i = 0; i < 6; ++i)
                piece(5 + (i % 2) * (small ? 10 : 14), 19 + (i / 2) * (small ? 6 : 9), small ? 6 : 8, 4);
        }
        return graphic;
    }
    void ButtonText(lv_obj_t* button, const char* text)
    {
        lv_label_set_text(lv_obj_get_child(button, 0), text);
    }
    bool ValidUrl() const
    {
        std::array<uint8_t, 180> memory{};
        std::array<uint8_t, 7> uid{};
        return stock_tools::UrlTag(url_, uid, memory);
    }
    std::string SignalSummary() const
    {
        if (ir_.learned.empty()) return "No signal captured";
        uint32_t value = 0;
        char text[80];
        if (stock_tools::NecDecode(ir_.learned, value))
            std::snprintf(text, sizeof(text), "NEC %08lX\n%u timings", static_cast<unsigned long>(value),
                          static_cast<unsigned>(ir_.learned.size()));
        else
            std::snprintf(text, sizeof(text), "Raw IR signal\n%u timings", static_cast<unsigned>(ir_.learned.size()));
        return text;
    }
    void Header(const char* title)
    {
        ClearPanel();
        summary_ = detail_ = status_ = primary_ = url_input_ = graphic_ = nullptr;
        lv_obj_set_style_text_font(root_, &lv_font_montserrat_20, 0);
        lv_obj_set_style_bg_color(root_, lv_color_hex(stock_ui::Canvas), 0);
        lv_obj_set_style_text_color(root_, lv_color_hex(stock_ui::Ink), 0);
        TouchButton("Back", stock_tools::ui::BackButton, screen_ == Screen::Home ? 999 : stock_tools::ui::Back);
        title_ = Text(title, 104, 12, 208);
        lv_obj_set_height(title_, 32);
        lv_obj_set_style_text_font(title_, &lv_font_montserrat_24, 0);
        lv_label_set_long_mode(title_, LV_LABEL_LONG_DOT);
    }
    void Render()
    {
        using namespace stock_tools::ui;
        LvglLockGuard lock;
        switch (screen_) {
            case Screen::Home:
                Header("Tools");
                {
                    auto b = TouchButton("Infrared", ListRows[0], OpenInfrared);
                    lv_obj_align(lv_obj_get_child(b, 0), LV_ALIGN_LEFT_MID, 16, 0);
                    Graphic(b, 260, 2, false, true);
                }
                {
                    auto b = TouchButton("NFC URL", ListRows[1], OpenNfc);
                    lv_obj_align(lv_obj_get_child(b, 0), LV_ALIGN_LEFT_MID, 16, 0);
                    Graphic(b, 254, 2, true, true);
                }
                TouchButton("UART terminal",ListRows[2],OpenUart);
                Label("Actions start only when tapped", 212, 24, &lv_font_montserrat_16);
                break;
            case Screen::Infrared:
                Header("Infrared");
                TouchButton("Learn a signal", ListRows[0], OpenLearn);
                TouchButton("Review & send", ListRows[1], OpenSend);
                TouchButton("Saved remotes",ListRows[2],OpenLibrary);
                Label("Named commands stored on SD", 212, 24, &lv_font_montserrat_16);
                break;
            case Screen::Learn:
                Header("Learn IR");
                summary_ = Label("Ready to learn", 60, 48);
                lv_obj_set_width(summary_, 232);
                graphic_ = Graphic(root_, 270, 60, false);
                detail_  = Label("Aim the remote at the IR sensor.\nPress its button after starting.", 108, 38,
                                 &lv_font_montserrat_16);
                status_  = Label("IR off", 148, 24);
                primary_ = TouchButton("Start 10s scan", Primary, StartLearn);
                break;
            case Screen::Send: {
                Header("Send IR");
                summary_ = Label(SignalSummary().c_str(), 60, 52);
                lv_obj_set_width(summary_, 232);
                graphic_ = Graphic(root_, 270, 60, false);
                std::string send_detail="Aim at device. "+std::to_string(ir_.carrier_hz/1000)+"kHz, once.";
                detail_  = Label(send_detail.c_str(), 120, 24, &lv_font_montserrat_16);
                status_  = Label("Nothing sent yet", 148, 24, &lv_font_montserrat_16);
                {
                    auto save=TouchButton("Save remote",SplitPrimary[0],SaveRemote);
                    lv_obj_set_style_text_font(save,&lv_font_montserrat_16,0);
                    if(!stock_tools::ValidSignal(ir_.learned) || remotes_.Library().Records().size()>=stock_tools::NamedIrLibrary::MaxRecords)
                        lv_obj_add_state(save,LV_STATE_DISABLED);
                }
                primary_ = TouchButton("Send once", SplitPrimary[1], SendOnce);
                if (ir_.learned.empty()) lv_obj_add_state(primary_, LV_STATE_DISABLED);
                break;
            }
            case Screen::Library: {
                Header("Saved remotes");
                const auto& records=remotes_.Library().Records();
                for(size_t slot=0;slot<2;++slot){
                    auto index=remote_page_*2+slot;
                    std::string name=index<records.size()?records[index].device+" / "+records[index].command:"Empty slot";
                    auto b=TouchButton(name.c_str(),RemoteRows[slot],
                                       ChooseRemote+static_cast<int>(index));
                    lv_obj_set_style_text_font(b,&lv_font_montserrat_16,0);
                    auto label=lv_obj_get_child(b,0);lv_obj_set_width(label,272);lv_label_set_long_mode(label,LV_LABEL_LONG_DOT);lv_obj_center(label);
                    if(index>=records.size())lv_obj_add_state(b,LV_STATE_DISABLED);
                }
                status_=Label(stock_tools::IrStoreMessage(remotes_.Status()),152,20,&lv_font_montserrat_16);
                TouchButton("Next page",Primary,NextRemotes);
                break;
            }
            case Screen::Nfc:
                Header("NFC URL");
                summary_ = Label(url_.empty() ? "No URL - tap Edit" : url_.c_str(), 56, 40);
                lv_obj_set_width(summary_, 248);
                graphic_ = Graphic(root_, 276, 54, true, true);
                TouchButton("Edit URL", EditButton, OpenEdit);
                primary_ = TouchButton("Emulate 60s", EmulateButton, StartEmulate);
                if (!ValidUrl()) lv_obj_add_state(primary_, LV_STATE_DISABLED);
                break;
            case Screen::Edit:
            case Screen::IrDeviceEdit:
            case Screen::IrCommandEdit:
            case Screen::UartEdit: {
                const bool uart=screen_==Screen::UartEdit;
                const bool ir_name=screen_==Screen::IrDeviceEdit || screen_==Screen::IrCommandEdit;
                Header(ir_name?(screen_==Screen::IrDeviceEdit?"Device name":"Command name"):uart?"Demo line":"Edit URL");
                url_input_ = lv_textarea_create(root_);
                lv_obj_set_pos(url_input_, UrlInput.x, UrlInput.y);
                lv_obj_set_size(url_input_, UrlInput.w, UrlInput.h);
                lv_textarea_set_one_line(url_input_, true);
                lv_textarea_set_max_length(url_input_, ir_name?24:uart?64:96);
                lv_textarea_set_placeholder_text(url_input_, ir_name?"TV, Amp / Power, Volume":uart?"Printable text":"https://");
                lv_textarea_set_text(url_input_, draft_.c_str());
                lv_textarea_set_cursor_pos(url_input_, cursor_);
                auto keys=(uart || ir_name)?UartKeyPages[key_page_]:KeyPages[key_page_];
                for (size_t i = 0; i < keys.size(); ++i) {
                    char text[2] = {keys[i], 0};
                    TouchButton(text, KeyRect(static_cast<int>(i)), Character + static_cast<unsigned char>(text[0]));
                }
                char page[20];
                std::snprintf(page, sizeof(page), "Keys %u/%u", static_cast<unsigned>(key_page_ + 1),
                              static_cast<unsigned>((uart || ir_name)?UartKeyPages.size():KeyPages.size()));
                auto next = TouchButton(page, EditorFooter[0], NextKeys);
                lv_obj_set_style_text_font(next, &lv_font_montserrat_16, 0);
                TouchButton("<--", EditorFooter[1], Erase);
                TouchButton(screen_==Screen::IrDeviceEdit?"Next":uart?"Use line":"Save", EditorFooter[2], Save);
                break;
            }
            case Screen::Uart:
                Header("UART demo");
                summary_=Label("Grove IO unavailable",52,24,&lv_font_montserrat_16);
                detail_=Label(terminal_.Text().empty()?"Local echo only. No pins opened.":terminal_.Preview().c_str(),80,56,
                              &lv_font_montserrat_16);
                status_=Label("Demo stopped",140,24,&lv_font_montserrat_16);
                {
                    auto edit=TouchButton("Edit line",SplitPrimary[0],OpenUartEdit);
                    lv_obj_set_style_text_font(edit,&lv_font_montserrat_16,0);
                }
                primary_=TouchButton(terminal_.Active()?"Send demo":"Start demo",SplitPrimary[1],
                                     terminal_.Active()?SendDemo:StartDemo);
                lv_obj_set_style_text_font(primary_,&lv_font_montserrat_16,0);
                break;
            case Screen::Emulate:
                Header("Emulate");
                summary_ = Label("Hold a reader near\nthe NFC antenna.", 60, 52);
                lv_obj_set_width(summary_, 232);
                graphic_ = Graphic(root_, 254, 60, true);
                status_  = Label("NFC off", 120, 32, &lv_font_montserrat_24);
                detail_  = Label("Stops automatically after 60s", 152, 20, &lv_font_montserrat_16);
                primary_ = TouchButton("Emulate 60s", Primary, StartEmulate);
                break;
        }
        if (!notice_.empty() && status_) lv_label_set_text(status_, notice_.c_str());
    }
    bool Go(Screen target, uint64_t now)
    {
        pending_screen_ = target;
        if (!StopResources()) {
            next_stop_retry_ = now + 500;
            notice_          = "Stop failed - retry Back";
            Refresh(now);
            return false;
        }
        pending_screen_.reset();
        screen_ = target;
        notice_.clear();
        deadline_ = 0;
        Render();
        return true;
    }
    void Refresh(uint64_t now)
    {
        using namespace stock_tools::ui;
        LvglLockGuard lock;
        if (graphic_)
            lv_obj_set_style_border_color(graphic_, lv_color_hex((ir_.Active() || nfc_.Active()) ? 0x69e3a7 : 0x53d4d4),
                                          0);
        if (!status_) return;
        if (stop_failed_) {
            lv_label_set_text(status_, "Stop failed - retry Back");
            if (primary_) {
                ButtonText(primary_, "Retry Stop");
                lv_obj_remove_state(primary_, LV_STATE_DISABLED);
            }
            return;
        }
        if (screen_ == Screen::Learn) {
            if (ir_.Active()) {
                char text[40];
                auto remaining = deadline_ > now ? (deadline_ - now + 999) / 1000 : 0;
                std::snprintf(text, sizeof(text), "Listening - %lus left", static_cast<unsigned long>(remaining));
                lv_label_set_text(summary_, "Press the remote button");
                lv_label_set_text(status_, text);
                ButtonText(primary_, "Stop scan");
            } else if (captured_) {
                lv_label_set_text(summary_, SignalSummary().c_str());
                lv_label_set_text(detail_, "Captured. Review before sending.");
                lv_label_set_text(status_, "IR off");
                ButtonText(primary_, "Review & send");
            } else {
                lv_label_set_text(status_, notice_.empty() ? "IR off" : notice_.c_str());
                ButtonText(primary_, "Start 10s scan");
            }
        } else if (screen_ == Screen::Emulate) {
            if (nfc_.Active()) {
                char text[40];
                auto remaining = deadline_ > now ? (deadline_ - now + 999) / 1000 : 0;
                std::snprintf(text, sizeof(text), "NFC active - %lus", static_cast<unsigned long>(remaining));
                lv_label_set_text(status_, text);
                ButtonText(primary_, "Stop emulation");
            } else {
                lv_label_set_text(status_, notice_.empty() ? "NFC off" : notice_.c_str());
                ButtonText(primary_, "Emulate 60s");
            }
        } else if(screen_==Screen::Uart){
            lv_label_set_text(detail_,terminal_.Text().empty()?"Local echo only. No pins opened.":terminal_.Preview().c_str());
            bool restoring=terminal_.State()==stock_tools::TerminalState::Restoring;
            ButtonText(primary_,restoring?"Retry Stop":terminal_.Active()?"Send demo":"Start demo");
            if(restoring)lv_label_set_text(status_,"Stop pending - retry Back");
            else if(!notice_.empty())lv_label_set_text(status_,notice_.c_str());
            else lv_label_set_text(status_,terminal_.Active()?"Local demo, 60s maximum":"Demo stopped");
        } else if (!notice_.empty())
            lv_label_set_text(status_, notice_.c_str());
    }

public:
    ToolsApp()
    {
        setAppInfo().name = "TOOLS";
        static auto icon  = assets::get_image("icon_controller.bin");
        setAppInfo().icon = &icon;
    }
    void onOpen() override
    {
        screen_ = Screen::Home;
        url_.clear();
        draft_.clear();
        notice_.clear();
        pending_screen_.reset();
        remotes_.Load();
        terminal_.Clear();
        remote_page_=0;
        uart_line_="ping";
        captured_ = stop_failed_ = false;
        Panel("");
        Render();
    }
    void onRunning() override
    {
        using namespace stock_tools::ui;
        auto action = action_.exchange(0);
        auto now    = static_cast<uint64_t>(esp_timer_get_time() / 1000);
        if (screen_ == Screen::Home && Page::Back(action == stock_tools::ui::Back ? 999 : action)) return;
        if (pending_screen_) {
            if (now >= next_stop_retry_) Go(*pending_screen_, now);
            return;
        }
        if (action && !Allowed(screen_, action)) action = 0;
        if (action == stock_tools::ui::Back) {
            Go(Parent(screen_), now);
            return;
        }
        if (action == OpenInfrared) {
            Go(Screen::Infrared, now);
            return;
        }
        if (action == OpenNfc) {
            Go(Screen::Nfc, now);
            return;
        }
        if(action==OpenLibrary){remote_page_=0;Go(Screen::Library,now);return;}
        if(action==OpenUart){Go(Screen::Uart,now);return;}
        if(action==NextRemotes){remote_page_=(remote_page_+1)%std::max(size_t(1),(remotes_.Library().Records().size()+1)/2);Render();return;}
        if(screen_==Screen::Library && action>=ChooseRemote && action<ChooseRemote+64){
            auto index=static_cast<size_t>(action-ChooseRemote);
            if(index<remotes_.Library().Records().size()){
                ir_.learned=remotes_.Library().Records()[index].pulses;
                ir_.carrier_hz=remotes_.Library().Records()[index].carrier_hz;
                if(Go(Screen::Send,now)){notice_="Loaded; nothing sent";Refresh(now);}
            }
            return;
        }
        if (action == OpenLearn) {
            captured_ = false;
            Go(Screen::Learn, now);
            return;
        }
        if (action == OpenSend) {
            Go(Screen::Send, now);
            return;
        }
        if (action == OpenEdit || action==OpenUartEdit) {
            draft_    = action==OpenUartEdit?uart_line_:url_;
            key_page_ = 0;
            cursor_   = static_cast<int>(draft_.size());
            Go(action==OpenUartEdit?Screen::UartEdit:Screen::Edit, now);
            return;
        }
        if (screen_ == Screen::Edit || screen_==Screen::UartEdit || screen_==Screen::IrDeviceEdit || screen_==Screen::IrCommandEdit) {
            if (action >= Character + 32 && action <= Character + 126) {
                LvglLockGuard lock;
                lv_textarea_add_char(url_input_, static_cast<uint32_t>(action - Character));
            }
            if (action == Erase) {
                LvglLockGuard lock;
                lv_textarea_delete_char(url_input_);
            }
            if (action == NextKeys) {
                {
                    LvglLockGuard lock;
                    draft_  = lv_textarea_get_text(url_input_);
                    cursor_ = lv_textarea_get_cursor_pos(url_input_);
                }
                key_page_ = (key_page_ + 1) % ((screen_==Screen::UartEdit || screen_==Screen::IrDeviceEdit || screen_==Screen::IrCommandEdit)?UartKeyPages.size():KeyPages.size());
                Render();
            }
            if (action == Save) {
                std::string candidate;
                {
                    LvglLockGuard lock;
                    candidate = lv_textarea_get_text(url_input_);
                }
                if(screen_==Screen::IrDeviceEdit || screen_==Screen::IrCommandEdit){
                    if(!stock_tools::NamedIrLibrary::ValidName(candidate)){
                        LvglLockGuard lock;lv_label_set_text(title_,"Letters, numbers, space ._- only");return;
                    }
                    if(screen_==Screen::IrDeviceEdit){
                        remote_device_=candidate;draft_.clear();key_page_=0;cursor_=0;Go(Screen::IrCommandEdit,now);return;
                    }
                    const bool saved=remotes_.Save({remote_device_,candidate,ir_.carrier_hz,ir_.learned});
                    Go(Screen::Send,now);
                    notice_=saved?"Saved on SD":remotes_.Library().Find(remote_device_,candidate)?"Name exists; choose another":stock_tools::IrStoreMessage(remotes_.Status());
                    Refresh(now);return;
                }
                if(screen_==Screen::UartEdit){uart_line_=candidate;Go(Screen::Uart,now);return;}
                std::array<uint8_t, 180> memory{};
                std::array<uint8_t, 7> uid{};
                if (stock_tools::UrlTag(candidate, uid, memory)) {
                    url_ = candidate;
                    Go(Screen::Nfc, now);
                } else {
                    LvglLockGuard lock;
                    lv_label_set_text(title_, "Use http(s) URL");
                    lv_obj_set_style_text_font(title_, &lv_font_montserrat_20, 0);
                }
            }
            return;
        }
        if (action == StartLearn && screen_ == Screen::Learn) {
            if (stop_failed_) {
                StopResources();
                notice_ = "IR off";
            } else if (ir_.Active()) {
                ir_.Stop();
                notice_ = "Scan stopped";
            } else if (captured_) {
                Go(Screen::Send, now);
                return;
            } else {
                captured_ = false;
                notice_   = ir_.Learn(now) ? "" : "IR start failed";
                deadline_ = now + 10000;
            }
        }
        if (action == SendOnce && screen_ == Screen::Send)
            notice_ = ir_.SendOnce() ? "Sent once" : "Send failed / no signal";
        if(action==SaveRemote && screen_==Screen::Send){
            if(!remote_files_.Available()){notice_="SD not mounted";}
            else if(remotes_.Library().Records().size()>=stock_tools::NamedIrLibrary::MaxRecords){notice_="IR library full";}
            else{draft_=remote_device_;key_page_=0;cursor_=static_cast<int>(draft_.size());Go(Screen::IrDeviceEdit,now);return;}
        }
        if(screen_==Screen::Uart && (action==StartDemo || action==SendDemo)){
            if(stop_failed_ || terminal_.State()==stock_tools::TerminalState::Restoring)
                notice_=StopResources()?"Demo stopped":"Stop failed - retry Back";
            else if(!terminal_.Active()){
                notice_=terminal_.Start({stock_tools::GrovePort::A,115200},now)?"Local demo only":"Demo start failed";
                Render();
            } else notice_=terminal_.Send(uart_line_)?"Demo line sent":"Line rejected - edit or Back";
        }
        if (action == StartEmulate) {
            if (stop_failed_) {
                StopResources();
                notice_ = "NFC off";
            } else if (nfc_.Active()) {
                notice_ = StopResources() ? "NFC stopped" : "Stop failed";
            } else if (screen_ == Screen::Nfc || screen_ == Screen::Emulate) {
                if (screen_ == Screen::Nfc && !Go(Screen::Emulate, now)) return;
                bool started = nfc_.Start(url_, now);
                notice_      = started ? "" : "NFC start failed";
                if (!started) StopResources();
                deadline_ = now + 60000;
            }
        }
        if (action == Stop) notice_ = StopResources() ? "Stopped" : "Stop failed";
        bool learning = ir_.Active();
        if (ir_.Poll(now)) {
            captured_ = true;
            notice_   = "Captured";
        } else if (learning && !ir_.Active())
            notice_ = "No signal - try again";
        bool emitting = nfc_.Active();
        nfc_.Poll(now);
        bool uart_active=terminal_.Active();
        terminal_.Poll(now);
        if(uart_active && !terminal_.Active())notice_="Demo ended - no GPIO IO";
        if (emitting && !nfc_.Active()) {
            stop_failed_ = !nfc_.Stop();
            notice_      = stop_failed_ ? "Stop failed" : "Window ended - NFC off";
        }
        Refresh(now);
    }
    void onClose() override
    {
        Page::onClose();
        ir_.learned.clear();
        url_.clear();
        draft_.clear();
        // Durable library remains on SD; the next Tools open reloads it.
        terminal_.Clear();
        uart_line_.clear();
        pending_screen_.reset();
        title_ = summary_ = detail_ = status_ = primary_ = url_input_ = graphic_ = nullptr;
    }
};
#endif
#if STACKCHAN_BUILD_GAMES
class GamesApp : public Page {
    stock_tools::Game2048 game_;
    local_tools_ui::Model dice_;
    local_tools_ui::GameRoute route_=local_tools_ui::GameRoute::Collection;
    std::array<lv_obj_t*, 16> tiles_{};
    std::array<lv_obj_t*,7> dice_choices_{};
    lv_obj_t* score_ = nullptr;
    void Render()
    {
        if(route_==local_tools_ui::GameRoute::Collection)return;
        LvglLockGuard lock;
        if(route_==local_tools_ui::GameRoute::Dice){char text[128];if(dice_.dice_failed)std::snprintf(text,sizeof(text),"%ud%u%+d: RNG rejected; tap Roll",dice_.dice_count,dice_.sides,dice_.dice_modifier);else if(dice_.dice_rolled)std::snprintf(text,sizeof(text),"%ud%u%+d = %d",dice_.dice_count,dice_.sides,dice_.dice_modifier,dice_.dice_result.total);else std::snprintf(text,sizeof(text),"%ud%u%+d: tap Roll",dice_.dice_count,dice_.sides,dice_.dice_modifier);lv_label_set_text(score_,text);const unsigned sides[]={4,6,8,10,12,20,100};for(unsigned i=0;i<7;++i)if(dice_choices_[i])stock_ui::Active(dice_choices_[i],dice_.sides==sides[i]);return;}
        for (int i = 0; i < 16; ++i) {
            char text[8];
            if (game_.cells[i])
                std::snprintf(text, sizeof(text), "%u", game_.cells[i]);
            else
                text[0] = 0;
            lv_label_set_text(tiles_[i], text);
            auto cell=lv_obj_get_parent(tiles_[i]);lv_obj_set_style_bg_color(cell,lv_color_hex(game_.cells[i]?stock_ui::Accent:stock_ui::Panel),0);lv_obj_set_style_text_color(cell,lv_color_hex(game_.cells[i]?stock_ui::Canvas:stock_ui::Ink),0);
        }
        char text[64];
        std::snprintf(text, sizeof(text), "2048 | %lu%s", static_cast<unsigned long>(game_.score),
                      game_.CanMove() ? "" : " | Game over");
        lv_label_set_text(score_, text);
    }

public:
    GamesApp()
    {
        setAppInfo().name = "GAMES";
        static auto icon  = assets::get_image("icon_dance.bin");
        setAppInfo().icon = &icon;
    }
    void Page()
    {
        ClearPanel();
        {LvglLockGuard lock;
         Text(route_==local_tools_ui::GameRoute::Collection?"Games":route_==local_tools_ui::GameRoute::Dice?"DnD dice":"2048",10,6,240);
         Button(route_==local_tools_ui::GameRoute::Collection?"Close":"Back",260,2,56,40,999);
         if(route_==local_tools_ui::GameRoute::Collection){for(size_t i=0;i<local_tools_ui::GameCollection.size();++i){auto entry=local_tools_ui::GameCollection[i];Button(entry.label,10+(i%2)*156,54+(i/2)*52,140,44,entry.action);}score_=nullptr;return;}
         if(route_==local_tools_ui::GameRoute::Dice){const unsigned sides[]={4,6,8,10,12,20,100};for(unsigned i=0;i<7;++i){char text[12];std::snprintf(text,sizeof(text),"d%u",sides[i]);dice_choices_[i]=Button(text,8+(i%4)*76,44+(i/4)*42,70,40,100+int(sides[i]));}Button("Count -",8,130,72,40,51);Button("Count +",84,130,72,40,52);Button("Mod -",160,130,72,40,53);Button("Mod +",236,130,72,40,54);auto result=stock_ui::Card(root_,8,176,140,62);score_=stock_ui::Text(result,"",8,8,124);lv_label_set_long_mode(score_,LV_LABEL_LONG_WRAP);Button("Roll",160,184,152,44,10);}
        }
        if(route_==local_tools_ui::GameRoute::Game2048)
        {
            LvglLockGuard lock;
            score_ = Text("", 8, 32, 196);lv_obj_set_height(score_,18);lv_label_set_long_mode(score_,LV_LABEL_LONG_SCROLL_CIRCULAR);
            for (int y = 0; y < 4; ++y)
                for (int x = 0; x < 4; ++x) {
                    auto cell = lv_obj_create(root_);
                    lv_obj_set_pos(cell, 8 + x * 47, 54 + y * 40);
                    lv_obj_set_size(cell, 44, 37);
                    lv_obj_set_style_pad_all(cell,0,0);lv_obj_set_style_border_width(cell,0,0);lv_obj_set_style_radius(cell,4,0);lv_obj_set_style_text_font(cell,&lv_font_montserrat_14,0);
                    lv_obj_remove_flag(cell, LV_OBJ_FLAG_SCROLLABLE);
                    tiles_[y * 4 + x] = lv_label_create(cell);
                    lv_obj_center(tiles_[y * 4 + x]);
                }
            Button("Up", 224, 54, 82, 40, 3);
            Button("Left", 205, 100, 54, 40, 1);
            Button("Right", 261, 100, 54, 40, 2);
            Button("Down", 224, 146, 82, 40, 4);
            Button("New", 224, 190, 82, 40, 5);
        }
        Render();
    }
    void onOpen() override {Panel("Games");route_=local_tools_ui::GameRoute::Collection;game_.Reset();Page();}
    void onRunning() override
    {
        auto action = action_.exchange(0);
        if(action==999&&route_!=local_tools_ui::GameRoute::Collection){route_=local_tools_ui::GameRoute::Collection;Page();return;}
        if (Back(action)) return;
        if(route_==local_tools_ui::GameRoute::Collection){auto next=local_tools_ui::SelectGame(action);if(next!=route_){route_=next;Page();}return;}
        if(route_==local_tools_ui::GameRoute::Dice){if(action>=102&&action<=1100)dice_.Die(unsigned(action-100));else if(action==10)dice_.RollDice([]{return esp_random();});else if(action==51||action==52)dice_.Count(action==52?1:-1);else if(action==53||action==54)dice_.Modifier(action==54?1:-1);if(action)Render();return;}
        if (action == 5) {
            game_.Reset();
            Render();
        } else if (action >= 1 && action <= 4) {
            if (game_.Move(action - 1)) game_.AddTile(esp_random());
            Render();
        }
    }
};
#endif
void DestroyShellForTransfer(mooncake::Mooncake& shell)
{
    while(!TearDownShell<LvglLockGuard>(shell,[]{
        view::destroy_home_indicator();
        view::destroy_status_bar();
        view::reset_toast_runtime();
    },[]{mooncake::DestroyMooncake();})){
        // TearDownShell released the UI lock; actor/codec owners can make progress.
        GetHAL().feedTheDog();GetHAL().delay(20);
    }
}
[[noreturn]] void Run()
{
    if (!GetHAL().initCompanion())
        while (true) GetHAL().delay(1000);
    smooth_ui_toolkit::ui_hal::on_delay([](uint32_t ms) { GetHAL().delay(ms); });
    smooth_ui_toolkit::ui_hal::on_get_tick([]() { return GetHAL().millis(); });
    while(true){
    auto& shell = mooncake::GetMooncake();
    auto launcher_owned = std::make_unique<AppLauncher>();
    auto* launcher = launcher_owned.get();
    shell.installApp(std::move(launcher_owned));
    // Official 1.5.1 launcher order; setup workers are children of SETUP.
    int stock_agent_id=shell.installApp(stock_agent::MakeApp());
    std::map<stock_services::Feature,int> native_ids;
    using Feature=stock_services::Feature;
    for(auto feature:LauncherNativeFeatures) native_ids[feature]=shell.installApp(stock_services::MakeApp(feature));
    auto setup_owned=std::make_unique<AppSetup>();auto* setup=setup_owned.get();
    int setup_id=shell.installApp(std::move(setup_owned));
    shell.installApp(stock_tools::MakeJointCompanionApp());
#if STACKCHAN_BUILD_PROVIDERS
    for(const auto& provider:companion_modes::Catalog)
        if(provider.route==companion_modes::Route::AdapterRequired)
            shell.installApp(stock_tools::MakeProviderCompanionApp(provider.provider));
#endif
    // USB Mira is part of Mira, not a separate launcher entry.
#if STACKCHAN_BUILD_LED_CONTROLS
    shell.installApp(stock_tools::MakeLedExpressionApp());
#endif
#if STACKCHAN_BUILD_TOOLBOX
    shell.installApp(std::make_unique<ToolsApp>());
#endif
#if STACKCHAN_BUILD_GAMES
    shell.installApp(std::make_unique<GamesApp>());
#endif
    shell.installApp(stock_tools::MakeSdStatusApp());
#if STACKCHAN_BUILD_GAMES
    shell.installApp(local_tools_ui::MakePetApp());
#endif
#if STACKCHAN_BUILD_MUSIC
    shell.installApp(local_tools_ui::MakeAudioToolsApp());
    shell.installApp(instrument_ui::MakeApp(instrument_ui::Kind::Synth));
    shell.installApp(instrument_ui::MakeApp(instrument_ui::Kind::Groovebox));
    shell.installApp(instrument_ui::MakeApp(instrument_ui::Kind::Sampler));
#endif
#if STACKCHAN_BUILD_MUSIC || STACKCHAN_BUILD_TOOLBOX
    shell.installApp(usb_composite_v2::MakeControlsApp());
#endif
#if STACKCHAN_BUILD_NETWORK_TOOLS
    shell.installApp(stock_tools::MakeProximityExpressionApp());
    shell.installApp(stock_tools::MakeNetworkToolsApp());
#endif
    bool stopped_preview=false;
    bool setup_return=ConsumeSetupReturn(); // consume even when a more specific preview wins
    if(stock_agent::ConsumePreviewRequest()){stopped_preview=true;launcher->openApp(stock_agent_id);}
    if(network_setup::ConsumePreviewRequest()){
        stopped_preview=true;setup->openInitialWorker([]{return network_setup::MakeWorker();});launcher->openApp(setup_id);
    }
    if(auto feature=stock_services::ConsumePreviewRequest()){
        stopped_preview=true;
        if(IsSetupFeature(*feature)){
            if(*feature==Feature::Rgb)setup->openInitialWorker([feature]{return stock_services::MakeWorker(*feature);});
            launcher->openApp(setup_id);
        }else if(*feature==Feature::Onboarding){
            setup->openInitialWorker([feature]{return stock_services::MakeWorker(*feature);});launcher->openApp(setup_id);
        }else{
            auto destination=native_ids.find(*feature);
            if(destination!=native_ids.end())launcher->openApp(destination->second);
        }
    }
    if(!stopped_preview&&setup_return){stopped_preview=true;launcher->openApp(setup_id);}
    // Setup is an explicit Settings action, including after a watchdog/panic.
    // A reset must never be mistaken for consent to the Welcome motor test.
    if (!stopped_preview && stock_agent::AutoStartRequested()) {
        stock_agent::AuthorizeAutoStart();
        DestroyShellForTransfer(shell);
        stock_agent::RunStockAgent();continue;
    }
    while (true) {
        GetHAL().feedTheDog();
        if(!runtime_lifecycle::HomeRequested())shell.update();
        if(runtime_lifecycle::HomeRequested()){
            auto& session=runtime_lifecycle::Session();
            if(session.Poll(uint64_t(esp_timer_get_time()/1000),[]{runtime_lifecycle::QuietHardware();},
                [&]{return runtime_lifecycle::PollSourcesStopped(&shell);},
                []{return companion::CurrentAudioOwner()==companion::AudioOwner::None;},
                [&]{runtime_lifecycle::FinalizeNativeUi(true);DestroyShellForTransfer(shell);runtime_lifecycle::FinalizeNativeUi(false);SetSetupContext(false);companion::SetSettingsScope(companion::SettingsScope::None);return true;}))break;
            GetHAL().delay(20);continue;
        }
        if(ConsumeSettingsRequest())launcher->openApp(setup_id);
        led_expression::Tick(static_cast<uint64_t>(esp_timer_get_time()/1000));
        if(auto feature=stock_services::ConsumeRequest()) {
            DestroyShellForTransfer(shell);
            companion::SetSettingsScope((*feature==Feature::Servo || *feature==Feature::Onboarding) ? companion::SettingsScope::Calibration :
                *feature==Feature::Account ? companion::SettingsScope::Account :
                (*feature==Feature::Avatar || *feature==Feature::EzData || *feature==Feature::AppCenter || *feature==Feature::FirmwareCheck) ? companion::SettingsScope::RemoteService : companion::SettingsScope::None);
            SetSetupContext(IsSetupFeature(*feature));
            stock_services::RunFeature(*feature);
            break; // reconstruct Home with a fresh Mooncake generation
        }
        if (stock_agent::ConsumeStartRequest()) {
            DestroyShellForTransfer(shell);
            stock_agent::RunStockAgent();
            break; // old shell/launcher/setup pointers are no longer valid
        }
        GetHAL().delay(20);
    }
    }
}
}  // namespace stock_shell
