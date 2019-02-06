library L {
    function f(mapping(uint => uint)[] storage) external pure {
    }
}
// ----
// TypeError: (27-58): Mappings cannot live outside storage.
