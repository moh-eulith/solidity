contract Main {
    bytes3 name;
    bool flag;

    constructor(bytes3 x, bool f) {
        name = x;
        flag = f;
    }

    function getName() public returns (bytes3 ret) {
        return name;
    }

    function getFlag() public returns (bool ret) {
        return flag;
    }
}
// ====
// bytecodeFormat: legacy,>=EOFv1
// ----
// constructor(): "abc", true
// gas irOptimized: 80112
// gas irOptimized code: 29000
// gas legacy: 85098
// gas legacy code: 58200
// gas legacyOptimized: 80108
// gas legacyOptimized code: 27600
// getFlag() -> true
// getName() -> "abc"
