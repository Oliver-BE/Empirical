#ifndef EMP_INSTRUCTION_LIB_H
#define EMP_INSTRUCTION_LIB_H

//////////////////////////////////////////////////////////////////////////////////////////////////
//
//  The InstLib class maintains a library of all instructions available to a particular type
//  of virtual CPU, including the functions associated with them, their costs, etc.
//

#include <functional>
#include <map>
#include <string>
#include <vector>

#include "Instruction.h"

namespace emp {
  struct InstInfo {
    // User-specified data for each instruction
    std::string name;    // Name of this instruction
    int cycle_cost;      // CPU Cycle Cost to execute this instruction

    // Auto-generated by InstLib
    char short_name;     // Single character representation of this instruction
    int id;              // Unique ID indicating position of this instruction in the set.

    InstInfo(const std::string & _name, int _cost, char _sname, int _id)
      : name(_name), cycle_cost(_cost), short_name(_sname), id(_id) { ; }
  };

  const char inst_char_chart[] = { 'a', 'b', 'c', 'd', 'e', 'f', 'g', 'h', 'i', 'j', 'k', 'l', 
                                   'm', 'n', 'o', 'p', 'q', 'r', 's', 't', 'u', 'v', 'w', 'x', 
                                   'y', 'z', 'A', 'B', 'C', 'D', 'E', 'F', 'G', 'H', 'I', 'J',
                                   'K', 'L', 'M', 'N', 'O', 'P', 'Q', 'R', 'S', 'T', 'U', 'V', 
                                   'W', 'X', 'Y', 'Z', '0', '1', '2', '3', '4', '5', '6', '7',
                                   '8', '9', '!', '@', '$', '%', '^', '&', '*', '_', '=', '-',
                                   '+' };

  template <typename HARDWARE_TYPE> class InstLib {
  private:
    // All of the info for the functions is below.
    // The functions pointers are separated out for improved (?) cache performance.
    std::vector< std::function<bool(HARDWARE_TYPE&)> inst_calls;
    std::vector<InstInfo> inst_info;

    std::map<std::string, int> name_map;
    std::map<char, int> short_name_map;

  public:
    InstLib() { ; }
    ~InstLib() { ; }

    int GetSize() const { return (int) inst_info.size(); }

    // Add a new instruction to this library.
    InstLib & AddInst(const std::string & _name, std::function<bool(HARDWARE_TYPE&)> _call,
                 int _cost = 1) {
      const int next_id = (int) inst_info.size();  // The ID number of this new instruction.
      const int char_id = std::min(next_id, 72);   // We only have 72 chars, so additional are "+"
      const char next_char = inst_char_chart[char_id];
      inst_calls.push_back(_call);
      inst_info.push_back( InstInfo(_name, _cost, next_char, next_id) );
      name_map.insert(_name, next_id);
      if (next_id == char_id) short_name_map(next_char, next_id);
      return *this;
    }

    // Retrieve information about each instruction.
    const std::string & GetName(const Instruction & inst) const { return inst_info[inst.GetID()].name; }
    char GetShortName(const Instruction & inst) const { return inst_info[inst.GetID()].short_name; }
    int GetCycleCost(const Instruction & inst) const { return inst_info[inst.GetID()].cycle_cost; }
    int GetID(const Instruction & inst) const { return inst_info[inst.GetID()].id; }

    // Convert an Instruction into a single character (only works perfectly if inst lib size < 72)
    char AsChar(const Instruction & inst) const { return inst_info[inst.GetID()].short_name; }
    
    //Convert an instruction vector into a series of characters.
    std::string AsString(const std::vector<Instruction> & inst_vector) const {
      const int vector_size= inst_vector.GetSize();
      std::string out_string(vector_size);
      for (int i = 0; i < vector_size; i++) {
        out_string[i] = ToChar(inst_vector[i]);
      }
      // @CAO Should we do something here to facilitate move sematics?
      return out_string;
    }
  };
};

#endif
