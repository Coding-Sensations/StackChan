#include "../../main/hal/board/dma_sent_fence.hpp"
#include <cassert>
using codec_drain::DmaSentFence;
int main() {
  DmaSentFence f;
  assert(!f.Begin(0));
  assert(f.Begin(10));
  assert(!f.Begin(11)); // A pending drain cannot be displaced.
  assert(!f.OnSent(9, 1, 64));
  for (unsigned i=1; i<=6; ++i) assert(!f.OnSent(10, i, 64));
  for (unsigned i=1; i<6; ++i) assert(!f.OnSent(10, i, 64));
  assert(!f.Sent(10));
  assert(f.OnSent(10, 6, 64));
  assert(f.Sent(10));
  assert(!f.Sent(9));
  assert(!f.Begin(10));
  assert(f.Begin(11));
  f.Cancel(10); // A stale cancellation cannot affect the current fence.
  assert(f.state()==DmaSentFence::State::Pending);
  f.Cancel(11);
  assert(!f.Sent(11));
  assert(!f.OnSent(11, 1, 64));
  assert(f.Begin(12));
  assert(!f.OnSent(12, 0, 64));
  assert(f.state()==DmaSentFence::State::Failed);
  assert(f.Begin(13));
  assert(!f.OnSent(13, 1, 0));
  assert(f.state()==DmaSentFence::State::Failed);
  assert(f.Begin(14));
  for (unsigned i=1; i<=6; ++i) assert(!f.OnSent(14, i, 64));
  assert(!f.OnSent(14, 7, 64)); // Unknown descriptor fails closed.
  assert(f.state()==DmaSentFence::State::Failed);
  assert(f.Begin(15));
  f.Fail(14);
  assert(f.state()==DmaSentFence::State::Pending);
  f.Fail(15);
  assert(!f.Sent(15));
}
