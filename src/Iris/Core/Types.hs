{-# LANGUAGE FlexibleInstances #-}

module Iris.Core.Types (Type (..), Scheme (..), Subst, Substitutable (..), TypeVar) where

import Data.Map (Map, delete, findWithDefault)
import Data.Set (Set, difference, empty, fromList, singleton, unions)
import Data.Text (Text)

type TypeVar = Text

data Type
  = TVar TypeVar
  | TDouble
  | TBool
  | TStr
  | TUnit
  | TFun [Type] Type
  | TModule Text
  deriving (Show, Eq, Ord)

data Scheme = Forall [TypeVar] Type
  deriving (Show, Eq)

type Subst = Map TypeVar Type

class Substitutable a where
  apply :: Subst -> a -> a
  free :: a -> Set TypeVar

instance Substitutable Type where
  apply s t@(TVar v) = findWithDefault t v s
  apply s (TFun args ret) = TFun (map (apply s) args) (apply s ret)
  apply _ (TModule m) = TModule m
  apply _ t = t

  free (TVar v) = singleton v
  free (TFun args ret) = unions (map free args) <> free ret
  free (TModule _) = empty
  free _ = empty

instance Substitutable Scheme where
  apply s (Forall vs t) = Forall vs (apply (foldr delete s vs) t)
  free (Forall vs t) = free t `difference` fromList vs

instance (Substitutable a) => Substitutable [a] where
  apply s = map (apply s)
  free = unions . map free