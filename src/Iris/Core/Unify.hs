module Iris.Core.Unify (unify, compose, UnifyError (..)) where

import Data.Map (empty, map, singleton, union)
import Data.Set (member)
import Data.Text (Text)
import Iris.Common.Span (Span)
import Iris.Core.Types (Subst, Substitutable (apply, free), Type (..), TypeVar)

data UnifyError
  = Mismatch Type Type Span
  | InfiniteType TypeVar Type Span
  | UnboundVariable Text Span
  deriving (Show)

compose :: Subst -> Subst -> Subst
compose s1 s2 = Data.Map.map (apply s1) s2 `union` s1

unify :: Span -> Type -> Type -> Either UnifyError Subst
unify _ (TVar v) t = bind v t
unify _ t (TVar v) = bind v t
unify _ TDouble TDouble = Right empty
unify _ TBool TBool = Right empty
unify _ TStr TStr = Right empty
unify _ TUnit TUnit = Right empty
unify sp (TFun a1 r1) (TFun a2 r2)
  | length a1 /= length a2 = Left (Mismatch (TFun a1 r1) (TFun a2 r2) sp)
  | otherwise = unifyMany sp (r1 : a1) (r2 : a2)
unify sp t1 t2 = Left (Mismatch t1 t2 sp)

unifyMany :: Span -> [Type] -> [Type] -> Either UnifyError Subst
unifyMany _ [] [] = Right empty
unifyMany sp (t1 : ts1) (t2 : ts2) = do
  s1 <- unify sp t1 t2
  s2 <- unifyMany sp (Prelude.map (apply s1) ts1) (Prelude.map (apply s1) ts2)
  pure (s2 `compose` s1)
unifyMany sp _ _ = Left (Mismatch TStr TStr sp)

bind :: TypeVar -> Type -> Either UnifyError Subst
bind v t
  | t == TVar v = Right empty
  | v `member` free t = Left (InfiniteType v t (error "Span context required"))
  | otherwise = Right (singleton v t)