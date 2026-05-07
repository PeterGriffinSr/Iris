module Iris.Common.Span (Span (..), Spanned (..), unspan) where

import Text.Megaparsec (SourcePos)

data Span = Span
  { spanStart :: !SourcePos,
    spanEnd :: !SourcePos
  }
  deriving (Show, Eq, Ord)

data Spanned a = Spanned
  { spannedSpan :: !Span,
    spannedValue :: a
  }
  deriving (Show, Eq, Ord)

unspan :: Spanned a -> a
unspan = spannedValue