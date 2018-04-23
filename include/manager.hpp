#ifndef MANAGER_HPP
#define MANAGER_HPP

#include <IREmitter.hpp>
#include <IROpt.hpp>
#include <IRLazyJIT.hpp>
#include <machine.hpp>
#include <thread>
#include <mutex>
#include <shared_mutex>
#include <timer.hpp>
#include <sparsepp/spp.h>

#include "llvm/Support/TargetSelect.h"

#define OIInstList std::vector<std::array<uint32_t,2>>
#define NATIVE_REGION_SIZE 1000000

namespace dbt {
  class Machine;
  class Manager {
    public:
      enum OptPolitic { None, Normal, Aggressive };

    private:
      llvm::LLVMContext TheContext;

      spp::sparse_hash_map<uint32_t, spp::sparse_hash_map<uint32_t, uint32_t>> OIBrTargets;
      spp::sparse_hash_map<uint32_t, OIInstList> OIRegions;
      std::unordered_map<uint32_t, OIInstList> CompiledOIRegions;
      spp::sparse_hash_map<uint32_t, llvm::Module*> IRRegions;
      std::vector<uint32_t> RegionAddresses;
      volatile uint64_t NativeRegions[NATIVE_REGION_SIZE];

      mutable std::shared_mutex OIRegionsMtx, IRRegionsMtx, NativeRegionsMtx, CompiledOIRegionsMtx;

      unsigned NumOfThreads;
      OptPolitic OptMode;

      uint32_t DataMemOffset;

      std::unique_ptr<IREmitter> IRE;
      std::unique_ptr<IROpt> IRO;
      llvm::orc::IRLazyJIT* IRJIT;

      std::atomic<bool> isRegionRecorging;
      std::atomic<bool> isRunning;
      std::atomic<bool> isFinished;
      std::thread Thr;

      unsigned regionFrequency = 0;
      unsigned CompiledRegions = 0;
      unsigned OICompiled = 0;
      unsigned LLVMCompiled = 0;
      float AvgOptCodeSize = 0;

      bool VerboseOutput = false;

      void runPipeline();

    public:
      unsigned getCompiledRegions (void){
        return CompiledRegions;
      }

      unsigned getOICompiled (void) {
        return OICompiled;
      }

      bool getRegionRecording (void) {
        return static_cast<bool>(isRegionRecorging);
      }

      void setRegionRecorging (bool value) {
        isRegionRecorging = value;
      }

      unsigned getLLVMCompiled (void) {
        return LLVMCompiled;
      }

      float getAvgOptCodeSize (void) {
        return AvgOptCodeSize;
      }

      Manager(unsigned T, OptPolitic O, uint32_t DMO, bool VO = false) : NumOfThreads(T), OptMode(O),
            DataMemOffset(DMO), isRunning(true), isFinished(false), VerboseOutput(VO) {

        memset((void*) NativeRegions, 0, sizeof(NativeRegions));

        if(T)
          Thr = std::thread(&Manager::runPipeline, this);
      }

      ~Manager() {
        // Alert threads to stop
        isRunning = false;

        // Waits the thread finish
        if(NumOfThreads)
        {
          while (!isFinished) {}

          if (Thr.joinable())
            Thr.join();
        }

        std::cerr << "Compiled Regions: " << std::dec << CompiledRegions << "\n";
        std::cerr << "Avg Code Size Reduction: ";
        std::cerr << CompiledRegions? AvgOptCodeSize/CompiledRegions : 0;
        std::cerr << std::endl;
        std::cerr << "Compiled OI: " << OICompiled << "\n";
        std::cerr << "Compiled LLVM: " << LLVMCompiled << "\n";
      }

      bool addOIRegion(uint32_t, OIInstList, spp::sparse_hash_map<uint32_t, uint32_t>);

      int32_t jumpToRegion(uint32_t, dbt::Machine&);

      bool isRegionEntry(uint32_t EntryAddress) {
        std::shared_lock<std::shared_mutex> lockOI(OIRegionsMtx);
        std::shared_lock<std::shared_mutex> lockNative(NativeRegionsMtx);
        return OIRegions.count(EntryAddress) != 0 || NativeRegions[EntryAddress] != 0;
      }

      bool isNativeRegionEntry(uint32_t EntryAddress) {
        return (NativeRegions[EntryAddress] != 0);
      }

      size_t getNumOfOIRegions() {
        return OIRegions.size();
      }

      unsigned int getNumOfThreads(void){
        return NumOfThreads;
      }

      float getAvgRegionsSize() {
        uint64_t total;
        for (auto Region : OIRegions)
          total += Region.second.size();
        return (float)total / getNumOfOIRegions();
      }

      bool inCodeCache(uint32_t Addrs) {
        int i = 0;
        for (auto Region : OIRegions) {
          for (auto InstAddr : Region.second)
            if (InstAddr[0] == Addrs)
              return true;

            if(i > OIRegions.size())
              break;

            ++i;
        }
        return false;
      }

      std::vector<uint32_t> getDirectTransitions(uint32_t EntryAddrs) {
        return IRE->getDirectTransitions(EntryAddrs);
      }

      OIInstList getCompiledOIRegion(uint32_t EntryAddrs) {
        return CompiledOIRegions[EntryAddrs];
      }

      std::unordered_map<uint32_t, OIInstList>::iterator oiregions_begin() { return CompiledOIRegions.begin(); };
      std::unordered_map<uint32_t, OIInstList>::iterator oiregions_end()   { return CompiledOIRegions.end(); }; // FIXME: Not Thread Safe
  };
}

#endif
