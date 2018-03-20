#include <syscall.hpp>

#include <iostream>
#include <errno.h>
#include <signal.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <fcntl.h>
#include <ostream>

using namespace dbt;

int LinuxSyscallManager::processSyscall(Machine& M) {
  SyscallType SysTy = static_cast<SyscallType>(M.getRegister(4) - 4000);

  switch (SysTy) {
  case SyscallType::Exit:
    ExitStatus = M.getRegister(2);
    std::cerr << "Exiting with status " << (uint32_t) ExitStatus << " (" << M.getRegister(2) << ")\n"; 
    return 1; 
  case SyscallType::Fstat: {
//    int r = fstat(M.getRegister(5), (struct stat*) (M.getByteMemoryPtr() + (M.getRegister(6) - M.getDataMemOffset())));
//		M.setRegister(2, r);
		M.setRegister(2, -1);
    return 0; 
  }
  //GET/PUT right registers and memory locations
  case SyscallType::Read:{
    //fflush(stdin);
    ssize_t r = read(M.getRegister(5), (M.getByteMemoryPtr() + (M.getRegister(6) - M.getDataMemOffset())), M.getRegister(7));
    M.setRegister(2, r);
    return 0;
  } 
  case SyscallType::Write: {
    //M.setMemValueAt((M.getRegister(6) - M.getDataMemOffset(), 8);
    ssize_t r = write(M.getRegister(5), (M.getByteMemoryPtr() + (M.getRegister(6) - M.getDataMemOffset())), M.getRegister(7));
    //uint32_t value = (M.getRegister(6) - M.getDataMemOffset());
    M.setRegister(2, r);
    return 0;
  }

  case SyscallType::Open: {
    char* filename = M.getByteMemoryPtr() + (M.getRegister(5) - M.getDataMemOffset());
    char* flag = M.getByteMemoryPtr() + (M.getRegister(5) - M.getDataMemOffset()+strlen(filename)+1);
    ssize_t r = -1;

    if(strcmp(flag, "r") == 0)
      r = open(filename, O_RDONLY);
    else if(strcmp(flag, "w") == 0)
      r = open(filename, O_WRONLY);
    
    M.setRegister(2, r);
    assert(r >= 0 && "Error with file descriptor..");
    return 0;
  }

  case SyscallType::Close: {
    ssize_t r = close(M.getRegister(5));
    M.setRegister(2, r);
    return 0;
  }

  default:
    std::cerr << "Syscall (" << SysTy << ") not implemented!\n";
    exit(2);
    break;
  }
  return 0;
}
