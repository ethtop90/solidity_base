pragma solidity ^0.4.24;

contract C {
  function test() public pure returns (fixed8x80) {
    fixed8x80 a = -1 / (1e9 ** 10);
    return a;
  }                                                                                                                                            
}
