{-# LANGUAGE FlexibleContexts #-}
{-# LANGUAGE OverloadedStrings #-}

module Iris.Core.Infer (runInfer, Context (..), fresh, liftUnify, instantiate, generalize, findModuleExport, infer, runInferModule) where

import Control.Monad.Except (Except, MonadError (throwError), liftEither, runExcept)
import Control.Monad.Reader (MonadReader (ask, local), MonadTrans (lift), ReaderT (runReaderT))
import Control.Monad.State (MonadState (get, put), StateT, evalStateT)
import Data.Map (Map, elems, fromList, insert, lookup, union)
import qualified Data.Map as Map
import Data.Set (difference, toList)
import Data.Text (Text, pack, unpack)
import qualified Data.Text as T
import Iris.Common.Span (Span)
import Iris.Core.AST
  ( BinOp (Add, Div, Eq, Ge, Gt, Le, Lt, Mod, Mul, Ne, Pow, Sub),
    Expr (..),
    UnOp (Neg, Not),
  )
import Iris.Core.Module (ModuleCache, getModuleName)
import Iris.Core.Types (Scheme (..), Substitutable (..), Type (..))
import Iris.Core.Unify (UnifyError (UnboundVariable), unify)
import System.FilePath (normalise)
import Prelude hiding (lookup)

data Context = Context
  { env :: Map Text Scheme,
    modules :: ModuleCache
  }

instance Substitutable Context where
  apply s (Context e m) = Context (Map.map (apply s) e) m
  free (Context e _) = free (elems e)

type Infer a = ReaderT Context (StateT Int (Except UnifyError)) a

runInfer :: Context -> Infer a -> Either UnifyError a
runInfer ctx m = runExcept (evalStateT (runReaderT m ctx) 0)

fresh :: Infer Type
fresh = do
  n <- get
  put (n + 1)
  pure $ TVar (pack $ "t" ++ show n)

liftUnify :: Either UnifyError a -> Infer a
liftUnify = lift . lift . liftEither

instantiate :: Scheme -> Infer Type
instantiate (Forall vars t) = do
  newVars <- mapM (const fresh) vars
  let s = fromList $ zip vars newVars
  pure $ apply s t

generalize :: Context -> Type -> Scheme
generalize ctx t =
  let vars = toList $ free t `difference` free ctx
   in Forall vars t

findModuleExport :: Text -> Text -> Span -> Infer Type
findModuleExport modName field sp = do
  ctx <- ask
  case Map.lookup modName (modules ctx) of
    Nothing -> throwError $ UnboundVariable modName sp
    Just exports -> case Map.lookup field exports of
      Nothing -> throwError $ UnboundVariable (modName <> "." <> field) sp
      Just scheme -> instantiate scheme

infer :: Expr -> Infer Type
infer expr = case expr of
  Num _ _ -> pure TDouble
  Bool _ _ -> pure TBool
  Str _ _ -> pure TStr
  Var n sp -> do
    Context e _ <- ask
    case lookup n e of
      Nothing -> throwError $ UnboundVariable n sp
      Just s -> instantiate s
  Unary op e sp -> do
    t <- infer e
    case op of
      Neg -> liftUnify (unify sp t TDouble) >> pure TDouble
      Not -> liftUnify (unify sp t TBool) >> pure TBool
  Field (Var modName _) fieldName sp -> do
    ctx <- ask
    case lookup modName (modules ctx) of
      Just _ -> findModuleExport modName fieldName sp
      Nothing -> do
        case lookup modName (env ctx) of
          Just s -> do
            t <- instantiate s
            case t of
              TModule m -> findModuleExport m fieldName sp
              _ -> throwError $ UnboundVariable (modName <> "." <> fieldName) sp
          Nothing -> throwError $ UnboundVariable modName sp
  Field e fieldName sp -> do
    t <- infer e
    case t of
      TModule m -> findModuleExport m fieldName sp
      _ -> throwError $ UnboundVariable fieldName sp
  Block exprs sp -> do
    let scan (Let name (Lambda {}) _) = [(name, ())]
        scan _ = []
    let functions = concatMap scan exprs
    placeholders <- mapM (\(name, _) -> (,) name . Forall [] <$> fresh) functions
    local (\(Context e m) -> Context (fromList placeholders `union` e) m) $
      inferBlock exprs sp
  Lambda args body sp -> do
    argTypes <- mapM (const fresh) args
    let argSchemes = Prelude.map (Forall []) argTypes
    let envExt = fromList (zip args argSchemes)
    bodyType <- local (\(Context e m) -> Context (envExt `union` e) m) (infer (Block body sp))
    pure $ TFun argTypes bodyType
  Call func args sp -> do
    tFunc <- infer func
    tArgs <- mapM infer args
    tRet <- fresh
    s <- liftUnify $ unify sp tFunc (TFun tArgs tRet)
    pure (apply s tRet)
  Binary op l r sp -> do
    tl <- infer l
    tr <- infer r
    case op of
      o | o `elem` [Add, Sub, Mul, Div, Mod, Pow] -> do
        _ <- liftUnify $ unify sp tl TDouble
        _ <- liftUnify $ unify sp tr TDouble
        pure TDouble
      o | o `elem` [Eq, Ne, Lt, Le, Gt, Ge] -> do
        _ <- liftUnify $ unify sp tl tr
        pure TBool
      _ -> do
        _ <- liftUnify $ unify sp tl TBool
        _ <- liftUnify $ unify sp tr TBool
        pure TBool
  If cond thenB elseB sp -> do
    tCond <- infer cond
    _ <- liftUnify $ unify sp tCond TBool
    tThen <- infer (Block thenB sp)
    case elseB of
      Nothing -> pure TUnit
      Just b -> do
        tElse <- infer (Block b sp)
        _ <- liftUnify $ unify sp tThen tElse
        pure tThen
  Import path _ _ -> do
    let pathStr = unpack $ T.map (\c -> if c == ':' then '/' else c) path
    let modName = getModuleName pathStr
    pure $ TModule modName
  Let _ val _ -> infer val
  Package _ _ -> pure TUnit

inferBlock :: [Expr] -> Span -> Infer Type
inferBlock [] _ = pure TUnit
inferBlock [e] _ = infer e
inferBlock (e : es) sp = do
  case e of
    Import path _ _ -> do
      let pathStr = unpack $ T.map (\c -> if c == ':' then '/' else c) path
      let modName = getModuleName (normalise pathStr)
      pure $ TModule modName
    Let name val _ -> do
      tVal <- infer val
      ctx@(Context ctxEnv _) <- ask
      let scheme = generalize ctx tVal
      case lookup name ctxEnv of
        Just s -> do
          tPlaceholder <- instantiate s
          _ <- liftUnify $ unify sp tPlaceholder tVal
          pure ()
        Nothing -> pure ()
      local (\(Context e' m) -> Context (insert name scheme e') m) (inferBlock es sp)
    _ -> infer e >> inferBlock es sp

runInferModule :: Context -> Infer a -> Either UnifyError Context
runInferModule ctx m = runExcept (evalStateT (runReaderT (m >> ask) ctx) 0)