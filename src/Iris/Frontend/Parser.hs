{-# LANGUAGE LambdaCase #-}
{-# LANGUAGE OverloadedStrings #-}

module Iris.Frontend.Parser (parse) where

import Control.Monad (void)
import Control.Monad.Combinators.Expr (Operator (InfixL, InfixN, InfixR), makeExprParser)
import Data.Text (Text, breakOn, drop, length, null, splitOn, unpack)
import Data.Void (Void)
import Iris.Common.Span (Span (..), Spanned (..))
import Iris.Core.AST (BinOp (..), Expr (..), UnOp (..))
import Iris.Frontend.Token (Tok (..), TokenStream (..))
import Text.Megaparsec
  ( MonadParsec (eof, token),
    Parsec,
    between,
    choice,
    errorBundlePretty,
    getSourcePos,
    many,
    optional,
    sepBy,
    (<|>),
  )
import qualified Text.Megaparsec as MP
import Prelude hiding (drop, length, null)

type Parser = Parsec Void TokenStream

getSpan :: Parser Span
getSpan = (\p -> Span p p) <$> getSourcePos

spanned :: Parser (Span -> a) -> Parser a
spanned p = do
  s <- getSpan
  f <- p
  pure $ f s

matchTok :: (Tok -> Maybe a) -> Parser a
matchTok f = token (\(Spanned _ t) -> f t) mempty

tok :: Tok -> Parser ()
tok t = void $ matchTok (\t' -> if t == t' then Just () else Nothing)

identifier :: Parser Text
identifier = matchTok $ \case Identifier i -> Just i; _ -> Nothing

parseExpr :: Parser Expr
parseExpr = choice [parseLet, parseIf, exprTable]

exprTable :: Parser Expr
exprTable = makeExprParser parseUnary table
  where
    table =
      [ [InfixR (binary Pow "^")],
        [InfixL (binary Mul "*"), InfixL (binary Div "/"), InfixL (binary Mod "%")],
        [InfixL (binary Add "+"), InfixL (binary Sub "-")],
        [ InfixN (binary Eq "=="),
          InfixN (binary Ne "!="),
          InfixN (binary Le "<="),
          InfixN (binary Ge ">="),
          InfixN (binary Lt "<"),
          InfixN (binary Gt ">")
        ],
        [InfixL (binary And "&&")],
        [InfixL (binary Or "||")]
      ]
    binary op name = do
      tok (Operator name)
      s <- getSpan
      pure $ \lhs rhs -> Binary op lhs rhs s

parseUnary :: Parser Expr
parseUnary =
  choice
    [ spanned $ do
        op <- (Neg <$ tok (Operator "-")) <|> (Not <$ tok (Operator "!"))
        inner <- parseUnary
        pure $ \s -> Unary op inner s,
      parseCall
    ]

parseCall :: Parser Expr
parseCall = spanned $ do
  e <- parsePrimary
  ops <-
    many $
      choice
        [ do
            args <- between (tok (Delimiters "(")) (tok (Delimiters ")")) (parseExpr `sepBy` tok (Delimiters ","))
            pure $ \s acc -> Call acc args s,
          do
            tok (Operator ".")
            field <- identifier
            pure $ \s acc -> Field acc field s
        ]
  pure $ \s -> foldl (\acc f -> f s acc) e ops

parsePrimary :: Parser Expr
parsePrimary =
  spanned $
    choice
      [ Num <$> matchTok (\case Number n -> Just (read $ unpack n); _ -> Nothing),
        Str <$> matchTok (\case String s -> Just s; _ -> Nothing),
        Var <$> identifier,
        Bool True <$ tok (Keyword "true"),
        Bool False <$ tok (Keyword "false"),
        const <$> between (tok (Delimiters "(")) (tok (Delimiters ")")) parseExpr
      ]

parseLet :: Parser Expr
parseLet = spanned $ do
  tok (Keyword "let")
  name <- identifier
  choice
    [ do
        params <- between (tok (Delimiters "(")) (tok (Delimiters ")")) (identifier `sepBy` tok (Delimiters ","))
        body <- parseBlock
        pure $ \s -> Let name (Lambda params body s) s,
      do
        tok (Operator "=")
        Let name <$> parseExpr
    ]

parseIf :: Parser Expr
parseIf = spanned $ do
  tok (Keyword "if")
  cond <- parseExpr
  thn <- parseBlock
  els <- optional (tok (Keyword "else") *> parseBlock)
  pure $ If cond thn els

parseBlock :: Parser [Expr]
parseBlock =
  between (tok (Delimiters "{")) (tok (Delimiters "}")) $
    many parseExpr

parsePackage :: Parser Expr
parsePackage = spanned $ do
  tok (Keyword "package")
  Package <$> identifier

parseImport :: Parser Expr
parseImport = spanned $ do
  tok (Keyword "import")
  raw <- matchTok (\case String s -> Just s; _ -> Nothing)
  let (col, rest) = breakOn ":" raw
  if null col || length rest <= 1
    then fail "import path must follow 'collection:path' format"
    else do
      let path = drop 1 rest
          segments = splitOn "/" path
      if "" `elem` segments
        then fail "import path has empty segment"
        else pure $ Import raw segments

parse :: FilePath -> TokenStream -> Either String [Expr]
parse path ts = case MP.parse program path ts of
  Left err -> Left (errorBundlePretty err)
  Right ast -> Right ast
  where
    program = do
      pkg <- optional parsePackage
      imports <- many parseImport
      exprs <- many parseExpr <* eof
      pure $ maybe [] pure pkg ++ imports ++ exprs