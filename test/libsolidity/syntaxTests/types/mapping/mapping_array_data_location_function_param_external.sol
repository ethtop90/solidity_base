contract c {
    function f1(mapping(uint => uint)[] calldata) pure external {}
}
// ----
// TypeError: (29-61): Mappings cannot live outside storage.
// TypeError: (29-61): Internal or recursive type is not allowed for public or external functions.
