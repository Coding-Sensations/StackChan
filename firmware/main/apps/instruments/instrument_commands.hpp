#pragma once
#include "instrument_engine.hpp"
#include <array>
namespace stackchan_instruments {
enum class CommandKind { NoteOn, NoteOff, Sustain, Pad, SequenceStart, SequenceStop, Project, Panic };
struct Command {CommandKind kind=CommandKind::Panic;unsigned a=0,b=0,c=0;Project project;};
// Caller serializes Push/Pop; fixed storage, no allocations on render path.
class Commands {
 std::array<Command,32> entries_{};unsigned head_=0,size_=0;bool panic_=false;
public:
 bool Push(const Command& c){if(size_==entries_.size()){if(c.kind==CommandKind::NoteOff||c.kind==CommandKind::Panic||c.kind==CommandKind::SequenceStop||(c.kind==CommandKind::Sustain&&!c.b))panic_=true;return false;}entries_[(head_+size_)%entries_.size()]=c;++size_;return true;}
 bool Pop(Command& out){if(panic_){Clear();out={};out.kind=CommandKind::Panic;return true;}if(!size_)return false;out=entries_[head_];head_=(head_+1)%entries_.size();--size_;return true;}
 void Clear(){head_=size_=0;panic_=false;}
};
inline bool Apply(Engine& e,const Command& c){switch(c.kind){case CommandKind::NoteOn:return e.NoteOn(c.a,c.b,c.c);case CommandKind::NoteOff:e.NoteOff(c.a,c.b);return true;case CommandKind::Sustain:e.Sustain(c.a,c.b);return true;case CommandKind::Pad:return e.TriggerPad(c.a,c.b,c.c);case CommandKind::SequenceStart:return e.StartSequence();case CommandKind::SequenceStop:e.StopSequence();return true;case CommandKind::Project:return e.SetProject(c.project);case CommandKind::Panic:e.Stop();return true;}return false;}
}
