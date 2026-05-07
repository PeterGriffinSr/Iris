{-# LANGUAGE RecordWildCards #-}
{-# LANGUAGE TypeFamilies #-}

module Iris.Frontend.Token (Tok (..), TokenStream (..)) where

import Data.List.NonEmpty (toList)
import Data.Text (Text, lines, unpack)
import Iris.Common.Span (Spanned (..), spanStart)
import Text.Megaparsec
  ( PosState
      ( PosState,
        pstateInput,
        pstateLinePrefix,
        pstateOffset,
        pstateSourcePos,
        pstateTabWidth
      ),
    SourcePos (sourceLine),
    Stream (..),
    TraversableStream (reachOffset),
    VisualStream (showTokens),
    unPos,
  )
import Prelude hiding (lines)

data Tok = Keyword Text | Identifier Text | Operator Text | Number Text | String Text | Delimiters Char
  deriving (Show, Eq, Ord)

data TokenStream = TokenStream {unTs :: [Spanned Tok], src :: Text} deriving (Show)

instance Stream TokenStream where
  type Token TokenStream = Spanned Tok
  type Tokens TokenStream = [Spanned Tok]
  tokenToChunk _ t = [t]
  tokensToChunk _ ts = ts
  chunkToTokens _ ts = ts
  chunkLength _ = length
  chunkEmpty _ = null
  take1_ (TokenStream [] _) = Nothing
  take1_ (TokenStream (t : ts) s) = Just (t, TokenStream ts s)
  takeN_ n (TokenStream ts s) | n <= 0 = Just ([], TokenStream ts s) | null ts = Nothing | otherwise = Just (take n ts, TokenStream (drop n ts) s)
  takeWhile_ p (TokenStream ts s) = (takeWhile p ts, TokenStream (dropWhile p ts) s)

instance VisualStream TokenStream where
  showTokens _ = unwords . map (show . spannedValue) . toList

instance TraversableStream TokenStream where
  reachOffset o PosState {..} = (Just ltxt, PosState (pstateInput {unTs = post}) (max o pstateOffset) pos pstateTabWidth pstateLinePrefix)
    where
      (pre, post) = splitAt (o - pstateOffset) (unTs pstateInput)
      pos = case post of (t : _) -> spanStart (spannedSpan t); _ -> case pre of [] -> pstateSourcePos; _ -> spanStart (spannedSpan (last pre))
      ltxt = maybe "" (unpack . fst) . uncons . drop (unPos (sourceLine pos) - 1) $ lines (src pstateInput)
      uncons [] = Nothing; uncons (x : xs) = Just (x, xs)