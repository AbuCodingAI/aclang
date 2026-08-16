; AC ilib: widgets — x86-64 NASM extern declarations (libacwidgets.so)
; Link: nasm -f elf64 output.asm && gcc output.o -L<libdir> -lacwidgets -Wl,-rpath,<libdir> -o output
;
; Previously declared raw `gtk_*` C API symbols directly, which nothing in this
; file (or any AC-generated call site) ever actually called — hollow. Real fix:
; extern the .so's OWN `ac_widgets_*` functions, matching every other ilib's ASM
; FFI file (math, native-cpu, ...). Reachable from AC source via the *dotted*
; namespaced form (`widgets.screen.new(...)`, `widgets.tabs.add_tab(...)`, ...),
; which the compiler's generic ilib fallback rule (widgets:X.Y -> ac_widgets_X_Y,
; see widgets.acl) lowers straight to these exact names — no further compiler
; change needed for that path. The bare OOP-constructor form the example .ac
; files use (`Screen(...)`, `t.add_tab(...)`) does NOT resolve on ASM — that
; needs a bare-name compiler-side rewrite (the same fix native-cpu's Java gap
; needed), which is a separate, not-yet-done follow-up, not squeezed in here.

extern ac_widgets_init

extern ac_widgets_screen_new
extern ac_widgets_screen_mainloop
extern ac_widgets_screen_update
extern ac_widgets_screen_destroy

extern ac_widgets_display_new
extern ac_widgets_display_pack
extern ac_widgets_display_set
extern ac_widgets_display_get

extern ac_widgets_ask_new
extern ac_widgets_ask_pack
extern ac_widgets_ask_get
extern ac_widgets_ask_set

extern ac_widgets_btn_new
extern ac_widgets_btn_pack
extern ac_widgets_btn_on_click

extern ac_widgets_ckbtn_new
extern ac_widgets_ckbtn_pack
extern ac_widgets_ckbtn_get
extern ac_widgets_ckbtn_set

extern ac_widgets_dropdown_new
extern ac_widgets_dropdown_pack
extern ac_widgets_dropdown_add
extern ac_widgets_dropdown_get
extern ac_widgets_dropdown_set

extern ac_widgets_advance_new
extern ac_widgets_advance_pack
extern ac_widgets_advance_set
extern ac_widgets_advance_get

extern ac_widgets_slider_new
extern ac_widgets_slider_pack
extern ac_widgets_slider_get
extern ac_widgets_slider_set

extern ac_widgets_group_new
extern ac_widgets_group_pack

extern ac_widgets_tabs_new
extern ac_widgets_tabs_pack
extern ac_widgets_tabs_add_tab

extern ac_widgets_scroller_new
extern ac_widgets_scroller_pack

extern ac_widgets_listbox_new
extern ac_widgets_listbox_pack
extern ac_widgets_listbox_add
extern ac_widgets_listbox_item
extern ac_widgets_listbox_count

extern ac_widgets_table_new
extern ac_widgets_table_pack
extern ac_widgets_table_add
extern ac_widgets_table_row
extern ac_widgets_table_count

extern ac_widgets_sketch_new
extern ac_widgets_sketch_pack
extern ac_widgets_sketch_clear
extern ac_widgets_sketch_line
extern ac_widgets_sketch_rect
extern ac_widgets_sketch_circle
extern ac_widgets_sketch_text

extern ac_widgets_pack
extern ac_widgets_add
extern ac_widgets_set_d
extern ac_widgets_set_lazy
extern ac_widgets_pack_spaced

section .note.GNU-stack noalloc noexec nowrite
