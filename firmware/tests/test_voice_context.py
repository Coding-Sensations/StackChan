from pathlib import Path
import subprocess,tempfile
r=Path(__file__).resolve().parent
with tempfile.TemporaryDirectory(prefix="voice-context-") as temp:
 t=Path(temp)
 for name in ["test_model.cpp","test_freshness.cpp"]:
  binary=t/name
  subprocess.run(["clang++","-std=c++17","-Wall","-Wextra","-Werror","-fsanitize=address,undefined","-I"+str(r.parent/"components/muse_direct_protocol/voice/context"),str(r/("voice_context_"+name)),"-o",str(binary)],check=True)
  for args in ([[]] if name=="test_model.cpp" else [[str(i)] for i in range(38)]):subprocess.run([str(binary),*args],check=True)
print("PASS portable returned-context model plus 38 freshness cases; native permission unavailable")
