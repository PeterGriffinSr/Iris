{-# LANGUAGE InstanceSigs #-}
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

data Tok
  = Keyword Text
  | Identifier Text
  | Operator Text
  | Number Text
  | String Text
  | Delimiters Text
  deriving (Show, Eq, Ord)

data TokenStream = TokenStream
  { unTokenStream :: [Spanned Tok],
    streamSource :: Text
  }
  deriving (Show)

instance Stream TokenStream where
  type Token TokenStream = Spanned Tok
  type Tokens TokenStream = [Spanned Tok]

  tokenToChunk _ t = [t]
  tokensToChunk _ ts = ts
  chunkToTokens _ ts = ts
  chunkLength _ = length
  chunkEmpty _ = null

  take1_ (TokenStream [] _) = Nothing
  take1_ (TokenStream (t : ts) src) = Just (t, TokenStream ts src)

  takeN_ n (TokenStream ts src)
    | n <= 0 = Just ([], TokenStream ts src)
    | null ts = Nothing
    | otherwise = Just (take n ts, TokenStream (drop n ts) src)

  takeWhile_ :: (Token TokenStream -> Bool) -> TokenStream -> (Tokens TokenStream, TokenStream)
  takeWhile_ p (TokenStream ts src) = (takeWhile p ts, TokenStream (dropWhile p ts) src)

instance VisualStream TokenStream where
  showTokens _ ts = unwords $ map (show . spannedValue) (toList ts)

instance TraversableStream TokenStream where
  reachOffset o PosState {..} =
    let consumed = o - pstateOffset
        (pre, post) = splitAt consumed (unTokenStream pstateInput)
        newPos = case post of
          (t : _) -> spanStart (spannedSpan t)
          [] -> case pre of
            [] -> pstateSourcePos
            _ -> spanStart (spannedSpan (last pre))

        lineText =
          maybe "" (unpack . fst) $
            uncons (drop (unPos (sourceLine newPos) - 1) (lines (streamSource pstateInput)))
     in ( Just lineText,
          PosState
            { pstateInput = (pstateInput {unTokenStream = post}),
              pstateOffset = max o pstateOffset,
              pstateSourcePos = newPos,
              pstateTabWidth = pstateTabWidth,
              pstateLinePrefix = pstateLinePrefix
            }
        )
    where
      uncons [] = Nothing; uncons (x : xs) = Just (x, xs)