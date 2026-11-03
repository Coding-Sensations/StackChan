#pragma once
#include <array>
#include <string_view>
namespace stock_tools::ui {
enum class Screen { Home, Infrared, Learn, Send, Library, Nfc, Edit, Emulate, Uart, UartEdit, IrDeviceEdit, IrCommandEdit };
enum Action {
    Back         = 900,
    OpenInfrared = 101,
    OpenNfc      = 102,
    OpenLearn    = 103,
    OpenSend     = 104,
    OpenEdit     = 105,
    OpenUart     = 106,
    OpenUartEdit = 107,
    OpenLibrary  = 108,
    StartLearn   = 1,
    SendOnce     = 2,
    StartEmulate = 4,
    Stop         = 5,
    StartDemo    = 6,
    SendDemo     = 7,
    SaveRemote   = 8,
    NextRemotes  = 213,
    ChooseRemote = 500,
    NextKeys     = 210,
    Erase        = 211,
    Save         = 212,
    Character    = 300
};
inline Screen Parent(Screen screen)
{
    switch (screen) {
        case Screen::Learn:
        case Screen::Send:
        case Screen::Library:
            return Screen::Infrared;
        case Screen::Edit:
        case Screen::Emulate:
            return Screen::Nfc;
        case Screen::IrDeviceEdit:
        case Screen::IrCommandEdit:
            return Screen::Send;
        case Screen::UartEdit:
            return Screen::Uart;
        default:
            return Screen::Home;
    }
}
inline bool Allowed(Screen screen, int action)
{
    if (action == Back) return true;
    switch (screen) {
        case Screen::Home:
            return action == OpenInfrared || action == OpenNfc || action == OpenUart;
        case Screen::Infrared:
            return action == OpenLearn || action == OpenSend || action == OpenLibrary;
        case Screen::Learn:
            return action == StartLearn || action == Stop || action == OpenSend;
        case Screen::Send:
            return action == SendOnce || action == SaveRemote;
        case Screen::Library:
            return action == NextRemotes || (action >= ChooseRemote && action < ChooseRemote+64);
        case Screen::Nfc:
            return action == OpenEdit || action == StartEmulate;
        case Screen::Edit:
            return action == NextKeys || action == Erase || action == Save ||
                   (action >= Character + 33 && action <= Character + 126);
        case Screen::IrDeviceEdit:
        case Screen::IrCommandEdit:
        case Screen::UartEdit:
            return action == NextKeys || action == Erase || action == Save ||
                   (action >= Character + 32 && action <= Character + 126);
        case Screen::Emulate:
            return action == StartEmulate || action == Stop;
        case Screen::Uart:
            return action == OpenUartEdit || action == StartDemo || action == SendDemo || action == Stop;
    }
    return false;
}
inline constexpr std::array<std::string_view, 8> KeyPages = {"abcdefghij", "klmnopqrst", "uvwxyz./:-", "0123456789",
                                                             "ABCDEFGHIJ", "KLMNOPQRST", "UVWXYZ?=&_", "%#~+!$()*;"};
inline constexpr std::array<std::string_view,10> UartKeyPages={"abcdefghij","klmnopqrst","uvwxyz./:-","0123456789",
    "ABCDEFGHIJ","KLMNOPQRST","UVWXYZ?=&_","%#~+!$()*;"," []{},<>\\|","\"'`^@"};
struct Rect {
    int x, y, w, h;
};
inline constexpr Rect BackButton{8, 4, 84, 44};
inline constexpr Rect RowOne{8, 64, 304, 64}, RowTwo{8, 140, 304, 64};
inline constexpr std::array<Rect,3> ListRows={Rect{8,60,304,44},Rect{8,112,304,44},Rect{8,164,304,44}};
inline constexpr std::array<Rect,2> SplitPrimary={Rect{8,172,144,60},Rect{160,172,152,60}};
inline constexpr std::array<Rect,2> RemoteRows={Rect{8,56,304,44},Rect{8,108,304,44}};
inline constexpr Rect EmulateDetail{8, 152, 304, 20};
inline constexpr Rect Primary{8, 172, 304, 60};
inline constexpr Rect EditButton{8, 104, 304, 56}, EmulateButton{8, 176, 304, 56};
inline constexpr Rect UrlInput{8, 52, 304, 44};
inline constexpr std::array<Rect, 3> EditorFooter = {Rect{8, 192, 88, 44}, Rect{100, 192, 64, 44},
                                                     Rect{168, 192, 144, 44}};
inline Rect KeyRect(int index)
{
    return {8 + (index % 5) * 60, 100 + (index / 5) * 46, 56, 44};
}
}  // namespace stock_tools::ui
