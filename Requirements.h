#ifndef CLANG_MUTATE_REQUIREMENTS_H
#define CLANG_MUTATE_REQUIREMENTS_H

#include "ASTRef.h"
#include "Macros.h"
#include "Function.h"
#include "Variable.h"
#include "Scopes.h"
#include "Utils.h"
#include "Bindings.h"

#include "clang/Basic/LLVM.h"
#include "clang/AST/ASTConsumer.h"
#include "clang/AST/ASTContext.h"
#include "clang/AST/AST.h"
#include "clang/AST/RecursiveASTVisitor.h"
#include "clang/Rewrite/Frontend/Rewriters.h"
#include "clang/Basic/SourceManager.h"
#include "clang/Frontend/CompilerInstance.h"

#include <set>

namespace clang_mutate {

// A traversal to run on each statement in order to gather
// information about the context required by the statement;
// for example, what variables appear free, what macros
// are used, and what #include directives are needed to get
// the appropriate library functions.
class Requirements
  : public clang::ASTConsumer
  , public clang::RecursiveASTVisitor<Requirements>
{
public:
    typedef RecursiveASTVisitor<Requirements> base;

    Requirements(
        clang::CompilerInstance * _ci,
        const std::vector<std::vector<std::string> > & scopes);
    
    std::set<VariableInfo> variables() const;
    std::set<FunctionInfo> functions() const;
    std::set<std::string>  includes()  const;
    std::set<Hash>         types()     const;
    std::set<Macro>        macros()    const;
    std::string            text()      const;
    AstRef                 parent()    const;
    PTNode                 scopePos() const;
    
    bool VisitStmt(clang::Stmt * expr);

    bool VisitExplicitCastExpr(clang::ExplicitCastExpr * expr);
    bool VisitUnaryExprOrTypeTraitExpr(
        clang::UnaryExprOrTypeTraitExpr * expr);

    bool TraverseVarDecl(clang::VarDecl * decl);
    bool VisitDeclRefExpr(clang::DeclRefExpr * decl);
    
    bool toplevel_is_macro() const { return toplev_is_macro; }

    void setText(const std::string & _text)
    { m_text = _text; }

    void setParent(AstRef p)
    { m_parent = p; }

    void setScopePos(PTNode pos)
    { m_scope_pos = pos; }
    
private:

    void gatherMacro(clang::Stmt * stmt);
    void addAddlType(const clang::QualType & qt);

    clang::CompilerInstance * ci;
    std::set<BindingCtx::Binding> m_vars;
    std::set<FunctionInfo> m_funs;
    std::set<std::string> m_includes;
    std::set<Hash> addl_types;
    std::set<Macro> m_macros;
    std::string m_text;
    AstRef m_parent;
    PTNode m_scope_pos;
    
    bool toplev_is_macro, is_first;
    BindingCtx ctx;

    std::map<std::string, size_t> decl_depth;
};


} // end namespace clang_mutate

#endif
