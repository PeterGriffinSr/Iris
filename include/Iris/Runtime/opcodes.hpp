#pragma once

#include <cstdint>

namespace Iris::Runtime {

enum class OpCode : uint8_t {
  PushConst,
  PushBool,
  PushUnit,
  Load,
  Store,
  StoreClosure,
  Add,
  Sub,
  Mul,
  Div,
  Mod,
  Pow,
  Neg,
  Not,
  Eq,
  Neq,
  Lt,
  LtEq,
  Gt,
  GtEq,
  Jump,
  JumpFalse,
  Call,
  TailCall,
  Pop,
  Return,
  MakeClosure,
  LoadUpvalue,
  GetField,
  Print,
  Panic
};
}