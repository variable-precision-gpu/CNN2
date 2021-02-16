EXEC_FILE_HARDWARE = cnn2_hardware
EXEC_FILE_SIM = cnn2_sim

CPU_SOURCE_FILES := $(shell find $(SOURCEDIR) -name '*.cpp')
GPU_SOURCE_FILES := $(shell find $(SOURCEDIR) -name '*.cu')

NVCC_FLAGS := -std=c++11 -gencode arch=compute_60,code=compute_60

build: 
	nvcc ${CPU_SOURCE_FILES} ${GPU_SOURCE_FILES} ${NVCC_FLAGS} -o ${EXEC_FILE_HARDWARE}
	nvcc ${CPU_SOURCE_FILES} ${GPU_SOURCE_FILES} ${NVCC_FLAGS} -o ${EXEC_FILE_SIM} --cudart shared