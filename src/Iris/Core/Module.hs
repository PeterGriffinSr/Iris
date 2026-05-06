{-# LANGUAGE OverloadedStrings #-}

module Iris.Core.Module (ModuleCache, getModuleName) where

import Data.Map (Map)
import Data.Text (Text, pack)
import Iris.Core.Types (Scheme)
import System.FilePath (normalise, splitDirectories)

type Exports = Map Text Scheme

type ModuleCache = Map Text Exports

getModuleName :: FilePath -> Text
getModuleName fp =
  let parts = filter (not . null) $ splitDirectories (normalise fp)
   in if null parts then "Main" else pack (last parts)
