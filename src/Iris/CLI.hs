{-# LANGUAGE LambdaCase #-}
{-# LANGUAGE OverloadedStrings #-}
{-# LANGUAGE RecordWildCards #-}
{-# LANGUAGE TupleSections #-}

module Iris.CLI (run) where

import Control.Monad.IO.Class (liftIO)
import Control.Monad.Trans.Except (runExceptT, throwE)
import Data.Maybe (fromMaybe)
import Data.Text.IO (readFile)
import Data.Version (showVersion)
import Iris.Core.AST (Expr)
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

data Opts = Opts {wError :: Bool, verbose :: Bool}

data Command
  = Run FilePath Opts
  | Compile FilePath (Maybe FilePath) Opts

run :: IO ()
run =
  execParser (info (parser <**> helper) (fullDesc <> progDesc "Iris: A split programming language")) >>= \case
    Run path opts -> execute path opts print
    Compile src out opts -> execute src opts $ \ast -> do
      let dest = fromMaybe "out.bc" out
      putStrLn $ "Compiling to " ++ dest ++ "..."
      print ast

execute :: FilePath -> Opts -> ([Expr] -> IO ()) -> IO ()
execute path Opts {..} action =
  runExceptT pipeline >>= \case
    Left (stage, err) -> putStrLn $ (if wError then "FATAL [" ++ stage ++ "]: " else stage ++ ": ") ++ err
    Right ast -> action ast
  where
    pipeline = do
      src <- liftIO $ readFile path
      tokens <- either (throwE . ("Lexer Error",)) pure $ tokenize path src
      either (throwE . ("Parser Error",)) pure $ parse path tokens

parser :: Parser Command
parser =
  hsubparser
    ( command "run" (info (Run <$> fileArg <*> flags) (progDesc "Execute an Iris file"))
        <> command "compile" (info (Compile <$> fileArg <*> output <*> flags) (progDesc "Compile to binary"))
    )
    <**> infoOption ("iris version " ++ showVersion version) (long "version" <> help "Show version")
  where
    fileArg = strArgument (metavar "FILE" <> help "Source .is file")
    output = optional $ strOption (long "output" <> short 'o' <> metavar "OUT")
    flags = Opts <$> switch (long "werror") <*> switch (long "verbose" <> short 'v')