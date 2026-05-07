{-# LANGUAGE LambdaCase #-}
{-# LANGUAGE OverloadedStrings #-}
{-# LANGUAGE TupleSections #-}

module Iris.Frontend.Parser (parse) where

import Control.Monad.Combinators.Expr (Operator (InfixL, InfixN, InfixR), makeExprParser)
import Data.Text (Text, breakOn, drop, null, splitOn, unpack)
import Data.Void (Void)
import Iris.Common.Span (Span (..), Spanned (..))
import Iris.Core.AST (BinOp (..), Expr, ExprNode (..), UnOp (..))
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
import Text.Read (readMaybe)
import Prelude hiding (drop, length)

type Parser = Parsec Void TokenStream

matchTokSpan :: (Tok -> Maybe a) -> Parser (a, Span)
matchTokSpan f = token (\(Spanned s t) -> (,s) <$> f t) mempty

tok :: Tok -> Parser Span
tok expected = snd <$> matchTokSpan (\t -> if t == expected then Just () else Nothing)

identifier :: Parser (Text, Span)
identifier = matchTokSpan $ \case Identifier name -> Just name; _ -> Nothing

parseExpr :: Parser Expr
parseExpr = choice [parseLet, parseIf, exprTable]

binary :: BinOp -> Text -> Parser (Expr -> Expr -> Expr)
binary op name = do
  _ <- tok (Operator name)
  pure $ \lhs@(Spanned (Span sStart _) _) rhs@(Spanned (Span _ rEnd) _) ->
    Spanned (Span sStart rEnd) (Binary op lhs rhs)

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

parseUnary :: Parser Expr
parseUnary =
  choice
    [ do
        start <- getSourcePos
        op <- (Neg <$ tok (Operator "-")) <|> (Not <$ tok (Operator "!"))
        val <- parseUnary
        pure $ Spanned (Span start (spanEnd (spannedSpan val))) (Unary op val),
      parseCall
    ]

parseCall :: Parser Expr
parseCall = do
  base <- parsePrimary
  ops <-
    many $
      choice
        [ do
            _ <- tok (Delimiters '(')
            args <- parseExpr `sepBy` tok (Delimiters ',')
            endS <- tok (Delimiters ')')
            pure $ \acc@(Spanned (Span s _) _) -> Spanned (Span s (spanEnd endS)) (Call acc args),
          do
            _ <- tok (Operator ".")
            (field, fSpan) <- identifier
            pure $ \acc@(Spanned (Span s _) _) -> Spanned (Span s (spanEnd fSpan)) (Field acc field)
        ]
  pure $ foldl (\acc f -> f acc) base ops

parsePrimary :: Parser Expr
parsePrimary =
  choice
    [ do
        (n, s) <- matchTokSpan (\case Number n -> readMaybe (unpack n); _ -> Nothing)
        pure $ Spanned s (Num n),
      do
        (t, s) <- matchTokSpan (\case String s -> Just s; _ -> Nothing)
        pure $ Spanned s (Str t),
      do
        (name, s) <- identifier
        pure $ Spanned s (Var name),
      (`Spanned` Bool True) <$> tok (Keyword "true"),
      (`Spanned` Bool False) <$> tok (Keyword "false"),
      parseParens
    ]

parseParens :: Parser Expr
parseParens = do
  start <- tok (Delimiters '(')
  val <- parseExpr
  end <- tok (Delimiters ')')
  pure $ Spanned (Span (spanStart start) (spanEnd end)) (spannedValue val)

parseLet :: Parser Expr
parseLet = do
  start <- tok (Keyword "let")
  (name, _) <- identifier
  choice
    [ do
        _ <- tok (Delimiters '(')
        params <- (fst <$> identifier) `sepBy` tok (Delimiters ',')
        _ <- tok (Delimiters ')')
        body <- parseBlock
        let endS = if Prelude.null body then start else spannedSpan (last body)
        pure $ Spanned (Span (spanStart start) (spanEnd endS)) (Func name params body),
      do
        _ <- tok (Operator "=")
        val <- parseExpr
        pure $ Spanned (Span (spanStart start) (spanEnd (spannedSpan val))) (Let name val)
    ]

parseIf :: Parser Expr
parseIf = do
  start <- tok (Keyword "if")
  cond <- parseExpr
  thn <- parseBlock
  els <- optional (tok (Keyword "else") *> parseBlock)
  let endS = maybe (if Prelude.null thn then spannedSpan cond else spannedSpan (last thn)) (spannedSpan . last) els
  pure $ Spanned (Span (spanStart start) (spanEnd endS)) (If cond thn els)

parseBlock :: Parser [Expr]
parseBlock = between (tok (Delimiters '{')) (tok (Delimiters '}')) (many parseExpr)

parsePackage :: Parser Expr
parsePackage = do
  start <- tok (Keyword "package")
  (name, s) <- identifier
  pure $ Spanned (Span (spanStart start) (spanEnd s)) (Package name)

parseImport :: Parser Expr
parseImport = do
  start <- tok (Keyword "import")
  (raw, s) <- matchTokSpan (\case String s -> Just s; _ -> Nothing)
  let (_, rest) = breakOn ":" raw
  let segments =
        if Data.Text.null rest
          then []
          else splitOn "/" (drop 1 rest)
  pure $ Spanned (Span (spanStart start) (spanEnd s)) (Import raw segments)

parse :: FilePath -> TokenStream -> Either String [Expr]
parse path tokenStream = case MP.parse (program <* eof) path tokenStream of
  Left err -> Left (errorBundlePretty err)
  Right ast -> Right ast
  where
    program = do
      pkg <- optional parsePackage
      imports <- many parseImport
      exprs <- many parseExpr
      pure $ maybe [] pure pkg ++ imports ++ exprs