#pragma once

namespace dbt {
class AOSSolver {
public:
  AOSSolver() {}
  virtual ~AOSSolver() {}

  virtual void Solve();
  virtual void Evaluate();
};

class GASolver : public AOSSolver {
public:
  GASolver();
  void Solve() override;
  void Evaluate() override;
};

class RMHCSolver : public AOSSolver {
public:
  RMHCSolver();

  void Solve() override;
  void Evaluate() override;
};

}; // namespace dbt