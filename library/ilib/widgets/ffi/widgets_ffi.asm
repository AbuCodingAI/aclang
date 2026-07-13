; AC ilib: widgets — x86-64 NASM extern declarations (GTK3 C API)
; Link: nasm -f elf64 output.s && gcc output.o -o output $(pkg-config --libs gtk+-3.0)

section .text

; Init
; gtk_init(argc: rdi, argv: rsi) -> void
extern gtk_init
; gtk_main() -> void
extern gtk_main
; gtk_main_quit() -> void
extern gtk_main_quit
; gtk_widget_show_all(widget: rdi) -> void
extern gtk_widget_show_all
; gtk_widget_destroy(widget: rdi) -> void
extern gtk_widget_destroy
; gtk_widget_show(widget: rdi) -> void
extern gtk_widget_show

; Window
; gtk_window_new(type: edi) -> rax: GtkWidget*
extern gtk_window_new
; gtk_window_set_title(window: rdi, title: rsi) -> void
extern gtk_window_set_title
; gtk_window_set_default_size(window: rdi, width: esi, height: edx) -> void
extern gtk_window_set_default_size

; Box container
; gtk_box_new(orientation: edi, spacing: esi) -> rax: GtkWidget*
extern gtk_box_new
; gtk_box_pack_start(box: rdi, child: rsi, expand: edl, fill: ecl, padding: r8d) -> void
extern gtk_box_pack_start

; Label (display)
; gtk_label_new(str: rdi) -> rax: GtkWidget*
extern gtk_label_new
; gtk_label_set_text(label: rdi, str: rsi) -> void
extern gtk_label_set_text

; Entry (ask)
; gtk_entry_new() -> rax: GtkWidget*
extern gtk_entry_new
; gtk_entry_get_text(entry: rdi) -> rax: const char*
extern gtk_entry_get_text
; gtk_entry_set_text(entry: rdi, text: rsi) -> void
extern gtk_entry_set_text

; Button (btn)
; gtk_button_new_with_label(label: rdi) -> rax: GtkWidget*
extern gtk_button_new_with_label

; CheckButton (ckbtn)
; gtk_check_button_new_with_label(label: rdi) -> rax: GtkWidget*
extern gtk_check_button_new_with_label
; gtk_toggle_button_get_active(button: rdi) -> eax: gboolean
extern gtk_toggle_button_get_active
; gtk_toggle_button_set_active(button: rdi, is_active: esi) -> void
extern gtk_toggle_button_set_active

; ComboBoxText (dropdown)
; gtk_combo_box_text_new() -> rax: GtkWidget*
extern gtk_combo_box_text_new
; gtk_combo_box_text_append_text(combo_box: rdi, text: rsi) -> void
extern gtk_combo_box_text_append_text
; gtk_combo_box_text_get_active_text(combo_box: rdi) -> rax: gchar*
extern gtk_combo_box_text_get_active_text

; ProgressBar (advance)
; gtk_progress_bar_new() -> rax: GtkWidget*
extern gtk_progress_bar_new
; gtk_progress_bar_set_fraction(pbar: rdi, xmm0: double fraction) -> void
extern gtk_progress_bar_set_fraction
; gtk_progress_bar_get_fraction(pbar: rdi) -> xmm0: double
extern gtk_progress_bar_get_fraction

; Scale/Slider
; gtk_scale_new_with_range(orientation: edi, xmm0: double min, xmm1: double max, xmm2: double step) -> rax: GtkWidget*
extern gtk_scale_new_with_range
; gtk_range_get_value(range: rdi) -> xmm0: double
extern gtk_range_get_value
; gtk_range_set_value(range: rdi, xmm0: double value) -> void
extern gtk_range_set_value

; Frame (group)
; gtk_frame_new(label: rdi) -> rax: GtkWidget*
extern gtk_frame_new
; gtk_container_add(container: rdi, widget: rsi) -> void
extern gtk_container_add

; g_signal_connect_data(instance: rdi, ...) — for button callbacks
extern g_signal_connect_data
