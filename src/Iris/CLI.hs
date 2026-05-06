{-# LANGUAGE LambdaCase #-}
{-# LANGUAGE OverloadedStrings #-}
{-# LANGUAGE RecordWildCards #-}

module Iris.CLI (run) where

import Data.Map (empty)
import Data.Maybe (fromMaybe)
import Data.Text.IO (readFile)
import Data.Version (showVersion)
import Iris.Core.AST (Expr (Block))
import Iris.Core.Infer (Context (Context), infer, runInfer)
import Iris.Core.Loader (loadDependencies)
import Iris.Frontend.Lexer (tokenize)
import Iris.Frontend.Parser (parse)
import Options.Applicative
  ( Parser,
    command,
    execParser,
    fullDesc,
    help,
    helper,
    hsubparser,
    info,
    infoOption,
    long,
    metavar,
    optional,
    progDesc,
    short,
    strArgument,
    strOption,
    switch,
    (<**>),
  )
import Paths_iris (version)
import Prelude hiding (readFile)

data GlobalOptions = GlobalOptions
  { wError :: Bool,
    verbose :: Bool
  }

data Command
  = Run FilePath GlobalOptions
  | Compile FilePath (Maybe FilePath) GlobalOptions

versionString :: String
versionString = "iris version " ++ showVersion version

run :: IO ()
run =
  execParser opts >>= \case
    Run path opts' -> runInterpreter path opts'
    Compile src dest opts' -> runCompiler src dest opts'
  where
    opts =
      info
        (commandParser <**> helper <**> versionHelper)
        (fullDesc <> progDesc "Iris: A split programming language")
    versionHelper = infoOption versionString (long "version" <> help "Show version")

pipeline :: FilePath -> GlobalOptions -> IO (Maybe [Expr])
pipeline path GlobalOptions {..} = do
  src <- readFile path
  case tokenize path src of
    Left err -> handleFailure "Lexer Error" err
    Right tokens ->
      case parse path tokens of
        Left err -> handleFailure "Parser Error" err
        Right ast -> do
          depResult <- loadDependencies ast
          case depResult of
            Left err -> handleFailure "Dependency Error" (show err)
            Right cache -> do
              let ctx = Context empty cache
              case runInfer ctx (infer (Block ast (error "Top level span"))) of
                Left err -> handleFailure "Type Error" (show err)
                Right _ -> return (Just ast)
  where
    handleFailure stage err = do
      let prefix = if wError then "FATAL [" ++ stage ++ "]: " else stage ++ ": "
      putStrLn $ prefix ++ err
      return Nothing

runInterpreter :: FilePath -> GlobalOptions -> IO ()
runInterpreter path opts = do
  pipeline path opts >>= \case
    Just ast -> do
      print ast
    Nothing -> return ()

runCompiler :: FilePath -> Maybe FilePath -> GlobalOptions -> IO ()
runCompiler src dest opts = do
  pipeline src opts >>= \case
    Just ast -> do
      let outPath = fromMaybe "out.bc" dest
      putStrLn $ "Compiling to " ++ outPath ++ "..."
      print ast
    Nothing -> return ()

commandParser :: Parser Command
commandParser =
  hsubparser
    ( command "run" (info (runP <**> helper) (progDesc "Execute an Iris file"))
        <> command "compile" (info (compileP <**> helper) (progDesc "Compile to binary"))
    )
  where
    runP = Run <$> fileArg <*> globalFlags
    compileP = Compile <$> fileArg <*> outputOpt <*> globalFlags

    fileArg = strArgument (metavar "FILE" <> help "Source .is file")
    outputOpt = optional $ strOption (long "output" <> short 'o' <> metavar "OUT" <> help "Output destination")
    globalFlags =
      GlobalOptions
        <$> switch (long "werror" <> help "Treat warnings as errors")
        <*> switch (long "verbose" <> short 'v' <> help "Enable debug logging")