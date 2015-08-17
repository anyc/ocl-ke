
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

/* function pointers that are set if the OpenCL runtime supports OpenCL v1.2 or higher */
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

cl_int (*l_clGetKernelArgInfo)(cl_kernel kernel,
					cl_uint arg_indx,
					cl_kernel_arg_info param_name,
					size_t param_value_size,
					void *param_value,
					size_t *param_value_size_ret) = 0;

unsigned char opencl_api_version = 10;

#define INFO_STR_SIZE 1000

#ifndef CL_CONTEXT_OFFLINE_DEVICES_AMD
#define CL_CONTEXT_OFFLINE_DEVICES_AMD 0x403f
#endif

static char *syntax =
	"Syntax: %s [<options>] <kernel.cl>\n"
	"\n"
	"%s uses the OpenCL API to compile a kernel for the selected device(s)\n"
	"and stores the resulting binary code in a file. Afterwards, applications\n"
	"can load the kernel with clCreateProgramWithBinary instead of compiling\n"
	"the kernel during every application run.\n"
	"\n"
	"Options:\n"
	"\t-L              Print list of available platforms\n"
	"\t-l              Print list of available devices for selected platform\n"
	"\t-e              Print list of supported extensions\n"
	"\t-a              Activate CL_CONTEXT_OFFLINE_DEVICES_AMD extension, if\n"
	"\t                available, to choose from the list of all supported\n"
	"\t                devices by the compiler instead of only the available\n"
	"\t                devices in the current system.\n"
	"\t-k              Show detailed information about included kernels\n"
	"\t-p <plat_idx>   Index of the desired platform (default: 1)\n"
	"\t-d <dev_idx>    Index of the desired device (default: 1)\n"
	"\t                A value of 0 equals all devices on the platform.\n"
	"\t                This option can be specified multiple times to select\n"
	"\t                multiple devices but this is not supported by all\n"
	"\t                available operations. \n"
	"\t-b <build_opts> Build options that are passed to the compiler\n"
	"\t-B <link_opts>  Options passed to the linker\n"
	"\t-s              Create a kernel library instead of an executable\n"
	"\t                (OpenCL 1.2 or higher only)\n"
	"\t-i <source>     Include this source file (OpenCL 1.2 or higher only)\n"
	"\t                This option can be specified multiple times.\n"
	"\t-I <binary>     Include this binary file (OpenCL 1.2 or higher only)\n"
	"\t                This option can be specified multiple times.\n"
	"\t-o <filename>   Write kernel into this file instead of ${kernel}.bin\n"
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

void show_build_log(cl_program program, unsigned int n_devices, cl_device_id *devices, cl_int err) {
	int i;
	
	for (i=0;i<n_devices;i++) {
		cl_build_status bstatus;
		
		clGetProgramBuildInfo(program, devices[i], CL_PROGRAM_BUILD_STATUS, sizeof(bstatus), &bstatus, NULL);
		
		if (bstatus != CL_BUILD_SUCCESS) {
			char *buf;
			size_t logsize;
			
			clGetProgramBuildInfo(program, devices[i], CL_PROGRAM_BUILD_LOG, 0, 0, &logsize);
			
			buf = (char*) malloc(logsize);
			
			clGetProgramBuildInfo(program, devices[i], CL_PROGRAM_BUILD_LOG, logsize, buf, NULL);
			fprintf(stderr, "Compiler message: %s\n", buf);
			ocl_fatal(err, "compilation failed");
		}
	}
}

void show_kernel_info(cl_program program) {
	int i, j;
	cl_int err;
	cl_uint n_kernels, n_args;
	cl_kernel *kernels;
	size_t size;
	char *fname, *attrs;
	
	err = clCreateKernelsInProgram(program, 0, 0, &n_kernels);
	if (err != CL_SUCCESS)
		ocl_fatal(err, "clCreateKernelsInProgram failed");
	
	kernels = (cl_kernel*) malloc(n_kernels*sizeof(cl_kernel));
	
	clCreateKernelsInProgram(program, n_kernels*sizeof(cl_kernel), kernels, 0);
	if (err != CL_SUCCESS)
		ocl_fatal(err, "clCreateKernelsInProgram failed");
	
	for (i=0;i<n_kernels;i++) {
		err = clGetKernelInfo(kernels[i], CL_KERNEL_FUNCTION_NAME, 0, 0, &size);
		if (err != CL_SUCCESS)
			ocl_fatal(err, "clGetKernelInfo failed");
		fname = (char*) malloc(size);
		err = clGetKernelInfo(kernels[i], CL_KERNEL_FUNCTION_NAME, size, fname, 0);
		if (err != CL_SUCCESS)
			ocl_fatal(err, "clGetKernelInfo failed");
		
		err = clGetKernelInfo(kernels[i], CL_KERNEL_NUM_ARGS, sizeof(cl_uint), &n_args, 0);
		if (err != CL_SUCCESS)
			ocl_fatal(err, "clGetKernelInfo failed");
		
		err = clGetKernelInfo(kernels[i], CL_KERNEL_ATTRIBUTES, 0, 0, &size);
		if (err != CL_SUCCESS)
			ocl_fatal(err, "clGetKernelInfo failed");
		attrs = (char*) malloc(size);
		err = clGetKernelInfo(kernels[i], CL_KERNEL_ATTRIBUTES, size, attrs, 0);
		if (err != CL_SUCCESS)
			ocl_fatal(err, "clGetKernelInfo failed");
		
		printf("\tkernel %s %s(", attrs, fname);
		
		for (j=0;j<n_args;j++) {
			char *argtype, *argname;
			cl_kernel_arg_address_qualifier addr_qual;
			cl_kernel_arg_access_qualifier acc_qual;
			cl_kernel_arg_type_qualifier type_qual;
			
			err = l_clGetKernelArgInfo(kernels[i], j, CL_KERNEL_ARG_TYPE_NAME, 0, 0, &size);
			// skip this loop if program does not contain information about kernel arguments
			if (err == CL_KERNEL_ARG_INFO_NOT_AVAILABLE)
				break;
			if (err != CL_SUCCESS)
				ocl_fatal(err, "clGetKernelInfo failed");
			argtype = (char*) malloc(size);
			err = l_clGetKernelArgInfo(kernels[i], j, CL_KERNEL_ARG_TYPE_NAME, size, argtype, 0);
			if (err != CL_SUCCESS)
				ocl_fatal(err, "clGetKernelInfo failed");
			
			err = l_clGetKernelArgInfo(kernels[i], j, CL_KERNEL_ARG_NAME, 0, 0, &size);
			if (err != CL_SUCCESS)
				ocl_fatal(err, "clGetKernelInfo failed");
			argname = (char*) malloc(size);
			err = l_clGetKernelArgInfo(kernels[i], j, CL_KERNEL_ARG_NAME, size, argname, 0);
			if (err != CL_SUCCESS)
				ocl_fatal(err, "clGetKernelInfo failed");
			
			err = l_clGetKernelArgInfo(kernels[i], j, CL_KERNEL_ARG_ADDRESS_QUALIFIER, sizeof(cl_kernel_arg_address_qualifier), &addr_qual, 0);
			if (err != CL_SUCCESS)
				ocl_fatal(err, "clGetKernelInfo failed");
			
			switch (addr_qual) {
				case CL_KERNEL_ARG_ADDRESS_GLOBAL: printf("__global "); break;
				case CL_KERNEL_ARG_ADDRESS_LOCAL: printf("__local "); break;
				case CL_KERNEL_ARG_ADDRESS_CONSTANT: printf("__constant "); break;
				case CL_KERNEL_ARG_ADDRESS_PRIVATE: /* printf("__private "); */ break;
			}
			
			err = l_clGetKernelArgInfo(kernels[i], j, CL_KERNEL_ARG_ACCESS_QUALIFIER, sizeof(cl_kernel_arg_access_qualifier), &acc_qual, 0);
			if (err != CL_SUCCESS)
				ocl_fatal(err, "clGetKernelInfo failed");
			
			switch (acc_qual) {
				case CL_KERNEL_ARG_ACCESS_READ_ONLY: printf("__read_only "); break;
				case CL_KERNEL_ARG_ACCESS_WRITE_ONLY: printf("__write_only "); break;
				case CL_KERNEL_ARG_ACCESS_READ_WRITE: printf("__read_write "); break;
				case CL_KERNEL_ARG_ACCESS_NONE: /* printf("__private "); */ break;
			}
			
			err = l_clGetKernelArgInfo(kernels[i], j, CL_KERNEL_ARG_TYPE_QUALIFIER, sizeof(cl_kernel_arg_type_qualifier), &type_qual, 0);
			if (err != CL_SUCCESS)
				ocl_fatal(err, "clGetKernelInfo failed");
			
			switch (type_qual) {
				case CL_KERNEL_ARG_TYPE_CONST: printf("const "); break;
				case CL_KERNEL_ARG_TYPE_RESTRICT: printf("restrict "); break;
				case CL_KERNEL_ARG_TYPE_VOLATILE: printf("volatile "); break;
				case  CL_KERNEL_ARG_TYPE_NONE: /* printf("__private "); */ break;
			}
			
			printf("%s %s", argtype, argname);
			
			if (j < n_args-1)
				printf(", ");
			
			free(argtype);
			free(argname);
		}
		
		printf(")\n");
		
		free(fname);
		free(attrs);
	}
}

int main(int argc, char **argv)
{
	int opt;
	unsigned int i;
	char **device_strings = NULL;
	unsigned int n_device_strings = 0;
	char *platform_str = NULL;
	char *filename = NULL;
	size_t size;
	long platform_id;
	int action_list_devices = 0;  /* Dump list of devices */
	int action_list_platforms = 0;  /* Dump list of platforms */
	int action_list_exts = 0;
	int use_amd_extension = 0;
	char make_shared_lib = 0;
	char detailed_kernels = 0;
	
	cl_int err;
	cl_uint n_platforms;
	cl_platform_id *platforms;
	char **platform_names;
	
	char *kernel_file_name = NULL;  /* Kernel source file */

	cl_platform_id platform;
	cl_context context;

	unsigned int n_all_devices = 0;
	cl_device_id *all_devices;
	cl_device_id *devices;
	unsigned int n_devices;
	char *build_options = 0;
	char *link_options = 0;
	char **includes = 0;
	unsigned int n_includes = 0;
	char **bin_includes = 0;
	unsigned int n_bin_includes = 0;

	#ifdef OCL_AUTODETECT
	/* check if the linked OpenCL runtime supports v1.2 API */
	l_clCompileProgram = dlsym(0, "clCompileProgram");
	l_clLinkProgram = dlsym(0, "clLinkProgram");
	l_clGetKernelArgInfo = dlsym(0, "clGetKernelArgInfo");
	if (l_clCompileProgram && l_clLinkProgram && l_clGetKernelArgInfo) {
		opencl_api_version = 12;
		printf("Linked OpenCL runtime seems to support v1.2 API\n");
	}
	#endif
	
	/* No arguments */
	if (argc == 1) {
		fprintf(stderr, syntax, argv[0], argv[0]);
		return 1;
	}

	/* Process options */
	while ((opt = getopt(argc, argv, "lLeap:d:b:o:i:I:sB:k")) != -1) {
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
			action_list_exts = 1;
			break;
		case 'k':
			detailed_kernels = 1;
			break;
		case 'p':
			platform_str = optarg;
			break;
		case 'd':
			device_strings = (char**) realloc(device_strings, sizeof(char*)*(n_device_strings+1));
			device_strings[n_device_strings] = malloc(strlen(optarg)+1);
			strcpy(device_strings[n_device_strings], optarg);
			n_device_strings++;
			
			char *endptr;
			long id = strtol(optarg, &endptr, 10);
			if ((!*endptr && id == 0) ||
				(n_device_strings == 2))
			{
				printf("Please note that multiple devices are not supported during all operations\n");
			}
			
			break;
		case 'b':
			build_options = optarg;
			break;
		case 'o':
			filename = optarg;
			break;
		case 'i':
			includes = (char**) realloc(includes, sizeof(char*)*(n_includes+1));
			includes[n_includes] = malloc(strlen(optarg)+1);
			strcpy(includes[n_includes], optarg);
			n_includes++;
			
			break;
		case 'I':
			bin_includes = (char**) realloc(bin_includes, sizeof(char*)*(n_bin_includes+1));
			bin_includes[n_bin_includes] = malloc(strlen(optarg)+1);
			strcpy(bin_includes[n_bin_includes], optarg);
			n_bin_includes++;
			
			break;
		case 's':
			if (opencl_api_version < 12)
				fatal("OpenCL >=v1.2 required to create shared libraries");
			make_shared_lib = 1;
			break;
		case 'B':
			link_options = optarg;
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
		action_list_devices = 1;
	
	// get number of platforms
	err = clGetPlatformIDs(0, 0, &n_platforms);
	if (err != CL_SUCCESS)
		ocl_fatal(err, "cannot get OpenCL platform");
	
	if (n_platforms < 1) {
		printf("no OpenCL platform found\n");
		exit(1);
	}
	
	platforms = (cl_platform_id*) malloc(sizeof(cl_platform_id)*n_platforms);
	platform_names = (char**) malloc(sizeof(char*)*n_platforms);
	
	// get platforms
	err = clGetPlatformIDs(n_platforms, platforms, NULL);
	if (err != CL_SUCCESS)
		ocl_fatal(err, "cannot get OpenCL platform");
	
	
	/* parse platform index */
	if (platform_str) {
		/* try to interpret 'platform_str' as a number */
		char *endptr;
		platform_id = strtol(platform_str, &endptr, 10);
		if (!*endptr) {
			if (platform_id > n_platforms || platform_id < 1)
				fatal("%d is not a valid platform ID; use '-L' option for a list of valid IDs", platform_id);
		} else
			fatal("cannot parse %s\n", platform_str);
	} else
		platform_id = 1;
	
	platform = platforms[platform_id-1];
	
	if (action_list_platforms) {
		printf("\n ID    Name, Vendor (Version)\n");
		printf("----  ----------------------------------------------------\n");
	}
	
	
	/* get platform info */
	for (i=0;i<n_platforms;i++) {
		err = clGetPlatformInfo(platforms[i], CL_PLATFORM_NAME, 0, 0, &size);
		if (err != CL_SUCCESS)
			ocl_fatal(err, "error while querying platform info");
		
		platform_names[i] = (char*) malloc(sizeof(char)*size);
		
		err = clGetPlatformInfo(platforms[i], CL_PLATFORM_NAME, size, platform_names[i], 0);
		if (err != CL_SUCCESS)
			ocl_fatal(err, "error while querying platform info");
		
		if (action_list_platforms) {
			char vendor[INFO_STR_SIZE], version[INFO_STR_SIZE];
			err = clGetPlatformInfo(platforms[i], CL_PLATFORM_VENDOR, INFO_STR_SIZE, vendor, 0);
			if (err != CL_SUCCESS)
				ocl_fatal(err, "error while querying platform info");
			
			err = clGetPlatformInfo(platforms[i], CL_PLATFORM_VERSION, INFO_STR_SIZE, version, 0);
			if (err != CL_SUCCESS)
				ocl_fatal(err, "error while querying platform info");
			
			printf(" %2d    %s, %s (%s)\n", i+1, platform_names[i], vendor, version);
		}
	}
	
	if (action_list_platforms) {
		printf("----------------------------------------------------------\n");
		printf("\t%d platforms available\n\n", n_platforms);
	}
	
	printf("\nPlatform %ld selected: %s\n", platform_id, platform_names[platform_id-1]);
	
	// get supported extensions
	if (action_list_exts) {
		char exts[INFO_STR_SIZE];
		err = clGetPlatformInfo(platform, CL_PLATFORM_EXTENSIONS, INFO_STR_SIZE, exts, 0);
		if (err != CL_SUCCESS)
			ocl_fatal(err, "error while querying platform info");
		
		printf("Extensions: %s\n", exts);
	}
	
	/* query OpenCL version of chosen platform */
	char version[INFO_STR_SIZE];
	err = clGetPlatformInfo(platform, CL_PLATFORM_VERSION, INFO_STR_SIZE, version, 0);
	if (err != CL_SUCCESS)
		ocl_fatal(err, "error while querying platform version");
	
	float cl_version;
	if (sscanf(version, "OpenCL %f", &cl_version) != 1)
		fatal("cannot parse platform version string");
	
	printf("Platform supports OpenCL v%.1f\n", cl_version);
	opencl_api_version = cl_version * 10;
	
	
	/* create context */
	cl_context_properties cprops[5];
	cprops[0] = CL_CONTEXT_PLATFORM;
	cprops[1] = (cl_context_properties)platform;
	
	i=2;
	
	char extensions[INFO_STR_SIZE];
	clGetPlatformInfo(platform, CL_PLATFORM_EXTENSIONS, INFO_STR_SIZE, extensions, 0);
	// check if platform supports cl_amd_offline_devices
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
	
	// get number of devices
	err = clGetContextInfo(context, CL_CONTEXT_NUM_DEVICES, sizeof(n_all_devices), &n_all_devices, NULL);
	if (err != CL_SUCCESS)
		ocl_fatal(err, "cannot get number of devices");
	
	// get all device IDs
	all_devices = (cl_device_id*) malloc(sizeof(cl_device_id)*n_all_devices);
	err = clGetContextInfo(context, CL_CONTEXT_DEVICES, sizeof(cl_device_id)*n_all_devices, all_devices, NULL);
	if (err != CL_SUCCESS)
		ocl_fatal(err, "cannot get list of devices");
	
	// determine selected devices
	if (device_strings) {
		char *endptr;
		long dev_id;
		int i;
		
		/* multiple devices have been selected, create a list of selected devices */
		
		devices = (cl_device_id*) malloc(sizeof(cl_device_id)*n_device_strings);

		n_devices = n_device_strings;
		for (i=0;i<n_device_strings;i++) {
			/* Try to interpret 'device_str' as a number */
			dev_id = strtol(device_strings[i], &endptr, 10);
			if (!*endptr) {
				if (dev_id > n_all_devices || dev_id < 0)
					fatal("%d is not a valid device ID; use '-l' option for a list of valid IDs", dev_id);
			} else {
				fatal("cannot parse device index \"%s\"\n", device_strings[i]);
			}
			
			if (dev_id == 0 && i > 0)
				fatal("error, \"-d 0\" cannot be combined with other values");
			
			if (dev_id == 0) {
				free(devices);
				printf("All devices selected\n");
				devices = all_devices;
				n_devices = n_all_devices;
				break;
			}
			
			devices[i] = all_devices[dev_id-1];
		}
	} else {
		/* no device has been selected, choose first device */
		
		devices = (cl_device_id*) malloc(sizeof(cl_device_id));
		devices[0] = all_devices[0];
		n_devices = 1;
	}
	
	/* Show device info */
	for (i=0;i<n_devices;i++) {
		char device_name[INFO_STR_SIZE];
		clGetDeviceInfo(devices[i], CL_DEVICE_NAME, INFO_STR_SIZE, device_name, NULL);
		if (err != CL_SUCCESS)
			ocl_fatal(err, "clGetDeviceInfo failed");
		
		printf("Device %d selected: %s\n", 1, device_name);
		
		if (action_list_exts) {
			char exts[INFO_STR_SIZE];
			clGetDeviceInfo(devices[i], CL_DEVICE_EXTENSIONS, INFO_STR_SIZE, exts, NULL);
			if (err != CL_SUCCESS)
				ocl_fatal(err, "clGetDeviceInfo failed");
			printf("Extensions: %s\n", exts);
		}
		
		if (opencl_api_version >= 12) {
			char *s;
			size_t size;
			
			clGetDeviceInfo(devices[i], CL_DEVICE_BUILT_IN_KERNELS, 0, 0, &size);
			if (err != CL_SUCCESS)
				ocl_fatal(err, "clGetDeviceInfo failed");
			if (size > 1) {
				s = (char*) malloc(size);
				clGetDeviceInfo(devices[i], CL_DEVICE_BUILT_IN_KERNELS, size, s, NULL);
				if (err != CL_SUCCESS)
					ocl_fatal(err, "clGetDeviceInfo failed");
				
				printf("Built-in kernels: %s\n", s);
				
				free(s);
			}
		}
		
	}
	
	/* List available devices */
	if (action_list_devices) {
		FILE *f = stdout;
		int i;
		char name[INFO_STR_SIZE], vendor[INFO_STR_SIZE];

		/* List devices */
		fprintf(f, "\n ID    Name, Vendor\n");
		fprintf(f, "----  ----------------------------------------------------\n");
		for (i = 0; i < n_all_devices; i++) {
			clGetDeviceInfo(all_devices[i], CL_DEVICE_NAME, INFO_STR_SIZE, name, NULL);
			clGetDeviceInfo(all_devices[i], CL_DEVICE_VENDOR, INFO_STR_SIZE, vendor, NULL);
			fprintf(f, " %2d    %s %s\n", i+1, name, vendor);
		}
		fprintf(f, "----------------------------------------------------------\n");
		fprintf(f, "\t%d devices available\n\n", n_all_devices);
	}
	
	/* release context with all devices and recreate with selected device */
	clReleaseContext(context);
	context = clCreateContext(cprops, n_devices, devices, 0, 0, &err);
	if (err != CL_SUCCESS)
		ocl_fatal(err, "creating device context failed");
	
	/* load precompiled kernels that shall be included */
	cl_program *bin_input_headers = 0;
	if (n_bin_includes > 0) {
		char *src;
		size_t size;
		
		if (n_devices > 1)
			fatal("the current implementation of -I only works with a single selected device");
		
		bin_input_headers = (cl_program*) malloc(sizeof(cl_program)*n_bin_includes);
		
		for (i=0;i<n_bin_includes;i++) {
			printf("Loading '%s'... ", bin_includes[i]);
			
			/* read file and create cl_program */
			src = read_file(bin_includes[i], &size);
			bin_input_headers[i] = clCreateProgramWithBinary(context, n_devices, devices, &size, (const unsigned char **) &src, 0, &err);
			if (err != CL_SUCCESS)
				ocl_fatal(err, "clCreateProgramWithBinary failed");
			free(src);
			
			/* get build options */
			err = clGetProgramBuildInfo(bin_input_headers[i], devices[0], CL_PROGRAM_BUILD_OPTIONS, 0, 0, &size);
			if (err != CL_SUCCESS)
				ocl_fatal(err, "clGetProgramBuildInfo failed");
			src = (char*) malloc(size);
			err = clGetProgramBuildInfo(bin_input_headers[i], devices[0], CL_PROGRAM_BUILD_OPTIONS, size, src, 0);
			if (err != CL_SUCCESS)
				ocl_fatal(err, "clGetProgramBuildInfo failed");
			printf("build_options=\"%s\" ", src);
			free(src);
			
			/* retrieve more detailed information if supported */
			if (opencl_api_version >= 12) {
				printf("bin_type=");
				cl_program_binary_type btype;
				err = clGetProgramBuildInfo(bin_input_headers[i], devices[0], CL_PROGRAM_BINARY_TYPE, sizeof(cl_program_binary_type), &btype, 0);
				if (err != CL_SUCCESS)
					ocl_fatal(err, "clGetProgramBuildInfo failed");
				switch (btype) {
					case CL_PROGRAM_BINARY_TYPE_NONE: printf("\"none\" "); break;
					case CL_PROGRAM_BINARY_TYPE_COMPILED_OBJECT: printf("\"compiled\" "); break;
					case CL_PROGRAM_BINARY_TYPE_LIBRARY: printf("\"library\" "); break;
					case CL_PROGRAM_BINARY_TYPE_EXECUTABLE: printf("\"executable\" "); break;
				}
				
				
				cl_program program;
				
// 				if (opencl_api_version < 12) {
// 					program = bin_input_headers[i];
// 					err = clBuildProgram(program, n_devices, devices, build_options, NULL, NULL);
// 				} else {
					program = l_clLinkProgram(context, n_devices, devices, link_options, 1, &bin_input_headers[i], 0, 0, &err);
// 				}
				
				if (err != CL_SUCCESS) {
					if (!program)
						ocl_fatal(err, "clLinkProgram failed (maybe the included binary does not match the selected device?)");
					
					int j;
					for (j=0;j<n_devices;j++) {
						cl_build_status bstatus;
						err = clGetProgramInfo(program, CL_PROGRAM_BUILD_STATUS, sizeof(bstatus), &bstatus, 0);
						if (err != CL_SUCCESS)
							ocl_fatal(err, "clGetProgramInfo failed");
						switch(bstatus) {
							case CL_BUILD_NONE: printf("none\n"); break;
							case CL_BUILD_ERROR: printf("error\n"); break;
							case CL_BUILD_SUCCESS: printf("success\n"); break;
							case CL_BUILD_IN_PROGRESS: printf("in progress\n"); break;
						}
					}
					
					show_build_log(program, n_devices, devices, err);
				}
				
				err = clGetProgramInfo(program, CL_PROGRAM_NUM_KERNELS, sizeof(size_t), &size, 0);
				if (err != CL_SUCCESS)
					ocl_fatal(err, "clGetProgramInfo failed");
				printf("n_kernels=%zu ", size);
				
				err = clGetProgramInfo(program, CL_PROGRAM_KERNEL_NAMES, 0, 0, &size);
				if (err != CL_SUCCESS)
					ocl_fatal(err, "clGetProgramInfo failed");
				src = (char*) malloc(size);
				err = clGetProgramInfo(program, CL_PROGRAM_KERNEL_NAMES, size, src, 0);
				if (err != CL_SUCCESS)
					ocl_fatal(err, "clGetProgramInfo failed");
				printf("kernels=\"%s\" ", src);
				free(src);
				
				// only show detailed info after link
				if (detailed_kernels) {
					printf("\n");
					show_kernel_info(program);
				}
				
				if (opencl_api_version >= 12)
					clReleaseProgram(program);
			}
			
			printf("\n");
		}
	}
	
	cl_program program = 0;
	cl_program *input_headers = 0;
	
	if (n_includes > 0)
		input_headers = (cl_program*) malloc(sizeof(cl_program)*n_includes);
	
	/* create program for all source files (the includes and main source file) */
	for (i=0;i<=n_includes;i++) {
		char *src;
		size_t size;
		
		
		if (i < n_includes) {
			printf("Loading '%s'...\n", includes[i]);
			src = read_file(includes[i], &size);
			input_headers[i] = clCreateProgramWithSource(context, 1, (const char **) &src, &size, &err);
		} else {
			if (!kernel_file_name)
				continue;
			printf("Loading '%s'...\n", kernel_file_name);
			src = read_file(kernel_file_name, &size);
			program = clCreateProgramWithSource(context, 1, (const char **) &src, &size, &err);
		}
		
		if (err != CL_SUCCESS)
			ocl_fatal(err, "clCreateProgramWithSource failed");
		
		free(src);
	}
	
	// compile sources
	if (kernel_file_name || (make_shared_lib && n_includes > 0)) {
		cl_int err;
		cl_program src_prog;
		char *name;
		
		for (i=0;i<=n_includes;i++) {
			if (i < n_includes) {
				src_prog = input_headers[i];
				name = includes[i];
			} else {
				if (!kernel_file_name)
					continue;
				src_prog = program;
				name = kernel_file_name;
			}
			
			if (opencl_api_version < 12) {
				if (n_includes > 0) {
					printf("\nOpenCL < v1.2 does not support explicitly specified include files.\n");
					printf("Please use -b to specify the include path for the header source,\n");
					printf("e.g., %s -b \"-I/path/to/header.h.cl\"\n\n", argv[0]);
				}
				
				printf("Building '%s'...\n", name);
				
				err = clBuildProgram(src_prog, n_devices, devices, build_options, NULL, NULL);
				if (err != CL_SUCCESS)
					show_build_log(src_prog, n_devices, devices, err);
			} else
			if (opencl_api_version == 12) {
				printf("Compiling '%s'...\n", name);
				
				err = l_clCompileProgram(src_prog, n_devices, devices, build_options, n_includes, input_headers,(const char**) includes, 0, 0);
				if (err != CL_SUCCESS)
					show_build_log(src_prog, n_devices, devices, err);
			} else
				fatal("failed to detect OpenCL API version");
			
			if (detailed_kernels) {
				cl_program kinfo;
				kinfo = l_clLinkProgram(context, n_devices, devices, link_options, 1, &src_prog, 0, 0, &err);
				show_kernel_info(kinfo);
			}
		}
	}
	
	// build list of all compiled objects and create library
	if (make_shared_lib) {
		char *final_link_options;
		cl_program *link_programs;
		size_t n_links, i,j;
		
		
		if (!filename && !kernel_file_name)
			fatal("Please specify a library file name\n");
		
		printf("Linking '%s'...\n", kernel_file_name?kernel_file_name:filename);
		
		if (link_options) {
			final_link_options = (char *) malloc(strlen("-create-library") + 1 + strlen(link_options) + 1);
			sprintf(final_link_options, "-create-library %s", link_options);
		} else {
			final_link_options = "-create-library";
		}
		
		n_links = n_includes + n_bin_includes;
		if (kernel_file_name)
			n_links++;
		
		link_programs = (cl_program *) malloc(sizeof(cl_program)*n_links);
		
		
		for (i=0,j=0;i<n_includes;i++,j++)
			link_programs[j] = input_headers[i];
		
		for (i=0;i<n_bin_includes;i++,j++)
			link_programs[j] = bin_input_headers[i];
		
		if (kernel_file_name)
			link_programs[j] = program;
		
		program = l_clLinkProgram(context, n_devices, devices, final_link_options, n_links, link_programs, 0, 0, &err);
		if (err != CL_SUCCESS)
			ocl_fatal(err, "clLinkProgram failed");
	}
	
	// write created library or kernel(s) into file(s)
	if (kernel_file_name || make_shared_lib) {
		cl_int err;
		
		char *bin_file_name;
		
		size_t *bin_sizes;
		size_t bin_sizes_ret;
		char **bin_bits;
		
		
		/* Get number and size of binaries */
		bin_sizes = (size_t*) malloc(sizeof(size_t)*n_devices);
		err = clGetProgramInfo(program, CL_PROGRAM_BINARY_SIZES, sizeof(size_t)*n_devices, bin_sizes, &bin_sizes_ret);
		if (err != CL_SUCCESS)
			ocl_fatal(err, "clGetProgramInfo CL_PROGRAM_BINARY_SIZES failed: %d", err);
		
		assert(bin_sizes_ret == sizeof(size_t)*n_devices);
		for (i=0;i<n_devices;i++) {
			if (bin_sizes[i] == 0)
				fatal("no binary returned for device %d", i+1);
		}

		// query binary data
		bin_bits = (char**) malloc(sizeof(char*)*n_devices);
		for (i=0;i<n_devices;i++)
			bin_bits[i] = malloc(bin_sizes[i]);
		err = clGetProgramInfo(program, CL_PROGRAM_BINARIES, sizeof(char*)*n_devices, bin_bits, &bin_sizes_ret);
		if (err != CL_SUCCESS)
			ocl_fatal(err, "clGetProgramInfo CL_PROGRAM_BINARIES failed: %d", err);
		assert(bin_sizes_ret == sizeof(char*)*n_devices);
		
		if (n_devices == 1) {
			// write one file
			
			if (!filename) {
				char *last;
				size_t prefix_len;
				last = strrchr(kernel_file_name, '.');
				prefix_len = last - kernel_file_name;
				if (!strcmp(last, ".cl")) {
					bin_file_name = (char*) malloc(prefix_len+4+1);
					strncpy(bin_file_name, kernel_file_name, prefix_len);
					strcpy(bin_file_name + prefix_len, ".bin");
				} else {
					bin_file_name = (char*) malloc(strlen(kernel_file_name)+4+1);
					sprintf(bin_file_name, "%s.bin", kernel_file_name);
				}
			} else {
				bin_file_name = (char*) malloc(strlen(filename)+1);
				strcpy(bin_file_name, filename);
			}
			
			write_to_file(bin_file_name, bin_bits[0], bin_sizes[0]);
			free(bin_bits[0]);
			
			printf("Successfully created kernel binary '%s'\n", bin_file_name);
		} else {
			// create a file for each device
			
			for (i=0;i<n_devices;i++) {
				char name[INFO_STR_SIZE];
				size_t name_len;
				char *last;
				size_t prefix_len;
				int j;
				
				clGetDeviceInfo(devices[i], CL_DEVICE_NAME, INFO_STR_SIZE, name, NULL);
				name_len = strlen(name);
				
				for (j=0;j<name_len;j++)
					if (name[j] == ' ')
						name[j] = '_';
				
				if (!filename)
					filename = kernel_file_name;
				
				last = strrchr(filename, '.');
				prefix_len = last - kernel_file_name;
				
				bin_file_name = (char*) malloc(prefix_len + 1 + name_len + 4 + 1);
				strncpy(bin_file_name, filename, prefix_len);
				bin_file_name[prefix_len] = '_';
				strcpy(bin_file_name + prefix_len + 1, name);
				strcpy(bin_file_name + prefix_len + 1 + name_len, ".bin");
				
				write_to_file(bin_file_name, bin_bits[i], bin_sizes[i]);
				
				printf("Successfully created kernel binary '%s'\n", bin_file_name);
				
				free(bin_bits[i]);
				free(bin_file_name);
			}
		}
	}
	
	printf("\n");
	
	return 0;
}

