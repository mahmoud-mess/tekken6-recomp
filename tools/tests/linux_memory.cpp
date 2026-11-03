// Compare normal execution with REX_LINUX_FAST_MAP_QUERY=1.
#include <rex/memory/utils.h>
#include <sys/mman.h>
#include <unistd.h>
#include <sys/wait.h>
#include <chrono>
#include <iostream>
#include <stdexcept>
void check(bool value) { if (!value) throw std::runtime_error("memory query regression"); }
int main() {
  using rex::memory::PageAccess;
  const size_t page = sysconf(_SC_PAGESIZE), count = 512;
  auto* base = static_cast<unsigned char*>(mmap(nullptr, count*page, PROT_READ|PROT_WRITE,
      MAP_PRIVATE|MAP_ANONYMOUS, -1, 0));
  check(base != MAP_FAILED);
  for (size_t n=0; n<count; n+=2) check(mprotect(base+n*page,page,PROT_READ)==0);
  auto query = [&](size_t n, PageAccess expected) {
    size_t length=page; PageAccess access{};
    check(rex::memory::QueryProtect(base+n*page,length,access));
    check(length>=page && access==expected);
  };
  query(0,PageAccess::kReadOnly); query(1,PageAccess::kReadWrite);
  check(mprotect(base+page,page,PROT_NONE)==0); query(1,PageAccess::kNoAccess);
  check(mprotect(base+page,page,PROT_READ|PROT_WRITE)==0); query(1,PageAccess::kReadWrite);
  // A child must query its own protections, not an inherited /proc parent fd.
  const pid_t child = fork();
  check(child >= 0);
  if (child == 0) {
    check(mprotect(base+page,page,PROT_READ)==0);
    query(1,PageAccess::kReadOnly);
    _exit(0);
  }
  int status=0; check(waitpid(child,&status,0)==child);
  check(WIFEXITED(status) && WEXITSTATUS(status)==0);
  query(1,PageAccess::kReadWrite);
  const auto start=std::chrono::steady_clock::now();
  for(int i=0;i<2000;++i) query(count-1,PageAccess::kReadWrite);
  const double ms=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-start).count();
  check(munmap(base,count*page)==0);
  size_t length=page; PageAccess access{};
  check(!rex::memory::QueryProtect(base+page,length,access));
  std::cout<<"PASS: RO/RW/no-access, live mprotect changes, unmapped query; 2000 queries: "<<ms<<" ms\n";
}
