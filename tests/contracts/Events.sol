pragma solidity ^0.5.1;

contract Events {
    constructor() public {
        log0(bytes32(uint256(0xdeadbeef)));
    }
    
    event Name(address a);
    function name(address a) public {
        emit Name(a);
    }
    
    event Interesting(uint indexed a, uint b, uint c);
    function emit_interesting(uint a, uint b, uint c) public {
        emit Interesting(a, b, c);
    }
}