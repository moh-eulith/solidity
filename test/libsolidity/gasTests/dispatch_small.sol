contract Small {
    uint public a;
    uint[] public b;
    function f1(uint x) public returns (uint) { a = x; b[uint8(msg.data[0])] = x; }
    fallback () external payable {}
}
// ====
// EVMVersion: =current
// bytecodeFormat: legacy
// ----
// creation:
//   codeDepositCost: 101800
//   executionCost: 145
//   totalCost: 101945
// external:
//   fallback: 128
//   a(): 2402
//   b(uint256): infinite
//   f1(uint256): infinite
