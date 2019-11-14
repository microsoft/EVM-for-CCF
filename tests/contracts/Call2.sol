pragma solidity ^0.5.1;

// Simple pure functions used by unit tests, displaying selection by function signature (see Call2 test case)
contract Call2 {
    function get() public pure returns (uint) {
        return 42;
    }

    function add(uint _n) public pure returns (uint) {
        return _n + 42;
    }

    function mul(uint _n) public pure returns (uint) {
        return _n * 42;
    }
}