ocl-ke (OpenCL kernel extractor)
================================

```
Syntax: ocl-ke [<options>] <kernel.cl>

ocl-ke uses the OpenCL API to compile OpenCL code for the selected devices
and stores the resulting binary code in a file. Afterwards, applications
can load the kernels with clCreateProgramWithBinary instead of compiling
the kernels during every application run.

Options:
        -L              Print list of available platforms
        -l              Print list of available devices for selected platform
        -e              Print list of supported extensions
        -a              Activate CL_CONTEXT_OFFLINE_DEVICES_AMD extension, if
                        available, to choose from the list of all supported
                        devices by the compiler instead of only the available
                        devices in the current system.
        -k              Show detailed information about included kernels
        -p <plat_idx>   Index of the desired platform (default: 1)
        -d <dev_idx>    Index of the desired device (default: 1)
                        A value of 0 equals all devices on the platform.
                        This option can be specified multiple times to select
                        multiple devices but this is not supported by all
                        available operations. 
        -b <build_opts> Build options that are passed to the compiler
        -B <link_opts>  Options passed to the linker
        -s              Create a kernel library instead of an executable
                        (OpenCL 1.2 or higher only)
        -i <source>     Include this source file (OpenCL 1.2 or higher only)
                        This option can be specified multiple times.
        -I <binary>     Include this binary file (OpenCL 1.2 or higher only)
                        This option can be specified multiple times.
        -o <filename>   Write binary code into this file instead of ${source}.bin
        -O              Write binary code into ${source}_${device name}.bin
                        Special characters in the device name will be replaced by
                        underscore. This option is enabled by default if multiple
                        devices are selected.
```

Like regular OpenCL applications, ocl-ke calls the `clCreateProgramWithSource` function to compile the OpenCL kernel source code in the given file but queries the OpenCL vendor implementation for the resulting binary code using `clGetProgramInfo` afterwards and stores it into a separate file. Consequetnly, an application can use `clCreateProgramWithBinary` to avoid compiling the kernel code during every application run. Hence, it acts as an offline compiler for OpenCL kernels but contains no compiler functionality itself and depends completely on the provided OpenCL vendor libraries.

Please note, the resulting file may contain actual hardware instructions for the specific device or just another intermediate representation. The actual behavior is implementation-specific and not regulated by the OpenCL specification. Hence, using cached kernels may only partially reduce the initialization overhead for an application.

If you want to use multiple OpenCL libraries in parallel, please see the documentation of the [OpenCL ICD extension](https://www.khronos.org/registry/cl/extensions/khr/cl_khr_icd.txt).

The ocl-ke code is based on the m2s-opencl-kc tool from [multi2sim simulator](https://www.multi2sim.org/) v3.0.3 that was written by Rafael Ubal Tena and released under the GPLv3 license.

Usage
-----

Compile a kernel for the first device on the first platform and store the result in `mykernel.bin`:

`ocl-ke mykernel.cl`

Compile a kernel for the second device on the third platform and store the result in `mykernel_2nd_device.bin`:

`ocl-ke -p 3 -d 2 mykernel.cl -o mykernel_2nd_device.bin`

Compile `mykernel.cl` and use current directory to look for include files:

`ocl-ke -b "-I ./" mykernel.cl`

Compile `mykernel.cl` and include `myinclude.h.cl` (OpenCL v1.2 and higher only):

`ocl-ke -i myinclude.h.cl mykernel.cl`

Create a kernel library `library.bin` from uncompiled `mykernel1.cl` and precompiled `mykernel2.bin` for the
first device on the first platform (OpenCL v1.2 and higher only):

`ocl-ke -i mykernel1.cl -I mykernel2.bin -s -o library.bin`

Show information about binary files and included kernels for the default platform and device:

```
# ocl-ke -I library.bin -k
Loading 'library.bin'... build_options="" bin_type="library" n_kernels=2 kernels="mykernel1;mykernel2"
        kernel mykernel1(__global int* a, __global int* b, int c)
        kernel mykernel2(__global int* c, __global int* d, int e)
```