#include "../include/ac.hpp"
#include <sstream>
#include <stack>
#include <set>
#include <cctype>
#include <cmath>
#include <cstdlib>
#include <functional>
#include <iostream>
#include <limits>
#include <unordered_map>
#include <unordered_set>
#include <map>

// Declared in main.cpp; set by --allow-foreign CLI flag
extern bool g_allow_foreign;

namespace AC_IR {

// ─── IRRef legacy helpers ───────────────────────────────────────────────────

// Legacy: Create VAR from string name (for backward compatibility)
// This should be replaced with symbol table lookups
IRRef IRRef::varLegacy(const std::string& varName) {
    IRRef ref;
    ref.kind = Kind::VAR;
    ref.id = -1;  // Mark as legacy (needs symbol table lookup)
    // Store name temporarily in value field as string
    ref.value = IRValue(varName);
    return ref;
}

// Legacy: Create FUNCTION from string name (for backward compatibility)
IRRef IRRef::funcLegacy(const std::string& funcName) {
    IRRef ref;
    ref.kind = Kind::FUNCTION;
    ref.id = -1;  // Mark as legacy (needs symbol table lookup)
    // Store name temporarily in value field as string
    ref.value = IRValue(funcName);
    return ref;
}

// Helper to get string representation with symbol table (for better debugging)
std::string IRRef::toStringWithSymbols(SymbolTable* symbols) const {
    if (!symbols) {
        return toString();
    }
    
    switch (kind) {
        case Kind::VAR:
            if (id >= 0) {
                return symbols->getName(id);
            }
            // Legacy: extract name from value field
            if (value.type == IRType::STRING) {
                return std::get<std::string>(value.data);
            }
            return "v" + std::to_string(id);
            
        case Kind::FUNCTION:
            if (id >= 0) {
                return symbols->getName(id);
            }
            // Legacy: extract name from value field
            if (value.type == IRType::STRING) {
                return std::get<std::string>(value.data);
            }
            return "f" + std::to_string(id);
            
        default:
            return toString();
    }
}

// ─── helpers ────────────────────────────────────────────────────────────────

static std::string typeStr(IRType t) {
    switch (t) {
        case IRType::VOID:     return "void";
        case IRType::INT:      return "int";
        case IRType::FLOAT:    return "float";
        case IRType::STRING:   return "string";
        case IRType::BOOL:     return "bool";
        case IRType::LIST:     return "list";
        case IRType::TUPLE:    return "tuple";
        case IRType::OBJECT:   return "object";
        case IRType::FUNCTION: return "function";
        case IRType::POINTER:  return "ptr";
        default:               return "unknown";
    }
}

static std::string opcodeStr(IROpcode op) {
    switch (op) {
        case IROpcode::LABEL:         return "label";
        case IROpcode::JUMP:          return "jmp";
        case IROpcode::JUMP_IF_TRUE:  return "jt";
        case IROpcode::JUMP_IF_FALSE: return "jf";
        case IROpcode::CALL:          return "call";
        case IROpcode::RETURN:        return "ret";
        case IROpcode::LOAD_CONST:    return "ldc";
        case IROpcode::LOAD_VAR:      return "ldv";
        case IROpcode::STORE_VAR:     return "stv";
        case IROpcode::ADD:           return "add";
        case IROpcode::SUB:           return "sub";
        case IROpcode::MUL:           return "mul";
        case IROpcode::PMUL:          return "pmul";
        case IROpcode::DIV:           return "div";
        case IROpcode::IDIV:          return "idiv";
        case IROpcode::FDIV:          return "fdiv";
        case IROpcode::MOD:           return "mod";
        case IROpcode::EQ:            return "eq";
        case IROpcode::NEQ:           return "neq";
        case IROpcode::LT:            return "lt";
        case IROpcode::GT:            return "gt";
        case IROpcode::LTE:           return "lte";
        case IROpcode::GTE:           return "gte";
        case IROpcode::AND:           return "and";
        case IROpcode::OR:            return "or";
        case IROpcode::NOT:           return "not";
        case IROpcode::XOR:           return "xor";
        case IROpcode::XNOR:          return "xnor";
        case IROpcode::XSUB:          return "xsub";
        case IROpcode::BAND:          return "band";
        case IROpcode::BOR:           return "bor";
        case IROpcode::BXOR:          return "bxor";
        case IROpcode::BNOT:          return "bnot";
        case IROpcode::PTM:           return "ptm";
        case IROpcode::PTD:           return "ptd";
        case IROpcode::ALLOC:         return "alloc";
        case IROpcode::FREE:          return "free";
        case IROpcode::FREE_DECL:     return "free_decl";
        case IROpcode::ALIAS_DECL:    return "alias_decl";
        case IROpcode::CONST_DECL:    return "const_decl";
        case IROpcode::RAISE_CLAUSE:  return "raise_clause";
        case IROpcode::SAVE_FILE:     return "save_file";
        case IROpcode::LAZY_EVAL:     return "lazy_eval";
        case IROpcode::TYPE_CAST:     return "cast";
        case IROpcode::LOAD_INDEX:    return "ldi";
        case IROpcode::STORE_INDEX:   return "sti";
        case IROpcode::PRINT:         return "print";
        case IROpcode::INPUT:         return "input";
        case IROpcode::NOP:           return "nop";
        case IROpcode::HALT:          return "halt";
        case IROpcode::FUNC_BEGIN:    return "func_begin";
        case IROpcode::FUNC_END:      return "func_end";
        case IROpcode::IF_BEGIN:      return "if_begin";
        case IROpcode::IF_ELSE:       return "if_else";
        case IROpcode::IF_END:        return "if_end";
        case IROpcode::WHILE_BEGIN:   return "while_begin";
        case IROpcode::WHILE_END:     return "while_end";
        case IROpcode::FOR_BEGIN:     return "for_begin";
        case IROpcode::FOR_END:       return "for_end";
        case IROpcode::EVENT_BIND:    return "ev_bind";
        case IROpcode::EVENT_TRIGGER: return "ev_trigger";
        case IROpcode::LIB_CALL:      return "lib_call";
        case IROpcode::EVAL:          return "eval";
        case IROpcode::CLASS_BEGIN:   return "class_begin";
        case IROpcode::CLASS_END:     return "class_end";
        case IROpcode::TRY_BEGIN:     return "try_begin";
        case IROpcode::CATCH_BEGIN:   return "catch_begin";
        case IROpcode::AFTER_BEGIN:   return "after_begin";
        case IROpcode::TRY_END:       return "try_end";
        case IROpcode::TAG_BEGIN:     return "tag_begin";
        case IROpcode::TAG_END:       return "tag_end";
        case IROpcode::YIELD:         return "yield";
        case IROpcode::GEN_CREATE:    return "gen_create";
        case IROpcode::GEN_NEXT:      return "gen_next";
        case IROpcode::GEN_DONE:      return "gen_done";
        default:                      return "???";
    }
}

// ─── generator ──────────────────────────────────────────────────────────────

class IRGenerator {
    IRProgram        prog;
    IRFunction*      cur  = nullptr;   // current function (null = global)
    int              tc   = 0;         // global temp counter
    int              lc   = 0;         // global label counter
    bool             inMainSection = false; // true once we enter <mainloop>
    bool             inFreeScope   = false; // true inside a <free> block
    std::set<int>    freeDeclaredIds;       // var IDs already FREE_DECL'd in current free scope
    bool             inBoundScope  = false; // true inside a <bound> block
    std::set<int>    boundDeclaredIds;      // var IDs already FREE_DECL'd(bound) in current bound scope
    std::string      currentClass_;         // non-empty while inside a bundle body
    std::string      currentCustomTag_;     // non-empty while inside a def tag body
    std::vector<std::string> currentTagObjects_; // ObjDecl names declared inside current custom tag
    std::set<std::string>    glObjects_;    // variable names declared as GL objects via ObjDecl
    // Vars assigned via a `.datac` row (`__dict__`-tagged AssignStmt, from injectDatacImports).
    // The immediately-following `__list__` assign (`pets = [_dc_pets_0, _dc_pets_1, ...]`)
    // wraps these — checked so those bare-identifier elements go straight into the ALLOC's
    // literal content text instead of through the generic placeholder-then-STORE_INDEX path
    // every OTHER "computed expression" list element takes. Skipping that path matters because
    // it's the only way codegen ever sees the REAL dict-var names in one place to recognize
    // "this is a list of dicts, not a list of ints" (see ir_codegen.cpp's listOfDictVars_ for
    // the consuming half — verified: examples/keyword_catalog_modules.ac's datac-imported
    // `pets`, "cannot convert std::map<...> to long long" on C++/Java/Go/V/Rust alike, since
    // the placeholder path always left plain "0,0" in the ALLOC text for these elements,
    // invisible to any dict-var check on the codegen side).
    std::set<std::string>    datacDictVars_;
    // Vars constructed via a bare widgets-ilib ctor call (`tb = textbox(...)`) — needed so the
    // string-cheese "receiver.method(args)" rewrite (below) can tell a WIDGET var apart from a
    // plain string var when a widget's OWN method name happens to collide with a string-cheese
    // one. `find` is the concrete case: it's both `stringm.find` (string search) AND
    // `textbox.find` (Abu's widget search method) — with no receiver-type info available at
    // this backend-agnostic lowering stage otherwise, `tb.find($x$)` was always rewritten to
    // `stringm.find(tb, $x$)`, calling the string utility on a raw widget HANDLE instead of the
    // widget's own method (verified: garbage int output instead of the matched text).
    static const std::set<std::string>& widgetCtorNames() {
        static const std::set<std::string> names = {
            "Screen", "display", "ask", "btn", "ckbtn", "radbtn", "dropdown",
            "advance", "slider", "group", "tabs", "scroller", "listbox", "table",
            "sketch", "textbox"
        };
        return names;
    }
    std::set<std::string>    widgetCtorVars_;
    // Every `Make func` (qualified "Class.method" for bundle methods, matching funcId's own
    // interning convention above) whose body contains a `yield` anywhere — populated by a
    // dedicated AST-only prepass (collectGeneratorFunctions, run once before gen(ast) in
    // generate()) BECAUSE generate() lowers in a single forward pass: a FOR loop calling a
    // generator defined LATER in the file would otherwise still be unresolved when the FOR
    // loop itself is lowered, and would silently fall through to the ordinary array-collection
    // path instead of the generator one.
    std::set<std::string>    generatorFuncNames_;
    // Vars assigned a DIRECT generator call (`g = twovals()`) BEFORE being used in a FOR loop —
    // `FOR x in g:` needs to know `g` already holds a live handle (reuse it directly) rather
    // than a fresh one (GEN_CREATE a NEW generator each time, which is what the `FOR x in
    // twovals():` direct-call shape correctly does instead). Populated by a second prepass,
    // collectGeneratorHandleVars, run right after collectGeneratorFunctions (needs
    // generatorFuncNames_ already complete to recognize the call as generator-producing).
    std::set<std::string>    generatorHandleVars_;
    // Every `on value is <key>` binding registered via `configure event-listener`, tracked so
    // the auto-generated `<StartHere>` game loop can poll+trigger them for real every frame —
    // see that case's own comment for why this was previously entirely dead (EVENT_BIND/
    // EVENT_TRIGGER worked fine, but nothing ever called EVENT_TRIGGER except an explicit
    // `input <key>` statement; real SDL key state and the callback table were never connected).
    // (key, callback-function-name, isContinuous — true for a nested `WHILST value is <samekey>`
    // body, meaning poll via key_pressed/held every frame rather than key_just_pressed once).
    std::vector<std::tuple<std::string,std::string,bool>> polledKeyBindings_;

    // ── tuples ───────────────────────────────────────────────────────────────
    // Memoized per-shape synthesized fallback bundle classes for escaping tuples (a tuple
    // that's bound to a var and used later, returned, passed as an argument, or otherwise
    // not immediately consumed in the same statement it's built in). Shape key ("2:i:s") →
    // synthesized class name ("_AcTuple2_i_s"), so structurally identical tuple shapes
    // anywhere in the program share exactly one class — see emitSyntheticTupleClass.
    std::map<std::string, std::string> tupleShapeClasses_;
    int tupleTempCounter_ = 0;
    // className -> its slots' IRTypes, and the set of classes built from an `; any` tuple
    // literal — both populated in emitSyntheticTupleClass, consulted by IndexExpr's tuple
    // lowering to decide constant-only vs. bounded-dynamic-dispatch indexing (see its comment).
    std::map<std::string, std::vector<IRType>> tupleClassSlotTypes_;
    std::set<std::string> tupleAnyClasses_;

    static std::string tupleTypeCode(IRType t) {
        switch (t) {
            case IRType::FLOAT:  return "f";
            case IRType::STRING: return "s";
            case IRType::BOOL:   return "b";
            default:             return "i"; // INT and anything else default to int slot
        }
    }

    // Build (once per distinct shape, memoized in tupleShapeClasses_) a compiler-synthesized
    // bundle class `_AcTupleN_...` with fields `_0.._N-1`, all defaulted to a type-appropriate
    // zero value — mirrors BundleDef's own synthetic-init emission shape exactly (the
    // `!hasUserInit && !fieldDefaults.empty()` branch above) so every backend's existing class
    // codegen, including BNY's whole-program classOwner/self.field scan (which never looks at
    // the AST at all), picks it up with zero backend-specific code. Fields are written
    // AFTERWARD by the caller via plain `t._0 = value` STORE_VARs — the exact same IR shape
    // PropAssign lowering produces for `p.x = 5` (Phase 0's own fix target) — rather than
    // passing values as constructor args, so tuple construction reuses the freshly-verified
    // "construct then field-assign" pattern instead of a novel one. Returns the class name.
    //
    // `isAny` (from an `; any` tuple literal) is part of the memoization key — an `; any`
    // tuple and a plain inferred/colloid tuple that HAPPEN to share the same slot types must
    // still get separate classes, since only the `; any` one is barred from dynamic indexing
    // (see IndexExpr's own tuple-lowering comment) — sharing a class would silently let a
    // dynamic index through on `(1, 2; any)` just because it's structurally identical to a
    // plain `(1, 2)`.
    std::string emitSyntheticTupleClass(const std::vector<IRType>& slotTypes, bool isAny = false) {
        std::string key = std::to_string(slotTypes.size());
        for (auto t : slotTypes) key += ":" + tupleTypeCode(t);
        if (isAny) key += ":any";
        auto it = tupleShapeClasses_.find(key);
        if (it != tupleShapeClasses_.end()) return it->second;

        // "AcTupleN_..." not "_AcTupleN_..." — class names pass through every backend's own
        // naming transform UNCHANGED (e.g. VStrategy::emitClassBegin emits `name` raw, no
        // vName()), and V specifically hard-errors on a struct name starting with anything but
        // an uppercase letter ("struct name `_AcTuple2_i_i` must begin with capital letter").
        std::string className = "AcTuple" + std::to_string(slotTypes.size());
        for (auto t : slotTypes) className += "_" + tupleTypeCode(t);
        if (isAny) className += "_any";
        tupleShapeClasses_[key] = className;
        tupleClassSlotTypes_[className] = slotTypes;
        if (isAny) tupleAnyClasses_.insert(className);

        // A tuple literal can be lowered from anywhere — inside a function body, inside
        // mainloop, at top level — but its class DEFINITION must land wherever BundleDef's
        // own definitions do (a standalone IRFunction entry + dataSection/globalInit markers),
        // never spliced into whatever section is currently active. Save/restore around it.
        //
        // `cur` is a raw IRFunction* into prog.functions (a std::vector) — saving it as a bare
        // pointer here is a real use-after-free hazard: this function's OWN prog.functions.
        // push_back() below (for the synthetic init) can reallocate the vector's backing
        // storage, silently invalidating any pointer into it, INCLUDING the caller's `cur` if
        // this ran while lowering was already inside another function body (e.g. `return x, y`
        // inside `Make makePair`). Verified real crash: Bus error, `return 3, 4` alone (no
        // caller-side destructure even needed to trigger it) — save/restore by INDEX instead,
        // recomputed against the vector's current (possibly moved) buffer afterward.
        IRFunction* savedCur = cur;
        size_t savedCurIdx = savedCur ? (size_t)(savedCur - prog.functions.data()) : (size_t)-1;
        bool savedInMain = inMainSection;
        cur = nullptr;
        inMainSection = false;

        IRInstruction cb(IROpcode::CLASS_BEGIN);
        cb.typedOperands = {mkConst(className)};
        rawEmit(cb);

        int funcId = prog.symbols.intern(className + ".init", IRType::FUNCTION);
        IRFunction fn("init", className);
        fn.returnType = IRType::VOID;
        prog.symbols.intern("self");
        fn.parameters.push_back("self");
        prog.functions.push_back(std::move(fn));
        cur = &prog.functions.back();
        IRInstruction entry(IROpcode::FUNC_BEGIN);
        entry.typedOperands = {IRRef::func(funcId)};
        emit(std::move(entry));

        for (size_t i = 0; i < slotTypes.size(); i++) {
            // "f0"/"f1"/... not "_0"/"_1" — a leading-underscore-plus-digit field name hits
            // VStrategy::vName's leading-'_'-becomes-'a' identifier sanitization at the field
            // DECLARATION site (emitFieldDecl, called with the bare field name) but NOT at any
            // USE site (`t._0`, embedded in a dotted string where vName only ever touches
            // index 0 of the WHOLE string, i.e. the receiver's first character) — verified
            // real bug: V declared `a0 i64` in the struct but every read/write still said
            // `t._0`, a genuinely undeclared field. Plain "f0" passes through every backend's
            // naming transform completely unchanged, sidestepping the mismatch entirely.
            IRRef dst = mkVar("self.f" + std::to_string(i));
            setRefType(dst, slotTypes[i]);
            IRRef zero = slotTypes[i] == IRType::FLOAT  ? IRRef::constant(IRValue(0.0)) :
                         slotTypes[i] == IRType::STRING ? mkConst("") :
                         mkConstInt(0);
            IRInstruction st(IROpcode::STORE_VAR, dst, {zero});
            st.resultType = slotTypes[i];
            emit(std::move(st));
        }
        IRInstruction ret(IROpcode::RETURN); ret.typedOperands = {}; emit(std::move(ret));
        IRInstruction end(IROpcode::FUNC_END); end.typedOperands = {IRRef::func(funcId)}; emit(std::move(end));
        cur = nullptr;

        IRInstruction ce(IROpcode::CLASS_END);
        ce.typedOperands = {mkConst(className)};
        rawEmit(ce);

        cur = (savedCurIdx != (size_t)-1) ? &prog.functions[savedCurIdx] : savedCur;
        inMainSection = savedInMain;
        return className;
    }

    // var name -> synthesized tuple class name, for every named var known to hold a tuple
    // instance (populated by lowerTupleEscaping below). Lets a later `t[i]` (constant i)
    // redirect straight to the field `t._{i-1}` instead of a real LOAD_INDEX.
    std::map<std::string, std::string> tupleInstanceVars_;

    // qualifiedFuncName -> {paramIndex (0-based, matches call-argument position, i.e. n.attrs'
    // own index — NOT fn.parameters', which has an extra leading "self" for methods) ->
    // synthesized tuple class name}. A tuple crosses a call boundary as a real fallback-bundle
    // instance (verified: `show(t)` passes the actual `AcTuple2_i_i` object, never a native
    // multi-return on any backend for the callee side) — but the callee's OWN parameter is just
    // a plain Identifier with no static type annotation, so `p[i]` inside `show`'s body has no
    // way to know `p` is a tuple unless told. Two-pass discovery (see discoverTupleParamShapes
    // and generateIR's driver) fixes this: pass 1 lowers the whole program once to see which
    // vars ever get passed as tuple-typed call arguments (impossible to know up front — a
    // tuple's class name is only known once its OWN binding site is actually lowered, and
    // FuncDefs are lowered in written order, almost always BEFORE the call sites that invoke
    // them), pass 2 re-lowers for real with this map pre-seeded so FuncDef's own case can treat
    // a matched parameter exactly like an already-known tupleInstanceVars_ entry from the very
    // first statement of the function body — zero changes needed to IndexExpr's tuple lookup.
    std::map<std::string, std::map<int, std::string>> tupleParamClasses_;

    // Populated by discoverTupleParamShapes() at the end of a lowering pass: a shallow,
    // single-hop scan of every already-emitted CALL/LIB_CALL instruction's arguments against
    // THIS pass's own tupleInstanceVars_. Only sees what pass 1 already knew — a tuple
    // forwarded through TWO hops (`show(p)` itself passing `p` on to a third function) needs a
    // third pass to resolve, which this does not attempt; a real (if narrower) fix for the
    // direct case beats none.
    //
    // A bare statement-level call with no assignment (`show(t)`, as opposed to `x = show(t)`)
    // does NOT go through the plain-CALL path used elsewhere — MethodCall's generic fallback
    // (verified: ir.cpp's own `case NodeType::MethodCall`, its LAST branch before FunctionCall)
    // emits it as a LIB_CALL instead, with the exact same {mkVar(calleeName), args...} operand
    // shape (confirmed via --stop-after-ir: `show(t)` lowered to `lib_call show, t`, not `call`).
    // LIB_CALL is also used for real library/widget/gl calls whose operand[0] is a mkConst
    // string, not a VAR — the Kind::VAR guard below already excludes those for free, so this
    // scan only ever matches a genuine user-defined function/method name.
    void discoverTupleParamShapes() {
        auto scanList = [&](const std::vector<IRInstruction>& instrs) {
            for (const auto& ins : instrs) {
                if ((ins.opcode != IROpcode::CALL && ins.opcode != IROpcode::LIB_CALL)
                        || ins.typedOperands.empty()) continue;
                const IRRef& callee = ins.typedOperands[0];
                if (callee.kind != IRRef::Kind::VAR) continue;
                std::string calleeName = prog.symbols.getName(callee.id);
                for (size_t ai = 1; ai < ins.typedOperands.size(); ai++) {
                    const IRRef& arg = ins.typedOperands[ai];
                    if (arg.kind != IRRef::Kind::VAR) continue;
                    auto tv = tupleInstanceVars_.find(prog.symbols.getName(arg.id));
                    if (tv != tupleInstanceVars_.end())
                        tupleParamClasses_[calleeName][(int)(ai - 1)] = tv->second;
                }
            }
        };
        for (auto& fn : prog.functions) scanList(fn.instructions);
        scanList(prog.globalInit);
        scanList(prog.dataSection);
        scanList(prog.mainSection);
    }

    // ── tuple scalarization (SROA) ──────────────────────────────────────────
    // "qualifiedFn::varName" -> {arity, isAny}, populated ONCE by collectTupleTrackability
    // (an AST-only prepass, run before gen(ast) — see its own comment) for every
    // `var = (tuple literal)` binding proven to never escape its enclosing function body (or
    // the top-level/mainloop scope, qualifiedFn=""). Consulted at REAL lowering time by the
    // AssignStmt/IndexExpr/DestructureAssignStmt cases to route through flat shadow vars
    // instead of ever constructing a synthesized bundle instance — the actual "optimize it
    // away completely" case for a tuple that's bound now and read later, as opposed to
    // DestructureAssignStmt's own separate same-statement fast path.
    struct TupleScalarCandidate { size_t arity; bool isAny; };
    std::map<std::string, TupleScalarCandidate> scalarizableTupleVars_;
    // "qualifiedFn::varName" -> the REAL per-slot IRTypes, filled in when the (already proven
    // scalarizable) binding is actually lowered — a prepass over the AST alone can't know an
    // element's type (it may depend on other vars' inferred types), only real lowering can.
    // Consulted by later index/destructure reads of the same var within the SAME forward pass
    // (this compiler's established single-forward-pass assumption — see e.g.
    // selectiveImportAliases_'s own comment for the identical pattern elsewhere).
    std::map<std::string, std::vector<IRType>> tupleScalarShadowTypes_;
    // Qualified name of the function currently being lowered ("" = top-level/mainloop scope) —
    // matches collectGeneratorFunctions'/collectTupleTrackability's own qualification
    // convention exactly, so a lookup key built the same way at both prepass and real-lowering
    // time always agrees. Set/restored around FuncDef's own case.
    std::string currentFunc_;

    static std::string sanitizeForVarName(const std::string& s) {
        std::string r = s;
        for (char& c : r) if (c == '.') c = '_';
        return r;
    }
    // Deterministic — both the binding lowering (which creates these) and any later index/
    // destructure read (which just needs to KNOW the name, not store it anywhere) compute the
    // exact same name independently; no separate "where do this var's shadows live" table
    // needed beyond scalarizableTupleVars_ answering "is it scalarized at all".
    std::string tupleScalarShadowName(const std::string& varName, size_t i) const {
        return "_ac_tup_scal_" + sanitizeForVarName(currentFunc_) + "_" + varName + "_f" + std::to_string(i);
    }

    // Whole-word substring search — `text` contains `word` bounded by non-identifier
    // characters (or the string's own edges) on both sides, so matching "t" doesn't fire on
    // "total" or "at". Used to scan legacy TEXT-based `attrs` fields for a reference to the
    // tracked var (see scanTupleEscaping's own comment on why this exists at all).
    static bool containsWholeWord(const std::string& text, const std::string& word) {
        if (word.empty()) return false;
        size_t pos = 0;
        while ((pos = text.find(word, pos)) != std::string::npos) {
            bool leftOk = (pos == 0) || !(std::isalnum((unsigned char)text[pos - 1]) || text[pos - 1] == '_');
            size_t endPos = pos + word.size();
            bool rightOk = (endPos >= text.size()) || !(std::isalnum((unsigned char)text[endPos]) || text[endPos] == '_');
            if (leftOk && rightOk) return true;
            pos = endPos;
        }
        return false;
    }

    // Escape-detection walker for ONE candidate var within ONE scope (see
    // collectTupleTrackabilityScope). Stops at FuncDef/BundleDef boundaries (this language has
    // no closures, so nothing outside the var's own enclosing scope could reference it anyway).
    // Two contexts are exempted from flagging the var's OWN reference as escaping (though their
    // OTHER parts are still recursed into normally): the base of a t[i] index read (constant or
    // dynamic — both are supported directly against shadow vars, see lowerTupleScalarIndex),
    // and a bare `a, b = t` whole-tuple destructure. Anything else — a call argument, a return
    // value, stored into a list/dict/field, compared, copied to another var, printed as a
    // whole value — has no representation once scalarized, so it forces the fallback bundle
    // path instead.
    void scanTupleEscaping(const ASTNode& node, const std::string& varName, bool& escaped) {
        if (escaped) return;
        if (node.type == NodeType::FuncDef || node.type == NodeType::BundleDef) return;
        if (node.type == NodeType::IndexExpr && node.children.size() >= 2 && node.children[0]
                && node.children[1]
                && node.children[0]->type == NodeType::Identifier && node.children[0]->value == varName) {
            scanTupleEscaping(*node.children[1], varName, escaped);
            return;
        }
        if (node.type == NodeType::DestructureAssignStmt && !node.children.empty() && node.children[0]
                && node.children[0]->type == NodeType::Identifier && node.children[0]->value == varName) {
            return;
        }
        if (node.type == NodeType::AssignStmt && node.value == varName) {
            // The var's own (single, per assignCount) binding statement — assigning TO it
            // isn't a read of a prior value. Still recurse into the RHS for OTHER references.
            for (auto& c : node.children) { if (escaped) return; if (c) scanTupleEscaping(*c, varName, escaped); }
            return;
        }
        if (node.type == NodeType::Identifier && node.value == varName) {
            escaped = true;
            return;
        }
        // A LOT of this parser's grammar (MethodCall's various forms, plain FunctionCall,
        // ConfigCall, ObjDecl, ...) represents its argument/operand list as legacy TEXT in
        // `attrs`, not as child AST nodes at all — a var referenced ONLY that way (verified
        // real bug: `useIt(t)` — a bare call, parsed as MethodCall with attrs=["t"] — was
        // completely invisible to the Identifier-node check above, so `t` got scalarized
        // despite being passed to a function that expects a real value; the generated code
        // referenced a `t` that was never declared, since only its shadow vars existed).
        // Conservative by construction: ANY node with a matching word in ANY attrs string is
        // escaping, whatever the node type — there's no "safe" attrs-text context in this
        // design (every genuinely safe context above is a STRUCTURED node with empty attrs).
        for (auto& a : node.attrs) {
            if (containsWholeWord(a, varName)) { escaped = true; return; }
        }
        for (auto& c : node.children) {
            if (escaped) return;
            if (c) scanTupleEscaping(*c, varName, escaped);
        }
    }

    // Binding+escape analysis for ONE scope (a function body, or the top-level/mainloop
    // content) — see scalarizableTupleVars_'s own comment for the overall design. A var
    // qualifies only with EXACTLY ONE assignment anywhere in scope (AssignStmt OR
    // DestructureAssignStmt target) — a conservative "single static binding only" rule that
    // sidesteps reasoning about whether a later reassignment is a compatible re-binding: a var
    // reassigned even once more, to anything, falls back to the ordinary bundle path.
    void collectTupleTrackabilityScope(const ASTNode& scopeRoot, const std::string& qualifiedFn) {
        std::map<std::string, int> assignCount;
        std::map<std::string, const ASTNode*> bindingLiteral;
        std::function<void(const ASTNode&)> collectAssigns = [&](const ASTNode& node) {
            if (node.type == NodeType::FuncDef || node.type == NodeType::BundleDef) return;
            if (node.type == NodeType::AssignStmt && !node.value.empty()
                    && node.value.find('.') == std::string::npos) {
                assignCount[node.value]++;
                if (!node.children.empty() && node.children[0]
                        && node.children[0]->type == NodeType::TupleLiteral)
                    bindingLiteral[node.value] = node.children[0].get();
            } else if (node.type == NodeType::DestructureAssignStmt) {
                for (auto& t : node.attrs) assignCount[t]++;
            }
            for (auto& c : node.children) if (c) collectAssigns(*c);
        };
        collectAssigns(scopeRoot);

        for (auto& [varName, litNode] : bindingLiteral) {
            if (assignCount[varName] != 1) continue;
            bool escaped = false;
            scanTupleEscaping(scopeRoot, varName, escaped);
            if (!escaped)
                scalarizableTupleVars_[qualifiedFn + "::" + varName] =
                    { litNode->children.size(), litNode->value == "any" };
        }
    }

    // Top-level prepass entry: the whole program's top-level/mainloop content is one flat
    // scope (qualifiedFn=""), and each FuncDef body is its own separate scope — matches
    // collectGeneratorFunctions' own qualification convention (classCtx + "." + name) exactly,
    // so lookups built the same way at real-lowering time always agree.
    void collectTupleTrackability(const ASTNode& ast) {
        collectTupleTrackabilityScope(ast, "");
        std::function<void(const ASTNode&, const std::string&)> findFuncs =
            [&](const ASTNode& node, const std::string& classCtx) {
                if (node.type == NodeType::FuncDef) {
                    std::string qualified = classCtx.empty() ? node.value : classCtx + "." + node.value;
                    if (!node.children.empty() && node.children[0])
                        collectTupleTrackabilityScope(*node.children[0], qualified);
                    for (auto& c : node.children) if (c) findFuncs(*c, classCtx);
                    return;
                }
                if (node.type == NodeType::BundleDef) {
                    for (auto& c : node.children) if (c) findFuncs(*c, node.value);
                    return;
                }
                for (auto& c : node.children) if (c) findFuncs(*c, classCtx);
            };
        findFuncs(ast, "");
    }

    static IRType tupleTypeNameToIRType(const std::string& name) {
        if (name == "dec")    return IRType::FLOAT;
        if (name == "string") return IRType::STRING;
        if (name == "bool")   return IRType::BOOL;
        if (name == "short")  return IRType::SHORT;
        if (name == "mini")   return IRType::MINI;
        if (name == "atomic") return IRType::ATOMIC;
        return IRType::INT;   // "int" and any unrecognized name (parser already validated it)
    }
    static const char* irTypeName(IRType t) {
        switch (t) {
            case IRType::FLOAT:  return "dec";
            case IRType::STRING: return "string";
            case IRType::BOOL:   return "bool";
            case IRType::SHORT:  return "short";
            case IRType::MINI:   return "mini";
            case IRType::ATOMIC: return "atomic";
            default:             return "int";
        }
    }
    // `(elems; TYPE)` colloid coercion for a single LITERAL element — resolved entirely at
    // compile time (the literal's own text is reparsed/reformatted for the target type), per
    // Abu's own spec: "tries to convert each item to that type before IR(And throws a
    // Preposterous if it can[not])". Throws ACError::tupleColloidConversionFailed on any
    // literal that provably can't convert (e.g. a non-numeric string to int/dec).
    IRRef coerceLiteralToType(const ASTNode& lit, IRType target) {
        const std::string& kind = lit.attrs.empty() ? std::string() : lit.attrs[0];
        auto asDisplay = [&]{ return kind == "STRING" ? ("$" + lit.value + "$") : lit.value; };
        switch (target) {
            case IRType::SHORT: case IRType::MINI: case IRType::ATOMIC: case IRType::INT: {
                if (kind == "INT") return mkConstInt(std::stoll(lit.value));
                if (kind == "FLOAT") { try { return mkConstInt((int64_t)std::stod(lit.value)); } catch (...) {} }
                if (kind == "BOOL") return mkConstInt(lit.value == "true" ? 1 : 0);
                if (kind == "STRING") {
                    try {
                        size_t idx = 0;
                        long long v = std::stoll(lit.value, &idx);
                        if (idx == lit.value.size()) return mkConstInt(v);
                    } catch (...) {}
                }
                throw ACError::tupleColloidConversionFailed(asDisplay(), irTypeName(target));
            }
            case IRType::FLOAT: {
                if (kind == "INT" || kind == "FLOAT") { try { return IRRef::constant(IRValue(std::stod(lit.value))); } catch (...) {} }
                if (kind == "BOOL") return IRRef::constant(IRValue(lit.value == "true" ? 1.0 : 0.0));
                if (kind == "STRING") {
                    try {
                        size_t idx = 0;
                        double v = std::stod(lit.value, &idx);
                        if (idx == lit.value.size()) return IRRef::constant(IRValue(v));
                    } catch (...) {}
                }
                throw ACError::tupleColloidConversionFailed(asDisplay(), irTypeName(target));
            }
            case IRType::BOOL: {
                if (kind == "BOOL") return IRRef::constant(IRValue(lit.value == "true"));
                if (kind == "INT") { try { return IRRef::constant(IRValue(std::stoll(lit.value) != 0)); } catch (...) {} }
                if (kind == "FLOAT") { try { return IRRef::constant(IRValue(std::stod(lit.value) != 0.0)); } catch (...) {} }
                if (kind == "STRING") return IRRef::constant(IRValue(lit.value == "true"));
                throw ACError::tupleColloidConversionFailed(asDisplay(), irTypeName(target));
            }
            case IRType::STRING:
                // Every literal kind has a natural string form — always succeeds.
                if (kind == "BOOL") return mkConst(lit.value == "true" ? std::string("true") : std::string("false"));
                return mkConst(lit.value);
            default:
                throw ACError::tupleColloidConversionFailed(asDisplay(), irTypeName(target));
        }
    }

    // Shared by every TupleLiteral consumer — BOTH the escaping/fallback-bundle path
    // (lowerTupleEscaping, below) AND the same-statement destructure fast path
    // (DestructureAssignStmt's own case in gen(), which deliberately never calls
    // lowerTupleEscaping at all, since its whole point is to avoid ever building a bundle) —
    // the type-unification RULE applies to a tuple regardless of whether it ends up allocated.
    // `n.value` carries the tuple's optional annotation (parser.cpp's paren-expr-or-tuple
    // branch): "" = normal/inferred (must end up homogeneous — mixed int/dec widens to dec,
    // anything else is a Preposterous compile error), "any" = explicit heterogeneous (no
    // coercion at all), or a coercion type name (int/dec/string/bool/short/mini/atomic) = a
    // "colloid" — every element is converted to that one type before IR is generated.
    void lowerTupleElements(const ASTNode& n, std::vector<IRRef>& elems, std::vector<IRType>& slotTypes) {
        if (n.value == "any") {
            for (auto& c : n.children) {
                if (!c) continue;
                IRRef er = lowerExprNode(*c);
                elems.push_back(er);
                slotTypes.push_back(typeOfRef(er));
            }
        } else if (!n.value.empty()) {
            // Colloid: coerce every element to the annotated type.
            IRType target = tupleTypeNameToIRType(n.value);
            for (auto& c : n.children) {
                if (!c) continue;
                IRRef er = (c->type == NodeType::LiteralExpr)
                    ? coerceLiteralToType(*c, target)
                    : lowerExprNode(*c);
                if (er.kind != IRRef::Kind::CONST && typeOfRef(er) != target) {
                    IRRef cast = mkTemp();
                    IRInstruction ci(IROpcode::TYPE_CAST);
                    ci.typedOperands = {er};
                    ci.result = cast;
                    ci.resultType = target;
                    emit(std::move(ci));
                    er = cast;
                }
                elems.push_back(er);
                slotTypes.push_back(target);
            }
        } else {
            // Normal/inferred: lower every element, then require a single common type — mixed
            // int/dec widens to dec (matches every other numeric-mixing context in this
            // compiler); any other mismatch is a hard compile-time error.
            for (auto& c : n.children) {
                if (!c) continue;
                IRRef er = lowerExprNode(*c);
                elems.push_back(er);
                slotTypes.push_back(typeOfRef(er));
            }
            bool anyFloat = false, mismatch = false;
            IRType first = IRType::VOID;
            IRType other = IRType::VOID;
            for (IRType t : slotTypes) {
                // VOID means "not known yet at this point in lowering" (e.g. a plain function
                // call's result — return types aren't inferred until codegen, see
                // lowerExprNode's CallExpr case, a bare mkTemp() with no setRefType at all) —
                // NOT a real type, and never conflicts with anything. Verified real bug:
                // `(getNum(), 100)` — both genuinely int at runtime — was rejected as
                // "mismatched types (int and int)" (irTypeName's own VOID-defaults-to-"int"
                // fallback made the message doubly misleading) purely because getNum()'s
                // result type wasn't resolved yet.
                if (t == IRType::VOID) continue;
                if (t == IRType::FLOAT) anyFloat = true;
                if (first == IRType::VOID) { first = t; continue; }
                if (t != first && !(t == IRType::INT && first == IRType::FLOAT)
                               && !(first == IRType::INT && t == IRType::FLOAT)) {
                    mismatch = true; other = t;
                }
            }
            if (mismatch)
                throw ACError::tupleNotHomogeneous(irTypeName(first), irTypeName(other));
            if (anyFloat) {
                for (size_t i = 0; i < elems.size(); i++) {
                    if (slotTypes[i] == IRType::FLOAT) continue;
                    if (elems[i].kind == IRRef::Kind::CONST && elems[i].value.type == IRType::INT) {
                        elems[i] = IRRef::constant(IRValue((double)std::get<int64_t>(elems[i].value.data)));
                    } else {
                        IRRef cast = mkTemp();
                        IRInstruction ci(IROpcode::TYPE_CAST);
                        ci.typedOperands = {elems[i]};
                        ci.result = cast;
                        ci.resultType = IRType::FLOAT;
                        emit(std::move(ci));
                        elems[i] = cast;
                    }
                    slotTypes[i] = IRType::FLOAT;
                }
            }
        }
    }

    // Escaping-path lowering: construct a synthesized bundle instance and field-assign each
    // element (via lowerTupleElements, above). Returns the instance var's IRRef. `explicitDst`,
    // when given, constructs DIRECTLY into that (already-named) var — matching the verified
    // `t = Point()` construct-call shape exactly — rather than a synthesized temp name; used by
    // AssignStmt's `t = (x, y)` binding case so `t` itself becomes a real, trackable
    // class-instance var (a plain STORE_VAR copying a synthetic var's value into `t` would NOT
    // make codegen's own instanceVarClass_/classInstanceVars_ tracking recognize `t` as a class
    // instance — those are populated only at construct-call sites and via noteInstanceClass,
    // never by a generic value copy).
    IRRef lowerTupleEscaping(const ASTNode& n, const std::string& hint = "tup", const IRRef* explicitDst = nullptr) {
        std::vector<IRRef> elems;
        std::vector<IRType> slotTypes;
        lowerTupleElements(n, elems, slotTypes);
        std::string className = emitSyntheticTupleClass(slotTypes, n.value == "any");
        IRRef instVar;
        std::string varName;
        if (explicitDst && explicitDst->kind == IRRef::Kind::VAR) {
            instVar = *explicitDst;
            varName = prog.symbols.getName(instVar.id);
        } else {
            varName = "_ac_tup_" + hint + "_" + std::to_string(tupleTempCounter_++);
            instVar = mkVar(varName);
        }
        std::vector<IRRef> ctorOps = {mkVar(className)};
        IRInstruction ctor(IROpcode::CALL, instVar, ctorOps);
        emit(std::move(ctor));
        for (size_t i = 0; i < elems.size(); i++) {
            IRRef fdst = mkVar(varName + ".f" + std::to_string(i));
            IRInstruction st(IROpcode::STORE_VAR, fdst, {elems[i]});
            emit(std::move(st));
        }
        tupleInstanceVars_[varName] = className;
        return instVar;
    }

    // Shared bounded-dispatch/constant-index core for BOTH representations a tuple var can be
    // in: escaping (a synthesized bundle instance, fields `t.fN`) or scalarized (flat shadow
    // vars, see scalarizableTupleVars_). `hintName` is only for error messages; `fieldRef(i)`
    // returns the IRRef for slot i in whichever representation the caller is using.
    //
    // `idxRef` is the ALREADY-LOWERED index value, still 1-based per AC convention — callers
    // lower it themselves (via lowerExprNode for the structured-AST path, lowerExpr for the
    // legacy string-expression path — see lowerTupleIndex's/lowerTupleScalarIndex's own call
    // sites) BEFORE calling in here, so this core has no dependency on which lowering path
    // produced it. A CONSTANT index (idxRef is a genuine IRRef CONST int, not merely written
    // as a literal in the source — the two paths fold identically) redirects straight to the
    // matching field — zero cost, works for every tuple including `; any`. A non-constant
    // index needs a bounded runtime dispatch (one arm per known slot, EQ-tested against the
    // index, first match wins) — safe ONLY when every slot shares one type, since IRType has
    // no tagged-union/Any variant this compiler's whole per-var static-type-inference
    // machinery could route a dynamically-selected read through. An `; any` tuple is therefore
    // constant-index-only ("t[i] should be dynamic except for 'any' tuples" — Abu's own spec):
    // rejected here as a compile-time error, not deferred to a runtime failure, since the type
    // mismatch is already knowable at compile time.
    IRRef lowerTupleIndexCore(const std::string& hintName, const std::vector<IRType>& slotTypes,
                              bool isAny, const IRRef& idxRef,
                              const std::function<IRRef(size_t)>& fieldRef) {
        size_t arity = slotTypes.size();

        if (idxRef.kind == IRRef::Kind::CONST && idxRef.value.type == IRType::INT) {
            long long idx1 = std::get<int64_t>(idxRef.value.data);
            if (idx1 < 1 || (size_t)idx1 > arity)
                throw ACError::semantic("tuple index " + std::to_string(idx1) +
                    " is out of range for a " + std::to_string(arity) + "-element tuple");
            return fieldRef((size_t)(idx1 - 1));
        }

        if (isAny)
            throw ACError::semantic("`" + hintName + "` is an `; any` tuple — only a constant "
                "index is allowed on it (a dynamic index has no single type to give the result)");
        if (arity == 0) return mkConstInt(0);

        // Bounded dispatch — same IF_BEGIN/IF_ELSE/IF_END (high-level IR) / label+jump
        // (low-level, ASM/BNY) shape TernaryExpr's own lowering already establishes, so no
        // backend-specific codegen is needed here either. An index that matches no arm at
        // runtime (out of range) leaves the result at its placeholder zero value — a safe,
        // silent fallback rather than a crash, matching this compiler's general lenient-
        // runtime-conversion style (e.g. ac_atoi's own silent-0-on-failure convention).
        // "_ac_tupidx_" — LEADING underscore, matching every other synthetic name in this
        // pass (_ac_tup_*, _ac_swap_*). A bare "ac_"-prefixed name (no leading underscore)
        // collides with an unrelated, pre-existing convention: CStrategy's isKnownFloatName
        // (and its siblings) treats ANY "ac_"-prefixed identifier as a known runtime float
        // constant/helper (ac_pi, etc.) — verified real bug: an all-int tuple's dynamic-index
        // read printed "10.0" instead of "10" purely because its var happened to start with
        // "ac_", nothing to do with setRefType (which was already correct).
        thread_local int tupIdxC = 0;
        std::string rn = "_ac_tupidx_" + std::to_string(tupIdxC++);
        IRRef resV = mkVar(rn);
        IRRef rawIdx = idxRef;
        IRType elemType = slotTypes[0];   // homogeneous, guaranteed — `isAny` already handled above
        setRefType(resV, elemType);       // real type on record for anything that consults
                                           // prog.symbols directly (TYPE_CAST decisions, etc.)
        IRRef zeroVal = elemType == IRType::FLOAT  ? IRRef::constant(IRValue(0.0))
                      : elemType == IRType::STRING ? mkConst("")
                      : mkConstInt(0);
        emit(IRInstruction(IROpcode::STORE_VAR, resV, {zeroVal}));

        if (prog.useHighLevelIR) {
            for (size_t i = 0; i < arity; i++) {
                IRRef eq = mkTemp();
                emit(IRInstruction(IROpcode::EQ, eq, {rawIdx, mkConstInt((int64_t)(i + 1))}));
                IRInstruction ib(IROpcode::IF_BEGIN); ib.typedOperands = {eq}; emit(std::move(ib));
                emit(IRInstruction(IROpcode::STORE_VAR, resV, {fieldRef(i)}));
                if (i + 1 < arity) emit(IRInstruction(IROpcode::IF_ELSE));
            }
            for (size_t i = 0; i < arity; i++) emit(IRInstruction(IROpcode::IF_END));
        } else {
            IRRef endL = mkLabel();
            for (size_t i = 0; i < arity; i++) {
                IRRef eq = mkTemp();
                emit(IRInstruction(IROpcode::EQ, eq, {rawIdx, mkConstInt((int64_t)(i + 1))}));
                IRRef nextL = (i + 1 < arity) ? mkLabel() : endL;
                emitJF(eq, nextL);
                emit(IRInstruction(IROpcode::STORE_VAR, resV, {fieldRef(i)}));
                emitJump(endL);
                if (i + 1 < arity) emitLabel(nextL);
            }
            emitLabel(endL);
        }
        return resV;
    }

    // `t[i]` on a known ESCAPING tuple instance var (a real synthesized bundle). `idxRef` must
    // already be lowered by the caller (see lowerTupleIndexCore's own comment).
    IRRef lowerTupleIndex(const std::string& varName, const std::string& className, const IRRef& idxRef) {
        auto stIt = tupleClassSlotTypes_.find(className);
        static const std::vector<IRType> emptySlots;
        const std::vector<IRType>& slotTypes = stIt != tupleClassSlotTypes_.end() ? stIt->second : emptySlots;
        bool isAny = tupleAnyClasses_.count(className) > 0;
        return lowerTupleIndexCore(varName, slotTypes, isAny, idxRef,
            [&](size_t i) { return mkVar(varName + ".f" + std::to_string(i)); });
    }

    // `t[i]` on a known SCALARIZED tuple var (flat shadow vars, no bundle at all). `idxRef`
    // must already be lowered by the caller (see lowerTupleIndexCore's own comment).
    IRRef lowerTupleScalarIndex(const std::string& varName, const std::string& scalKey,
                                bool isAny, const IRRef& idxRef) {
        auto tyIt = tupleScalarShadowTypes_.find(scalKey);
        static const std::vector<IRType> emptySlots;
        const std::vector<IRType>& slotTypes = tyIt != tupleScalarShadowTypes_.end() ? tyIt->second : emptySlots;
        return lowerTupleIndexCore(varName, slotTypes, isAny, idxRef,
            [&](size_t i) { return mkVar(tupleScalarShadowName(varName, i)); });
    }

    // `from ilib math use sin` (selective import): maps the bare alias name ("sin") to its
    // fully-qualified ilib call name ("math.sin"). Populated when the UseLibStmt is lowered
    // (imports are always written before use, single forward pass — same assumption every
    // other incrementally-built lowering table in this class already makes). Consulted by
    // every CallExpr/FunctionCall lowering site so a bare `sin(x)` after a selective import
    // is treated EXACTLY like `math.sin(x)` from that point on — every backend's existing
    // float-return-type inference, calling-convention marshaling, and symbol-name tables
    // already handle the qualified form correctly, so rewriting here (once, backend-agnostic)
    // avoids needing N separate per-backend "recognize this bare alias" mechanisms. Before
    // this fix, only PythonStrategy (`sin = math.sin` alias) and CppStrategy (an incomplete
    // `using ac_math::sym;` with a cmath-collision skip-list) had ANY handling at all, and
    // both were still wrong — Python was actually fine, but Rust/Go/V/Java hard-failed
    // ("sin not found in this scope") and C/C++ silently called the WRONG function's return
    // type (libc's real `::sin` exists too, so it link-resolved, but the destination var's
    // type was inferred as int instead of float since no float-return check recognized the
    // bare name — verified: `ac_int y = sin(t_1)` printed `0` instead of `0.0`).
    std::unordered_map<std::string,std::string> selectiveImportAliases_;

    // loop label stacks for break/continue
    std::stack<IRRef> loopStart;
    std::stack<IRRef> loopEnd;

    // type tracking: temp id → IRType (propagated from literals and operations)
    std::unordered_map<int,IRType> tempTypes_;

    // Which VAR symbol ids have EVER been the destination of a plain assignment — separate from
    // "what type is it currently tracked as" (prog.symbols.getType). A var's first-ever
    // assignment must stay a plain STORE_VAR even when its RHS type is unknown (VOID), but a
    // LATER reassignment whose RHS type doesn't match the CURRENT one must still get a TYPE_CAST
    // even when the var's tracked type is ALSO VOID (a function-call result: `typeOfRef` returns
    // VOID until backend-specific return-type inference runs, well after ir.cpp's lowering).
    // Without this set, `x = getNum(); x = $hello$` silently stayed a plain STORE_VAR (no TYPE_CAST at
    // all — verified via --stop-after-ir) since oldType read VOID either way, indistinguishable
    // from a genuine first assignment — the retype was never even detected, let alone lowered
    // correctly; every backend's emitTypeCast had nothing to react to.
    std::set<int> everAssignedVarIds_;

    // Infer IRType from any ref (CONST → embedded type, VAR/TEMP → tracked type)
    IRType typeOfRef(const IRRef& r) const {
        if (r.kind == IRRef::Kind::CONST)  return r.value.type;
        if (r.kind == IRRef::Kind::VAR)    return prog.symbols.getType(r.id);
        if (r.kind == IRRef::Kind::TEMP) {
            auto it = tempTypes_.find(r.id);
            return it != tempTypes_.end() ? it->second : IRType::VOID;
        }
        return IRType::VOID;
    }

    // Record the type of a temp or var ref
    void setRefType(const IRRef& r, IRType t) {
        if (r.kind == IRRef::Kind::TEMP) tempTypes_[r.id] = t;
        if (r.kind == IRRef::Kind::VAR)  prog.symbols.setType(r.id, t);
    }

    // `atomic x = e`'s declaration lowering (TypeCoerceStmt, ATOMIC case) already calls
    // setRefType(varRef, IRType::ATOMIC), which records it via prog.symbols.setType —
    // queryable here for any LATER reassignment to the same var, since prog.symbols is
    // shared program-wide state, not per-statement. Used to close the atomic
    // read-modify-write race: see the compound-assignment cases below.
    bool isAtomicRef(const IRRef& r) const {
        return r.kind == IRRef::Kind::VAR && prog.symbols.getType(r.id) == IRType::ATOMIC;
    }

    // ── allocation helpers ──────────────────────────────────────────────────

    IRRef mkTemp() {
        IRRef r; r.kind = IRRef::Kind::TEMP; r.id = tc++;
        return r;
    }
    IRRef mkLabel() {
        IRRef r; r.kind = IRRef::Kind::LABEL; r.id = lc++;
        return r;
    }
    
    // NEW: Use symbol table for variable references
    IRRef mkVar(const std::string& name) {
        int id = prog.symbols.lookup(name);
        if (id < 0) {
            // Variable not in symbol table yet, intern it
            id = prog.symbols.intern(name);
        }
        return IRRef::var(id);
    }
    
    static IRRef mkConst(const std::string& s) {
        return IRRef::constant(IRValue(s));
    }
    static IRRef mkConstInt(int64_t v) {
        return IRRef::constant(IRValue(v));
    }

    // Find the first comma NOT nested inside (), [], {} — so "f(a,b),c" splits at the
    // top-level comma, not the inner one. Returns npos if there is no top-level comma.
    static size_t topLevelComma(const std::string& s) {
        int depth = 0;
        for (size_t i = 0; i < s.size(); ++i) {
            char c = s[i];
            if (c == '(' || c == '[' || c == '{') ++depth;
            else if (c == ')' || c == ']' || c == '}') { if (depth > 0) --depth; }
            else if (c == ',' && depth == 0) return i;
        }
        return std::string::npos;
    }

    // Convert 1-based index (AC) to 0-based (target languages)
    IRRef adjustIndex(const IRRef& idx) {
        // DICT keys are strings — pass through untouched (the -1 adjust is for LIST positions;
        // SUB(stringconst, 1) folded to -1 and broke every dict read: ages[$bob$] → ages[-1]).
        // typeOfRef covers a string CONST, a string VAR, AND a computed string TEMP
        // (e.g. ages[first + last], ages[getKey()]) — all of which are dict keys, not positions.
        if (typeOfRef(idx) == IRType::STRING)
            return idx;
        if (idx.kind == IRRef::Kind::CONST && idx.value.type == IRType::INT) {
            int64_t n = std::get<int64_t>(idx.value.data);
            return mkConstInt(n - 1);
        }
        // Variable/temp index: emit SUB(idx, 1)
        IRRef one = mkConstInt(1);
        IRRef adj = mkTemp();
        IRInstruction i(IROpcode::SUB, adj, {idx, one});
        emit(std::move(i));
        return adj;
    }

    // ── RAII scope guard ────────────────────────────────────────────────────
    struct ScopeGuard {
        SymbolTable& sym;
        ScopeGuard(SymbolTable& s) : sym(s) { sym.enterScope(); }
        ~ScopeGuard() { sym.exitScope(); }
    };

    // ── emit helpers ────────────────────────────────────────────────────────

    void rawEmit(IRInstruction i) {
        if (cur) {
            cur->instructions.push_back(std::move(i));
        } else {
            if (inMainSection) prog.mainSection.push_back(i);
            else               prog.dataSection.push_back(i);
            prog.globalInit.push_back(std::move(i));
        }
    }

    void emit(IRInstruction i) {
        // Inside a <free> block: auto-emit FREE_DECL before the first assignment to each var
        if (inFreeScope && i.opcode == IROpcode::STORE_VAR
                        && i.result.kind == IRRef::Kind::VAR) {
            int id = i.result.id;
            if (!freeDeclaredIds.count(id)) {
                freeDeclaredIds.insert(id);
                IRInstruction fd(IROpcode::FREE_DECL);
                fd.typedOperands = {i.result};
                rawEmit(std::move(fd));
            }
        }
        // Inside a <bound> block: same mechanism as <free> above, but the FREE_DECL carries
        // the "bound" attr — exempts the var from loop save/restore (so it survives the
        // enclosing loop, matching <free>) WITHOUT promoting it to a true program global
        // (mirrors the `bound var = expr` statement form's own FreeDecl lowering, see its
        // comment). <bound> was previously a pure no-op (TAG_BEGIN/TAG_END with zero
        // scoping effect) despite tags.hpp documenting real "cannot leak out" semantics.
        if (inBoundScope && i.opcode == IROpcode::STORE_VAR
                         && i.result.kind == IRRef::Kind::VAR) {
            int id = i.result.id;
            if (!boundDeclaredIds.count(id)) {
                boundDeclaredIds.insert(id);
                IRInstruction fd(IROpcode::FREE_DECL);
                fd.typedOperands = {i.result};
                fd.attrs.push_back("bound");
                rawEmit(std::move(fd));
            }
        }
        rawEmit(std::move(i));
    }

    // emit a LABEL definition
    void emitTag(IROpcode op, const std::string& name) {
        IRInstruction i(op);
        i.typedOperands = {IRRef::constant(IRValue(name))};
        rawEmit(std::move(i)); // bypass free-scope interceptor for structural markers
    }

    void emitLabel(const IRRef& lbl) {
        IRInstruction i(IROpcode::LABEL);
        i.typedOperands = {lbl};
        emit(std::move(i));
    }

    // emit an unconditional jump
    void emitJump(const IRRef& lbl) {
        IRInstruction i(IROpcode::JUMP);
        i.typedOperands = {lbl};
        emit(std::move(i));
    }

    // emit conditional jump (jf cond, label)
    void emitJF(const IRRef& cond, const IRRef& lbl) {
        IRInstruction i(IROpcode::JUMP_IF_FALSE);
        i.typedOperands = {cond, lbl};
        emit(std::move(i));
    }

    // ── expression lowering ─────────────────────────────────────────────────
    // New method: Lower structured AST expression nodes to IR
    IRRef lowerExprNode(const ASTNode& expr) {
        switch (expr.type) {
            case NodeType::LiteralExpr: {
                // Literal values: int, float, string, bool
                if (!expr.attrs.empty()) {
                    const std::string& typeStr = expr.attrs[0];
                    if (typeStr == "INT") {
                        try {
                            // std::stoll defaults to base 10, which silently truncates a hex
                            // literal at its first non-decimal-digit character — "0xDEADBEEF"
                            // parsed as base 10 stops at '0' (the leading zero) before ever
                            // reaching 'x', returning 0 with no error (base-10 accepts a lone
                            // "0" and treats 'x' as trailing garbage stoll simply ignores).
                            // Hex literals are the ONLY non-decimal form the lexer emits (see
                            // lexer.cpp's NUMBER scan — only "0x"/"0X" triggers a separate hex
                            // path), so detecting that exact prefix and parsing base 16 is safe
                            // without risking reinterpreting an ordinary leading-zero decimal
                            // literal (e.g. "010") as octal, which a blanket base-0 auto-detect
                            // would do.
                            bool isHex = expr.value.size() > 2 && expr.value[0] == '0' &&
                                         (expr.value[1] == 'x' || expr.value[1] == 'X');
                            return mkConstInt(std::stoll(expr.value, nullptr, isHex ? 16 : 10));
                        } catch (...) {
                            return mkConst(expr.value);
                        }
                    } else if (typeStr == "FLOAT") {
                        try {
                            return IRRef::constant(IRValue(std::stod(expr.value)));
                        } catch (...) {
                            return mkConst(expr.value);
                        }
                    } else if (typeStr == "STRING") {
                        return mkConst(expr.value);
                    } else if (typeStr == "BOOL") {
                        bool val = (expr.value == "true");
                        return IRRef::constant(IRValue(val));
                    } else if (typeStr == "NULL") {
                        return mkConst("null");
                    } else if (typeStr == "NIL") {
                        return mkConst("nil");
                    }
                }
                // null/nil literal as bare string
                if (expr.value == "null") return mkConst("null");
                if (expr.value == "nil")  return mkConst("nil");
                return mkConst(expr.value);
            }
            
            case NodeType::Identifier: {
                // Remap AC direction constants to string literals
                static const std::unordered_map<std::string,std::string> dirConsts = {
                    {"RightDir","right"}, {"LeftDir","left"},
                    {"UpDir","up"},       {"DownDir","down"},
                };
                auto dc = dirConsts.find(expr.value);
                if (dc != dirConsts.end()) return mkConst(dc->second);
                return mkVar(expr.value);
            }
            
            case NodeType::BinaryExpr: {
                // Binary operation: left op right
                if (expr.children.size() >= 2 && expr.children[0] && expr.children[1]) {
                    // LIST REPETITION: [elem, ...] @ n → list of n copies of the whole element
                    // sequence (alloc + fill loop — rides existing ALLOC/append machinery, so
                    // it works on EVERY backend). A multi-element list literal in this position
                    // has empty .children and a raw comma-joined .value ("1,2") — split it so
                    // each repeat appends every element instead of feeding the whole "1,2" text
                    // to a single append() call (see splitTopLevelCommas's own comment).
                    if ((expr.value == "@" || expr.value == "*") &&
                        expr.children[0]->type == NodeType::ListLiteral &&
                        (!expr.children[0]->children.empty() || !expr.children[0]->value.empty())) {
                        thread_local int repC = 0;
                        std::string dstName = "ac_rep_" + std::to_string(repC);
                        std::string idxName = "ac_repi_" + std::to_string(repC); repC++;
                        IRRef dstV = mkVar(dstName), idxV = mkVar(idxName);
                        std::vector<IRRef> elems;
                        if (!expr.children[0]->children.empty()) {
                            for (auto& c : expr.children[0]->children) elems.push_back(lowerExprNode(*c));
                        } else {
                            for (auto& piece : splitTopLevelCommas(expr.children[0]->value)) elems.push_back(lowerExpr(piece));
                        }
                        IRRef count = lowerExprNode(*expr.children[1]);
                        emit(IRInstruction(IROpcode::ALLOC, dstV, {mkConst("list"), mkConst("")}));
                        emit(IRInstruction(IROpcode::STORE_VAR, idxV, {mkConstInt(0)}));
                        if (prog.useHighLevelIR) {
                            IRRef brk = mkConst("__break__");
                            loopEnd.push(brk); loopStart.push(mkConst("__continue__"));
                            emit(IRInstruction(IROpcode::WHILE_BEGIN));
                            IRRef c = mkTemp();
                            emit(IRInstruction(IROpcode::LT, c, {idxV, count}));
                            { IRInstruction jf(IROpcode::JUMP_IF_FALSE); jf.typedOperands = {c, brk}; emit(std::move(jf)); }
                            for (auto& elem : elems) {
                                IRInstruction ap(IROpcode::LIB_CALL);
                                ap.typedOperands = {mkConst(dstName + ".append"), elem}; emit(std::move(ap));
                            }
                            IRRef inc = mkTemp();
                            emit(IRInstruction(IROpcode::ADD, inc, {idxV, mkConstInt(1)}));
                            emit(IRInstruction(IROpcode::STORE_VAR, idxV, {inc}));
                            emit(IRInstruction(IROpcode::WHILE_END));
                            loopEnd.pop(); loopStart.pop();
                        } else {
                            IRRef startL = mkLabel(), endL = mkLabel();
                            emitLabel(startL);
                            IRRef c = mkTemp();
                            emit(IRInstruction(IROpcode::LT, c, {idxV, count}));
                            emitJF(c, endL);
                            for (auto& elem : elems) {
                                IRInstruction ap(IROpcode::LIB_CALL);
                                ap.typedOperands = {mkConst(dstName + ".append"), elem}; emit(std::move(ap));
                            }
                            IRRef inc = mkTemp();
                            emit(IRInstruction(IROpcode::ADD, inc, {idxV, mkConstInt(1)}));
                            emit(IRInstruction(IROpcode::STORE_VAR, idxV, {inc}));
                            emitJump(startL);
                            emitLabel(endL);
                        }
                        return dstV;
                    }

                    // SHORT-CIRCUIT and/or: `a and b` must not evaluate b when a is false
                    // (V's bounds-checking exposed `j > 0 and arr[j] > key` evaluating arr[-1]).
                    if (expr.value == "and" || expr.value == "or") {
                        thread_local int scC = 0;
                        std::string rn = "ac_sc_" + std::to_string(scC++);
                        IRRef resV = mkVar(rn);
                        IRRef lRef = lowerExprNode(*expr.children[0]);
                        IRRef lb = mkTemp();
                        emit(IRInstruction(IROpcode::NEQ, lb, {lRef, mkConstInt(0)}));
                        emit(IRInstruction(IROpcode::STORE_VAR, resV, {lb}));
                        bool isAnd = expr.value == "and";
                        auto emitRhs = [&]() {
                            IRRef rRef = lowerExprNode(*expr.children[1]);
                            IRRef rb = mkTemp();
                            emit(IRInstruction(IROpcode::NEQ, rb, {rRef, mkConstInt(0)}));
                            emit(IRInstruction(IROpcode::STORE_VAR, resV, {rb}));
                        };
                        if (prog.useHighLevelIR) {
                            IRRef condT = mkTemp();
                            if (isAnd) emit(IRInstruction(IROpcode::NEQ, condT, {resV, mkConstInt(0)}));
                            else       emit(IRInstruction(IROpcode::EQ,  condT, {resV, mkConstInt(0)}));
                            { IRInstruction ib(IROpcode::IF_BEGIN); ib.typedOperands = {condT}; emit(std::move(ib)); }
                            emitRhs();
                            emit(IRInstruction(IROpcode::IF_END));
                        } else {
                            IRRef skipL = mkLabel();
                            if (isAnd) emitJF(resV, skipL);
                            else {
                                IRRef c = mkTemp();
                                emit(IRInstruction(IROpcode::EQ, c, {resV, mkConstInt(0)}));
                                emitJF(c, skipL);
                            }
                            emitRhs();
                            emitLabel(skipL);
                        }
                        return resV;
                    }

                    IRRef lRef = lowerExprNode(*expr.children[0]);
                    IRRef rRef = lowerExprNode(*expr.children[1]);
                    IRRef dst = mkTemp();

                    // Map operator string to IROpcode
                    IROpcode opcode = IROpcode::NOP;
                    const std::string& op = expr.value;
                    // 16b: arithmetic requires NUMERIC operands (ADD excluded — string concat).
                    {
                        auto isStrConst = [](const ASTNode* n) {
                            return n && n->type == NodeType::LiteralExpr && !n->attrs.empty() && n->attrs[0] == "STRING";
                        };
                        bool l = expr.children.size() >= 2 && isStrConst(expr.children[0].get());
                        bool r = expr.children.size() >= 2 && isStrConst(expr.children[1].get());
                        // These require numeric operands (ADD excluded — string concat).
                        if ((op == "-" || op == "*" || op == "/" || op == "//" || op == "///") && (l || r))
                            throw ACError::nonNumericArith(op);
                        // `@` is polymorphic multiply: `str @ n` / `list @ n` repeat are valid;
                        // only `str @ str` is meaningless.
                        if (op == "@" && l && r)
                            throw ACError::nonNumericArith(op);
                    }
                    if (op == "+") opcode = IROpcode::ADD;
                    else if (op == "-") opcode = IROpcode::SUB;
                    else if (op == "*") opcode = IROpcode::MUL;
                    else if (op == "@") opcode = IROpcode::PMUL;
                    else if (op == "/") opcode = IROpcode::DIV;
                    else if (op == "//") opcode = IROpcode::IDIV;
                    else if (op == "///") opcode = IROpcode::FDIV;
                    else if (op == "is") opcode = IROpcode::EQ;
                    else if (op == "#=") opcode = IROpcode::NEQ;
                    else if (op == "<") opcode = IROpcode::LT;
                    else if (op == ">") opcode = IROpcode::GT;
                    else if (op == "#>") opcode = IROpcode::LTE;   // NOT greater = ≤
                    else if (op == "#<") opcode = IROpcode::GTE;   // NOT less = ≥
                    // Logical operators
                    else if (op == "and") opcode = IROpcode::AND;
                    else if (op == "or")  opcode = IROpcode::OR;
                    else if (op == "xor") opcode = IROpcode::XOR;
                    else if (op == "#|")  opcode = IROpcode::XNOR;

                    // Bitwise operators (word forms; legacy symbols & and | kept for old sources)
                    else if (op == "band" || op == "&") opcode = IROpcode::BAND;
                    else if (op == "bxor" || op == "|") opcode = IROpcode::BXOR;
                    else if (op == "bor")               opcode = IROpcode::BOR;

                    else if (op == "xsub") opcode = IROpcode::XSUB;

                    if (op == "overlap") {
                        // Extract base object name (before first dot) from each operand
                        auto extractBase = [this](const IRRef& ref) -> std::string {
                            std::string n;
                            if (ref.kind == IRRef::Kind::VAR) n = prog.symbols.getName(ref.id);
                            else if (ref.kind == IRRef::Kind::CONST && ref.value.type == IRType::STRING)
                                n = std::get<std::string>(ref.value.data);
                            auto dot = n.find('.');
                            return (dot != std::string::npos) ? n.substr(0, dot) : n;
                        };
                        IRRef dst = mkTemp();
                        IRInstruction libcall(IROpcode::LIB_CALL, dst, {});
                        // `X.hitbox overlap boundary` — bare `boundary` as the RHS operand routes
                        // to the real (already-implemented) ac_gl_hitbox_overlap_boundary(name)
                        // instead of the two-object overlap check. `boundary` isn't a keyword, so
                        // this is purely a text match on the extracted base name — matches how
                        // `X.overlap_boundary()` (method-call syntax) already reaches the same
                        // function; this just gives the infix-operator form the same destination
                        // (verified: examples/pong.ac's `ball.hitbox overlap boundary`).
                        std::string rBase = extractBase(rRef);
                        if (rBase == "boundary") {
                            libcall.typedOperands.push_back(mkConst("gl:hitbox.overlap_boundary"));
                            libcall.typedOperands.push_back(mkConst(extractBase(lRef)));
                        } else if (!rBase.empty() && rBase.back() == '%') {
                            // `ball.hitbox.coords overlap p%.hitbox.coords` — a trailing-`%`
                            // object-name PATTERN (parser.cpp folds the `%` into the identifier
                            // text — see its matching comment) checks overlap against every
                            // currently-live object whose name starts with the text before the
                            // `%`, not one fixed object. `ac_gl_hitbox_overlap_pattern(name,
                            // pattern)` already exists and does exactly this (gl.cpp's `_wmatch`
                            // against the live object registry) — it was simply never reachable
                            // from AC source before, since `%` was a hard parse error. Routes
                            // through the same `gl:X.Y -> ac_gl_X_Y` LibLowering fallback every
                            // other gl:hitbox.* call already uses — no new backend/.acl wiring.
                            libcall.typedOperands.push_back(mkConst("gl:hitbox.overlap_pattern"));
                            libcall.typedOperands.push_back(mkConst(extractBase(lRef)));
                            libcall.typedOperands.push_back(mkConst(rBase));
                        } else {
                            libcall.typedOperands.push_back(mkConst("gl:hitbox.overlap"));
                            libcall.typedOperands.push_back(mkConst(extractBase(lRef)));
                            libcall.typedOperands.push_back(mkConst(rBase));
                        }
                        emit(std::move(libcall));
                        return dst;
                    }

                    if (op == "^") {
                        // Exponentiation: integer power builtin ac_ipow(base, exp).
                        // Standalone (no math-lib dependency) and integer-typed so `2^10 == 1024`
                        // consistently on every backend. (math.pow stays float via the math lib.)
                        // Fold literal^literal at compile time — zero runtime cost, and it lets
                        // constant powers work on BNY (which has no ac_ipow helper).
                        if (lRef.kind == IRRef::Kind::CONST && rRef.kind == IRRef::Kind::CONST &&
                            lRef.value.type == IRType::INT && rRef.value.type == IRType::INT) {
                            int64_t base = std::get<int64_t>(lRef.value.data);
                            int64_t exp  = std::get<int64_t>(rRef.value.data);
                            if (exp >= 0) {
                                // Bound the loop so a huge constant exponent (e.g. 2 ^ 1000000000)
                                // can't spin the compiler; compute in UNSIGNED so overflow wraps
                                // (defined) instead of signed-overflow UB. Past 2^64 every result is
                                // int64-overflow garbage anyway, so the cap loses nothing meaningful.
                                uint64_t r = 1, ub = (uint64_t)base;
                                int64_t iters = exp < 4096 ? exp : 4096;
                                for (int64_t k = 0; k < iters; k++) r *= ub;
                                return mkConstInt((int64_t)r);
                            }
                        }
                        IRRef dst = mkTemp();
                        emit(IRInstruction(IROpcode::CALL, dst, {mkVar("ac_ipow"), lRef, rRef}));
                        return dst;
                    }

                    if (op == "ptm" || op == "ptd") {
                        // Power-two multiply/divide — LITERAL bit shifts (for speed).
                        // const<<const folds; otherwise emit PTM/PTD (a << b / a >> b).
                        if (lRef.kind == IRRef::Kind::CONST && lRef.value.type == IRType::INT &&
                            rRef.kind == IRRef::Kind::CONST && rRef.value.type == IRType::INT) {
                            int64_t a = std::get<int64_t>(lRef.value.data);
                            int64_t b = std::get<int64_t>(rRef.value.data);
                            // Only fold an in-range shift (0..63). A shift >= 64 or negative is UB;
                            // left-shift via unsigned so a negative `a` doesn't hit signed-shift UB.
                            if (b >= 0 && b < 64)
                                return mkConstInt(op == "ptm" ? (int64_t)((uint64_t)a << b) : (a >> b));
                            // out-of-range shift → leave as a runtime PTM/PTD op (don't fold to UB)
                        }
                        // A non-constant (or constant-but-out-of-range) shift amount reaches
                        // every backend's own `<<`/`>>` codegen UNMASKED — every one of those
                        // is a REAL 64-bit shift in the target language too, so a runtime
                        // amount >= 64 is genuine UB there as well, not just a folding
                        // shortcut this compiler skips. Verified real bug: `1 ptm 65` printed
                        // "0" on C (with a "shift count >= width of type" gcc warning) — no
                        // per-backend fix needed, since masking the amount to 0-63 HERE, once,
                        // in the IR, matches real x86 hardware shift semantics anyway (`shl`/
                        // `shr` already mask their count by 63 in silicon), so every backend's
                        // shift becomes well-defined for free.
                        IRRef bMasked = rRef;
                        if (!(rRef.kind == IRRef::Kind::CONST && rRef.value.type == IRType::INT
                                && std::get<int64_t>(rRef.value.data) >= 0
                                && std::get<int64_t>(rRef.value.data) < 64)) {
                            bMasked = mkTemp();
                            emit(IRInstruction(IROpcode::BAND, bMasked, {rRef, mkConstInt(63)}));
                        }
                        IRRef dst = mkTemp();
                        emit(IRInstruction(op == "ptm" ? IROpcode::PTM : IROpcode::PTD,
                                           dst, {lRef, bMasked}));
                        return dst;
                    }

                    if (opcode != IROpcode::NOP) {
                        // Constant folding: both operands are compile-time constants
                        if (lRef.kind == IRRef::Kind::CONST && rRef.kind == IRRef::Kind::CONST) {
                            auto& L = lRef.value;
                            auto& R = rRef.value;
                            bool lFloat = L.type == IRType::FLOAT;
                            bool rFloat = R.type == IRType::FLOAT;
                            bool anyFloat = lFloat || rFloat;
                            auto asDouble = [](const IRValue& v) -> double {
                                if (v.type == IRType::FLOAT) return std::get<double>(v.data);
                                if (v.type == IRType::INT)   return (double)std::get<int64_t>(v.data);
                                return 0.0;
                            };
                            auto asInt = [](const IRValue& v) -> int64_t {
                                if (v.type == IRType::INT)   return std::get<int64_t>(v.data);
                                if (v.type == IRType::FLOAT) return (int64_t)std::get<double>(v.data);
                                if (v.type == IRType::BOOL)  return std::get<bool>(v.data) ? 1 : 0;
                                return 0;
                            };
                            auto asBool = [&](const IRValue& v) -> bool { return asInt(v) != 0; };
                            IRRef folded;
                            bool did_fold = true;
                            switch (opcode) {
                                case IROpcode::ADD:
                                    if (L.type == IRType::STRING || R.type == IRType::STRING) {
                                        auto toStr = [](const IRValue& v) -> std::string {
                                            if (v.type == IRType::STRING) return std::get<std::string>(v.data);
                                            if (v.type == IRType::INT)    return std::to_string(std::get<int64_t>(v.data));
                                            if (v.type == IRType::FLOAT)  return std::to_string(std::get<double>(v.data));
                                            if (v.type == IRType::BOOL)   return std::get<bool>(v.data) ? "true" : "false";
                                            return "";
                                        };
                                        folded = IRRef::constant(IRValue(toStr(L) + toStr(R)));
                                    } else {
                                        // Integer folds wrap in UNSIGNED: signed overflow is UB, and
                                        // the two's-complement wrap matches every backend's int64 runtime.
                                        folded = anyFloat ? IRRef::constant(IRValue(asDouble(L)+asDouble(R)))
                                                          : IRRef::constant(IRValue((int64_t)((uint64_t)asInt(L)+(uint64_t)asInt(R))));
                                    }
                                    break;
                                case IROpcode::SUB:  folded = anyFloat ? IRRef::constant(IRValue(asDouble(L)-asDouble(R))) : IRRef::constant(IRValue((int64_t)((uint64_t)asInt(L)-(uint64_t)asInt(R)))); break;
                                case IROpcode::MUL: folded = anyFloat ? IRRef::constant(IRValue(asDouble(L)*asDouble(R))) : IRRef::constant(IRValue((int64_t)((uint64_t)asInt(L)*(uint64_t)asInt(R)))); break;
                                case IROpcode::PMUL:
                                    // Polymorphic multiply: `str @ n` repeats the string.
                                    if (L.type == IRType::STRING && R.type == IRType::INT) {
                                        const std::string& s = std::get<std::string>(L.data);
                                        int64_t n = std::get<int64_t>(R.data);
                                        std::string outp;
                                        if (n > 0 && n < 1000000) { outp.reserve(s.size()*(size_t)n); for (int64_t i=0;i<n;i++) outp += s; }
                                        folded = IRRef::constant(IRValue(outp));
                                    } else {
                                        folded = anyFloat ? IRRef::constant(IRValue(asDouble(L)*asDouble(R))) : IRRef::constant(IRValue(asInt(L)*asInt(R)));
                                    }
                                    break;
                                case IROpcode::DIV: {
                                    double dR = asDouble(R);
                                    if (dR == 0.0) { did_fold = false; break; }
                                    double q = asDouble(L) / dR;
                                    int64_t qi = (int64_t)q;
                                    folded = (q == (double)qi)
                                             ? IRRef::constant(IRValue(qi))
                                             : IRRef::constant(IRValue(q));
                                    break;
                                }
                                case IROpcode::FDIV: {
                                    double dR = asDouble(R);
                                    if (dR == 0.0) { did_fold = false; break; }
                                    folded = IRRef::constant(IRValue(asDouble(L) / dR));
                                    break;
                                }
                                case IROpcode::IDIV: {
                                    int64_t iR = asInt(R);
                                    if (iR == 0) { did_fold = false; break; }
                                    folded = IRRef::constant(IRValue(asInt(L) / iR));
                                    break;
                                }
                                case IROpcode::MOD: {
                                    int64_t b = asInt(R);
                                    if (b == 0) { did_fold = false; break; }
                                    int64_t r = asInt(L) % b;                    // floor-mod (matches PY/runtime)
                                    if (r != 0 && ((r < 0) != (b < 0))) r += b;
                                    folded = IRRef::constant(IRValue(r));
                                    break;
                                }
                                // Comparisons/logicals fold to int64 0/1 (AC's canonical truth —
                                // matches BNY/C runtime output and keeps typed backends compiling;
                                // a bool const like `true` can't initialise an int64 in Go/Java/Rust).
                                case IROpcode::EQ:  case IROpcode::NEQ:
                                case IROpcode::LT:  case IROpcode::GT:
                                case IROpcode::LTE: case IROpcode::GTE: {
                                    // STRING operands must compare as strings, not via asInt()
                                    // (which returns 0 for any string → "cat" is "dog" folded to 1).
                                    if (L.type == IRType::STRING || R.type == IRType::STRING) {
                                        // Only fold string-vs-string; mixed string/number → defer to runtime.
                                        if (L.type != IRType::STRING || R.type != IRType::STRING) { did_fold = false; break; }
                                        int c = std::get<std::string>(L.data).compare(std::get<std::string>(R.data));
                                        bool r = (opcode==IROpcode::EQ) ? (c==0) : (opcode==IROpcode::NEQ) ? (c!=0)
                                               : (opcode==IROpcode::LT) ? (c<0)  : (opcode==IROpcode::GT)  ? (c>0)
                                               : (opcode==IROpcode::LTE)? (c<=0) : (c>=0);
                                        folded = mkConstInt(r ? 1 : 0);
                                        break;
                                    }
                                    bool r = (opcode==IROpcode::EQ)  ? (anyFloat ? asDouble(L)==asDouble(R) : asInt(L)==asInt(R))
                                           : (opcode==IROpcode::NEQ) ? (anyFloat ? asDouble(L)!=asDouble(R) : asInt(L)!=asInt(R))
                                           : (opcode==IROpcode::LT)  ? (anyFloat ? asDouble(L)< asDouble(R) : asInt(L)< asInt(R))
                                           : (opcode==IROpcode::GT)  ? (anyFloat ? asDouble(L)> asDouble(R) : asInt(L)> asInt(R))
                                           : (opcode==IROpcode::LTE) ? (anyFloat ? asDouble(L)<=asDouble(R) : asInt(L)<=asInt(R))
                                           :                           (anyFloat ? asDouble(L)>=asDouble(R) : asInt(L)>=asInt(R));
                                    folded = mkConstInt(r ? 1 : 0);
                                    break;
                                }
                                case IROpcode::AND:  folded = mkConstInt((asBool(L) && asBool(R)) ? 1 : 0); break;
                                case IROpcode::OR:   folded = mkConstInt((asBool(L) || asBool(R)) ? 1 : 0); break;
                                case IROpcode::XOR:  folded = mkConstInt((asBool(L) != asBool(R)) ? 1 : 0); break;
                                case IROpcode::XNOR: folded = mkConstInt((asBool(L) == asBool(R)) ? 1 : 0); break;
                                // Bitwise folds (int-only)
                                case IROpcode::BAND: if (!anyFloat) folded = mkConstInt(asInt(L) & asInt(R)); else did_fold = false; break;
                                case IROpcode::BOR:  if (!anyFloat) folded = mkConstInt(asInt(L) | asInt(R)); else did_fold = false; break;
                                case IROpcode::BXOR: if (!anyFloat) folded = mkConstInt(asInt(L) ^ asInt(R)); else did_fold = false; break;
                                case IROpcode::XSUB: {
                                    // |a-b|+1 with overflow guard — compute the distance in unsigned
                                    // (signed a-b and -INT64_MIN are UB) and defer to runtime if +1 overflows.
                                    int64_t a = asInt(L), b = asInt(R);
                                    unsigned long long diff = (a >= b)
                                        ? (unsigned long long)a - (unsigned long long)b
                                        : (unsigned long long)b - (unsigned long long)a;
                                    if (diff >= (unsigned long long)std::numeric_limits<int64_t>::max()) { did_fold = false; break; }
                                    folded = IRRef::constant(IRValue((int64_t)diff + 1));
                                    break;
                                }
                                default: did_fold = false; break;
                            }
                            if (did_fold) return folded;
                        }
                        // Propagate float type: / always produces float; // always produces int; others propagate
                        IRType lt = typeOfRef(lRef), rt = typeOfRef(rRef);
                        // AC widening rule: on a fixed-width mismatch, promote the narrower operand to
                        // the BIGGER integer type before the op (mini + int → to_int(mini) + int;
                        // short + mini → short + to_short(mini)). Done once here so every backend sees
                        // matched widths — the strict ones (Rust/Go/V) stop rejecting `i16 + i32`, and
                        // the result width is consistent everywhere. Rank: mini(16) < short(32) < int(64).
                        // A CONST literal ADAPTS to the other operand's width (short a * 4 stays
                        // short — 4 is not "an int" forcing a promotion), so only promote when BOTH
                        // sides are typed values.
                        if (opcode != IROpcode::DIV
                            && lRef.kind != IRRef::Kind::CONST && rRef.kind != IRRef::Kind::CONST
                            && lt != IRType::FLOAT && rt != IRType::FLOAT
                            && lt != IRType::STRING && rt != IRType::STRING
                            && (lt == IRType::SHORT || lt == IRType::MINI
                             || rt == IRType::SHORT || rt == IRType::MINI)) {
                            auto rank = [](IRType t){ return t==IRType::MINI?16 : t==IRType::SHORT?32 : 64; };
                            if (rank(lt) != rank(rt)) {
                                // Mixed widths → promote both to the 64-bit INT (the biggest wired-in
                                // int). Simpler and more consistent than promoting to the wider narrow:
                                // the result is i64 on EVERY backend, so no backend chokes on the mix
                                // and there's no i32-result-declared-i64 skew on the strict ones.
                                auto promote = [&](IRRef& ref, IRType t){
                                    if (rank(t) < 64) {
                                        IRRef c = mkTemp();
                                        IRInstruction ci(IROpcode::TYPE_CAST, c, {ref});
                                        ci.resultType = IRType::INT; setRefType(c, IRType::INT);
                                        emit(std::move(ci));
                                        ref = c;
                                    }
                                };
                                promote(lRef, lt); promote(rRef, rt);
                                lt = rt = IRType::INT;
                            }
                        }
                        IRType resType = (opcode == IROpcode::DIV)
                                         ? IRType::FLOAT
                                         : (opcode == IROpcode::IDIV)
                                           ? IRType::INT
                                           : ((lt == IRType::FLOAT || rt == IRType::FLOAT)
                                              ? IRType::FLOAT : (lt != IRType::VOID ? lt : rt));
                        IRInstruction i(opcode, dst, {lRef, rRef});
                        i.resultType = resType;
                        setRefType(dst, resType);
                        emit(std::move(i));
                        return dst;
                    }
                    // Structured children present but operator not recognised — surface it
                    throw ACError::unknownBinaryOp(expr.value);
                }
                // No structured children: legacy string-based expression from older AST cache
                return lowerExpr(expr.value);
            }
            
            case NodeType::TernaryExpr: {
                // `condition | true_expr, # false_expr` (parser.cpp's post-loop check).
                // Desugars to a synthetic result var + a real if/else, the exact same
                // shape as the short-circuit and/or case above (branch, store into a
                // synthetic var, continue) just with BOTH branches populated instead of
                // one guarded branch. Reuses IF_BEGIN/IF_ELSE/IF_END (high-level IR) or
                // emitJF+labels (low-level IR, e.g. BNY) — opcodes every backend already
                // implements — so no backend-specific codegen is needed anywhere.
                thread_local int ternC = 0;
                std::string rn = "ac_tern_" + std::to_string(ternC++);
                IRRef resV = mkVar(rn);
                IRRef condRef = lowerExprNode(*expr.children[0]);
                IRRef condT = mkTemp();
                emit(IRInstruction(IROpcode::NEQ, condT, {condRef, mkConstInt(0)}));
                // Unconditional placeholder write BEFORE the branch — same reason
                // short-circuit and/or's resV above does this: `ac_tern_` is in
                // isSyntheticVar()'s exemption list (needed to protect it from loop
                // save/restore corruption), but that SAME exemption also excludes it
                // from the cross-block hoist scan that block-scoped backends (C/C++/
                // Java/Rust) need for any var whose ONLY writes are inside if/else arms
                // — without this, those backends emit "ac_tern_0 undeclared" (verified
                // regression via a --all sweep). Writing it once, unconditionally, first
                // means it's always genuinely declared in the enclosing scope before
                // either branch touches it, so hoisting is never needed.
                emit(IRInstruction(IROpcode::STORE_VAR, resV, {mkConstInt(0)}));
                if (prog.useHighLevelIR) {
                    { IRInstruction ib(IROpcode::IF_BEGIN); ib.typedOperands = {condT}; emit(std::move(ib)); }
                    emit(IRInstruction(IROpcode::STORE_VAR, resV, {lowerExprNode(*expr.children[1])}));
                    emit(IRInstruction(IROpcode::IF_ELSE));
                    emit(IRInstruction(IROpcode::STORE_VAR, resV, {lowerExprNode(*expr.children[2])}));
                    emit(IRInstruction(IROpcode::IF_END));
                } else {
                    IRRef elseL = mkLabel(), endL = mkLabel();
                    emitJF(condT, elseL);
                    emit(IRInstruction(IROpcode::STORE_VAR, resV, {lowerExprNode(*expr.children[1])}));
                    emitJump(endL);
                    emitLabel(elseL);
                    emit(IRInstruction(IROpcode::STORE_VAR, resV, {lowerExprNode(*expr.children[2])}));
                    emitLabel(endL);
                }
                return resV;
            }

            case NodeType::UnaryExpr: {
                // Unary operation: op operand
                if (!expr.children.empty() && expr.children[0]) {
                    IRRef operand = lowerExprNode(*expr.children[0]);
                    IRRef dst = mkTemp();

                    if (expr.value == "-") {
                        // Unary minus: 0 - operand
                        IRRef zero = mkConstInt(0);
                        IRInstruction i(IROpcode::SUB, dst, {zero, operand});
                        emit(std::move(i));
                        return dst;
                    } else if (expr.value == "NOT" || expr.value == "#") {
                        // Logical NOT
                        IRInstruction i(IROpcode::NOT, dst, {operand});
                        emit(std::move(i));
                        return dst;
                    } else if (expr.value == "TO_STRING" || expr.value == "TO_INT" ||
                               expr.value == "TO_DEC" || expr.value == "TO_BOOL" ||
                               expr.value == "TO_SHORT" || expr.value == "TO_MINI" ||
                               expr.value == "TO_ATOMIC") {
                        IRInstruction i(IROpcode::TYPE_CAST);
                        i.typedOperands = {operand};
                        i.result = dst;
                        i.resultType = expr.value == "TO_STRING" ? IRType::STRING
                                     : expr.value == "TO_INT"    ? IRType::INT
                                     : expr.value == "TO_SHORT"  ? IRType::SHORT
                                     : expr.value == "TO_MINI"   ? IRType::MINI
                                     : expr.value == "TO_ATOMIC" ? IRType::ATOMIC
                                     : expr.value == "TO_DEC"    ? IRType::FLOAT : IRType::BOOL;
                        emit(std::move(i));
                        return dst;
                    } else if (expr.value == "BNOT") {
                        // Bitwise NOT (~)
                        IRInstruction i(IROpcode::BNOT, dst, {operand});
                        emit(std::move(i));
                        return dst;
                    }
                    throw ACError::unknownUnaryOp(expr.value);
                }
                return mkConst("");
            }
            
            case NodeType::CallExpr:
            case NodeType::FunctionCall: {
                std::string fname = expr.value;
                {
                    auto ait = selectiveImportAliases_.find(fname);
                    if (ait != selectiveImportAliases_.end()) fname = ait->second;
                }

                // INDIRECT call: empty name + callee expression as first child
                // (e.g. funcs[i](x)). Lower the callee to a temp; CALL's operand[0]
                // being a TEMP tells codegen to emit a call-through-value.
                if (fname.empty() && !expr.children.empty() && expr.children[0]) {
                    IRRef callee = lowerExprNode(*expr.children[0]);
                    std::vector<IRRef> ops = {callee};
                    for (size_t ai = 1; ai < expr.children.size(); ai++)
                        if (expr.children[ai]) ops.push_back(lowerExprNode(*expr.children[ai]));
                    IRRef dst = mkTemp();
                    emit(IRInstruction(IROpcode::CALL, dst, ops));
                    return dst;
                }

                // Built-in length function - compile as a direct call (each backend has ac_length)
                if (fname == "length") {
                    if (expr.children.empty() || !expr.children[0]) {
                        throw ACError::lengthNeedsArg();
                    }
                    IRRef arg = lowerExprNode(*expr.children[0]);
                    IRRef dst = mkTemp();
                    std::vector<IRRef> ops = {mkVar("ac_length"), arg};
                    IRInstruction i(IROpcode::CALL, dst, ops);
                    emit(std::move(i));
                    return dst;
                }

                // Remap GL helper functions to high-level gl:* names (lowering pass handles the rest)
                static const std::unordered_map<std::string,std::string> glFuncMapExpr = {
                    {"is_obj",                 "gl:obj.is"},
                    {"is_draw",                "gl:obj.is_draw"},
                    {"gl_hitbox_many_overlap", "gl:hitbox.many_overlap"},
                    {"gl_hitbox_coords_overlap","gl:hitbox.overlap"},
                    {"gl_frame_begin",         "gl:frame.begin"},
                    {"gl_frame_render",        "gl:frame.render"},
                    {"gl_frame_end",           "gl:frame.end"},
                    {"gl_frame_update",        "gl:frame.update"},
                    {"gl_obj_animate",         "gl:obj.animate"},
                    {"gl_obj_regen",           "gl:obj.regen"},
                    {"gl_screen_init_from_ac", "gl:screen.init"},
                };
                auto fit = glFuncMapExpr.find(fname);
                if (fit != glFuncMapExpr.end()) {
                    IRRef dst = mkTemp();
                    IRInstruction i(IROpcode::LIB_CALL, dst, {});
                    i.typedOperands.push_back(mkConst(fit->second));
                    for (auto& arg : expr.children) i.typedOperands.push_back(lowerExprNode(*arg));
                    emit(std::move(i));
                    return dst;
                }
                // Regular function call
                IRRef dst = mkTemp();
                std::vector<IRRef> ops = {mkVar(fname)};
                for (auto& arg : expr.children) ops.push_back(lowerExprNode(*arg));
                IRInstruction i(IROpcode::CALL, dst, ops);
                emit(std::move(i));
                return dst;
            }
            
            case NodeType::IndexExpr: {
                // Array indexing: array[index]  (AC uses 1-based, targets use 0-based)
                if (expr.children.size() >= 2 && expr.children[0] && expr.children[1]) {
                    // t[i] on a known tuple var — see lowerTupleIndexCore's own comment for the
                    // constant/dynamic/`; any` rules. Scalarized (flat shadow vars) checked
                    // first, then the escaping/bundle representation.
                    if (expr.children[0]->type == NodeType::Identifier) {
                        const std::string& idName = expr.children[0]->value;
                        std::string scalKey = currentFunc_ + "::" + idName;
                        auto scalIt = scalarizableTupleVars_.find(scalKey);
                        auto tv = tupleInstanceVars_.find(idName);
                        if (scalIt != scalarizableTupleVars_.end() || tv != tupleInstanceVars_.end()) {
                            IRRef idxRef = lowerExprNode(*expr.children[1]);
                            if (scalIt != scalarizableTupleVars_.end())
                                return lowerTupleScalarIndex(idName, scalKey, scalIt->second.isAny, idxRef);
                            return lowerTupleIndex(tv->first, tv->second, idxRef);
                        }
                    }
                    IRRef arr = lowerExprNode(*expr.children[0]);
                    IRRef rawIdx = lowerExprNode(*expr.children[1]);
                    IRRef idx = adjustIndex(rawIdx);
                    IRRef dst = mkTemp();
                    IRInstruction i(IROpcode::LOAD_INDEX, dst, {arr, idx});
                    emit(std::move(i));
                    return dst;
                }
                // Fallback: use variable name
                return mkVar(expr.value);
            }

            case NodeType::MethodCall: {
                // Handle Term.ask as an expression (input with prompt)
                const std::string& mname = expr.value;
                // Only Term.ask (or a bare "ask") means generic input — "endsWith(mname,\"ask\")"
                // used to also swallow OTHER receivers' .ask methods (e.g. camera's sidebar.ask),
                // silently shadowing them with input() and making them unreachable from AC source.
                {
                    auto askDot = mname.rfind('.');
                    std::string askRecv = askDot == std::string::npos ? "" : mname.substr(0, askDot);
                    std::string askMeth = askDot == std::string::npos ? mname : mname.substr(askDot + 1);
                    if (askMeth == "ask" && (askRecv.empty() || askRecv == "Term")) {
                        IRRef dst = mkTemp();
                        IRRef prompt = expr.children.empty() ? mkConst("$$") : lowerExprNode(*expr.children[0]);
                        IRInstruction i(IROpcode::INPUT, dst, {prompt});
                        emit(std::move(i));
                        return dst;
                    }
                }
                // sure $msg$ as expression: result = sure $Delete?$  → confirm("Delete?")
                if (mname == "sure") {
                    IRRef dst = mkTemp();
                    IRRef arg = expr.children.empty() ? mkConst("$Confirm?$") : lowerExprNode(*expr.children[0]);
                    IRInstruction i(IROpcode::LIB_CALL, dst, {});
                    i.typedOperands = {mkVar("sure"), arg};
                    emit(std::move(i));
                    return dst;
                }
                // GL object method calls used as expressions: receiver.METHOD(args) → gl:obj.METHOD
                {
                    static const std::unordered_map<std::string,std::string> glMethodsExpr = {
                        {"move_y","gl:obj.move_y"},   {"move_x","gl:obj.move_x"},
                        {"vertex","gl:obj.vertex"},   {"curveshape","gl:obj.curveshape"},
                        {"CircleFall","gl:obj.circle_fall"}, {"CircleFell","gl:obj.circle_fell"},
                        {"regen","gl:obj.regen"},     {"set_spawn","gl:obj.set_spawn"},
                        {"animate","gl:obj.animate"}, {"velocity","gl:obj.velocity"},
                        {"set_speed","gl:obj.set_speed"}, {"set_direction","gl:obj.set_direction"},
                        {"overlap_boundary","gl:hitbox.overlap_boundary"},
                    };
                    auto dotPos = mname.find('.');
                    if (dotPos != std::string::npos) {
                        std::string receiver = mname.substr(0, dotPos);
                        std::string method   = mname.substr(dotPos + 1);
                        auto it = glMethodsExpr.find(method);
                        if (it != glMethodsExpr.end()) {
                            IRRef dst = mkTemp();
                            IRInstruction i(IROpcode::LIB_CALL, dst, {});
                            i.typedOperands.push_back(mkConst(it->second));
                            // known GL objects → string literal; parameters/variables → variable ref
                            if (glObjects_.count(receiver) > 0)
                                i.typedOperands.push_back(mkConst(receiver));
                            else
                                i.typedOperands.push_back(mkVar(receiver));
                            for (auto& child : expr.children)
                                i.typedOperands.push_back(lowerExprNode(*child));
                            emit(std::move(i));
                            return dst;
                        }
                    }
                }
                // No-arg property/constant reference or explicit empty-arg call
                if (expr.children.empty()) {
                    bool explicitCall = !expr.attrs.empty() && expr.attrs[0] == "__called__";
                    // receiver.method with no args — check for string-cheese methods FIRST,
                    // before the generic `explicitCall` shortcut below claims it. Both
                    // `speech.lower()` (a real string-cheese call) and `ball.x()` (property-
                    // style access on a GL object) parse identically here — zero children,
                    // attrs[0]=="__called__" — so the explicitCall branch used to run
                    // unconditionally and return early, making THIS check dead code for every
                    // zero-arg case (verified: jarvis.ac's `speech.lower()` flattened to a
                    // literal, undefined `speech_lower()`/`speech.lower` callee on every backend
                    // — C: "implicit declaration of function 'speech_lower'"; the WITH-ARGS
                    // sibling check below, `speech.strip($x$)`, was never affected since it
                    // lives outside this `children.empty()` gate entirely).
                    // Only fires when receiver is NOT a known library namespace.
                    auto dotPos2 = mname.rfind('.');
                    if (dotPos2 != std::string::npos && dotPos2 > 0) {
                        std::string recv2 = mname.substr(0, dotPos2);
                        std::string meth2 = mname.substr(dotPos2 + 1);
                        if (recv2.find('.') == std::string::npos) {
                            static const std::unordered_set<std::string> strMethods0 = {
                                "lower","upper","LOWER","UPPER","strip","STRIP",
                                "trim","TRIM","length","len","LEN","format","FORMAT"
                            };
                            static const std::unordered_set<std::string> libNS0 = {
                                "math","os","regex","gl","maudio","camera","widgets",
                                "stringm","term","Term","sidebar","screen"
                            };
                            if (strMethods0.count(meth2) && !libNS0.count(recv2)
                                && !prog.importedLibs.count(recv2)) {
                                IRRef dst = mkTemp();
                                IRInstruction call(IROpcode::LIB_CALL);
                                call.result = dst;
                                call.typedOperands = {mkConst("stringm." + meth2), mkVar(recv2)};
                                emit(std::move(call));
                                return dst;
                            }
                        }
                    }
                    if (explicitCall) {
                        // ball.x() — empty-arg method call, not property access
                        IRRef dst = mkTemp();
                        std::vector<IRRef> ops = {mkVar(mname)};
                        IRInstruction i(IROpcode::CALL, dst, ops);
                        emit(std::move(i));
                        return dst;
                    }
                    return mkVar(mname);
                }
                // receiver.method(args) — check for string-cheese methods with arguments
                // Only fires when receiver is NOT a known library namespace
                {
                    auto dotPos3 = mname.rfind('.');
                    if (dotPos3 != std::string::npos && dotPos3 > 0) {
                        std::string recv3 = mname.substr(0, dotPos3);
                        std::string meth3 = mname.substr(dotPos3 + 1);
                        if (recv3.find('.') == std::string::npos) {
                            static const std::unordered_set<std::string> strMethodsN = {
                                "find","FIND","strip","STRIP","replace","REPLACE",
                                "startswith","endswith","count","COUNT","split","SPLIT"
                            };
                            // Skip if receiver is a known library namespace
                            static const std::unordered_set<std::string> libNamespaces = {
                                "math","os","regex","gl","maudio","camera","widgets",
                                "stringm","term","Term","sidebar","screen"
                            };
                            if (strMethodsN.count(meth3) && !libNamespaces.count(recv3)
                                && !prog.importedLibs.count(recv3) && !widgetCtorVars_.count(recv3)) {
                                IRRef dst = mkTemp();
                                IRInstruction call(IROpcode::LIB_CALL);
                                call.result = dst;
                                call.typedOperands = {mkConst("stringm." + meth3), mkVar(recv3)};
                                for (auto& arg : expr.children)
                                    call.typedOperands.push_back(lowerExprNode(*arg));
                                emit(std::move(call));
                                return dst;
                            }
                        }
                    }
                }
                // Function call: lib.func(args) → translated to lib_func in codegen
                IRRef dst = mkTemp();
                std::vector<IRRef> ops = {mkVar(mname)};
                for (auto& arg : expr.children)
                    ops.push_back(lowerExprNode(*arg));
                IRInstruction i(IROpcode::CALL, dst, ops);
                emit(std::move(i));
                return dst;
            }

            case NodeType::EvalExpr: {
                // eval(x) dispatches per-backend on x's STATIC type (see emitEval in
                // ir_codegen.cpp): a STRING argument is interpreted as code (the arithmetic-
                // string evaluator); anything else is evaluated directly, wrapped in try/catch
                // (this used to be lazy_eval's job — see LazyEvalExpr below for why it moved).
                IRRef dst = mkTemp();
                IRRef arg = expr.children.empty()
                    ? mkConst("")
                    : lowerExprNode(*expr.children[0]);
                IRInstruction i(IROpcode::EVAL, dst, {arg});
                // The string branch always yields a FLOAT (the arithmetic evaluator); the
                // expression branch yields whatever the expression's own known type is. Without
                // this, dst stays VOID-typed — verified real bug on Go/Java: `d = eval(f())`
                // where f() returns an int compiled to an untyped interface{}/Object assignment,
                // a hard compile error ("cannot use ... as int64 value", "incompatible types").
                IRType argType = typeOfRef(arg);
                i.resultType = (argType == IRType::STRING) ? IRType::FLOAT : argType;
                if (i.resultType != IRType::VOID) setRefType(dst, i.resultType);
                emit(std::move(i));
                return dst;
            }

            case NodeType::LazyEvalExpr: {
                // lazy_eval(expr) — Abu's spec: a pure IDENTITY passthrough, zero runtime cost.
                // Its entire purpose is documentation/intent ("this value is meant to be
                // sanitized before being eval'd") — it holds the value off, unchanged, for the
                // CALLER to sanitize with ordinary operations (e.g. stringm.* on a string) before
                // passing it to eval(). Since it's identity, sanitizing via string ops naturally
                // preserves the STRING type eval() dispatches on — no wrapper object or
                // provenance tracking needed. (This used to build IROpcode::LAZY_EVAL — that
                // opcode's codegen, the try/catch-wrapped-expr-eval, is still used, just now as
                // eval()'s own "argument isn't a string" branch — see emitEval.)
                if (expr.children.empty()) return mkConst("");
                return lowerExprNode(*expr.children[0]);
            }

            case NodeType::RangeExpr: {
                // range N used as expression (e.g., FOR i in range 5)
                IRRef dst = mkTemp();
                // New parser stores bound as child; legacy uses value string
                IRRef bound = !expr.children.empty()
                    ? lowerExprNode(*expr.children[0])
                    : lowerExpr(expr.value);
                IRInstruction i(IROpcode::ALLOC, dst, {mkConst("range"), bound});
                emit(std::move(i));
                return dst;
            }
            case NodeType::SequenceExpr: {
                // sequence(a,b) used as expression (e.g., FOR n in sequence(3,7))
                IRRef dst = mkTemp();
                IRRef aRef, bRef;
                if (expr.children.size() >= 2) {
                    aRef = lowerExprNode(*expr.children[0]);
                    bRef = lowerExprNode(*expr.children[1]);
                } else {
                    // Legacy: value = "a,b" — split at the TOP-LEVEL comma so "f(a,b),c" is safe.
                    size_t comma = topLevelComma(expr.value);
                    aRef = lowerExpr(comma != std::string::npos ? expr.value.substr(0, comma) : "0");
                    bRef = lowerExpr(comma != std::string::npos ? expr.value.substr(comma + 1) : expr.value);
                }
                std::vector<IRRef> sops = {mkConst("sequence"), aRef, bRef};
                if (expr.children.size() >= 3) sops.push_back(lowerExprNode(*expr.children[2]));
                IRInstruction i(IROpcode::ALLOC, dst, sops);
                emit(std::move(i));
                return dst;
            }

            // iota/stream used as a VALUE (not a FOR collection): iota generates NUMBERS, one
            // at a time, on the spot — it is NEVER a string. It used to route through a
            // ac_iota/ac_stream backend builtin that built a concatenated STRING of digits
            // ("iota 5" -> "01234") — flatly wrong (Abu, directly, repeatedly: "iota is not a
            // string, never a string, it is a lazy generator" / "IOTA IS NOT A CONCATENATED
            // STRING ... IT GENERATES A NUMBER ON THE SPOT"). Route through the exact same
            // ALLOC("range"/"sequence") mechanism RangeExpr/SequenceExpr use, so a bare `x =
            // iota 5` materializes real numbers [0,1,2,3,4], matching every other numeric
            // context instead of a digit-string. The FOR-loop path (ir.cpp's ForLoop case) was
            // never affected by this bug — it already generates one number per iteration lazily,
            // with nothing materialized upfront; that's iota's real, primary use and stays as-is.
            case NodeType::IotaExpr: {
                IRRef dst = mkTemp();
                IRRef bound = !expr.children.empty() ? lowerExprNode(*expr.children[0]) : mkConstInt(0);
                IRInstruction i(IROpcode::ALLOC, dst, {mkConst("range"), bound});
                emit(std::move(i));
                return dst;
            }
            case NodeType::StreamExpr: {
                IRRef dst = mkTemp();
                IRRef a = expr.children.size() >= 1 ? lowerExprNode(*expr.children[0]) : mkConstInt(0);
                IRRef b = expr.children.size() >= 2 ? lowerExprNode(*expr.children[1]) : mkConstInt(0);
                std::vector<IRRef> sops = {mkConst("sequence"), a, b};
                if (expr.children.size() >= 3) sops.push_back(lowerExprNode(*expr.children[2]));
                IRInstruction i(IROpcode::ALLOC, dst, sops);
                emit(std::move(i));
                return dst;
            }

            // xrange N / xiota N — 1-indexed range/iota (AC arrays are 1-indexed; range/
            // sequence/iota/stream aren't). Pure desugaring to sequence(1, N+1)/stream(1, N+1):
            // reuses the exact same "sequence" ALLOC machinery every backend already handles
            // correctly, computing the +1 with a real IR ADD instead of AST manipulation.
            case NodeType::XRangeExpr: {
                IRRef dst = mkTemp();
                IRRef bound = !expr.children.empty() ? lowerExprNode(*expr.children[0]) : mkConstInt(0);
                IRRef bPlus1 = mkTemp();
                emit(IRInstruction(IROpcode::ADD, bPlus1, {bound, mkConstInt(1)}));
                IRInstruction i(IROpcode::ALLOC, dst, {mkConst("sequence"), mkConstInt(1), bPlus1});
                emit(std::move(i));
                return dst;
            }
            case NodeType::XIotaExpr: {
                IRRef dst = mkTemp();
                IRRef bound = !expr.children.empty() ? lowerExprNode(*expr.children[0]) : mkConstInt(0);
                IRRef bPlus1 = mkTemp();
                emit(IRInstruction(IROpcode::ADD, bPlus1, {bound, mkConstInt(1)}));
                IRInstruction i(IROpcode::ALLOC, dst, {mkConst("sequence"), mkConstInt(1), bPlus1});
                emit(std::move(i));
                return dst;
            }

            case NodeType::ListLiteral: {
                IRRef dst = mkTemp();
                if (!expr.children.empty()) {
                    // #11: computed elements can't ride the literal text — allocate with a 0
                    // placeholder and STORE_INDEX the runtime value right after the ALLOC.
                    std::string content;
                    std::vector<std::pair<size_t, IRRef>> computed;
                    for (size_t ci = 0; ci < expr.children.size(); ci++) {
                        if (ci > 0) content += ", ";
                        IRRef er = lowerExprNode(*expr.children[ci]);
                        if (er.kind == IRRef::Kind::VAR && er.id >= 0)
                            content += prog.symbols.getName(er.id);
                        else if (er.kind == IRRef::Kind::CONST) {
                            const auto& cv = er.value;
                            if (cv.type == IRType::INT)   content += std::to_string(std::get<int64_t>(cv.data));
                            else if (cv.type == IRType::FLOAT) content += std::to_string(std::get<double>(cv.data));
                            else if (cv.type == IRType::STRING) content += "\"" + std::get<std::string>(cv.data) + "\"";
                            else content += "0";
                        } else { content += "0"; computed.push_back({ci, er}); }
                    }
                    IRInstruction i(IROpcode::ALLOC, dst, {mkConst("list"), mkConst(content)});
                    emit(std::move(i));
                    for (auto& [ci, er] : computed) {
                        IRInstruction st(IROpcode::STORE_INDEX);
                        st.typedOperands = {dst, mkConstInt((int64_t)ci), er};  // IR indices 0-based
                        emit(std::move(st));
                    }
                } else {
                    IRInstruction i(IROpcode::ALLOC, dst, {mkConst("list"), mkConst(expr.value)});
                    emit(std::move(i));
                }
                setRefType(dst, IRType::LIST);   // so `nums = [..]` types nums LIST (arg cloning, #22)
                return dst;
            }

            // A tuple literal reached AS AN EXPRESSION (return x,y; a function-call argument;
            // an element of a list/dict/another tuple; etc — anything that isn't the direct,
            // same-statement RHS of a DestructureAssignStmt, which has its own zero-cost fast
            // path in gen()'s DestructureAssignStmt case). This is always the escaping/fallback
            // path — a synthesized bundle instance, per the tuple design's Design section.
            case NodeType::TupleLiteral:
                return lowerTupleEscaping(expr, "expr");

            default:
                // Fallback: treat as variable or constant
                if (!expr.value.empty()) {
                    // "[...]" — a list literal that reached here as a raw token (some parser
                    // paths don't build a ListLiteral node). mkVar'ing it made BNY look up a
                    // variable literally named "[5, 2, 9]" → null pointer at runtime.
                    if (expr.value.size() >= 2 && expr.value.front() == '[' && expr.value.back() == ']') {
                        IRRef dst = mkTemp();
                        std::string inner = expr.value.substr(1, expr.value.size() - 2);
                        emit(IRInstruction(IROpcode::ALLOC, dst, {mkConst("list"), mkConst(inner)}));
                        return dst;
                    }
                    // Non-identifier text ("i@i", "x+1") is an expression, not a var name.
                    bool isIdent = true;
                    for (char c : expr.value)
                        if (!std::isalnum((unsigned char)c) && c != '_' && c != '.') { isIdent = false; break; }
                    if (!isIdent) return lowerExpr(expr.value);
                    return mkVar(expr.value);
                }
                return mkConst("");
        }
    }

    static bool longIntFromExpr(const ASTNode& expr, std::string& out) {
        struct BigNat {
            enum : uint32_t { BASE = 1000000000u };
            std::vector<uint32_t> d;

            BigNat(uint64_t v = 0) {
                while (v) { d.push_back((uint32_t)(v % BASE)); v /= BASE; }
            }

            bool isZero() const { return d.empty(); }
            void norm() { while (!d.empty() && d.back() == 0) d.pop_back(); }

            static bool parse(const std::string& s, BigNat& out) {
                if (s.empty()) return false;
                BigNat v;
                for (char c : s) {
                    if (!std::isdigit((unsigned char)c)) return false;
                    v = mulSmall(v, 10);
                    v = add(v, BigNat((uint64_t)(c - '0')));
                }
                out = v;
                return true;
            }

            std::string str() const {
                if (d.empty()) return "0";
                std::string s = std::to_string(d.back());
                for (int i = (int)d.size() - 2; i >= 0; --i) {
                    std::string part = std::to_string(d[i]);
                    s += std::string(9 - part.size(), '0') + part;
                }
                return s;
            }

            bool toU64(uint64_t& out) const {
                uint64_t v = 0;
                for (int i = (int)d.size() - 1; i >= 0; --i) {
                    if (v > (std::numeric_limits<uint64_t>::max() - d[i]) / BASE) return false;
                    v = v * BASE + d[i];
                }
                out = v;
                return true;
            }

            static int cmp(const BigNat& a, const BigNat& b) {
                if (a.d.size() != b.d.size()) return a.d.size() < b.d.size() ? -1 : 1;
                for (int i = (int)a.d.size() - 1; i >= 0; --i)
                    if (a.d[i] != b.d[i]) return a.d[i] < b.d[i] ? -1 : 1;
                return 0;
            }

            static BigNat add(const BigNat& a, const BigNat& b) {
                BigNat r;
                uint64_t carry = 0;
                size_t n = std::max(a.d.size(), b.d.size());
                r.d.resize(n);
                for (size_t i = 0; i < n; ++i) {
                    uint64_t cur = carry;
                    if (i < a.d.size()) cur += a.d[i];
                    if (i < b.d.size()) cur += b.d[i];
                    r.d[i] = (uint32_t)(cur % BASE);
                    carry = cur / BASE;
                }
                if (carry) r.d.push_back((uint32_t)carry);
                return r;
            }

            static BigNat sub(const BigNat& a, const BigNat& b) {
                BigNat r;
                r.d.resize(a.d.size());
                int64_t carry = 0;
                for (size_t i = 0; i < a.d.size(); ++i) {
                    int64_t cur = (int64_t)a.d[i] - carry - (i < b.d.size() ? b.d[i] : 0);
                    if (cur < 0) { cur += BASE; carry = 1; }
                    else carry = 0;
                    r.d[i] = (uint32_t)cur;
                }
                r.norm();
                return r;
            }

            static BigNat mulSmall(const BigNat& a, uint32_t m) {
                if (a.isZero() || m == 0) return BigNat();
                BigNat r;
                r.d.resize(a.d.size());
                uint64_t carry = 0;
                for (size_t i = 0; i < a.d.size(); ++i) {
                    uint64_t cur = carry + (uint64_t)a.d[i] * m;
                    r.d[i] = (uint32_t)(cur % BASE);
                    carry = cur / BASE;
                }
                if (carry) r.d.push_back((uint32_t)carry);
                return r;
            }

            static BigNat mul(const BigNat& a, const BigNat& b) {
                if (a.isZero() || b.isZero()) return BigNat();
                BigNat r;
                r.d.assign(a.d.size() + b.d.size(), 0);
                for (size_t i = 0; i < a.d.size(); ++i) {
                    uint64_t carry = 0;
                    for (size_t j = 0; j < b.d.size() || carry; ++j) {
                        uint64_t cur = r.d[i + j] + carry;
                        if (j < b.d.size()) cur += (uint64_t)a.d[i] * b.d[j];
                        r.d[i + j] = (uint32_t)(cur % BASE);
                        carry = cur / BASE;
                    }
                }
                r.norm();
                return r;
            }

            static bool divmod(const BigNat& a, const BigNat& b, BigNat& q, BigNat& rem) {
                if (b.isZero()) return false;
                q.d.assign(a.d.size(), 0);
                rem = BigNat();
                for (int i = (int)a.d.size() - 1; i >= 0; --i) {
                    if (!rem.isZero() || a.d[i] != 0) rem.d.insert(rem.d.begin(), a.d[i]);
                    rem.norm();
                    uint32_t lo = 0, hi = BASE - 1, best = 0;
                    while (lo <= hi) {
                        uint32_t mid = lo + (hi - lo) / 2;
                        BigNat prod = mulSmall(b, mid);
                        if (cmp(prod, rem) <= 0) {
                            best = mid;
                            if (mid == BASE - 1) break;
                            lo = mid + 1;
                        } else {
                            if (mid == 0) break;
                            hi = mid - 1;
                        }
                    }
                    q.d[i] = best;
                    if (best) rem = sub(rem, mulSmall(b, best));
                }
                q.norm();
                rem.norm();
                return true;
            }
        };

        // LongInt bounds (signed 96-bit): min = -(2^95), max = 2^95 - 1
        static const BigNat LONGINT_MAX_MAG = []{
            BigNat v;
            BigNat::parse("39614081257132168796771975167", v); // 2^95 - 1
            return v;
        }();
        static const BigNat LONGINT_MIN_MAG = []{
            BigNat v;
            BigNat::parse("39614081257132168796771975168", v); // 2^95
            return v;
        }();

        struct BigInt {
            bool neg = false;
            BigNat mag;
            void norm() { if (mag.isZero()) neg = false; }
        };

        auto parseSignedDecimal = [](const std::string& s, BigInt& out) -> bool {
            if (s.empty()) return false;
            std::string t = s;
            bool neg = false;
            if (!t.empty() && t[0] == '-') { neg = true; t.erase(0, 1); }
            if (t.empty()) return false;
            BigNat m;
            if (!BigNat::parse(t, m)) return false;
            out.neg = neg;
            out.mag = m;
            out.norm();
            return true;
        };

        auto inSigned96 = [&](const BigInt& v) -> bool {
            if (!v.neg) return BigNat::cmp(v.mag, LONGINT_MAX_MAG) <= 0;
            return BigNat::cmp(v.mag, LONGINT_MIN_MAG) <= 0;
        };

        auto addSigned = [&](const BigInt& a, const BigInt& b) -> BigInt {
            BigInt r;
            if (a.neg == b.neg) {
                r.neg = a.neg;
                r.mag = BigNat::add(a.mag, b.mag);
                r.norm();
                return r;
            }
            int c = BigNat::cmp(a.mag, b.mag);
            if (c == 0) return BigInt{};
            if (c > 0) { r.neg = a.neg; r.mag = BigNat::sub(a.mag, b.mag); }
            else       { r.neg = b.neg; r.mag = BigNat::sub(b.mag, a.mag); }
            r.norm();
            return r;
        };

        auto subSigned = [&](const BigInt& a, const BigInt& b) -> BigInt {
            BigInt nb = b;
            nb.neg = !nb.neg;
            nb.norm();
            return addSigned(a, nb);
        };

        auto mulSigned = [&](const BigInt& a, const BigInt& b) -> BigInt {
            BigInt r;
            r.neg = (a.neg != b.neg);
            r.mag = BigNat::mul(a.mag, b.mag);
            r.norm();
            return r;
        };

        auto divmodSigned = [&](const BigInt& a, const BigInt& b, BigInt& q, BigInt& rem) -> bool {
            if (b.mag.isZero()) return false;
            BigNat qmag, rmag;
            if (!BigNat::divmod(a.mag, b.mag, qmag, rmag)) return false;
            q.neg = (a.neg != b.neg);
            q.mag = qmag;
            q.norm();
            rem.neg = a.neg; // C-like remainder sign = dividend sign
            rem.mag = rmag;
            rem.norm();
            return true;
        };

        std::function<bool(const ASTNode&, BigInt&)> eval = [&](const ASTNode& n, BigInt& outv) -> bool {
            if (n.type == NodeType::LiteralExpr && !n.attrs.empty() && n.attrs[0] == "INT")
                return parseSignedDecimal(n.value, outv);
            if (n.type == NodeType::NumberLit)
                return parseSignedDecimal(n.value, outv);

            if (n.type == NodeType::UnaryExpr) {
                if (n.children.empty()) return false;
                if (!eval(*n.children[0], outv)) return false;
                if (n.value == "-") { outv.neg = !outv.neg; outv.norm(); return true; }
                return false;
            }

            if ((n.type == NodeType::CallExpr || n.type == NodeType::MethodCall) && n.children.size() == 2) {
                BigInt a, b;
                if (!eval(*n.children[0], a) || !eval(*n.children[1], b)) return false;
                if (n.value == "pow" || n.value == "math.pow") {
                    if (b.neg) return false;
                    uint64_t exp = 0;
                    if (!b.mag.toU64(exp) || exp > 4096) return false;
                    BigInt result;
                    result.mag = BigNat(1);
                    BigInt base = a;
                    while (exp > 0) {
                        if (exp & 1) result = mulSigned(result, base);
                        exp >>= 1;
                        if (exp) base = mulSigned(base, base);
                    }
                    outv = result;
                    return true;
                }
                if (n.value == "mod" || n.value == "math.mod" || n.value == "math.mod_int") {
                    BigInt q, r;
                    if (!divmodSigned(a, b, q, r)) return false;
                    outv = r;
                    return true;
                }
                return false;
            }

            if (n.type == NodeType::BinaryExpr && n.children.size() >= 2) {
                BigInt l, r;
                if (!eval(*n.children[0], l) || !eval(*n.children[1], r)) return false;
                const std::string& op = n.value;
                if (op == "+") outv = addSigned(l, r);
                else if (op == "-") outv = subSigned(l, r);
                else if (op == "*" || op == "@") outv = mulSigned(l, r);
                else if (op == "/") {
                    BigInt q, rem;
                    if (!divmodSigned(l, r, q, rem)) return false;
                    outv = q;
                } else return false;
                return true;
            }

            return false;
        };

        BigInt v;
        if (!eval(expr, v)) return false;
        if (!inSigned96(v)) return false;
        out = (v.neg ? "-" : "") + v.mag.str();
        return true;
    }

    static void normalizeGoodDecPair(std::string& unscaledSigned, int64_t& scale) {
        // Canonicalize: strip leading zeros from magnitude, and if scale < 0
        // strip trailing zeros from magnitude while increasing scale.
        bool neg = false;
        std::string mag = unscaledSigned;
        if (!mag.empty() && mag[0] == '-') { neg = true; mag.erase(0, 1); }
        if (mag.empty()) mag = "0";

        size_t nz = mag.find_first_not_of('0');
        if (nz == std::string::npos) {
            mag = "0";
            neg = false;
            scale = 0;
        } else if (nz > 0) {
            mag.erase(0, nz);
        }

        while (scale < 0 && mag.size() > 1 && mag.back() == '0') {
            mag.pop_back();
            scale += 1;
        }

        unscaledSigned = (neg ? "-" : "") + mag;
        if (unscaledSigned == "-0") unscaledSigned = "0";
    }

    static bool goodDecPairFromExpr(const ASTNode& expr, std::string& unscaledOut, int64_t& scaleOut) {
        struct BigInt {
            enum : uint32_t { BASE = 1000000000u };
            bool neg = false;
            std::vector<uint32_t> d; // little-endian base 1e9

            BigInt(int64_t v = 0) {
                if (v < 0) { neg = true; v = -v; }
                uint64_t u = (uint64_t)v;
                while (u) { d.push_back((uint32_t)(u % BASE)); u /= BASE; }
            }

            bool isZero() const { return d.empty(); }
            void norm() {
                while (!d.empty() && d.back() == 0) d.pop_back();
                if (d.empty()) neg = false;
            }

            static int cmpAbs(const BigInt& a, const BigInt& b) {
                if (a.d.size() != b.d.size()) return a.d.size() < b.d.size() ? -1 : 1;
                for (int i = (int)a.d.size() - 1; i >= 0; --i)
                    if (a.d[i] != b.d[i]) return a.d[i] < b.d[i] ? -1 : 1;
                return 0;
            }

            static BigInt addAbs(const BigInt& a, const BigInt& b) {
                BigInt r;
                uint64_t carry = 0;
                size_t n = std::max(a.d.size(), b.d.size());
                r.d.resize(n);
                for (size_t i = 0; i < n; ++i) {
                    uint64_t cur = carry;
                    if (i < a.d.size()) cur += a.d[i];
                    if (i < b.d.size()) cur += b.d[i];
                    r.d[i] = (uint32_t)(cur % BASE);
                    carry = cur / BASE;
                }
                if (carry) r.d.push_back((uint32_t)carry);
                return r;
            }

            static BigInt subAbs(const BigInt& a, const BigInt& b) {
                // |a| - |b|, assuming |a| >= |b|
                BigInt r;
                r.d.resize(a.d.size());
                int64_t carry = 0;
                for (size_t i = 0; i < a.d.size(); ++i) {
                    int64_t cur = (int64_t)a.d[i] - carry - (i < b.d.size() ? b.d[i] : 0);
                    if (cur < 0) { cur += BASE; carry = 1; }
                    else carry = 0;
                    r.d[i] = (uint32_t)cur;
                }
                r.norm();
                return r;
            }

            static BigInt add(const BigInt& a, const BigInt& b) {
                if (a.neg == b.neg) {
                    BigInt r = addAbs(a, b);
                    r.neg = a.neg;
                    r.norm();
                    return r;
                }
                int c = cmpAbs(a, b);
                if (c == 0) return BigInt();
                if (c > 0) {
                    BigInt r = subAbs(a, b);
                    r.neg = a.neg;
                    r.norm();
                    return r;
                }
                BigInt r = subAbs(b, a);
                r.neg = b.neg;
                r.norm();
                return r;
            }

            static BigInt sub(const BigInt& a, const BigInt& b) {
                BigInt nb = b;
                if (!nb.isZero()) nb.neg = !nb.neg;
                return add(a, nb);
            }

            static BigInt mul(const BigInt& a, const BigInt& b) {
                if (a.isZero() || b.isZero()) return BigInt();
                BigInt r;
                r.neg = (a.neg != b.neg);
                r.d.assign(a.d.size() + b.d.size(), 0);
                for (size_t i = 0; i < a.d.size(); ++i) {
                    uint64_t carry = 0;
                    for (size_t j = 0; j < b.d.size() || carry; ++j) {
                        uint64_t cur = r.d[i + j] + carry;
                        if (j < b.d.size()) cur += (uint64_t)a.d[i] * b.d[j];
                        r.d[i + j] = (uint32_t)(cur % BASE);
                        carry = cur / BASE;
                    }
                }
                r.norm();
                return r;
            }

            static BigInt mulPow10(const BigInt& a, int64_t p) {
                if (a.isZero()) return BigInt();
                if (p <= 0) return a;
                BigInt r = a;
                for (int64_t i = 0; i < p; ++i) {
                    uint64_t carry = 0;
                    for (size_t j = 0; j < r.d.size(); ++j) {
                        uint64_t cur = carry + (uint64_t)r.d[j] * 10u;
                        r.d[j] = (uint32_t)(cur % BASE);
                        carry = cur / BASE;
                    }
                    if (carry) r.d.push_back((uint32_t)carry);
                }
                r.norm();
                return r;
            }

            std::string absStr() const {
                if (d.empty()) return "0";
                std::string s = std::to_string(d.back());
                for (int i = (int)d.size() - 2; i >= 0; --i) {
                    std::string part = std::to_string(d[i]);
                    s += std::string(9 - part.size(), '0') + part;
                }
                return s;
            }
        };

        struct BigDec {
            BigInt unscaled;
            int64_t scale = 0; // base-10 exponent: value = unscaled * 10^scale
        };

        auto parseDecimalLiteral = [&](const std::string& s, BigDec& outDec) -> bool {
            if (s.empty()) return false;
            std::string t = s;
            bool neg = false;
            if (!t.empty() && t[0] == '-') { neg = true; t.erase(0, 1); }
            if (t.empty()) return false;
            if (t.find_first_of("eE") != std::string::npos) return false;
            size_t dot = t.find('.');
            std::string intPart = (dot == std::string::npos) ? t : t.substr(0, dot);
            std::string fracPart = (dot == std::string::npos) ? "" : t.substr(dot + 1);
            if (intPart.empty()) intPart = "0";
            if (intPart.empty() && fracPart.empty()) return false;
            for (char c : intPart) if (!std::isdigit((unsigned char)c)) return false;
            for (char c : fracPart) if (!std::isdigit((unsigned char)c)) return false;

            // Remove leading zeros from integer part only; keep fractional zeros for scale.
            size_t nz = intPart.find_first_not_of('0');
            if (nz == std::string::npos) intPart = "0";
            else if (nz > 0) intPart.erase(0, nz);

            std::string digits = intPart + fracPart;
            // Strip leading zeros in combined digits to keep unscaled minimal.
            size_t dnz = digits.find_first_not_of('0');
            if (dnz == std::string::npos) digits = "0";
            else if (dnz > 0) digits.erase(0, dnz);

            BigInt u;
            u.neg = neg;
            // parse digits into BigInt (base 1e9)
            for (char c : digits) {
                if (!std::isdigit((unsigned char)c)) return false;
                // u = u*10 + (c-'0')
                uint64_t carry = (uint64_t)(c - '0');
                uint64_t ccarry = 0;
                for (size_t i = 0; i < u.d.size(); ++i) {
                    uint64_t cur = (uint64_t)u.d[i] * 10u + ccarry;
                    u.d[i] = (uint32_t)(cur % BigInt::BASE);
                    ccarry = cur / BigInt::BASE;
                }
                if (ccarry) u.d.push_back((uint32_t)ccarry);
                // add carry to u (least limb)
                size_t idx = 0;
                while (carry) {
                    if (idx >= u.d.size()) u.d.push_back(0);
                    uint64_t cur = (uint64_t)u.d[idx] + carry;
                    u.d[idx] = (uint32_t)(cur % BigInt::BASE);
                    carry = cur / BigInt::BASE;
                    idx++;
                }
            }
            u.norm();

            outDec.unscaled = u;
            outDec.scale = -(int64_t)fracPart.size();
            return true;
        };

        std::function<bool(const ASTNode&, BigDec&)> eval = [&](const ASTNode& n, BigDec& outDec) -> bool {
            if (n.type == NodeType::LiteralExpr && !n.attrs.empty()) {
                if (n.attrs[0] == "INT" || n.attrs[0] == "FLOAT") {
                    return parseDecimalLiteral(n.value, outDec);
                }
            }

            if (n.type == NodeType::UnaryExpr && n.value == "-" && !n.children.empty()) {
                BigDec inner;
                if (!eval(*n.children[0], inner)) return false;
                if (!inner.unscaled.isZero()) inner.unscaled.neg = !inner.unscaled.neg;
                outDec = inner;
                return true;
            }

            if (n.type == NodeType::BinaryExpr && n.children.size() >= 2) {
                BigDec a, b;
                if (!eval(*n.children[0], a) || !eval(*n.children[1], b)) return false;
                const std::string& op = n.value;

                if (op == "+" || op == "-") {
                    int64_t targetScale = std::min(a.scale, b.scale);
                    BigInt au = BigInt::mulPow10(a.unscaled, a.scale - targetScale);
                    BigInt bu = BigInt::mulPow10(b.unscaled, b.scale - targetScale);
                    BigInt ru = (op == "+") ? BigInt::add(au, bu) : BigInt::sub(au, bu);
                    outDec.unscaled = ru;
                    outDec.scale = targetScale;
                    return true;
                }

                if (op == "*") {
                    outDec.unscaled = BigInt::mul(a.unscaled, b.unscaled);
                    outDec.scale = a.scale + b.scale;
                    return true;
                }
            }

            return false;
        };

        BigDec v;
        if (!eval(expr, v)) return false;
        std::string signedUnscaled = (v.unscaled.neg ? "-" : "") + v.unscaled.absStr();
        int64_t scale = v.scale;
        normalizeGoodDecPair(signedUnscaled, scale);
        unscaledOut = signedUnscaled;
        scaleOut = scale;
        return true;
    }
    
    // Returns the IRRef holding the result of the expression string.
    // For simple names/literals we just return a VAR or CONST ref.
    // For compound expressions we emit arithmetic and return a temp.

    // Call attrs are comma-split by the parser, which shreds list literals:
    // f([5, 2, 9]) arrives as attrs ["[5","2","9]"]. Re-join bracket groups so a
    // list literal is one argument again ("[5,2,9]").
    // Splits a raw ListLiteral's flattened `.value` text ("1,2" for `[1, 2]`) back into its
    // individual element expressions — needed by the `[elem1, elem2, ...] @ n` repeat lowering
    // below, which otherwise only ever saw ONE element (children is empty for a multi-element
    // list literal in THIS position; see that lowering's own comment) and fed the whole raw
    // "1,2" text to a SINGLE append() call — a real crash on every backend whose append() takes
    // exactly one argument (verified: Python — "TypeError: list.append() takes exactly one
    // argument (2 given)"). Tracks bracket/paren/brace depth and `$...$` string spans so a
    // comma inside a nested list/call/string doesn't split early.
    static std::vector<std::string> splitTopLevelCommas(const std::string& s) {
        std::vector<std::string> out;
        std::string cur;
        int depth = 0;
        bool inStr = false;
        for (size_t i = 0; i < s.size(); i++) {
            char c = s[i];
            if (c == '$' && (i == 0 || s[i-1] != '\\')) inStr = !inStr;
            if (!inStr) {
                if (c == '[' || c == '(' || c == '{') depth++;
                else if (c == ']' || c == ')' || c == '}') depth--;
            }
            if (c == ',' && depth == 0 && !inStr) {
                out.push_back(cur);
                cur.clear();
            } else {
                cur += c;
            }
        }
        if (!cur.empty() || !out.empty()) out.push_back(cur);
        // Trim surrounding whitespace off each piece (list literals are written "1, 2", not "1,2").
        for (auto& p : out) {
            size_t a = p.find_first_not_of(" \t");
            size_t b = p.find_last_not_of(" \t");
            p = (a == std::string::npos) ? "" : p.substr(a, b - a + 1);
        }
        return out;
    }

    static std::vector<std::string> mergeBracketAttrs(const std::vector<std::string>& attrs) {
        std::vector<std::string> out;
        std::string pending;
        int depth = 0;
        for (const auto& a : attrs) {
            // #38: track parens too — f(g(x, y), z) attrs were re-split at inner commas
            for (char c : a) { if (c == '[' || c == '(') depth++; else if (c == ']' || c == ')') depth--; }
            if (!pending.empty()) {
                pending += "," + a;
                if (depth == 0) { out.push_back(pending); pending.clear(); }
            } else if (depth > 0) {
                pending = a;
            } else {
                out.push_back(a);
            }
        }
        if (!pending.empty()) out.push_back(pending); // unbalanced — pass through
        return out;
    }

    // Lower a "[e1,e2,...]" argument to a list ALLOC temp; returns true if handled.
    bool lowerListLiteralArg(const std::string& trimmed, std::vector<IRRef>& ops) {
        if (trimmed.size() < 2 || trimmed.front() != '[' || trimmed.back() != ']') return false;
        std::string inner = trimmed.substr(1, trimmed.size() - 2);
        IRRef dst = mkTemp();
        IRInstruction i(IROpcode::ALLOC, dst, {mkConst("list"), mkConst(inner)});
        emit(std::move(i));
        ops.push_back(dst);
        return true;
    }

    IRRef lowerExpr(const std::string& expr) {
        if (expr.empty()) return mkConst("");

        // strip outer $…$ string literal — but only when the two boundary characters are the
        // ONLY `$`s present. A reconstructed multi-part concatenation (`$a$ + x + $b$`, from a
        // parser-side token-value rebuild — see the FunctionCall/MethodCall arg-collectors'
        // matching comment) ALSO happens to start and end with `$`, with more `$`s embedded in
        // the middle; blindly stripping just the outer pair turned the whole concatenation into
        // one literal string with the inner `$`s left in as literal text instead of actually
        // being evaluated (verified: `os.bash($ac $ + path + $ --target $ + backend + $
        // --no-cache...$)` — every `+`-joined part collapsed into one string containing the
        // literal text "$+path+$..." instead of the real interpolated command).
        {
            int dollarCount = 0;
            for (char c : expr) if (c == '$') dollarCount++;
            if (expr.size() >= 2 && expr.front() == '$' && expr.back() == '$' && dollarCount == 2)
                return mkConst(expr.substr(1, expr.size() - 2));
        }

        // integer literal — a '-' is only a sign at position 0, NOT an interior operator.
        // (Old check allowed '-' anywhere → "5-3" passed, then stoll stopped at '-' and
        //  returned 5, silently dropping the subtraction.)
        bool isInt = !expr.empty() && expr != "-";
        for (size_t ci = 0; isInt && ci < expr.size(); ++ci) {
            char c = expr[ci];
            if (!std::isdigit((unsigned char)c) && !(c == '-' && ci == 0)) { isInt = false; break; }
        }
        if (isInt) {
            try { return mkConstInt(std::stoll(expr)); } catch (...) {}
        }

        // float literal — same shape as the int check, one interior '.' allowed. Was
        // completely missing: a bare decimal like "0.785398163" fell all the way through
        // every branch below to the final "plain variable name" catch-all, silently treating
        // a FLOAT CONSTANT as a variable REFERENCE named "0.785398163" — invisible until a
        // typed backend tried to sanitize/emit that "identifier" (verified: RAD's
        // math.rad2deg(0.785398163) wrapping — C emitted `math_rad2deg(0_785398163)`, gcc:
        // "invalid suffix on integer constant" — the dot became an underscore via whatever
        // identifier-safety pass a VAR-kind ref goes through, since nothing upstream ever
        // recognized this text as a literal in the first place).
        bool isFloat = !expr.empty() && expr != "-" && expr != ".";
        bool sawDot = false;
        for (size_t ci = 0; isFloat && ci < expr.size(); ++ci) {
            char c = expr[ci];
            if (c == '.' && !sawDot) { sawDot = true; continue; }
            if (!std::isdigit((unsigned char)c) && !(c == '-' && ci == 0)) { isFloat = false; break; }
        }
        if (isFloat && sawDot) {
            try { return IRRef::constant(IRValue(std::stod(expr))); } catch (...) {}
        }

        // boolean / null / nil literals
        if (expr == "true")  return IRRef::constant(IRValue(true));
        if (expr == "false") return IRRef::constant(IRValue(false));
        if (expr == "null")  return mkConst("null");
        if (expr == "nil")   return mkConst("nil");

        // indexing: name[expr] → LOAD_INDEX (AC is 1-indexed; IR indices are 0-based)
        if (expr.back() == ']') {
            auto lb = expr.find('[');
            if (lb != std::string::npos && lb > 0) {
                std::string recv = expr.substr(0, lb);
                std::string inner = expr.substr(lb + 1, expr.size() - lb - 2);
                bool identRecv = true;
                for (char c : recv)
                    if (!std::isalnum((unsigned char)c) && c != '_') { identRecv = false; break; }
                if (identRecv && !inner.empty()) {
                    // t[i] on a known tuple var — see lowerTupleIndexCore's own comment for
                    // the rules. This legacy string-expression path (PropAssign's RHS, among
                    // others — anywhere text gets re-lowered rather than the structured AST
                    // going through lowerExprNode) needs the exact same redirect lowerExprNode's
                    // own IndexExpr case has, or it falls straight through to a real LOAD_INDEX
                    // against a bundle instance — verified real bug: `h.val = t[1]` crashed at
                    // runtime ("'AcTuple2_i_i' object is not subscriptable") since t is not
                    // actually a list.
                    std::string scalKey = currentFunc_ + "::" + recv;
                    auto scalIt = scalarizableTupleVars_.find(scalKey);
                    auto tv = tupleInstanceVars_.find(recv);
                    if (scalIt != scalarizableTupleVars_.end() || tv != tupleInstanceVars_.end()) {
                        IRRef idxRef = lowerExpr(inner);
                        if (scalIt != scalarizableTupleVars_.end())
                            return lowerTupleScalarIndex(recv, scalKey, scalIt->second.isAny, idxRef);
                        return lowerTupleIndex(tv->first, tv->second, idxRef);
                    }
                    IRRef idx = lowerExpr(inner);
                    IRRef adj = adjustIndex(idx);   // string dict keys pass through; ints -1
                    IRRef dst = mkTemp();
                    emit(IRInstruction(IROpcode::LOAD_INDEX, dst, {mkVar(recv), adj}));
                    return dst;
                }
            }
        }

        // receiver.method in expression context (no parens) — promote to call
        {
            auto dotPos = expr.rfind('.');
            if (dotPos != std::string::npos && dotPos > 0) {
                std::string receiver = expr.substr(0, dotPos);
                std::string method   = expr.substr(dotPos + 1);
                // Only when receiver is a plain variable (no dots — not a library namespace)
                if (receiver.find('.') == std::string::npos) {
                    // String-cheese methods: lower(s), upper(s), strip(s), find(s,p), trim(s), len(s)
                    static const std::unordered_set<std::string> strMethods = {
                        "lower","upper","LOWER","UPPER","strip","STRIP",
                        "trim","TRIM","length","len","LEN","format","FORMAT"
                    };
                    // Widget / object no-arg methods: get(), mainloop(), update()
                    static const std::unordered_set<std::string> noArgMethods = {
                        "get","mainloop","update","destroy","pack","clear"
                    };
                    static const std::unordered_set<std::string> libNS2 = {
                        "math","os","regex","gl","maudio","camera","widgets",
                        "stringm","term","Term","sidebar","screen"
                    };
                    if (strMethods.count(method) && !libNS2.count(receiver)
                        && !prog.importedLibs.count(receiver)) {
                        // Emit: stringm.method(receiver)
                        IRRef t = mkTemp();
                        IRInstruction call(IROpcode::LIB_CALL);
                        call.result = t;
                        call.typedOperands = {mkConst("stringm." + method), mkVar(receiver)};
                        emit(std::move(call));
                        return t;
                    } else if (noArgMethods.count(method)) {
                        // Emit: receiver.method() — widget/object no-arg call
                        IRRef t = mkTemp();
                        IRInstruction call(IROpcode::LIB_CALL);
                        call.result = t;
                        call.typedOperands = {mkVar(expr)};  // codegen emits expr() with no args
                        emit(std::move(call));
                        return t;
                    }
                }
            }
        }

        // compound arithmetic: detect binary operator (+, -, *, /)
        // simple single-pass scan (handles "a + b", "a - b", etc.)
        // We look for the last +/- or first */ outside parens
        int depth = 0;
        int opPos = -1;
        IROpcode opcode = IROpcode::NOP;

        // Marks which character positions fall INSIDE a `$...$` string region (between an
        // opening $ and its matching close) — the operator scans below only track paren depth,
        // so a literal `-`/`+` that's meant to be STRING CONTENT (e.g. `$--no-cache$`'s leading
        // `--`, a CLI flag) was being read as a real SUB/ADD operator instead of text (verified:
        // `os.bash($ac $ + path + $ --target $ + backend + $ --no-cache...$)` — the `--` inside
        // "--no-cache" produced a spurious SUBTRACT of two unrelated temps instead of leaving
        // that text alone; only surfaced once the dollar-count fix above stopped short-circuiting
        // this whole expression as one opaque literal in the first place).
        std::vector<bool> inStrMask(expr.size(), false);
        {
            bool inside = false;
            for (size_t i = 0; i < expr.size(); i++) {
                if (expr[i] == '$') { inside = !inside; continue; }
                inStrMask[i] = inside;
            }
        }

        // Space-delimited WORD operators (bxor/band/bor/and/or/xor) — lowest precedence first,
        // rightmost at paren-depth 0. Without this, `arr[i] = a bxor 17` reached the string path
        // and emitted raw "a bxor17" (operator untranslated) on C/etc.
        {
            // The parser may glue tokens (spaces stripped): `arr[1] bxor 6` → "arr[1]bxor6".
            // So match the bare keyword with a LEFT word-boundary (char before is not [A-Za-z0-9_],
            // rejecting mid-identifier like "mybxor") — no spaces required.
            static const std::vector<std::pair<std::string, IROpcode>> wordOps = {
                {"or",  IROpcode::OR},   {"xor", IROpcode::XOR}, {"and", IROpcode::AND},
                {"bor", IROpcode::BOR},  {"bxor",IROpcode::BXOR},{"band",IROpcode::BAND},
            };
            auto idc = [](char c){ return std::isalnum((unsigned char)c) || c == '_'; };
            int wOpPos = -1, wOpLen = 0; IROpcode wOpc = IROpcode::NOP;
            for (auto& [tok, oc] : wordOps) {
                int d = 0;
                for (int i = (int)expr.size() - (int)tok.size(); i > 0; --i) {
                    char c = expr[i];
                    if (c == ')' || c == ']') d++; else if (c == '(' || c == '[') d--;
                    else if (d == 0 && !inStrMask[i] && expr.compare(i, tok.size(), tok) == 0
                             && !idc(expr[i-1])                                            // left word-boundary
                             && (i + (int)tok.size() >= (int)expr.size()
                                 || !idc(expr[i + (int)tok.size()]))) {                     // right word-boundary
                        // both boundaries → won't match "or" inside "orbit"/"factor" etc.
                        wOpPos = i; wOpLen = (int)tok.size(); wOpc = oc; break;
                    }
                }
                if (wOpPos >= 0) break;
            }
            if (wOpPos > 0) {
                std::string lhs = expr.substr(0, wOpPos);
                std::string rhs = expr.substr(wOpPos + wOpLen);
                while (!lhs.empty() && lhs.back() == ' ') lhs.pop_back();
                while (!rhs.empty() && rhs.front() == ' ') rhs.erase(rhs.begin());
                IRRef lRef = lowerExpr(lhs), rRef = lowerExpr(rhs), dst = mkTemp();
                emit(IRInstruction(wOpc, dst, {lRef, rRef}));
                return dst;
            }
        }

        // scan right-to-left for +/- (lowest precedence)
        for (int i = (int)expr.size() - 1; i >= 0; --i) {
            char c = expr[i];
            if (c == ')') depth++;
            else if (c == '(') depth--;
            else if (depth == 0 && !inStrMask[i] && (c == '+' || c == '-') && i > 0) {
                opPos = i;
                opcode = (c == '+') ? IROpcode::ADD : IROpcode::SUB;
                break;
            }
        }
        // if no +/-, scan for */@ (higher precedence)
        if (opPos == -1) {
            depth = 0;
            for (int i = (int)expr.size() - 1; i >= 0; --i) {
                char c = expr[i];
                if (c == ')') depth++;
                else if (c == '(') depth--;
                else if (depth == 0 && !inStrMask[i] && (c == '*' || c == '/' || c == '@') && i > 0) {
                    opPos = i;
                    opcode = (c == '*') ? IROpcode::MUL : (c == '@') ? IROpcode::PMUL : IROpcode::DIV;
                    break;
                }
            }
        }

        if (opPos > 0) {
            std::string lhs = expr.substr(0, opPos);
            std::string rhs = expr.substr(opPos + 1);
            // trim spaces
            while (!lhs.empty() && lhs.back() == ' ') lhs.pop_back();
            while (!rhs.empty() && rhs.front() == ' ') rhs.erase(rhs.begin());

            IRRef lRef = lowerExpr(lhs);
            IRRef rRef = lowerExpr(rhs);
            IRRef dst  = mkTemp();

            IRInstruction i(opcode, dst, {lRef, rRef});
            emit(std::move(i));
            return dst;
        }

        // function call inside expression: name(args)
        auto paren = expr.find('(');
        if (paren != std::string::npos && expr.back() == ')') {
            std::string fname = expr.substr(0, paren);
            std::string argsStr = expr.substr(paren + 1, expr.size() - paren - 2);
            IRRef dst = mkTemp();
            // Split argsStr on TOP-LEVEL commas (respecting [ ] ( ) and $…$) and lower each
            // argument. Passing the whole string as one const meant `sort([5,2,9])` sent the
            // text "[5, 2, 9]" as the argument — BNY then dereferenced a null "pointer".
            std::vector<IRRef> ops = {mkVar(fname)};
            std::string cur;
            int depth = 0; bool inStr = false;
            auto flush = [&]() {
                while (!cur.empty() && cur.front() == ' ') cur.erase(cur.begin());
                while (!cur.empty() && cur.back() == ' ') cur.pop_back();
                if (cur.empty()) return;
                if (!lowerListLiteralArg(cur, ops)) ops.push_back(lowerExpr(cur));
                cur.clear();
            };
            for (char c : argsStr) {
                if (c == '$') inStr = !inStr;
                if (!inStr) {
                    if (c == '[' || c == '(') depth++;
                    else if (c == ']' || c == ')') depth--;
                    else if (c == ',' && depth == 0) { flush(); continue; }
                }
                cur += c;
            }
            flush();
            IRInstruction i(IROpcode::CALL, dst, ops);
            emit(std::move(i));
            return dst;
        }

        // plain variable name
        return mkVar(expr);
    }

    // ── compound assignment helper ──────────────────────────────────────────

    // `x += rRef` etc (already-lowered rhs): if x is `atomic`, LOCK_BEGIN goes before the
    // READ of x's current value (the ADD/SUB/etc below), not just around the final
    // STORE_VAR — closing the real TOCTOU race every backend's atomic codegen used to
    // have (the lock previously only ever wrapped the write; the read that computed the
    // new value happened earlier, unlocked, as a separate instruction — see
    // ac_atomic_rmw_race_fixed memory for the full trace). LOCK_BEGIN/LOCK_END are
    // real opcodes now (include/ir.hpp) that every backend implements by holding its
    // already-existing atomic lock object across the whole bracketed span instead of
    // just a single statement.
    void emitCompoundRef(IROpcode op, const std::string& varName, IRRef rRef) {
        IRRef lRef = mkVar(varName);
        bool atomic = isAtomicRef(lRef);
        if (atomic) emit(IRInstruction(IROpcode::LOCK_BEGIN));
        IRRef tmp = mkTemp();
        emit(IRInstruction(op, tmp, {lRef, rRef}));
        IRInstruction st(IROpcode::STORE_VAR);
        st.typedOperands = {mkVar(varName), tmp};
        emit(std::move(st));
        if (atomic) emit(IRInstruction(IROpcode::LOCK_END));
    }
    void emitCompound(IROpcode op, const std::string& varName, const std::string& rhs) {
        emitCompoundRef(op, varName, lowerExpr(rhs));
    }

    // ── else-chain helper (high-level IR only) ──────────────────────────────
    // Processes ElseIfStmt/OTHER children as nested if-else-endif blocks so
    // Python/JS backends get proper `else: if:` nesting instead of a flat list.
    void genElseChain(const std::vector<std::unique_ptr<ASTNode>>& children, size_t idx) {
        if (idx >= children.size()) return;
        const ASTNode& node = *children[idx];

        if (node.type == NodeType::ElseIfStmt) {
            if (node.children.empty() || node.children[0]->type == NodeType::Block)
                return; // malformed elseif — no condition
            IRRef condRef = lowerExprNode(*node.children[0]);
            size_t bodyIdx = 1;

            IRInstruction ifBegin(IROpcode::IF_BEGIN);
            ifBegin.typedOperands = {condRef};
            emit(std::move(ifBegin));

            if (bodyIdx < node.children.size()) gen(*node.children[bodyIdx]);

            if (idx + 1 < children.size()) {
                emit(IRInstruction(IROpcode::IF_ELSE));
                genElseChain(children, idx + 1);
            }

            emit(IRInstruction(IROpcode::IF_END));
        } else if (node.type == NodeType::IfStmt && node.value == "OTHER") {
            if (!node.children.empty()) gen(*node.children[0]);
        }
    }

    // ── else-chain helper (low-level IR: BNY/ASM) ────────────────────────────
    // `NodeType::ElseIfStmt`'s OWN handler (see its case below) only jumps past ITS OWN body on
    // the FALSE path (`skipL`) — it has no way to jump past the REST of an IF/ELSEIF/OTHER chain
    // on the TRUE path, because the plain `for (i = bodyIndex+1..) gen(children[i])` loop this
    // used to be called from treats ElseIf/OTHER as a flat sibling list with no shared "end"
    // label threaded through. Concretely: `IF a ELSEIF b {body} OTHER {other}` — when `b` is
    // true, `body` ran, but execution then fell straight through into `other` too (verified:
    // `examples/keyword_catalog_core.ac`'s ELSEIF branch printed "small" AND "other-branch",
    // matching neither PY nor the language's own control-flow semantics). Mirrors the
    // high-level `genElseChain` above structurally, but threads a real jump-to-`endL` (owned by
    // the outer IfStmt) through each ElseIf link instead of relying on IF_BEGIN/IF_ELSE/IF_END
    // nesting (which only the high-level backends consume).
    void genElseChainLowLevel(const std::vector<std::unique_ptr<ASTNode>>& children, size_t idx,
                              const IRRef& endL) {
        if (idx >= children.size()) return;
        const ASTNode& node = *children[idx];

        if (node.type == NodeType::ElseIfStmt) {
            if (node.children.empty() || node.children[0]->type == NodeType::Block)
                return; // malformed elseif — no condition
            IRRef condRef = lowerExprNode(*node.children[0]);
            size_t bodyIdx = 1;

            IRRef skipL = mkLabel();
            emitJF(condRef, skipL);
            if (bodyIdx < node.children.size()) gen(*node.children[bodyIdx]);
            bool hasMore = idx + 1 < children.size();
            if (hasMore) emitJump(endL);   // the missing piece: skip the rest of the chain
            emitLabel(skipL);
            genElseChainLowLevel(children, idx + 1, endL);
        } else if (node.type == NodeType::IfStmt && node.value == "OTHER") {
            if (!node.children.empty()) gen(*node.children[0]);
        }
    }

    // ── node visitor ────────────────────────────────────────────────────────

    void gen(const ASTNode& n) {
        switch (n.type) {

        case NodeType::Program:
        case NodeType::Block:
            for (auto& c : n.children) gen(*c);
            break;

        case NodeType::TagBlock: {
            const std::string& tag = n.value;

            // Local: truly transparent — no IR markers
            if (tag == "Local") { for (auto& c : n.children) gen(*c); break; }

            // mainloop: entry point — switch to main section.
            // No closing <mainloop> tag → body loops forever (parser marks "__infinite__").
            if (tag == "mainloop") {
                prog.hadExplicitMainloop = true;
                inMainSection = true;
                bool infinite = std::find(n.attrs.begin(), n.attrs.end(), "__infinite__") != n.attrs.end();
                emitTag(IROpcode::TAG_BEGIN, tag);
                if (infinite) {
                    prog.infiniteMainloop = true;
                    if (prog.useHighLevelIR) {
                        // while (true) { body }  — no break sentinel is emitted here, but push
                        // the loop sentinels so a `/stop` (break) inside the body still targets it.
                        loopEnd.push(mkConst("__break__"));
                        loopStart.push(mkConst("__continue__"));
                        emit(IRInstruction(IROpcode::WHILE_BEGIN));   // empty cond = while true
                        for (auto& c : n.children) gen(*c);
                        emit(IRInstruction(IROpcode::WHILE_END));
                        loopEnd.pop(); loopStart.pop();
                    } else {
                        // Low-level (BNY/ASM): label at top, jump back at bottom.
                        loopEnd.push(mkConst("__break__"));
                        loopStart.push(mkConst("__continue__"));
                        IRRef loopL = mkLabel();
                        emitLabel(loopL);
                        for (auto& c : n.children) gen(*c);
                        emitJump(loopL);
                        loopEnd.pop(); loopStart.pop();
                    }
                } else {
                    for (auto& c : n.children) gen(*c);
                }
                emitTag(IROpcode::TAG_END, tag);
                break;
            }

            // StartHere: entry point — GL game loop
            // Python: while True: t=gl_frame_begin(); if not t: break; ...body...; render(); end()
            if (tag == "StartHere") {
                prog.hadExplicitMainloop = true;
                inMainSection = true;
                emitTag(IROpcode::TAG_BEGIN, tag);
                // Poll every `configure event-listener` binding's real SDL key state and call
                // its callback directly (bypassing EVENT_TRIGGER/_ac_trigger's table lookup —
                // we already have the exact callback name from KeyBinding's lowering). This is
                // the piece that was ENTIRELY MISSING before: EVENT_BIND correctly registered
                // callbacks and EVENT_TRIGGER correctly invoked them, but nothing ever CALLED
                // EVENT_TRIGGER based on actual keyboard state — only an explicit `input <key>`
                // statement did, which no game loop ever wrote. `gl:key.just_pressed`/
                // `gl:key.pressed` reach the already-implemented (previously unreferenced from
                // anywhere) ac_gl_key_just_pressed/ac_gl_key_pressed via the same "gl:noun.verb"
                // → "ac_gl_noun_verb" resolution every other gl: call already goes through.
                auto emitKeyPolling = [&]() {
                    for (auto& [key, cbName, continuous] : polledKeyBindings_) {
                        IRRef pdst = mkTemp();
                        IRInstruction poll(IROpcode::LIB_CALL, pdst, {});
                        poll.typedOperands.push_back(mkConst(continuous ? "gl:key.pressed" : "gl:key.just_pressed"));
                        poll.typedOperands.push_back(mkConst(key));
                        emit(std::move(poll));
                        if (prog.useHighLevelIR) {
                            IRInstruction ifb(IROpcode::IF_BEGIN);
                            ifb.typedOperands = {pdst};
                            emit(std::move(ifb));
                            IRInstruction call(IROpcode::CALL);
                            call.typedOperands = {mkVar(cbName)};
                            emit(std::move(call));
                            emit(IRInstruction(IROpcode::IF_END));
                        } else {
                            IRRef skipL = mkLabel();
                            IRInstruction jf(IROpcode::JUMP_IF_FALSE);
                            jf.typedOperands = {pdst, skipL};
                            emit(std::move(jf));
                            IRInstruction call(IROpcode::CALL);
                            call.typedOperands = {mkVar(cbName)};
                            emit(std::move(call));
                            emitLabel(skipL);
                        }
                    }
                };
                if (prog.useHighLevelIR) {
                    IRRef breakSentinel = mkConst("__break__");
                    loopEnd.push(breakSentinel);
                    loopStart.push(mkConst("__continue__"));

                    emit(IRInstruction(IROpcode::WHILE_BEGIN));

                    // t = gl:frame.begin(); if not t: break
                    IRRef dst = mkTemp();
                    IRInstruction callBegin(IROpcode::LIB_CALL, dst, {});
                    callBegin.typedOperands.push_back(mkConst("gl:frame.begin"));
                    emit(std::move(callBegin));
                    IRInstruction jfb(IROpcode::JUMP_IF_FALSE);
                    jfb.typedOperands = {dst, breakSentinel};
                    emit(std::move(jfb));

                    // gl:frame.update(dt) — apply velocity + CircleFall physics each frame
                    // (without this, objects with velocity never move; the loop was begin→body→render)
                    { IRInstruction u(IROpcode::LIB_CALL);
                      u.typedOperands = {mkConst("gl:frame.update"), IRRef::constant(IRValue(0.016))};
                      emit(std::move(u)); }

                    emitKeyPolling();

                    for (auto& c : n.children) gen(*c);

                    // gl:frame.render(); gl:frame.end() at bottom of loop
                    IRInstruction callRender(IROpcode::LIB_CALL);
                    callRender.typedOperands.push_back(mkConst("gl:frame.render"));
                    emit(std::move(callRender));
                    IRInstruction callEnd(IROpcode::LIB_CALL);
                    callEnd.typedOperands.push_back(mkConst("gl:frame.end"));
                    emit(std::move(callEnd));

                    emit(IRInstruction(IROpcode::WHILE_END));
                    loopEnd.pop(); loopStart.pop();
                } else {
                    IRRef loopL = mkLabel();
                    emitLabel(loopL);
                    { IRInstruction b(IROpcode::LIB_CALL); b.typedOperands = {mkConst("gl:frame.begin")}; emit(std::move(b)); }
                    { IRInstruction u(IROpcode::LIB_CALL);
                      u.typedOperands = {mkConst("gl:frame.update"), IRRef::constant(IRValue(0.016))}; emit(std::move(u)); }
                    emitKeyPolling();
                    for (auto& c : n.children) gen(*c);
                    { IRInstruction r(IROpcode::LIB_CALL); r.typedOperands = {mkConst("gl:frame.render")}; emit(std::move(r)); }
                    { IRInstruction e(IROpcode::LIB_CALL); e.typedOperands = {mkConst("gl:frame.end")}; emit(std::move(e)); }
                    emitJump(loopL);
                }
                emitTag(IROpcode::TAG_END, tag);
                break;
            }

            // bound: scoped block — codegen emits { } or if True: as appropriate
            if (tag == "bound") {
                emitTag(IROpcode::TAG_BEGIN, tag);
                for (auto& c : n.children) gen(*c);
                emitTag(IROpcode::TAG_END, tag);
                break;
            }

            // free: variables assigned inside become globally accessible
            if (tag == "free") {
                emitTag(IROpcode::TAG_BEGIN, tag);
                bool prevFree = inFreeScope;
                std::set<int> prevFreeIds;
                if (!prevFree) {
                    inFreeScope = true;
                    freeDeclaredIds.clear();
                } else {
                    prevFreeIds = freeDeclaredIds;
                }
                for (auto& c : n.children) gen(*c);
                if (!prevFree) {
                    inFreeScope = false;
                    freeDeclaredIds.clear();
                } else {
                    freeDeclaredIds = prevFreeIds;
                }
                emitTag(IROpcode::TAG_END, tag);
                break;
            }

            // shutoff: user-defined cleanup — compiled as __ac_shutoff__ function
            if (tag == "shutoff") {
                prog.functions.push_back(IRFunction("__ac_shutoff__"));
                IRFunction* prevCur = cur;
                cur = &prog.functions.back();
                for (auto& c : n.children) gen(*c);
                emit(IRInstruction(IROpcode::RETURN));
                cur = prevCur;
                prog.hasShutoff = true;
                break;
            }

            // Custom tag invocation: <terrain> inside a block → call spawn_terrain()
            {
                std::string spawnName = "spawn_" + tag;
                bool isCustom = false;
                for (auto& f : prog.functions)
                    if (f.name == spawnName) { isCustom = true; break; }
                if (isCustom) {
                    IRRef dst = mkTemp();
                    IRInstruction i(IROpcode::CALL, dst, {mkVar(spawnName)});
                    emit(std::move(i));
                    break;
                }
            }

            // All other tags (gui, SCREEN, LOGIC, OBJECT, standard): emit TAG markers + walk children
            emitTag(IROpcode::TAG_BEGIN, tag);
            for (auto& c : n.children) gen(*c);
            emitTag(IROpcode::TAG_END, tag);
            break;
        }

        case NodeType::BackendDecl:
            break; // metadata only

        case NodeType::SaveStmt: {
            // `save as <file>` — writes everything Term.display'd SO FAR (from program start up
            // to this statement) to the named file. Was a complete no-op before ("metadata
            // only") despite being a real, documented feature (CHANGELOG: "save as records a
            // save directive") — no file was ever written, on any backend.
            IRInstruction i(IROpcode::SAVE_FILE);
            i.typedOperands = {mkConst(n.value)};
            emit(std::move(i));
            break;
        }

        // ── function definition ─────────────────────────────────────────────
        case NodeType::FuncDef: {
            // Intern function name in symbol table
            int funcId = prog.symbols.intern(
                currentClass_.empty() ? n.value : currentClass_ + "." + n.value,
                IRType::FUNCTION);

            IRFunction fn(n.value, currentClass_);
            fn.returnType = IRType::VOID;
            fn.isGenerator = generatorFuncNames_.count(
                currentClass_.empty() ? n.value : currentClass_ + "." + n.value) > 0;

            // Scope guard: guarantees exitScope even if gen() throws
            ScopeGuard scope(prog.symbols);

            // Methods receive 'self' as first parameter.
            // If the user explicitly wrote func(self), do not duplicate it.
            if (!currentClass_.empty()) {
                bool userHasSelf = (!n.attrs.empty() && n.attrs[0] == "self");
                if (!userHasSelf) {
                    prog.symbols.intern("self");
                    fn.parameters.push_back("self");
                }
            }

            // Intern parameters in symbol table
            for (auto& p : n.attrs) {
                prog.symbols.intern(p);
                fn.parameters.push_back(p);
            }

            prog.functions.push_back(std::move(fn));
            cur = &prog.functions.back();

            // function entry label
            IRInstruction entry(IROpcode::FUNC_BEGIN);
            entry.typedOperands = {IRRef::func(funcId)};
            emit(std::move(entry));

            // Matches collectTupleTrackability's own qualification exactly, so a
            // scalarizableTupleVars_ lookup built here always agrees with what the prepass
            // computed. Saved/restored (not just cleared) in case this ever nests — this
            // language has no closures, so it shouldn't in practice, but restoring costs
            // nothing and avoids leaking a wrong context into whatever called into this one.
            std::string savedFunc = currentFunc_;
            currentFunc_ = currentClass_.empty() ? n.value : currentClass_ + "." + n.value;

            // Tuple-parameter pre-seeding (see tupleParamClasses_'s own comment and
            // discoverTupleParamShapes): on the real (2nd) generation pass, a parameter
            // discovery proved receives a tuple argument at some call site is treated exactly
            // like an already-bound tupleInstanceVars_ entry from the very first statement of
            // this body, so IndexExpr's existing t[i] lookup picks it up with zero changes of
            // its own — fixes `Make show func(p): Term.display p[1]` crashing when called as
            // `show(t)` (p[1] used to fall through to plain list-index codegen since nothing
            // ever told this function's body that p is a tuple). tupleInstanceVars_ is a flat,
            // unscoped map (matches lowerTupleEscaping's own convention), so save/restore by
            // name around this one function body only — leaving a stale entry after returning
            // would incorrectly leak into any later, unrelated var of the same name.
            std::vector<std::pair<std::string, std::string>> savedTupleParamEntries;
            auto tpcIt = tupleParamClasses_.find(currentFunc_);
            if (tpcIt != tupleParamClasses_.end()) {
                for (auto& [idx, className] : tpcIt->second) {
                    if (idx < 0 || (size_t)idx >= n.attrs.size()) continue;
                    const std::string& pname = n.attrs[(size_t)idx];
                    auto prior = tupleInstanceVars_.find(pname);
                    savedTupleParamEntries.push_back(
                        {pname, prior != tupleInstanceVars_.end() ? prior->second : std::string()});
                    tupleInstanceVars_[pname] = className;
                }
            }

            if (!n.children.empty()) gen(*n.children[0]);

            // implicit void return if no explicit return
            IRInstruction ret(IROpcode::RETURN);
            ret.typedOperands = {};
            emit(std::move(ret));

            IRInstruction end(IROpcode::FUNC_END);
            end.typedOperands = {IRRef::func(funcId)};
            emit(std::move(end));

            currentFunc_ = savedFunc;
            for (auto& [pname, priorClass] : savedTupleParamEntries) {
                if (priorClass.empty()) tupleInstanceVars_.erase(pname);
                else tupleInstanceVars_[pname] = priorClass;
            }
            cur = nullptr;
            break;
        }

        // ── bundle (class) definition ────────────────────────────────────────
        case NodeType::BundleDef: {
            const std::string& className = n.value;

            // CLASS_BEGIN marker in globalInit
            IRInstruction cb(IROpcode::CLASS_BEGIN);
            cb.typedOperands = {mkConst(className)};
            emit(std::move(cb));

            // Lower body inside class context.
            // Bundle field defaults should become instance fields (set in init), not class/static vars.
            currentClass_ = className;
            std::vector<std::pair<std::string, const ASTNode*>> fieldDefaults;
            bool hasUserInit = false;
            // Body is either: old style — one Block child; new style — BundleMember children.
            // Normalize to a flat list of (access, stmt*) pairs.
            std::vector<std::pair<std::string, const ASTNode*>> bodyStmts;
            if (!n.children.empty() && n.children[0] && n.children[0]->type == NodeType::Block) {
                for (const auto& sp : n.children[0]->children)
                    if (sp) bodyStmts.push_back({"public", sp.get()});
            } else {
                for (const auto& mp : n.children) {
                    if (!mp) continue;
                    const std::string& acc = (mp->type == NodeType::BundleMember) ? mp->value : "public";
                    const ASTNode* stmt = (mp->type == NodeType::BundleMember && !mp->children.empty())
                                          ? mp->children[0].get() : mp.get();
                    if (stmt) bodyStmts.push_back({acc, stmt});
                }
            }

            if (!bodyStmts.empty()) {
                // Collect field defaults and detect init method
                for (const auto& [acc, stmtRaw] : bodyStmts) {
                    (void)acc;
                    const auto& stmt = *stmtRaw;
                    if (stmt.type == NodeType::AssignStmt) {
                        // Only treat bare identifiers as fields (no dots).
                        if (stmt.value.find('.') == std::string::npos) {
                            if (!stmt.children.empty())
                                fieldDefaults.push_back({stmt.value, stmt.children[0].get()});
                            else if (!stmt.attrs.empty()) {
                                // Legacy string-based assignment: lowerExpr() later
                                // Store nullptr to signal legacy; handled below.
                                fieldDefaults.push_back({stmt.value, nullptr});
                            }
                        }
                    } else if (stmt.type == NodeType::FuncDef && stmt.value == "init") {
                        hasUserInit = true;
                    }
                }

                // Emit a synthetic init if user didn't define one but there are fields.
                if (!hasUserInit && !fieldDefaults.empty()) {
                    int funcId = prog.symbols.intern(className + ".init", IRType::FUNCTION);
                    IRFunction fn("init", className);
                    fn.returnType = IRType::VOID;
                    prog.symbols.intern("self");
                    fn.parameters.push_back("self");
                    prog.functions.push_back(std::move(fn));
                    cur = &prog.functions.back();
                    IRInstruction entry(IROpcode::FUNC_BEGIN);
                    entry.typedOperands = {IRRef::func(funcId)};
                    emit(std::move(entry));

                    // Field initializers
                    for (const auto& [field, exprNode] : fieldDefaults) {
                        IRRef dst = mkVar("self." + field);
                        if (exprNode) {
                            IRRef src = lowerExprNode(*exprNode);
                            IRInstruction i(IROpcode::STORE_VAR, dst, {src});
                            i.resultType = typeOfRef(src);
                            emit(std::move(i));
                        } else {
                            // Legacy: value stored in attrs[0] on the AssignStmt. Re-lower from string.
                            // (We don't have the original string here; default to 0.)
                            IRInstruction i(IROpcode::STORE_VAR, dst, {mkConstInt(0)});
                            i.resultType = IRType::INT;
                            emit(std::move(i));
                        }
                    }

                    IRInstruction ret(IROpcode::RETURN); ret.typedOperands = {}; emit(std::move(ret));
                    IRInstruction end(IROpcode::FUNC_END); end.typedOperands = {IRRef::func(funcId)}; emit(std::move(end));
                    cur = nullptr;
                }

                // Lower remaining statements:
                // - Skip bare field-default AssignStmt (handled via init)
                // - For user-defined init, we'll prepend initializers by emitting them right after FUNC_BEGIN.
                for (const auto& [acc2, stmtRaw2] : bodyStmts) {
                    (void)acc2;
                    if (!stmtRaw2) continue;
                    const auto& stmt = *stmtRaw2;
                    if (stmt.type == NodeType::AssignStmt && stmt.value.find('.') == std::string::npos)
                        continue; // field default

                    if (stmt.type == NodeType::FuncDef && stmt.value == "init" && !fieldDefaults.empty()) {
                        // Manual lowering of init so we can inject field initializers at the beginning.
                        int funcId = prog.symbols.intern(className + ".init", IRType::FUNCTION);
                        IRFunction fn("init", className);
                        fn.returnType = IRType::VOID;

                        ScopeGuard scope(prog.symbols);

                        // Parameters: include user args; ensure self exists once.
                        prog.symbols.intern("self");
                        fn.parameters.push_back("self");
                        for (auto& p : stmt.attrs) {
                            if (p == "self") continue;
                            prog.symbols.intern(p);
                            fn.parameters.push_back(p);
                        }
                        prog.functions.push_back(std::move(fn));
                        cur = &prog.functions.back();
                        IRInstruction entry(IROpcode::FUNC_BEGIN);
                        entry.typedOperands = {IRRef::func(funcId)};
                        emit(std::move(entry));

                        // Inject field initializers before user body.
                        for (const auto& [field, exprNode] : fieldDefaults) {
                            IRRef dst = mkVar("self." + field);
                            if (exprNode) {
                                IRRef src = lowerExprNode(*exprNode);
                                IRInstruction i(IROpcode::STORE_VAR, dst, {src});
                                i.resultType = typeOfRef(src);
                                emit(std::move(i));
                            } else {
                                IRInstruction i(IROpcode::STORE_VAR, dst, {mkConstInt(0)});
                                i.resultType = IRType::INT;
                                emit(std::move(i));
                            }
                        }

                        if (!stmt.children.empty()) gen(*stmt.children[0]);
                        IRInstruction ret(IROpcode::RETURN); ret.typedOperands = {}; emit(std::move(ret));
                        IRInstruction end(IROpcode::FUNC_END); end.typedOperands = {IRRef::func(funcId)}; emit(std::move(end));
                        cur = nullptr;
                        continue;
                    }

                    // Default: lower normally.
                    gen(stmt);
                }
            }

            currentClass_ = "";

            // CLASS_END marker
            IRInstruction ce(IROpcode::CLASS_END);
            ce.typedOperands = {mkConst(className)};
            emit(std::move(ce));
            break;
        }

        // ── assignment ──────────────────────────────────────────────────────
        case NodeType::AssignStmt: {
            // Const reassignment guard
            if (prog.constVars.count(n.value)) {
                std::cerr << "Preposterous: Cannot reassign const variable '" << n.value << "'\n";
                break;
            }
            IRRef dst = mkVar(n.value);

            // Check for special assignment types first (before checking children)
            if (!n.attrs.empty()) {
                const std::string& raw = n.attrs[0];
                
                // Function call result stored in variable
                if (raw.substr(0, 11) == "__funcall__") {
                    if (!n.children.empty() && n.children[0]->type == NodeType::FunctionCall) {
                        auto& fc = *n.children[0];
                        std::string fcFname = fc.value;
                        {
                            auto ait = selectiveImportAliases_.find(fcFname);
                            if (ait != selectiveImportAliases_.end()) fcFname = ait->second;
                        }
                        std::vector<IRRef> ops = {mkVar(fcFname)};
                        // Screen's title is mandatory and must use the title= keyword — there is
                        // no positional form and no geometry argument at all anymore (size the
                        // window with .dimensions(w, h) instead). Checked against the RAW attrs
                        // (before the keyword-stripping loop below runs) so a bare positional
                        // `Screen($X$)` is rejected just as clearly as `Screen()`.
                        if (fc.value == "Screen") {
                            auto rawAttrs = mergeBracketAttrs(fc.attrs);
                            bool hasTitleKw = !rawAttrs.empty() && rawAttrs[0].rfind("title=", 0) == 0;
                            if (!hasTitleKw) {
                                throw ACError::semantic(
                                    "Screen(...) requires an explicit title= argument, e.g. "
                                    "Screen(title=$My Window$) — there is no positional form and "
                                    "no geometry argument; use .dimensions(w, h) to size the window.");
                            }
                        }
                        for (auto& a0 : mergeBracketAttrs(fc.attrs)) {
                            std::string a = a0;
                            // Keyword-arg sugar for widget ctors: `Screen(title=$X$)` — strip a
                            // recognized "paramName=" prefix so the value flows through the
                            // SAME literal/ident/expr handling below as a plain positional arg
                            // would. A small, targeted table (not a general language keyword-arg
                            // feature).
                            {
                                static const std::unordered_map<std::string, std::vector<std::string>> ctorParamNames = {
                                    {"Screen", {"title"}},
                                };
                                auto pit = ctorParamNames.find(fc.value);
                                if (pit != ctorParamNames.end()) {
                                    auto eq = a.find('=');
                                    if (eq != std::string::npos) {
                                        std::string key = a.substr(0, eq);
                                        bool keyIsIdent = !key.empty();
                                        for (char c : key) if (!std::isalnum((unsigned char)c) && c != '_') { keyIsIdent = false; break; }
                                        if (keyIsIdent) {
                                            bool known = false;
                                            for (auto& nm : pit->second) if (nm == key) { known = true; break; }
                                            if (known) a = a.substr(eq + 1);
                                        }
                                    }
                                }
                            }
                            // Check if attr is a variable name or a constant
                            // If it's a number, make it a const int
                            bool isInt = !a.empty();
                            for (size_t ci = 0; ci < a.size(); ++ci) if (!std::isdigit((unsigned char)a[ci]) && !(a[ci]=='-'&&ci==0)) { isInt = false; break; }
                            if (isInt) {
                                try {
                                    ops.push_back(mkConstInt(std::stoll(a)));
                                } catch (...) {
                                    ops.push_back(mkConst(a));
                                }
                            } else if (a.size() >= 2 && a.front() == '$' && a.back() == '$'
                                       && [&]{ int n=0; for (char c : a) if (c=='$') n++; return n; }() == 2) {
                                // String literal — see the sibling FunctionCall-arg lowering's
                                // matching comment (~line 3131) for why the dollar-count check
                                // is needed here too, not just a first/last-char check.
                                ops.push_back(mkConst(a));
                            } else if (lowerListLiteralArg(a, ops)) {
                                // list literal argument — lowered to an ALLOC temp
                            } else {
                                bool isIdent = true;
                                for (char c : a)
                                    if (!std::isalnum((unsigned char)c) && c != '_' && c != '.') { isIdent = false; break; }
                                if (isIdent)
                                    ops.push_back(mkVar(a));       // variable name
                                else
                                    ops.push_back(lowerExpr(a));   // expression argument
                            }
                        }
                        // `g = twovals()` on a family-C backend (Java/C/CPP/LIB — no native
                        // generator/channel construct): must produce a real handle via
                        // GEN_CREATE, not a plain CALL — matches the same opcode a direct
                        // `FOR x in twovals():` already uses (ir.cpp's ForLoop case) so a LATER
                        // `FOR x in g:` (see generatorHandleVars_'s own comment/prepass) finds a
                        // real, family-C-shaped handle to reuse. Family A/B need no special
                        // case here at all — their generator-ness lives entirely in the
                        // callee's own signature/codegen, so the plain CALL below already
                        // produces the right kind of value (a real Python/JS generator object,
                        // or a Go/Rust/V channel).
                        static const std::set<std::string> genFamilyCA = {"Java", "C", "CPP", "C++", "LIB", "BNY", "ASM"};
                        if (generatorFuncNames_.count(fc.value) && genFamilyCA.count(prog.backend)) {
                            IRInstruction gc(IROpcode::GEN_CREATE, dst, ops);
                            emit(std::move(gc));
                            break;
                        }
                        IRInstruction i(IROpcode::CALL, dst, ops);
                        emit(std::move(i));
                        // Mark dst assigned even though its type stays VOID here (a call
                        // result's real type isn't known until backend-specific inference,
                        // well after this lowering pass) — otherwise a LATER retype of this
                        // same var (`x = someFunc(); x = $hello$`) can't tell "first assignment,
                        // no cast needed" apart from "reassignment of unknown-typed prior value,
                        // genuinely needs a TYPE_CAST" — see everAssignedVarIds_'s own comment.
                        everAssignedVarIds_.insert(dst.id);
                        if (widgetCtorNames().count(fc.value)) widgetCtorVars_.insert(n.value);
                        break;
                    }
                }
            }

            // Check if we have a structured expression as child
            if (!n.children.empty()) {
                // a = [elem, ...] @ n  → ALLOC directly into `a` + fill loop (typed backends
                // declare `a` as a list via emitAlloc; a temp var would type it as int). See
                // splitTopLevelCommas's comment for why a multi-element list literal here needs
                // splitting instead of a single append() call per repeat.
                ASTNode* rep = n.children[0].get();
                if (rep && rep->type == NodeType::BinaryExpr &&
                    (rep->value == "@" || rep->value == "*") &&
                    rep->children.size() >= 2 &&
                    rep->children[0]->type == NodeType::ListLiteral &&
                    (!rep->children[0]->children.empty() || !rep->children[0]->value.empty())) {
                    thread_local int repiC = 0;
                    std::string idxName = "_ac_repi_" + std::to_string(repiC++);
                    IRRef idxV = mkVar(idxName);
                    std::vector<IRRef> elems;
                    if (!rep->children[0]->children.empty()) {
                        for (auto& c : rep->children[0]->children) elems.push_back(lowerExprNode(*c));
                    } else {
                        for (auto& piece : splitTopLevelCommas(rep->children[0]->value)) elems.push_back(lowerExpr(piece));
                    }
                    IRRef count = lowerExprNode(*rep->children[1]);
                    emit(IRInstruction(IROpcode::ALLOC, dst, {mkConst("list"), mkConst("")}));
                    emit(IRInstruction(IROpcode::STORE_VAR, idxV, {mkConstInt(0)}));
                    std::string apName = n.value + ".append";
                    if (prog.useHighLevelIR) {
                        IRRef brk = mkConst("__break__");
                        loopEnd.push(brk); loopStart.push(mkConst("__continue__"));
                        emit(IRInstruction(IROpcode::WHILE_BEGIN));
                        IRRef c = mkTemp();
                        emit(IRInstruction(IROpcode::LT, c, {idxV, count}));
                        { IRInstruction jf(IROpcode::JUMP_IF_FALSE); jf.typedOperands = {c, brk}; emit(std::move(jf)); }
                        for (auto& elem : elems) {
                            IRInstruction ap(IROpcode::LIB_CALL);
                            ap.typedOperands = {mkConst(apName), elem}; emit(std::move(ap));
                        }
                        IRRef inc = mkTemp();
                        emit(IRInstruction(IROpcode::ADD, inc, {idxV, mkConstInt(1)}));
                        emit(IRInstruction(IROpcode::STORE_VAR, idxV, {inc}));
                        emit(IRInstruction(IROpcode::WHILE_END));
                        loopEnd.pop(); loopStart.pop();
                    } else {
                        IRRef startL = mkLabel(), endL = mkLabel();
                        emitLabel(startL);
                        IRRef c = mkTemp();
                        emit(IRInstruction(IROpcode::LT, c, {idxV, count}));
                        emitJF(c, endL);
                        for (auto& elem : elems) {
                            IRInstruction ap(IROpcode::LIB_CALL);
                            ap.typedOperands = {mkConst(apName), elem}; emit(std::move(ap));
                        }
                        IRRef inc = mkTemp();
                        emit(IRInstruction(IROpcode::ADD, inc, {idxV, mkConstInt(1)}));
                        emit(IRInstruction(IROpcode::STORE_VAR, idxV, {inc}));
                        emitJump(startL);
                        emitLabel(endL);
                    }
                    break;
                }
                // t = (x, y) — bind a tuple literal to a var.
                if (rep && rep->type == NodeType::TupleLiteral) {
                    // Proven by collectTupleTrackability to never escape this scope: store
                    // each element straight into flat shadow vars — no bundle, no class, no
                    // allocation at all. This is the "bind now, use later" half of the
                    // "optimize it away completely" ask; DestructureAssignStmt's own
                    // same-statement fast path already covers the other half.
                    auto scalIt = scalarizableTupleVars_.find(currentFunc_ + "::" + n.value);
                    if (scalIt != scalarizableTupleVars_.end()) {
                        std::vector<IRRef> elems;
                        std::vector<IRType> slotTypes;
                        lowerTupleElements(*rep, elems, slotTypes);
                        for (size_t i = 0; i < elems.size(); i++) {
                            IRRef shadow = mkVar(tupleScalarShadowName(n.value, i));
                            setRefType(shadow, slotTypes[i]);
                            IRInstruction st(IROpcode::STORE_VAR, shadow, {elems[i]});
                            st.resultType = slotTypes[i];
                            emit(std::move(st));
                        }
                        tupleScalarShadowTypes_[currentFunc_ + "::" + n.value] = slotTypes;
                        break;
                    }
                    // Otherwise: construct the synthesized bundle DIRECTLY into `dst` (see
                    // lowerTupleEscaping's own comment on why a plain value-copy from a
                    // separately-named instance wouldn't make `dst` trackable as a class
                    // instance for later field access).
                    lowerTupleEscaping(*rep, "bind", &dst);
                    break;
                }
                IRRef src = lowerExprNode(*n.children[0]);
                IRType t  = typeOfRef(src);
                // Automatic retype: when a plain reassignment's value is a DIFFERENT type
                // than what this var was last known to hold, insert a real TYPE_CAST (the
                // same mechanism `dec x = 5`/`atomic x = 5` already use) at exactly this
                // point instead of silently keeping the var's old static type — Abu's own
                // words: "IN THE IR, WHEN TYPE CHANGES WE INSERT A TYPE CHANGE ... IS WHAT
                // HAPPENS WITHOUT THE USER HAVING TO WRITE IT." Verified real bug this
                // closes: `x = 5; x = $String$; x = 5.0` — C had been declaring `x` with
                // ONE static type picked from a whole-lifetime scan (here: `double`, since
                // a float assignment appears later), so the STRING assignment spliced in as
                // a bare, unquoted identifier; JS's compile-time `_acp` vs `_acpf` print-
                // site choice silently lost the float's `.0` formatting once `x` had also
                // been used as a string in between. This is the ACTIVE structured-AST path
                // (the sibling "Legacy string-based handling" below shares the same fix for
                // whatever narrower cases still reach it without a children[0] expression).
                IRType oldType = prog.symbols.getType(dst.id);
                bool wasAssignedBefore = everAssignedVarIds_.count(dst.id) > 0;
                everAssignedVarIds_.insert(dst.id);
                // SHORT/MINI/ATOMIC are "sticky" qualifiers, not just the most recent
                // snapshot type: once a var is declared with one, a later plain-INT
                // reassignment (the ordinary way to use `short`/`mini`/`atomic` itself,
                // e.g. `atomic counter = 10; counter = 100`) must NOT silently downgrade it
                // back to bare INT here — that corrupts prog.symbols' own type for this
                // var, which every LATER reference consults too. Verified real bug this
                // closes: `atomic counter = 10; counter = 100; counter -= 3` silently
                // dropped LOCK_BEGIN/LOCK_END on that trailing `-=` entirely (emitCompoundRef's
                // isAtomicRef check saw the corrupted INT type) — a genuine cross-backend
                // thread-safety regression, not a display/formatting issue.
                //
                // ATOMIC specifically is sticky against EVERY later type, not just INT:
                // it's documented as always an int variable ("atomic x [= expr] — int
                // variable; any op touching it is a global critical section", token.hpp),
                // so `atomic x = 5; x = 5.5` should coerce 5.5 into the atomic int (like
                // any other int coercion), not silently turn x into a genuine float var.
                // Verified real bug the narrower INT-only version still had: several
                // backends' emitTypedStoreVar gate their special atomic-store branch on
                // "is this var's CURRENT type ATOMIC" — once a later FLOAT type_cast won
                // that check, the var's OWN initial `atomic x = 5` declaration stopped
                // being recognized as atomic (varCastTypes only holds the FINAL type),
                // so its declaration was silently skipped — undeclared-variable compile
                // errors on some backends, silently-zero-initialized on others.
                bool stickyKeep = (oldType == IRType::ATOMIC)
                                  || ((oldType == IRType::SHORT || oldType == IRType::MINI)
                                      && t == IRType::INT);
                IRType effType = stickyKeep ? oldType : t;
                setRefType(dst, effType);          // propagate type to destination var
                // A genuine retype needs a TYPE_CAST even when oldType itself is VOID — that
                // happens whenever the PRIOR assignment's value came from something whose type
                // isn't known until backend-specific inference (a function call result), not
                // just "this is x's true first assignment" (wasAssignedBefore tells them apart).
                bool priorTypeIsAmbiguous = (oldType == IRType::VOID) && wasAssignedBefore;
                if (effType != IRType::VOID && (oldType != IRType::VOID || priorTypeIsAmbiguous)
                    && oldType != effType) {
                    IRInstruction ci(IROpcode::TYPE_CAST);
                    ci.typedOperands = {src};
                    ci.result = dst;
                    ci.resultType = effType;
                    emit(std::move(ci));
                } else {
                    IRInstruction i(IROpcode::STORE_VAR, dst, {src});
                    i.resultType = effType;
                    emit(std::move(i));
                }
                break;
            }

            // Legacy string-based handling (fallback)
            if (n.attrs.empty()) break;
            const std::string& raw = n.attrs[0];

            if (raw.substr(0, 8) == "__list__") {
                // #11: split elements — literals stay in the ALLOC text; computed expressions
                // get a 0 placeholder + STORE_INDEX with the lowered value (depth-aware split).
                std::string inner = raw.substr(8);
                std::vector<std::string> elems;
                {
                    auto trimS = [](std::string s) {
                        size_t a = s.find_first_not_of(' '), b = s.find_last_not_of(' ');
                        return (a == std::string::npos) ? std::string() : s.substr(a, b - a + 1);
                    };
                    std::string t = trimS(inner);
                    // Single-span form: the WHOLE content is one `$..$` block with no other
                    // '$' inside (`[$a, b, c$]` — one STRING token whose raw text already
                    // contains the commas, ast.hpp's documented canonical list-of-strings
                    // form) — its OWN internal commas are the element separators, each
                    // re-wrapped so isLiteral() below still recognizes them as complete
                    // strings. This loop used to only track bracket depth, never `$`-span
                    // state, so it split "$a, b, c$" into the three UNBALANCED fragments
                    // "$a"/" b"/" c$" instead — none recognized as a complete string by
                    // isLiteral(), so each was sent through lowerExpr as garbage (verified
                    // real bug: printed literally "$a" instead of "a").
                    if (t.size() >= 2 && t.front() == '$' && t.back() == '$'
                        && t.find('$', 1) == t.size() - 1) {
                        std::string innerSpan = t.substr(1, t.size() - 2);
                        std::string cur;
                        for (char c : innerSpan) {
                            if (c == ',') { elems.push_back("$" + trimS(cur) + "$"); cur.clear(); }
                            else cur += c;
                        }
                        elems.push_back("$" + trimS(cur) + "$");
                    } else {
                        // General (multi-span) form: `[$a$, $b$, $c$]` — each element
                        // individually `$`-delimited, joined by commas genuinely OUTSIDE any
                        // span. Track `inDollar` so a string element's own text (unlikely to
                        // contain a bracket, but never a bare comma either, by construction)
                        // is never split mid-span.
                        std::string cur; int depth = 0; bool inDollar = false;
                        for (char c : inner) {
                            if (c == '$') { inDollar = !inDollar; cur += c; continue; }
                            if (!inDollar && (c=='['||c=='('||c=='{')) depth++;
                            else if (!inDollar && (c==']'||c==')'||c=='}')) depth--;
                            if (c==',' && depth==0 && !inDollar) { elems.push_back(cur); cur.clear(); }
                            else cur += c;
                        }
                        if (!cur.empty()) elems.push_back(cur);
                    }
                }
                auto isLiteral = [this](std::string e) {
                    size_t a=e.find_first_not_of(' '), b=e.find_last_not_of(' ');
                    if (a==std::string::npos) return false;
                    e = e.substr(a, b-a+1);
                    if (e.size()>=2 && e.front()=='$' && e.back()=='$') return true;
                    // A `.datac` row var (see datacDictVars_'s comment) is a bare identifier
                    // reference, not a literal — but it must go into the ALLOC's literal content
                    // text the same as one, so the codegen-side dict-var check can ever see it.
                    if (datacDictVars_.count(e)) return true;
                    size_t i = (e[0]=='-')?1:0; if (i>=e.size()) return false;
                    bool dot=false;
                    for (; i<e.size(); ++i) {
                        if (e[i]=='.') { if (dot) return false; dot=true; }
                        else if (!std::isdigit((unsigned char)e[i])) return false;
                    }
                    return true;
                };
                std::string content;
                std::vector<std::pair<size_t,std::string>> computed;
                for (size_t ci = 0; ci < elems.size(); ci++) {
                    if (ci) content += ",";
                    std::string e = elems[ci];
                    size_t a=e.find_first_not_of(' '), b=e.find_last_not_of(' ');
                    if (a!=std::string::npos) e = e.substr(a, b-a+1);
                    if (isLiteral(e)) content += e;
                    else { content += "0"; computed.push_back({ci, e}); }
                }
                IRInstruction i(IROpcode::ALLOC, dst, {mkConst("list"), mkConst(content)});
                emit(std::move(i));
                setRefType(dst, IRType::LIST);   // type the var LIST (arg cloning, #22)
                for (auto& [ci, e] : computed) {
                    IRRef er = lowerExpr(e);
                    IRInstruction st(IROpcode::STORE_INDEX);
                    st.typedOperands = {dst, mkConstInt((int64_t)ci), er};
                    emit(std::move(st));
                }
            } else if (raw.substr(0, 9) == "__tuple__") {
                IRInstruction i(IROpcode::ALLOC, dst, {mkConst("tuple"), mkConst(raw.substr(9))});
                emit(std::move(i));
            } else if (raw.substr(0, 8) == "__dict__") {
                IRInstruction i(IROpcode::ALLOC, dst, {mkConst("dict"), mkConst(raw.substr(8))});
                emit(std::move(i));
                datacDictVars_.insert(n.value);
            } else if (raw == "__range__") {
                // Range with structured expression
                if (n.children.size() > 0) {
                    IRRef rangeExpr = lowerExprNode(*n.children[0]);
                    IRInstruction i(IROpcode::ALLOC, dst, {mkConst("range"), rangeExpr});
                    emit(std::move(i));
                }
            } else if (raw.substr(0, 9) == "__range__") {
                // Legacy range with string
                IRInstruction i(IROpcode::ALLOC, dst, {mkConst("range"), mkConst(raw.substr(9))});
                emit(std::move(i));
            } else if (raw == "__sequence__") {
                // Sequence with structured expressions
                if (n.children.size() >= 2) {
                    IRRef xExpr = lowerExprNode(*n.children[0]);
                    IRRef yExpr = lowerExprNode(*n.children[1]);
                    // Create a temporary string representation for now
                    // TODO: Improve sequence handling in IR
                    IRInstruction i(IROpcode::ALLOC, dst, {mkConst("sequence"), xExpr, yExpr});
                    emit(std::move(i));
                }
            } else if (raw.substr(0, 12) == "__sequence__") {
                // Legacy sequence with string
                IRInstruction i(IROpcode::ALLOC, dst, {mkConst("sequence"), mkConst(raw.substr(12))});
                emit(std::move(i));
            } else if (raw.substr(0, 6) == "__fn__") {
                IRRef src = lowerExpr(raw.substr(6));
                IRInstruction i(IROpcode::STORE_VAR, dst, {src});
                emit(std::move(i));
            } else {
                // Plain `x = <expr>` (e.g. `x = x + 1`, the un-sugared form of `x += 1`,
                // already handled via emitCompoundRef above). If `x` is atomic, bracket
                // the WHOLE expression evaluation + store in the lock, not just the
                // store — unconditionally, whether or not `expr` textually references
                // `x` itself: harmless extra scope when it doesn't (matches what the
                // old single-store auto-wrap already did for a non-self-referential
                // atomic reassignment), and closes the real TOCTOU race when it does
                // (see emitCompoundRef's comment for the full story). Detecting
                // self-reference precisely isn't possible here anyway — `raw` is a
                // legacy free-text expression string, not a structured AST child.
                bool atomic = isAtomicRef(dst);
                if (atomic) emit(IRInstruction(IROpcode::LOCK_BEGIN));
                IRRef src = lowerExpr(raw);
                // Automatic retype: when a plain reassignment's literal value is a
                // DIFFERENT type than what this var was last known to hold, insert a real
                // TYPE_CAST (the same mechanism `dec x = 5`/`atomic x = 5` already use) at
                // exactly this point instead of silently keeping the var's old static type
                // — Abu's own words: "IN THE IR, WHEN TYPE CHANGES WE INSERT A TYPE CHANGE
                // ... IS WHAT HAPPENS WITHOUT THE USER HAVING TO WRITE IT." Verified real
                // bug this closes: `x = 5; x = $String$; x = 5.0` — C had been declaring
                // `x` with ONE static type picked from a whole-lifetime scan (here:
                // `double`, since a float assignment appears later), so the STRING
                // assignment spliced in as a bare, unquoted identifier; JS's compile-time
                // `_acp` vs `_acpf` print-site choice silently lost the float's `.0`
                // formatting once `x` had also been used as a string in between. Scoped to
                // literal CONST sources only (`src.kind == CONST`) — inferring the type of
                // an arbitrary expression RHS is the much larger general problem every
                // float/string/list type-inference fix elsewhere in this file already
                // grapples with; this targets exactly the case Abu specified.
                IRType castType = IRType::VOID;
                if (dst.kind == IRRef::Kind::VAR && src.kind == IRRef::Kind::CONST) {
                    IRType newType = src.value.type;
                    if (newType != IRType::VOID) {
                        IRType oldType = prog.symbols.getType(dst.id);
                        // Same ATOMIC/SHORT/MINI stickiness as the structured-AST path
                        // above (see its comment, including why ATOMIC is sticky against
                        // EVERY type, not just INT) — a plain literal reassignment must
                        // not downgrade a qualifier type back here either.
                        bool stickyKeep = (oldType == IRType::ATOMIC)
                                          || ((oldType == IRType::SHORT || oldType == IRType::MINI)
                                              && newType == IRType::INT);
                        IRType effType = stickyKeep ? oldType : newType;
                        if (oldType != IRType::VOID && oldType != effType) castType = effType;
                        setRefType(dst, effType);
                    }
                }
                if (castType != IRType::VOID) {
                    IRInstruction ci(IROpcode::TYPE_CAST);
                    ci.typedOperands = {src};
                    ci.result = dst;
                    ci.resultType = castType;
                    emit(std::move(ci));
                } else {
                    IRInstruction i(IROpcode::STORE_VAR, dst, {src});
                    emit(std::move(i));
                }
                if (atomic) emit(IRInstruction(IROpcode::LOCK_END));
            }
            break;
        }

        case NodeType::PlusEqualStmt:
            if (!n.children.empty()) emitCompoundRef(IROpcode::ADD, n.value, lowerExprNode(*n.children[0]));
            else if (!n.attrs.empty()) emitCompound(IROpcode::ADD, n.value, n.attrs[0]);
            break;
        case NodeType::MinusEqualStmt:
            if (!n.children.empty()) emitCompoundRef(IROpcode::SUB, n.value, lowerExprNode(*n.children[0]));
            else if (!n.attrs.empty()) emitCompound(IROpcode::SUB, n.value, n.attrs[0]);
            break;
        case NodeType::MultiplyEqualStmt:
        case NodeType::AtEqualStmt:
            if (!n.children.empty()) emitCompoundRef(IROpcode::MUL, n.value, lowerExprNode(*n.children[0]));
            else if (!n.attrs.empty()) emitCompound(IROpcode::MUL, n.value, n.attrs[0]);
            break;
        case NodeType::DivideEqualStmt:
            if (!n.children.empty()) emitCompoundRef(IROpcode::DIV, n.value, lowerExprNode(*n.children[0]));
            else if (!n.attrs.empty()) emitCompound(IROpcode::DIV, n.value, n.attrs[0]);
            break;
        case NodeType::XorEqualStmt:
            if (!n.children.empty()) emitCompoundRef(IROpcode::XOR, n.value, lowerExprNode(*n.children[0]));
            else if (!n.attrs.empty()) emitCompound(IROpcode::XOR, n.value, n.attrs[0]);
            break;

        // ── display / print ─────────────────────────────────────────────────
        case NodeType::DisplayStmt: {
            IRRef val;
            if (!n.children.empty()) {
                // Structured expression as child
                val = lowerExprNode(*n.children[0]);
            } else if (!n.value.empty()) {
                // Legacy string-based expression (fallback)
                val = lowerExpr(n.value);
            } else {
                val = mkConst("");
            }
            
            IRInstruction i(IROpcode::PRINT);
            i.typedOperands = {val};
            emit(std::move(i));
            break;
        }

        // ── method / function calls ─────────────────────────────────────────
        case NodeType::MethodCall: {
            // Self-referential tag call: terrain.generation() inside spawn_terrain → skip
            if (!currentCustomTag_.empty()) {
                std::string selfCall = currentCustomTag_ + ".generation";
                if (n.value == selfCall || n.value == currentCustomTag_ + ".generate") break;
            }
            // Check for display-style methods where arg is a structured child expression
            const std::string& mname = n.value;
            // Term.log / Term.write / Term.print do not exist in AC; keep only Term.display (+ styled variants).
            if (mname == "Term.log" || mname == "Term.write" || mname == "Term.print") {
                throw ACError::methodNotFound(mname);
            }
            auto endsWith = [](const std::string& s, const std::string& suf) {
                return s.size() >= suf.size() && s.substr(s.size()-suf.size()) == suf;
            };
            if (!n.children.empty() && (endsWith(mname, "display") || mname == "alert" || mname == "sure")) {
                IRRef val = lowerExprNode(*n.children[0]);
                // Term.display is core I/O — a first-class PRINT opcode, NOT a LIB_CALL.
                // (Styled variants like bold.display, and alert/sure, stay LIB_CALLs.)
                if (mname == "Term.display") {
                    IRInstruction i(IROpcode::PRINT);
                    i.typedOperands = {val};
                    emit(std::move(i));
                    break;
                }
                IRInstruction i(IROpcode::LIB_CALL);
                i.typedOperands = {mkVar(n.value), val};
                emit(std::move(i));
                break;
            }
            // `recv.write(expr)` — the parser's "display/print/log/write" no-paren-required
            // special case (see its own comment) puts ALL FOUR of these method names' argument
            // into `n.children[0]` uniformly, but this consumer only ever handled "display" (+
            // alert/sure) — "write" (a real widget method, e.g. `textbox.write(...)`; `Term.write`
            // itself is already rejected above) fell through every later branch, which only
            // reads `n.attrs` (empty here, since the parser used `.children` for this shape),
            // silently dropping the argument entirely (verified: `src_box.write(content)`
            // compiled to a bare `src_box.write()` call on every backend — content never sent).
            if (!n.children.empty() && endsWith(mname, ".write")) {
                IRRef val = lowerExprNode(*n.children[0]);
                IRInstruction i(IROpcode::LIB_CALL);
                i.typedOperands = {mkVar(n.value), val};
                emit(std::move(i));
                break;
            }
            // print_page — no argument
            if (mname == "print_page") {
                IRInstruction i(IROpcode::LIB_CALL);
                i.typedOperands = {mkVar("print_page")};
                emit(std::move(i));
                break;
            }
            if (mname == "Screen.OBJECT") break;

            // background.config("color=X") → gl:screen.set_bg_by_name("X")
            if (mname == "background.config" && !n.attrs.empty()) {
                std::string spec = n.attrs[0];
                std::string color;
                if (spec.rfind("color=", 0) == 0) color = spec.substr(6);
                else color = spec;
                IRInstruction i(IROpcode::LIB_CALL);
                i.typedOperands = {mkConst("gl:screen.set_bg_by_name"), mkConst(color)};
                emit(std::move(i));
                break;
            }

            // animate("60fps") → gl:screen.animate("60fps")
            if (mname == "animate" && !n.attrs.empty()) {
                IRInstruction i(IROpcode::LIB_CALL);
                i.typedOperands = {mkConst("gl:screen.animate"), mkConst(n.attrs[0])};
                emit(std::move(i));
                break;
            }

            // Terrain.ANIMATE inside a custom tag → gl:obj.animate(obj, "left", 300)
            if (mname == "Terrain.ANIMATE" && !currentTagObjects_.empty()) {
                IRInstruction i(IROpcode::LIB_CALL);
                i.typedOperands = {mkConst("gl:obj.animate"),
                                   mkConst(currentTagObjects_[0]),
                                   mkConst("left"),
                                   IRRef::constant(IRValue(300.0))};
                emit(std::move(i));
                break;
            }

            // GL object method calls: receiver.METHOD(args) → gl:obj.METHOD
            {
                static const std::unordered_map<std::string,std::string> glMethods = {
                    {"move_y","gl:obj.move_y"},   {"move_x","gl:obj.move_x"},
                    {"vertex","gl:obj.vertex"},   {"curveshape","gl:obj.curveshape"},
                    {"CircleFall","gl:obj.circle_fall"}, {"CircleFell","gl:obj.circle_fell"},
                    {"regen","gl:obj.regen"},     {"set_spawn","gl:obj.set_spawn"},
                    {"animate","gl:obj.animate"}, {"velocity","gl:obj.velocity"},
                    {"set_speed","gl:obj.set_speed"}, {"set_direction","gl:obj.set_direction"},
                };
                // Helper: parse compound "1/4RightDir" attr into fraction + direction operands
                auto addCircleArgs = [&](IRInstruction& i, const std::string& a) -> bool {
                    for (auto& [dirKey, dirVal] : std::vector<std::pair<std::string,std::string>>{
                            {"RightDir","right"},{"LeftDir","left"},{"UpDir","up"},{"DownDir","down"}}) {
                        auto dpos = a.find(dirKey);
                        if (dpos != std::string::npos) {
                            std::string fracStr = a.substr(0, dpos);
                            while (!fracStr.empty() && (fracStr.back()==' '||fracStr.back()=='\t')) fracStr.pop_back();
                            double fracVal = 0.25;
                            auto sl = fracStr.find('/');
                            if (sl != std::string::npos) {
                                try { fracVal = std::stod(fracStr.substr(0,sl)) / std::stod(fracStr.substr(sl+1)); } catch(...) {}
                            } else if (!fracStr.empty()) {
                                try { fracVal = std::stod(fracStr); } catch(...) {}
                            }
                            i.typedOperands.push_back(IRRef::constant(IRValue(fracVal)));
                            i.typedOperands.push_back(mkConst(dirVal));
                            return true;
                        }
                    }
                    return false;
                };
                auto dotPos = mname.find('.');
                if (dotPos != std::string::npos) {
                    std::string receiver = mname.substr(0, dotPos);
                    std::string method   = mname.substr(dotPos + 1);
                    bool isGlObj = glObjects_.count(receiver) > 0;
                    auto it = glMethods.find(method);
                    // Route if: known GL method, regardless of whether receiver is statically in glObjects_
                    // (receiver might be a function parameter holding an object name)
                    if (it != glMethods.end()) {
                        IRInstruction i(IROpcode::LIB_CALL);
                        i.typedOperands.push_back(mkConst(it->second));
                        // Known GL object → string literal; variable/param → variable ref
                        if (isGlObj)
                            i.typedOperands.push_back(mkConst(receiver));
                        else
                            i.typedOperands.push_back(mkVar(receiver));
                        // Add attrs as arguments
                        for (auto& a : n.attrs) {
                            if (a == "RightDir") i.typedOperands.push_back(mkConst("right"));
                            else if (a == "LeftDir")  i.typedOperands.push_back(mkConst("left"));
                            else if (a == "UpDir")    i.typedOperands.push_back(mkConst("up"));
                            else if (a == "DownDir")  i.typedOperands.push_back(mkConst("down"));
                            else if (!addCircleArgs(i, a)) {
                                // Normalize: trim whitespace, collapse "- N" → "-N"
                                std::string na = a;
                                while (!na.empty() && na.front() == ' ') na.erase(na.begin());
                                while (!na.empty() && na.back()  == ' ') na.pop_back();
                                if (na.size() >= 3 && na[0] == '-' && na[1] == ' ') na = "-" + na.substr(2);
                                // numeric, string literal, or bare constant
                                bool isNum = !na.empty();
                                for (char c : na) if (!std::isdigit((unsigned char)c) && c!='-' && c!='.') { isNum=false; break; }
                                if (isNum) { try { i.typedOperands.push_back(IRRef::constant(IRValue(std::stod(na)))); } catch(...) { i.typedOperands.push_back(mkConst(na)); } }
                                else if (na.size()>=2 && na.front()=='$' && na.back()=='$') i.typedOperands.push_back(mkConst(na.substr(1,na.size()-2)));
                                else i.typedOperands.push_back(mkConst(na));
                            }
                        }
                        if (it->second == "ac_gl_obj_circle_fell") i.result = mkTemp();
                        emit(std::move(i));
                        break;
                    }
                    // Receiver is a GL object but method not in map — forward with gl:obj. prefix
                    if (isGlObj && method.rfind("gl:", 0) != 0) {
                        IRInstruction i(IROpcode::LIB_CALL);
                        i.typedOperands.push_back(mkConst("gl:obj." + method));
                        i.typedOperands.push_back(mkConst(receiver));
                        for (auto& a : n.attrs) i.typedOperands.push_back(mkConst(a));
                        emit(std::move(i));
                        break;
                    }
                }
            }

            // ── string method normalization ─────────────────────────────────────
            // Map AC string method names to cross-backend equivalents.
            // \ws in args → backend-native whitespace pattern string.
            {
                size_t dot = n.value.rfind('.');
                if (dot != std::string::npos) {
                    static const std::unordered_map<std::string,std::string> strMethods = {
                        {"UPPER","upper"},{"LOWER","lower"},{"lower","lower"},{"upper","upper"},
                        {"STRIP","strip"},{"FIND","find"},{"REPLACE","replace"},
                        {"SPLIT","split"},{"JOIN","join"},{"LEN","__len__"},
                        {"STARTSWITH","startswith"},{"ENDSWITH","endswith"},
                        {"COUNT","count"},{"FORMAT","format"},
                    };
                    std::string meth = n.value.substr(dot + 1);
                    auto it = strMethods.find(meth);
                    if (it != strMethods.end())
                        const_cast<std::string&>(n.value) = n.value.substr(0, dot + 1) + it->second;
                }
                // Replace \ws token in attrs with whitespace pattern sentinel
                for (auto& a : const_cast<std::vector<std::string>&>(n.attrs)) {
                    if (a == "\\ws" || a == "\\\\ws") a = "__WS__";
                }
            }

            std::vector<IRRef> ops = {mkVar(n.value)};
            for (auto& a : mergeBracketAttrs(n.attrs)) {
                // Trim whitespace
                std::string trimmed = a;
                while (!trimmed.empty() && trimmed.back() == ' ') trimmed.pop_back();
                while (!trimmed.empty() && trimmed.front() == ' ') trimmed.erase(trimmed.begin());
                
                // Check if attr is a variable name or a constant.
                // Numeric attrs must stay numeric for ilib calls like ml.weights(0.01, w).
                bool isNum = !trimmed.empty();
                bool hasDot = false;
                bool hasDigit = false;
                for (size_t idx = 0; idx < trimmed.size(); ++idx) {
                    char c = trimmed[idx];
                    if (std::isdigit((unsigned char)c)) { hasDigit = true; continue; }
                    if (c == '.' && !hasDot) { hasDot = true; continue; }
                    if (c == '-' && idx == 0) continue;
                    isNum = false;
                    break;
                }
                isNum = isNum && hasDigit;
                if (isNum) {
                    try {
                        if (hasDot)
                            ops.push_back(IRRef::constant(IRValue(std::stod(trimmed))));
                        else
                            ops.push_back(mkConstInt(std::stoll(trimmed)));
                    } catch (...) {
                        ops.push_back(mkConst(trimmed));
                    }
                } else if (trimmed == "True" || trimmed == "true" || trimmed == "False" || trimmed == "false") {
                    // Boolean literal arg (e.g. list.append(True)). Was falling through to the
                    // identifier branch → raw "True"/"False", undefined on C/Rust. AC lists are
                    // i64, so lower to 1/0 (fits []i64 and reads truthy everywhere).
                    ops.push_back(mkConstInt((trimmed == "True" || trimmed == "true") ? 1 : 0));
                } else if (trimmed.size() >= 2 && trimmed.front() == '$' && trimmed.back() == '$'
                           && [&]{ int n=0; for (char c : trimmed) if (c=='$') n++; return n; }() == 2) {
                    // String literal — strip the `$...$` delimiters (matches lowerExpr's own
                    // string handling a few hundred lines up). This generic attrs-based LIB_CALL
                    // lowering is what a chained method call (`fn Term.display $x$ & Term.display
                    // $y$`) routes through instead of the optimized PRINT opcode path (which
                    // handles a real expression CHILD, not a raw attrs STRING) — leaving the
                    // delimiters attached here made every backend that doesn't independently
                    // re-strip them (verified: BNY) print the literal text "$chained-one$"
                    // instead of "chained-one". The dollar-count guard additionally fixes a
                    // multi-part concatenation reconstructed by the parser's method-call arg
                    // collector (`Name.method(args)`, parser.cpp) — WITHOUT it, `os.bash($a$ +
                    // path + $b$ + backend + $c$)` collapsed into one literal string containing
                    // the un-evaluated text "$+path+$..." instead of falling through to
                    // lowerExpr below to actually parse the concatenation.
                    ops.push_back(mkConst(trimmed.substr(1, trimmed.size() - 2)));
                } else if (!trimmed.empty() && std::isdigit((unsigned char)trimmed.front())
                           && [&]{ for (char c : trimmed) if (!std::isalnum((unsigned char)c) && c != '_') return false; return true; }()) {
                    // Mixed digit-alpha token like "60fps" — treat as string constant. The
                    // ORIGINAL check here was just "starts with a digit and isn't pure
                    // numeric" — far broader than "60fps"-style unit suffixes, so it ALSO
                    // swallowed any digit-led ARITHMETIC EXPRESSION (`7/2`, `3+4`, `5*x`) as a
                    // literal string constant instead of evaluating it. Verified real bug:
                    // `foo(7/2)` printed the literal text "7/2", not 3.5. Now requires the
                    // WHOLE token to be alnum/underscore (matching the isIdent check just
                    // below for the non-digit-starting case) — anything with an operator
                    // character falls through to the expression-lowering branch instead.
                    ops.push_back(mkConst(trimmed));
                } else if (!trimmed.empty() && lowerListLiteralArg(trimmed, ops)) {
                    // list literal argument — lowered to an ALLOC temp
                } else if (!trimmed.empty()) {
                    // Plain identifier (var / dotted member) → VAR ref.
                    // Anything else ("i@i", "x+1", "f(y)") is an EXPRESSION — lower it to a
                    // temp. Passing it as mkVar made text backends emit it as source (works
                    // by luck) while BNY looked up a variable literally named "i@i" → garbage.
                    bool isIdent = true;
                    for (char c : trimmed)
                        if (!std::isalnum((unsigned char)c) && c != '_' && c != '.') { isIdent = false; break; }
                    if (isIdent)
                        ops.push_back(mkVar(trimmed));
                    else
                        ops.push_back(lowerExpr(trimmed));
                } else {
                    // Empty - skip
                }
            }
            // .kick()/.kick(i) — unifies pop (no arg, drops the last element) and remove-by-
            // index (one arg, 1-based like every other AC index); .swap(i, j) — swaps two
            // elements in place. Arrays only ever exposed .append() as a real mutator, and a
            // true in-place shrink/grow needs backend-specific runtime support none of the 12
            // targets share — so both are pure front-end desugaring (rebuild-and-reassign for
            // kick, load/store/load/store for swap) built entirely from LOAD_INDEX/STORE_INDEX/
            // ALLOC/.append, the same primitives [elem]@n list-repeat and the ternary already
            // ride, so it works on every backend for free with zero codegen changes.
            {
                auto dotPosM = mname.rfind('.');
                if (dotPosM != std::string::npos && dotPosM > 0) {
                    std::string recvM = mname.substr(0, dotPosM);
                    std::string methM = mname.substr(dotPosM + 1);
                    if (methM == "swap" && ops.size() == 3) {
                        IRRef arrV = mkVar(recvM);
                        IRRef i1 = adjustIndex(ops[1]);
                        IRRef j1 = adjustIndex(ops[2]);
                        IRRef tmp = mkTemp();
                        emit(IRInstruction(IROpcode::LOAD_INDEX, tmp, {arrV, i1}));
                        IRRef vj = mkTemp();
                        emit(IRInstruction(IROpcode::LOAD_INDEX, vj, {arrV, j1}));
                        { IRInstruction st(IROpcode::STORE_INDEX); st.typedOperands = {arrV, i1, vj}; emit(std::move(st)); }
                        { IRInstruction st(IROpcode::STORE_INDEX); st.typedOperands = {arrV, j1, tmp}; emit(std::move(st)); }
                        break;
                    }
                    if (methM == "kick" && ops.size() <= 2) {
                        thread_local int kickC = 0;
                        std::string tag = std::to_string(kickC++);
                        IRRef arrV = mkVar(recvM);
                        IRRef lenT = mkTemp();
                        emit(IRInstruction(IROpcode::CALL, lenT, {mkVar("ac_length"), arrV}));
                        IRRef target = ops.size() == 2 ? ops[1] : lenT;
                        std::string newName = "_ac_kick_new_" + tag;
                        std::string idxName = "_ac_kick_i_" + tag;
                        IRRef newV = mkVar(newName), idxV = mkVar(idxName);
                        emit(IRInstruction(IROpcode::ALLOC, newV, {mkConst("list"), mkConst("")}));
                        emit(IRInstruction(IROpcode::STORE_VAR, idxV, {mkConstInt(1)}));
                        auto emitBody = [&]() {
                            IRRef keep = mkTemp();
                            emit(IRInstruction(IROpcode::NEQ, keep, {idxV, target}));
                            if (prog.useHighLevelIR) {
                                IRInstruction ib(IROpcode::IF_BEGIN); ib.typedOperands = {keep}; emit(std::move(ib));
                                IRRef elem = mkTemp();
                                emit(IRInstruction(IROpcode::LOAD_INDEX, elem, {arrV, adjustIndex(idxV)}));
                                { IRInstruction ap(IROpcode::LIB_CALL); ap.typedOperands = {mkConst(newName + ".append"), elem}; emit(std::move(ap)); }
                                emit(IRInstruction(IROpcode::IF_END));
                            } else {
                                IRRef skipL = mkLabel();
                                emitJF(keep, skipL);
                                IRRef elem = mkTemp();
                                emit(IRInstruction(IROpcode::LOAD_INDEX, elem, {arrV, adjustIndex(idxV)}));
                                { IRInstruction ap(IROpcode::LIB_CALL); ap.typedOperands = {mkConst(newName + ".append"), elem}; emit(std::move(ap)); }
                                emitLabel(skipL);
                            }
                        };
                        if (prog.useHighLevelIR) {
                            IRRef brk = mkConst("__break__");
                            loopEnd.push(brk); loopStart.push(mkConst("__continue__"));
                            emit(IRInstruction(IROpcode::WHILE_BEGIN));
                            IRRef c = mkTemp();
                            emit(IRInstruction(IROpcode::LTE, c, {idxV, lenT}));
                            { IRInstruction jf(IROpcode::JUMP_IF_FALSE); jf.typedOperands = {c, brk}; emit(std::move(jf)); }
                            emitBody();
                            IRRef inc = mkTemp();
                            emit(IRInstruction(IROpcode::ADD, inc, {idxV, mkConstInt(1)}));
                            emit(IRInstruction(IROpcode::STORE_VAR, idxV, {inc}));
                            emit(IRInstruction(IROpcode::WHILE_END));
                            loopEnd.pop(); loopStart.pop();
                        } else {
                            IRRef startL = mkLabel(), endL = mkLabel();
                            emitLabel(startL);
                            IRRef c = mkTemp();
                            emit(IRInstruction(IROpcode::LTE, c, {idxV, lenT}));
                            emitJF(c, endL);
                            emitBody();
                            IRRef inc = mkTemp();
                            emit(IRInstruction(IROpcode::ADD, inc, {idxV, mkConstInt(1)}));
                            emit(IRInstruction(IROpcode::STORE_VAR, idxV, {inc}));
                            emitJump(startL);
                            emitLabel(endL);
                        }
                        emit(IRInstruction(IROpcode::STORE_VAR, arrV, {newV}));
                        break;
                    }
                }
            }
            // Sugar: `widget.add($a$, $b$, $c$)` — every widget "add" method (dropdown/listbox/
            // table/generic) only ever took ONE item per call; passing extra args used to just
            // load them into unused argument registers and silently drop them (verified on BNY:
            // ac_widgets_dropdown_add(w, item) ignores anything past its 2nd param). No existing
            // "add" use in the language takes 2+ positional args (checked examples/), so it's
            // safe to desugar N args into N sequential single-arg LIB_CALLs here — works on every
            // backend for free since each already handles the single-arg form correctly.
            if (ops.size() > 2 && n.value.size() >= 4 && n.value.compare(n.value.size()-4, 4, ".add") == 0) {
                for (size_t k = 1; k < ops.size(); k++) {
                    IRInstruction i(IROpcode::LIB_CALL);
                    i.typedOperands = {ops[0], ops[k]};
                    emit(std::move(i));
                }
                break;
            }
            IRInstruction i(IROpcode::LIB_CALL);
            i.typedOperands = ops;
            emit(std::move(i));
            break;
        }

        case NodeType::FunctionCall: {
            // Remap GL helpers to high-level gl:* names (lowering pass handles the rest)
            static const std::unordered_map<std::string,std::string> glFuncMap = {
                {"is_obj",                  "gl:obj.is"},
                {"is_draw",                 "gl:obj.is_draw"},
                {"gl_hitbox_many_overlap",  "gl:hitbox.many_overlap"},
                {"gl_hitbox_coords_overlap","gl:hitbox.overlap"},
                {"gl_frame_begin",          "gl:frame.begin"},
                {"gl_frame_render",         "gl:frame.render"},
                {"gl_frame_end",            "gl:frame.end"},
                {"gl_frame_update",         "gl:frame.update"},
                {"gl_obj_animate",          "gl:obj.animate"},
                {"gl_obj_regen",            "gl:obj.regen"},
                {"gl_screen_init_from_ac",  "gl:screen.init"},
            };
            std::string fname = n.value;
            {
                auto fit = glFuncMap.find(fname);
                if (fit != glFuncMap.end()) fname = fit->second;
                else {
                    auto ait = selectiveImportAliases_.find(fname);
                    if (ait != selectiveImportAliases_.end()) fname = ait->second;
                }
            }
            std::vector<IRRef> ops = {mkVar(fname)};
            for (auto& a : mergeBracketAttrs(n.attrs)) {
                // Check if attr is a variable name or a constant
                // If it's a number, make it a const int
                bool isInt = !a.empty();
                for (size_t ci = 0; ci < a.size(); ++ci) if (!std::isdigit((unsigned char)a[ci]) && !(a[ci]=='-'&&ci==0)) { isInt = false; break; }
                if (isInt) {
                    try {
                        ops.push_back(mkConstInt(std::stoll(a)));
                    } catch (...) {
                        ops.push_back(mkConst(a));
                    }
                } else if (a.size() >= 2 && a.front() == '$' && a.back() == '$'
                           && [&]{ int n=0; for (char c : a) if (c=='$') n++; return n; }() == 2) {
                    // A genuine single string literal has EXACTLY the two boundary `$`s. The
                    // parser reconstructs a call argument's source text by concatenating token
                    // values (see parser.cpp's positional-argument loop, which re-wraps every
                    // STRING token as "$" + value + "$") — a multi-part concatenation like
                    // `$a$ + x + $b$` reconstructs to a string that ALSO happens to start and
                    // end with `$`, with MORE `$`s embedded in the middle. The old first/last-
                    // char-only check couldn't tell the two apart and treated the whole
                    // concatenation as one literal string, dollar signs and all (verified:
                    // `os.bash($ac $ + path + $ --target $ + backend + $ --no-cache...$)` —
                    // 3+ concatenated parts inside a call's parens produced a single string
                    // containing the literal text "$+path+$" instead of actually concatenating;
                    // exactly 2 parts happened to dodge this by never re-closing with `$`, which
                    // is why it looked backend/widget-specific until isolated). Anything with
                    // extra embedded `$`s falls through to lowerExpr below instead, which
                    // correctly re-tokenizes and parses the whole expression.
                    ops.push_back(mkConst(a));
                } else if (lowerListLiteralArg(a, ops)) {
                    // list literal argument — lowered to an ALLOC temp
                } else {
                    bool isIdent = true;
                    for (char c : a)
                        if (!std::isalnum((unsigned char)c) && c != '_' && c != '.') { isIdent = false; break; }
                    if (isIdent)
                        ops.push_back(mkVar(a));       // variable name
                    else
                        ops.push_back(lowerExpr(a));   // expression argument
                }
            }
            IRInstruction i(IROpcode::CALL);
            i.typedOperands = ops;
            emit(std::move(i));
            break;
        }

        case NodeType::DestructureAssignStmt: {
            // a, b[, c...] = expr — n.attrs = ordered target names, n.children[0] = RHS.
            if (n.children.empty() || !n.children[0]) break;
            const ASTNode& rhs = *n.children[0];

            if (rhs.type == NodeType::TupleLiteral) {
                // Same-statement destructure: a, b = (x, y) or a, b = x, y — the headline
                // zero-cost case (no bundle, no class, nothing to allocate). Only aliased RHS
                // elements (an RHS var that's ALSO one of the LHS targets — e.g. a swap,
                // a, b = b, a) need to be snapshotted into a temp BEFORE any target is
                // overwritten; every other element stores straight into its target with no
                // intermediate var at all. A temp for every element regardless (the original,
                // simpler version) was ALSO correct, but left an unconditionally-declared,
                // never-read scratch var behind whenever constant-folding later resolved the
                // store to a literal (`x, y = (10, 20)`) — harmless on most backends, but a
                // real compile error on Go, which hard-rejects an unused local (`t_0 declared
                // and not used`).
                std::set<std::string> targetNames(n.attrs.begin(), n.attrs.end());
                bool needsSnapshot = false;
                for (auto& c : rhs.children)
                    if (c && c->type == NodeType::Identifier && targetNames.count(c->value)) { needsSnapshot = true; break; }
                // lowerTupleElements applies the SAME homogeneity/coercion rule (inferred
                // widening, colloid, or `; any`) this fast path would otherwise skip entirely —
                // the type rule applies to a tuple regardless of whether it's ever allocated.
                std::vector<IRRef> elems;
                std::vector<IRType> slotTypes;
                lowerTupleElements(rhs, elems, slotTypes);
                std::vector<IRRef> vals;
                for (size_t ei = 0; ei < elems.size(); ei++) {
                    IRRef er = elems[ei];
                    if (!needsSnapshot) { vals.push_back(er); continue; }
                    // A named `_ac_`-prefixed var, not mkTemp() — the dead-CONST-store
                    // elimination pass (ir.cpp's own "Dead-const-store elimination" section)
                    // only prunes STORE_VAR targets of Kind::VAR; runDCE's TEMP-cleanup never
                    // applies here either, since it unconditionally keeps every STORE_VAR
                    // (side-effecting by default) regardless of what kind its target is.
                    // A genuine mkTemp() snapshot was therefore NEVER reclaimed once a later
                    // constant-fold pass replaced its only consumer with a literal — verified
                    // real bug: Go hard-errors ("t_0 declared and not used") on the swap case
                    // (a, b = b, a) once a, b's values are both compile-time-known constants.
                    IRRef t = mkVar("_ac_swap_" + std::to_string(tupleTempCounter_++));
                    IRInstruction mv(IROpcode::STORE_VAR, t, {er});
                    mv.resultType = slotTypes[ei];
                    setRefType(t, slotTypes[ei]);
                    emit(std::move(mv));
                    vals.push_back(t);
                }
                size_t count = std::min(n.attrs.size(), vals.size());
                for (size_t i = 0; i < count; i++) {
                    IRRef dst = mkVar(n.attrs[i]);
                    IRInstruction st(IROpcode::STORE_VAR, dst, {vals[i]});
                    st.resultType = slotTypes[i];
                    emit(std::move(st));
                }
                break;
            }

            // A bare var previously bound via `t = (x, y)` and proven scalarizable — read
            // straight off its flat shadow vars, no bundle involved at all.
            if (rhs.type == NodeType::Identifier) {
                std::string scalKey = currentFunc_ + "::" + rhs.value;
                auto scalIt = scalarizableTupleVars_.find(scalKey);
                if (scalIt != scalarizableTupleVars_.end()) {
                    for (size_t i = 0; i < n.attrs.size(); i++) {
                        IRRef dst = mkVar(n.attrs[i]);
                        IRRef fld = mkVar(tupleScalarShadowName(rhs.value, i));
                        IRInstruction st(IROpcode::STORE_VAR, dst, {fld});
                        emit(std::move(st));
                    }
                    break;
                }
            }

            // Otherwise: RHS is a single expression that resolves to an escaping tuple
            // instance — a direct call (`a, b = f()`, f always tuple-returning), or a bare var
            // previously bound via `t = (x, y)` that DIDN'T qualify for scalarization above
            // (always the fallback-bundle path — see TupleLiteral's own lowerExprNode case).
            // Field-read `_0.._N-1` off it: the exact "construct in one place, field-access
            // across a boundary" shape Phase 0's bundle-in-free-function fix already made work
            // end-to-end on every backend.
            std::string srcName;
            IRRef srcVar;
            if (rhs.type == NodeType::CallExpr || rhs.type == NodeType::FunctionCall) {
                // Mirrors the verified `__funcall__` AssignStmt pattern (mkVar(fname) as
                // ops[0], CALL's result a NAMED var, not a temp) — a CALL result routed
                // through a plain TEMP first isn't the shape Phase 0's instance tracking was
                // verified against, so this is built directly rather than via lowerExprNode's
                // generic CallExpr case (which returns a bare mkTemp()).
                std::string fname = rhs.value;
                auto ait = selectiveImportAliases_.find(fname);
                if (ait != selectiveImportAliases_.end()) fname = ait->second;
                srcName = "_ac_tup_dst_" + std::to_string(tupleTempCounter_++);
                srcVar = mkVar(srcName);
                std::vector<IRRef> ops = {mkVar(fname)};
                for (auto& a : rhs.children) if (a) ops.push_back(lowerExprNode(*a));
                IRInstruction call(IROpcode::CALL, srcVar, ops);
                emit(std::move(call));
            } else if (rhs.type == NodeType::Identifier) {
                srcName = rhs.value;
                srcVar = mkVar(srcName);
            } else {
                IRRef er = lowerExprNode(rhs);
                if (er.kind == IRRef::Kind::VAR) {
                    srcName = prog.symbols.getName(er.id);
                    srcVar = er;
                } else {
                    srcName = "_ac_tup_dst_" + std::to_string(tupleTempCounter_++);
                    srcVar = mkVar(srcName);
                    IRInstruction mv(IROpcode::STORE_VAR, srcVar, {er});
                    emit(std::move(mv));
                }
            }
            (void)srcVar;
            for (size_t i = 0; i < n.attrs.size(); i++) {
                IRRef dst = mkVar(n.attrs[i]);
                IRRef fld = mkVar(srcName + ".f" + std::to_string(i));
                IRInstruction st(IROpcode::STORE_VAR, dst, {fld});
                emit(std::move(st));
            }
            break;
        }

        case NodeType::PropAssign: {
            // name.prop = value  OR  name.prop /= value (compound, attrs[0] = "/=")
            IRRef dst = mkVar(n.value);
            // Compound assignment: attr[0] is the op ("/=", "*=", etc.), rhs in children[0]
            if (!n.attrs.empty() && !n.children.empty() && n.attrs[0] == "@=") {
                // `<glObj>.speed@=X` — the runtime's OWN documented convention for this exact
                // syntax (gl_c.h: "speed *= mult (ball.speed@=-1)") is ac_gl_obj_speed_mult,
                // which existed already but was completely orphaned — neither glMethods nor
                // glMethodsExpr referenced it, and the parser didn't even accept AT_EQUAL here
                // until now (see the parser-side comment on this same construct). Falls through
                // to the generic __compound_assign__ path below (unchanged, existing behavior)
                // for anything that isn't `<glObj>.speed` — e.g. `@=` on a plain variable is
                // handled entirely differently, via AtEqualStmt, never reaching PropAssign at all.
                auto dotPos = n.value.find('.');
                if (dotPos != std::string::npos) {
                    std::string receiver = n.value.substr(0, dotPos);
                    std::string prop = n.value.substr(dotPos + 1);
                    if (glObjects_.count(receiver) && prop == "speed") {
                        IRRef rhs = lowerExprNode(*n.children[0]);
                        IRInstruction i(IROpcode::LIB_CALL);
                        i.typedOperands = {mkConst("gl:obj.speed_mult"), mkConst(receiver), rhs};
                        emit(std::move(i));
                        break;
                    }
                }
            }
            // Compound assignment: attr[0] is the op ("/=", "*=", etc.), rhs in children[0]
            if (!n.attrs.empty() && !n.children.empty() &&
                (n.attrs[0] == "/=" || n.attrs[0] == "*=" || n.attrs[0] == "+=" || n.attrs[0] == "-=" ||
                 n.attrs[0] == "@=")) {
                // This used to unconditionally skip (silently drop the whole statement, no
                // error) any compound assign whose receiver wasn't a known GL object — meant
                // to guard a narrow "function-attribute style" case (e.g. jump.vertex /= 2,
                // a closure-attribute pseudo-property with no real backing storage), but the
                // condition was far broader than that: it skipped compound assignment on
                // EVERY ordinary bundle field in the language, GL or not. Verified real bug:
                // `h.val += 5` (h a plain bundle instance) silently vanished — no IR emitted
                // for it at all, not even a wrong value, just gone — since dst below (a plain
                // VAR ref to the dotted name, same self.field/namedVar.field convention as
                // every other bundle field access) already reaches the SAME
                // "__compound_assign__" LIB_CALL mechanism a GL object's compound assign uses,
                // there was never a real reason to special-case GL objects here at all.
                IRRef rhs = lowerExprNode(*n.children[0]);
                // "@=" isn't a real operator in ANY target language outside this one (Python's
                // `@=` means matrix-multiply, not this) — only reachable here for a fallthrough
                // `@=` the gl:obj.speed_mult special-case above didn't claim (a non-GL-object
                // receiver, or a GL property other than "speed"). `@` is AC's general multiply
                // operator everywhere else, so treat it as MUL below like every other `@`.
                //
                // emitCompoundRef — the SAME established helper a plain (non-dotted) `x += y`
                // already goes through (PlusEqualStmt et al., just below this case) — instead
                // of the single opaque LIB_CALL("__compound_assign__", ...) this used to emit.
                // That LIB_CALL was handled by ONE generic text-substitution block in the
                // shared driver (`lhs + " " + op + " " + rhs + ";"`) that assumed every backend
                // could accept raw "h.val += 5;"-shaped text. Fine for every text-emitting
                // backend, but BNY (a wholly separate hand-rolled compiler) never implemented
                // this LIB_CALL name AT ALL — unreachable before the GL-object restriction
                // above was removed, so the gap was never exposed — and ASM (shared driver, but
                // real x86-64, not text) took the SAME generic branch and got literal C-syntax
                // spliced into a .asm file (verified: "h.val += 5;" — a hard nasm parse error).
                // A hand-rolled read+ADD/SUB/MUL/DIV+STORE_VAR replacement (tried first) turned
                // out to have its OWN gap — a fresh mkTemp() result has no established type-
                // inference tie back to the field's already-declared type, so C++/Java/Rust/Go/V
                // variously mis-declared or rejected it (verified: DIV producing a genuinely
                // float-typed temp against an int field — "incompatible types", "cannot use
                // float64 as int64"). emitCompoundRef is the proven-working mechanism plain
                // compound-assign already relies on; reusing it directly needs no new backend
                // code either, since dotted-name field reads/writes already work everywhere.
                emitCompoundRef(n.attrs[0] == "@=" ? IROpcode::MUL
                              : n.attrs[0] == "+=" ? IROpcode::ADD
                              : n.attrs[0] == "-=" ? IROpcode::SUB
                              : n.attrs[0] == "/=" ? IROpcode::DIV
                              : IROpcode::MUL, n.value, rhs);
            } else {
                // DEG/RAD prefix sugar (`DEG x.prop=45` / `RAD x.prop=0.785`) — set by the
                // parser's statement-prefix handling (see its comment). DEG is a no-op (AC's
                // angle-taking functions, e.g. gl's set_direction, already take degrees); RAD
                // wraps the raw value text in a real math.rad2deg(...) call before it's lowered
                // — reuses the exact same textual call-lowering path every other `name(args)`
                // expression already goes through just below, not a duplicated formula.
                std::string valText = n.attrs.empty() ? "" : n.attrs[0];
                // The parser builds this text token-by-token, each with a TRAILING space
                // (`val += advance().value + " ";`, parser.cpp's "assignment: Name.prop =
                // value" branch) — so a single-token RHS like `p.x = 5` arrives here as "5 "
                // (trailing space), not "5". A genuine, real, PRE-EXISTING bug found while
                // debugging an unrelated BNY issue, not BNY-specific: `lowerExpr("5 ")`
                // doesn't recognize the untrimmed text as a numeric literal at all, so it
                // falls through to treating it as an unknown VARIABLE NAME (literally named
                // "5 ") — a var that's never legitimately assigned anywhere, so it reads back
                // as whatever uninitialized memory/register happens to be there. Every TEXT-
                // EMITTING backend (Python/JS/C/...) masked this by accident: `ref()`'s
                // fallback for an unresolvable symbol just prints the symbol's own NAME as
                // literal text, and "5" is *also* valid literal syntax in every one of those
                // target languages, so the generated source came out looking right by pure
                // coincidence. BNY (real machine code, not text) has no such accidental
                // safety net — it allocates a genuine, real, uninitialized stack slot for a
                // symbol named "5 " and reads garbage/zero from it, which is what actually
                // surfaced this (verified: `p = Point(); p.x = 5; Term.display p.x` printed 0
                // instead of 5 — bare, no free function boundary involved at all).
                { size_t a = valText.find_first_not_of(' '), b = valText.find_last_not_of(' ');
                  valText = (a == std::string::npos) ? std::string() : valText.substr(a, b - a + 1); }
                if (n.angleUnit == 2 && !valText.empty())
                    valText = "math.rad2deg(" + valText + ")";
                IRRef src = valText.empty() ? mkConst("") : lowerExpr(valText);
                // A GL object's `direction`/`speed` PROPERTY ASSIGNMENT (`ball.direction=45`)
                // must reach the real `ac_gl_obj_set_direction`/`set_speed` — without this it
                // fell through to a plain STORE_VAR on a flattened "ball.direction" name (the
                // same convention bundle self.field assignment uses), which silently updates
                // nothing in the gl runtime at all (verified: examples/pong.ac's
                // `DEG ball.direction=45`). `X.set_direction(Y)` (method-CALL syntax) already
                // worked via glMethods (ir.cpp's statement dispatch table) — this just gives the
                // equivalent property-ASSIGN syntax the same real destination.
                static const std::unordered_map<std::string,std::string> glPropSetters = {
                    {"direction", "gl:obj.set_direction"}, {"speed", "gl:obj.set_speed"},
                };
                auto dotPos = n.value.find('.');
                bool routedToGl = false;
                if (dotPos != std::string::npos) {
                    std::string receiver = n.value.substr(0, dotPos);
                    std::string prop = n.value.substr(dotPos + 1);
                    if (glObjects_.count(receiver)) {
                        auto pit = glPropSetters.find(prop);
                        if (pit != glPropSetters.end()) {
                            IRInstruction i(IROpcode::LIB_CALL);
                            i.typedOperands = {mkConst(pit->second), mkConst(receiver), src};
                            emit(std::move(i));
                            routedToGl = true;
                        }
                    }
                }
                if (!routedToGl) {
                    IRInstruction i(IROpcode::STORE_VAR, dst, {src});
                    emit(std::move(i));
                }
            }
            break;
        }

        case NodeType::ConfigCall: {
            // Emit a single gl:obj.config call; the GL lowering pass expands it into
            // ac_gl_obj_config_item / ac_gl_obj_pos_from_spec / ac_gl_obj_color_by_name calls.
            IRInstruction cfg(IROpcode::LIB_CALL);
            cfg.typedOperands.push_back(mkConst("gl:obj.config"));
            cfg.typedOperands.push_back(mkConst(n.value));
            for (auto& a : n.attrs)
                cfg.typedOperands.push_back(mkConst(a));
            emit(std::move(cfg));
            break;
        }

        case NodeType::ObjDecl: {
            std::string full = n.value;
            std::string objName = (full.size() > 4 && full.substr(0,4) == "Obj.") ? full.substr(4) : full;

            if (objName == "Screen") {
                // Parse WxH from "resize:WxH" attr
                int w = 1720, h = 1080;
                for (auto& a : n.attrs) {
                    if (a.rfind("resize:", 0) == 0) {
                        std::string dims = a.substr(7);
                        auto xp = dims.find('x');
                        if (xp != std::string::npos) {
                            try { w = std::stoi(dims.substr(0, xp)); h = std::stoi(dims.substr(xp+1)); } catch(...) {}
                        }
                    }
                }
                // gl:screen.init — lowering pass maps to ac_gl_screen_init
                IRInstruction i(IROpcode::LIB_CALL);
                i.typedOperands.push_back(mkConst("gl:screen.init"));
                i.typedOperands.push_back(mkConstInt(w));
                i.typedOperands.push_back(mkConstInt(h));
                i.typedOperands.push_back(mkConst("Geodeo"));
                emit(std::move(i));
                break;
            }

            glObjects_.insert(objName);
            if (!currentCustomTag_.empty())
                currentTagObjects_.push_back(objName);

            // gl:obj.create — lowering pass maps to ac_gl_obj_create
            {
                IRInstruction ic(IROpcode::LIB_CALL);
                ic.typedOperands = {mkConst("gl:obj.create"), mkConst(objName)};
                emit(std::move(ic));
            }
            // Declare variable as string "Name" so it can be passed to functions
            {
                IRRef dst = mkVar(objName);
                IRInstruction id(IROpcode::ALLOC, dst, {mkConst("string"), mkConst(objName)});
                emit(std::move(id));
            }
            break;
        }

        case NodeType::MethodChain: {
            for (auto& c : n.children) gen(*c);
            break;
        }

        // ── control flow ────────────────────────────────────────────────────
        case NodeType::IfStmt: {
            if (n.value == "OTHER") {
                // OTHER (else) block - just generate the body
                // The IF_ELSE opcode was already emitted by the parent IF
                if (!n.children.empty()) gen(*n.children[0]);
                break;
            }

            // First child is condition expression, second is body
            if (n.children.empty() || n.children[0]->type == NodeType::Block) break;
            IRRef condRef = lowerExprNode(*n.children[0]);
            size_t bodyIndex = 1;

            if (prog.useHighLevelIR) {
                // High-level IR: emit IF_BEGIN with condition
                IRInstruction ifBegin(IROpcode::IF_BEGIN);
                ifBegin.typedOperands = {condRef};
                emit(std::move(ifBegin));
                
                if (bodyIndex < n.children.size()) gen(*n.children[bodyIndex]);
                
                if (n.children.size() > bodyIndex + 1) {
                    emit(IRInstruction(IROpcode::IF_ELSE));
                    genElseChain(n.children, bodyIndex + 1);
                }
                
                // Emit IF_END
                IRInstruction ifEnd(IROpcode::IF_END);
                emit(std::move(ifEnd));
            } else {
                // Low-level IR: use jumps and labels
                IRRef elseL   = mkLabel();
                IRRef endL    = mkLabel();

                emitJF(condRef, elseL);
                if (bodyIndex < n.children.size()) gen(*n.children[bodyIndex]);

                bool hasElse = n.children.size() > bodyIndex + 1;
                if (hasElse) emitJump(endL);
                emitLabel(elseL);

                genElseChainLowLevel(n.children, bodyIndex + 1, endL);
                if (hasElse) emitLabel(endL);
            }
            break;
        }

        case NodeType::CondStmt: {
            // cond expr
            //   is v1: block
            //   is v2: block
            //   OTHER: block
            // Lowers to a nested IF/ELSE chain with expr evaluated once.
            if (n.children.empty()) break;

            // Evaluate the scrutinee once into a temp var (skip the var entirely for a
            // CONSTANT scrutinee — comparisons fold and Go rejects the unused variable).
            IRRef scrut = lowerExprNode(*n.children[0]);
            IRRef tmpVar = scrut;
            if (scrut.kind != IRRef::Kind::CONST) {
                thread_local int condCounter = 0;
                std::string tmpName = "ac_cond_" + std::to_string(condCounter++); // no leading _ (V renames those)
                tmpVar = mkVar(tmpName);
                emit(IRInstruction(IROpcode::STORE_VAR, tmpVar, {scrut}));
            }

            struct Case { const ASTNode* expr; const ASTNode* block; };
            std::vector<Case> cases;
            const ASTNode* otherBlock = nullptr;

            for (size_t i = 1; i < n.children.size(); i++) {
                const auto& c = *n.children[i];
                if (c.type == NodeType::CondCase) {
                    const ASTNode* ex = (c.children.size() > 0) ? c.children[0].get() : nullptr;
                    const ASTNode* bl = (c.children.size() > 1) ? c.children[1].get() : nullptr;
                    if (ex && bl) cases.push_back({ex, bl});
                } else if (c.type == NodeType::CondOther) {
                    otherBlock = (!c.children.empty()) ? c.children[0].get() : nullptr;
                }
            }
            if (cases.empty()) break;

            auto genNested = [&](auto&& self, size_t idx) -> void {
                // Build cond = (tmpVar == caseExpr)
                IRRef rhs = lowerExprNode(*cases[idx].expr);
                IRRef condRef = mkTemp();
                IRInstruction cmp(IROpcode::EQ, condRef, {tmpVar, rhs});
                emit(std::move(cmp));

                IRInstruction ifBegin(IROpcode::IF_BEGIN);
                ifBegin.typedOperands = {condRef};
                emit(std::move(ifBegin));
                gen(*cases[idx].block);

                bool hasMore = (idx + 1 < cases.size()) || (otherBlock != nullptr);
                if (hasMore) {
                    emit(IRInstruction(IROpcode::IF_ELSE));
                    if (idx + 1 < cases.size()) self(self, idx + 1);
                    else if (otherBlock) gen(*otherBlock);
                }
                emit(IRInstruction(IROpcode::IF_END));
            };

            // genNested emits IF_BEGIN/IF_ELSE/IF_END markers directly — BNY/ASM handle those
            // natively (structured-IF stack). Do NOT flip useHighLevelIR around it: that forced
            // the case BODIES high-level too, so a loop inside a cond case lowered as
            // for_begin/for_end on a label-based backend → body silently mis-ran (#26).
            genNested(genNested, 0);
            break;
        }

        case NodeType::ElseIfStmt: {
            // First child is condition expression, second is body
            if (n.children.empty() || n.children[0]->type == NodeType::Block) break;
            IRRef condRef = lowerExprNode(*n.children[0]);
            size_t bodyIndex = 1;

            IRRef skipL   = mkLabel();

            emitJF(condRef, skipL);
            if (bodyIndex < n.children.size()) gen(*n.children[bodyIndex]);
            emitLabel(skipL);
            break;
        }

        case NodeType::WhilstLoop: {
            if (prog.useHighLevelIR) {
                // High-level backends: emit "while True:" with break-at-top-on-condition.
                // Push sentinels so BreakStmt/ContinueStmt emit the correct keywords.
                IRRef breakSentinel = mkConst("__break__");
                IRRef contSentinel  = mkConst("__continue__");
                loopEnd.push(breakSentinel);
                loopStart.push(contSentinel);

                emit(IRInstruction(IROpcode::WHILE_BEGIN));

                // Condition is computed INSIDE the loop body, re-evaluated each iteration
                if (n.children.empty() || n.children[0]->type == NodeType::Block) {
                    emit(IRInstruction(IROpcode::WHILE_END));
                    loopEnd.pop(); loopStart.pop();
                    break;
                }
                IRRef condRef = lowerExprNode(*n.children[0]);
                size_t bodyIndex = 1;

                // if not cond: break
                {
                    IRInstruction jfb(IROpcode::JUMP_IF_FALSE);
                    jfb.typedOperands = {condRef, breakSentinel};
                    emit(std::move(jfb));
                }

                if (bodyIndex < n.children.size()) gen(*n.children[bodyIndex]);
                for (size_t i = bodyIndex + 1; i < n.children.size(); i++) gen(*n.children[i]);

                emit(IRInstruction(IROpcode::WHILE_END));

                loopEnd.pop();
                loopStart.pop();
            } else {
                // Low-level backends: labels + jumps, condition re-evaluated each iteration
                IRRef startL = mkLabel();
                IRRef endL   = mkLabel();

                loopStart.push(startL);
                loopEnd.push(endL);

                emitLabel(startL);  // label FIRST so condition is re-evaluated each loop

                if (n.children.empty() || n.children[0]->type == NodeType::Block) {
                    loopStart.pop(); loopEnd.pop();
                    break;
                }
                IRRef condRef = lowerExprNode(*n.children[0]);
                size_t bodyIndex = 1;
                emitJF(condRef, endL);

                if (bodyIndex < n.children.size()) gen(*n.children[bodyIndex]);
                for (size_t i = bodyIndex + 1; i < n.children.size(); i++) gen(*n.children[i]);

                emitJump(startL);
                emitLabel(endL);

                loopStart.pop();
                loopEnd.pop();
            }
            break;
        }

        case NodeType::ForLoop: {
            std::string iterVar = n.value;

            if (n.children.empty() || n.children[0]->type == NodeType::Block) break;
            size_t bodyIndex = 1;
            ASTNode& collNode = *n.children[0];

            // Low-level (BNY/ASM): range/sequence/iota/stream → compact counted WHILST loop.
            // iota iterates like range; stream like sequence. Optional step (3rd arg).
            // xrange/xiota are the 1-indexed cousins — same shape as range, just a=1 and
            // b=bound+1 instead of a=0 and b=bound.
            bool isRangeLike = collNode.type == NodeType::RangeExpr || collNode.type == NodeType::IotaExpr;
            bool isXLike     = collNode.type == NodeType::XRangeExpr || collNode.type == NodeType::XIotaExpr;
            bool isSeqLike   = collNode.type == NodeType::SequenceExpr || collNode.type == NodeType::StreamExpr;
            if (!prog.useHighLevelIR && (isRangeLike || isXLike || isSeqLike)) {

                IRRef startL = mkLabel();
                IRRef contL  = mkLabel();  // continue → the INCREMENT, not the top (#2: skipping
                IRRef endL   = mkLabel();  // the bottom increment made `continue` loop forever)
                IRRef iter   = mkVar(iterVar);

                loopStart.push(contL);
                loopEnd.push(endL);

                IRRef aRef, bRef, stepRef = mkConstInt(1);
                if (isRangeLike) {
                    aRef = mkConstInt(0);
                    bRef = !collNode.children.empty()
                        ? lowerExprNode(*collNode.children[0])
                        : lowerExpr(collNode.value);
                } else if (isXLike) {
                    aRef = mkConstInt(1);
                    IRRef bound = !collNode.children.empty()
                        ? lowerExprNode(*collNode.children[0])
                        : lowerExpr(collNode.value);
                    bRef = mkTemp();
                    emit(IRInstruction(IROpcode::ADD, bRef, {bound, mkConstInt(1)}));
                } else {
                    if (collNode.children.size() >= 2) {
                        aRef = lowerExprNode(*collNode.children[0]);
                        bRef = lowerExprNode(*collNode.children[1]);
                        if (collNode.children.size() >= 3) stepRef = lowerExprNode(*collNode.children[2]);
                    } else {
                        std::string val = collNode.value;
                        size_t comma = topLevelComma(val);
                        aRef = lowerExpr(comma != std::string::npos ? val.substr(0, comma) : "0");
                        bRef = lowerExpr(comma != std::string::npos ? val.substr(comma + 1) : val);
                    }
                }

                emit(IRInstruction(IROpcode::STORE_VAR, iter, {aRef}));
                emitLabel(startL);

                IRRef cmpT = mkTemp();
                // A negative constant step counts DOWN → terminate on iter > b, not iter < b
                // (stream(10,0,-1) with a fixed `iter < 0` test ran zero iterations).
                bool negStep = (stepRef.kind == IRRef::Kind::CONST && stepRef.value.type == IRType::INT
                                && std::get<int64_t>(stepRef.value.data) < 0)
                             || (collNode.children.size() >= 3 && collNode.children[2]
                                 && collNode.children[2]->type == NodeType::UnaryExpr && collNode.children[2]->value == "-");
                emit(IRInstruction(negStep ? IROpcode::GT : IROpcode::LT, cmpT, {iter, bRef}));
                emitJF(cmpT, endL);

                if (bodyIndex < n.children.size()) gen(*n.children[bodyIndex]);

                emitLabel(contL);
                IRRef incT = mkTemp();
                emit(IRInstruction(IROpcode::ADD, incT, {iter, stepRef}));
                emit(IRInstruction(IROpcode::STORE_VAR, iter, {incT}));

                emitJump(startL);
                emitLabel(endL);
                loopStart.pop();
                loopEnd.pop();
                break;
            }

            // High-level backends: sequence/stream WITH AN EXPLICIT STEP can't ride the
            // 2-arg ALLOC machinery (typed backends store {a,b} pairs and emit i++ loops,
            // silently dropping the step). Lower as a while-loop instead — every backend
            // handles WHILE + explicit increment, and loop scoping treats WHILE == FOR.
            if (prog.useHighLevelIR && isSeqLike && collNode.children.size() >= 3) {
                IRRef iter    = mkVar(iterVar);
                IRRef aRef    = lowerExprNode(*collNode.children[0]);
                IRRef bRef    = lowerExprNode(*collNode.children[1]);
                IRRef stepRef = lowerExprNode(*collNode.children[2]);
                IRRef breakSentinel = mkConst("__break__");
                IRRef contSentinel  = mkConst("__continue__");
                loopEnd.push(breakSentinel);
                loopStart.push(contSentinel);
                // #2 (high-level): increment at the TOP (iter starts at a-step) so a native
                // `continue` inside the body still advances the iterator — the old bottom
                // increment was skipped by continue → infinite loop.
                IRRef startT = mkTemp();
                emit(IRInstruction(IROpcode::SUB, startT, {aRef, stepRef}));
                emit(IRInstruction(IROpcode::STORE_VAR, iter, {startT}));
                emit(IRInstruction(IROpcode::WHILE_BEGIN));
                IRRef incT = mkTemp();
                emit(IRInstruction(IROpcode::ADD, incT, {iter, stepRef}));
                emit(IRInstruction(IROpcode::STORE_VAR, iter, {incT}));
                IRRef cmpT = mkTemp();
                bool negStep2 = (stepRef.kind == IRRef::Kind::CONST && stepRef.value.type == IRType::INT
                                 && std::get<int64_t>(stepRef.value.data) < 0)
                              || (collNode.children.size() >= 3 && collNode.children[2]
                                  && collNode.children[2]->type == NodeType::UnaryExpr && collNode.children[2]->value == "-");
                emit(IRInstruction(negStep2 ? IROpcode::GT : IROpcode::LT, cmpT, {iter, bRef}));
                {
                    IRInstruction jfb(IROpcode::JUMP_IF_FALSE);
                    jfb.typedOperands = {cmpT, breakSentinel};
                    emit(std::move(jfb));
                }
                if (bodyIndex < n.children.size()) gen(*n.children[bodyIndex]);
                for (size_t i = bodyIndex + 1; i < n.children.size(); i++) gen(*n.children[i]);
                emit(IRInstruction(IROpcode::WHILE_END));
                loopEnd.pop();
                loopStart.pop();
                break;
            }

            // `FOR x in gen(args):` where gen is a known generator function, on a family-C
            // backend (Java/C/CPP/LIB — no native generator/channel construct, unlike family
            // A's real `yield`/`for` or family B's channel+range) — expand into the WHILE-shape
            // this exact function already uses for sequence-with-explicit-step just above (same
            // idiom, not a new one): GEN_CREATE once, then GEN_NEXT/GEN_DONE each iteration.
            // Family A/B backends fall through to the unchanged generic path below instead —
            // their generator-ness is handled entirely by the callee's OWN signature/codegen,
            // never visible at this call site.
            {
                static const std::set<std::string> genFamilyC = {"Java", "C", "CPP", "C++", "LIB"};
                bool isGenCall = (collNode.type == NodeType::CallExpr || collNode.type == NodeType::FunctionCall);
                std::string calleeName = isGenCall ? collNode.value : "";
                // `FOR x in g:` where `g = twovals()` ran earlier — reuse g's EXISTING handle
                // directly (no GEN_CREATE: that would spin up a whole SECOND, independent
                // generator, and the correct behavior — matching every other family — is that
                // iterating an already-partially-or-fully-consumed handle a second time picks
                // up where it left off, e.g. yielding nothing further once exhausted).
                bool isGenVar = (collNode.type == NodeType::Identifier)
                                && generatorHandleVars_.count(collNode.value);
                if ((isGenCall && generatorFuncNames_.count(calleeName)
                        && genFamilyC.count(prog.backend))
                    || (isGenVar && genFamilyC.count(prog.backend))) {
                    IRRef genHandle;
                    if (isGenVar) {
                        genHandle = mkVar(collNode.value);
                    } else {
                        genHandle = mkTemp();
                        std::vector<IRRef> ops = {mkVar(calleeName)};
                        for (auto& c : collNode.children) if (c) ops.push_back(lowerExprNode(*c));
                        IRInstruction gc(IROpcode::GEN_CREATE, genHandle, ops);
                        emit(std::move(gc));
                    }
                    IRRef breakSentinel = mkConst("__break__");
                    IRRef contSentinel  = mkConst("__continue__");
                    loopEnd.push(breakSentinel);
                    loopStart.push(contSentinel);
                    emit(IRInstruction(IROpcode::WHILE_BEGIN));
                    IRRef tmpVal = mkTemp();
                    emit(IRInstruction(IROpcode::GEN_NEXT, tmpVal, {genHandle}));
                    IRRef tmpDone = mkTemp();
                    emit(IRInstruction(IROpcode::GEN_DONE, tmpDone, {genHandle}));
                    // JUMP_IF_TRUE has no real implementation on several backends (Java among
                    // them — the base BackendStrategy default is a silent no-op) — negate and
                    // use JUMP_IF_FALSE instead, the exact same proven idiom the sequence-
                    // with-explicit-step WHILE loop just above already relies on.
                    IRRef tmpNotDone = mkTemp();
                    emit(IRInstruction(IROpcode::NOT, tmpNotDone, {tmpDone}));
                    {
                        IRInstruction jf(IROpcode::JUMP_IF_FALSE);
                        jf.typedOperands = {tmpNotDone, breakSentinel};
                        emit(std::move(jf));
                    }
                    emit(IRInstruction(IROpcode::STORE_VAR, mkVar(iterVar), {tmpVal}));
                    if (bodyIndex < n.children.size()) gen(*n.children[bodyIndex]);
                    for (size_t i = bodyIndex + 1; i < n.children.size(); i++) gen(*n.children[i]);
                    emit(IRInstruction(IROpcode::WHILE_END));
                    loopEnd.pop();
                    loopStart.pop();
                    break;
                }
            }

            // `FOR x in gen(args):` on the low-level (!useHighLevelIR) family-C backends — BNY and
            // ASM, neither of which has structured-loop IR opcodes at all (see prog.useHighLevelIR's
            // own comment). Same GEN_CREATE/GEN_NEXT/GEN_DONE opcodes as the high-level family-C
            // branch above, just expanded into the label/jump loop shape this function's own
            // low-level range/seq branch (just above, `isRangeLike`/`isSeqLike`) already
            // establishes for these backends, instead of WHILE_BEGIN/WHILE_END.
            if (!prog.useHighLevelIR && (prog.backend == "BNY" || prog.backend == "ASM")) {
                bool isGenCall = (collNode.type == NodeType::CallExpr || collNode.type == NodeType::FunctionCall);
                std::string calleeName = isGenCall ? collNode.value : "";
                bool isGenVar = (collNode.type == NodeType::Identifier)
                                && generatorHandleVars_.count(collNode.value);
                if ((isGenCall && generatorFuncNames_.count(calleeName)) || isGenVar) {
                    IRRef genHandle;
                    if (isGenVar) {
                        genHandle = mkVar(collNode.value);
                    } else {
                        genHandle = mkTemp();
                        std::vector<IRRef> ops = {mkVar(calleeName)};
                        for (auto& c : collNode.children) if (c) ops.push_back(lowerExprNode(*c));
                        IRInstruction gc(IROpcode::GEN_CREATE, genHandle, ops);
                        emit(std::move(gc));
                    }
                    IRRef startL = mkLabel();
                    IRRef endL   = mkLabel();
                    loopStart.push(startL);   // no separate continue-target: nothing to increment
                    loopEnd.push(endL);
                    emitLabel(startL);
                    IRRef tmpVal = mkTemp();
                    emit(IRInstruction(IROpcode::GEN_NEXT, tmpVal, {genHandle}));
                    IRRef tmpDone = mkTemp();
                    emit(IRInstruction(IROpcode::GEN_DONE, tmpDone, {genHandle}));
                    IRRef tmpNotDone = mkTemp();
                    emit(IRInstruction(IROpcode::NOT, tmpNotDone, {tmpDone}));
                    emitJF(tmpNotDone, endL);
                    emit(IRInstruction(IROpcode::STORE_VAR, mkVar(iterVar), {tmpVal}));
                    if (bodyIndex < n.children.size()) gen(*n.children[bodyIndex]);
                    for (size_t i = bodyIndex + 1; i < n.children.size(); i++) gen(*n.children[i]);
                    emitJump(startL);
                    emitLabel(endL);
                    loopStart.pop();
                    loopEnd.pop();
                    break;
                }
            }

            // For iteration, iota/stream behave as range/sequence (LAZY, one-value-per-iteration
            // generation — NOT materializing anything upfront) — this is the entire point of
            // having iota/stream as a separate keyword from range/sequence in a FOR context:
            // `FOR item in iota 6` is a counter that gets asked for the next value each pass,
            // never a precomputed [0,1,2,3,4,5]. Reverted a wrong "fix" from earlier this session
            // that misread this as an inconsistency and made it iterate a materialized string's
            // characters instead — that's backwards: it made a lazy generator even MORE eager
            // (fully building a string) rather than keeping it lazy. Only the VALUE form (`Term.
            // display iota 3` → "012", outside a FOR loop) is a real string — see IotaExpr/
            // StreamExpr's `lowerExpr` case above; that contract was never in question.
            IRRef collRef;
            if (collNode.type == NodeType::IotaExpr) {
                IRRef bound = !collNode.children.empty() ? lowerExprNode(*collNode.children[0]) : mkConstInt(0);
                collRef = mkTemp();
                emit(IRInstruction(IROpcode::ALLOC, collRef, {mkConst("range"), bound}));
            } else if (collNode.type == NodeType::StreamExpr) {
                IRRef a = collNode.children.size() >= 1 ? lowerExprNode(*collNode.children[0]) : mkConstInt(0);
                IRRef b = collNode.children.size() >= 2 ? lowerExprNode(*collNode.children[1]) : mkConstInt(0);
                std::vector<IRRef> sops = {mkConst("sequence"), a, b};
                if (collNode.children.size() >= 3) sops.push_back(lowerExprNode(*collNode.children[2]));
                collRef = mkTemp();
                emit(IRInstruction(IROpcode::ALLOC, collRef, sops));
            } else {
                collRef = lowerExprNode(collNode);
            }

            if (prog.useHighLevelIR) {
                // High-level backends: FOR_BEGIN {iterVar, collection} … FOR_END
                IRRef breakSentinel = mkConst("__break__");
                IRRef contSentinel  = mkConst("__continue__");
                loopEnd.push(breakSentinel);
                loopStart.push(contSentinel);

                IRInstruction fb(IROpcode::FOR_BEGIN);
                fb.typedOperands = {mkVar(iterVar), collRef};
                emit(std::move(fb));

                if (bodyIndex < n.children.size()) gen(*n.children[bodyIndex]);

                emit(IRInstruction(IROpcode::FOR_END));

                loopEnd.pop();
                loopStart.pop();
            } else {
                // Low-level (BNY): index-based iteration over an array.
                //   iter = collRef; cur = 0; len = LOAD_INDEX iter,"__len__"
                //   start: if !(cur < len) goto end
                //          iterVar = LOAD_INDEX iter, cur ; body ; cur = cur+1 ; goto start
                // Avoids the stateful "__next__" iterator (which has no machine-code home).
                // iterator ptr and length are loop-carried temps; the linear-scan allocator now
                // extends intervals across loop back-edges (computeLoopRegions), so they survive.
                IRRef startL = mkLabel();
                IRRef endL   = mkLabel();
                std::string sfx = std::to_string(tc);
                IRRef iterT  = mkTemp();
                IRRef lenT   = mkTemp();
                IRRef curV   = mkVar("__foridx_" + sfx); // named loop cursor
                IRRef itemT  = mkTemp();

                IRRef contL = mkLabel();   // continue target = cursor increment (#2)
                loopStart.push(contL);
                loopEnd.push(endL);

                emit(IRInstruction(IROpcode::LOAD_VAR, iterT, {collRef}));
                emit(IRInstruction(IROpcode::LOAD_INDEX, lenT, {iterT, mkConst("__len__")}));
                emit(IRInstruction(IROpcode::STORE_VAR, curV, {mkConstInt(0)}));

                emitLabel(startL);
                IRRef cmpT = mkTemp();
                emit(IRInstruction(IROpcode::LT, cmpT, {curV, lenT}));
                emitJF(cmpT, endL);

                emit(IRInstruction(IROpcode::LOAD_INDEX, itemT, {iterT, curV}));
                emit(IRInstruction(IROpcode::STORE_VAR, mkVar(iterVar), {itemT}));

                if (bodyIndex < n.children.size()) gen(*n.children[bodyIndex]);

                emitLabel(contL);
                IRRef incT = mkTemp();
                emit(IRInstruction(IROpcode::ADD, incT, {curV, mkConstInt(1)}));
                emit(IRInstruction(IROpcode::STORE_VAR, curV, {incT}));

                emitJump(startL);
                emitLabel(endL);

                loopStart.pop();
                loopEnd.pop();
            }
            break;
        }

        case NodeType::ReturnStmt: {
            IRRef val;
            if (!n.children.empty()) {
                // Structured expression as child
                val = lowerExprNode(*n.children[0]);
            } else if (!n.value.empty()) {
                // Legacy string-based expression (fallback)
                val = lowerExpr(n.value);
            } else {
                // No return value (void return)
                val = mkConst("");
            }
            
            IRInstruction i(IROpcode::RETURN);
            i.typedOperands = {val};
            emit(std::move(i));
            break;
        }

        case NodeType::YieldStmt: {
            IRRef val = !n.children.empty() ? lowerExprNode(*n.children[0]) : mkConst("");
            IRInstruction i(IROpcode::YIELD);
            i.typedOperands = {val};
            emit(std::move(i));
            break;
        }

        case NodeType::BreakStmt:
        case NodeType::SkipStmt:
            if (!loopEnd.empty()) emitJump(loopEnd.top());
            break;

        case NodeType::ContinueStmt:
            if (!loopStart.empty()) emitJump(loopStart.top());
            break;

        case NodeType::KillStmt: {
            emit(IRInstruction(IROpcode::HALT));
            break;
        }

        case NodeType::StopStmt: {
            emit(IRInstruction(IROpcode::SOFT_HALT));
            break;
        }

        case NodeType::RestartStmt: {
            prog.hasRestart = true;
            emit(IRInstruction(IROpcode::RESTART_PROGRAM));
            break;
        }

        case NodeType::HaltStmt: {
            // /halt n — sleep for n seconds; /halt math.inf is already StopStmt in parser
            IRInstruction sleep(IROpcode::SLEEP);
            sleep.typedOperands.push_back(mkConst(n.value));
            emit(std::move(sleep));
            break;
        }

        case NodeType::EndStmt: {
            // Break out of the enclosing loop regardless of its condition.
            // Falls back to HALT if used outside any loop.
            if (!loopEnd.empty())
                emitJump(loopEnd.top());
            else
                emit(IRInstruction(IROpcode::HALT));
            break;
        }

        case NodeType::PassStmt: {
            IRInstruction i(IROpcode::NOP);
            emit(std::move(i));
            break;
        }

        case NodeType::AliasDecl: {
            // alias x = y — register bidirectional live binding
            IRInstruction i(IROpcode::ALIAS_DECL);
            i.typedOperands = {mkVar(n.value), mkVar(n.attrs.empty() ? n.value : n.attrs[0])};
            emit(std::move(i));
            // Initialise x = y so they start equal
            IRInstruction init(IROpcode::STORE_VAR, mkVar(n.value), {mkVar(n.attrs.empty() ? n.value : n.attrs[0])});
            emit(std::move(init));
            break;
        }

        case NodeType::FreeDecl: {
            // free x, y, z — emit one FREE_DECL per named variable.
            // `free var = expr` (Form 2) is parsed as a FreeDecl carrying the target name
            // in attrs and the assignment statement as a child: emit FREE_DECL, then lower
            // the assignment so the store lands in the now-free-scoped variable.
            // node value "bound" marks the bound (function-scoped) variant: exempt from
            // loop save/restore like free, but NEVER promoted to a program global.
            for (const auto& varName : n.attrs) {
                IRInstruction i(IROpcode::FREE_DECL);
                i.typedOperands = {mkVar(varName)};
                if (n.value == "bound") i.attrs.push_back("bound");
                emit(std::move(i));
            }
            for (const auto& child : n.children)
                if (child) gen(*child);
            break;
        }

        case NodeType::ConstDecl: {
            // const x = expr — immutable binding; track name so reassignment is an error
            IRRef varRef = mkVar(n.value);
            IRRef src = n.children.empty() ? mkConstInt(0) : lowerExprNode(*n.children[0]);
            prog.constVars.insert(n.value);
            IRInstruction i(IROpcode::CONST_DECL, varRef, {src});
            emit(std::move(i));
            break;
        }

        case NodeType::CompfoldStmt: {
            // compfold x = expr — user-requested compile-time constant folding
            IRRef varRef = mkVar(n.value);
            IRRef src = n.children.empty() ? mkConstInt(0) : lowerExprNode(*n.children[0]);

            // Check if the result is a constant
            if (src.kind == IRRef::Kind::CONST) {
                // Successfully folded!
                IRInstruction i(IROpcode::STORE_VAR, varRef, {src});
                i.attrs.push_back("compfold_success");
                emit(std::move(i));
            } else {
                // Can't fold — emit Toxic warning and fall back to runtime
                std::cerr << Toxic::impossibleOperations() << "\n";
                std::cerr << "    compfold " << n.value << " = <expression>\n";
                std::cerr << "    ^^^^^^^^\n";
                std::cerr << "Falling back to runtime evaluation\n";

                // Still emit the code, just at runtime
                IRInstruction i(IROpcode::STORE_VAR, varRef, {src});
                i.attrs.push_back("compfold_fallback");
                emit(std::move(i));
            }
            break;
        }

        case NodeType::CopyStmt: {
            // cp x = y — deep copy of rhs into lhs
            IRRef dst = mkVar(n.value);
            IRRef src = n.children.empty() ? mkConstInt(0) : lowerExprNode(*n.children[0]);
            IRInstruction i(IROpcode::STORE_VAR, dst, {src});
            i.attrs.push_back("copy");   // marks this as an explicit deep copy
            emit(std::move(i));
            break;
        }

        case NodeType::TypeCoerceStmt: {
            // dec/int/string/bool x [= expr] — coerce x to target type
            const std::string& varName = n.value;
            // Determine target IRType from attrs
            IRType targetType = IRType::VOID;
            if (!n.attrs.empty()) {
                if      (n.attrs[0] == "DEC")    targetType = IRType::FLOAT;
                else if (n.attrs[0] == "INT")    targetType = IRType::INT;
                else if (n.attrs[0] == "SHORT")  targetType = IRType::SHORT;
                else if (n.attrs[0] == "MINI")   targetType = IRType::MINI;
                else if (n.attrs[0] == "ATOMIC") targetType = IRType::ATOMIC;
                else if (n.attrs[0] == "STRING") targetType = IRType::STRING;
                else if (n.attrs[0] == "BOOL")   targetType = IRType::BOOL;
                else if (n.attrs[0] == "LONGINT") targetType = IRType::STRING;
                else if (n.attrs[0] == "GOODDEC") targetType = IRType::STRING;
            }

            if (!n.attrs.empty() && n.attrs[0] == "LONGINT") {
                std::string decimal;
                if (!n.children.empty() && longIntFromExpr(*n.children[0], decimal)) {
                    IRRef varRef = mkVar(varName);
                    // Fit check: if within int64, store as INT so all backends work natively
                    bool fits64 = false;
                    int64_t intVal = 0;
                    if (!decimal.empty() && decimal.size() <= 20) {
                        try {
                            size_t pos = 0;
                            long long v = std::stoll(decimal, &pos);
                            if (pos == decimal.size()) {
                                intVal = (int64_t)v;
                                fits64 = true;
                            }
                        } catch (...) {}
                    }
                    if (fits64) {
                        setRefType(varRef, IRType::INT);
                        IRInstruction i(IROpcode::STORE_VAR, varRef, {mkConstInt(intVal)});
                        i.resultType = IRType::INT;
                        emit(std::move(i));
                    } else {
                        setRefType(varRef, IRType::STRING);
                        IRInstruction i(IROpcode::STORE_VAR, varRef, {mkConst(decimal)});
                        i.resultType = IRType::STRING;
                        emit(std::move(i));
                    }
                } else {
                    IRRef varRef = mkVar(varName);
                    setRefType(varRef, IRType::STRING);
                    IRInstruction i(IROpcode::STORE_VAR, varRef, {mkConst(std::string("inf"))});
                    i.resultType = IRType::STRING;
                    emit(std::move(i));
                }
                break;
            }

            if (!n.attrs.empty() && n.attrs[0] == "GOODDEC") {
                std::string unscaled;
                int64_t scale = 0;
                if (!n.children.empty() &&
                    goodDecPairFromExpr(*n.children[0], unscaled, scale)) {
                    IRRef varRef = mkVar(varName);
                    setRefType(varRef, IRType::TUPLE);
                    std::string content = unscaled + "," + std::to_string(scale);
                    IRInstruction i(IROpcode::ALLOC, varRef, {mkConst("tuple"), mkConst(content)});
                    i.resultType = IRType::TUPLE;
                    emit(std::move(i));
                } else {
                    IRRef varRef = mkVar(varName);
                    setRefType(varRef, IRType::TUPLE);
                    IRInstruction i(IROpcode::ALLOC, varRef, {mkConst("tuple"), mkConst(std::string("0,0"))});
                    i.resultType = IRType::TUPLE;
                    emit(std::move(i));
                }
                break;
            }

            // Source: either provided expr, or the variable itself (coerce in-place)
            IRRef src = n.children.empty() ? mkVar(varName) : lowerExprNode(*n.children[0]);
            // Fixed-width overflow check: a `short`/`mini` initialised with a constant that doesn't
            // fit is a mistake — reject it uniformly on EVERY backend (C/C++/Java silently wrapped;
            // Rust/Go/V errored at their own compiler → cross-backend divergence). Runtime values
            // still truncate to the width; only compile-time constants are range-checked here.
            if ((targetType == IRType::SHORT || targetType == IRType::MINI)
                && src.kind == IRRef::Kind::CONST && src.value.type == IRType::INT) {
                int64_t v = std::get<int64_t>(src.value.data);
                int bits = (targetType == IRType::SHORT) ? 32 : 16;
                int64_t lo = -(int64_t(1) << (bits - 1));
                int64_t hi =  (int64_t(1) << (bits - 1)) - 1;
                if (v < lo || v > hi)
                    throw ACError::type("value " + std::to_string(v) + " doesn't fit in `"
                        + std::string(targetType == IRType::SHORT ? "short" : "mini")
                        + "` (" + std::to_string(bits) + "-bit: " + std::to_string(lo)
                        + " to " + std::to_string(hi) + ")");
            }
            IRRef varRef = mkVar(varName);
            setRefType(varRef, targetType);
            IRInstruction i(IROpcode::TYPE_CAST);
            i.typedOperands = {src};
            i.result        = varRef;
            i.resultType    = targetType;
            emit(std::move(i));
            break;
        }

        case NodeType::DestroyStmt: {
            if (!n.value.empty()) emit(IRInstruction(IROpcode::FREE, IRRef(), {mkVar(n.value)}));
            for (const auto& extra : n.attrs)     // destroy a, b, c → free each
                emit(IRInstruction(IROpcode::FREE, IRRef(), {mkVar(extra)}));
            break;
        }

        // ── index expressions ───────────────────────────────────────────────
        case NodeType::IndexExpr: {
            if (n.attrs.size() >= 2) {
                // store: arr[idx] = val  (AC 1-based → 0-based)
                IRRef arr = mkVar(n.value);
                IRRef rawIdx = lowerExpr(n.attrs[0]);
                IRRef idx = adjustIndex(rawIdx);
                IRRef val = lowerExpr(n.attrs[1]);
                IRInstruction i(IROpcode::STORE_INDEX);
                i.typedOperands = {arr, idx, val};
                emit(std::move(i));
            } else if (n.attrs.size() == 1) {
                // load: tmp = arr[idx]  (AC 1-based → 0-based)
                IRRef arr = mkVar(n.value);
                IRRef rawIdx = lowerExpr(n.attrs[0]);
                IRRef idx = adjustIndex(rawIdx);
                IRRef dst = mkTemp();
                IRInstruction i(IROpcode::LOAD_INDEX, dst, {arr, idx});
                emit(std::move(i));
            }
            break;
        }

        // ── literals ────────────────────────────────────────────────────────
        case NodeType::ListLiteral: {
            IRRef dst = mkTemp();
            // If children are present (e.g. from `return x, y`), lower each and join names
            if (!n.children.empty()) {
                std::string content;
                for (size_t ci = 0; ci < n.children.size(); ci++) {
                    if (ci > 0) content += ", ";
                    IRRef er = lowerExprNode(*n.children[ci]);
                    if (er.kind == IRRef::Kind::VAR && er.id >= 0)
                        content += prog.symbols.getName(er.id);
                    else if (er.kind == IRRef::Kind::CONST) {
                        const auto& cv = er.value;
                        if (cv.type == IRType::INT)   content += std::to_string(std::get<int64_t>(cv.data));
                        else if (cv.type == IRType::FLOAT) content += std::to_string(std::get<double>(cv.data));
                        else if (cv.type == IRType::STRING) content += "\"" + std::get<std::string>(cv.data) + "\"";
                        else content += "0";
                    }
                    else
                        content += "_";
                }
                IRInstruction i(IROpcode::ALLOC, dst, {mkConst("list"), mkConst(content)});
                emit(std::move(i));
            } else {
                IRInstruction i(IROpcode::ALLOC, dst, {mkConst("list"), mkConst(n.value)});
                emit(std::move(i));
            }
            break;
        }
        case NodeType::TupleLiteral: {
            IRRef dst = mkTemp();
            IRInstruction i(IROpcode::ALLOC, dst, {mkConst("tuple"), mkConst(n.value)});
            emit(std::move(i));
            break;
        }
        case NodeType::DictLiteral: {
            IRRef dst = mkTemp();
            IRInstruction i(IROpcode::ALLOC, dst, {mkConst("dict"), mkConst(n.value)});
            emit(std::move(i));
            break;
        }
        case NodeType::RangeExpr: {
            IRRef dst = mkTemp();
            IRInstruction i(IROpcode::ALLOC, dst, {mkConst("range"), mkConst(n.value)});
            emit(std::move(i));
            break;
        }
        case NodeType::SequenceExpr: {
            IRRef dst = mkTemp();
            IRInstruction i(IROpcode::ALLOC, dst, {mkConst("sequence"), mkConst(n.value)});
            emit(std::move(i));
            break;
        }
        case NodeType::BinaryExpr: {
            lowerExprNode(n);  // evaluate via structured path; result discarded (expression-as-statement)
            break;
        }

        // ── events ──────────────────────────────────────────────────────────
        case NodeType::EventListener:
            for (auto& c : n.children) gen(*c);
            break;

        case NodeType::KeyBinding: {
            // Generate a named callback function __keycb_<key> for the binding body
            std::string key = n.value;
            // Sanitize key name for use as identifier
            std::string safeName = key;
            for (char& c : safeName) if (!std::isalnum((unsigned char)c)) c = '_';
            std::string cbName = "__keycb_" + safeName;

            // `WHILST value is <samekey>` nested directly inside `on value is <samekey>` is
            // this system's spelling for "run this every frame while the key is held" (vs the
            // default "run once when the key transitions to pressed"). `value`/`is`/the key
            // token all parse as an ordinary BinaryExpr("is", Identifier("value"),
            // Identifier(key)) — there's no dedicated grammar for it, it's recognized purely by
            // shape here. When matched, the WHILST's OWN body becomes the callback (the
            // redundant "value is X" condition is discarded — polling via key_pressed every
            // frame already IS the "while held" semantics, see the <StartHere> injection below).
            const NodeList* bodyChildren = &n.children;
            bool continuous = false;
            if (n.children.size() == 1 && n.children[0]->type == NodeType::WhilstLoop) {
                const ASTNode& w = *n.children[0];
                if (!w.children.empty() && w.children[0]->type == NodeType::BinaryExpr &&
                    w.children[0]->value == "is" && w.children[0]->children.size() == 2 &&
                    w.children[0]->children[0]->type == NodeType::Identifier &&
                    w.children[0]->children[0]->value == "value" &&
                    w.children[0]->children[1]->type == NodeType::Identifier &&
                    w.children[0]->children[1]->value == key) {
                    continuous = true;
                    bodyChildren = &w.children;   // children[0] is the condition, skipped below
                }
            }

            if (!bodyChildren->empty()) {
                // Lower children as a function body
                int funcId = prog.symbols.intern(cbName, IRType::FUNCTION);
                IRFunction fn(cbName);
                fn.returnType = IRType::VOID;
                prog.functions.push_back(std::move(fn));
                cur = &prog.functions.back();
                IRInstruction entry(IROpcode::FUNC_BEGIN);
                entry.typedOperands = {IRRef::func(funcId)};
                emit(std::move(entry));
                size_t startIdx = continuous ? 1 : 0;   // skip the WHILST's condition child
                for (size_t ci = startIdx; ci < bodyChildren->size(); ci++) gen(*(*bodyChildren)[ci]);
                IRInstruction ret(IROpcode::RETURN); ret.typedOperands = {}; emit(std::move(ret));
                IRInstruction end(IROpcode::FUNC_END); end.typedOperands = {IRRef::func(funcId)}; emit(std::move(end));
                cur = nullptr;
            }

            // Emit EVENT_BIND: {key_string, callback_name} — kept for explicit `input <key>`
            // statements, which still look this up via EVENT_TRIGGER/_ac_trigger unchanged.
            IRInstruction i(IROpcode::EVENT_BIND);
            i.typedOperands = {mkConst(key), mkVar(cbName)};
            emit(std::move(i));
            // Additionally record it so <StartHere>'s auto-generated per-frame loop can poll
            // real SDL key state and call the callback directly every frame — see its comment.
            polledKeyBindings_.push_back({key, cbName, continuous});
            break;
        }

        case NodeType::ExportStmt:
            break; // compile-time visibility marker — nothing to lower

        case NodeType::InputStmt: {
            IRInstruction i(IROpcode::EVENT_TRIGGER);
            i.typedOperands = {mkConst(n.value)};
            emit(std::move(i));
            break;
        }

        case NodeType::BindStmt: {
            // bind <key> to <function> — same EVENT_BIND opcode `configure event-listener`'s
            // KeyBinding case uses, but referencing an EXISTING user function directly instead
            // of synthesizing a wrapper callback (there's no inline block body here to wrap).
            IRInstruction i(IROpcode::EVENT_BIND);
            std::string func = n.attrs.empty() ? "" : n.attrs[0];
            i.typedOperands = {mkConst(n.value), mkVar(func)};
            emit(std::move(i));
            break;
        }

        // ── library / misc ──────────────────────────────────────────────────
        case NodeType::UseStmt:
        case NodeType::UseLibStmt: {
            // track imported lib name (strip "ilib:", "elib:", etc.)
            std::string effValue = n.value;
            std::vector<std::string> effAttrs = n.attrs;
            {
                std::string libType = effValue, libName = effValue;
                auto colon = effValue.find(':');
                if (colon != std::string::npos) { libType = effValue.substr(0, colon); libName = effValue.substr(colon + 1); }

                // web-server is a sub-ilib of web, never a standalone top-level import —
                // `from ilib web use web-server` (rewritten to this same ilib:web-server
                // form just below) is the ONLY valid spelling. A direct `use ilib
                // web-server` / `from ilib web-server use ...` used to also silently work
                // (an earlier session treated the two forms as interchangeable aliases) —
                // Abu's explicit correction: only the `from ilib web use` spelling should be
                // accepted, since it's the one that actually reads as "web-server is a
                // sub-thing of web."
                if (libType == "ilib" && libName == "web-server") {
                    throw ACError::semantic(
                        "'web-server' is a sub-library of 'web', not a standalone ilib — use "
                        "'from ilib web use web-server' instead of 'use ilib web-server'.");
                }

                // `from ilib web use web-server` — web-server is a sub-ilib of web, not
                // a restricted symbol list of web's own functions. Rewrite to a plain
                // import of web-server (and ONLY web-server — `use ilib web` on its own
                // stays client-only, importing one never pulls in the other).
                if (libType == "ilib" && libName == "web") {
                    auto it = std::find(effAttrs.begin(), effAttrs.end(), "web-server");
                    if (it != effAttrs.end()) {
                        effValue = "ilib:web-server";
                        effAttrs.clear();
                    }
                }

                std::string trackedName = libName;
                if (effValue != n.value) {
                    auto c2 = effValue.find(':');
                    trackedName = (c2 != std::string::npos) ? effValue.substr(c2 + 1) : effValue;
                }
                prog.importedLibs.insert(trackedName);

                // Selective import (`from ilib math use sin, cos`): register each requested
                // symbol as an alias for its qualified ilib call — see
                // selectiveImportAliases_'s own comment for why this is done here instead of
                // per-backend. Only real ilib namespace calls have a qualified dotted form to
                // alias to (elib/clib/flib symbols aren't namespaced the same way).
                if (libType == "ilib")
                    for (auto& sym : effAttrs)
                        selectiveImportAliases_[sym] = libName + "." + sym;
            }
            IRInstruction i(IROpcode::LIB_CALL);
            // operands: "import", "ilib:math"[, "sin,cos,sqrt" if selective]
            i.typedOperands = {mkConst("import"), mkConst(effValue)};
            if (!effAttrs.empty()) {
                // Build comma-separated symbol list from attrs
                std::string symbols;
                for (size_t k = 0; k < effAttrs.size(); k++) {
                    if (k) symbols += ',';
                    symbols += effAttrs[k];
                }
                i.typedOperands.push_back(mkConst(symbols));
            }
            emit(std::move(i));
            break;
        }

        case NodeType::RaiseStmt: {
            // raise ERR or raise ERR($msg$) → Preposterous: + abort
            IRInstruction i(IROpcode::LIB_CALL);
            std::string msg = n.value.empty() ? "Fatality occurred" : n.value;
            i.typedOperands = {mkConst("raise"), mkConst(msg)};
            emit(std::move(i));
            break;
        }

        case NodeType::RaiseClauseStmt: {
            // raise Clause($msg$) — emit "Clause: msg" to stderr
            // Special cases: "hint" → Suggestion:, "toxic" → Toxic:
            // All others (including Hint, Err, Toxic, Praise...) → "Clause: msg"
            IRInstruction i(IROpcode::RAISE_CLAUSE);
            std::string clauseName = n.value;
            std::string msg = n.attrs.empty() ? "" : n.attrs[0];
            i.typedOperands = {mkConst(clauseName), mkConst(msg)};
            emit(std::move(i));
            break;
        }

        case NodeType::TryCatchStmt: {
            // children[0] = tryBody
            // children[1..] = CatchClause(typeName, attrs[0]=exVar, children[0]=Block) and optional AfterClause(children[0]=Block)
            emit(IRInstruction(IROpcode::TRY_BEGIN));

            if (!n.children.empty())
                gen(*n.children[0]); // try body

            bool sawCatch = false;
            for (size_t ci = 1; ci < n.children.size(); ci++) {
                const auto& child = *n.children[ci];
                if (child.type == NodeType::CatchClause) {
                    sawCatch = true;
                    std::string typeName = child.value; // may be empty for catch-all
                    std::string exVar = (!child.attrs.empty() && !child.attrs[0].empty()) ? child.attrs[0] : "_exc";
                    IRInstruction cb(IROpcode::CATCH_BEGIN);
                    cb.typedOperands = {mkConst(exVar), mkConst(typeName)};
                    emit(std::move(cb));
                    if (!child.children.empty())
                        gen(*child.children[0]); // catch body
                } else if (child.type == NodeType::AfterClause) {
                    emit(IRInstruction(IROpcode::AFTER_BEGIN));
                    if (!child.children.empty())
                        gen(*child.children[0]);
                } else {
                    // Unknown node inside TryCatchStmt: ignore
                }
            }

            if (!sawCatch) {
                throw ACError::tryMissingCatch();
            }

            emit(IRInstruction(IROpcode::TRY_END));
            break;
        }

        case NodeType::ForeignBlock: {
            if (prog.backend == "BNY") {
                throw ACError::fluencyInCPU();
            }
            if (!g_allow_foreign) {
                throw ACError::foreignDisabled();
            }
            IRInstruction i(IROpcode::LIB_CALL);
            i.typedOperands = {mkConst("foreign"), mkConst(n.value)};
            emit(std::move(i));
            break;
        }

        case NodeType::CustomTagDef: {
            // Lower as a function named spawn_<tag> so all backends get proper function wrapping
            std::string spawnName = "spawn_" + n.value;
            int funcId = prog.symbols.intern(spawnName, IRType::FUNCTION);
            IRFunction fn(spawnName);
            fn.returnType = IRType::VOID;
            prog.functions.push_back(std::move(fn));
            cur = &prog.functions.back();
            std::string prevTag = currentCustomTag_;
            std::vector<std::string> prevTagObjs = currentTagObjects_;
            currentTagObjects_.clear();
            currentCustomTag_ = n.value;
            IRInstruction entry(IROpcode::FUNC_BEGIN); entry.typedOperands = {IRRef::func(funcId)}; emit(std::move(entry));
            if (!n.children.empty()) gen(*n.children[0]);
            IRInstruction ret(IROpcode::RETURN); ret.typedOperands = {}; emit(std::move(ret));
            IRInstruction end(IROpcode::FUNC_END); end.typedOperands = {IRRef::func(funcId)}; emit(std::move(end));
            cur = nullptr;
            currentCustomTag_ = prevTag;
            currentTagObjects_ = prevTagObjs;
            break;
        }

        case NodeType::SpawnStmt: {
            std::string spawnName = "spawn_" + n.value;
            IRRef dst = mkTemp();
            IRInstruction i(IROpcode::CALL, dst, {mkVar(spawnName)});
            emit(std::move(i));
            break;
        }

        // eval(...)/lazy_eval(...) as a BARE statement (result never assigned) — the generic
        // `default:` fallback below only recurses into children, which for these nodes means
        // "recurse into the string/expr argument," never actually lowering the EvalExpr/
        // LazyEvalExpr node itself. Verified real bug: `eval($sideEffectFn()$)` alone on a line
        // emitted NO instruction at all (not even reaching runDCE — this is a lowering gap, not
        // a dead-code-elimination one), silently dropping the entire evaluated side effect.
        case NodeType::EvalExpr:
        case NodeType::LazyEvalExpr:
            lowerExprNode(n);
            break;

        default:
            for (auto& c : n.children) gen(*c);
            break;
        }
    }

    // True if `node` (or anything under it) is a YieldStmt, WITHOUT descending into a
    // nested FuncDef — a yield inside a nested `Make` belongs to that inner function, not
    // whichever outer one we're currently scanning (matches Python's own scoping rule).
    static bool containsYield(const ASTNode& node) {
        if (node.type == NodeType::YieldStmt) return true;
        if (node.type == NodeType::FuncDef) return false;
        for (auto& c : node.children)
            if (c && containsYield(*c)) return true;
        return false;
    }

    // AST-only prepass (see generatorFuncNames_'s own comment for why this must run BEFORE
    // gen(ast), not be discovered incidentally during it). Walks the whole tree once,
    // recording every `Make func` (qualified "Class.method" inside a bundle, matching
    // FuncDef's own funcId interning convention) whose body contains a yield anywhere.
    void collectGeneratorFunctions(const ASTNode& node, const std::string& classCtx = "") {
        if (node.type == NodeType::FuncDef) {
            std::string qualified = classCtx.empty() ? node.value : classCtx + "." + node.value;
            if (!node.children.empty() && node.children[0] && containsYield(*node.children[0]))
                generatorFuncNames_.insert(qualified);
            for (auto& c : node.children)
                if (c) collectGeneratorFunctions(*c, classCtx);
            return;
        }
        if (node.type == NodeType::BundleDef) {
            for (auto& c : node.children)
                if (c) collectGeneratorFunctions(*c, node.value);
            return;
        }
        for (auto& c : node.children)
            if (c) collectGeneratorFunctions(*c, classCtx);
    }

    // Second prepass (run after collectGeneratorFunctions — needs generatorFuncNames_ already
    // complete). `g = twovals()` reaches ir.cpp's AssignStmt case through one of two AST
    // shapes depending on parser context: the dedicated "__funcall__"-tagged fast path
    // (attrs[0], children[0] = FunctionCall), or the generic structured-expression path
    // (children[0] = CallExpr/FunctionCall directly, no attrs tag) — check both.
    void collectGeneratorHandleVars(const ASTNode& node) {
        if (node.type == NodeType::AssignStmt && !node.children.empty() && node.children[0]) {
            const ASTNode& val = *node.children[0];
            if (val.type == NodeType::CallExpr || val.type == NodeType::FunctionCall) {
                if (generatorFuncNames_.count(val.value))
                    generatorHandleVars_.insert(node.value);
            }
        }
        for (auto& c : node.children)
            if (c) collectGeneratorHandleVars(*c);
    }

public:
    // Two-pass tuple-parameter-shape driver (see tupleParamClasses_'s own comment): the free
    // function generateIR() reads this back after a discovery pass and feeds it into a second,
    // real pass via setTupleParamClasses before calling generate() again.
    const std::map<std::string, std::map<int, std::string>>& getTupleParamClasses() const {
        return tupleParamClasses_;
    }
    void setTupleParamClasses(std::map<std::string, std::map<int, std::string>> m) {
        tupleParamClasses_ = std::move(m);
    }
    // A discovered className (e.g. "AcTuple2_i_i") is meaningless on its own in a fresh pass —
    // lowerTupleIndex needs its slotTypes/isAny-ness too (tupleClassSlotTypes_/tupleAnyClasses_,
    // both populated only once the class's own defining tuple literal is actually lowered,
    // which for a tuple passed as a parameter happens LATER in program order than the callee
    // whose body needs to index it). Merge — not replace — into the real pass's own registry;
    // the real pass's later emitSyntheticTupleClass call for that exact shape still fires
    // normally and re-derives the identical entry, so this only fills the gap for code that
    // runs BEFORE that point. Deliberately does NOT touch tupleShapeClasses_ (the memoization
    // key map) — pre-seeding THAT would make emitSyntheticTupleClass's own dedup check think
    // the class was already defined and skip emitting its CLASS_BEGIN/definition entirely.
    void mergeTupleClassRegistry(const std::map<std::string, std::vector<IRType>>& slotTypes,
                                 const std::set<std::string>& anyClasses) {
        for (auto& [cls, st] : slotTypes) tupleClassSlotTypes_[cls] = st;
        for (auto& cls : anyClasses) tupleAnyClasses_.insert(cls);
    }
    const std::map<std::string, std::vector<IRType>>& getTupleClassSlotTypes() const { return tupleClassSlotTypes_; }
    const std::set<std::string>& getTupleAnyClasses() const { return tupleAnyClasses_; }

    IRProgram generate(const ASTNode& ast, const std::string& backend) {
        prog         = IRProgram();
        prog.backend = backend;
        prog.target  = backend;
        // Use high-level IR for all backends that support structured while loops.
        // High-level IR: proper if/else blocks + while-true/break (no goto).
        // ASM stays low-level (uses real label/jump emitters for actual assembly).
        prog.useHighLevelIR = (backend == "PY" || backend == "JS" || backend == "HTML" ||
                               backend == "Java" || backend == "C++" || backend == "CPP" ||
                               backend == "RS" || backend == "V" || backend == "GO" ||
                               backend == "C" || backend == "LIB");
        // LIB was missing here — it inherited CppStrategy but received LOW-level IR,
        // so every if/loop became C++ `goto` → "jump crosses initialization" build errors.
        tc = lc = 0;
        inMainSection = false;
        generatorFuncNames_.clear();
        generatorHandleVars_.clear();
        collectGeneratorFunctions(ast);
        collectGeneratorHandleVars(ast);
        scalarizableTupleVars_.clear();
        tupleScalarShadowTypes_.clear();
        currentFunc_.clear();
        collectTupleTrackability(ast);
        gen(ast);

        // If no explicit <mainloop> tag was encountered, all non-import instructions
        // in dataSection are actually main body — reclassify them.
        if (prog.mainSection.empty() && !prog.dataSection.empty()) {
            for (auto& ins : prog.dataSection) {
                bool isImport = (ins.opcode == IROpcode::LIB_CALL &&
                                 !ins.typedOperands.empty() &&
                                 ins.typedOperands[0].kind == IRRef::Kind::CONST &&
                                 ins.typedOperands[0].value.type == IRType::STRING &&
                                 std::get<std::string>(ins.typedOperands[0].value.data) == "import");
                if (!isImport) prog.mainSection.push_back(ins);
            }
            // Keep only imports in dataSection
            auto& ds = prog.dataSection;
            ds.erase(std::remove_if(ds.begin(), ds.end(), [](const IRInstruction& ins) {
                if (ins.opcode != IROpcode::LIB_CALL) return true;
                if (ins.typedOperands.empty()) return true;
                if (ins.typedOperands[0].kind != IRRef::Kind::CONST) return true;
                if (ins.typedOperands[0].value.type != IRType::STRING) return true;
                return std::get<std::string>(ins.typedOperands[0].value.data) != "import";
            }), ds.end());
        }

        discoverTupleParamShapes();

        prog.globalTempCount  = tc;
        prog.globalLabelCount = lc;
        return std::move(prog);
    }
};

// ─── Optimization passes ────────────────────────────────────────────────────

// Copy propagation: when a TEMP is defined by a single STORE_VAR-like assignment
// and used exactly once, propagate its source to eliminate the temp.
// Specifically handles: STORE_VAR var {temp} where temp is used only here →
// rewrite the instruction that defines temp to write directly to var.
static void runCopyProp(std::vector<IRInstruction>& instrs) {
    // Count uses of each TEMP id
    std::unordered_map<int, int> useCount;
    // Track definition index for each TEMP id
    std::unordered_map<int, int> defIdx;

    auto countUse = [&](const IRRef& r) {
        if (r.kind == IRRef::Kind::TEMP) useCount[r.id]++;
    };

    for (int i = 0; i < (int)instrs.size(); i++) {
        const auto& ins = instrs[i];
        if (ins.result.kind == IRRef::Kind::TEMP) defIdx[ins.result.id] = i;
        for (const auto& op : ins.typedOperands) countUse(op);
    }

    // Iterate STORE_VAR instructions — two patterns:
    //   A) result=VAR, typedOperands={src_temp}          (from assignments)
    //   B) result=NONE, typedOperands={var, src_temp}    (from compound assigns)
    for (int i = 0; i < (int)instrs.size(); i++) {
        auto& ins = instrs[i];
        if (ins.opcode != IROpcode::STORE_VAR) continue;

        IRRef destVar;
        IRRef srcRef;
        if (ins.typedOperands.size() == 1 && ins.result.kind == IRRef::Kind::VAR) {
            // Pattern A
            destVar = ins.result;
            srcRef  = ins.typedOperands[0];
        } else if (ins.typedOperands.size() == 2 &&
                   ins.typedOperands[0].kind == IRRef::Kind::VAR) {
            // Pattern B
            destVar = ins.typedOperands[0];
            srcRef  = ins.typedOperands[1];
        } else {
            continue;
        }

        if (srcRef.kind != IRRef::Kind::TEMP) continue;
        if (useCount[srcRef.id] != 1) continue;
        auto dit = defIdx.find(srcRef.id);
        if (dit == defIdx.end()) continue;

        IRInstruction& def = instrs[dit->second];
        if (def.result.kind != IRRef::Kind::TEMP) continue;
        def.result = destVar;
        ins.opcode = IROpcode::NOP;
        ins.typedOperands.clear();
        ins.result = IRRef(); // clear to match NOP removal condition
    }

    // Remove NOPs
    instrs.erase(
        std::remove_if(instrs.begin(), instrs.end(),
                       [](const IRInstruction& i) { return i.opcode == IROpcode::NOP && i.typedOperands.empty() && !i.result.isValid(); }),
        instrs.end());
}

// Dead code elimination: remove TEMP-producing instructions whose result is never used
// (safe: does NOT remove side-effect ops PRINT, HALT, CALL, STORE_VAR, RETURN, etc.)
static void runDCE(std::vector<IRInstruction>& instrs) {
    // Side-effect opcodes — always kept
    auto hasSideEffect = [](IROpcode op) {
        switch (op) {
            case IROpcode::PRINT:
            case IROpcode::INPUT:        // Term.ask reads user input (side effect!)
            case IROpcode::HALT:
            case IROpcode::CALL:
            case IROpcode::LIB_CALL:
            case IROpcode::STORE_VAR:
            case IROpcode::RETURN:
            case IROpcode::JUMP:
            case IROpcode::JUMP_IF_FALSE:
            case IROpcode::JUMP_IF_TRUE:
            case IROpcode::LABEL:
            case IROpcode::ALLOC:
            case IROpcode::STORE_INDEX:
            case IROpcode::IF_BEGIN:
            case IROpcode::IF_ELSE:
            case IROpcode::IF_END:
            case IROpcode::WHILE_BEGIN:
            case IROpcode::WHILE_END:
            case IROpcode::FOR_BEGIN:
            case IROpcode::FOR_END:
            case IROpcode::TRY_BEGIN:
            case IROpcode::CATCH_BEGIN:
            case IROpcode::AFTER_BEGIN:
            case IROpcode::TRY_END:
            case IROpcode::TAG_BEGIN:
            case IROpcode::TAG_END:
            // Mirrors isSideEffect() below (used for constexpr-eval purity checks) — verified
            // real bug: `eval($sideEffectFn()$)` with its result never assigned/used got fully
            // deleted by this pass (the compiler even warned the callee "defined but never
            // called"), silently dropping eval's entire side effect. SOFT_HALT/SLEEP/EVENT_*/
            // RESTART_PROGRAM produce no TEMP result in practice so were never actually at risk,
            // but are listed here too so this switch can't silently drift from isSideEffect's
            // again if that ever changes.
            case IROpcode::EVAL:
            case IROpcode::LAZY_EVAL:
            case IROpcode::SOFT_HALT:
            case IROpcode::SLEEP:
            case IROpcode::EVENT_BIND:
            case IROpcode::EVENT_TRIGGER:
            case IROpcode::RESTART_PROGRAM:
            case IROpcode::ALIAS_DECL:
            case IROpcode::SAVE_FILE:
            case IROpcode::RAISE_CLAUSE:
            case IROpcode::FREE_DECL:
                return true;
            default:
                return false;
        }
    };

    // Collect used TEMP ids
    std::unordered_set<int> usedTemps;
    for (const auto& ins : instrs) {
        for (const auto& op : ins.typedOperands)
            if (op.kind == IRRef::Kind::TEMP) usedTemps.insert(op.id);
    }

    // Remove instructions that produce a TEMP that is never used and have no side effect
    instrs.erase(
        std::remove_if(instrs.begin(), instrs.end(), [&](const IRInstruction& ins) {
            if (hasSideEffect(ins.opcode)) return false;
            if (ins.result.kind == IRRef::Kind::TEMP && !usedTemps.count(ins.result.id))
                return true;
            return false;
        }),
        instrs.end());
}

// Resolve smart `/` (DIV) → concrete IDIV/FDIV wherever the types make it unambiguous, so the
// "special" runtime-smart division dies before a backend (esp. BNY, which otherwise float-divides
// everything — slow, and 1.9999999 for large exact integer divides). A DIV is LEFT as-is only when
// genuinely ambiguous (integer operands whose result is only displayed) → the backend keeps the
// runtime remainder check. Observable output is unchanged: an index/int-op result is the same value
// via IDIV, and a float-context result is the same via FDIV. (#div-resolve)
static void resolveDivisions(std::vector<IRInstruction>& code) {
    auto key = [](const IRRef& r) -> std::string {
        if (r.kind == IRRef::Kind::TEMP) return "t" + std::to_string(r.id);
        if (r.kind == IRRef::Kind::VAR)  return "v" + std::to_string(r.id);
        return "";
    };
    // A ref is FLOAT if a float const or defined by an instruction whose resultType is FLOAT.
    std::unordered_map<std::string, IRType> defType;
    for (const auto& ins : code)
        if (ins.result.isValid() && ins.resultType != IRType::VOID)
            defType[key(ins.result)] = ins.resultType;
    auto isFloat = [&](const IRRef& r) {
        if (r.kind == IRRef::Kind::CONST) return r.value.type == IRType::FLOAT;
        auto it = defType.find(key(r));
        return it != defType.end() && it->second == IRType::FLOAT;
    };
    auto isIntOnlyOp = [](IROpcode op) {
        return op == IROpcode::IDIV || op == IROpcode::MOD  || op == IROpcode::BAND ||
               op == IROpcode::BOR  || op == IROpcode::BXOR || op == IROpcode::BNOT ||
               op == IROpcode::PTM  || op == IROpcode::PTD;
    };
    // Classify how each ref is consumed: forced toward int (index / integer-only op) or float.
    std::set<std::string> intForced, floatForced;
    for (const auto& ins : code) {
        if ((ins.opcode == IROpcode::LOAD_INDEX || ins.opcode == IROpcode::STORE_INDEX)
            && ins.typedOperands.size() >= 2)
            intForced.insert(key(ins.typedOperands[1]));       // array index must be int
        if (isIntOnlyOp(ins.opcode))
            for (auto& op : ins.typedOperands) intForced.insert(key(op));
        if (ins.opcode == IROpcode::FDIV)
            for (auto& op : ins.typedOperands) floatForced.insert(key(op));
        if ((ins.opcode == IROpcode::ADD || ins.opcode == IROpcode::SUB
          || ins.opcode == IROpcode::MUL) && ins.typedOperands.size() >= 2) {
            if (isFloat(ins.typedOperands[0])) floatForced.insert(key(ins.typedOperands[1]));
            if (isFloat(ins.typedOperands[1])) floatForced.insert(key(ins.typedOperands[0]));
        }
    }
    for (auto& ins : code) {
        if (ins.opcode != IROpcode::DIV || ins.typedOperands.size() < 2) continue;
        std::string rk = key(ins.result);
        bool floatOperand = isFloat(ins.typedOperands[0]) || isFloat(ins.typedOperands[1]);
        if (floatOperand || floatForced.count(rk)) {
            ins.opcode = IROpcode::FDIV; ins.resultType = IRType::FLOAT;
        } else if (intForced.count(rk)) {
            ins.opcode = IROpcode::IDIV; ins.resultType = IRType::INT;
        }
        // else: leave DIV — genuinely ambiguous, backend does the runtime smart check
    }
}

// ─── Constexpr function folding ─────────────────────────────────────────────
// Evaluates pure user-defined functions at compile time when all arguments are
// constants. Equivalent to C++ constexpr: if the result is known at compile
// time, the CALL is replaced with the folded constant.

struct ConstExprEnv {
    std::unordered_map<int, IRValue> temps;   // temp id → value
    std::unordered_map<std::string, IRValue> vars; // var name → value
    bool failed = false;

    void set(const IRRef& r, const IRValue& v) {
        if (r.kind == IRRef::Kind::TEMP) temps[r.id] = v;
        else if (r.kind == IRRef::Kind::VAR) {
            // We need the symbol table to resolve VAR names but don't have it here.
            // Store by id as fallback.
            temps[-r.id - 1] = v;
        }
    }
    bool get(const IRRef& r, IRValue& out) const {
        if (r.kind == IRRef::Kind::CONST) { out = r.value; return true; }
        if (r.kind == IRRef::Kind::TEMP) {
            auto it = temps.find(r.id);
            if (it != temps.end()) { out = it->second; return true; }
        }
        if (r.kind == IRRef::Kind::VAR) {
            auto it = temps.find(-r.id - 1);
            if (it != temps.end()) { out = it->second; return true; }
        }
        return false;
    }
};

static IRValue applyBinOp(IROpcode op, const IRValue& L, const IRValue& R) {
    // String concatenation: any ADD where either side is a string
    if (op == IROpcode::ADD) {
        bool ls = L.type == IRType::STRING, rs = R.type == IRType::STRING;
        if (ls || rs) {
            auto toStr = [](const IRValue& v) -> std::string {
                if (v.type == IRType::STRING) return std::get<std::string>(v.data);
                if (v.type == IRType::INT)    return std::to_string(std::get<int64_t>(v.data));
                if (v.type == IRType::FLOAT)  return std::to_string(std::get<double>(v.data));
                if (v.type == IRType::BOOL)   return std::get<bool>(v.data) ? "true" : "false";
                return "";
            };
            return IRValue(toStr(L) + toStr(R));
        }
    }
    auto asDbl = [](const IRValue& v) -> double {
        if (v.type == IRType::FLOAT) return std::get<double>(v.data);
        if (v.type == IRType::INT)   return (double)std::get<int64_t>(v.data);
        if (v.type == IRType::BOOL)  return std::get<bool>(v.data) ? 1.0 : 0.0;
        return 0.0;
    };
    auto asInt = [](const IRValue& v) -> int64_t {
        if (v.type == IRType::INT)   return std::get<int64_t>(v.data);
        if (v.type == IRType::FLOAT) return (int64_t)std::get<double>(v.data);
        if (v.type == IRType::BOOL)  return std::get<bool>(v.data) ? 1 : 0;
        return 0;
    };
    bool anyFloat = (L.type == IRType::FLOAT || R.type == IRType::FLOAT);
    switch (op) {
        case IROpcode::ADD:
            if (anyFloat) return IRValue(asDbl(L)+asDbl(R));
            {
                __int128 v = (__int128)asInt(L) + (__int128)asInt(R);
                if (v > std::numeric_limits<int64_t>::max() || v < std::numeric_limits<int64_t>::min())
                    return IRValue(std::numeric_limits<double>::infinity());
                return IRValue((int64_t)v);
            }
        case IROpcode::SUB:
            if (anyFloat) return IRValue(asDbl(L)-asDbl(R));
            {
                __int128 v = (__int128)asInt(L) - (__int128)asInt(R);
                if (v > std::numeric_limits<int64_t>::max() || v < std::numeric_limits<int64_t>::min())
                    return IRValue(std::numeric_limits<double>::infinity());
                return IRValue((int64_t)v);
            }
        case IROpcode::MUL:
        case IROpcode::PMUL:
            if (anyFloat) return IRValue(asDbl(L)*asDbl(R));
            {
                __int128 v = (__int128)asInt(L) * (__int128)asInt(R);
                if (v > std::numeric_limits<int64_t>::max() || v < std::numeric_limits<int64_t>::min())
                    return IRValue(std::numeric_limits<double>::infinity());
                return IRValue((int64_t)v);
            }
        case IROpcode::DIV:  {
            // `/` is intentionally the "smart" division: int result for speed when the
            // quotient is exactly whole, float only when precision actually needs it
            // (Abu: "we store ints for speed but floats for precision" — `//` is
            // dedicated int division, `///` dedicated float division; plain `/` isn't
            // either of those, it auto-picks). NOT a bug — reverting my own earlier
            // "always float" change here, which wrongly treated this as broken.
            double d = asDbl(R);
            if (d == 0.0) return IRValue();   // VOID → don't fold; keep runtime div-by-zero error
            double q = asDbl(L) / d;
            int64_t qi = (int64_t)q;
            return (q == (double)qi) ? IRValue(qi) : IRValue(q);
        }
        // Division by zero must stay a RUNTIME error — folding it to 0 silently erased
        // the exception (try/catch around `x // 0` never fired on ANY backend).
        case IROpcode::FDIV: { double d = asDbl(R); return d != 0.0 ? IRValue(asDbl(L)/d) : IRValue(); }
        case IROpcode::IDIV: { int64_t r = asInt(R); return r != 0 ? IRValue(asInt(L)/r) : IRValue(); }
        case IROpcode::MOD:  { int64_t b = asInt(R); if (b==0) return IRValue(); int64_t r = asInt(L)%b; if (r!=0 && ((r<0)!=(b<0))) r+=b; return IRValue(r); }
        // Comparisons/logicals return int64 0/1 (AC's canonical truth) — a BOOL here breaks
        // typed backends (Go/Java/Rust can't put `true` in an int64) and diverges from BNY/C.
        // STRING operands must compare as strings, not via asInt() (which returns 0 for any
        // string — verified real bug reachable via `--target BNY -O4`'s whole-function
        // constexpr-eval loop: a user fn doing `IF a is b` called with two DIFFERENT constant
        // strings folded to 1/true, since asInt("cat")==asInt("dog")==0==0. Mirrors the fix
        // already applied to the OTHER, more commonly-hit fold path in this file — see its
        // "STRING operands must compare as strings" comment).
        case IROpcode::EQ: case IROpcode::NEQ: case IROpcode::LT:
        case IROpcode::GT: case IROpcode::LTE: case IROpcode::GTE:
            if (L.type == IRType::STRING && R.type == IRType::STRING) {
                int c = std::get<std::string>(L.data).compare(std::get<std::string>(R.data));
                bool r = (op==IROpcode::EQ) ? (c==0) : (op==IROpcode::NEQ) ? (c!=0)
                       : (op==IROpcode::LT) ? (c<0)  : (op==IROpcode::GT)  ? (c>0)
                       : (op==IROpcode::LTE)? (c<=0) : (c>=0);
                return IRValue((int64_t)(r ? 1 : 0));
            }
            switch (op) {
                case IROpcode::EQ:  return IRValue((int64_t)((anyFloat ? (asDbl(L)==asDbl(R)) : (asInt(L)==asInt(R))) ? 1 : 0));
                case IROpcode::NEQ: return IRValue((int64_t)((anyFloat ? (asDbl(L)!=asDbl(R)) : (asInt(L)!=asInt(R))) ? 1 : 0));
                case IROpcode::LT:  return IRValue((int64_t)((anyFloat ? (asDbl(L)< asDbl(R)) : (asInt(L)< asInt(R))) ? 1 : 0));
                case IROpcode::GT:  return IRValue((int64_t)((anyFloat ? (asDbl(L)> asDbl(R)) : (asInt(L)> asInt(R))) ? 1 : 0));
                case IROpcode::LTE: return IRValue((int64_t)((anyFloat ? (asDbl(L)<=asDbl(R)) : (asInt(L)<=asInt(R))) ? 1 : 0));
                default:             return IRValue((int64_t)((anyFloat ? (asDbl(L)>=asDbl(R)) : (asInt(L)>=asInt(R))) ? 1 : 0));
            }
        case IROpcode::AND:  return IRValue((int64_t)((asInt(L) && asInt(R)) ? 1 : 0));
        case IROpcode::OR:   return IRValue((int64_t)((asInt(L) || asInt(R)) ? 1 : 0));
        case IROpcode::XOR:  return IRValue((int64_t)(((bool)(asInt(L)) != (bool)(asInt(R))) ? 1 : 0));
        case IROpcode::XNOR: return IRValue((int64_t)(((bool)(asInt(L)) == (bool)(asInt(R))) ? 1 : 0));
        // Bitwise (int-only)
        case IROpcode::BAND: return IRValue(asInt(L) & asInt(R));
        case IROpcode::BOR:  return IRValue(asInt(L) | asInt(R));
        case IROpcode::BXOR: return IRValue(asInt(L) ^ asInt(R));
        case IROpcode::XSUB: {
            // |a-b|+1 computed in unsigned to avoid signed-overflow / -INT64_MIN UB; clamp on overflow.
            int64_t a = asInt(L), b = asInt(R);
            unsigned long long diff = (a >= b)
                ? (unsigned long long)a - (unsigned long long)b
                : (unsigned long long)b - (unsigned long long)a;
            int64_t mx = std::numeric_limits<int64_t>::max();
            return IRValue(diff >= (unsigned long long)mx ? mx : (int64_t)diff + 1);
        }
        default: return IRValue(0);
    }
}

static double constexprAsDouble(const IRValue& v) {
    if (v.type == IRType::FLOAT) return std::get<double>(v.data);
    if (v.type == IRType::INT)   return (double)std::get<int64_t>(v.data);
    if (v.type == IRType::BOOL)  return std::get<bool>(v.data) ? 1.0 : 0.0;
    return 0.0;
}

static int64_t constexprAsInt(const IRValue& v) {
    if (v.type == IRType::INT)   return std::get<int64_t>(v.data);
    if (v.type == IRType::FLOAT) return (int64_t)std::get<double>(v.data);
    if (v.type == IRType::BOOL)  return std::get<bool>(v.data) ? 1 : 0;
    return 0;
}

static int64_t constexprGcd(int64_t a, int64_t b) {
    a = std::llabs(a);
    b = std::llabs(b);
    while (b != 0) {
        int64_t r = a % b;
        a = b;
        b = r;
    }
    return a;
}

static bool constexprIsPrime(int64_t n) {
    if (n < 2) return false;
    if (n % 2 == 0) return n == 2;
    for (int64_t d = 3; d <= n / d; d += 2)
        if (n % d == 0) return false;
    return true;
}

static double constexprPiPrecision(int64_t digits, double value) {
    if (digits <= 0 || digits > 15) return value;
    double f = std::pow(10.0, (double)digits);
    return std::floor(value * f) / f;
}

static std::string normalizeMathName(std::string name, bool mathImported) {
    if (name.rfind("AcMath.", 0) == 0) name = name.substr(7);
    if (name.rfind("math_", 0) == 0) name = "math." + name.substr(5);
    for (char& c : name) {
        if (c == ':') c = '.';
    }
    if (name == "math.abs.int") name = "math.abs_int";
    if (name == "math.mod.int") name = "math.mod_int";
    if (name == "math.pi.prec") name = "math.pi";
    if (name == "math.e.prec") name = "math.e";
    if (name == "math.phi.prec") name = "math.phi";

    if (name.find('.') == std::string::npos && mathImported) {
        static const std::unordered_set<std::string> bareMath = {
            "pi","e","tau","em","phi","inf",
            "sin","cos","tan","csc","sec","cot",
            "asin","acos","atan","acsc","asec","acot","atan2",
            "deg2rad","rad2deg",
            "pow","sqrt","cbrt","hypot",
            "floor","ceil","round","abs","abs_int",
            "ln","log","log2","log10",
            "mod","mod_int",
            "to_int","to_dec",
            "gcd","lcm","is_prime","clamp"
        };
        if (bareMath.count(name)) name = "math." + name;
    }
    return name;
}

static IRValue constexprMathConstant(const std::string& normalizedName) {
    if (normalizedName == "math.pi")  return IRValue(3.141592653589793238462643383279502884197);
    if (normalizedName == "math.e")   return IRValue(2.718281828459045235360287471352662497757);
    if (normalizedName == "math.tau") return IRValue(6.283185307179586476925286766559005768394);
    if (normalizedName == "math.em")  return IRValue(0.5772156649015328606065120900824024310422);
    if (normalizedName == "math.phi") return IRValue(1.618033988749894848204586834365638117720);
    if (normalizedName == "math.inf") return IRValue(std::numeric_limits<double>::infinity());
    return IRValue();
}

static IRValue tryConstexprMathEval(const std::string& rawName,
                                    const std::vector<IRValue>& args,
                                    const IRProgram& prog)
{
    std::string name = normalizeMathName(rawName, prog.importedLibs.count("math") > 0);
    if (name.rfind("math.", 0) != 0) return IRValue();

    auto constant = constexprMathConstant(name);
    if (args.empty() && constant.type != IRType::VOID) return constant;

    auto arity = [&](size_t n) { return args.size() == n; };
    auto D = [&](size_t i) { return constexprAsDouble(args[i]); };
    auto I = [&](size_t i) { return constexprAsInt(args[i]); };

    if (name == "math.pi"  && arity(1)) return IRValue(constexprPiPrecision(I(0), 3.141592653589793238462643383279502884197));
    if (name == "math.e"   && arity(1)) return IRValue(constexprPiPrecision(I(0), 2.718281828459045235360287471352662497757));
    if (name == "math.phi" && arity(1)) return IRValue(constexprPiPrecision(I(0), 1.618033988749894848204586834365638117720));

    if (name == "math.sin"     && arity(1)) return IRValue(std::sin(D(0)));
    if (name == "math.cos"     && arity(1)) return IRValue(std::cos(D(0)));
    if (name == "math.tan"     && arity(1)) return IRValue(std::tan(D(0)));
    if (name == "math.csc"     && arity(1)) return IRValue(1.0 / std::sin(D(0)));
    if (name == "math.sec"     && arity(1)) return IRValue(1.0 / std::cos(D(0)));
    if (name == "math.cot"     && arity(1)) return IRValue(std::cos(D(0)) / std::sin(D(0)));
    if (name == "math.asin"    && arity(1)) return IRValue(std::asin(D(0)));
    if (name == "math.acos"    && arity(1)) return IRValue(std::acos(D(0)));
    if (name == "math.atan"    && arity(1)) return IRValue(std::atan(D(0)));
    if (name == "math.acsc"    && arity(1)) return IRValue(std::asin(1.0 / D(0)));
    if (name == "math.asec"    && arity(1)) return IRValue(std::acos(1.0 / D(0)));
    if (name == "math.acot"    && arity(1)) return IRValue((D(0) > 0 ? 1.0 : -1.0) * std::atan(1.0 / std::fabs(D(0))));
    if (name == "math.atan2"   && arity(2)) return IRValue(std::atan2(D(0), D(1)));
    if (name == "math.deg2rad" && arity(1)) return IRValue(D(0) * (3.141592653589793238462643383279502884197 / 180.0));
    if (name == "math.rad2deg" && arity(1)) return IRValue(D(0) * (180.0 / 3.141592653589793238462643383279502884197));

    if (name == "math.pow"   && arity(2)) {
        double v = std::pow(D(0), D(1));
        if (std::isfinite(v) && std::fabs(v) > (double)std::numeric_limits<int64_t>::max())
            return IRValue(std::numeric_limits<double>::infinity());
        return IRValue(v);
    }
    if (name == "math.sqrt"  && arity(1)) return IRValue(std::sqrt(D(0)));
    if (name == "math.cbrt"  && arity(1)) return IRValue(std::cbrt(D(0)));
    if (name == "math.hypot" && arity(2)) return IRValue(std::hypot(D(0), D(1)));
    if (name == "math.abs"   && arity(1)) return IRValue(std::fabs(D(0)));
    if (name == "math.abs_int" && arity(1)) return IRValue((int64_t)std::llabs(I(0)));
    if (name == "math.floor" && arity(1)) return IRValue(std::floor(D(0)));
    if (name == "math.ceil"  && arity(1)) return IRValue(std::ceil(D(0)));
    if (name == "math.round" && arity(1)) return IRValue(std::round(D(0)));

    if (name == "math.ln"    && arity(1)) return IRValue(std::log(D(0)));
    if (name == "math.log"   && arity(2)) return IRValue(std::log(D(1)) / std::log(D(0)));
    if (name == "math.log2"  && arity(1)) return IRValue(std::log2(D(0)));
    if (name == "math.log10" && arity(1)) return IRValue(std::log10(D(0)));

    if (name == "math.mod" && arity(2)) {
        double r = std::fmod(D(0), D(1));
        if (r != 0.0 && (r < 0) != (D(1) < 0)) r += D(1);
        // int-exact when whole (matches the PY ffi smart wrap — keeps 10 from becoming 10.0)
        int64_t ri = (int64_t)r;
        return (r == (double)ri) ? IRValue(ri) : IRValue(r);
    }
    if (name == "math.mod_int" && arity(2)) {
        int64_t b = I(1);
        if (b == 0) return IRValue();
        int64_t r = I(0) % b;
        if (r != 0 && (r < 0) != (b < 0)) r += b;
        return IRValue(r);
    }

    if (name == "math.to_int" && arity(1)) return IRValue((int64_t)D(0));
    if (name == "math.to_dec" && arity(1)) return IRValue((double)I(0));
    if (name == "math.gcd" && arity(2)) return IRValue(constexprGcd(I(0), I(1)));
    if (name == "math.lcm" && arity(2)) {
        int64_t g = constexprGcd(I(0), I(1));
        if (g == 0) return IRValue((int64_t)0);
        return IRValue((int64_t)std::llabs((I(0) / g) * I(1)));
    }
    if (name == "math.is_prime" && arity(1)) return IRValue((int64_t)(constexprIsPrime(I(0)) ? 1 : 0));
    if (name == "math.clamp" && arity(3)) return IRValue(std::min(std::max(D(0), D(1)), D(2)));

    return IRValue();
}

// Returns true if opcode has side effects (makes a function non-pure)
static bool isSideEffect(IROpcode op) {
    switch (op) {
        case IROpcode::PRINT: case IROpcode::INPUT: case IROpcode::LIB_CALL:
        case IROpcode::HALT: case IROpcode::SOFT_HALT: case IROpcode::SLEEP:
        case IROpcode::EVENT_BIND: case IROpcode::EVENT_TRIGGER:
        case IROpcode::EVAL: case IROpcode::LAZY_EVAL: case IROpcode::RESTART_PROGRAM:
        case IROpcode::ALIAS_DECL: case IROpcode::SAVE_FILE:
            return true;
        default: return false;
    }
}

// Try to evaluate a user-defined function at compile time.
// Returns a valid IRValue on success, or IRValue(VOID) if the function is not pure
// or the result can't be determined.
static IRValue tryConstexprEval(const std::string& fname,
                                const std::vector<IRValue>& args,
                                const IRProgram& prog,
                                int depth = 0)
{
    static const int MAX_DEPTH = 64;       // matches default backend recursion limit
    static const int MAX_STEPS = 15000000; // 15M steps per constexpr evaluation
    if (depth > MAX_DEPTH) return IRValue();

    const IRFunction* fn = prog.findFunction(fname);
    if (!fn) return IRValue();

    // Check purity first
    for (const auto& ins : fn->instructions)
        if (isSideEffect(ins.opcode)) return IRValue();

    ConstExprEnv env;
    // Bind parameters from fn->parameters to args
    for (size_t k = 0; k < fn->parameters.size() && k < args.size(); ++k) {
        IRRef paramRef;
        paramRef.kind = IRRef::Kind::VAR;
        paramRef.id   = k; // placeholder — map by position
        // Store under a recognizable key: negative of param index
        env.temps[-(int)k - 10000] = args[k];
        // Also store by param name if we can find the symbol
        // Actually store by the param name via the vars map
        env.vars[fn->parameters[k]] = args[k];
    }

    // Pre-scan: label positions and WHILE_BEGIN/WHILE_END pairing
    std::unordered_map<int, int> labelPc;         // label-id → pc
    std::unordered_map<int, int> whileEndToBegin; // end_pc → begin_pc (for WHILE_END)
    std::unordered_map<int, int> whileBeginToEnd; // begin_pc → end_pc (for break sentinel)
    {
        std::stack<int> stk;
        for (int i = 0; i < (int)fn->instructions.size(); ++i) {
            const auto& ins2 = fn->instructions[i];
            if (ins2.opcode == IROpcode::LABEL && !ins2.typedOperands.empty()
                    && ins2.typedOperands[0].kind == IRRef::Kind::LABEL)
                labelPc[ins2.typedOperands[0].id] = i;
            else if (ins2.opcode == IROpcode::WHILE_BEGIN)
                stk.push(i);
            else if (ins2.opcode == IROpcode::WHILE_END && !stk.empty()) {
                int bpc = stk.top(); stk.pop();
                whileEndToBegin[i] = bpc;
                whileBeginToEnd[bpc] = i;
            }
        }
    }

    int steps = 0;
    int pc = 0;
    bool inSkippedIf = false;
    int skipDepth = 0;

    while (pc < (int)fn->instructions.size() && steps < MAX_STEPS) {
        ++steps;
        const auto& ins = fn->instructions[pc];

        // Skip branches of false conditions
        if (inSkippedIf) {
            if (ins.opcode == IROpcode::IF_BEGIN) { ++skipDepth; ++pc; continue; }
            if (ins.opcode == IROpcode::IF_ELSE && skipDepth == 0) { inSkippedIf = false; ++pc; continue; }
            if (ins.opcode == IROpcode::IF_END) {
                if (skipDepth == 0) inSkippedIf = false;
                else --skipDepth;
                ++pc; continue;
            }
            ++pc; continue;
        }

        auto getRef = [&](const IRRef& r) -> IRValue {
            if (r.kind == IRRef::Kind::CONST) return r.value;
            if (r.kind == IRRef::Kind::TEMP) {
                auto it = env.temps.find(r.id); if (it != env.temps.end()) return it->second;
            }
            if (r.kind == IRRef::Kind::VAR) {
                // Try symbol name lookup via prog.symbols
                std::string vname = r.id >= 0 ? prog.symbols.getName(r.id) : "";
                if (!vname.empty()) {
                    auto it = env.vars.find(vname);
                    if (it != env.vars.end()) return it->second;
                }
            }
            return IRValue(); // unknown
        };
        auto setRef = [&](const IRRef& r, const IRValue& v) {
            if (r.kind == IRRef::Kind::TEMP) env.temps[r.id] = v;
            else if (r.kind == IRRef::Kind::VAR) {
                std::string vname = r.id >= 0 ? prog.symbols.getName(r.id) : "";
                if (!vname.empty()) env.vars[vname] = v;
            }
        };

        switch (ins.opcode) {
        case IROpcode::NOP: case IROpcode::FREE_DECL: case IROpcode::FUNC_BEGIN:
        case IROpcode::FUNC_END: case IROpcode::TAG_BEGIN: case IROpcode::TAG_END:
            break;
        case IROpcode::STORE_VAR: {
            IRValue val;
            if (ins.typedOperands.size() >= 2) val = getRef(ins.typedOperands[1]);
            else if (!ins.typedOperands.empty()) val = getRef(ins.typedOperands[0]);
            if (val.type == IRType::VOID) return IRValue();
            if (ins.typedOperands.size() >= 2) setRef(ins.typedOperands[0], val);
            else if (ins.result.isValid()) setRef(ins.result, val);
            break;
        }
        case IROpcode::RETURN: {
            if (ins.typedOperands.empty()) return IRValue();
            IRValue v = getRef(ins.typedOperands[0]);
            return v; // success!
        }
        case IROpcode::IF_BEGIN: {
            if (ins.typedOperands.empty()) { ++pc; continue; }
            IRValue cond = getRef(ins.typedOperands[0]);
            if (cond.type == IRType::VOID) return IRValue();
            bool condTrue = false;
            if (cond.type == IRType::BOOL)  condTrue = std::get<bool>(cond.data);
            else if (cond.type == IRType::INT) condTrue = std::get<int64_t>(cond.data) != 0;
            else if (cond.type == IRType::FLOAT) condTrue = std::get<double>(cond.data) != 0.0;
            if (!condTrue) { inSkippedIf = true; skipDepth = 0; }
            break;
        }
        case IROpcode::IF_ELSE:
            // We're here because the if-branch was taken → skip else
            inSkippedIf = true; skipDepth = 0;
            break;
        case IROpcode::IF_END:
            break;
        case IROpcode::ADD: case IROpcode::SUB: case IROpcode::MUL: case IROpcode::PMUL:
        case IROpcode::DIV: case IROpcode::IDIV: case IROpcode::MOD: case IROpcode::EQ: case IROpcode::NEQ:
        case IROpcode::LT: case IROpcode::GT: case IROpcode::LTE: case IROpcode::GTE:
        case IROpcode::AND: case IROpcode::OR: case IROpcode::XOR: case IROpcode::XNOR:
        case IROpcode::XSUB:
            if (ins.result.isValid() && ins.typedOperands.size() >= 2) {
                IRValue L = getRef(ins.typedOperands[0]);
                IRValue R = getRef(ins.typedOperands[1]);
                if (L.type == IRType::VOID || R.type == IRType::VOID) return IRValue();
                setRef(ins.result, applyBinOp(ins.opcode, L, R));
            }
            break;
        case IROpcode::NOT:
            if (ins.result.isValid() && !ins.typedOperands.empty()) {
                IRValue v = getRef(ins.typedOperands[0]);
                if (v.type == IRType::VOID) return IRValue();
                bool b = (v.type == IRType::INT   ? std::get<int64_t>(v.data) != 0
                        : v.type == IRType::FLOAT ? std::get<double>(v.data) != 0
                        : v.type == IRType::BOOL  ? std::get<bool>(v.data)
                        : false);
                setRef(ins.result, IRValue((int64_t)(b ? 0 : 1))); // int 0/1, not bool
            }
            break;
        case IROpcode::CALL:
            if (ins.result.isValid() && !ins.typedOperands.empty()) {
                std::string callee;
                const auto& fn_ref = ins.typedOperands[0];
                if (fn_ref.kind == IRRef::Kind::VAR)
                    callee = fn_ref.id >= 0 ? prog.symbols.getName(fn_ref.id) : "";
                if (callee.empty()) return IRValue();
                std::vector<IRValue> cargs;
                for (size_t j = 1; j < ins.typedOperands.size(); ++j) {
                    IRValue av = getRef(ins.typedOperands[j]);
                    if (av.type == IRType::VOID) return IRValue(); // unknown arg
                    cargs.push_back(av);
                }
                IRValue cv = tryConstexprMathEval(callee, cargs, prog);
                if (cv.type == IRType::VOID)
                    cv = tryConstexprEval(callee, cargs, prog, depth + 1);
                if (cv.type == IRType::VOID) return IRValue();
                setRef(ins.result, cv);
            }
            break;
        case IROpcode::LABEL:
            break; // position marker already in labelPc
        case IROpcode::JUMP: {
            if (ins.typedOperands.empty()) return IRValue();
            const auto& lbl = ins.typedOperands[0];
            if (lbl.kind != IRRef::Kind::LABEL) return IRValue();
            auto it = labelPc.find(lbl.id);
            if (it == labelPc.end()) return IRValue();
            pc = it->second;
            continue;
        }
        case IROpcode::JUMP_IF_FALSE:
        case IROpcode::JUMP_IF_TRUE: {
            if (ins.typedOperands.size() < 2) return IRValue();
            IRValue cond = getRef(ins.typedOperands[0]);
            if (cond.type == IRType::VOID) return IRValue();
            bool condTrue = (cond.type == IRType::BOOL  ? std::get<bool>(cond.data)
                           : cond.type == IRType::INT   ? std::get<int64_t>(cond.data) != 0
                           : cond.type == IRType::FLOAT ? std::get<double>(cond.data) != 0.0
                           : false);
            bool shouldJump = (ins.opcode == IROpcode::JUMP_IF_FALSE) ? !condTrue : condTrue;
            if (shouldJump) {
                const auto& lbl = ins.typedOperands[1];
                if (lbl.kind == IRRef::Kind::LABEL) {
                    // Low-level: direct label jump
                    auto it = labelPc.find(lbl.id);
                    if (it == labelPc.end()) return IRValue();
                    pc = it->second;
                    continue;
                } else {
                    // High-level __break__ sentinel: jump past the matching WHILE_END
                    int beginPc = -1, scanDepth = 0;
                    for (int i = pc - 1; i >= 0; --i) {
                        if (fn->instructions[i].opcode == IROpcode::WHILE_END) ++scanDepth;
                        else if (fn->instructions[i].opcode == IROpcode::WHILE_BEGIN) {
                            if (scanDepth == 0) { beginPc = i; break; }
                            --scanDepth;
                        }
                    }
                    if (beginPc < 0) return IRValue();
                    auto it = whileBeginToEnd.find(beginPc);
                    if (it == whileBeginToEnd.end()) return IRValue();
                    pc = it->second + 1; // after WHILE_END
                    continue;
                }
            }
            break;
        }
        case IROpcode::WHILE_BEGIN:
            break;
        case IROpcode::WHILE_END: {
            // High-level loop: jump back to WHILE_BEGIN for next iteration
            auto it = whileEndToBegin.find(pc);
            if (it == whileEndToBegin.end()) return IRValue();
            pc = it->second;
            continue;
        }
        case IROpcode::LOAD_CONST:
            if (ins.result.isValid() && !ins.typedOperands.empty())
                setRef(ins.result, getRef(ins.typedOperands[0]));
            break;
        default:
            // Unknown opcode — give up
            return IRValue();
        }
        ++pc;
    }
    if (steps >= MAX_STEPS)   // blew the 15M-op compile-time budget — roast + fall back to runtime
        std::cerr << Toxic::threeBusinessDays() << "\n";
    return IRValue(); // no RETURN found or ran out of steps
}

static void runMathConstantRefs(std::vector<IRInstruction>& instrs, const IRProgram& prog) {
    auto foldRef = [&](IRRef& r) {
        if (r.kind != IRRef::Kind::VAR || r.id < 0) return;
        std::string name = normalizeMathName(prog.symbols.getName(r.id), false);
        IRValue constant = constexprMathConstant(name);
        if (constant.type != IRType::VOID)
            r = IRRef::constant(constant);
    };

    for (auto& ins : instrs) {
        // operand[0] of a CALL/LIB_CALL is the CALLEE NAME, not a value reference — for the
        // precision-argument form (`math.pi(5)`), that operand holds "math.pi.prec", which
        // normalizeMathName folds right back down to "math.pi" (the bare-constant name), so
        // foldRef was blindly replacing the call's OWN function-name operand with the constant
        // 3.14159... — every backend then tried to emit a "call" to a raw float literal (verified:
        // even the PY reference crashed, `TypeError: 'float' object is not callable`, on
        // `t_0 = 3.141592653589793(5)` — this was never ASM-specific, just visibly a hard NASM
        // assembly error there instead of a runtime crash elsewhere). Skip index 0 for these two
        // opcodes; every other opcode's operands are all genuine value references.
        bool isCallLike = ins.opcode == IROpcode::CALL || ins.opcode == IROpcode::LIB_CALL;
        for (size_t idx = 0; idx < ins.typedOperands.size(); idx++) {
            if (isCallLike && idx == 0) continue;
            foldRef(ins.typedOperands[idx]);
        }
    }
}

static void runLocalConstFolding(std::vector<IRInstruction>& instrs, const IRProgram& prog) {
    std::unordered_map<int, IRValue> tempConst;
    std::unordered_map<int, IRValue> varConst;

    auto clearOnControlFlow = [&](IROpcode op) {
        switch (op) {
            case IROpcode::LABEL:
            case IROpcode::JUMP:
            case IROpcode::JUMP_IF_TRUE:
            case IROpcode::JUMP_IF_FALSE:
            case IROpcode::IF_BEGIN:
            case IROpcode::IF_ELSE:
            case IROpcode::IF_END:
            case IROpcode::WHILE_BEGIN:
            case IROpcode::WHILE_END:
            case IROpcode::FOR_BEGIN:
            case IROpcode::FOR_END:
                return true;
            default:
                return false;
        }
    };

    auto known = [&](const IRRef& r, IRValue& out) -> bool {
        if (r.kind == IRRef::Kind::CONST && r.value.type != IRType::VOID) {
            out = r.value;
            return true;
        }
        if (r.kind == IRRef::Kind::TEMP) {
            auto it = tempConst.find(r.id);
            if (it != tempConst.end()) { out = it->second; return true; }
        }
        if (r.kind == IRRef::Kind::VAR) {
            auto it = varConst.find(r.id);
            if (it != varConst.end()) { out = it->second; return true; }
        }
        return false;
    };

    auto remember = [&](const IRRef& r, const IRValue& v) {
        if (r.kind == IRRef::Kind::TEMP) tempConst[r.id] = v;
        else if (r.kind == IRRef::Kind::VAR) varConst[r.id] = v;
    };

    auto forget = [&](const IRRef& r) {
        if (r.kind == IRRef::Kind::TEMP) tempConst.erase(r.id);
        else if (r.kind == IRRef::Kind::VAR) varConst.erase(r.id);
    };

    for (auto& ins : instrs) {
        if (clearOnControlFlow(ins.opcode)) {
            tempConst.clear();
            varConst.clear();
            continue;
        }

        switch (ins.opcode) {
            case IROpcode::LOAD_CONST: {
                if (ins.result.isValid() && !ins.typedOperands.empty()) {
                    IRValue v;
                    if (known(ins.typedOperands[0], v)) {
                        ins.typedOperands[0] = IRRef::constant(v);
                        remember(ins.result, v);
                    } else {
                        forget(ins.result);
                    }
                }
                break;
            }

            case IROpcode::STORE_VAR: {
                IRRef dest;
                IRRef src;
                if (ins.typedOperands.size() >= 2) {
                    dest = ins.typedOperands[0];
                    src = ins.typedOperands[1];
                } else if (ins.result.isValid() && !ins.typedOperands.empty()) {
                    dest = ins.result;
                    src = ins.typedOperands[0];
                }

                IRValue v;
                if (dest.isValid() && known(src, v)) {
                    if (ins.typedOperands.size() >= 2) ins.typedOperands[1] = IRRef::constant(v);
                    else ins.typedOperands[0] = IRRef::constant(v);
                    remember(dest, v);
                } else if (dest.isValid()) {
                    forget(dest);
                }
                break;
            }

            case IROpcode::ADD: case IROpcode::SUB: case IROpcode::MUL: case IROpcode::PMUL:
            case IROpcode::DIV: case IROpcode::IDIV: case IROpcode::MOD: case IROpcode::EQ: case IROpcode::NEQ:
            case IROpcode::LT: case IROpcode::GT: case IROpcode::LTE: case IROpcode::GTE:
            case IROpcode::AND: case IROpcode::OR: case IROpcode::XOR: case IROpcode::XNOR:
            case IROpcode::XSUB: {
                if (!ins.result.isValid() || ins.typedOperands.size() < 2) {
                    break;
                }
                IRValue L, R;
                if (known(ins.typedOperands[0], L) && known(ins.typedOperands[1], R)) {
                    IRValue folded = applyBinOp(ins.opcode, L, R);
                    if (folded.type == IRType::VOID) { forget(ins.result); break; } // e.g. div by 0 — keep runtime error
                    // A short/mini-typed result must wrap to its real width BEFORE it gets
                    // baked into a literal — applyBinOp computes in plain int64, unaware of
                    // the destination's declared width (ins.resultType, set during
                    // lowering, still intact here — only overwritten a few lines down).
                    // Skipping this let a folded out-of-range literal reach the target
                    // language verbatim: `short x=2147483647; x=x+1` folded straight to
                    // the literal 2147483648, which Go/Java reject outright at compile
                    // time ("constant overflows int32" / "lossy conversion") since it
                    // never fits a 32-bit type — not merely a wrong VALUE bug, a compile
                    // failure. Preserving the narrow type on `folded` (not widening to
                    // INT) — NOT retyped to SHORT/MINI: commonRef's CONST-value rendering
                    // (ir_codegen.cpp) only special-cases IRType::INT/FLOAT/BOOL/STRING;
                    // handing it a CONST literally typed SHORT falls through every branch
                    // to its final `return "0"` default, discarding the correctly-wrapped
                    // number entirely (caught by this exact test). `x`'s own declared
                    // width is already tracked separately from its TYPE_CAST declaration —
                    // this fold only needs the NUMBER wrapped, not the const's type changed.
                    int w = irIntWidth(ins.resultType);
                    if (w && folded.type == IRType::INT) {
                        uint64_t v = (uint64_t)std::get<int64_t>(folded.data);
                        uint64_t mask = (w >= 64) ? ~0ULL : ((1ULL << w) - 1);
                        uint64_t u = v & mask;
                        uint64_t signBit = 1ULL << (w - 1);
                        if (u & signBit) u -= (mask + 1);
                        folded.data = (int64_t)u;
                    }
                    ins.opcode = IROpcode::LOAD_CONST;
                    ins.typedOperands = {IRRef::constant(folded)};
                    ins.resultType = folded.type;
                    remember(ins.result, folded);
                } else {
                    forget(ins.result);
                }
                break;
            }

            case IROpcode::NOT: {
                if (!ins.result.isValid() || ins.typedOperands.empty()) break;
                IRValue v;
                if (known(ins.typedOperands[0], v)) {
                    bool b = (v.type == IRType::INT   ? std::get<int64_t>(v.data) != 0
                            : v.type == IRType::FLOAT ? std::get<double>(v.data) != 0
                            : v.type == IRType::BOOL  ? std::get<bool>(v.data)
                            : false);
                    IRValue folded((int64_t)(b ? 0 : 1)); // int 0/1, not bool (typed backends)
                    ins.opcode = IROpcode::LOAD_CONST;
                    ins.typedOperands = {IRRef::constant(folded)};
                    ins.resultType = folded.type;
                    remember(ins.result, folded);
                } else {
                    forget(ins.result);
                }
                break;
            }

            case IROpcode::CALL: {
                if (!ins.result.isValid() || ins.typedOperands.empty()) break;
                const auto& fnRef = ins.typedOperands[0];
                if (fnRef.kind != IRRef::Kind::VAR || fnRef.id < 0) {
                    forget(ins.result);
                    break;
                }
                std::vector<IRValue> args;
                bool allConst = true;
                for (size_t j = 1; j < ins.typedOperands.size(); ++j) {
                    IRValue v;
                    if (known(ins.typedOperands[j], v)) args.push_back(v);
                    else { allConst = false; break; }
                }
                if (allConst) {
                    IRValue folded = tryConstexprMathEval(prog.symbols.getName(fnRef.id), args, prog);
                    if (folded.type != IRType::VOID) {
                        ins.opcode = IROpcode::LOAD_CONST;
                        ins.typedOperands = {IRRef::constant(folded)};
                        ins.resultType = folded.type;
                        remember(ins.result, folded);
                        break;
                    }
                }
                forget(ins.result);
                break;
            }

            default:
                if (ins.result.isValid()) forget(ins.result);
                break;
        }
    }

}

// Walk all instructions; replace CALL with constant result when possible
static void runConstexprFolding(std::vector<IRInstruction>& instrs, const IRProgram& prog) {
    for (auto& ins : instrs) {
        if (ins.opcode != IROpcode::CALL) continue;
        if (!ins.result.isValid() || ins.typedOperands.empty()) continue;
        // Get function name
        const auto& fn_ref = ins.typedOperands[0];
        if (fn_ref.kind != IRRef::Kind::VAR) continue;
        std::string callee = fn_ref.id >= 0 ? prog.symbols.getName(fn_ref.id) : "";
        if (callee.empty()) continue;
        // Check all args are constants
        std::vector<IRValue> args;
        bool allConst = true;
        for (size_t j = 1; j < ins.typedOperands.size(); ++j) {
            const auto& op = ins.typedOperands[j];
            if (op.kind == IRRef::Kind::CONST && op.value.type != IRType::VOID)
                args.push_back(op.value);
            else { allConst = false; break; }
        }
        if (!allConst) continue;
        // Try to evaluate built-in constexpr math first, then user-defined pure functions.
        IRValue result = tryConstexprMathEval(callee, args, prog);
        if (result.type == IRType::VOID)
            result = tryConstexprEval(callee, args, prog);
        if (result.type == IRType::VOID) continue;
        // Replace CALL with LOAD_CONST
        IRInstruction folded(IROpcode::LOAD_CONST, ins.result, {IRRef::constant(result)});
        folded.resultType = result.type;
        ins = std::move(folded);
    }
}

static void expandAliasWrites(std::vector<IRInstruction>& instrs) {
    std::unordered_map<int, std::set<int>> groups;

    auto merge = [&](int a, int b) {
        std::set<int> merged;
        auto ia = groups.find(a);
        auto ib = groups.find(b);
        if (ia != groups.end()) merged.insert(ia->second.begin(), ia->second.end());
        if (ib != groups.end()) merged.insert(ib->second.begin(), ib->second.end());
        merged.insert(a);
        merged.insert(b);
        for (int id : merged) groups[id] = merged;
    };

    auto aliasesFor = [&](int id) {
        std::vector<int> out;
        auto it = groups.find(id);
        if (it == groups.end()) return out;
        for (int alias : it->second)
            if (alias != id) out.push_back(alias);
        return out;
    };

    std::vector<IRInstruction> out;
    out.reserve(instrs.size());

    for (auto ins : instrs) {
        if (ins.opcode == IROpcode::ALIAS_DECL) {
            if (ins.typedOperands.size() >= 2 &&
                ins.typedOperands[0].kind == IRRef::Kind::VAR &&
                ins.typedOperands[1].kind == IRRef::Kind::VAR) {
                merge(ins.typedOperands[0].id, ins.typedOperands[1].id);
            }
            continue;
        }

        out.push_back(ins);

        if (ins.opcode != IROpcode::STORE_VAR) continue;

        IRRef dest;
        bool destInResult = false;
        if (ins.result.kind == IRRef::Kind::VAR && ins.typedOperands.size() == 1) {
            dest = ins.result;
            destInResult = true;
        } else if (ins.typedOperands.size() >= 2 && ins.typedOperands[0].kind == IRRef::Kind::VAR) {
            dest = ins.typedOperands[0];
        } else {
            continue;
        }

        for (int aliasId : aliasesFor(dest.id)) {
            IRInstruction dup = ins;
            if (destInResult) dup.result = IRRef::var(aliasId);
            else dup.typedOperands[0] = IRRef::var(aliasId);
            out.push_back(std::move(dup));
        }
    }

    instrs = std::move(out);
}

static void expandAliases(IRProgram& prog) {
    expandAliasWrites(prog.globalInit);
    for (auto& fn : prog.functions) expandAliasWrites(fn.instructions);
}

static void runOptPasses(IRProgram& prog) {
    // ── Collect usage info BEFORE optimization (constexpr removes CALLs) ──────
    std::set<std::string> calledFnsPre, writtenVarsPre, readVarsPre;
    if (prog.backend != "LIB") {
        auto scanPre = [&](const std::vector<IRInstruction>& instrs) {
            for (const auto& ins : instrs) {
                if ((ins.opcode == IROpcode::CALL || ins.opcode == IROpcode::LIB_CALL
                        || ins.opcode == IROpcode::GEN_CREATE)
                        && !ins.typedOperands.empty()) {
                    const auto& r = ins.typedOperands[0];
                    if (r.kind == IRRef::Kind::VAR && r.id >= 0) {
                        std::string callee = prog.symbols.getName(r.id);
                        if (prog.findFunction(callee))
                            calledFnsPre.insert(callee);
                    }
                    // Arguments that are function names passed as values → mark as called
                    for (size_t ai = 1; ai < ins.typedOperands.size(); ++ai) {
                        const auto& arg = ins.typedOperands[ai];
                        if (arg.kind == IRRef::Kind::VAR && arg.id >= 0) {
                            std::string argName = prog.symbols.getName(arg.id);
                            if (prog.findFunction(argName))
                                calledFnsPre.insert(argName);
                        }
                    }
                }
                for (const auto& op : ins.typedOperands) {
                    if (op.kind == IRRef::Kind::VAR && op.id >= 0)
                        readVarsPre.insert(prog.symbols.getName(op.id));
                }
                if (ins.opcode == IROpcode::STORE_VAR) {
                    std::string v;
                    if (ins.typedOperands.size() >= 2 && ins.typedOperands[0].kind == IRRef::Kind::VAR)
                        v = prog.symbols.getName(ins.typedOperands[0].id);
                    else if (ins.result.isValid() && ins.result.kind == IRRef::Kind::VAR)
                        v = prog.symbols.getName(ins.result.id);
                    if (!v.empty() && v[0] != '_') writtenVarsPre.insert(v);
                }
            }
        };
        scanPre(prog.globalInit);
        for (const auto& fn : prog.functions) scanPre(fn.instructions);
        // Remove written vars that ARE read (they appear in both sets)
        for (auto it = writtenVarsPre.begin(); it != writtenVarsPre.end(); ) {
            if (readVarsPre.count(*it)) it = writtenVarsPre.erase(it);
            else ++it;
        }
        // But we only warn if the write-only var is NOT a function parameter
        // (function params appear in readVarsPre if they're used inside the function)
    }

    // Rewrite math-constant-named VARs (math.pi → CONST) ONCE up front: variable names never
    // change and folding never introduces new math-constant VARs, so re-running it each pass was
    // pure wasted string work. The folding below still cascades on the substituted constants.
    for (auto& fn : prog.functions) runMathConstantRefs(fn.instructions, prog);
    runMathConstantRefs(prog.globalInit, prog);

    // ── -O level gating — TCC on the low end, GCC at the top ─────────────────
    // Design: -O0..-O3 optimize for COMPILE SPEED (TCC-style — every level stays fast to build);
    // -O4 optimizes for RUNTIME SPEED (GCC-style — pay compile time to make the binary fast). So the
    // one *expensive* transform, constexpr folding (inline pure fns + evaluate whole constant calls
    // like fib(35) at compile time), lives ONLY at -O4; the low levels do only cheap linear passes.
    //   -O0  none    : no folding/copy-prop. Fastest possible build.
    //   -O1  basic   : local constant folding (2+3→5), 1 pass. Cheap.
    //   -O2  standard: + copy-prop + DCE, 2 passes. (default — fast compile, tidy code)
    //   -O3  max-fast: same passes, 3 iterations — the best you get while STILL optimizing for compile time.
    //   -O4  runtime : GCC tier — turns on constexpr folding (8-pass cascade) and is the home for the
    //                  machine-level runtime optimizations (strength reduction, loop-var regalloc,
    //                  peephole). Slow compile, fast binary. BNY-only; text backends clamp to -O3
    //                  (they hand runtime optimization to gcc/rustc via the matching -O flag).
    int lvl = prog.optLevel;
    if (prog.backend != "BNY" && lvl > 3) lvl = 3;
    // constexpr folding AND copy-propagation both live at -O4. copy-prop is control-flow-unaware
    // (it can hoist a def across a branch) — safe in practice ONLY next to constexpr folding, which
    // reshapes the branchy IR first. Keeping both at -O4 makes the TCC tiers (-O1..-O3) do only the
    // cheap, provably-safe passes (local folding + DCE) → fast AND correct.
    const bool doConstexpr = (lvl >= 4);
    const bool doCopyProp  = (lvl >= 4);
    const int  passes      = (lvl <= 0) ? 0 : (lvl == 1) ? 1 : (lvl == 2) ? 2 : (lvl == 3) ? 3 : 8;
    for (int pass = 0; pass < passes; ++pass) {
        for (auto& fn : prog.functions) {
            runLocalConstFolding(fn.instructions, prog);
            if (doConstexpr) runConstexprFolding(fn.instructions, prog);
        }
        runLocalConstFolding(prog.globalInit, prog);
        if (doConstexpr) runConstexprFolding(prog.globalInit, prog);
        if (doCopyProp) {
            for (auto& fn : prog.functions) { runCopyProp(fn.instructions); runDCE(fn.instructions); }
            runCopyProp(prog.globalInit);
            runDCE(prog.globalInit);
        }
    }
    // Smart `/` MUST become a concrete DIV/FDIV before any backend sees it — a CORRECTNESS pass,
    // run once at every level including -O0. Likewise a final DCE keeps strict backends (Go rejects
    // unused vars) compiling even at -O0.
    for (auto& fn : prog.functions) { resolveDivisions(fn.instructions); runDCE(fn.instructions); }
    resolveDivisions(prog.globalInit);
    runDCE(prog.globalInit);

    // Dead-const-store elimination (PROGRAM-WIDE) — run ONCE after folding converges. After folding,
    // a var whose reads were all constant-propagated leaves `x = stv <const>` stores that no one
    // reads. The dead-store set is monotonic and removing write-only const stores can't affect any
    // earlier fold/copy-prop, so a single pass here is identical to running it every fold pass. Go
    // hard-errors on unused vars, and every backend wins by dropping them. A var counts as READ if
    // any instruction references it as a non-target VAR operand, a FREE_DECL names it, or its name
    // appears inside any CONST STRING (legacy text-expression paths evaluate strings verbatim).
    {
            std::set<std::string> readNames;
            auto scanReads = [&](const std::vector<IRInstruction>& code) {
                for (const auto& ins : code) {
                    size_t skipTarget = (ins.opcode == IROpcode::STORE_VAR && ins.typedOperands.size() >= 2) ? 0 : SIZE_MAX;
                    for (size_t k = 0; k < ins.typedOperands.size(); k++) {
                        const auto& op = ins.typedOperands[k];
                        if (ins.opcode == IROpcode::STORE_VAR && k == skipTarget) continue;
                        if (op.kind == IRRef::Kind::VAR && op.id >= 0)
                            readNames.insert(prog.symbols.getName(op.id));
                        if (op.kind == IRRef::Kind::CONST && op.value.type == IRType::STRING) {
                            // legacy guard: any identifier-looking word in a string const is a read
                            const std::string& txt = std::get<std::string>(op.value.data);
                            std::string w;
                            for (size_t ci = 0; ci <= txt.size(); ci++) {
                                char c = ci < txt.size() ? txt[ci] : ' ';
                                if (std::isalnum((unsigned char)c) || c == '_') w += c;
                                else { if (!w.empty() && !std::isdigit((unsigned char)w[0])) readNames.insert(w); w.clear(); }
                            }
                        }
                    }
                    if (ins.opcode == IROpcode::FREE_DECL && ins.result.kind == IRRef::Kind::VAR && ins.result.id >= 0)
                        readNames.insert(prog.symbols.getName(ins.result.id));
                }
            };
            scanReads(prog.globalInit);
            for (auto& fn : prog.functions) scanReads(fn.instructions);
            auto dropDead = [&](std::vector<IRInstruction>& code) {
                code.erase(std::remove_if(code.begin(), code.end(), [&](const IRInstruction& ins) {
                    if (ins.opcode != IROpcode::STORE_VAR && ins.opcode != IROpcode::LOAD_CONST) return false;
                    // target var + const value, in either operand form
                    const IRRef* tgt = nullptr; const IRRef* val = nullptr;
                    if (ins.typedOperands.size() >= 2) { tgt = &ins.typedOperands[0]; val = &ins.typedOperands[1]; }
                    else if (ins.result.isValid() && !ins.typedOperands.empty()) { tgt = &ins.result; val = &ins.typedOperands[0]; }
                    if (!tgt || !val || tgt->kind != IRRef::Kind::VAR || tgt->id < 0) return false;
                    if (val->kind != IRRef::Kind::CONST) return false;
                    // Bundle fields (self.x inside a method) are read through a DIFFERENT var
                    // name once instantiated (instance.x outside the method) — this pass only
                    // tracks read names textually, so it can never see that aliasing. Treating
                    // "self.*" stores as dead here silently dropped every bundle field
                    // initializer (found via `bundle X / x = default` producing an instance
                    // with unset fields). Never eliminate them.
                    //
                    // The SAME aliasing problem applies to ANY dotted field write on a NAMED
                    // instance, not just literally "self." — `p.x = 5` (from OUTSIDE a method,
                    // on any bundle-holding var) is read back as `q.x` once assigned to a
                    // DIFFERENT variable (`q = f()` where f constructs+returns p) or even just
                    // read back through `p.x` itself if nothing textually happens to reuse that
                    // exact string elsewhere — this pass has no way to know. Was scoped to only
                    // "self." before — verified real bug found on BNY (the one backend that
                    // actually acts on this DCE, not just warns): `p.x = 5; ...; return p` then
                    // `q = f(); Term.display q.x` silently dropped the `p.x = 5` store entirely,
                    // printing 0. Any dotted VAR name is a field write; exempt all of them.
                    const std::string& tgtName = prog.symbols.getName(tgt->id);
                    if (tgtName.find('.') != std::string::npos) return false;
                    return readNames.count(tgtName) == 0;
                }), code.end());
            };
            dropDead(prog.globalInit);
            for (auto& fn : prog.functions) dropDead(fn.instructions);
    }

    // Toxic warnings: unused variables, functions (skip in AC->LIB)
    if (prog.backend != "LIB") {
        // Variables written but never read (collected pre-opt)
        for (const auto& v : writtenVarsPre)
            std::cerr << Toxic::slackingOffUnread(v) << "\n";

        // Functions defined but never called (use pre-opt call set)
        for (const auto& fn : prog.functions) {
            if (fn.name.empty() || fn.name[0] == '_') continue;
            if (fn.name == "main" || fn.name == "mainloop") continue;
            if (!calledFnsPre.count(fn.name))
                std::cerr << Toxic::slackingOffUncalled(fn.name) << "\n";
        }
    }
}

// ─── public API ─────────────────────────────────────────────────────────────

// Inject auto-cleanup calls into __ac_shutoff__ for known imported libraries.
// Also creates the function if it doesn't exist but auto-cleanup is needed.
static void injectAutoShutoff(IRProgram& prog) {
    bool needsMaudio = prog.importedLibs.count("machine-audio") ||
                       prog.importedLibs.count("maudio");
    if (!needsMaudio) return;

    // Find or create __ac_shutoff__
    IRFunction* fn = prog.findFunction("__ac_shutoff__");
    bool created = false;
    if (!fn) {
        prog.functions.push_back(IRFunction("__ac_shutoff__"));
        fn = &prog.functions.back();
        created = true;
        prog.hasShutoff = true;
    }

    // Find insertion point: before first RETURN, or at end
    auto& instrs = fn->instructions;
    size_t insertAt = instrs.size();
    for (size_t k = 0; k < instrs.size(); k++) {
        if (instrs[k].opcode == IROpcode::RETURN) { insertAt = k; break; }
    }

    // machine-audio → maudio.stop()  (LIB_CALL so each backend formats it natively)
    if (needsMaudio) {
        IRInstruction callStop(IROpcode::LIB_CALL);
        callStop.typedOperands.push_back(IRRef::constant(IRValue(std::string("maudio.stop"))));
        instrs.insert(instrs.begin() + (long)insertAt, std::move(callStop));
        insertAt++;
    }

    if (created)
        instrs.push_back(IRInstruction(IROpcode::RETURN));
}

// Wrap globalInit in a once-only restart loop.
// Semantics: on the first pass /restart sets the flag and continues from the top;
// on the second pass /restart is skipped (flag already set) and execution falls through.
static void wrapWithRestartLoop(IRProgram& prog) {
    int rdoneId = prog.symbols.intern("_ac_restart_done", IRType::BOOL);
    int& ntc    = prog.globalTempCount;
    int& nlc    = prog.globalLabelCount;

    // Separate library import instructions (they stay outside the loop)
    std::vector<IRInstruction> imports, body;
    for (auto& instr : prog.globalInit) {
        bool isImport = (instr.opcode == IROpcode::LIB_CALL &&
                         !instr.typedOperands.empty() &&
                         instr.typedOperands[0].kind == IRRef::Kind::CONST &&
                         instr.typedOperands[0].value.type == IRType::STRING &&
                         std::get<std::string>(instr.typedOperands[0].value.data) == "import");
        if (isImport) imports.push_back(instr);
        else          body.push_back(instr);
    }

    std::vector<IRInstruction> out;

    // Imports first — outside the restart loop
    for (auto& i : imports) out.push_back(i);

    // _ac_restart_done = false
    {
        IRInstruction st(IROpcode::STORE_VAR);
        st.result = IRRef::var(rdoneId);
        st.typedOperands.push_back(IRRef::constant(IRValue(false)));
        out.push_back(std::move(st));
    }

    // WHILE_BEGIN (infinite loop — exited by /kill, /stop, or the trailing break)
    out.push_back(IRInstruction(IROpcode::WHILE_BEGIN));

    for (auto& instr : body) {
        if (instr.opcode != IROpcode::RESTART_PROGRAM) {
            out.push_back(instr);
            continue;
        }

        // Expand /restart:
        //   t0 = _ac_restart_done
        //   t1 = not t0
        //   IF_BEGIN t1
        //       _ac_restart_done = true
        //       JUMP __continue__
        //   IF_END
        int t0 = ntc++;
        int t1 = ntc++;
        (void)nlc; // labels not needed here — IF_BEGIN/END handle the block structure

        // t0 = _ac_restart_done
        {
            IRInstruction ld(IROpcode::LOAD_VAR);
            ld.result = IRRef::temp(t0);
            ld.typedOperands.push_back(IRRef::var(rdoneId));
            out.push_back(std::move(ld));
        }
        // t1 = NOT t0
        {
            IRInstruction ni(IROpcode::NOT);
            ni.result = IRRef::temp(t1);
            ni.typedOperands.push_back(IRRef::temp(t0));
            out.push_back(std::move(ni));
        }
        // IF_BEGIN t1
        {
            IRInstruction ifB(IROpcode::IF_BEGIN);
            ifB.typedOperands.push_back(IRRef::temp(t1));
            out.push_back(std::move(ifB));
        }
        // _ac_restart_done = true
        {
            IRInstruction st(IROpcode::STORE_VAR);
            st.result = IRRef::var(rdoneId);
            st.typedOperands.push_back(IRRef::constant(IRValue(true)));
            out.push_back(std::move(st));
        }
        // JUMP __continue__
        {
            IRInstruction jc(IROpcode::JUMP);
            jc.typedOperands.push_back(IRRef::constant(IRValue(std::string("__continue__"))));
            out.push_back(std::move(jc));
        }
        // IF_END
        out.push_back(IRInstruction(IROpcode::IF_END));
    }

    // JUMP __break__ — natural end of program breaks out of the restart loop
    {
        IRInstruction jb(IROpcode::JUMP);
        jb.typedOperands.push_back(IRRef::constant(IRValue(std::string("__break__"))));
        out.push_back(std::move(jb));
    }

    // WHILE_END
    out.push_back(IRInstruction(IROpcode::WHILE_END));

    prog.globalInit = std::move(out);
}

// Cheap gate for the two-pass tuple-parameter-shape discovery below (see
// IRGenerator::tupleParamClasses_'s own comment) — a full discovery pass costs a whole extra
// lowering of the program, so skip it entirely for the overwhelming majority of programs that
// never construct a tuple at all. A false positive (TupleLiteral present but no tuple ever
// actually crosses a call boundary as a parameter) just costs one wasted discovery pass, not
// incorrectness — discoverTupleParamShapes() naturally produces an empty map in that case.
static bool astHasTupleLiteral(const ASTNode& n) {
    if (n.type == NodeType::TupleLiteral) return true;
    for (auto& c : n.children) if (c && astHasTupleLiteral(*c)) return true;
    return false;
}

IRProgram generateIR(const ASTNode& ast, const std::string& backend, bool runtimeMode, int optLevel) {
    IRGenerator g;
    if (astHasTupleLiteral(ast)) {
        // Discovery pass: lower once (throwaway output) purely to learn which functions'
        // parameters ever receive a tuple argument — impossible to know up front, since a
        // tuple's synthesized class name is only known once its own binding site is lowered,
        // and FuncDefs are lowered in written order, almost always before the call sites that
        // invoke them. See tupleParamClasses_'s own comment for the full rationale.
        IRGenerator discover;
        discover.generate(ast, backend);
        auto tupleParams = discover.getTupleParamClasses();
        if (!tupleParams.empty()) {
            g.setTupleParamClasses(std::move(tupleParams));
            g.mergeTupleClassRegistry(discover.getTupleClassSlotTypes(), discover.getTupleAnyClasses());
        }
    }
    auto prog = g.generate(ast, backend);
    prog.optLevel = optLevel;
    if (!runtimeMode) runOptPasses(prog);
    else {
        // --runtime: skip constexpr but still run copy-prop/DCE and toxic warnings
        // Toxic warnings (pre-opt, same as normal)
        if (prog.backend != "LIB") {
            std::set<std::string> calledFnsPre;
            std::set<std::string> writtenVarsPre, readVarsPre;
            auto scan = [&](const std::vector<IRInstruction>& instrs) {
                for (const auto& ins : instrs) {
                    if ((ins.opcode == IROpcode::CALL || ins.opcode == IROpcode::LIB_CALL
                            || ins.opcode == IROpcode::GEN_CREATE)
                            && !ins.typedOperands.empty()) {
                        const auto& r = ins.typedOperands[0];
                        if (r.kind == IRRef::Kind::VAR && r.id >= 0) {
                            std::string callee = prog.symbols.getName(r.id);
                            if (prog.findFunction(callee))
                                calledFnsPre.insert(callee);
                        }
                        // Function names passed as arguments → mark as used
                        for (size_t ai = 1; ai < ins.typedOperands.size(); ++ai) {
                            const auto& arg = ins.typedOperands[ai];
                            if (arg.kind == IRRef::Kind::VAR && arg.id >= 0) {
                                std::string argName = prog.symbols.getName(arg.id);
                                if (prog.findFunction(argName))
                                    calledFnsPre.insert(argName);
                            }
                        }
                    }
                    for (const auto& op : ins.typedOperands)
                        if (op.kind == IRRef::Kind::VAR && op.id >= 0)
                            readVarsPre.insert(prog.symbols.getName(op.id));
                    if (ins.opcode == IROpcode::STORE_VAR) {
                        std::string v;
                        if (ins.typedOperands.size() >= 2 && ins.typedOperands[0].kind == IRRef::Kind::VAR)
                            v = prog.symbols.getName(ins.typedOperands[0].id);
                        else if (ins.result.isValid() && ins.result.kind == IRRef::Kind::VAR)
                            v = prog.symbols.getName(ins.result.id);
                        if (!v.empty() && v[0] != '_') writtenVarsPre.insert(v);
                    }
                }
            };
            scan(prog.globalInit);
            for (const auto& fn : prog.functions) scan(fn.instructions);
            for (auto it = writtenVarsPre.begin(); it != writtenVarsPre.end(); )
                if (readVarsPre.count(*it)) it = writtenVarsPre.erase(it); else ++it;
            for (const auto& v : writtenVarsPre)
                std::cerr << Toxic::slackingOffUnread(v) << "\n";
            for (const auto& fn : prog.functions) {
                if (fn.name.empty() || fn.name[0] == '_') continue;
                if (fn.name == "main" || fn.name == "mainloop") continue;
                if (!calledFnsPre.count(fn.name))
                    std::cerr << Toxic::slackingOffUncalled(fn.name) << "\n";
            }
        }
        for (auto& fn : prog.functions) { runCopyProp(fn.instructions); runDCE(fn.instructions); }
        runCopyProp(prog.globalInit); runDCE(prog.globalInit);
    }
    injectAutoShutoff(prog);
    expandAliases(prog);
    if (prog.hasRestart)
        wrapWithRestartLoop(prog);
    return prog;
}

// ─── LIR text serialiser ────────────────────────────────────────────────────

static std::string refStr(const IRRef& r, SymbolTable* symbols = nullptr) {
    if (symbols) {
        return r.toStringWithSymbols(symbols);
    }
    return r.toString();
}

static std::string instrToLIR(const IRInstruction& i, SymbolTable* symbols = nullptr) {
    std::ostringstream o;

    if (i.result.isValid())
        o << refStr(i.result, symbols) << " = ";

    o << opcodeStr(i.opcode);

    if (!i.typedOperands.empty()) {
        o << ' ';
        for (size_t k = 0; k < i.typedOperands.size(); k++) {
            if (k) o << ", ";
            o << refStr(i.typedOperands[k], symbols);
        }
    } else if (!i.operands.empty()) {
        // legacy string operands fallback
        o << ' ';
        for (size_t k = 0; k < i.operands.size(); k++) {
            if (k) o << ", ";
            o << i.operands[k];
        }
    }

    if (!i.comment.empty()) o << "  ; " << i.comment;
    return o.str();
}

std::string generateIRText(const IRProgram& program) {
    std::ostringstream o;
    o << "; AC LIR  backend=" << program.backend << "\n";
    o << "; Symbol Table: " << program.symbols.size() << " symbols\n";
    o << "; Arena: " << program.arena.totalUsed() << " / " << program.arena.totalAllocated() 
      << " bytes used (" << program.arena.numBlocks() << " blocks)\n\n";

    if (!program.globalInit.empty()) {
        o << "section .global:\n";
        for (auto& i : program.globalInit)
            o << "  " << instrToLIR(i, const_cast<SymbolTable*>(&program.symbols)) << '\n';
        o << '\n';
    }

    for (auto& fn : program.functions) {
        o << "fn " << fn.name << '(';
        for (size_t k = 0; k < fn.parameters.size(); k++) {
            if (k) o << ", ";
            o << fn.parameters[k];
        }
        o << ") -> " << typeStr(fn.returnType) << " {\n";
        for (auto& i : fn.instructions)
            o << "  " << instrToLIR(i, const_cast<SymbolTable*>(&program.symbols)) << '\n';
        o << "}\n\n";
    }

    return o.str();
}

} // namespace AC_IR
