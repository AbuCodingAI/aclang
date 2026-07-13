// LibACML.java — Java JNI bindings for AC Machine Learning Library
// Used by AC->Java backend
// Build: javac LibACML.java && javah -jni LibACML

public class LibACML {
    // Load native library
    static {
        System.loadLibrary("acml_jni");
    }

    // ========================================================================
    // NATIVE METHODS (implemented in C/C++ with JNI)
    // ========================================================================

    // Tensor creation
    public static native long ptTensorFromData(
        double[] data, long dataLen,
        long[] shape, long shapeLen,
        int dtype
    );

    public static native long ptRandn(long[] shape, long shapeLen, int dtype);
    public static native long ptZeros(long[] shape, long shapeLen, int dtype);
    public static native long ptOnes(long[] shape, long shapeLen, int dtype);

    // Tensor queries
    public static native long ptTensorShape(long tensorId, long[] shapeOut, long shapeLen);
    public static native int ptTensorDtype(long tensorId);
    public static native long ptTensorToArray(long tensorId, double[] out, long outLen);

    // Operations
    public static native long ptMatmul(long a, long b);
    public static native long ptAdd(long a, long b);
    public static native long ptMultiply(long a, long b);
    public static native long ptRelu(long x);
    public static native long ptSoftmax(long x, long dim);
    public static native long ptReshape(long x, long[] newShape, long shapeLen);
    public static native long ptToDevice(long x, long device);

    // Cleanup
    public static native void ptTensorFree(long x);

    // TensorFlow API
    public static native long tfConstant(
        double[] data, long dataLen,
        long[] shape, long shapeLen,
        int dtype
    );

    public static native long tfMatmul(long a, long b);
    public static native long tfNnRelu(long x);
    public static native long tfNnSoftmax(long x, long axis);
    public static native long tfLossesSparseCategoricalCrossentropy(long yTrue, long yPred);

    // Gradients & Optimization
    public static native int mlBackward(long loss);
    public static native long mlGetGrad(long tensorId);
    public static native int mlSgdStep(double learningRate, long tensorId);
    public static native int mlAdamStep(
        double learningRate,
        double beta1,
        double beta2,
        double epsilon,
        long tensorId
    );

    // Models
    public static native long mlModelNew();
    public static native long mlModelAddDense(long modelId, long inFeatures, long outFeatures);
    public static native long mlModelForward(long modelId, long x);
    public static native void mlModelFree(long modelId);

    // Utility
    public static native int mlInit();
    public static native void mlCleanup();
    public static native String mlVersion();

    // ========================================================================
    // JAVA WRAPPER CLASS
    // ========================================================================

    public static class Tensor {
        private long id;

        public Tensor(long id) {
            this.id = id;
        }

        public static Tensor fromArray(double[] data, long... shape) {
            long id = ptTensorFromData(data, data.length, shape, shape.length, 1); // FLOAT32
            return new Tensor(id);
        }

        public static Tensor randn(long... shape) {
            long id = ptRandn(shape, shape.length, 1); // FLOAT32
            return new Tensor(id);
        }

        public static Tensor zeros(long... shape) {
            long id = ptZeros(shape, shape.length, 1); // FLOAT32
            return new Tensor(id);
        }

        public static Tensor ones(long... shape) {
            long id = ptOnes(shape, shape.length, 1); // FLOAT32
            return new Tensor(id);
        }

        public long[] shape() {
            long[] shape = new long[8];
            long len = ptTensorShape(id, shape, 8);
            long[] result = new long[(int)len];
            System.arraycopy(shape, 0, result, 0, (int)len);
            return result;
        }

        public double[] toArray() {
            long[] shape = shape();
            long size = 1;
            for (long dim : shape) {
                size *= dim;
            }
            double[] data = new double[(int)size];
            ptTensorToArray(id, data, size);
            return data;
        }

        public Tensor matmul(Tensor other) {
            return new Tensor(ptMatmul(this.id, other.id));
        }

        public Tensor add(Tensor other) {
            return new Tensor(ptAdd(this.id, other.id));
        }

        public Tensor multiply(Tensor other) {
            return new Tensor(ptMultiply(this.id, other.id));
        }

        public Tensor relu() {
            return new Tensor(ptRelu(this.id));
        }

        public Tensor softmax(long dim) {
            return new Tensor(ptSoftmax(this.id, dim));
        }

        public Tensor reshape(long... newShape) {
            return new Tensor(ptReshape(this.id, newShape, newShape.length));
        }

        public Tensor toDevice(long device) {
            return new Tensor(ptToDevice(this.id, device));
        }

        public void free() {
            ptTensorFree(this.id);
        }

        public long getId() {
            return id;
        }
    }

    public static class Model {
        private long id;

        public Model() {
            this.id = mlModelNew();
        }

        public long addDenseLayer(long inFeatures, long outFeatures) {
            return mlModelAddDense(this.id, inFeatures, outFeatures);
        }

        public Tensor forward(Tensor x) {
            return new Tensor(mlModelForward(this.id, x.getId()));
        }

        public void free() {
            mlModelFree(this.id);
        }

        public long getId() {
            return id;
        }
    }

    // ========================================================================
    // STATIC UTILITIES
    // ========================================================================

    public static void backward(Tensor loss) throws RuntimeException {
        int result = mlBackward(loss.getId());
        if (result != 0) {
            throw new RuntimeException("Backward pass failed: " + result);
        }
    }

    public static Tensor getGrad(Tensor tensor) {
        return new Tensor(mlGetGrad(tensor.getId()));
    }

    public static void sgdStep(double learningRate, Tensor tensor) throws RuntimeException {
        int result = mlSgdStep(learningRate, tensor.getId());
        if (result != 0) {
            throw new RuntimeException("SGD step failed: " + result);
        }
    }

    public static void adamStep(
        double learningRate,
        double beta1,
        double beta2,
        double epsilon,
        Tensor tensor
    ) throws RuntimeException {
        int result = mlAdamStep(learningRate, beta1, beta2, epsilon, tensor.getId());
        if (result != 0) {
            throw new RuntimeException("Adam step failed: " + result);
        }
    }

    public static void init() throws RuntimeException {
        int result = mlInit();
        if (result != 0) {
            throw new RuntimeException("ML init failed: " + result);
        }
    }

    public static void cleanup() {
        mlCleanup();
    }

    public static String version() {
        return mlVersion();
    }
}
