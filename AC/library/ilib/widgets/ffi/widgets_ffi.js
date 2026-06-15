// AC ilib: widgets — JavaScript FFI (ffi-napi → libacwidgets.so / acwidgets.dll)
// Inlined by AC->JS compiler when "use ilib widgets" is declared
// Requires: npm install ffi-napi ref-napi
const _ffi=require('ffi-napi'),_path=require('path');
const _lib=_path.join(
    typeof _ac_widgets_lib_dir!=='undefined' ? _ac_widgets_lib_dir
        : _path.join(process.cwd(),'library','ilib','widgets'),
    process.platform==='win32'?'acwidgets':'libacwidgets');
const _W='int64',_I='int',_D='double',_U8='uint8',_V='void',_S='string';

const _m=_ffi.Library(_lib,{
    'ac_widgets_init':            [_V,[]],'ac_widgets_screen_new':      [_W,[_S,_S]],
    'ac_widgets_screen_mainloop': [_V,[_W]],'ac_widgets_screen_update':  [_V,[_W]],
    'ac_widgets_screen_destroy':  [_V,[_W]],
    'ac_widgets_display_new':     [_W,[_W,_S]],'ac_widgets_display_pack': [_V,[_W]],
    'ac_widgets_display_set':     [_V,[_W,_S]],'ac_widgets_display_get':  [_S,[_W]],
    'ac_widgets_ask_new':         [_W,[_W,_I]],'ac_widgets_ask_pack':     [_V,[_W]],
    'ac_widgets_ask_get':         [_S,[_W]],'ac_widgets_ask_set':         [_V,[_W,_S]],
    'ac_widgets_btn_new':         [_W,[_W,_S]],'ac_widgets_btn_pack':     [_V,[_W]],
    'ac_widgets_btn_on_click':    [_V,[_W,'pointer','pointer']],
    'ac_widgets_ckbtn_new':       [_W,[_W,_S]],'ac_widgets_ckbtn_pack':   [_V,[_W]],
    'ac_widgets_ckbtn_get':       [_I,[_W]],'ac_widgets_ckbtn_set':       [_V,[_W,_I]],
    'ac_widgets_dropdown_new':    [_W,[_W]],'ac_widgets_dropdown_pack':   [_V,[_W]],
    'ac_widgets_dropdown_add':    [_V,[_W,_S]],'ac_widgets_dropdown_get':  [_S,[_W]],
    'ac_widgets_dropdown_set':    [_V,[_W,_S]],
    'ac_widgets_advance_new':     [_W,[_W,_I]],'ac_widgets_advance_pack':  [_V,[_W]],
    'ac_widgets_advance_set':     [_V,[_W,_D]],'ac_widgets_advance_get':   [_D,[_W]],
    'ac_widgets_slider_new':      [_W,[_W,_D,_D,_S]],'ac_widgets_slider_pack':  [_V,[_W]],
    'ac_widgets_slider_get':      [_D,[_W]],'ac_widgets_slider_set':       [_V,[_W,_D]],
    'ac_widgets_group_new':       [_W,[_W,_S]],'ac_widgets_group_pack':    [_V,[_W]],
    'ac_widgets_listbox_new':     [_W,[_W,_I,_I]],'ac_widgets_listbox_pack':[_V,[_W]],
    'ac_widgets_listbox_add':     [_V,[_W,_S]],'ac_widgets_listbox_item':  [_S,[_W,_I]],
    'ac_widgets_listbox_count':   [_I,[_W]],
    'ac_widgets_sketch_new':      [_W,[_W,_I,_I]],'ac_widgets_sketch_pack': [_V,[_W]],
    'ac_widgets_sketch_clear':    [_V,[_W]],
    'ac_widgets_sketch_line':     [_V,[_W,_D,_D,_D,_D,_U8,_U8,_U8]],
    'ac_widgets_sketch_rect':     [_V,[_W,_D,_D,_D,_D,_U8,_U8,_U8]],
    'ac_widgets_sketch_circle':   [_V,[_W,_D,_D,_D,_U8,_U8,_U8]],
    'ac_widgets_sketch_text':     [_V,[_W,_D,_D,_S,_U8,_U8,_U8]],
    'ac_widgets_set_lazy':        [_V,[_W]],
    'ac_widgets_pack_spaced':     [_V,[_W,_I,_I]],
});

_m.ac_widgets_init();

const _cbs = [];
var lazy = "lazy";  // pass as last arg to any widget to defer auto-pack

function _autoOrLazy(h, packFn, lz) {
    if (lz === "lazy") _m.ac_widgets_set_lazy(h);
    else packFn(h);
}
function _packOrSpaced(h, packFn, sx, sy) {
    if (sx || sy) _m.ac_widgets_pack_spaced(h, sx|0, sy|0);
    else packFn(h);
}
/* strip lazy sentinel from any arg position — returns {isLazy, rest[]} */
function _stripLazy(...args) {
    const isLazy = args.some(a => a === "lazy");
    return { isLazy, rest: args.filter(a => a !== "lazy") };
}

// Factory functions — callable without `new`, positional args matching AC codegen output
function Screen(title='AC App', geometry='800x600') {
    const o={_h:_m.ac_widgets_screen_new(String(title),String(geometry))};
    o.mainloop=()=>_m.ac_widgets_screen_mainloop(o._h);
    o.update=()=>_m.ac_widgets_screen_update(o._h);
    o.destroy=()=>_m.ac_widgets_screen_destroy(o._h);
    return o;
}
function display(master,text='',lz=null) {
    const o={_h:_m.ac_widgets_display_new(master._h,String(text))};
    _autoOrLazy(o._h,_m.ac_widgets_display_pack,lz);
    o.pack=(sx=0,sy=0)=>_packOrSpaced(o._h,_m.ac_widgets_display_pack,sx,sy);
    o.set=(v)=>_m.ac_widgets_display_set(o._h,String(v));
    o.get=()=>_m.ac_widgets_display_get(o._h);
    o.config=(opts)=>{if(opts&&opts.text!==undefined)o.set(opts.text);};
    return o;
}
function ask(master,width=20,lz=null) {
    const {isLazy,rest}=_stripLazy(width); if(isLazy){lz="lazy";width=rest[0]??20;}
    const o={_h:_m.ac_widgets_ask_new(master._h,parseInt(width)||20)};
    _autoOrLazy(o._h,_m.ac_widgets_ask_pack,lz);
    o.pack=(sx=0,sy=0)=>_packOrSpaced(o._h,_m.ac_widgets_ask_pack,sx,sy);
    o.get=()=>_m.ac_widgets_ask_get(o._h);
    o.set=(v)=>_m.ac_widgets_ask_set(o._h,String(v));
    return o;
}
function btn(master,text='Button',cmd=null,lz=null) {
    const o={_h:_m.ac_widgets_btn_new(master._h,String(text))};
    o.on_click=(cb)=>{
        const wrapped=_ffi.Callback(_V,['pointer'],()=>cb());
        _cbs.push(wrapped);
        _m.ac_widgets_btn_on_click(o._h,wrapped,null);
    };
    if(cmd)o.on_click(cmd);
    _autoOrLazy(o._h,_m.ac_widgets_btn_pack,lz);
    o.pack=(sx=0,sy=0)=>_packOrSpaced(o._h,_m.ac_widgets_btn_pack,sx,sy);
    return o;
}
function ckbtn(master,text='',lz=null) {
    const o={_h:_m.ac_widgets_ckbtn_new(master._h,String(text))};
    _autoOrLazy(o._h,_m.ac_widgets_ckbtn_pack,lz);
    o.pack=(sx=0,sy=0)=>_packOrSpaced(o._h,_m.ac_widgets_ckbtn_pack,sx,sy);
    o.get=()=>_m.ac_widgets_ckbtn_get(o._h)!==0;
    o.set=(v)=>_m.ac_widgets_ckbtn_set(o._h,v?1:0);
    return o;
}
function radbtn(master,text='',lz=null) {
    const o={_h:_m.ac_widgets_ckbtn_new(master._h,String(text))};
    _autoOrLazy(o._h,_m.ac_widgets_ckbtn_pack,lz);
    o.pack=(sx=0,sy=0)=>_packOrSpaced(o._h,_m.ac_widgets_ckbtn_pack,sx,sy);
    o.get=()=>_m.ac_widgets_ckbtn_get(o._h)!==0;
    return o;
}
function dropdown(master,values='',lz=null) {
    const {isLazy,rest}=_stripLazy(values); if(isLazy){lz="lazy";values=rest[0]??'';}
    const o={_h:_m.ac_widgets_dropdown_new(master._h)};
    const vals=typeof values==='string'?(values?values.split(','):[]):(values||[]);
    vals.forEach(v=>_m.ac_widgets_dropdown_add(o._h,v.trim()));
    _autoOrLazy(o._h,_m.ac_widgets_dropdown_pack,lz);
    o.pack=(sx=0,sy=0)=>_packOrSpaced(o._h,_m.ac_widgets_dropdown_pack,sx,sy);
    o.add=(v)=>_m.ac_widgets_dropdown_add(o._h,String(v));
    o.get=()=>_m.ac_widgets_dropdown_get(o._h);
    o.set=(v)=>_m.ac_widgets_dropdown_set(o._h,String(v));
    return o;
}
function advance(master,length=200,lz=null) {
    const {isLazy,rest}=_stripLazy(length); if(isLazy){lz="lazy";length=rest[0]??200;}
    const o={_h:_m.ac_widgets_advance_new(master._h,parseInt(length)||200)};
    _autoOrLazy(o._h,_m.ac_widgets_advance_pack,lz);
    o.pack=(sx=0,sy=0)=>_packOrSpaced(o._h,_m.ac_widgets_advance_pack,sx,sy);
    o.set=(v)=>_m.ac_widgets_advance_set(o._h,parseFloat(v));
    o.get=()=>_m.ac_widgets_advance_get(o._h);
    return o;
}
function slider(master,from_val=0,to_val=100,orient='horizontal',lz=null) {
    const {isLazy,rest}=_stripLazy(from_val,to_val,orient);
    if(isLazy){lz="lazy";[from_val,to_val,orient]=[rest[0]??0,rest[1]??100,rest[2]??'horizontal'];}
    const o={_h:_m.ac_widgets_slider_new(master._h,parseFloat(from_val),parseFloat(to_val),String(orient))};
    _autoOrLazy(o._h,_m.ac_widgets_slider_pack,lz);
    o.pack=(sx=0,sy=0)=>_packOrSpaced(o._h,_m.ac_widgets_slider_pack,sx,sy);
    o.get=()=>_m.ac_widgets_slider_get(o._h);
    o.set=(v)=>_m.ac_widgets_slider_set(o._h,parseFloat(v));
    return o;
}
function group(master,text='',lz=null) {
    const o={_h:_m.ac_widgets_group_new(master._h,String(text))};
    _autoOrLazy(o._h,_m.ac_widgets_group_pack,lz);
    o.pack=(sx=0,sy=0)=>_packOrSpaced(o._h,_m.ac_widgets_group_pack,sx,sy);
    return o;
}
function tabs(master,lz=null) {
    const o={_h:_m.ac_widgets_group_new(master._h,'')};
    _autoOrLazy(o._h,_m.ac_widgets_group_pack,lz);
    o.pack=(sx=0,sy=0)=>_packOrSpaced(o._h,_m.ac_widgets_group_pack,sx,sy);
    o.add_tab=(name)=>o;
    return o;
}
function scroller(master,lz=null) {
    const o={_h:_m.ac_widgets_group_new(master._h,'')};
    _autoOrLazy(o._h,_m.ac_widgets_group_pack,lz);
    o.pack=(sx=0,sy=0)=>_packOrSpaced(o._h,_m.ac_widgets_group_pack,sx,sy);
    return o;
}
function table(master,lz=null) {
    const o={_h:_m.ac_widgets_listbox_new(master._h,40,10)};
    _autoOrLazy(o._h,_m.ac_widgets_listbox_pack,lz);
    o.pack=(sx=0,sy=0)=>_packOrSpaced(o._h,_m.ac_widgets_listbox_pack,sx,sy);
    o.add=(row)=>_m.ac_widgets_listbox_add(o._h,String(row));
    return o;
}
function listbox(master,width=30,height=5,lz=null) {
    const o={_h:_m.ac_widgets_listbox_new(master._h,parseInt(width),parseInt(height))};
    _autoOrLazy(o._h,_m.ac_widgets_listbox_pack,lz);
    o.pack=(sx=0,sy=0)=>_packOrSpaced(o._h,_m.ac_widgets_listbox_pack,sx,sy);
    o.add=(item)=>_m.ac_widgets_listbox_add(o._h,String(item));
    o.get=()=>{const n=_m.ac_widgets_listbox_count(o._h);return Array.from({length:n},(_,i)=>_m.ac_widgets_listbox_item(o._h,i));};
    return o;
}
function sketch(master,width=400,height=300,lz=null) {
    const o={_h:_m.ac_widgets_sketch_new(master._h,parseInt(width),parseInt(height))};
    _autoOrLazy(o._h,_m.ac_widgets_sketch_pack,lz);
    o.pack=(sx=0,sy=0)=>_packOrSpaced(o._h,_m.ac_widgets_sketch_pack,sx,sy);
    o.clear=()=>_m.ac_widgets_sketch_clear(o._h);
    o.line=(x1,y1,x2,y2,r=0,g=0,b=0)=>_m.ac_widgets_sketch_line(o._h,x1,y1,x2,y2,r,g,b);
    o.rect=(x1,y1,x2,y2,r=0,g=0,b=0)=>_m.ac_widgets_sketch_rect(o._h,x1,y1,x2,y2,r,g,b);
    o.circle=(cx,cy,rad,r=0,g=0,b=0)=>_m.ac_widgets_sketch_circle(o._h,cx,cy,rad,r,g,b);
    o.text=(x,y,t,r=0,g=0,b=0)=>_m.ac_widgets_sketch_text(o._h,x,y,String(t),r,g,b);
    return o;
}
