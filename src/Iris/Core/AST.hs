module Iris.Core.AST (BinOp (..), UnOp (..), Expr, ExprNode (..)) where

import Data.Text (Text)
import Iris.Common.Span (Spanned)

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

type Expr = Spanned ExprNode

data ExprNode
  = Num Double
  | Str Text
  | Bool Bool
  | Var Text
  | Unary UnOp Expr
  | Binary BinOp Expr Expr
  | If Expr [Expr] (Maybe [Expr])
  | Call Expr [Expr]
  | Lambda [Text] [Expr]
  | Let Text Expr
  | Func Text [Text] [Expr]
  | Field Expr Text
  | Package Text
  | Import Text [Text]
  deriving (Show, Eq)