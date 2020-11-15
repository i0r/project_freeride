// Run: %dxc -T vs_6_0 -E main

// According to HLSL reference:
// The 'sin' function can only operate on float, vector of float, and matrix of float.

// CHECK:      [[glsl:%\d+]] = OpExtInstImport "GLSL.std.450"

void main() {
  float result;
  float2 result2;
  float3 result3;
  float4 result4;
  float3x2 result3x2;

// CHECK:      [[a:%\d+]] = OpLoad %float %a
// CHECK-NEXT: [[sin_a:%\d+]] = OpExtInst %float [[glsl]] Sin [[a]]
// CHECK-NEXT: OpStore %result [[sin_a]]
  float a;
  result = sin(a);

// CHECK-NEXT: [[b:%\d+]] = OpLoad %float %b
// CHECK-NEXT: [[sin_b:%\d+]] = OpExtInst %float [[glsl]] Sin [[b]]
// CHECK-NEXT: OpStore %result [[sin_b]]
  float1 b;
  result = sin(b);

// CHECK-NEXT: [[c:%\d+]] = OpLoad %v3float %c
// CHECK-NEXT: [[sin_c:%\d+]] = OpExtInst %v3float [[glsl]] Sin [[c]]
// CHECK-NEXT: OpStore %result3 [[sin_c]]
  float3 c;
  result3 = sin(c);

// CHECK-NEXT: [[d:%\d+]] = OpLoad %float %d
// CHECK-NEXT: [[sin_d:%\d+]] = OpExtInst %float [[glsl]] Sin [[d]]
// CHECK-NEXT: OpStore %result [[sin_d]]
  float1x1 d;
  result = sin(d);

// CHECK-NEXT: [[e:%\d+]] = OpLoad %v2float %e
// CHECK-NEXT: [[sin_e:%\d+]] = OpExtInst %v2float [[glsl]] Sin [[e]]
// CHECK-NEXT: OpStore %result2 [[sin_e]]
  float1x2 e;
  result2 = sin(e);

// CHECK-NEXT: [[f:%\d+]] = OpLoad %v4float %f
// CHECK-NEXT: [[sin_f:%\d+]] = OpExtInst %v4float [[glsl]] Sin [[f]]
// CHECK-NEXT: OpStore %result4 [[sin_f]]
  float4x1 f;
  result4 = sin(f);

// CHECK-NEXT: [[g:%\d+]] = OpLoad %mat3v2float %g
// CHECK-NEXT: [[g_row0:%\d+]] = OpCompositeExtract %v2float [[g]] 0
// CHECK-NEXT: [[sin_g_row0:%\d+]] = OpExtInst %v2float [[glsl]] Sin [[g_row0]]
// CHECK-NEXT: [[g_row1:%\d+]] = OpCompositeExtract %v2float [[g]] 1
// CHECK-NEXT: [[sin_g_row1:%\d+]] = OpExtInst %v2float [[glsl]] Sin [[g_row1]]
// CHECK-NEXT: [[g_row2:%\d+]] = OpCompositeExtract %v2float [[g]] 2
// CHECK-NEXT: [[sin_g_row2:%\d+]] = OpExtInst %v2float [[glsl]] Sin [[g_row2]]
// CHECK-NEXT: [[sin_matrix:%\d+]] = OpCompositeConstruct %mat3v2float [[sin_g_row0]] [[sin_g_row1]] [[sin_g_row2]]
// CHECK-NEXT: OpStore %result3x2 [[sin_matrix]]
  float3x2 g;
  result3x2 = sin(g);
}
