// Replay-only correction of the diagnosed dword4 -> dword3 texture result bias.
// Preserve the original dword4 LOD loads; clone the access/load/bitcast only for
// BitFieldSExtract(13,6) consumed by Ldexp. No runtime or retail code is changed.
#pragma once
#include <map>
#include <stdexcept>
#include <vector>
static unsigned FixReplayFetchExponent(std::vector<uint32_t>& module) {
  using Inst=std::vector<uint32_t>;
  std::vector<Inst> instructions;
  std::map<uint32_t,Inst> definitions;
  std::map<uint32_t,uint32_t> constants,sets,bindings;
  std::map<std::pair<uint32_t,uint32_t>,uint32_t> constant_ids;
  if(module.size()<5 || module[0]!=spv::MagicNumber)throw std::runtime_error("Bad SPIR-V module");
  for(size_t i=5;i<module.size();) {
    unsigned n=module[i]>>16,op=module[i]&65535;
    if(!n || n>module.size()-i)throw std::runtime_error("Bad SPIR-V instruction");
    Inst v(module.begin()+i,module.begin()+i+n);instructions.push_back(v);
    if(op==spv::OpDecorate && n==4) {
      if(v[2]==spv::DecorationDescriptorSet)sets[v[1]]=v[3];
      if(v[2]==spv::DecorationBinding)bindings[v[1]]=v[3];
    }
    if(op==spv::OpConstant && n==4) {constants[v[2]]=v[3];constant_ids[{v[1],v[3]}]=v[2];}
    if(op==spv::OpAccessChain || op==spv::OpLoad || op==spv::OpBitcast || op==spv::OpConstant || op==spv::OpBitFieldSExtract)
      definitions[v[2]]=v;
    i+=n;
  }
  uint32_t next=module[3];unsigned count=0;
  std::vector<Inst> output,extra_constants;
  auto constant=[&](uint32_t type,uint32_t value) {
    auto key=std::make_pair(type,value);auto it=constant_ids.find(key);
    if(it!=constant_ids.end())return it->second;
    uint32_t id=next++;constant_ids[key]=id;
    extra_constants.push_back({(4u<<16)|spv::OpConstant,type,id,value});return id;
  };
  for(auto v:instructions) {
    if((v[0]&65535)==spv::OpBitFieldSExtract && v.size()==6 &&
        constants.count(v[4]) && constants.at(v[4])==13 &&
        constants.count(v[5]) && constants.at(v[5])==6) {
      auto cast_it=definitions.find(v[3]);
      if(cast_it!=definitions.end() && (cast_it->second[0]&65535)==spv::OpBitcast) {
        auto cast=cast_it->second;auto load_it=definitions.find(cast[3]);
        if(load_it!=definitions.end() && (load_it->second[0]&65535)==spv::OpLoad) {
          auto load=load_it->second;auto chain_it=definitions.find(load[3]);
          if(chain_it!=definitions.end() && (chain_it->second[0]&65535)==spv::OpAccessChain) {
            auto chain=chain_it->second;
            if(chain.size()==7 && sets.count(chain[3]) && sets.at(chain[3])==1 &&
                bindings.count(chain[3]) && bindings.at(chain[3])==4 &&
                constants.count(chain[5]) && constants.count(chain[6])) {
              const uint32_t flat=constants.at(chain[5])*4+constants.at(chain[6]);
              if(flat%6==4) {
                const auto vec_type=definitions.at(chain[5])[1],component_type=definitions.at(chain[6])[1];
                chain[2]=next++;chain[5]=constant(vec_type,(flat-1)/4);chain[6]=constant(component_type,(flat-1)%4);
                load[2]=next++;load[3]=chain[2];cast[2]=next++;cast[3]=load[2];v[3]=cast[2];
                output.push_back(chain);output.push_back(load);output.push_back(cast);++count;
              }
            }
          }
        }
      }
    }
    output.push_back(v);
  }
  std::vector<uint32_t> words(module.begin(),module.begin()+5);words[3]=next;
  bool inserted=false;
  for(const auto& v:output) {
    if(!inserted && (v[0]&65535)==spv::OpFunction) {
      for(const auto& c:extra_constants)words.insert(words.end(),c.begin(),c.end());inserted=true;
    }
    words.insert(words.end(),v.begin(),v.end());
  }
  if(!count)throw std::runtime_error("No wrong-word texture exponent found");
  module.swap(words);return count;
}
