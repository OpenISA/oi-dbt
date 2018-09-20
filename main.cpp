#include <RFT.hpp>
#include <algorithm>
#include <arglib/arglib.hpp>
#include <interpreter.hpp>
#include <manager.hpp>
#include <syscall.hpp>
#include <timer.hpp>

#include <iostream>
#include <machine.hpp>
#include <memory>

clarg::argString AOSFlag("-aos", "Automatic optimization selection input file",
                         "");
clarg::argString RFTFlag("-rft", "Region Formation Technique (net)",
                         "netplus-e-r");
clarg::argInt HotnessFlag("-hot", "Hotness threshold for the RFTs", 50);
clarg::argString ReportFileFlag("-report", "Write down report to a file", "");
clarg::argBool InterpreterFlag("-interpret", "Only interpret.");
clarg::argString
    BinaryFlag("-bin", "Path to the binary which will should be emulated.", "");
clarg::argBool PreheatFlag("-p", "Run one time to compile all regions and then "
                                 "reexecute measuring the time.");
clarg::argBool VerboseFlag("-v", "display the compiled regions");
clarg::argBool HelpFlag("-h", "display the help message");
clarg::argInt RegionLimitSize("-l", "region size limit", 0);
clarg::argString ToCompileFlag("-tc", "Functions to compile", "");
clarg::argString
    ArgumentsFlag("-args", "Pass Parameters to binary file (as string)", "");
clarg::argInt StackSizeFlag("-stack", "Set new stack size. (Default: 128mb)",
                            STACK_SIZE);
clarg::argInt HeapSizeFlag("-heap", "Set new heap size (Default: 128mb)",
                           HEAP_SIZE);
clarg::argInt NumThreadsFlag("-threads",
                             "Number of compilation threads (min 1)", 1);
clarg::argString RegionPath("-reg", "Set default path to load region files",
                            "./");

clarg::argBool WholeCompilationFlag(
    "-wc",
    "load .bc files and compile them all as one region (whole compilation).");

/* Iterative Compiler Tools */
clarg::argBool DumpRegionsFlag("-dr", "Dump Regions (llvm ir and OI) to files");
clarg::argBool DumpOIRegionsFlag("-doi", "Dump OI Regions to files");
clarg::argBool LoadRegionsFlag("-lr", "Load Regions (.bc) from files");
clarg::argBool LoadOIFlag("-loi", "Load Regions (.oi) from files");
clarg::argBool MergeOIFlag("-moi", "Merge OI Regions before dumping");
clarg::argString CustomOptsFlag("-opts",
                                "path to regions optimization list file", "");

#ifdef DEBUG
clarg::argInt debugFlag("-d",
                        "Set Debug Level. This value can be 1 or 2 (1 - Less "
                        "verbosive; 2 - More Verbosive)",
                        1);
#endif

void usage(char *PrgName) {
  cout << "Version: 0.0.1 (09-04-2018)";
#ifdef DEBUG
  cout << " (Debug Build) ";
#endif
  cout << "\n\n";
  cout << "Usage: " << PrgName
       << " [-rft {net, net-r, mret2, lef, lei, netplus, netplus-e-r, mb}] "
          "[-interpret] -bin PathToBinary\n\n";

  cout << "DESCRIPTION:\n";
  cout
      << "This program implements the OpenISA DBT (Dynamic Binary Translator)\n"
      << "Vanderson Martins do Rosario <vandersonmr2@gmail.com>, 2018.\n\n";

  cout << "ARGUMENTS:\n";
  clarg::arguments_descriptions(cout, "  ", "\n");
}

int validateArguments() {
  if (InterpreterFlag.was_set() && RFTFlag.was_set()) {
    cerr << "You can't use a RFT when you are only interpreting!\n";
    return 1;
  }

  if (AOSFlag.was_set() && AOSFlag.value == "") {
    cerr << "You must set the path of the AOS input file.\n" return 1;
  }

  if (!BinaryFlag.was_set()) {
    cerr << "You must set the path of the binary which will be emulated!\n";
    return 1;
  }

  if (LoadRegionsFlag.was_set() && LoadOIFlag.was_set()) {
    cerr << "You cannot use -lr and -loi together!\n";
    return 1;
  }

  return 0;
}

std::unique_ptr<dbt::RFT> RftChosen;
dbt::Machine M;

void sigHandler(int sig) {
  if (M.isOnNativeExecution()) {
    std::cerr << "Error while executing region " << std::hex
              << M.getRegionBeingExecuted() << "\n";
  } else {
    std::cerr << "Error while executing the interpreter.\n";
    if (M.getRegionBeingExecuted() != 0)
      std::cerr << "The last executed region was " << std::hex
                << M.getRegionBeingExecuted() << "\n";
  }

  if (sig == SIGABRT)
    std::cerr << "SIGABRT (" << sig << ") while emulating at PC: " << std::hex
              << M.getPC() << std::dec << "\n";
  else
    std::cerr << "SIGSEGV (" << sig << ") while emulating at PC: " << std::hex
              << M.getPC() << std::dec << "\n";

  std::cerr << "Last Machine state:";

  exit(1);
}

void doNothingHandler(int sig) {}

std::unordered_map<uint32_t, std::vector<std::string>> *
loadCustomOpts(std::string CustomOptsPath) {
  auto CustomOpts = new std::unordered_map<uint32_t, std::vector<std::string>>;

  ifstream File;
  File.open(CustomOptsPath);
  if (!File.is_open()) {
    std::cerr << "You must provide a valid path to the custom opts file\n";
    exit(1);
  }

  std::string Line;
  while (std::getline(File, Line)) {
    std::istringstream ISS(Line);
    uint32_t EntryAddrs;
    ISS >> EntryAddrs;

    std::string OPT;
    while (ISS >> OPT)
      (*CustomOpts)[EntryAddrs].push_back(OPT);
  }

  return CustomOpts;
}

int main(int argc, char **argv) {
  signal(SIGSEGV, sigHandler);
  signal(SIGABRT, sigHandler);

  dbt::Timer GlobalTimer;

  // Parse the arguments
  if (clarg::parse_arguments(argc, argv)) {
    cerr << "Error when parsing the arguments!" << endl;
    return 1;
  }

  if (HelpFlag.get_value() == true) {
    usage(argv[0]);
    return 1;
  }

  if (validateArguments())
    return 1;

  if (StackSizeFlag.was_set()) {
    std::cerr << "Stack size was set to " << StackSizeFlag.get_value()
              << std::endl;
    M.setStackSize(StackSizeFlag.get_value());
  }

  if (HeapSizeFlag.was_set()) {
    std::cerr << "Heap size was set to " << HeapSizeFlag.get_value()
              << std::endl;
    M.setHeapSize(HeapSizeFlag.get_value());
  }

  int LoadStatus = M.loadELF(BinaryFlag.get_value());

  if (!LoadStatus) {
    std::cerr << "Can't find or process ELF file " << argv[1] << std::endl;
    return 2;
  }

  dbt::Manager TheManager(M.getDataMemOffset(), M, VerboseFlag.was_set());

  if (LoadRegionsFlag.was_set() || LoadOIFlag.was_set() ||
      WholeCompilationFlag.was_set())
    TheManager.setToLoadRegions(
        RegionPath.get_value(),
        (!LoadOIFlag.was_set() && !WholeCompilationFlag.was_set()),
        WholeCompilationFlag.was_set());

  if (CustomOptsFlag.was_set()) {
    TheManager.setOptPolicy(dbt::Manager::OptPolitic::Custom);
    TheManager.setCustomOpts(loadCustomOpts(CustomOptsFlag.get_value()));
  } else {
    TheManager.setOptPolicy(dbt::Manager::OptPolitic::Normal);
  }

  TheManager.startCompilationThr();

  if (InterpreterFlag.was_set()) {
    RftChosen = std::make_unique<dbt::NullRFT>(TheManager);
  } else {
    std::string RFTName = RFTFlag.get_value();
    transform(RFTName.begin(), RFTName.end(), RFTName.begin(), ::tolower);

    if (LoadRegionsFlag.was_set() || LoadOIFlag.was_set() ||
        WholeCompilationFlag.was_set()) {
      std::cerr << "Preheated RFT Selected\n";
      RftChosen = std::make_unique<dbt::PreheatRFT>(TheManager);
    } else if (RFTName == "net") {
      std::cerr << "NET RFT Selected\n";
      RftChosen = std::make_unique<dbt::NET>(TheManager);
    } else if (RFTName == "net-r") {
      std::cerr << "NET-R RFT Selected\n";
      RftChosen = std::make_unique<dbt::NET>(TheManager, true);
    } else if (RFTName == "mret2") {
      std::cerr << "MRET2 RFT Selected\n";
      RftChosen = std::make_unique<dbt::MRET2>(TheManager);
    } else if (RFTName == "netplus") {
      std::cerr << "NETPlus RFT Selected\n";
      RftChosen = std::make_unique<dbt::NETPlus>(TheManager);
    } else if (RFTName == "netplus-e-r") {
      std::cerr << "NETPlus-e-r RFT Selected\n";
      RftChosen = std::make_unique<dbt::NETPlus>(TheManager, true);
    } else if (RFTName == "mb") {
      std::cerr << "MethodBased rft selected\n";
      if (ToCompileFlag.was_set())
        RftChosen = std::make_unique<dbt::MethodBased>(
            TheManager, ToCompileFlag.get_value());
      else
        RftChosen = std::make_unique<dbt::MethodBased>(TheManager);
    } else {
      std::cerr << "You should select a valid RFT!\n";
      return 1;
    }
  }

  if (HotnessFlag.was_set()) {
    std::cerr << "The Hotness Threshold was set to " << HotnessFlag.get_value()
              << std::endl;
    RftChosen->setHotnessThreshold(HotnessFlag.get_value());
  }

  if (RegionLimitSize.was_set())
    RftChosen->setRegionLimitSize(RegionLimitSize.get_value());

  std::unique_ptr<dbt::SyscallManager> SyscallM;
  SyscallM = std::make_unique<dbt::LinuxSyscallManager>();

  if (PreheatFlag.was_set()) {
    if (M.setCommandLineArguments(ArgumentsFlag.get_value()) < 0)
      exit(1);

    M.setPreheating(true);
    std::cerr << "Preheating...\n";

    GlobalTimer.startClock();
    dbt::ITDInterpreter I(*SyscallM.get(), *RftChosen.get());
    I.executeAll(M);
    GlobalTimer.stopClock();

    std::cerr << "done\n";
    std::cerr << "Cleaning VM... ";
    M.reset();
    std::cerr << "done\n";

    RftChosen = std::make_unique<dbt::PreheatRFT>(TheManager);
    GlobalTimer.printReport("Preheat");

    while (TheManager.getNumOfOIRegions() != 0) {
    }
    M.setPreheating(false);
  }

  if (M.setCommandLineArguments(ArgumentsFlag.get_value()) < 0)
    exit(1);

  GlobalTimer.startClock();
  dbt::ITDInterpreter I(*SyscallM.get(), *RftChosen.get());
  std::cerr << "Starting execution:\n";

  I.executeAll(M);

  if (DumpRegionsFlag.was_set() || DumpOIRegionsFlag.was_set())
    TheManager.dumpRegions(MergeOIFlag.was_set(), DumpOIRegionsFlag.was_set());

  GlobalTimer.stopClock();
  TheManager.dumpStats();
  GlobalTimer.printReport("Global");

  std::cerr.flush();
  std::cout.flush();

  signal(SIGSEGV, doNothingHandler);
  signal(SIGABRT, doNothingHandler);

  exit(SyscallM->getExitStatus());
}
