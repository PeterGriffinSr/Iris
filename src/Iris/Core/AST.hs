module Iris.Core.AST (BinOp (..), UnOp (..), Expr (..)) where

import Data.Text (Text)
import Iris.Common.Span (Span)

data BinOp
  = Add
  | Sub
  | Mul
  | Div
  | Mod
  | Pow
  | Eq
  | Ne
  | Lt
  | Le
  | Gt
  | Ge
  | And
  | Or
  deriving (Show, Eq)

data UnOp = Neg | Not
  deriving (Show, Eq)

data Expr
  = Num Double Span
  | Str Text Span
  | Bool Bool Span
  | Var Text Span
  | Unary UnOp Expr Span
  | Binary BinOp Expr Expr Span
  | If Expr [Expr] (Maybe [Expr]) Span
  | Call Expr [Expr] Span
  | Lambda [Text] [Expr] Span
  | Let Text Expr Span
  | Field Expr Text Span
  | Block [Expr] Span
  | Package Text Span
  | Import Text [Text] Span
  deriving (Show, Eq)