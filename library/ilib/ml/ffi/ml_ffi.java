class ml {
    private static class Node {
        double value;
        double grad;
        boolean track;
        String op;
        long a;
        long b;

        Node(double value, String op, long a, long b) {
            this.value = value;
            this.op = op;
            this.a = a;
            this.b = b;
        }
    }

    private static long nextId = 1;
    private static final java.util.Map<Long, Node> nodes = new java.util.HashMap<>();

    private static long put(double value, String op, long a, long b) {
        long id = nextId++;
        nodes.put(id, new Node(value, op, a, b));
        return id;
    }

    private static Node node(long id) { return nodes.get(id); }

    private static void back(long id, double upstream) {
        Node n = node(id);
        if (n == null) return;
        n.grad += upstream;
        if ("add".equals(n.op)) {
            back(n.a, upstream);
            back(n.b, upstream);
        } else if ("mul".equals(n.op)) {
            back(n.a, upstream * take(n.b));
            back(n.b, upstream * take(n.a));
        } else if ("relu".equals(n.op) && take(n.a) > 0.0) {
            back(n.a, upstream);
        }
    }

    static long tensor(double value) { return put(value, "", 0, 0); }
    static long grid(long rows, long cols, double fill) { return put(fill, "", 0, 0); }
    static long gradient_track(long tensorId, long enabled) {
        Node n = node(tensorId);
        if (n == null) return 0;
        n.track = enabled != 0;
        return 1;
    }
    static long grad_wipe(long tensorId) {
        Node n = node(tensorId);
        if (n == null) return 0;
        n.grad = 0.0;
        return 1;
    }
    static long weights(double learningRate, long tensorId) {
        Node n = node(tensorId);
        if (n == null) return 0;
        n.value -= learningRate * n.grad;
        return 1;
    }
    static double take(long tensorId) {
        Node n = node(tensorId);
        return n == null ? 0.0 : n.value;
    }
    static long add(long a, long b) { return put(take(a) + take(b), "add", a, b); }
    static long multiply(long a, long b) { return put(take(a) * take(b), "mul", a, b); }
    static long relu(long x) { return put(Math.max(0.0, take(x)), "relu", x, 0); }
    static long backward(long loss) {
        if (!nodes.containsKey(loss)) return 0;
        back(loss, 1.0);
        return 1;
    }
}
