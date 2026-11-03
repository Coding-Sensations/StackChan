#pragma once
#include "digest.hpp"
#include "frontend_feature_contract.hpp"
#include "tensorflow/lite/micro/micro_interpreter.h"
#include "tensorflow/lite/micro/micro_mutable_op_resolver.h"
#include "tensorflow/lite/micro/micro_resource_variable.h"
#include "tensorflow/lite/schema/schema_utils.h"
#include <algorithm>
#include <array>
#include <cstring>
#include <new>
namespace mira_model {
inline constexpr bool RuntimeAvailable=false;
inline constexpr std::size_t ModelBytes=13168,MaxArena=128*1024;
enum class Status{Ok,Unavailable,Arena,Asset,Binding,Expired,Input,Metadata,Operators,Allocate,Invoke,Busy,ClockRollback};
struct AdapterTestView; // Test-only fixture friendship; no production state getter.
inline Status InspectMetadata(const std::uint8_t* bytes,std::size_t size) noexcept {
 if(bytes==nullptr || reinterpret_cast<std::uintptr_t>(bytes)%8!=0 || size<8 || size>65536)return Status::Metadata;
 flatbuffers::Verifier verifier(bytes,size,32,1024);
 if(!tflite::VerifyModelBuffer(verifier))return Status::Metadata;
 const auto* model=tflite::GetModel(bytes);
 if(model->version()!=3 || !model->subgraphs() || model->subgraphs()->size()!=2 || !model->operator_codes() || model->operator_codes()->size()!=12)return Status::Metadata;
 constexpr tflite::BuiltinOperator codes[]={tflite::BuiltinOperator_CALL_ONCE,tflite::BuiltinOperator_VAR_HANDLE,tflite::BuiltinOperator_RESHAPE,tflite::BuiltinOperator_READ_VARIABLE,tflite::BuiltinOperator_CONCATENATION,tflite::BuiltinOperator_STRIDED_SLICE,tflite::BuiltinOperator_ASSIGN_VARIABLE,tflite::BuiltinOperator_CONV_2D,tflite::BuiltinOperator_DEPTHWISE_CONV_2D,tflite::BuiltinOperator_FULLY_CONNECTED,tflite::BuiltinOperator_LOGISTIC,tflite::BuiltinOperator_QUANTIZE};
 constexpr int versions[]={1,1,1,1,2,2,1,3,3,4,2,1};
 for(unsigned i=0;i<12;++i){const auto* op=model->operator_codes()->Get(i);if(!op)return Status::Metadata;if(tflite::GetBuiltinCode(op)!=codes[i] || op->version()!=versions[i] || op->custom_code()!=nullptr)return Status::Operators;}
 const auto* graph=model->subgraphs()->Get(0);
 if(!graph || !graph->tensors() || !graph->inputs() || !graph->outputs() || graph->inputs()->size()!=1 || graph->outputs()->size()!=1)return Status::Metadata;
 const auto input=graph->inputs()->Get(0),output=graph->outputs()->Get(0);
 if(input<0 || output<0 || static_cast<unsigned>(input)>=graph->tensors()->size() || static_cast<unsigned>(output)>=graph->tensors()->size())return Status::Metadata;
 const auto* in=graph->tensors()->Get(input);const auto* out=graph->tensors()->Get(output);
 if(!in || !out)return Status::Metadata;
 if(in->type()!=tflite::TensorType_INT8 || out->type()!=tflite::TensorType_UINT8 || !in->shape() || !out->shape() || in->shape()->size()!=3 || out->shape()->size()!=2 || in->shape()->Get(0)!=1 || in->shape()->Get(1)!=3 || in->shape()->Get(2)!=40 || out->shape()->Get(0)!=1 || out->shape()->Get(1)!=1)return Status::Metadata;
 auto quant=[](const tflite::Tensor* tensor,float scale,int zero){const auto* q=tensor->quantization();return q && q->scale() && q->zero_point() && q->scale()->size()==1 && q->zero_point()->size()==1 && q->scale()->Get(0)==scale && q->zero_point()->Get(0)==zero;};
 if(!quant(in,26.f/255.f,-128) || !quant(out,1.f/256.f,0))return Status::Metadata;
 for(const auto* sub:*model->subgraphs()){
  if(!sub || !sub->tensors() || !sub->operators())return Status::Metadata;
  for(const auto* tensor:*sub->tensors())if(!tensor || (tensor->type()!=tflite::TensorType_INT8 && tensor->type()!=tflite::TensorType_UINT8 && tensor->type()!=tflite::TensorType_INT32 && tensor->type()!=tflite::TensorType_RESOURCE))return Status::Metadata;
  for(const auto* op:*sub->operators())if(!op || op->opcode_index()>=12)return Status::Operators;
 }
 return Status::Ok;
}
// Exclusive owner-thread access. Clock/context and adapter remain alive through
// each call; never destroy while a native operation runs. Context is NOT consent.
// Configure/Close are trusted controller operations; Configure explicitly replaces
// the previous binding. Stale Reset/Invoke cannot mutate a newer binding.
// Fixed128KiB backing requires owner-placed static/heap storage, never task-stack placement.
class ModelAdapter final {
public:
 using Clock=std::uint64_t(*)(void*) noexcept;
 ModelAdapter(Clock clock,void* context) noexcept:clock_(clock),clock_context_(context){}
 ModelAdapter(const ModelAdapter&)=delete;ModelAdapter& operator=(const ModelAdapter&)=delete;
 ModelAdapter(ModelAdapter&&)=delete;ModelAdapter& operator=(ModelAdapter&&)=delete;
 ~ModelAdapter(){Close();}
 Status Configure(const std::uint8_t* asset,std::size_t count,std::uint64_t generation,std::uint64_t deadline_ms,std::size_t arena_budget=MaxArena) noexcept {
  if(busy_){cancel_=true;return Status::Busy;}Close();busy_=true;
  if(!clock_ || generation==0)return Fail(Status::Binding);
  generation_=generation;deadline_=deadline_ms;
  auto context=Context(generation,deadline_ms);if(context!=Status::Ok)return Fail(context);
  if(asset==nullptr || count!=ModelBytes)return Fail(Status::Asset);
  if(arena_budget<8192 || arena_budget>MaxArena)return Fail(Status::Arena);
  std::memcpy(model_bytes_.data(),asset,count);
  constexpr std::array<std::uint8_t,32> expected={0x80,0x1a,0x98,0x30,0x7f,0xb6,0xc2,0xb1,0xa5,0x68,0xde,0x83,0xf0,0x78,0xa6,0x1b,0xd1,0x88,0x5e,0x1a,0x2c,0x0a,0xa3,0x4d,0x62,0x1a,0x91,0x0f,0xa1,0x68,0x1f,0x77};
  if(Sha256(model_bytes_.data(),ModelBytes)!=expected)return Fail(Status::Asset);
  budget_=arena_budget;
  return BuildInference(generation,deadline_ms);
 }
 Status Reset(std::uint64_t generation,std::uint64_t deadline_ms) noexcept {
  if(generation_!=0 && (generation==0 || generation!=generation_ || deadline_ms!=deadline_))return Status::Binding;
  if(busy_){cancel_=true;return Status::Busy;}if(!configured_)return Status::Unavailable;
  if(generation==0 || generation!=generation_ || deadline_ms!=deadline_)return Status::Binding;
  busy_=true;const auto status=Context(generation,deadline_ms);if(status!=Status::Ok)return Fail(status);
  const auto budget=budget_;TearDownInference();budget_=budget;
  return BuildInference(generation,deadline_ms);
 }
 Status Invoke(const std::int8_t* features,std::size_t count,std::uint64_t generation,std::uint64_t deadline_ms,std::uint8_t& raw_score) noexcept {
  raw_score=0;if(generation_!=0 && (generation==0 || generation!=generation_ || deadline_ms!=deadline_))return Status::Binding;if(busy_){cancel_=true;return Status::Busy;}if(!configured_)return Status::Unavailable;busy_=true;
  auto status=Context(generation,deadline_ms);if(status!=Status::Ok)return Fail(status);
  if(features==nullptr || count!=120)return Fail(Status::Input);
  std::memcpy(interpreter_->input(0)->data.int8,features,count);
  ++native_invocations_;
  if(interpreter_->Invoke()!=kTfLiteOk)return Fail(Status::Invoke);
  status=Context(generation,deadline_ms);if(status!=Status::Ok)return Fail(status);
  raw_score=interpreter_->output(0)->data.uint8[0];busy_=false;return Status::Ok;
 }
 void Close() noexcept {
  if(busy_){cancel_=true;return;}
  TearDownInference();Wipe(model_bytes_.data(),model_bytes_.size());
  configured_=false;cancel_=false;generation_=deadline_=0;budget_=used_=0;
 }
 bool Configured()const noexcept{return configured_;}
 std::size_t ArenaUsed()const noexcept{return used_;}
 bool ArenaZero()const noexcept{for(auto byte:arena_)if(byte!=0)return false;return true;}
private:
 friend struct AdapterTestView;
 using Resolver=tflite::MicroMutableOpResolver<12>;
 Status BuildInference(std::uint64_t generation,std::uint64_t deadline_ms) noexcept {
  const auto metadata=InspectMetadata(model_bytes_.data(),ModelBytes);if(metadata!=Status::Ok)return Fail(metadata);
  model_=tflite::GetModel(model_bytes_.data());
  auto& r=resolver_;
  if(r.AddCallOnce()!=kTfLiteOk || r.AddVarHandle()!=kTfLiteOk || r.AddReshape()!=kTfLiteOk || r.AddReadVariable()!=kTfLiteOk || r.AddConcatenation()!=kTfLiteOk || r.AddStridedSlice()!=kTfLiteOk || r.AddAssignVariable()!=kTfLiteOk || r.AddConv2D()!=kTfLiteOk || r.AddDepthwiseConv2D()!=kTfLiteOk || r.AddFullyConnected()!=kTfLiteOk || r.AddLogistic()!=kTfLiteOk || r.AddQuantize()!=kTfLiteOk)return Fail(Status::Operators);
  allocator_=tflite::MicroAllocator::Create(arena_.data(),budget_);
  if(!allocator_ || allocator_->used_bytes()>budget_)return Fail(Status::Arena);
  // Selected pinned local factory null-guards both actual persistent allocations.
  // Eight slots cover the frozen graph's VAR_HANDLE instances.
  resources_=tflite::MicroResourceVariables::Create(allocator_,8);
  if(!resources_)return Fail(Status::Allocate);
  interpreter_=new(interpreter_storage_.data()) tflite::MicroInterpreter(model_,resolver_,allocator_,resources_);
  if(interpreter_->AllocateTensors()!=kTfLiteOk)return Fail(Status::Allocate);
  const auto* in=interpreter_->input(0);const auto* out=interpreter_->output(0);
  if(!in || !out || in->type!=kTfLiteInt8 || in->bytes!=120 || out->type!=kTfLiteUInt8 || out->bytes!=1 || in->params.scale!=26.f/255.f || in->params.zero_point!=-128 || out->params.scale!=1.f/256.f || out->params.zero_point!=0)return Fail(Status::Metadata);
  used_=interpreter_->arena_used_bytes();if(used_>budget_)return Fail(Status::Arena);
  auto context=Context(generation,deadline_ms);if(context!=Status::Ok)return Fail(context);
  configured_=true;busy_=false;return Status::Ok;
 }
 void TearDownInference() noexcept {
  if(interpreter_){interpreter_->~MicroInterpreter();}
  interpreter_=nullptr;allocator_=nullptr;resources_=nullptr;model_=nullptr;
  Wipe(arena_.data(),arena_.size());Wipe(interpreter_storage_.data(),interpreter_storage_.size());
  resolver_.~MicroMutableOpResolver();Wipe(&resolver_,sizeof(resolver_));new(&resolver_) Resolver();stride_.Reset();configured_=false;used_=0;
 }
 Status Context(std::uint64_t generation,std::uint64_t deadline) noexcept {
  if(!clock_ || generation==0 || generation!=generation_ || deadline!=deadline_)return Status::Binding;
  const auto now=clock_(clock_context_);if(cancel_)return Status::Busy;
  if(clock_seen_ && now<last_now_)return Status::ClockRollback;
  clock_seen_=true;last_now_=now;
  return now>=deadline?Status::Expired:Status::Ok;
 }
 Status Fail(Status status) noexcept{busy_=false;Close();return status;}
 static void Wipe(void* pointer,std::size_t bytes) noexcept{auto p=static_cast<volatile std::uint8_t*>(pointer);for(std::size_t i=0;i<bytes;++i)p[i]=0;}
 Clock clock_;void* clock_context_;bool clock_seen_{false};std::uint64_t last_now_{0},native_invocations_{0};bool configured_{false},busy_{false},cancel_{false};std::uint64_t generation_{0},deadline_{0};std::size_t budget_{0},used_{0};
 alignas(16) std::array<std::uint8_t,MaxArena> arena_{};
 alignas(16) std::array<std::uint8_t,ModelBytes> model_bytes_{};
 alignas(tflite::MicroInterpreter) std::array<std::uint8_t,sizeof(tflite::MicroInterpreter)> interpreter_storage_{};
 Resolver resolver_{};mira_frontend::StrideAssembler stride_{};
 const tflite::Model* model_{nullptr};tflite::MicroAllocator* allocator_{nullptr};tflite::MicroResourceVariables* resources_{nullptr};tflite::MicroInterpreter* interpreter_{nullptr};
};
}
