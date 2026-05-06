{-# LANGUAGE OverloadedStrings #-}
{-# LANGUAGE TupleSections #-}

module Iris.Core.Loader (loadDirectory, compileProject, loadDependencies) where

import Control.Monad (foldM)
import Data.List (filter)
import Data.Map (empty, insert, singleton, union)
import Data.Text (Text, map, pack, unpack)
import Iris.Core.AST (Expr (Block, Import))
import Iris.Core.Infer (Context (Context), infer, runInferModule)
import Iris.Core.Module (ModuleCache, getModuleName)
import Iris.Core.Unify (UnifyError)
import Iris.Frontend.Lexer (tokenize)
import Iris.Frontend.Parser (parse)
import System.Directory (listDirectory)
import System.FilePath (normalise, takeExtension, (</>))
import Prelude hiding (filter, map)

loadDirectory :: FilePath -> IO (Either String [Expr])
loadDirectory dir = do
  files <- filter (\f -> takeExtension f == ".is") <$> listDirectory dir
  contents <- mapM (\f -> (f,) <$> readFile (dir </> f)) files

  let parseFile (name, src) = case tokenize name (pack src) of
        Left e -> Left e
        Right t -> case parse name t of
          Left e -> Left e
          Right ast -> Right ast

  case mapM parseFile contents of
    Left err -> return $ Left err
    Right asts -> return $ Right (concat asts)

compileProject :: FilePath -> IO (Either UnifyError ModuleCache)
compileProject coreDir = do
  result <- loadDirectory coreDir
  case result of
    Left err -> error err
    Right ast -> do
      cacheRes <- loadDependencies ast
      case cacheRes of
        Left err -> return $ Left err
        Right existingCache -> do
          let initialCtx = Context empty existingCache
          case runInferModule initialCtx (infer (Block ast (error "top"))) of
            Left err -> return $ Left err
            Right (Context env _) -> do
              let modName = getModuleName coreDir
              return $ Right $ insert modName env existingCache

resolveImport :: Text -> FilePath
resolveImport pathText =
  let p = unpack $ map (\c -> if c == ':' then '/' else c) pathText
   in normalise p

loadDependencies :: [Expr] -> IO (Either UnifyError ModuleCache)
loadDependencies ast = do
  let imports = [path | Import path _ _ <- ast]
  foldM go (Right empty) imports
  where
    go (Left err) _ = return $ Left err
    go (Right cache) pathText = do
      let dir = resolveImport pathText
      res <- compileProjectWithCache dir cache
      case res of
        Left err -> return $ Left err
        Right newCache -> return $ Right (newCache `union` cache)

compileProjectWithCache :: FilePath -> ModuleCache -> IO (Either UnifyError ModuleCache)
compileProjectWithCache dir existingCache = do
  let cleanDir = normalise dir
  res <- loadDirectory cleanDir
  case res of
    Left err -> error $ "Failed to load directory " ++ cleanDir ++ ": " ++ err
    Right ast -> do
      let initialCtx = Context empty existingCache
      case runInferModule initialCtx (infer (Block ast (error "top"))) of
        Left err -> return $ Left err
        Right (Context env _) -> do
          let modName = getModuleName cleanDir
          return $ Right $ singleton modName env