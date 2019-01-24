#include "AOSDatabase.hpp"

using namespace dbt;

void MappingTraits<Data>::mapping(IO &io, Data &Params) {
  io.mapRequired("DNA", Params.DNA);
  io.mapRequired("SetOpts", Params.SetOpts);
  io.mapRequired("compileTime", Params.CompileTime);
  io.mapRequired("ExecTime", Params.ExecTime);
}