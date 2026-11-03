import re,pathlib,shutil,json
out=pathlib.Path(__file__).resolve().parent
s=(out/'upstream-source.txt').read_text()
# Mask strings/comments preserving offsets for brace matching.
masked=re.sub(r'//[^\n]*|/\*[\s\S]*?\*/|"(?:\\.|[^"\\])*"|\'(?:\\.|[^\'\\])*\'',lambda m:' '*len(m[0]),s)
funcs=[]
for m in re.finditer(r'void (draw\w+)\(\)\s*\{',masked):
 start=m.start(); p=m.end(); depth=1
 while depth:
  if masked[p]=='{':depth+=1
  if masked[p]=='}':depth-=1
  p+=1
 funcs.append((m[1],s[start:p]))
print('functions',len(funcs))
filter_source=(out.parent/'screensaver_filter.hpp').read_text()
excluded=set(map(int,re.search(r'ExcludedSlides=\{([^}]+)',filter_source)[1].split(',')))
(out/'upstream-source.txt').write_text(s)
# Complete upstream license is already retained beside this generator.
(out/'effect_catalog.hpp').write_text('''#pragma once
#include <cstdint>
namespace effect_catalog {
uint16_t Count();
const char* Name(uint16_t index);
bool Usable(uint16_t index);
void Reset(uint16_t index);
// Single engine; caller owns framebuffer. No hardware, blocking waits, or IO.
bool Render(uint16_t index, uint32_t elapsed_ms, uint16_t* pixels, uint16_t width=320, uint16_t height=240, uint64_t (*clock)()=nullptr, uint32_t work_budget=1000000);
}
''')
helper_start=s.index('void drawGear(int');helper_end=masked.index('{',helper_start)+1; depth=1
while depth:
 if masked[helper_end]=='{':depth+=1
 if masked[helper_end]=='}':depth-=1
 helper_end+=1
gear=s[helper_start:helper_end]
parts=['#include "raster.hpp"\n#include "../screensaver_filter.hpp"\nnamespace effect_catalog {\nnamespace {\nRaster raster; Raster* gfx=&raster; float animationTime=0; bool pendingClear=true;\n']
parts.append(gear+'\n')
manifest=[]
for i,(name,body) in enumerate(funcs):
 if i+1 in excluded:
  parts.append(f'// Stable slide {i+1}: removed by screensaver policy.\n')
  manifest.append({'index':i,'name':'Unavailable','removed':True})
  continue
 # Lift function static locals into per-effect namespace for explicit reset.
 bm=re.sub(r'//[^\n]*|/\*[\s\S]*?\*/',lambda m:' '*len(m[0]),body)
 matches=list(re.finditer(r'\bstatic\s+([^;]+);',bm))
 decls=[m[1] for m in matches]
 for m in reversed(matches):body=body[:m.start()]+body[m.end():]
 globals=[];resets=[]
 for d in decls:
  typ,rest=re.match(r'([\w:]+(?:\s*\*)?)\s+([\s\S]+)',d).groups()
  globals.append(typ+' '+rest+';')
  left,sep,init=rest.partition('=');left=left.strip(); var=re.match(r'(\w+)',left)[1]
  if '[' in left:
   if sep:
    globals.append('const '+typ+' initial_'+rest+';')
    resets.append('std::memcpy('+var+',initial_'+var+',sizeof('+var+'));')
   else: resets.append('std::memset('+var+',0,sizeof('+var+'));')
  else: resets.append(var+'='+ (init.strip() if sep else '{}')+';')
 body=body.replace('uint16_t baseColor;','uint16_t baseColor = BLACK;').replace('uint16_t color;','uint16_t color = BLACK;')
 body=body.replace('(int)(lyapunov * 500 + 100)','(int)constrain(double(lyapunov * 500 + 100),0.0,255.0)').replace('(int)(-lyapunov * 300 + 100)','(int)constrain(double(-lyapunov * 300 + 100),0.0,255.0)')
 body=re.sub(r'uint8_t (\w+) = ([^;]+);',lambda m:'uint8_t '+m[1]+' = static_cast<uint8_t>(constrain(double('+m[2]+'),0.0,255.0));',body)
 body=re.sub(r'\bdelay\([^;]*\);','/* Removed upstream blocking delay. */',body)
 # The target C abs fallback accepts int; preserve floating-point overloads too.
 body=re.sub(r'(?<![\w:])abs\s*\(', 'std::abs(', body)
 parts.append(f'namespace e{i} {{\n'+ '\n'.join(globals)+'\nvoid reset(){'+''.join(resets)+'}\n'+body+'\n}\n')
 manifest.append({'index':i,'name':name[4:],'static_declarations':len(decls)})
parts.append('using Fn=void(*)();\nconst Fn draws[]={'+','.join(('nullptr' if i+1 in excluded else f'e{i}::{n}') for i,(n,b) in enumerate(funcs))+'};\nconst Fn resets[]={'+','.join(('nullptr' if i+1 in excluded else f'e{i}::reset') for i in range(len(funcs)))+'};\nconst char* names[]={'+','.join(json.dumps('Unavailable' if i+1 in excluded else n[4:]) for i,(n,b) in enumerate(funcs))+'};\n}\n')
parts.append('''uint16_t Count(){return sizeof(draws)/sizeof(draws[0]);}
const char* Name(uint16_t i){return i<Count()?names[i]:"Unknown";}
bool Usable(uint16_t i){return i<Count()&&view::saver::Allowed(unsigned(i)+1)&&draws[i]&&resets[i];}
void Reset(uint16_t i){if(Usable(i)){resetRandom(0x71352u+i);resets[i]();pendingClear=true;}}
bool Render(uint16_t i,uint32_t ms,uint16_t* p,uint16_t w,uint16_t h,uint64_t (*clock)(),uint32_t work_budget){
 if(!Usable(i)||!p||w!=320||h!=240)return false;
 raster.bind(p,w,h); raster.beginFrame(work_budget,clock); if(pendingClear){raster.fillScreen(BLACK);pendingClear=false;} animationTime=(ms%3600000)/1000.0f;
 draws[i](); const bool complete=!raster.exhausted(); raster.unbind();return complete;
}
}
''')
(out/'effect_catalog.cpp').write_text(''.join(parts))
(out/'manifest.json').write_text(json.dumps(manifest,indent=2)+'\n')
