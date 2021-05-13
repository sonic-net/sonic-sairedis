#include "swss/logger.h"

#ifdef DEBUG
#include <signal.h>
#include <cassert>
#include <execinfo.h>
#include <regex>
#include <iostream>
#include <cxxabi.h>

void dump_stack(int sig, siginfo_t *info, void *context)
{
  constexpr unsigned char max_stack_depth = 32;
  // Get the stack
  void *stack[max_stack_depth];
  auto stack_depth = backtrace(stack, max_stack_depth);
  char **stack_lines = backtrace_symbols(stack, stack_depth);

  //-----------------------------------------------------------------------------------------------------------------
  // Ignore the first frame as it points into this method.
  // For the rest, attempt to demangle the functions.
  //-----------------------------------------------------------------------------------------------------------------
  const std::regex re_location(R"([^(]+\(([^+]+)\+0x([0-9a-f]+)\).*)");
  int rc;
  char *demangled;
  std::cerr << std::endl;
  for (auto i = 1; i < stack_depth; i++)
  {
    std::cmatch match;
    std::regex_match(stack_lines[i], match, re_location);
    rc = -1;
    demangled = nullptr;
    if (match.size() == 3)
    {
      demangled = abi::__cxa_demangle(match[1].str().c_str(), nullptr, nullptr, &rc);
    }

    if (demangled != nullptr)
    {
      // TODO: Figure out how to get line numbers
      std::cerr << demangled << std::endl;
      free(demangled);
    }
    else
    {
      std::cerr << stack_lines[i] << std::endl;
    }
  }
  free(stack_lines);

  // Terminate.
  raise(sig);
}
#endif

int syncd_main(int argc, char **argv);

int main(int argc, char **argv)
{
    SWSS_LOG_ENTER();

    #ifdef DEBUG
    // Install signal handling so we can get useful crash info.
    struct sigaction sa = {};
    sa.sa_sigaction = &dump_stack;
    sa.sa_flags = SA_SIGINFO | SA_RESETHAND;
    auto rc = sigaction(SIGSEGV, &sa, nullptr);
    assert(rc == 0);
    rc = sigaction(SIGABRT, &sa, nullptr);
    assert(rc == 0);
    #endif

    return syncd_main(argc, argv);
}
