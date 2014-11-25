# To be included before find_package(MPI) so that
# Intel MPI is propery detected from I_MPI_ROOT

if(WIN32)
    option(USE_INTEL_MPI "Use Intel MPI if available" ON)
else()
    option(USE_INTEL_MPI "Use Intel MPI if available" OFF)
endif()

if(USE_INTEL_MPI)
    # mpicc option for using multithread MPI library
    # This is not needed for hmat for now
    #set(ENV{I_MPI_LINK} "opt_mt")

    if( CMAKE_SIZEOF_VOID_P EQUAL 8 )
        if(WIN32)
            set(MPI_ARCH "em64t")
        else()
            set(MPI_ARCH "intel64")
        endif()
    else( CMAKE_SIZEOF_VOID_P EQUAL 8 )
        set(MPI_ARCH "ia32")
    endif( CMAKE_SIZEOF_VOID_P EQUAL 8 )

    find_program(MPI_C_COMPILER NAMES mpiicc mpiicc.bat
        HINTS $ENV{I_MPI_ROOT}/${MPI_ARCH} $ENV{I_MPI_ROOT}/${MPI_ARCH}/bin
        ${I_MPI_ROOT}/${MPI_ARCH} ${I_MPI_ROOT}/${MPI_ARCH}/bin
    )

    set(MPI_CXX_COMPILER ${MPI_C_COMPILER})

    if(WIN32)
        find_path(MPI_INCLUDE_PATH NAMES mpi.h
            HINTS $ENV{I_MPI_ROOT}/${MPI_ARCH}/include ${I_MPI_ROOT}/${MPI_ARCH}/include
            NO_DEFAULT_PATH)

        set(MPI_LIB_SUFFIX "" CACHE STRING "Set to d to link against Debug libraries")
        find_library(MPI_C_LIBRARIES NAMES impi${MPI_LIB_SUFFIX}.lib
            HINTS $ENV{I_MPI_ROOT}/${MPI_ARCH}/lib ${I_MPI_ROOT}/${MPI_ARCH}/lib)
        set(MPI_CXX_LIBRARIES ${MPI_C_LIBRARIES})
    else()
        # Let FindMPI do the job
    endif()
endif()
