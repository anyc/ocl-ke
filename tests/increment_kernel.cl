__kernel void increment(__global int* a, __global int* b, int by, unsigned int length) {
  int i = get_global_id(0);
  if(i < length)
       b[i] = a[i] + by;
}
