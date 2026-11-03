from pathlib import Path
import subprocess,tempfile
r=Path(__file__).resolve().parent
source=(r.parent/'main/hal/board/stackchan.cc').read_text();start=source.index('virtual AudioCodec* GetAudioCodec() override');begin=source.index('{',start);depth=1;end=begin+1
while depth:depth+=(source[end]=='{')-(source[end]=='}');end+=1
body=source[start:end].replace('virtual ','').replace(' override','')
cpp=r'''#include <cassert>
#include <companion/muse_voice_core_access.h>
static unsigned constructors=0;
struct AudioCodec{};class CoreS3AudioCodec:public AudioCodec{public:template<class...Args>CoreS3AudioCodec(Args...){++constructors;}};
constexpr int AUDIO_INPUT_SAMPLE_RATE=24000,AUDIO_OUTPUT_SAMPLE_RATE=24000,AUDIO_I2S_GPIO_MCLK=0,AUDIO_I2S_GPIO_BCLK=0,AUDIO_I2S_GPIO_WS=0,AUDIO_I2S_GPIO_DOUT=0,AUDIO_I2S_GPIO_DIN=0,AUDIO_CODEC_AW88298_ADDR=0,AUDIO_CODEC_ES7210_ADDR=0,AUDIO_INPUT_REFERENCE=0;
struct Board{void* i2c_bus_=nullptr;
'''+body+r'''
};
int main(){assert(!companion::muse::ExistingVoiceCore()&&!constructors);Board b;auto* original=b.GetAudioCodec();assert(constructors==1&&companion::muse::ExistingVoiceCore()==original);assert(b.GetAudioCodec()==original&&constructors==1);CoreS3AudioCodec other;assert(!companion::muse::PublishStaticVoiceCore(other)&&companion::muse::ExistingVoiceCore()==original);}
'''
with tempfile.TemporaryDirectory(prefix='voice-core-lookup-') as temp:
 t=Path(temp);(t/'test.cpp').write_text(cpp)
 subprocess.run(['clang++','-std=c++17','-Wall','-Wextra','-Werror','-fsanitize=address,undefined','-DSTACKCHAN_BUILD_MUSE=1','-DSTACKCHAN_BUILD_MUSE_VOICE_PROTOCOL=1','-I'+str(r.parent/'main'),str(t/'test.cpp'),str(r.parent/'main/companion/muse_voice_core_access.cc'),'-o',str(t/'test')],check=True)
 subprocess.run([str(t/'test')],check=True)
print('PASS actual board getter and registry: lookup never initializes, only existing static Core published')
