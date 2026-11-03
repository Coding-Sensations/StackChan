#include <companion/model_adapter.hpp>
#include <companion/canonical_frontend_bridge.hpp>
// NONEXECUTED link qualification reference. Retained using linker -u solely to
// prove actual model/frontend kernels resolve through --gc-sections. No caller,
// provider registration, activation, grant, wake dispatch or model asset exists.
// Arguments must be externally owner-placed/alive; no large automatic object.
extern "C" bool MiraWakeBackendLinkReference(mira_model::ModelAdapter* model,
    mira_native_frontend::Bridge* front,const std::uint8_t* asset,std::size_t bytes,
    std::uint64_t generation,std::uint64_t deadline,const std::int16_t* synthetic_pcm,
    std::size_t samples)noexcept{
    if(!model||!front)return false;
    std::array<std::int8_t,120> features{};
    bool result=front->Configure()&&model->Configure(asset,bytes,generation,deadline)==mira_model::Status::Ok;
    if(result)result=front->Feed(synthetic_pcm,samples,[&](const std::array<std::uint16_t,40>& raw)noexcept{
        if(front->Pack(raw,features)){std::uint8_t score=0;return model->Invoke(features.data(),features.size(),generation,deadline,score)==mira_model::Status::Ok;}
        return true;
    });
    if(result)result=front->Reset()&&model->Reset(generation,deadline)==mira_model::Status::Ok;
    model->Close();front->Close();
    volatile std::int8_t* p=features.data();for(std::size_t i=0;i<features.size();++i)p[i]=0;
    return result;
}
