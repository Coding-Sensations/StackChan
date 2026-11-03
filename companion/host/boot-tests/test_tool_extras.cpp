#include "toolbox_extras.hpp"
#include <cassert>
#include <iostream>
using namespace stock_tools;
int main()
{
    RemoteLibrary library;
    std::vector<uint32_t> pulses{9000, 4500, 560};
    assert(!library.Save("bad/name", pulses));
    assert(!library.Save("blank", {}));
    assert(!library.Save("zero", {0}));
    assert(!library.Save("long", {15001}));
    assert(!library.Save("budget", std::vector<uint32_t>(13,15000)));
    assert(!library.Save("count", std::vector<uint32_t>(512,100)));
    assert(library.Save("TV power", pulses));
    assert(!library.Save("TV power", pulses));
    auto archive = library.Export();
    RemoteLibrary imported;
    assert(imported.Import(archive));
    assert(imported.Records().size()==1 && imported.Records()[0].pulses==pulses);
    assert(imported.Export()==archive);
    auto corrupt = archive;
    corrupt.replace(corrupt.find("38000"),5,"56000");
    assert(!imported.Import(corrupt) && imported.Export()==archive);
    assert(!imported.Import(archive+"garbage\n") && imported.Export()==archive);
    assert(!imported.Import(std::string(RemoteLibrary::MaxArchive+1,'a')));
    assert(!imported.Import("STACKCHAN_IR_V1\n1\nTV\n38000\n1\n42949672960\n"));
    assert(!imported.Import("STACKCHAN_IR_V1\n1\nTV\n38000\n1\n+560\n"));
    for(size_t i=1;i<RemoteLibrary::MaxRecords;++i)
        assert(library.Save("Remote "+std::to_string(i),pulses));
    assert(!library.Save("Full",pulses));
    UnavailableRemoteStore store;
    std::string untouched="keep";
    assert(!store.Available() && !store.Load(untouched,RemoteLibrary::MaxArchive) && untouched=="keep" && !store.Save(archive));
    assert(!LoadRemotes(store,imported)&&!SaveRemotes(store,imported));
    struct MockStore : RemoteStore {
        bool fail=false;std::string bytes;
        bool Available() const override{return true;}
        bool Load(std::string& output,size_t limit) override{assert(limit==RemoteLibrary::MaxArchive);if(fail)return false;output=bytes;return true;}
        bool Save(std::string_view input) override{if(fail)return false;bytes=input;return true;}
    } memory;
    assert(SaveRemotes(memory,imported)&&memory.bytes==archive);
    memory.bytes=corrupt;assert(!LoadRemotes(memory,imported)&&imported.Export()==archive);
    memory.bytes=archive;assert(LoadRemotes(memory,imported));
    memory.fail=true;assert(!SaveRemotes(memory,imported)&&!LoadRemotes(memory,imported)&&imported.Export()==archive);
    imported.Clear(); assert(imported.Records().empty());

    UnavailableUart disabled;
    TerminalSession unavailable(disabled);
    assert(!unavailable.Start({GrovePort::None,115200},0));
    assert(!unavailable.Start({GrovePort::A,115200},0));
    assert(unavailable.State()==TerminalState::Unavailable);
    assert(!unavailable.Send("ping") && !unavailable.Active());
    assert(!ValidUartConfig({GrovePort::A,12345}));
    assert(ValidUartConfig({GrovePort::A,9600}));
    assert(ValidUartConfig({GrovePort::B,38400}));
    assert(ValidUartConfig({GrovePort::C,115200}));
    DemoUart demo;
    TerminalSession terminal(demo);
    assert(terminal.Start({GrovePort::A,115200},10));
    assert(!terminal.Start({GrovePort::B,115200},11));
    terminal.Poll(11); assert(terminal.Text().find("demo")!=std::string::npos);
    assert(!terminal.Send(std::string(65,'a')) && !terminal.Send("a\nb"));
    assert(terminal.Send("ping")); terminal.Poll(12);
    assert(terminal.Text().find("demo> ping")!=std::string::npos);
    for(int i=0;i<100;++i){assert(terminal.Send(std::string(60,'x')));terminal.Poll(13+i);}
    assert(terminal.Text().size()<=TerminalSession::MaxText);
    assert(terminal.Preview().size()<=160);
    terminal.Poll(60010); assert(!terminal.Active() && terminal.State()==TerminalState::Closed);
    assert(!terminal.Send("later"));
    assert(terminal.Start({GrovePort::C,9600},100));terminal.Poll(99);
    assert(!terminal.Active());
    struct FailedStop : DemoUart { bool Close() override { return false; } } failing;
    TerminalSession retry(failing);assert(retry.Start({GrovePort::A,9600},0));
    assert(!retry.Stop() && retry.State()==TerminalState::Restoring && retry.Active());
    assert(!retry.Send("blocked") && !retry.Start({GrovePort::B,9600},1));
    struct RejectWrite : DemoUart {bool Write(const uint8_t*,size_t) override{return false;}} rejected;
    TerminalSession write_failure(rejected);assert(write_failure.Start({GrovePort::A,9600},0));
    assert(!write_failure.Send("ping")&&!write_failure.Active()&&write_failure.State()==TerminalState::Error);
    assert(write_failure.Start({GrovePort::A,9600},1));assert(write_failure.Stop());
    struct RejectWriteAndClose : RejectWrite {bool Close() override{return false;}} restore_failure;
    TerminalSession restore(restore_failure);assert(restore.Start({GrovePort::A,9600},0));
    assert(!restore.Send("ping")&&restore.Active()&&restore.State()==TerminalState::Restoring);
    assert(!restore.Send("later")&&!restore.Start({GrovePort::B,9600},1));
    restore.Poll(60000);assert(restore.State()==TerminalState::Restoring);
    struct OversizeRead : DemoUart {int Read(uint8_t*,size_t) override{return 65;}} oversize;
    TerminalSession bad_read(oversize);assert(bad_read.Start({GrovePort::A,9600},0));bad_read.Poll(1);
    assert(!bad_read.Active()&&bad_read.State()==TerminalState::Error);
    struct RetryClose : DemoUart {
        bool first=true;
        bool Close() override{if(first){first=false;return false;}return DemoUart::Close();}
    } eventual;
    TerminalSession joined(eventual);assert(joined.Start({GrovePort::A,9600},0));
    assert(!joined.Stop()&&joined.Active()&&joined.State()==TerminalState::Restoring);
    assert(joined.Stop()&&!joined.Active()&&joined.State()==TerminalState::Closed);
    assert(joined.Clear()&&joined.Text().empty());
    std::cout<<"PASS remote archive bounds/atomic import/capacity/unavailable store; UART unavailable/demo/window/buffer/stop gates\n";
}
