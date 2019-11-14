pragma solidity ^0.5.1;

// Simple pure functions used by unit tests (see Call1 test case)
contract Call1 {
    function f(uint a, uint b) public pure returns (uint) {
        return a * (b + 42);
    }
}