//===----------------------------------------------------------------------===//
//
// Copyright (c) 2012 The University of Utah
// All rights reserved.
//
// This file is distributed under the University of Illinois Open Source
// License.  See the file COPYING for details.
//
//===----------------------------------------------------------------------===//

#if HAVE_CONFIG_H
#  include <config.h>
#endif

#include "SubstituteClassTemplateParameter.h"

#include "clang/AST/ASTContext.h"
#include "clang/AST/RecursiveASTVisitor.h"

#include "TransformationManager.h"
#include <cstdio>
#include <iostream>
#include <vector>
#include "llvm/Support/raw_os_ostream.h"

using namespace clang;
using namespace llvm;

static const char *DescriptionMsg =
"This pass tries to replace template arguments with types used in code";

static RegisterTransformation<SubstituteClassTemplateParameter>
         Trans("substitute-class-template-param", DescriptionMsg);

class SubstituteClassTemplateParameterASTVisitor : public
  RecursiveASTVisitor<SubstituteClassTemplateParameterASTVisitor> {

public:
    explicit SubstituteClassTemplateParameterASTVisitor(
            SubstituteClassTemplateParameter *Instance)
        : ConsumerInstance(Instance)
    {}

    bool VisitClassTemplateDecl(ClassTemplateDecl *D);

private:
    SubstituteClassTemplateParameter *ConsumerInstance;
};

class SubstituteClassTemplateParameterRewriteVisitor : public
   RecursiveASTVisitor<SubstituteClassTemplateParameterRewriteVisitor> {
public:
    explicit SubstituteClassTemplateParameterRewriteVisitor(
            SubstituteClassTemplateParameter *Instance)
        : ConsumerInstance(Instance)
    {}

    bool VisitTemplateTypeParmTypeLoc(TemplateTypeParmTypeLoc Loc);

private:
    SubstituteClassTemplateParameter *ConsumerInstance;
};

bool SubstituteClassTemplateParameterRewriteVisitor::VisitTemplateTypeParmTypeLoc(TemplateTypeParmTypeLoc Loc)
{
    SourceRange Range = Loc.getSourceRange();
    ConsumerInstance->TheRewriter.ReplaceText(Range, "...");
    return true;
}

bool isValidClassTemplateParam(clang::ClassTemplateDecl *D, unsigned paramIdx)
{
    const TemplateArgument *firstArg = 0;
    for (ClassTemplateDecl::spec_iterator I = D->spec_begin(), E = D->spec_end();
            I != E; ++I) {
        const TemplateArgument &curArg = (*I)->getTemplateArgs()[paramIdx];
        if (!firstArg)
            firstArg = &curArg;
        else if (!curArg.structurallyEquals(*firstArg))
            return false;
    }
    return true;
}

bool SubstituteClassTemplateParameterASTVisitor::VisitClassTemplateDecl(ClassTemplateDecl *D)
{
    ClassTemplateDecl *CanonicalD = D->getCanonicalDecl();

    if (ConsumerInstance->VisitedDecls.count(CanonicalD))
      return true;
    ConsumerInstance->VisitedDecls.insert(CanonicalD);

    const TemplateParameterList *TPList = D->getTemplateParameters();
    unsigned Index;
    for (Index = 0; Index < TPList->size(); ++Index) {
        bool res = isValidClassTemplateParam(CanonicalD, Index);
        printf("res = %d\n", res);
        if (!res)
            continue;

        ConsumerInstance->ValidInstanceNum++;
        if (ConsumerInstance->ValidInstanceNum ==
                ConsumerInstance->TransformationCounter) {
            ConsumerInstance->TheClassTemplateDecl = CanonicalD;
            ConsumerInstance->TheParameterIndex = Index;
            ConsumerInstance->TheTemplateName = new TemplateName(CanonicalD);
//            ConsumerInstance->TheTemplateArgument = ;
        }
    }
    return true;
}


void SubstituteClassTemplateParameter::Initialize(ASTContext &context)
{
    Transformation::Initialize(context);
    CollectionVisitor = new SubstituteClassTemplateParameterASTVisitor(this);
    RewriteVisitor = new SubstituteClassTemplateParameterRewriteVisitor(this);
}

void SubstituteClassTemplateParameter::HandleTranslationUnit(ASTContext &Ctx)
{
    if (TransformationManager::isCLangOpt()) {
      ValidInstanceNum = 0;
    }
    else {
      CollectionVisitor->TraverseDecl(Ctx.getTranslationUnitDecl());
    }

    if (QueryInstanceOnly)
      return;

    printf("ValidInstanceNum=%d \n", ValidInstanceNum);
    if (TransformationCounter > ValidInstanceNum) {
      TransError = TransMaxInstanceError;
      return;
    }

    TransAssert(TheClassTemplateDecl && "NULL TheClassTemplateDecl!");
    TransAssert(RewriteVisitor && "NULL RewriteVisitor!");
    Ctx.getDiagnostics().setSuppressAllDiagnostics(false);

    RewriteVisitor->TraverseDecl(Ctx.getTranslationUnitDecl());

    if (Ctx.getDiagnostics().hasErrorOccurred() ||
        Ctx.getDiagnostics().hasFatalErrorOccurred())
      TransError = TransInternalError;
}

