contract Test {
    function loop() public pure {
        for (uint k = 0; (k == 0 ? true : false); k++) {
        }
    }
}
// ====
// SMTEngine: bmc
// ----
// Info 6002: BMC: 1 verification condition(s) proved safe! Enable the model checker option "show proved safe" to see all of them.
