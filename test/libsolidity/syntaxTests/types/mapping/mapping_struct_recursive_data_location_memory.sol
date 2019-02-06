pragma experimental ABIEncoderV2;
contract C {
    struct S { mapping(uint => uint) a; }
    struct T { S s; }
    struct U { T t; }
    function f(U memory) public {}
}
// ----
// Warning: (0-33): Experimental features are turned on. Do not use experimental features on live deployments.
// TypeError: (148-156): Mappings cannot live outside storage.
// TypeError: (148-156): Internal or recursive type is not allowed for public or external functions.
