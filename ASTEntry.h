/*
 The classes within this file are utilized for storing information about a single entry in 
 an AST.  They allow for loading from/storing to a JSON representation.
*/

#ifndef AST_ENTRY_HPP
#define AST_ENTRY_HPP

#include "BinaryAddressMap.h"
#include "Renaming.h"
#include "Macros.h"

#include "clang/AST/AST.h"
#include "clang/Basic/SourceManager.h"
#include "clang/Rewrite/Frontend/Rewriters.h"
#include "clang/Rewrite/Core/Rewriter.h"

#include "third-party/picojson-1.3.0/picojson.h"

#include <string>

namespace clang_mutate
{
  // Abstract base class for all AST entries.
  // Includes methods to convert to a string or JSON representation.
  class ASTEntry
  {
  public:
    virtual ~ASTEntry() {}
    virtual ASTEntry* clone() const = 0;
    virtual unsigned int getCounter() const = 0;
    virtual std::string toString() const = 0;
    virtual picojson::value toJSON() const = 0;
  };

  class ASTEntryFactory
  {
  public:
    static ASTEntry* make( const picojson::value &jsonValue );
    static ASTEntry* make( const unsigned int counter,
                           clang::Stmt *s, 
                           clang::Rewriter& rewrite,
                           BinaryAddressMap &binaryAddressMap,
                           const Renames & renames,
                           const Macros & macros );
  private:
    ASTEntryFactory() {}
  };

  // AST entry without binary information
  class ASTNonBinaryEntry : public ASTEntry
  {
  public:
    ASTNonBinaryEntry();

    ASTNonBinaryEntry( const unsigned int counter,
                       const std::string &astClass,
                       const std::string &srcFileName,
                       const unsigned int beginSrcLine,
                       const unsigned int beginSrcCol,
                       const unsigned int endSrcLine,
                       const unsigned int endSrcCol,
                       const std::string &srcText,
                       const Renames & renames,
                       const Macros & macros );

    ASTNonBinaryEntry( const int counter,
                       clang::Stmt * s,
                       clang::Rewriter& rewrite,
                       const Renames & renames,
                       const Macros & macros );

    ASTNonBinaryEntry( const picojson::value &jsonValue );

    virtual ~ASTNonBinaryEntry();

    virtual ASTEntry* clone() const;

    virtual unsigned int getCounter() const;
    virtual std::string getASTClass() const;
    virtual std::string getSrcFileName() const;
    virtual unsigned int getBeginSrcLine() const;
    virtual unsigned int getBeginSrcCol() const;
    virtual unsigned int getEndSrcLine() const;
    virtual unsigned int getEndSrcCol() const;
    virtual std::string getSrcText() const;
    virtual Renames getRenames() const;
    virtual Macros getMacros() const;
    
    virtual std::string toString() const;
    virtual picojson::value toJSON() const;
    
    static bool jsonObjHasRequiredFields( const picojson::value& jsonValue );
  private:
    unsigned int m_counter;
    std::string  m_astClass;
    std::string  m_srcFileName;
    unsigned int m_beginSrcLine;
    unsigned int m_beginSrcCol;
    unsigned int m_endSrcLine;
    unsigned int m_endSrcCol;
    std::string  m_srcText;
    Renames m_renames;
    Macros m_macros;
  };

  // AST entry with binary information
  class ASTBinaryEntry : public ASTNonBinaryEntry
  {
  public:
    ASTBinaryEntry();

    ASTBinaryEntry( const unsigned int counter, 
                    const std::string &astClass,
                    const std::string &srcFileName,
                    const unsigned int beginSrcLine,
                    const unsigned int beginSrcCol,
                    const unsigned int endSrcLine,
                    const unsigned int endSrcCol,
                    const std::string &srcText,
                    const Renames & renames,
                    const Macros & macros,
                    const std::string &binaryFileName,
                    const unsigned long beginAddress,
                    const unsigned long endAddress,
                    const std::string &binaryContents );

    ASTBinaryEntry( const unsigned int counter,
                    clang::Stmt * s,
                    clang::Rewriter& rewrite,
                    BinaryAddressMap& binaryAddressMap,
                    const Renames & renames,
                    const Macros & macros );

    ASTBinaryEntry( const picojson::value& jsonValue );
    virtual ~ASTBinaryEntry();

    virtual ASTEntry* clone() const;

    virtual std::string getBinaryFilePath() const;
    virtual unsigned long getBeginAddress() const;
    virtual unsigned long getEndAddress() const;
    virtual std::string getBinaryContents() const;

    virtual std::string toString() const;
    virtual picojson::value toJSON() const;
    
    static bool jsonObjHasRequiredFields( const picojson::value &jsonValue );
  private:
    std::string       m_binaryFilePath;
    unsigned long     m_beginAddress;
    unsigned long     m_endAddress;
    std::string       m_binaryContents;
  };
}

#endif
