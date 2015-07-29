
/**
 * ocl-ke (OpenCL kernel extractor)
 * Copyright (C) -2010 Rafael Ubal Tena (raurte@gap.upv.es)
 * Copyright (C) 2015 Mario Kicherer (dev@kicherer.org)
 * 
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#include <CL/cl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <unistd.h>
#include <assert.h>


#define MAX_STRING_SIZE  200
#define MAX_DEVICES  100
#define CL_CONTEXT_OFFLINE_DEVICES_AMD  0x403f

static char *syntax =
	"Syntax: %s [<options>] <kernel.cl>\n"
	"\n"
	"%s uses the OpenCL API to compile kernels for the selected device\n"
	"and stores the resulting binary code in a file. Afterwards, applications\n"
	"can use clCreateProgramWithBinary instead of compiling the kernel during\n"
	"every application run.\n"
	"\n"
	"Options:\n"
	"\t-L              Print list of available platforms\n"
	"\t-l              Print list of available devices\n"
	"\t-e              Print list of supported platform extensions\n"
	"\t-a              Activate CL_CONTEXT_OFFLINE_DEVICES_AMD extension\n"
	"\t-p <plat_idx>   Index of the desired platform (default: 0)\n"
	"\t-d <dev_idx>    Index of the desired device (default: 0)\n"
	"\t-b <build_opts> Build options that are passed to the compiler\n"
	;

char *kernel_file_name = NULL;  /* Kernel source file */
char kernel_file_prefix[MAX_STRING_SIZE];  /* Prefix used for output files */
char bin_file_name[MAX_STRING_SIZE];  /* Name of binary */

cl_platform_id platform;
cl_context context;

int num_devices = 0;
int device_id = -1;
cl_device_id devices[MAX_DEVICES];
cl_device_id device;
char *build_options = 0;

void fatal(char *format, ...) {
	va_list args;
	
	va_start(args, format);
	
	printf("fatal error: ");
	vprintf(format, args);
	printf("\n");
	
	va_end(args);
	
	exit(1);
}

void main_list_devices(FILE *f) {
	int i;
	char name[MAX_STRING_SIZE], vendor[MAX_STRING_SIZE];

	/* List devices */
	fprintf(f, "\n ID    Name, Vendor\n");
	fprintf(f, "----  ----------------------------------------------------\n");
	for (i = 0; i < num_devices; i++) {
		clGetDeviceInfo(devices[i], CL_DEVICE_NAME, MAX_STRING_SIZE, name, NULL);
		clGetDeviceInfo(devices[i], CL_DEVICE_VENDOR, MAX_STRING_SIZE, vendor, NULL);
		fprintf(f, " %2d    %s %s\n", i, name, vendor);
	}
	fprintf(f, "----------------------------------------------------------\n");
	fprintf(f, "\t%d devices available\n\n", num_devices);
}

char * read_file(char *name, size_t *size) {
	FILE *f;
	char *result;
	
	f = fopen(name, "r");
	if (!f)
		fatal("cannot open file \"%s\"", name);
	
	fseek(f, 0L, SEEK_END);
	*size = ftell(f);
	fseek(f, 0L, SEEK_SET);
	
	result = (char*) malloc(*size);
	fread(result, *size, 1, f);
	
	fclose(f);
	
	return result;
}

void write_to_file(char *name, char *buf, size_t buf_len) {
	FILE *f;
	f = fopen(name, "w");
	if (!f)
		fatal("cannot open file \"%s\"", name);
	
	fwrite(buf, buf_len, 1, f);
	
	fclose(f);
}

void main_compile_kernel() {
	char device_name[MAX_STRING_SIZE];

	char *kernel_file_ext = ".cl";
	int len, extlen;
	cl_int err;

	char *program_source;
	size_t program_source_size;

	size_t bin_sizes[MAX_DEVICES];
	size_t bin_sizes_ret;
	char *bin_bits[MAX_DEVICES];


	/* Get device info */
	clGetDeviceInfo(device, CL_DEVICE_NAME, MAX_STRING_SIZE, device_name, NULL);
	
	/* Message */
	printf("Device %d selected: %s\n", device_id, device_name);
	printf("Compiling '%s'...\n", kernel_file_name);
	
	/* Get kernel prefix */
	extlen = strlen(kernel_file_ext);
	strncpy(kernel_file_prefix, kernel_file_name, MAX_STRING_SIZE);
	len = strlen(kernel_file_name);
	if (len > extlen && !strcmp(&kernel_file_name[len - extlen], kernel_file_ext))
		kernel_file_prefix[len - extlen] = 0;
	snprintf(bin_file_name, MAX_STRING_SIZE, "%s.bin", kernel_file_prefix);
	
	/* Read the program source */
	program_source = read_file(kernel_file_name, &program_source_size);
	if (!program_source)
		fatal("%s: cannot open kernel\n", kernel_file_name);
	
	/* Create program */
	cl_program program;
	program = clCreateProgramWithSource(context, 1, (const char **) &program_source, &program_source_size, &err);
	if (err != CL_SUCCESS)
		fatal("clCreateProgramWithSource failed");

	/* Compile source */
	err = clBuildProgram(program, 1, &device, build_options, NULL, NULL);
	if (err != CL_SUCCESS) {
		char buf[0x10000];
		clGetProgramBuildInfo(program, device, CL_PROGRAM_BUILD_LOG, sizeof(buf), buf, NULL);
		fprintf(stderr, "\nCompiler options: \"%s\"\n", build_options);
		fprintf(stderr, "Compiler message: %s\n", buf);
		fatal("compilation failed");
	}
	free(program_source);

	/* Get number and size of binaries */
	err = clGetProgramInfo(program, CL_PROGRAM_BINARY_SIZES, sizeof(bin_sizes), bin_sizes, &bin_sizes_ret);
	if (err != CL_SUCCESS)
		fatal("clGetProgramInfo CL_PROGRAM_BINARY_SIZES failed: %d", err);
	
	assert(bin_sizes_ret == sizeof(size_t));
	assert(bin_sizes[0] > 0);

	/* Dump binary into file */
	memset(bin_bits, 0, sizeof(bin_bits));
	bin_bits[0] = malloc(bin_sizes[0]);
	err = clGetProgramInfo(program, CL_PROGRAM_BINARIES, sizeof(char*), bin_bits, &bin_sizes_ret);
	if (err != CL_SUCCESS)
		fatal("clGetProgramInfo CL_PROGRAM_BINARIES failed: %d", err);
	assert(bin_sizes_ret == sizeof(char*));
	
	write_to_file(bin_file_name, bin_bits[0], bin_sizes[0]);
	free(bin_bits[0]);
	
	printf("%s: kernel binary created\n", bin_file_name);
}

int main(int argc, char **argv)
{
	int opt;
	unsigned int i;
	char *device_str = NULL;
	char *platform_str = NULL;
	size_t size;
	long platform_id;
	int action_list_devices = 0;  /* Dump list of devices */
	int action_list_platforms = 0;  /* Dump list of platforms */
	int action_list_plat_exts = 0;
	int use_amd_extension = 0;
	
	cl_int err;
	cl_uint n_platforms;
	cl_platform_id *platforms;
	char **platform_names;
	

	/* No arguments */
	if (argc == 1) {
		fprintf(stderr, syntax, argv[0], argv[0]);
		return 1;
	}

	/* Process options */
	while ((opt = getopt(argc, argv, "lLeap:d:b:")) != -1) {
		switch (opt) {
		case 'l':
			action_list_devices = 1;
			break;
		case 'L':
			action_list_platforms = 1;
			break;
		case 'a':
			use_amd_extension = 1;
			break;
		case 'e':
			action_list_plat_exts = 1;
			break;
		case 'p':
			platform_str = optarg;
			break;
		case 'd':
			device_str = optarg;
			break;
		case 'b':
			build_options = optarg;
			break;
		default:
			fprintf(stderr, syntax, argv[0], argv[0]);
			return 1;
		}
	}

	/* The only remaining argument should be the kernel to compile */
	if (argc - optind > 1) {
		fprintf(stderr, syntax, argv[0], argv[0]);
		return 1;
	} else if (argc - optind == 1)
		kernel_file_name = argv[optind];
	if (!kernel_file_name && !action_list_devices && !action_list_platforms)
		fatal("no kernel to compile");

	
	/* get platforms */
	err = clGetPlatformIDs(0, 0, &n_platforms);
	if (err != CL_SUCCESS)
		fatal("cannot get OpenCL platform");
	
	if (n_platforms < 1) {
		printf("no OpenCL platform found\n");
		exit(1);
	}
	
	platforms = (cl_platform_id*) malloc(sizeof(cl_platform_id)*n_platforms);
	platform_names = (char**) malloc(sizeof(char*)*n_platforms);
	
	err = clGetPlatformIDs(n_platforms, platforms, NULL);
	if (err != CL_SUCCESS)
		fatal("cannot get OpenCL platform");
	
	
	/* determine platform index */
	if (platform_str) {
		/* try to interpret 'platform_str' as a number */
		char *endptr;
		platform_id = strtol(platform_str, &endptr, 10);
		if (!*endptr) {
			if (platform_id >= n_platforms)
				fatal("%d is not a valid platform ID; use '-L' option for a list of valid IDs", platform_id);
		} else
			fatal("cannot parse %s\n", platform_str);
	} else
		platform_id = 0;
	
	platform = platforms[platform_id];
	
	if (action_list_platforms) {
		printf("\n ID    Name, Vendor\n");
		printf("----  ----------------------------------------------------\n");
	}
	
	
	/* get platform names */
	for (i=0;i<n_platforms;i++) {
		err = clGetPlatformInfo(platforms[i], CL_PLATFORM_NAME, 0, 0, &size);
		if (err != CL_SUCCESS)
			fatal("error while querying platform info");
		
		platform_names[i] = (char*) malloc(sizeof(char)*size);
		
		err = clGetPlatformInfo(platforms[i], CL_PLATFORM_NAME, size, platform_names[i], 0);
		if (err != CL_SUCCESS)
			fatal("error while querying platform info");
		
		if (action_list_platforms) {
			char vendor[MAX_STRING_SIZE];
			err = clGetPlatformInfo(platforms[i], CL_PLATFORM_VENDOR, MAX_STRING_SIZE, vendor, 0);
			if (err != CL_SUCCESS)
				fatal("error while querying platform info");
				
			printf(" %2d    %s, %s\n", i, platform_names[i], vendor);
		}
	}
	
	if (action_list_platforms) {
		printf("----------------------------------------------------------\n");
		printf("\t%d platforms available\n\n", n_platforms);
	}
	
	printf("\nPlatform %ld selected: %s\n", platform_id, platform_names[platform_id]);
	
	if (action_list_plat_exts) {
		char exts[MAX_STRING_SIZE];
		err = clGetPlatformInfo(platform, CL_PLATFORM_EXTENSIONS, MAX_STRING_SIZE, exts, 0);
		if (err != CL_SUCCESS)
			fatal("error while querying platform info");
		
		printf("Extensions: %s\n", exts);
	}
	
	/* create context */
	cl_context_properties cprops[5];
	cprops[0] = CL_CONTEXT_PLATFORM;
	cprops[1] = (cl_context_properties)platform;
	
	i=2;
	
	char extensions[MAX_STRING_SIZE];
	clGetPlatformInfo(platform, CL_PLATFORM_EXTENSIONS, MAX_STRING_SIZE, extensions, 0);
	if (use_amd_extension && strstr(extensions, "cl_amd_offline_devices")) {
		cprops[i] = CL_CONTEXT_OFFLINE_DEVICES_AMD;
		i++;
		cprops[i] = (cl_context_properties) 1;
		i++;
	}
	cprops[i] = (cl_context_properties) NULL;
	context = clCreateContextFromType(cprops, CL_DEVICE_TYPE_ALL, NULL, NULL, &err);
	if (err != CL_SUCCESS)
		fatal("cannot create context");
	
	/* Get device list from context */
	err = clGetContextInfo(context, CL_CONTEXT_NUM_DEVICES, sizeof(num_devices), &num_devices, NULL);
	if (err != CL_SUCCESS)
		fatal("cannot get number of devices");
	err = clGetContextInfo(context, CL_CONTEXT_DEVICES, sizeof(devices), devices, NULL);
	if (err != CL_SUCCESS)
		fatal("cannot get list of devices");
	
	/* Get selected device */
	if (device_str) {
		char *endptr;
		char name[MAX_STRING_SIZE];
		int i;

		/* Try to interpret 'device_str' as a number */
		device_id = strtol(device_str, &endptr, 10);
		if (!*endptr) {
			if (device_id >= num_devices)
				fatal("%d is not a valid device ID; use '-l' option for a list of valid IDs", device_id);
		} else {
			fatal("cannot parse %s\n", device_str);
		}
	} else {
		device_id = 0;
	}
	
	device = devices[device_id];
	
	/* List available devices */
	if (action_list_devices)
		main_list_devices(stdout);
	
	/* release context with all devices and recreate with selected device */
	clReleaseContext(context);
	context = clCreateContext(cprops, 1, &device, 0, 0, &err);
	if (err != CL_SUCCESS)
		fatal("creating device context failed");
	
	/* Compile list of kernels */
	if (kernel_file_name)
		main_compile_kernel();
	
	/* End program */
	printf("\n");
	return 0;
}

