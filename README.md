ocl-ke (OpenCL kernel extractor)
================================

```
Syntax: ocl-ke [<options>] <kernel.cl>

ocl-ke uses the OpenCL API to compile kernels for the selected device
and stores the resulting binary code in a file. Afterwards, applications
can use clCreateProgramWithBinary instead of compiling the kernel during
every application run.

Options:
        -L            Print list of available platforms
        -l            Print list of available devices
        -a            Activate CL_CONTEXT_OFFLINE_DEVICES_AMD extension
        -p <plat_idx> Index of the desired platform (default: 0)
        -d <dev_idx>  Index of the desired device (default: 0)
```

Like regular OpenCL applications, ocl-ke calls the `clCreateProgramWithSource` function to compile the OpenCL kernel source code in the given file but queries the OpenCL vendor implementation for the resulting binary code using `clGetProgramInfo` afterwards and stores it into a separate file. Consequently, an application can use `clCreateProgramWithBinary` to avoid compiling the kernel code during every application run. Hence, it acts as an offline compiler for OpenCL kernels but contains no compiler functionality itself and depends completely on the provided OpenCL vendor libraries.

With the `-a` flag, ocl-ke also activates the `CL_CONTEXT_OFFLINE_DEVICES_AMD` extension on supported platforms to enable the compilation even for devices that are not available in the current system.

Please note, the resulting file may contain actual hardware instructions for the specific device or just another intermediate representation. The actual behavior is implementation-specific and not regulated by the OpenCL specification. Hence, using cached kernels may only partially reduce the initialization overhead for an application.

If you want to use multiple OpenCL libraries in parallel, please see the documentation of the [OpenCL ICD extension](https://www.khronos.org/registry/cl/extensions/khr/cl_khr_icd.txt).

The ocl-ke code is based on the m2s-opencl-kc tool from [multi2sim simulator](https://www.multi2sim.org/) v3.0.3 that was written by Rafael Ubal Tena and released under the GPLv3 license.