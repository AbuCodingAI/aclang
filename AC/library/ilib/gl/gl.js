// AC ilib: gl — HTML/JS/CSS runtime
// Included by AC-compiled HTML output.
// <OBJECT> block → HTML + JS object setup
// <SCREEN> block → CSS container + canvas
// <LOGIC>  block → JS event listeners + requestAnimationFrame game loop
//
// use ilib gl  (HTML backend)

(function(global) {

// ── Internal state ────────────────────────────────────────────────────────

const _objs   = {};   // name → { x, y, w, h, r, g, b, vx, vy, speed, dir, curveshape, vertex_x, vertex_y }
const _draws  = {};   // name → { curveshape, vertex_x, vertex_y, cmds: [{kind,x1,y1,x2,y2,radius,r,g,b}] }
const _held   = {};   // key → bool (held)
const _just   = {};   // key → bool (just pressed this frame)
let   _canvas = null;
let   _ctx    = null;
let   _bg     = { r:0, g:0, b:0 };
let   _fps    = 60;
let   _alive  = false;
let   _t_last = 0;
let   _raf    = null;

// ── Colour constants ──────────────────────────────────────────────────────

const COLORS = {
    black:   [0,0,0],
    white:   [255,255,255],
    red:     [255,0,0],
    green:   [0,200,0],
    blue:    [0,0,255],
    yellow:  [255,255,0],
    cyan:    [0,255,255],
    magenta: [255,0,255],
};

function _rgb(r, g, b) { return `rgb(${r},${g},${b})`; }
function _col(name) {
    const c = COLORS[name.toLowerCase()];
    return c ? _rgb(...c) : name;
}

// ── Key normalisation ─────────────────────────────────────────────────────

function _norm(e) {
    const map = {
        'ArrowUp':'up','ArrowDown':'down','ArrowLeft':'left','ArrowRight':'right',
        ' ':'space','Enter':'enter','Escape':'escape',
    };
    return (map[e.key] || e.key.toLowerCase());
}

// ── Screen ────────────────────────────────────────────────────────────────

function screen_create(w, h, title) {
    _canvas = document.getElementById('ac-gl-canvas');
    if (!_canvas) {
        _canvas = document.createElement('canvas');
        _canvas.id = 'ac-gl-canvas';
        document.body.appendChild(_canvas);
    }
    _canvas.width  = w;
    _canvas.height = h;
    _canvas.style.display = 'block';
    _canvas.style.margin  = '0 auto';
    _ctx = _canvas.getContext('2d');
    if (title) document.title = title;
    _alive  = true;
    _t_last = performance.now();

    document.addEventListener('keydown', e => {
        const k = _norm(e);
        if (!_held[k]) _just[k] = true;
        _held[k] = true;
        e.preventDefault();
    });
    document.addEventListener('keyup', e => {
        _held[_norm(e)] = false;
    });
}

function screen_set_bg(r, g, b) { _bg = {r, g, b}; }
function screen_set_fps(fps) { _fps = fps > 0 ? fps : 60; }
function screen_w() { return _canvas ? _canvas.width  : 0; }
function screen_h() { return _canvas ? _canvas.height : 0; }

// ── CSS helpers (used by <SCREEN> compiler output) ────────────────────────
// Generates a <style> block for the game container and canvas.

function screen_css(w, h, bg_color) {
    return `
body { margin:0; background:#000; display:flex; justify-content:center; align-items:center; height:100vh; }
#ac-gl-canvas { width:${w}px; height:${h}px; background:${_col(bg_color)}; display:block; }
`.trim();
}

// ── Objects ───────────────────────────────────────────────────────────────

// ── Expression evaluator for curveshape ──────────────────────────────────

function _curve_eval(expr, x) {
    // Replace 'x' with the numeric value, then eval safely via Function
    try {
        const fn = new Function('x', 'Math', `return (${expr});`);
        return fn(x, Math);
    } catch(_) { return 0; }
}

// ── Objects ───────────────────────────────────────────────────────────────

function obj_create(name) {
    _objs[name] = { x:0, y:0, w:50, h:50, r:255, g:255, b:255, vx:0, vy:0, speed:0, dir:0, curveshape:'', vertex_x:0, vertex_y:0 };
}
function obj_geometry(name, w, h) { if(_objs[name]){_objs[name].w=w;_objs[name].h=h;} }
function obj_square(name, size)   { obj_geometry(name, size, size); }
function obj_pos(name, x, y)      { if(_objs[name]){_objs[name].x=x;_objs[name].y=y;} }
function obj_color(name, r, g, b) { if(_objs[name]){_objs[name].r=r;_objs[name].g=g;_objs[name].b=b;} }
function obj_color_name(name, col){ const c=COLORS[col.toLowerCase()]||[255,255,255]; obj_color(name,...c); }

function obj_velocity(name, vx, vy) {
    if(!_objs[name]) return;
    _objs[name].vx=vx; _objs[name].vy=vy;
    _objs[name].speed=Math.hypot(vx,vy);
}
function obj_set_speed(name, speed) {
    if(!_objs[name]) return;
    _objs[name].speed=speed;
    _update_vel(_objs[name]);
}
function obj_set_direction(name, deg) {
    if(!_objs[name]) return;
    _objs[name].dir=deg;
    _update_vel(_objs[name]);
}
function obj_speed_mult(name, mult) {
    if(!_objs[name]) return;
    _objs[name].vx    *= mult;
    _objs[name].vy    *= mult;
    _objs[name].speed  = Math.abs(_objs[name].speed * mult);
}
function obj_move_x(name, dx) { if(_objs[name]) _objs[name].x+=dx; }
function obj_move_y(name, dy) { if(_objs[name]) _objs[name].y+=dy; }
function obj_x(name) { return _objs[name]?_objs[name].x:0; }
function obj_y(name) { return _objs[name]?_objs[name].y:0; }
function obj_w(name) { return _objs[name]?_objs[name].w:0; }
function obj_h(name) { return _objs[name]?_objs[name].h:0; }
function obj_curveshape(name, expr)  { if(_objs[name]) _objs[name].curveshape=expr; }
function obj_vertex(name, vx, vy)    { if(_objs[name]){_objs[name].vertex_x=vx;_objs[name].vertex_y=vy;} }
function obj_to_draw(name)           { if(_objs[name]){_draws[name]={curveshape:'',vertex_x:0,vertex_y:0,cmds:[]};delete _objs[name];} }

function obj_circle_fall(name, fraction, dir) {
    const o = _objs[name]; if(!o || o.cf_active || o.cf_done) return;
    o.cf_active   = true;
    o.cf_done     = false;
    o.cf_fraction = fraction;
    o.cf_angle    = 0;
    o.cf_pivot_x  = o.x + o.w * 0.5;
    o.cf_pivot_y  = o.y + o.h;
    o.cf_radius   = o.h * 0.5;
}
function obj_circle_fell(name) { return !!(_objs[name] && _objs[name].cf_done); }

function obj_set_spawn(name) {
    const o = _objs[name]; if(!o) return;
    o.spawn_x=o.x; o.spawn_y=o.y; o.spawn_vx=o.vx; o.spawn_vy=o.vy; o.has_spawn=true;
}
function obj_regen(name) {
    const o = _objs[name]; if(!o || !o.has_spawn) return;
    o.x=o.spawn_x; o.y=o.spawn_y; o.vx=o.spawn_vx; o.vy=o.spawn_vy;
    o.speed=Math.hypot(o.vx,o.vy); o.cf_active=false; o.cf_done=false; o.cf_angle=0;
}
function obj_animate(name, dir, speed) {
    const o = _objs[name]; if(!o) return;
    if(dir==='right')     { o.vx= speed; o.vy=0; }
    else if(dir==='left') { o.vx=-speed; o.vy=0; }
    else if(dir==='up')   { o.vx=0; o.vy=-speed; }
    else if(dir==='down') { o.vx=0; o.vy= speed; }
    o.speed=speed;
}
function hitbox_many_overlap() {
    const names = Object.keys(_objs);
    let pairs = 0;
    for(let i=0;i<names.length;i++)
        for(let j=i+1;j<names.length;j++)
            if(_aabb(_objs[names[i]],_objs[names[j]]) && ++pairs>=2) return true;
    return false;
}

// ── Draw type ─────────────────────────────────────────────────────────────

function draw_create(name)               { _draws[name] = {curveshape:'',vertex_x:0,vertex_y:0,cmds:[]}; }
function draw_curveshape(name, expr)     { if(_draws[name]) _draws[name].curveshape=expr; }
function draw_vertex(name, vx, vy)       { if(_draws[name]){_draws[name].vertex_x=vx;_draws[name].vertex_y=vy;} }
function draw_line(name,x1,y1,x2,y2,r,g,b) {
    if(_draws[name]) _draws[name].cmds.push({kind:'line',x1,y1,x2,y2,r,g,b});
}
function draw_circle(name,cx,cy,radius,r,g,b) {
    if(_draws[name]) _draws[name].cmds.push({kind:'circle',cx,cy,radius,r,g,b});
}
function draw_clear(name)                { if(_draws[name]){_draws[name].cmds=[];_draws[name].curveshape='';} }
function draw_to_obj(name)               { if(_draws[name]){_objs[name]={x:0,y:0,w:50,h:50,r:255,g:255,b:255,vx:0,vy:0,speed:0,dir:0,curveshape:'',vertex_x:0,vertex_y:0};delete _draws[name];} }
function is_draw(name)                   { return !!_draws[name]; }
function is_obj(name)                    { return !!_objs[name]; }

function _update_vel(o) {
    const rad = o.dir * Math.PI / 180;
    o.vx =  o.speed * Math.sin(rad);
    o.vy = -o.speed * Math.cos(rad);
}

// ── Collision ─────────────────────────────────────────────────────────────

function _aabb(a, b) {
    return !(a.x+a.w<=b.x || b.x+b.w<=a.x || a.y+a.h<=b.y || b.y+b.h<=a.y);
}
function _wmatch(name, pat) {
    if (pat.endsWith('%')) return name.startsWith(pat.slice(0,-1));
    return name === pat;
}

function hitbox_overlap(a, b) {
    return !!(a && b && _objs[a] && _objs[b] && _aabb(_objs[a],_objs[b]));
}
function hitbox_boundary(name) {
    const o=_objs[name]; if(!o) return false;
    return o.x<=0 || o.x+o.w>=screen_w() || o.y<=0 || o.y+o.h>=screen_h();
}
function hitbox_overlap_pattern(name, pat) {
    const A=_objs[name]; if(!A) return false;
    for(const [bname,B] of Object.entries(_objs)) {
        if(bname===name) continue;
        if(_wmatch(bname,pat) && _aabb(A,B)) return true;
    }
    return false;
}

// ── Input ─────────────────────────────────────────────────────────────────

function key_pressed(k)      { return !!_held[k.toLowerCase()]; }
function key_just_pressed(k) { return !!_just[k.toLowerCase()]; }

// ── Frame loop ────────────────────────────────────────────────────────────

function delta_time() {
    const now = performance.now();
    const dt  = Math.min((now - _t_last) / 1000, 0.1);
    return dt;
}

function frame_update(dt) {
    for(const o of Object.values(_objs)) {
        if(o.cf_active) {
            const target = o.cf_fraction * Math.PI / 2;
            o.cf_angle += Math.PI * dt;
            if(o.cf_angle >= target) { o.cf_angle=target; o.cf_active=false; o.cf_done=true; }
            const cx = o.cf_pivot_x + o.cf_radius * Math.sin(o.cf_angle);
            const cy = o.cf_pivot_y - o.cf_radius * Math.cos(o.cf_angle);
            o.x = cx - o.w * 0.5;
            o.y = cy - o.h * 0.5;
        } else {
            o.x += o.vx * dt;
            o.y += o.vy * dt;
        }
    }
}

function _draw_circle_ctx(ctx, cx, cy, radius, r, g, b) {
    ctx.beginPath();
    ctx.arc(cx, cy, radius, 0, Math.PI * 2);
    ctx.strokeStyle = _rgb(r, g, b);
    ctx.stroke();
}

function _render_curve(ctx, expr, vx, vy, color) {
    const N = 300;
    const range = screen_w() * 0.5;
    const step  = range * 2 / N;
    ctx.beginPath();
    ctx.strokeStyle = color;
    let first = true;
    for (let i = 0; i <= N; i++) {
        const xoff = -range + step * i;
        const yval = _curve_eval(expr, xoff);
        const sx = vx + xoff;
        const sy = vy - yval;
        if (sx < 0 || sx > screen_w() || sy < 0 || sy > screen_h()) { first = true; continue; }
        if (first) { ctx.moveTo(sx, sy); first = false; }
        else ctx.lineTo(sx, sy);
    }
    ctx.stroke();
}

function frame_render() {
    if(!_ctx) return;
    _ctx.fillStyle = _rgb(_bg.r, _bg.g, _bg.b);
    _ctx.fillRect(0, 0, screen_w(), screen_h());

    // Objects
    for(const o of Object.values(_objs)) {
        _ctx.fillStyle = _rgb(o.r, o.g, o.b);
        _ctx.fillRect(o.x, o.y, o.w, o.h);
        // trajectory ghost curve
        if (o.curveshape) {
            _render_curve(_ctx, o.curveshape, o.vertex_x, o.vertex_y, 'rgba(200,200,200,0.6)');
        }
    }

    // Draw entities
    for(const d of Object.values(_draws)) {
        for(const cmd of d.cmds) {
            if(cmd.kind === 'line') {
                _ctx.beginPath();
                _ctx.strokeStyle = _rgb(cmd.r, cmd.g, cmd.b);
                _ctx.moveTo(cmd.x1, cmd.y1);
                _ctx.lineTo(cmd.x2, cmd.y2);
                _ctx.stroke();
            } else if(cmd.kind === 'circle') {
                _draw_circle_ctx(_ctx, cmd.cx, cmd.cy, cmd.radius, cmd.r, cmd.g, cmd.b);
            }
        }
        if(d.curveshape) {
            _render_curve(_ctx, d.curveshape, d.vertex_x, d.vertex_y, 'rgb(255,255,255)');
        }
    }
}

function frame_end() {
    for(const k of Object.keys(_just)) _just[k] = false;
    _t_last = performance.now();
}

// ── Game loop driver ──────────────────────────────────────────────────────
// Called by <StartHere>...<EndHere> compiler output.
// Pass your logic function; it receives dt and runs every frame.

function start_loop(logic_fn) {
    function tick() {
        if(!_alive) return;
        const dt = delta_time();
        frame_update(dt);
        logic_fn(dt);
        frame_render();
        frame_end();
        _raf = requestAnimationFrame(tick);
    }
    _raf = requestAnimationFrame(tick);
}

function stop_loop() {
    _alive = false;
    if(_raf) cancelAnimationFrame(_raf);
}

// ── Public namespace — matches the C .so API names ────────────────────────

const gl = {
    init()  { return true; },
    quit()  { stop_loop(); },

    screen: {
        create:   screen_create,
        set_bg:   screen_set_bg,
        set_fps:  screen_set_fps,
        w:        screen_w,
        h:        screen_h,
        css:      screen_css,
    },
    obj: {
        create:        obj_create,
        geometry:      obj_geometry,
        square:        obj_square,
        pos:           obj_pos,
        color:         obj_color,
        color_name:    obj_color_name,
        velocity:      obj_velocity,
        set_speed:     obj_set_speed,
        set_direction: obj_set_direction,
        speed_mult:    obj_speed_mult,
        move_x:        obj_move_x,
        move_y:        obj_move_y,
        x: obj_x, y: obj_y, w: obj_w, h: obj_h,
        curveshape:    obj_curveshape,
        vertex:        obj_vertex,
        to_draw:       obj_to_draw,
        circle_fall:   obj_circle_fall,
        circle_fell:   obj_circle_fell,
        set_spawn:     obj_set_spawn,
        regen:         obj_regen,
        animate:       obj_animate,
    },
    draw: {
        create:     draw_create,
        curveshape: draw_curveshape,
        vertex:     draw_vertex,
        line:       draw_line,
        circle:     draw_circle,
        clear:      draw_clear,
        to_obj:     draw_to_obj,
    },
    hitbox: {
        overlap:         hitbox_overlap,
        boundary:        hitbox_boundary,
        overlap_pattern: hitbox_overlap_pattern,
        many_overlap:    hitbox_many_overlap,
    },
    key: {
        pressed:      key_pressed,
        just_pressed: key_just_pressed,
    },
    frame: {
        update: frame_update,
        render: frame_render,
        end:    frame_end,
        delta:  delta_time,
    },
    loop: {
        start: start_loop,
        stop:  stop_loop,
    },
    colors: COLORS,
    is_draw: is_draw,
    is_obj:  is_obj,
};

global.gl = gl;

})(typeof window !== 'undefined' ? window : global);
