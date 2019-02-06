library L {
    function f(mapping(uint => uint)[] storage) public pure {
    }
}
// ----
// TypeError: (27-58): Mappings cannot live outside storage.
