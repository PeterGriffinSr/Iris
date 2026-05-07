{-# LANGUAGE OverloadedStrings #-}

module Iris.Frontend.Lexer (tokenize) where

import Data.Bifunctor (first)
import Data.Char (isAlphaNum, isDigit)
import Data.HashSet (fromList, member)
import Data.Text (Text, pack)
import Data.Void (Void)
import Iris.Common.Span (Span (..), Spanned (..))
import Iris.Frontend.Token (Tok (..), TokenStream (..))
import Text.Megaparsec
  ( MonadParsec (eof, notFollowedBy, takeWhile1P, try),
    Parsec,
    choice,
    empty,
    errorBundlePretty,
    getSourcePos,
    many,
    manyTill,
    oneOf,
    optional,
    parse,
  )
import Text.Megaparsec.Char (char, newline, space1, string)
import Text.Megaparsec.Char.Lexer (charLiteral, skipLineComment, space)

type Parser = Parsec Void Text

spaceConsumer :: Parser ()
spaceConsumer = space space1 (skipLineComment "#") empty

lexical :: Parser a -> Parser (Spanned a)
lexical parser = do
  start <- getSourcePos
  value <- parser
  end <- getSourcePos
  spaceConsumer
  pure $ Spanned (Span start end) value

tokenParser :: Parser Tok
tokenParser =
  choice
    [ Number <$> ((\intPart decPart -> intPart <> maybe "" ("." <>) decPart) <$> digits <*> optional (try (char '.' *> digits))) <* notFollowedBy (oneOf ('.' : ['0' .. '9'])),
      String . pack <$> (char '"' *> manyTill (notFollowedBy newline *> charLiteral) (char '"')),
      Operator <$> choice (string <$> ["==", "!=", "<=", ">=", "&&", "||", "=", "+", "-", "*", "/", "%", "<", ">", "!", "|", "^", "."]),
      Delimiters <$> oneOf ("()[]{}," :: [Char]),
      (\word -> if member word keywords then Keyword word else Identifier word) <$> takeWhile1P (Just "identifier") (\c -> c == '_' || isAlphaNum c)
    ]
  where
    digits = takeWhile1P (Just "digit") isDigit
    keywords = fromList ["let", "if", "else", "true", "false", "import", "package"]

tokenize :: FilePath -> Text -> Either String TokenStream
tokenize path input = first errorBundlePretty $ (`TokenStream` input) <$> parse (spaceConsumer *> many (lexical tokenParser) <* eof) path input