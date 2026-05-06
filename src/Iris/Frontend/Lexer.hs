{-# LANGUAGE OverloadedStrings #-}

module Iris.Frontend.Lexer (tokenize) where

import Data.Bifunctor (bimap)
import Data.Char (isAlphaNum, isDigit)
import Data.HashSet (HashSet, fromList, member)
import Data.Text (Text, pack)
import Data.Void (Void)
import Iris.Common.Span (Span (..), Spanned (..))
import Iris.Frontend.Token (Tok (..), TokenStream (..))
import Text.Megaparsec
  ( MonadParsec (eof, takeWhile1P, try),
    Parsec,
    choice,
    errorBundlePretty,
    getSourcePos,
    many,
    manyTill,
    notFollowedBy,
    oneOf,
    optional,
    parse,
    satisfy,
  )
import Text.Megaparsec.Char (char, space1, string)
import Text.Megaparsec.Char.Lexer (charLiteral, skipBlockComment, skipLineComment, space)
import Prelude hiding (lex)

type Lexer = Parsec Void Text

sc :: Lexer ()
sc = space space1 (skipLineComment "#") (skipBlockComment "#*" "*#")

lexeme :: Lexer a -> Lexer (Spanned a)
lexeme p = do
  start <- getSourcePos
  v <- p
  end <- getSourcePos
  sc
  pure $ Spanned (Span start end) v

keywords :: HashSet Text
keywords = fromList ["let", "if", "else", "true", "false", "import", "package"]

number :: Lexer Tok
number = do
  integral <- takeWhile1P (Just "digit") isDigit
  fractional <- optional (char '.' *> takeWhile1P (Just "digit") isDigit)
  notFollowedBy (satisfy (\c -> isDigit c || c == '.'))
  pure $ Number $ case fractional of
    Nothing -> integral
    Just f -> integral <> "." <> f

tok :: Lexer Tok
tok =
  choice
    [ number,
      String <$> (char '"' *> (pack <$> manyTill charLiteral (char '"'))),
      Operator <$> choice (map (try . string) ["==", "!=", "<=", ">=", "&&", "||", "=", "+", "-", "*", "/", "%", "<", ">", "!", "|", "^", "."]),
      Delimiters . pack . (: []) <$> oneOf ("()[]{}," :: String),
      identifierOrKeyword
    ]
  where
    identifierOrKeyword = do
      word <- takeWhile1P (Just "ident") (\c -> c == '_' || isAlphaNum c)
      pure $ if member word keywords then Keyword word else Identifier word

lex :: Lexer [Spanned Tok]
lex = sc *> many (lexeme tok) <* eof

tokenize :: FilePath -> Text -> Either String TokenStream
tokenize path src = bimap errorBundlePretty (`TokenStream` src) (parse lex path src)