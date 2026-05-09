// Unit tests for integerized IR implementation
// Tests: Task 3.1, 3.3, 3.5, 3.7, 3.9
// Requirements: 2.1-2.5, 3.1-3.5, 15.1-15.5

#include "../include/ir.hpp"
#include <cassert>
#include <iostream>

using namespace AC_IR;

// Test 1: IRRef uses integer IDs for temporaries
void test_integer_temp_refs() {
    std::cout << "Test 1: Integer temporary references... ";
    
    IRRef t0 = IRRef::temp(0);
    IRRef t1 = IRRef::temp(1);
    IRRef t2 = IRRef::temp(2);
    
    assert(t0.kind == IRRef::Kind::TEMP);
    assert(t0.id == 0);
    assert(t0.toString() == "t0");
    
    assert(t1.kind == IRRef::Kind::TEMP);
    assert(t1.id == 1);
    assert(t1.toString() == "t1");
    
    assert(t2.kind == IRRef::Kind::TEMP);
    assert(t2.id == 2);
    assert(t2.toString() == "t2");
    
    std::cout << "PASSED\n";
}

// Test 2: IRRef uses integer IDs for labels
void test_integer_label_refs() {
    std::cout << "Test 2: Integer label references... ";
    
    IRRef l0 = IRRef::label(0);
    IRRef l1 = IRRef::label(1);
    IRRef l2 = IRRef::label(2);
    
    assert(l0.kind == IRRef::Kind::LABEL);
    assert(l0.id == 0);
    assert(l0.toString() == "L0");
    
    assert(l1.kind == IRRef::Kind::LABEL);
    assert(l1.id == 1);
    assert(l1.toString() == "L1");
    
    assert(l2.kind == IRRef::Kind::LABEL);
    assert(l2.id == 2);
    assert(l2.toString() == "L2");
    
    std::cout << "PASSED\n";
}

// Test 3: SymbolTable with integer indexing
void test_symbol_table() {
    std::cout << "Test 3: Symbol table integer indexing... ";
    
    SymbolTable symbols;
    
    // Intern variables and get integer indices
    int xId = symbols.intern("x", IRType::INT);
    int yId = symbols.intern("y", IRType::INT);
    int zId = symbols.intern("z", IRType::INT);
    
    // Verify sequential indexing starting from 0
    assert(xId == 0);
    assert(yId == 1);
    assert(zId == 2);
    
    // Verify lookup by name
    assert(symbols.lookup("x") == 0);
    assert(symbols.lookup("y") == 1);
    assert(symbols.lookup("z") == 2);
    assert(symbols.lookup("nonexistent") == -1);
    
    // Verify getName by index
    assert(symbols.getName(0) == "x");
    assert(symbols.getName(1) == "y");
    assert(symbols.getName(2) == "z");
    
    // Verify size
    assert(symbols.size() == 3);
    
    std::cout << "PASSED\n";
}

// Test 4: SymbolTable with nested scopes
void test_symbol_table_scopes() {
    std::cout << "Test 4: Symbol table nested scopes... ";
    
    SymbolTable symbols;
    
    // Global scope
    int globalX = symbols.intern("x", IRType::INT);
    assert(globalX == 0);
    assert(symbols.getCurrentScope() == 0);
    
    // Enter function scope
    symbols.enterScope();
    assert(symbols.getCurrentScope() == 1);
    
    // Local variable shadows global
    int localX = symbols.intern("x", IRType::INT);
    int localY = symbols.intern("y", IRType::INT);
    assert(localX == 1);  // New symbol
    assert(localY == 2);
    
    // Lookup finds local x, not global
    assert(symbols.lookup("x") == 1);
    assert(symbols.lookup("y") == 2);
    
    // Exit scope
    symbols.exitScope();
    assert(symbols.getCurrentScope() == 0);
    
    // Lookup now finds global x again
    assert(symbols.lookup("x") == 0);
    assert(symbols.lookup("y") == -1);  // y no longer visible
    
    std::cout << "PASSED\n";
}

// Test 5: Arena allocator with 64KB blocks
void test_arena_allocator() {
    std::cout << "Test 5: Arena allocator... ";
    
    Arena arena(65536);  // 64KB blocks
    
    // Verify initial state
    assert(arena.numBlocks() == 1);
    assert(arena.totalAllocated() == 65536);
    assert(arena.totalUsed() == 0);
    
    // Allocate some memory
    void* ptr1 = arena.allocate(100);
    assert(ptr1 != nullptr);
    assert(arena.totalUsed() >= 100);
    assert(arena.numBlocks() == 1);  // Still in first block
    
    void* ptr2 = arena.allocate(200);
    assert(ptr2 != nullptr);
    assert(arena.totalUsed() >= 300);
    assert(arena.numBlocks() == 1);  // Still in first block
    
    // Allocate large chunk that requires new block
    void* ptr3 = arena.allocate(70000);
    assert(ptr3 != nullptr);
    assert(arena.numBlocks() == 2);  // New block allocated
    assert(arena.totalAllocated() >= 135536);  // At least 2 blocks
    
    // Test reset
    arena.reset();
    assert(arena.totalUsed() == 0);
    assert(arena.numBlocks() == 2);  // Blocks still exist
    
    // Test clear
    arena.clear();
    assert(arena.numBlocks() == 1);  // Back to 1 block
    assert(arena.totalUsed() == 0);
    
    std::cout << "PASSED\n";
}

// Test 6: Arena typed object creation
void test_arena_typed_allocation() {
    std::cout << "Test 6: Arena typed allocation... ";
    
    Arena arena;
    
    // Create IRInstruction objects
    IRInstruction* instr1 = arena.create<IRInstruction>(IROpcode::ADD);
    IRInstruction* instr2 = arena.create<IRInstruction>(IROpcode::SUB);
    IRInstruction* instr3 = arena.create<IRInstruction>(IROpcode::MUL);
    
    assert(instr1 != nullptr);
    assert(instr2 != nullptr);
    assert(instr3 != nullptr);
    
    assert(instr1->opcode == IROpcode::ADD);
    assert(instr2->opcode == IROpcode::SUB);
    assert(instr3->opcode == IROpcode::MUL);
    
    // Verify memory is from arena
    assert(arena.totalUsed() > 0);
    
    std::cout << "PASSED\n";
}

// Test 7: Flat array IR storage
void test_flat_array_storage() {
    std::cout << "Test 7: Flat array IR storage... ";
    
    IRFunction func("test");
    
    // Add instructions to flat array
    func.instructions.push_back(IRInstruction(IROpcode::LOAD_CONST));
    func.instructions.push_back(IRInstruction(IROpcode::ADD));
    func.instructions.push_back(IRInstruction(IROpcode::STORE_VAR));
    
    // Verify flat array storage
    assert(func.instructions.size() == 3);
    assert(func.instructions[0].opcode == IROpcode::LOAD_CONST);
    assert(func.instructions[1].opcode == IROpcode::ADD);
    assert(func.instructions[2].opcode == IROpcode::STORE_VAR);
    
    // Verify contiguous storage (addresses should be sequential)
    IRInstruction* ptr0 = &func.instructions[0];
    IRInstruction* ptr1 = &func.instructions[1];
    IRInstruction* ptr2 = &func.instructions[2];
    
    // Check that pointers are close together (within reasonable range)
    assert(ptr1 > ptr0);
    assert(ptr2 > ptr1);
    assert((char*)ptr1 - (char*)ptr0 == sizeof(IRInstruction));
    assert((char*)ptr2 - (char*)ptr1 == sizeof(IRInstruction));
    
    std::cout << "PASSED\n";
}

// Test 8: IRProgram with symbol table and arena
void test_ir_program_integration() {
    std::cout << "Test 8: IRProgram integration... ";
    
    IRProgram prog;
    
    // Verify symbol table exists
    int x = prog.symbols.intern("x");
    int y = prog.symbols.intern("y");
    assert(x == 0);
    assert(y == 1);
    
    // Verify arena exists
    assert(prog.arena.numBlocks() >= 1);
    assert(prog.arena.totalAllocated() >= 65536);
    
    // Add function with flat array storage
    prog.functions.emplace_back("main");
    IRFunction& func = prog.functions.back();
    func.instructions.push_back(IRInstruction(IROpcode::LOAD_CONST));
    
    assert(prog.functions.size() == 1);
    assert(prog.functions[0].instructions.size() == 1);
    
    std::cout << "PASSED\n";
}

// Test 9: IRRef variable references use symbol table indices
void test_var_ref_integer_ids() {
    std::cout << "Test 9: Variable references use integer IDs... ";
    
    IRRef v0 = IRRef::var(0);
    IRRef v1 = IRRef::var(1);
    IRRef v2 = IRRef::var(2);
    
    assert(v0.kind == IRRef::Kind::VAR);
    assert(v0.id == 0);
    assert(v0.toString() == "v0");
    
    assert(v1.kind == IRRef::Kind::VAR);
    assert(v1.id == 1);
    assert(v1.toString() == "v1");
    
    assert(v2.kind == IRRef::Kind::VAR);
    assert(v2.id == 2);
    assert(v2.toString() == "v2");
    
    std::cout << "PASSED\n";
}

// Test 10: String conversion only for debugging
void test_string_conversion_debug_only() {
    std::cout << "Test 10: String conversion for debugging only... ";
    
    // Create refs with integer IDs
    IRRef t0 = IRRef::temp(0);
    IRRef l0 = IRRef::label(0);
    IRRef v0 = IRRef::var(0);
    
    // toString() should work for debugging
    std::string t0Str = t0.toString();
    std::string l0Str = l0.toString();
    std::string v0Str = v0.toString();
    
    assert(t0Str == "t0");
    assert(l0Str == "L0");
    assert(v0Str == "v0");
    
    // But internal representation uses integers
    assert(t0.id == 0);
    assert(l0.id == 0);
    assert(v0.id == 0);
    
    std::cout << "PASSED\n";
}

int main() {
    std::cout << "=== Integerized IR Unit Tests ===\n\n";
    
    test_integer_temp_refs();
    test_integer_label_refs();
    test_symbol_table();
    test_symbol_table_scopes();
    test_arena_allocator();
    test_arena_typed_allocation();
    test_flat_array_storage();
    test_ir_program_integration();
    test_var_ref_integer_ids();
    test_string_conversion_debug_only();
    
    std::cout << "\n=== All tests PASSED ===\n";
    return 0;
}
