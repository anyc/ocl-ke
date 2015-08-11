
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <CL/cl.h>

#define STRINGIFY(s) str(s)
#define str(s) #s

unsigned char * read_file(char *name, size_t *size) {
	FILE *f;
	char *result;
	
	f = fopen(name, "r");
	if (!f) {
		printf("cannot open file \"%s\"\n", name);
		exit(1);
	}
	
	fseek(f, 0L, SEEK_END);
	*size = ftell(f);
	fseek(f, 0L, SEEK_SET);
	
	result = (char*) malloc(*size);
	fread(result, *size, 1, f);
	
	fclose(f);
	
	return result;
}

int main(int argc, char *argv[]) {
	int *a, *b;
	int i, result=0;
	int by;
	size_t length;
	unsigned int intlength;
	size_t size;
	const unsigned char *bin;
	
	cl_context context;
	cl_platform_id platform;
	cl_uint n_platforms;
	cl_device_id device;
	cl_uint n_devices;
	cl_program program;
	cl_command_queue queue;
	cl_kernel kernel;
	cl_int err;
	cl_mem buf_a, buf_b;
	
	
	by = 1;
	length = 24;
	intlength = length;
	
	a = (int*) malloc(sizeof(int)*length);
	b = (int*) malloc(sizeof(int)*length);
	
	memset(a, 0, sizeof(int)*length);
	
	clGetPlatformIDs(1, &platform, &n_platforms);
	if (n_platforms < 1) {
		printf("error: no OpenCL platform found.\n");
		return 1;
	}
	
	clGetDeviceIDs(platform, CL_DEVICE_TYPE_ALL, 1, &device, &n_devices);
	
	context = clCreateContext(NULL, 1, &device, 0, NULL, &err);
	
	queue = clCreateCommandQueue(context, device, 0, &err);
	
	#ifndef KERNEL_FILE
	bin = read_file("increment_kernel.bin", &size);
	#else
	bin = read_file(STRINGIFY(KERNEL_FILE), &size);
	#endif
	
	program = clCreateProgramWithBinary(context, 1, &device, &size, &bin, 0, &err);
	if (clBuildProgram(program, 1, &device, "", NULL, NULL) != CL_SUCCESS) {
		char buffer[10240];
		clGetProgramBuildInfo(program, device, CL_PROGRAM_BUILD_LOG, sizeof(buffer), buffer, NULL);
		fprintf(stderr, "CL Compilation failed:\n%s", buffer);
		exit(1);
	}

	buf_a = clCreateBuffer(context, CL_MEM_READ_ONLY, sizeof(int)*length, NULL, &err);
	buf_b = clCreateBuffer(context, CL_MEM_WRITE_ONLY, sizeof(int)*length, NULL, &err);
	
	clEnqueueWriteBuffer(queue, buf_a, CL_TRUE, 0, sizeof(int)*length, a, 0, NULL, NULL);
	clEnqueueWriteBuffer(queue, buf_b, CL_TRUE, 0, sizeof(int)*length, b, 0, NULL, NULL);
	
	#ifndef KERNEL_NAME
	kernel = clCreateKernel(program, "increment", &err);
	#else
	kernel = clCreateKernel(program, STRINGIFY(KERNEL_NAME), &err);
	#endif
	
	err = 0;
	err  = clSetKernelArg(kernel, 0, sizeof(cl_mem), &buf_a);
	err |= clSetKernelArg(kernel, 1, sizeof(cl_mem), &buf_b);
	err |= clSetKernelArg(kernel, 2, sizeof(int), &by);
	err |= clSetKernelArg(kernel, 3, sizeof(unsigned int), &intlength);
	if (err != CL_SUCCESS) {
		printf("Error during parameter setup: %d\n", err);
		return 0;
	}
	
	err = clEnqueueNDRangeKernel(queue, kernel, 1, NULL, &length, 0, 0, NULL, NULL);
	
	if (err != CL_SUCCESS) {
		printf("Error during kernel execution: %d\n", err);
		exit(1);
	}
	
	clFinish(queue);
	
	clEnqueueReadBuffer(queue, buf_b, CL_TRUE, 0, sizeof(int)*length, b, 0, NULL, NULL);
	
	for (i=0;i<length;i++) {
		if (b[i] != 1) {
			printf("result is wrong: b[%d] = %d\n", i, b[i]);
			return 1;
		}
	}
	
	clReleaseMemObject(buf_a);
	clReleaseMemObject(buf_b);
	
	clReleaseKernel(kernel);
	clReleaseProgram(program);
	clReleaseContext(context);
	
	return 0;
}