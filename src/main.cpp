


#include <stdio.h>
#include <fstream>
#include <iostream>
#include <string>
#include <iterator>
#include <sstream>
#include <vector>
#include <math.h>
#include <thread>   

#if defined(_WIN32)
#include <windows.h>
#endif

#include "../include/main.hpp"
#include "../include/ocl_util.hpp"
#include "../include/util.hpp"
#include "../include/hdf5_io.hpp"

#include "hdf5.h"
#include "hdf5_hl.h"

#define CL_HPP_ENABLE_EXCEPTIONS
#define CL_HPP_MINIMUM_OPENCL_VERSION 120
#define CL_HPP_TARGET_OPENCL_VERSION 120

#include <CL/cl2.hpp>
#include "../include/ocl_dev_mgr.hpp"


 using namespace std;

    
int main(int argc, char *argv[]) {

    
    Timer timer; //used to track performance
    
   
    cl_uint  deviceIndex   =      0; //set default OpenCL Device
  
    ofstream logfile;
	logfile.open("log.txt");
             
    //parse command line arguments
    parseArguments(argc, argv,&deviceIndex);
    
    
   ocl_dev_mgr& dev_mgr = ocl_dev_mgr::getInstance();
   cl_uint devices_availble=dev_mgr.get_avail_dev_num();   
    
    cout<<"Available devices: "<<devices_availble<<endl;

   
  // for (unsigned int i=0; i<dev_mgr.get_dev_names(dev_names);i++) {
   uint i=deviceIndex;
       cout<<dev_mgr.get_avail_dev_info(i).name.c_str()<<endl;
       cout<< "OpenCL version: "<<dev_mgr.get_avail_dev_info(i).ocl_version.c_str()<<endl;
       cout<<"Memory limit: "<<dev_mgr.get_avail_dev_info(i).max_mem<<endl;
       cout<<"WG limit: "<<dev_mgr.get_avail_dev_info(i).wg_size<<endl<<endl;
       dev_mgr.init_device(i);   
  // } 
  
  char filename[500];  
    sprintf(filename,"data.h5"); //path to input HDF5 data file

  char kernel_url[500]; 
 h5_read_string(filename, "Kernel_URL",kernel_url);

std::vector<std::string> kernel_list;
  
 h5_read_strings(filename, "Kernels",kernel_list);


dev_mgr.add_program_url(0,"ocl_Kernel",kernel_url);

	char settings[1024];
 h5_read_string(filename, "Settings",settings);


uint64_t kernels_found = 0;

	kernels_found = dev_mgr.compile_kernel(0, "ocl_Kernel", settings);

	if (kernels_found == 0) {
		cout<<"No valid kernels found"<<endl;
		return -1;
	}
    
	std::vector<std::string> found_kernels;
	dev_mgr.get_kernel_names(0,"ocl_Kernel",found_kernels) ;
cout <<"Found Kernels: "<<found_kernels.size()<<endl;


cout <<"Number of Kernels to execute: "<<kernel_list.size()<<endl;

/*
for (uint32_t kernel_idx = 0; kernel_idx < kernel_list.size(); kernel_idx++) {
	cout <<"Found : "<< kernel_list.at(kernel_idx) << endl;
}
*/
cout<<"Ingesting HDF5..."<<endl;

    	std::vector<std::string> data_list;
        std::vector<HD5_Type> datatype_list;
        std::vector<size_t> data_size;
  h5_get_content(filename,"/Data/",data_list,datatype_list,data_size);


	std::vector<cl::Buffer> data_in;
    	bool blocking = CL_TRUE;

double  *rw_flags_ptr;
rw_flags_ptr = new double[data_list.size()];
		std::fill(rw_flags_ptr, rw_flags_ptr + data_list.size(), 0);


for(cl_uint i=0;i<data_list.size();i++) {
    try {


 uint8_t *tmp_data = 0;
 //float *tmp_data = 0;
size_t var_size=0;

switch (datatype_list.at(i)){
case H5_float: var_size=data_size.at(i)*sizeof(cl_float); tmp_data = new uint8_t[var_size]; h5_read_buffer_float(filename, data_list.at(i).c_str(),(float *)tmp_data); break;
case H5_double: var_size=data_size.at(i)*sizeof(cl_double); tmp_data = new uint8_t[var_size]; h5_read_buffer_double(filename, data_list.at(i).c_str(),(double *)tmp_data); break;
case H5_uchar: var_size=data_size.at(i)*sizeof(cl_uchar); tmp_data = new uint8_t[var_size]; h5_read_buffer_uchar(filename, data_list.at(i).c_str(),(cl_uchar *)tmp_data); break;
case H5_char: var_size=data_size.at(i)*sizeof(cl_char); tmp_data = new uint8_t[var_size]; h5_read_buffer_char(filename, data_list.at(i).c_str(),(cl_char *)tmp_data); break;
case H5_uint: var_size=data_size.at(i)*sizeof(cl_uint); tmp_data = new uint8_t[var_size]; h5_read_buffer_uint(filename, data_list.at(i).c_str(),(cl_uint *)tmp_data); break;
case H5_int: var_size=data_size.at(i)*sizeof(cl_int); tmp_data = new uint8_t[var_size]; h5_read_buffer_int(filename, data_list.at(i).c_str(),(cl_int *)tmp_data); break;
//case default: break;
}

				switch ((uint32_t)round(rw_flags_ptr[i])) {
				case 0:	data_in.push_back(cl::Buffer(dev_mgr.get_context(0), CL_MEM_READ_WRITE | CL_MEM_ALLOC_HOST_PTR, var_size)); dev_mgr.get_queue(0, 0).enqueueWriteBuffer(data_in.at(data_in.size() - 1), blocking, 0, var_size,(tmp_data));  break;
				case 1:	data_in.push_back(cl::Buffer(dev_mgr.get_context(0), CL_MEM_READ_ONLY | CL_MEM_ALLOC_HOST_PTR, var_size));  dev_mgr.get_queue(0, 0).enqueueWriteBuffer(data_in.at(data_in.size() - 1), blocking, 0, var_size,(tmp_data));  break;
				case 2:	data_in.push_back(cl::Buffer(dev_mgr.get_context(0), CL_MEM_WRITE_ONLY | CL_MEM_ALLOC_HOST_PTR, var_size)); break;
				}

				for (uint32_t kernel_idx = 0; kernel_idx < found_kernels.size(); kernel_idx++) {
				dev_mgr.getKernelbyName(0, "ocl_Kernel", found_kernels.at(kernel_idx))->setArg(i, data_in.at(data_in.size() - 1));
				}
   delete[] tmp_data;                     
    tmp_data = 0;    
			}
			catch (cl::Error err) {
				cout<<"error"<<endl;
			}

}

	dev_mgr.get_queue(0, 0).finish();//Buffer Copy is asynchornous

	cout << "Launching kernel..." << endl;

cl::NDRange range_start;
cl::NDRange global_range;
cl::NDRange local_range;

int tmp_range[3];
h5_read_buffer_int(filename, "Global_Range", tmp_range);
global_range = cl::NDRange(tmp_range[0], tmp_range[1], tmp_range[2]);

h5_read_buffer_int(filename, "Range_Start", tmp_range);
range_start = cl::NDRange(tmp_range[0], tmp_range[1], tmp_range[2]);

h5_read_buffer_int(filename, "Local_Range", tmp_range);
if ((tmp_range[0]==0)&&(tmp_range[1]==0)&&(tmp_range[2]==0)){
local_range=cl::NullRange;	
} else{
local_range = cl::NDRange(tmp_range[0], tmp_range[1], tmp_range[2]);
}

	uint64_t exec_time = 0;
	uint32_t kernels_run=0;
	for (uint32_t kernel_idx = 0; kernel_idx < kernel_list.size(); kernel_idx++){
		exec_time = exec_time + dev_mgr.execute_kernelNA(*(dev_mgr.getKernelbyName(0, "ocl_Kernel", kernel_list.at(kernel_idx))), dev_mgr.get_queue(0, 0), range_start,global_range, local_range);
	 kernels_run++;	
	}

cout<<"Kernels executed: "<<kernels_run<<endl;
cout<<"Kernel runtime: "<<exec_time/1000<<" ms"<<endl;
              char out_name[500];  
    sprintf(out_name,"out.h5");

h5_write_single_long(out_name,"Kernel_Time",exec_time);



h5_create_dir(out_name,"/Data");

	uint32_t buffer_counter = 0;
  

for(cl_uint i=0;i<data_list.size();i++) {
    try {
 //   uint8_t *tmp_data = 0;
    uint8_t *tmp_data = 0;
size_t var_size=0;
switch (datatype_list.at(i)){
case H5_float:  var_size=data_size.at(i)*sizeof(cl_float);   break;
case H5_double: var_size=data_size.at(i)*sizeof(cl_double); break;
case H5_char: var_size=data_size.at(i)*sizeof(cl_char); break;
case H5_uchar: var_size=data_size.at(i)*sizeof(cl_uchar); break;
case H5_uint: var_size=data_size.at(i)*sizeof(cl_uint); break;
case H5_int: var_size=data_size.at(i)*sizeof(cl_int); break;
 default:  var_size=data_size.at(buffer_counter)*sizeof(cl_double); break;
}
tmp_data = new uint8_t[var_size];

				switch ((uint32_t)round(rw_flags_ptr[buffer_counter])) {

				case 0:  dev_mgr.get_queue(0, 0).enqueueReadBuffer(data_in.at(buffer_counter), blocking, 0, var_size,(float *)tmp_data); break;
				case 1: break;
				case 2: dev_mgr.get_queue(0, 0).enqueueReadBuffer(data_in.at(buffer_counter), blocking, 0, var_size, (float *)tmp_data); break;
				}

			
	dev_mgr.get_queue(0, 0).finish();//Buffer Copy is asynchornous
switch (datatype_list.at(i)){
case H5_float: h5_write_buffer_float(out_name,data_list.at(i).c_str(),(float *)tmp_data,data_size.at(buffer_counter)); break;
case H5_double: h5_write_buffer_double(out_name,data_list.at(i).c_str(),(double *)tmp_data,data_size.at(buffer_counter)); break;
case H5_char: h5_write_buffer_char(out_name,data_list.at(i).c_str(),(cl_char *)tmp_data,data_size.at(buffer_counter)); break;
case H5_uchar: h5_write_buffer_uchar(out_name,data_list.at(i).c_str(),(cl_uchar *)tmp_data,data_size.at(buffer_counter)); break;
case H5_uint: h5_write_buffer_uint(out_name,data_list.at(i).c_str(),(cl_uint *)tmp_data,data_size.at(buffer_counter)); break;
case H5_int: h5_write_buffer_int(out_name,data_list.at(i).c_str(),(cl_int *)tmp_data,data_size.at(buffer_counter)); break;
}
   delete[] tmp_data;                     
   // tmp_data = 0; 
       	buffer_counter++;
			}
			catch (cl::Error err) {
					cout<<"error"<<endl;
			}
	
	}






}
