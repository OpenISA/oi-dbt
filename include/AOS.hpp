#pragma once

#include "AOSSolver.hpp"
#include <string>

namespace dbt {
struct AOSParams {};

class AOS {
  AOSSolver *solver;

public:
  AOS(const AOSParams &params);
  void Run() { solver->Solve(); }
};
}; // namespace dbt