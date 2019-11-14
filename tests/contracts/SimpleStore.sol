pragma solidity ^0.5.1;

contract SimpleStore {
    uint n;

    constructor(uint _n) public {
        n = _n;
    }

    function get() public view returns (uint) {
        return n;
    }

    function set(uint _n) public {
        n = _n;
    }

    function add(uint _n) public {
        n = n + _n;
    }
}