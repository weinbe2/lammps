KOKKOS_PATH = ../..

GTEST_PATH = ../../TPL/gtest

vpath %.cpp ${KOKKOS_PATH}/containers/performance_tests

default: build_all
	echo "End Build"

ifneq (,$(findstring Cuda,$(KOKKOS_DEVICES)))
  CXX = $(KOKKOS_PATH)/bin/nvcc_wrapper
else
  CXX = g++
endif

CXXFLAGS = -O3
LINK ?= $(CXX)
LDFLAGS ?=
override LDFLAGS += -lpthread

KOKKOS_USE_DEPRECATED_MAKEFILES=1

include $(KOKKOS_PATH)/Makefile.kokkos

KOKKOS_CXXFLAGS += -I$(GTEST_PATH) -I${KOKKOS_PATH}/containers/performance_tests

TEST_TARGETS =
TARGETS =

ifeq ($(KOKKOS_INTERNAL_USE_CUDA), 1)
	OBJ_CUDA = TestCuda.o TestMain.o gtest-all.o
	TARGETS += KokkosContainers_PerformanceTest_Cuda
	TEST_TARGETS += test-cuda
endif

ifeq ($(KOKKOS_INTERNAL_USE_HIP), 1)
	OBJ_HIP = TestHIP.o TestMain.o gtest-all.o
	TARGETS += KokkosContainers_PerformanceTest_HIP
	TEST_TARGETS += test-hip
endif

ifeq ($(KOKKOS_INTERNAL_USE_THREADS), 1)
	OBJ_THREADS = TestThreads.o TestMain.o gtest-all.o
	TARGETS += KokkosContainers_PerformanceTest_Threads
	TEST_TARGETS += test-threads
endif

ifeq ($(KOKKOS_INTERNAL_USE_OPENMP), 1)
	OBJ_OPENMP = TestOpenMP.o TestMain.o gtest-all.o
	TARGETS += KokkosContainers_PerformanceTest_OpenMP
	TEST_TARGETS += test-openmp
endif

ifeq ($(KOKKOS_INTERNAL_USE_HPX), 1)
	OBJ_HPX = TestHPX.o TestMain.o gtest-all.o
	TARGETS += KokkosContainers_PerformanceTest_HPX
	TEST_TARGETS += test-hpx
endif

KokkosContainers_PerformanceTest_Cuda: $(OBJ_CUDA) $(KOKKOS_LINK_DEPENDS)
	$(LINK) $(KOKKOS_LDFLAGS) $(LDFLAGS) $(EXTRA_PATH) $(OBJ_CUDA) $(KOKKOS_LIBS) $(LIB) -o KokkosContainers_PerformanceTest_Cuda

KokkosContainers_PerformanceTest_HIP: $(OBJ_HIP) $(KOKKOS_LINK_DEPENDS)
	$(LINK) $(KOKKOS_LDFLAGS) $(LDFLAGS) $(EXTRA_PATH) $(OBJ_HIP) $(KOKKOS_LIBS) $(LIB) -o KokkosContainers_PerformanceTest_HIP

KokkosContainers_PerformanceTest_Threads: $(OBJ_THREADS) $(KOKKOS_LINK_DEPENDS)
	$(LINK) $(KOKKOS_LDFLAGS) $(LDFLAGS) $(EXTRA_PATH) $(OBJ_THREADS) $(KOKKOS_LIBS) $(LIB) -o KokkosContainers_PerformanceTest_Threads

KokkosContainers_PerformanceTest_OpenMP: $(OBJ_OPENMP) $(KOKKOS_LINK_DEPENDS)
	$(LINK) $(KOKKOS_LDFLAGS) $(LDFLAGS) $(EXTRA_PATH) $(OBJ_OPENMP) $(KOKKOS_LIBS) $(LIB) -o KokkosContainers_PerformanceTest_OpenMP

KokkosContainers_PerformanceTest_HPX: $(OBJ_HPX) $(KOKKOS_LINK_DEPENDS)
	$(LINK) $(KOKKOS_LDFLAGS) $(LDFLAGS) $(EXTRA_PATH) $(OBJ_HPX) $(KOKKOS_LIBS) $(LIB) -o KokkosContainers_PerformanceTest_HPX

test-cuda: KokkosContainers_PerformanceTest_Cuda
	./KokkosContainers_PerformanceTest_Cuda

test-hip: KokkosContainers_PerformanceTest_HIP
	./KokkosContainers_PerformanceTest_HIP

test-threads: KokkosContainers_PerformanceTest_Threads
	./KokkosContainers_PerformanceTest_Threads

test-openmp: KokkosContainers_PerformanceTest_OpenMP
	./KokkosContainers_PerformanceTest_OpenMP

test-hpx: KokkosContainers_PerformanceTest_HPX
	./KokkosContainers_PerformanceTest_HPX

build_all: $(TARGETS)

test: $(TEST_TARGETS)

clean: kokkos-clean
	rm -f *.o $(TARGETS)

# Compilation rules

%.o:%.cpp $(KOKKOS_CPP_DEPENDS)
	$(CXX) $(KOKKOS_CPPFLAGS) $(KOKKOS_CXXFLAGS) $(CXXFLAGS) $(EXTRA_INC) -c $<

gtest-all.o:$(GTEST_PATH)/gtest/gtest-all.cc
	$(CXX) $(KOKKOS_CPPFLAGS) $(KOKKOS_CXXFLAGS) $(CXXFLAGS) $(EXTRA_INC) -c $(GTEST_PATH)/gtest/gtest-all.cc
