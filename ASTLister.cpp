#include "ASTLister.h"
#include "BinaryAddressMap.h"
#include "Bindings.h"
#include "ASTEntryList.h"

#include "clang/Basic/FileManager.h"
#include "clang/Basic/Diagnostic.h"
#include "clang/Basic/SourceManager.h"
#include "clang/Lex/Lexer.h"
#include "clang/AST/AST.h"
#include "clang/AST/ASTConsumer.h"
#include "clang/AST/ASTContext.h"
#include "clang/AST/PrettyPrinter.h"
#include "clang/AST/RecordLayout.h"
#include "clang/AST/RecursiveASTVisitor.h"
#include "clang/Rewrite/Frontend/Rewriters.h"
#include "clang/Rewrite/Core/Rewriter.h"
#include "clang/AST/ParentMap.h"
#include "llvm/IR/Module.h"
#include "llvm/Support/Path.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Support/Timer.h"
#include "llvm/Support/SaveAndRestore.h"

#include <sys/types.h>
#include <sys/stat.h>
#include <stdio.h>

#define VISIT(func) \
  bool func { VisitRange(element->getSourceRange()); return true; }

namespace clang_mutate{
using namespace clang;

  class ASTLister : public ASTConsumer,
                    public RecursiveASTVisitor<ASTLister> {
    typedef RecursiveASTVisitor<ASTLister> base;

  public:
    ASTLister(raw_ostream *Out = NULL,
              StringRef Binary = (StringRef) "",
              bool OutputAsJSON = false)
      : Out(Out ? *Out : llvm::outs()),
        Binary(Binary),
        BinaryAddresses(Binary),
        OutputAsJSON(OutputAsJSON),
        PM(NULL),
	get_bindings()
    {}

    ~ASTLister(){
      delete PM;
      PM = NULL;
    }

    virtual void HandleTranslationUnit(ASTContext &Context) {
      TranslationUnitDecl *D = Context.getTranslationUnitDecl();

      // Setup
      Counter=0;

      MainFileID=Context.getSourceManager().getMainFileID();

      Rewrite.setSourceMgr(Context.getSourceManager(),
                           Context.getLangOpts());

      // Run Recursive AST Visitor
      TraverseDecl(D);
   
      // Output the results   
      if ( !ASTEntries.isEmpty() )
      {
        if ( OutputAsJSON )
          ASTEntries.toStreamJSON( Out );
        else
          ASTEntries.toStream( Out );
      }
    };

    bool IsSourceRangeInMainFile(SourceRange r)
    {
      FullSourceLoc loc = FullSourceLoc(r.getEnd(), Rewrite.getSourceMgr());
      return (loc.getFileID() == MainFileID);
    }

    // Return true if the clang::Expr is a statement in the C/C++ grammar.
    // This is done by testing if the parent of the clang::Expr
    // is an aggregation type.  The immediate children of an aggregation
    // type are all valid statements in the C/C++ grammar.
    bool IsCompleteCStatement(Stmt *ExpressionStmt)
    {
      Stmt* parent = (PM != NULL ) ? 
                     PM->getParent(ExpressionStmt) :
                     NULL;

      if ( parent != NULL ) 
      {
        switch ( parent->getStmtClass() )
        {
        case Stmt::CapturedStmtClass:
        case Stmt::CompoundStmtClass:
        case Stmt::CXXCatchStmtClass:
        case Stmt::CXXForRangeStmtClass:
        case Stmt::CXXTryStmtClass:
        case Stmt::DoStmtClass:
        case Stmt::ForStmtClass:
        case Stmt::IfStmtClass:
        case Stmt::SwitchStmtClass:
        case Stmt::WhileStmtClass: 
          return true;
      
        default:
          return false;
        }
      }

      return false;
    }

    virtual bool VisitDecl(Decl *D){
      // Delete the ParentMap if we are in a new
      // function declaration.  There is a tight 
      // coupling between this action and VisitStmt(Stmt* ).

      if (isa<FunctionDecl>(D)) {
        delete PM;
        PM = NULL;
      }

      return true;
    }
    
    virtual bool VisitStmt(Stmt *S){

      SaveAndRestore<GetBindingCtx> sr(get_bindings);
      
      SourceRange R = S->getSourceRange();
      ASTEntry* NewASTEntry = NULL;

      // Test if we are in a new function
      // declaration.  If so, update the parent
      // map with the root statement of this function declaration.
      if ( PM == NULL && S->getStmtClass() == Stmt::CompoundStmtClass ) {
        PM = new ParentMap(S);
      }
      
      if (S->getStmtClass() != Stmt::NoStmtClass &&
          IsSourceRangeInMainFile(R))
      { 
        get_bindings.TraverseStmt(S);

        switch (S->getStmtClass()){

        // These classes of statements
        // correspond to exactly 1 or more
        // lines in a source file.  If applicable,
        // associate them with binary source code.
        case Stmt::BreakStmtClass:
        case Stmt::CapturedStmtClass:
        case Stmt::CompoundStmtClass:
        case Stmt::ContinueStmtClass:
        case Stmt::CXXCatchStmtClass:
        case Stmt::CXXForRangeStmtClass:
        case Stmt::CXXTryStmtClass:
        case Stmt::DeclStmtClass:
        case Stmt::DoStmtClass:
        case Stmt::ForStmtClass:
        case Stmt::GotoStmtClass:
        case Stmt::IfStmtClass:
        case Stmt::IndirectGotoStmtClass:
        case Stmt::ReturnStmtClass:
        case Stmt::SwitchStmtClass:
        case Stmt::DefaultStmtClass: 
        case Stmt::CaseStmtClass: 
        case Stmt::WhileStmtClass:

          NewASTEntry = 
            ASTEntryFactory::make(
               Counter,
               S,
               Rewrite,
               BinaryAddresses,
               make_renames(get_bindings.free_values(),
                            get_bindings.free_functions()) );

          ASTEntries.addEntry( NewASTEntry );
          break;

        // These classes of statements may correspond
        // to one or more lines in a source file.
        // If applicable, associate them with binary
        // source code.
        case Stmt::AtomicExprClass:
        case Stmt::CXXMemberCallExprClass:
        case Stmt::CXXOperatorCallExprClass:
        case Stmt::CallExprClass:
          if(IsCompleteCStatement(S))
          {
            NewASTEntry = 
              ASTEntryFactory::make(
                 Counter,
                 S,
                 Rewrite,
                 BinaryAddresses,
                 make_renames(get_bindings.free_values(),
                              get_bindings.free_functions()) );

            ASTEntries.addEntry( NewASTEntry );
          }
          else
          {
            NewASTEntry = 
              new ASTNonBinaryEntry(
                Counter,
                S,
                Rewrite,
                make_renames(get_bindings.free_values(),
                             get_bindings.free_functions()) );
              
            ASTEntries.addEntry( NewASTEntry );
          }

          break;

        // These classes of statements correspond
        // to sub-expressions within a C/C++ statement.
        // They are too granular to associate with binary
        // source code. 
        default:
          NewASTEntry = new ASTNonBinaryEntry(
                                Counter,
                                S,
                                Rewrite,
                                make_renames(get_bindings.free_values(),
                                             get_bindings.free_functions()) );

          ASTEntries.addEntry( NewASTEntry );
          break;
        }

        Counter++;
      }

      return true;
    }

  private:
    raw_ostream &Out;
    StringRef Binary;
    bool OutputAsJSON;

    BinaryAddressMap BinaryAddresses;
    ASTEntryList ASTEntries;

    Rewriter Rewrite;
    ParentMap* PM;
    unsigned int Counter;
    FileID MainFileID;

    GetBindingCtx get_bindings;
  };
}

clang::ASTConsumer *clang_mutate::CreateASTLister(clang::StringRef Binary, bool OutputAsJSON){
  return new ASTLister(0, Binary, OutputAsJSON);
}
