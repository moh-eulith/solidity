PUSH 0x1234
SWAP2
SWAP1
SWAP2
PUSH 0x5678
SWAP4
SWAP1
SWAP4
// ====
// optimizationPreset: none
// optimizer.peephole: true
// outputs: Assembly,Opcodes
// ----
// Assembly:
//   swap1
//   0x1234
//   swap3
//   0x5678
// Opcodes: SWAP1 PUSH2 0x1234 SWAP3 PUSH2 0x5678
