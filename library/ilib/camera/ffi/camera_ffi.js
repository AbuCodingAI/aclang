const _ffi=require('ffi-napi'),_path=require('path');
const _lib=_path.join(process.cwd(),'library','camera',process.platform==='win32'?'libaccamera.dll':'libaccamera.so');
const _CS='string',_I='int',_V='void';
const _c=_ffi.Library(_lib,{
'ac_camera_init':[_I,[]],'ac_camera_capture':[_I,[_CS]],'ac_camera_capture_latest':[_I,[_CS]],
'ac_camera_capture_first':[_I,[_CS]],'ac_camera_release':[_V,[]],
'ac_sidebar_config':[_V,[_CS,_CS]],'ac_sidebar_setregion':[_V,[_CS]],
'ac_sidebar_setinteractive':[_V,[_I]],'ac_sidebar_display':[_V,[_CS]],
'ac_sidebar_ask':[_CS,[_CS]],'ac_sidebar_getinput':[_CS,[]],
'ac_screen_setmode':[_V,[_CS]],'ac_screen_update':[_V,[]],
});
const camera={
    init:()=>_c.ac_camera_init(),
    capture:f=>_c.ac_camera_capture(f),
    capture_latest:f=>_c.ac_camera_capture_latest(f),
    capture_first:f=>_c.ac_camera_capture_first(f),
    release:()=>_c.ac_camera_release(),
};
const sidebar={
    config:(k,v)=>_c.ac_sidebar_config(k,v),
    setregion:r=>_c.ac_sidebar_setregion(r),
    setinteractive:v=>_c.ac_sidebar_setinteractive(v?1:0),
    display:m=>_c.ac_sidebar_display(m),
    ask:p=>_c.ac_sidebar_ask(p)||'',
    getinput:()=>_c.ac_sidebar_getinput()||'',
};
const screen={
    setmode:m=>_c.ac_screen_setmode(m),
    update:()=>_c.ac_screen_update(),
};
