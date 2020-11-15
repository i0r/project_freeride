// RUN: %dxc -E main -T cs_6_0 %s | FileCheck %s

// CHECK-NOT: dx.op.bufferLoad


RWBuffer<int4> buf;

[numthreads(8, 8, 1)]
void main(uint2 id : SV_DispatchThreadId) {
  buf[id.x].xyzw = 1;
}