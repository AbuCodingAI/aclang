const ml = (() => {
  let nextId = 1;
  const nodes = new Map();
  const put = (value, op = '', a = 0, b = 0) => {
    const id = nextId++;
    nodes.set(id, { value: Number(value), grad: 0, track: false, op, a, b });
    return id;
  };
  const node = (id) => nodes.get(Number(id));
  const take = (id) => node(id)?.value ?? 0;
  const addGrad = (id, g) => { const n = node(id); if (n) n.grad += g; };
  const back = (id, upstream) => {
    const n = node(id);
    if (!n) return;
    n.grad += upstream;
    if (n.op === 'add') {
      back(n.a, upstream);
      back(n.b, upstream);
    } else if (n.op === 'mul') {
      back(n.a, upstream * take(n.b));
      back(n.b, upstream * take(n.a));
    } else if (n.op === 'relu' && take(n.a) > 0) {
      back(n.a, upstream);
    }
  };
  return {
    tensor(value) { return put(value); },
    grid(rows, cols, fill) { return put(fill); },
    gradient_track(tensorId, enabled) { const n = node(tensorId); if (!n) return 0; n.track = Number(enabled) !== 0; return 1; },
    grad_wipe(tensorId) { const n = node(tensorId); if (!n) return 0; n.grad = 0; return 1; },
    weights(learningRate, tensorId) { const n = node(tensorId); if (!n) return 0; n.value -= Number(learningRate) * n.grad; return 1; },
    take,
    add(a, b) { return put(take(a) + take(b), 'add', Number(a), Number(b)); },
    multiply(a, b) { return put(take(a) * take(b), 'mul', Number(a), Number(b)); },
    relu(x) { return put(Math.max(0, take(x)), 'relu', Number(x), 0); },
    backward(loss) { if (!node(loss)) return 0; back(Number(loss), 1); return 1; },
  };
})();
