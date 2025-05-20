PUSH 0x2

.sub
    PUSH 0x11234
    DUP1 // not a target for push dup swap
    SWAP1 // expected to be eliminated: dup1 swap1 -> dup1
    PUSH 0x3333
    DUP10
    SWAP1
    PUSH 0x2222
    DUP2
    SWAP1
// ====
// optimizationPreset: none
// optimizer.peephole: true
// outputs: Assembly
// ----
// Assembly:
//   0x02
// stop
//
// sub_0: assembly {
//       0x011234
//       dup1
//       dup9
//       0x3333
//       dup1
//       0x2222
// }
