//===----------------------------------------------------------------------===//
//
// Copyright (c) 2012 The University of Utah
// Copyright (c) 2012 Konstantin Tokarev <annulen@yandex.ru>
// All rights reserved.
//
// This file is distributed under the University of Illinois Open Source
// License.  See the file COPYING for details.
//
//===----------------------------------------------------------------------===//

#ifndef SUBSTITUTE_CLASS_TEMPLATE_PARAMETER
#define SUBSTITUTE_CLASS_TEMPLATE_PARAMETER

#include "Transformation.h"
#include "llvm/ADT/SmallPtrSet.h"

namespace clang {
class TemplateName;
class TemplateArgument;
}

class SubstituteClassTemplateParameterASTVisitor;
class SubstituteClassTemplateParameterRewriteVisitor;

class SubstituteClassTemplateParameter : public Transformation {
friend class SubstituteClassTemplateParameterASTVisitor;
friend class SubstituteClassTemplateParameterRewriteVisitor;

public:
  SubstituteClassTemplateParameter(const char *TransName, const char *Desc)
    : Transformation(TransName, Desc),
      CollectionVisitor(0),
      RewriteVisitor(0),
      TheClassTemplateDecl(0),
      TheTemplateName(0)
  {}

  ~SubstituteClassTemplateParameter() {}

private:
  typedef llvm::SmallPtrSet<const clang::ClassTemplateDecl *, 20> 
            ClassTemplateDeclSet;

  virtual void Initialize(clang::ASTContext &context);

  virtual void HandleTranslationUnit(clang::ASTContext &Ctx);

  ClassTemplateDeclSet VisitedDecls;

  SubstituteClassTemplateParameterASTVisitor *CollectionVisitor;

  SubstituteClassTemplateParameterRewriteVisitor *RewriteVisitor;

  clang::ClassTemplateDecl *TheClassTemplateDecl;

  unsigned TheParameterIndex;

  clang::TemplateName *TheTemplateName;
  const clang::TemplateArgument *TheTemplateArgument;

  // Unimplemented
  SubstituteClassTemplateParameter();

  SubstituteClassTemplateParameter(const SubstituteClassTemplateParameter &);
};

#endif
