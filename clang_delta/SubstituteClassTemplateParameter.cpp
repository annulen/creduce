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

//    bool shouldVisitTemplateInstantiations() const { return true; }
//    bool shouldWalkTypesOfTypeLocs() const { return true; }
//    bool shouldVisitImplicitCode() const { return true; }
    bool TraverseTemplateArgumentLoc(const TemplateArgumentLoc& argloc) {
        return true;
    }

    bool VisitTemplateTypeParmTypeLoc(TemplateTypeParmTypeLoc Loc);
//    bool VisitTemplateDecl(TemplateDecl *D) {
//        printf("TemplateDecl=\n");
//        D->getCanonicalDecl()->dump();
//        return true;
//    }
//    bool VisitCompoundStmt(CompoundStmt *Node) {
//        printf("CompoundStmt=%d\n", Node->size());
//        Node->dumpAll();
////        for (CompoundStmt::body_iterator I = Node->body_begin(), E = Node->body_end();
////                I != E; ++I)
////            TraverseStmt(*I);
//        return true;
//    }
//    bool VisitDeclRefExpr(DeclRefExpr *Expr) {
//        printf("DeclRefExpr=\n");
//        Expr->dump();
//        return true;
//    }
//    bool VisitExpr(Expr *Expr) {
//        printf("Expr=\n");
//        Expr->dump();
//        return true;
//    }
//    bool VisitStmt(Stmt *Stmt) {
//        printf("Stmt=\n");
//        Stmt->dump();
//        return true;
//    }

private:
    SubstituteClassTemplateParameter *ConsumerInstance;
};

bool SubstituteClassTemplateParameterRewriteVisitor::VisitTemplateTypeParmTypeLoc(TemplateTypeParmTypeLoc Loc)
{
    //printf("VisitTemplateTypeParmTypeLoc\n");
    const TemplateTypeParmType *Ty =
        dyn_cast<TemplateTypeParmType>(Loc.getTypePtr());
    TransAssert(Ty && "Invalid TemplateSpecializationType!");

    if (Ty->getIndex() == ConsumerInstance->TheParameterIndex) {
        PrintingPolicy Policy = ConsumerInstance->TheClassTemplateDecl->getASTContext().getPrintingPolicy();
        std::string argStr;
        raw_string_ostream out(argStr);
        ConsumerInstance->TheTemplateArgument->print(Policy, out);
        SourceRange Range = Loc.getSourceRange();
        ConsumerInstance->TheRewriter.ReplaceText(Range, out.str());
        ConsumerInstance->madeTransformation = true;
    }
    return true;
}

const TemplateArgument* isValidClassTemplateParam(clang::ClassTemplateDecl *D, unsigned paramIdx)
{
    const TemplateArgument *firstArg = 0;
    for (ClassTemplateDecl::spec_iterator I = D->spec_begin(), E = D->spec_end();
            I != E; ++I) {
        const TemplateArgument &curArg = (*I)->getTemplateArgs()[paramIdx];
        if (!firstArg)
            firstArg = &curArg;
        else if (!curArg.structurallyEquals(*firstArg))
            return 0;
    }
    return firstArg;
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
        const TemplateArgument *arg = isValidClassTemplateParam(CanonicalD, Index);
        if (arg)
            printf("%d\n", arg->getKind());
        if (!arg || arg->getKind() != TemplateArgument::Type)
            continue;

        ConsumerInstance->ValidInstanceNum++;
        if (ConsumerInstance->ValidInstanceNum ==
                ConsumerInstance->TransformationCounter) {
            ConsumerInstance->TheClassTemplateDecl = CanonicalD;
            ConsumerInstance->TheParameterIndex = Index;
            ConsumerInstance->TheTemplateName = new TemplateName(CanonicalD);
            ConsumerInstance->TheTemplateArgument = arg;
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

    //printf("ValidInstanceNum=%d \n", ValidInstanceNum);
    if (TransformationCounter > ValidInstanceNum) {
      TransError = TransMaxInstanceError;
      return;
    }

    TransAssert(TheClassTemplateDecl && "NULL TheClassTemplateDecl!");
    TransAssert(RewriteVisitor && "NULL RewriteVisitor!");
    Ctx.getDiagnostics().setSuppressAllDiagnostics(false);

    RewriteVisitor->TraverseDecl(Ctx.getTranslationUnitDecl());

    if (!madeTransformation)
        TransError = TransNoValidParameterOccurences;

    if (Ctx.getDiagnostics().hasErrorOccurred() ||
        Ctx.getDiagnostics().hasFatalErrorOccurred())
      TransError = TransInternalError;
}

