#pragma once
#include <string>
#include <vector>
#include <unordered_map>
#include <fstream>
#include <sstream>
#include "../include/ir.hpp"

namespace AC_IR {

// Library lowering pass
// Reads a .acl spec file and rewrites gl:* LIB_CALL instructions into
// canonical ac_gl_* calls that the FFI files and codegens already understand.
//
// Naming convention fallback (no explicit rule needed):
//   gl:X.Y  ->  ac_gl_X_Y   (strip "gl:", replace every '.' with '_', prepend "ac_gl_")
//
// Explicit .acl rules override the fallback for exceptions.
class LibLowering {
public:
    bool load(const std::string& path) {
        std::ifstream f(path);
        if (!f) return false;
        std::string line;
        while (std::getline(f, line)) {
            auto hash = line.find('#');
            if (hash != std::string::npos) line = line.substr(0, hash);
            trim(line);
            if (line.empty()) continue;
            auto arrow = line.find(" -> ");
            if (arrow == std::string::npos) continue;
            std::string src = line.substr(0, arrow);
            std::string tgt = line.substr(arrow + 4);
            trim(src); trim(tgt);
            if (!src.empty() && !tgt.empty())
                rules_[src] = tgt;
        }
        return true;
    }

    void apply(IRProgram& prog) const {
        lowerList(prog.globalInit);
        for (auto& fn : prog.functions)
            lowerList(fn.instructions);
    }

private:
    std::unordered_map<std::string, std::string> rules_;

    static void trim(std::string& s) {
        while (!s.empty() && (s.back()==' '||s.back()=='\t'||s.back()=='\r')) s.pop_back();
        size_t i = 0;
        while (i < s.size() && (s[i]==' '||s[i]=='\t')) ++i;
        if (i) s.erase(0, i);
    }

    static std::string getConstStr(const IRRef& r) {
        if (r.kind == IRRef::Kind::CONST && r.value.type == IRType::STRING)
            return std::get<std::string>(r.value.data);
        return "";
    }

    static IRRef mkConst(const std::string& s) { return IRRef::constant(IRValue(s)); }

    static IRInstruction makeCall(const std::string& fn, const std::vector<IRRef>& args,
                                  const IRRef& result = IRRef()) {
        IRInstruction i(IROpcode::LIB_CALL);
        i.result = result;
        i.typedOperands.push_back(mkConst(fn));
        for (auto& a : args) i.typedOperands.push_back(a);
        return i;
    }

    // Naming convention fallback:
    //   gl:X.Y        ->  ac_gl_X_Y
    //   camera:X.Y    ->  ac_camera_X_Y
    //   regex:X.Y     ->  ac_regex_X_Y
    //   widgets:X.Y   ->  ac_widgets_X_Y
    //   maudio:X.Y    ->  ac_maudio_X_Y
    //   math:X.Y      ->  ac_math_X_Y  (math has irregular C names; prefer explicit rules)
    static std::string fallback(const std::string& fname) {
        auto colon = fname.find(':');
        if (colon == std::string::npos) return fname;
        std::string lib  = fname.substr(0, colon);
        std::string rest = fname.substr(colon + 1);
        for (char& c : rest) if (c == '.') c = '_';
        return "ac_" + lib + "_" + rest;
    }

    void lowerList(std::vector<IRInstruction>& instrs) const {
        std::vector<IRInstruction> out;
        out.reserve(instrs.size());
        for (auto& instr : instrs) {
            if (instr.opcode != IROpcode::LIB_CALL || instr.typedOperands.empty()) {
                out.push_back(std::move(instr));
                continue;
            }
            std::string fname = getConstStr(instr.typedOperands[0]);
            if (fname.find(':') == std::string::npos) {
                out.push_back(std::move(instr));
                continue;
            }

            // Args = operands[1..]
            std::vector<IRRef> args(instr.typedOperands.begin() + 1, instr.typedOperands.end());

            auto it = rules_.find(fname);
            std::string target = (it != rules_.end()) ? it->second : fallback(fname);

            if (target == "expand:config-spec") {
                expandConfigSpec(args, instr.result, out);
            } else {
                out.push_back(makeCall(target, args, instr.result));
            }
        }
        instrs = std::move(out);
    }

    // Expand gl:obj.config into individual ac_gl_obj_* calls.
    // args[0] = object name ref
    // args[1..] = config spec parts: "item=X", "location=(x=A, y=B)", "color=C", "bare-color"
    void expandConfigSpec(const std::vector<IRRef>& args, const IRRef& /*result*/,
                          std::vector<IRInstruction>& out) const {
        if (args.empty()) return;
        const IRRef& objRef = args[0];
        std::string objName = getConstStr(objRef);

        for (size_t i = 1; i < args.size(); i++) {
            std::string spec = getConstStr(args[i]);
            if (spec.empty()) continue;

            if (spec.rfind("item=", 0) == 0) {
                out.push_back(makeCall("ac_gl_obj_config_item",
                    {objRef, mkConst(spec.substr(5))}));
            } else if (spec.rfind("location=(", 0) == 0) {
                std::string loc = spec.substr(9);
                while (!loc.empty() && (loc.front()=='('||loc.front()==' ')) loc.erase(loc.begin());
                while (!loc.empty() && (loc.back()==')'||loc.back()==' '))   loc.pop_back();
                std::string xv = "0", yv = "0";
                std::istringstream ss(loc);
                std::string part;
                while (std::getline(ss, part, ',')) {
                    trim(part);
                    if (part.rfind("x=",0)==0) xv = part.substr(2);
                    else if (part.rfind("y=",0)==0) yv = part.substr(2);
                }
                out.push_back(makeCall("ac_gl_obj_pos_from_spec",
                    {objRef, mkConst(xv), mkConst(yv)}));
            } else if (spec.rfind("color=", 0) == 0) {
                std::string color = spec.substr(6);
                if (objName == "background")
                    out.push_back(makeCall("ac_gl_screen_set_bg_by_name", {mkConst(color)}));
                else
                    out.push_back(makeCall("ac_gl_obj_color_by_name", {objRef, mkConst(color)}));
            } else if (spec.find('=') == std::string::npos) {
                // Bare color name: "white", "brown", etc.
                if (objName == "background")
                    out.push_back(makeCall("ac_gl_screen_set_bg_by_name", {mkConst(spec)}));
                else
                    out.push_back(makeCall("ac_gl_obj_color_by_name", {objRef, mkConst(spec)}));
            }
        }
    }
};

} // namespace AC_IR
