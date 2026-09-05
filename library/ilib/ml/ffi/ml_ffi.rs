#[link(name = "acml")]
extern "C" {
    fn ml_tensor(value: f64) -> i64;
    fn ml_grid(rows: i64, cols: i64, fill: f64) -> i64;
    fn ml_gradient_track(tensor_id: i64, enabled: i32) -> i32;
    fn ml_grad_wipe(tensor_id: i64) -> i32;
    fn ml_weights(learning_rate: f64, tensor_id: i64) -> i32;
    fn ml_take(tensor_id: i64) -> f64;
    fn ml_add(a: i64, b: i64) -> i64;
    fn ml_multiply(a: i64, b: i64) -> i64;
    fn ml_relu(x: i64) -> i64;
    fn ml_backward(loss: i64) -> i32;
    fn ml_grad(tensor_id: i64) -> i64;
    fn ml_optimize(learning_rate: f64, tensor_id: i64) -> i32;
}

pub struct Ml;
pub static ml: Ml = Ml;

pub trait MlScalar { fn to_f64(self) -> f64; }
impl MlScalar for i64 { fn to_f64(self) -> f64 { self as f64 } }
impl MlScalar for f64 { fn to_f64(self) -> f64 { self } }

impl Ml {
    pub fn tensor<T: MlScalar>(&self, value: T) -> i64 { unsafe { ml_tensor(value.to_f64()) } }
    pub fn grid<T: MlScalar>(&self, rows: i64, cols: i64, fill: T) -> i64 { unsafe { ml_grid(rows, cols, fill.to_f64()) } }
    pub fn gradient_track(&self, tensor_id: i64, enabled: i64) -> i64 { unsafe { ml_gradient_track(tensor_id, enabled as i32) as i64 } }
    pub fn grad_wipe(&self, tensor_id: i64) -> i64 { unsafe { ml_grad_wipe(tensor_id) as i64 } }
    pub fn weights(&self, learning_rate: f64, tensor_id: i64) -> i64 { unsafe { ml_weights(learning_rate, tensor_id) as i64 } }
    pub fn take(&self, tensor_id: i64) -> f64 { unsafe { ml_take(tensor_id) } }
    pub fn add(&self, a: i64, b: i64) -> i64 { unsafe { ml_add(a, b) } }
    pub fn multiply(&self, a: i64, b: i64) -> i64 { unsafe { ml_multiply(a, b) } }
    pub fn relu(&self, x: i64) -> i64 { unsafe { ml_relu(x) } }
    pub fn backward(&self, loss: i64) -> i64 { unsafe { ml_backward(loss) as i64 } }
    pub fn grad(&self, tensor_id: i64) -> i64 { unsafe { ml_grad(tensor_id) } }
    pub fn optimize(&self, learning_rate: f64, tensor_id: i64) -> i64 { unsafe { ml_optimize(learning_rate, tensor_id) as i64 } }
}
