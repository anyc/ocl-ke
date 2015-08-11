
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

#ifdef OCL_AUTODETECT
#include <dlfcn.h>
#endif


cl_int (*l_clCompileProgram)(cl_program program,
					cl_uint num_devices,
					const cl_device_id *device_list,
					const char *options,
					cl_uint num_input_headers,
					const cl_program *input_headers,
					const char **header_include_names,
					void (CL_CALLBACK *pfn_notify)( cl_program program, void *user_data),
					void *user_data) = 0;

cl_program (*l_clLinkProgram)(cl_context context,
					cl_uint num_devices,
					const cl_device_id *device_list,
					const char *options,
					cl_uint num_input_programs,
					const cl_program *input_programs,
					void (CL_CALLBACK *pfn_notify) (cl_program program, void *user_data),
					void *user_data,
					cl_int *errcode_ret) = 0;

unsigned char opencl_api_version = 10;

#define MAX_STRING_SIZE  200
#define MAX_DEVICES  100

#ifndef CL_CONTEXT_OFFLINE_DEVICES_AMD
#define CL_CONTEXT_OFFLINE_DEVICES_AMD 0x403f
#endif

static char *syntax =
	"Syntax: %s [<options>] <kernel.cl>\n"
	"\n"
	"%s uses the OpenCL API to compile a kernel for the selected device\n"
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
	"\t-i <include>    Include this binary (header) in resulting binary (OpenCL 1.2 only)\n"
	"\t-o <filename>   Write kernel into file instead of ${kernel}.bin\n"
	;

char * ocl_err2str(cl_int err) {
	switch (err) {
		case CL_SUCCESS:                            return "Success";
		case CL_DEVICE_NOT_FOUND:                   return "Device not found";
		case CL_DEVICE_NOT_AVAILABLE:               return "Device not available";
		case CL_COMPILER_NOT_AVAILABLE:             return "Compiler not available";
		case CL_MEM_OBJECT_ALLOCATION_FAILURE:      return "Memory object allocation failure";
		case CL_OUT_OF_RESOURCES:                   return "Out of resources";
		case CL_OUT_OF_HOST_MEMORY:                 return "Out of host memory";
		case CL_PROFILING_INFO_NOT_AVAILABLE:       return "Profiling information not available";
		case CL_MEM_COPY_OVERLAP:                   return "Memory copy overlap";
		case CL_IMAGE_FORMAT_MISMATCH:              return "Image format mismatch";
		case CL_IMAGE_FORMAT_NOT_SUPPORTED:         return "Image format not supported";
		case CL_BUILD_PROGRAM_FAILURE:              return "Program build failure";
		case CL_MAP_FAILURE:                        return "Map failure";
		case CL_INVALID_VALUE:                      return "Invalid value";
		case CL_INVALID_DEVICE_TYPE:                return "Invalid device type";
		case CL_INVALID_PLATFORM:                   return "Invalid platform";
		case CL_INVALID_DEVICE:                     return "Invalid device";
		case CL_INVALID_CONTEXT:                    return "Invalid context";
		case CL_INVALID_QUEUE_PROPERTIES:           return "Invalid queue properties";
		case CL_INVALID_COMMAND_QUEUE:              return "Invalid command queue";
		case CL_INVALID_HOST_PTR:                   return "Invalid host pointer";
		case CL_INVALID_MEM_OBJECT:                 return "Invalid memory object";
		case CL_INVALID_IMAGE_FORMAT_DESCRIPTOR:    return "Invalid image format descriptor";
		case CL_INVALID_IMAGE_SIZE:                 return "Invalid image size";
		case CL_INVALID_SAMPLER:                    return "Invalid sampler";
		case CL_INVALID_BINARY:                     return "Invalid binary";
		case CL_INVALID_BUILD_OPTIONS:              return "Invalid build options";
		case CL_INVALID_PROGRAM:                    return "Invalid program";
		case CL_INVALID_PROGRAM_EXECUTABLE:         return "Invalid program executable";
		case CL_INVALID_KERNEL_NAME:                return "Invalid kernel name";
		case CL_INVALID_KERNEL_DEFINITION:          return "Invalid kernel definition";
		case CL_INVALID_KERNEL:                     return "Invalid kernel";
		case CL_INVALID_ARG_INDEX:                  return "Invalid argument index";
		case CL_INVALID_ARG_VALUE:                  return "Invalid argument value";
		case CL_INVALID_ARG_SIZE:                   return "Invalid argument size";
		case CL_INVALID_KERNEL_ARGS:                return "Invalid kernel arguments";
		case CL_INVALID_WORK_DIMENSION:             return "Invalid work dimension";
		case CL_INVALID_WORK_GROUP_SIZE:            return "Invalid work group size";
		case CL_INVALID_WORK_ITEM_SIZE:             return "Invalid work item size";
		case CL_INVALID_GLOBAL_OFFSET:              return "Invalid global offset";
		case CL_INVALID_EVENT_WAIT_LIST:            return "Invalid event wait list";
		case CL_INVALID_EVENT:                      return "Invalid event";
		case CL_INVALID_OPERATION:                  return "Invalid operation";
		case CL_INVALID_GL_OBJECT:                  return "Invalid OpenGL object";
		case CL_INVALID_BUFFER_SIZE:                return "Invalid buffer size";
		case CL_INVALID_MIP_LEVEL:                  return "Invalid mip-map level";
		default: return "Unknown error";
	}
}

void fatal(char *format, ...) {
	va_list args;
	
	va_start(args, format);
	
	printf("fatal error: ");
	vprintf(format, args);
	printf("\n");
	
	va_end(args);
	
	exit(1);
}

void ocl_fatal(cl_int error, char *format, ...) {
	va_list args;
	
	va_start(args, format);
	
	printf("fatal OCL error: ");
	vprintf(format, args);
	printf(": %s\n", ocl_err2str(error));
	
	va_end(args);
	
	exit(1);
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

int main(int argc, char **argv)
{
	int opt;
	unsigned int i;
	char *device_str = NULL;
	char *platform_str = NULL;
	char *filename = NULL;
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
	char *link_options = 0;
	char **includes = 0;
	unsigned int n_includes = 0;

	#ifdef OCL_AUTODETECT
	l_clCompileProgram = dlsym(0, "clCompileProgram");
	l_clLinkProgram = dlsym(0, "clLinkProgram");
	if (l_clCompileProgram && l_clLinkProgram) {
		opencl_api_version = 12;
		printf("Detected OpenCL 1.2 capable library\n");
	}
	#endif
	
	/* No arguments */
	if (argc == 1) {
		fprintf(stderr, syntax, argv[0], argv[0]);
		return 1;
	}

	/* Process options */
	while ((opt = getopt(argc, argv, "lLeap:d:b:o:i:")) != -1) {
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
		case 'o':
			filename = optarg;
			break;
		case 'i':
			if (opencl_api_version < 12)
				printf("Warning: -i is only available with OpenCL 1.2\n");
			
			includes = (char**) realloc(includes, sizeof(char*)*(n_includes+1));
			includes[n_includes] = optarg;
			n_includes++;
			
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
		ocl_fatal(err, "cannot get OpenCL platform");
	
	if (n_platforms < 1) {
		printf("no OpenCL platform found\n");
		exit(1);
	}
	
	platforms = (cl_platform_id*) malloc(sizeof(cl_platform_id)*n_platforms);
	platform_names = (char**) malloc(sizeof(char*)*n_platforms);
	
	err = clGetPlatformIDs(n_platforms, platforms, NULL);
	if (err != CL_SUCCESS)
		ocl_fatal(err, "cannot get OpenCL platform");
	
	
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
		printf("\n ID    Name, Vendor (Version)\n");
		printf("----  ----------------------------------------------------\n");
	}
	
	
	/* get platform names */
	for (i=0;i<n_platforms;i++) {
		err = clGetPlatformInfo(platforms[i], CL_PLATFORM_NAME, 0, 0, &size);
		if (err != CL_SUCCESS)
			ocl_fatal(err, "error while querying platform info");
		
		platform_names[i] = (char*) malloc(sizeof(char)*size);
		
		err = clGetPlatformInfo(platforms[i], CL_PLATFORM_NAME, size, platform_names[i], 0);
		if (err != CL_SUCCESS)
			ocl_fatal(err, "error while querying platform info");
		
		if (action_list_platforms) {
			char vendor[MAX_STRING_SIZE], version[MAX_STRING_SIZE];
			err = clGetPlatformInfo(platforms[i], CL_PLATFORM_VENDOR, MAX_STRING_SIZE, vendor, 0);
			if (err != CL_SUCCESS)
				ocl_fatal(err, "error while querying platform info");
			
			err = clGetPlatformInfo(platforms[i], CL_PLATFORM_VERSION, MAX_STRING_SIZE, vendor, 0);
			if (err != CL_SUCCESS)
				ocl_fatal(err, "error while querying platform info");
			
			printf(" %2d    %s, %s (%s)\n", i, platform_names[i], vendor, version);
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
			ocl_fatal(err, "error while querying platform info");
		
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
		ocl_fatal(err, "cannot create context");
	
	/* Get device list from context */
	err = clGetContextInfo(context, CL_CONTEXT_NUM_DEVICES, sizeof(num_devices), &num_devices, NULL);
	if (err != CL_SUCCESS)
		ocl_fatal(err, "cannot get number of devices");
	err = clGetContextInfo(context, CL_CONTEXT_DEVICES, sizeof(devices), devices, NULL);
	if (err != CL_SUCCESS)
		ocl_fatal(err, "cannot get list of devices");
	
	/* Get selected device */
	if (device_str) {
		char *endptr;
		char name[MAX_STRING_SIZE];

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
	if (action_list_devices) {
		FILE *f = stdout;
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
	
	/* release context with all devices and recreate with selected device */
	clReleaseContext(context);
	context = clCreateContext(cprops, 1, &device, 0, 0, &err);
	if (err != CL_SUCCESS)
		ocl_fatal(err, "creating device context failed");
	
	/* Compile list of kernels */
	if (kernel_file_name) {
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
		
		if (!filename) {
			/* Get kernel prefix */
			extlen = strlen(kernel_file_ext);
			strncpy(kernel_file_prefix, kernel_file_name, MAX_STRING_SIZE);
			len = strlen(kernel_file_name);
			if (len > extlen && !strcmp(&kernel_file_name[len - extlen], kernel_file_ext))
				kernel_file_prefix[len - extlen] = 0;
			snprintf(bin_file_name, MAX_STRING_SIZE, "%s.bin", kernel_file_prefix);
		} else {
			strncpy(bin_file_name, filename, MAX_STRING_SIZE);
		}
		
		/* Read the program source */
		program_source = read_file(kernel_file_name, &program_source_size);
		if (!program_source)
			fatal("%s: cannot open kernel\n", kernel_file_name);
		
		/* Create program */
		cl_program program;
		program = clCreateProgramWithSource(context, 1, (const char **) &program_source, &program_source_size, &err);
		if (err != CL_SUCCESS)
			ocl_fatal(err, "clCreateProgramWithSource failed");

		if (opencl_api_version < 12) {
			printf("Building '%s'...\n", kernel_file_name);
			
			err = clBuildProgram(program, 1, &device, build_options, NULL, NULL);
			if (err != CL_SUCCESS) {
				char buf[0x10000];
				clGetProgramBuildInfo(program, device, CL_PROGRAM_BUILD_LOG, sizeof(buf), buf, NULL);
				fprintf(stderr, "\nCompiler options: \"%s\"\n", build_options);
				fprintf(stderr, "Compiler message: %s\n", buf);
				ocl_fatal(err, "compilation failed");
			}
			free(program_source);
		} else
		if (opencl_api_version == 12) {
			char *src;
			size_t size;
			cl_uint num_input_headers = 0;
			cl_program *input_headers = 0;
			
			if (n_includes > 0) {
				input_headers = (cl_program*) malloc(sizeof(cl_program)*n_includes);
				
				for (i=0;i<n_includes;i++) {
					printf("Loading '%s'...\n", includes[i]);
					
					src = read_file(includes[i], &size);
					input_headers[i] = clCreateProgramWithSource(context, 1, (const char **) &src, &size, &err);
					if (err != CL_SUCCESS)
						ocl_fatal(err, "clCreateProgramWithSource failed");
					free(src);
				}
			}
			
			printf("Compiling '%s'...\n", kernel_file_name);
			
			err = l_clCompileProgram(program, 1, &device, build_options, n_includes, input_headers,(const char**) includes, 0, 0);
			if (err != CL_SUCCESS) {
				char buf[0x10000];
				clGetProgramBuildInfo(program, device, CL_PROGRAM_BUILD_LOG, sizeof(buf), buf, NULL);
				fprintf(stderr, "\nCompiler options: \"%s\"\n", build_options);
				fprintf(stderr, "Compiler message: %s\n", buf);
				ocl_fatal(err, "compilation failed");
			}
			
// 			printf("Linking '%s'...\n", kernel_file_name);
// 			
// 			link_options = "-create-library";
// 			program = l_clLinkProgram(context, 1, &device, link_options, 1, &program, 0, 0, &err);
// 			if (err != CL_SUCCESS)
// 				ocl_fatal(err, "clLinkProgram failed");
			
			free(program_source);
		} else
			fatal("failed to detect OpenCL API version");

		/* Get number and size of binaries */
		err = clGetProgramInfo(program, CL_PROGRAM_BINARY_SIZES, sizeof(bin_sizes), bin_sizes, &bin_sizes_ret);
		if (err != CL_SUCCESS)
			ocl_fatal(err, "clGetProgramInfo CL_PROGRAM_BINARY_SIZES failed: %d", err);
		
		assert(bin_sizes_ret == sizeof(size_t));
		assert(bin_sizes[0] > 0);

		/* Dump binary into file */
		memset(bin_bits, 0, sizeof(bin_bits));
		bin_bits[0] = malloc(bin_sizes[0]);
		err = clGetProgramInfo(program, CL_PROGRAM_BINARIES, sizeof(char*), bin_bits, &bin_sizes_ret);
		if (err != CL_SUCCESS)
			ocl_fatal(err, "clGetProgramInfo CL_PROGRAM_BINARIES failed: %d", err);
		assert(bin_sizes_ret == sizeof(char*));
		
		write_to_file(bin_file_name, bin_bits[0], bin_sizes[0]);
		free(bin_bits[0]);
		
		printf("Successfully created kernel binary '%s'\n", bin_file_name);
	}
	
	/* End program */
	printf("\n");
	return 0;
}

